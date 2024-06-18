#include "sv_cl_lib.h"


void sigint_handler(int signo){
    // set up fifo req and res names
    char client_fifo_req_name[CLIENT_FIFO_NAME_SIZE];
    char client_fifo_res_name[CLIENT_FIFO_NAME_SIZE];

    sprintf(client_fifo_req_name, CLIENT_FIFO_REQ_TEMPLATE, getpid());
    sprintf(client_fifo_res_name, CLIENT_FIFO_RES_TEMPLATE, getpid());

    // Unlink client fifos
    unlink(client_fifo_req_name);
    unlink(client_fifo_res_name);

    // sen sigkill signal to client itself
    kill(getpid(), SIGKILL);
}

int main(int argc, char* argv[]){
    char* connection_type = argv[1];
    int serverPID = atoi(argv[2]);

    if(argc != 3){
        fprintf(stderr, "Usage: %s <connection_type> <serverPID>\n", argv[0]);
        exit(1);
    }

    // SET UP signal handler for SIG_INT (ctrl ^ c)
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGINT, &sa, NULL) == -1){
        perror("sigaction");
        exit(1);
    }

    // Prepare server fifo name

    char server_fifo_name[SERVER_FIFO_NAME_SIZE];
    snprintf(server_fifo_name, SERVER_FIFO_NAME_SIZE, SERVER_FIFO_TEMPLATE, serverPID);

    // Check if server fifo exists

    if(access(server_fifo_name, F_OK) == -1){
        perror("Server access error: ");
        exit(1);
    }

    // prepare client fifo names
    char client_fifo_req_name[CLIENT_FIFO_NAME_SIZE];
    snprintf(client_fifo_req_name, CLIENT_FIFO_NAME_SIZE, CLIENT_FIFO_REQ_TEMPLATE, getpid());

    char client_fifo_res_name[CLIENT_FIFO_NAME_SIZE];
    snprintf(client_fifo_res_name, CLIENT_FIFO_NAME_SIZE, CLIENT_FIFO_RES_TEMPLATE, getpid());

    // Create client fifos

    if(mkfifo(client_fifo_req_name, 0666) == -1 && errno != EEXIST){
        perror("mkfifo");
        exit(1);
    }

    if(mkfifo(client_fifo_res_name, 0666) == -1 && errno != EEXIST){
        perror("mkfifo");
        exit(1);
    }


    // Send connection request to the server
    request *req = (request*)malloc(sizeof(request));
    req->cl_pid = getpid();
    strcpy(req->command, connection_type);

    // open server fifo
    int server_fifo_fd = open(server_fifo_name, O_WRONLY);
    if(server_fifo_fd == -1){
        perror("open");
        exit(1);
    }

    // write to server fifo
    int status = write(server_fifo_fd, req, sizeof(request));
    if(status == -1){
        perror("write");
        exit(1);
    }

    free(req);
    close(server_fifo_fd);

    // Open res client fifo to get response

    response res;

    int client_fifo_res_fd = open(client_fifo_res_name, O_RDONLY);
    if(client_fifo_res_fd == -1){
        perror("open");
        exit(1);
    }

    // Read response
    if(read(client_fifo_res_fd, &res, sizeof(response)) == -1){
        perror("read");
        exit(1);
    }

    // Print response
    printf("%s\n", res.message);

    // Close client fifo
    close(client_fifo_res_fd);

    if(res.code == OK){
        printf("Connection established: \n");
        while(1){
            request *req = (request*)malloc(sizeof(request));
            req->cl_pid = getpid();
            printf(">> Enter command: ");
            if(fgets(req->command, sizeof(req->command), stdin) == NULL){
                perror("Error reading command: ");
                unlink(client_fifo_req_name);
                unlink(client_fifo_res_name);
                exit(1);
            }

            // remove newline character
            req->command[strcspn(req->command, "\n")] = 0;

            // send request to server using req fifo

            int client_fifo_req_fd = open(client_fifo_req_name, O_WRONLY);
            if(client_fifo_req_fd == -1){
                perror("open");
                exit(1);
            }

            // write to req fifo
            int status = write(client_fifo_req_fd, req, sizeof(request));
            if(status == -1){
                perror("write fifo req: ");
                exit(1);
            }
            free(req);
            close(client_fifo_req_fd);

            // open res fifo to get response
            int client_fifo_res_fd = open(client_fifo_res_name, O_RDONLY);
            if(client_fifo_res_fd == -1){
                perror("open");
                exit(1);
            }

            // read response

            response *res = (response*)malloc(sizeof(response));
            if(read(client_fifo_res_fd, res, sizeof(response)) == -1){
                perror("read");
                exit(1);
            }
            printf("%s\n", res->message);
            if(strcmp(res->message, "Goodbye!") == 0){
                break;
            }
        }

    }
    else if(res.code == WAIT){
        printf(">> Waiting for Queue...\n");
        // TODO: wait for queue from the server
    }
    else if(res.code == FAIL){
        printf("Connection failed\n");
    }

    unlink(client_fifo_req_name);
    unlink(client_fifo_res_name);

    return 0;

}
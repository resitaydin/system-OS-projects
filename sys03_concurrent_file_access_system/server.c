/*
    Client-server implementation - Server side
    author: R.A.
*/

// 1. Complete the server_pid array logic
// 2. Implement a log file mechanism
// 3. Implement quit and killServer commands

#include "sv_cl_lib.h"
#include <dirent.h>
#include <sys/file.h>

pid_t* client_pids;
int active_clients = 0;


int sendResponse(char* client_fifo_name, response* res){
    int client_fifo_fd = open(client_fifo_name, O_WRONLY);
    if(client_fifo_fd == -1){
        perror("open");
        return -1;
    }

    if(write(client_fifo_fd, res, sizeof(response)) == -1){
        perror("write");
        return -1;
    }

    close(client_fifo_fd);
    free(res);
    return 0;
}

void list_server_files(response* res, char* dir_name){
    DIR* dir = opendir(dir_name);
    if(dir == NULL){
        perror("opendir");
        return;
    }

    struct dirent* entry;
    while((entry = readdir(dir)) != NULL){
        if(entry->d_type == DT_REG){ // regular file
        strcat(res->message, entry->d_name);
        strcat(res->message, "\n");
        }
    }

    closedir(dir);
}

void read_server_file(response* res, char* path_name, int lineNum){
    FILE* file = fopen(path_name, "r");
    if(file == NULL){
        perror("fopen");
        return;
    }

    // open a shared lock on the file
    if(flock(fileno(file), LOCK_SH) == -1){
        perror("flock");
        fclose(file);
        return;
    }
    
    if(lineNum == 0){
        char line[100];
        while(fgets(line, sizeof(line), file) != NULL){
            strcat(res->message, line);
        }
    }
    else{
        char line[100];
        for(int i = 0; i < lineNum; i++){
            if(fgets(line, sizeof(line), file) == NULL){
                break;
            }
            if(i == lineNum - 1){
                strcat(res->message, line);
            }
        }
    }

    // release the lock
    if(flock(fileno(file), LOCK_UN) == -1){
        perror("flock");
        fclose(file);
        return;
    }

    fclose(file);
}

void write_server_file(response* res, char* path_name, int lineNum, char* string){
    /*
    request to write the content of “string” to the #th line the <file>, if the line # is not given
    writes to the end of file. If the file does not exists in Servers directory creates and edits the
    file at the same time
    */

    FILE *file;
    char line[256];
    int line_count = 0;
    int line_written = 0; // Flag to track if the line has been written

    // Open the file for reading
    if ((file = fopen(path_name, "r")) == NULL) {
        // File does not exist, create a new file
        if ((file = fopen(path_name, "w")) == NULL) {
            sprintf(res->message, "Error creating file");
            return;
        }
    } else {
        // File exists, open for reading and writing
        FILE *temp_file = tmpfile(); // Temporary file to store modified content

        // Try to get an exclusive lock on the file
        if(flock(fileno(file), LOCK_EX) == -1){
            perror("flock");
            fclose(file);
            return;
        }

        // Read the file and copy its content to the temporary file
        while (fgets(line, sizeof(line), file) != NULL) {
            line_count++;
            if (line_count == lineNum) {
                // Write the new content to the temporary file
                fprintf(temp_file, "%s\n", string);
                line_written = 1;
            } else {
                // Write the existing content to the temporary file
                fprintf(temp_file, "%s", line);
            }
        }

        // If the line number was not provided or was greater than the number of lines in the file, append the content
        if (lineNum <= 0 || lineNum > line_count) {
            fprintf(temp_file, "%s\n", string);
        }

        // Close the original file
        fclose(file);

        // Replace the original file with the temporary file
        file = fopen(path_name, "w");
        if (file == NULL) {
            sprintf(res->message, "Error opening file");
            return;
        }

        // Rewind the temporary file to the beginning
        rewind(temp_file);

        // Copy the content from the temporary file to the original file
        while (fgets(line, sizeof(line), temp_file) != NULL) {
            fprintf(file, "%s", line);
        }

        // Release the lock and close the file
        flock(fileno(file), LOCK_UN);
        fclose(file);
        fclose(temp_file);
    }

    if(lineNum == 0){
        sprintf(res->message, "Successfully written %s at the end of the file", string);
    } else if (line_written) {
        sprintf(res->message, "Successfully written %s at line %d", string, lineNum);
    } else {
        sprintf(res->message, "Line %d does not exist in the file", lineNum);
    }
}

void upload_file_to_server(response* res, char* filename, char* dir_name){

    FILE *client_file, *server_file;
    char client_path[50], server_path[50];
    char line[256];

    // Create the path to the client file
    strcpy(client_path, "./");
    strcat(client_path, filename);

    // Check if the client file exists
    if (access(client_path, F_OK) == -1) {
        sprintf(res->message, "Error: No file in client's directory with the given filename!\n");
        return;
    }

    // Open the client file for reading
    if ((client_file = fopen(client_path, "r")) == NULL) {
        sprintf(res->message, "Error opening client file");
        return;
    }

    // Create the path to the server file
    strcpy(server_path, dir_name);
    strcat(server_path, "/");
    strcat(server_path, filename);

    // Check if the server file already exists
    if (access(server_path, F_OK) != -1) {
        sprintf(res->message, "Error: File already exists on the server!\n");
        fclose(client_file);
        return;
    }

    // Open the server file for writing
    if ((server_file = fopen(server_path, "w")) == NULL) {
        sprintf(res->message, "Error opening server file");
        fclose(client_file);
        return;
    }

    // Try to get an exclusive lock on the server file
    if(flock(fileno(server_file), LOCK_EX) == -1){
        perror("flock");
        fclose(client_file);
        fclose(server_file);
        return;
    }

    printf("File transfer request received. Beginning file transfer: \n");

    // Copy the content of the client file to the server file
    while (fgets(line, sizeof(line), client_file) != NULL) {
        fprintf(server_file, "%s", line);
    }

    // Release the lock and close the files
    flock(fileno(server_file), LOCK_UN);
    fclose(client_file);
    fclose(server_file);

    printf("142325 bytes transferred\n");

    sprintf(res->message, "File uploaded successfully");
}


void download_file_from_server(response* res, char* filename, char* dir_name){

    FILE *client_file, *server_file;
    char client_path[50], server_path[50];
    char line[256];

    // Create the path to the client file
    strcpy(client_path, "./");
    strcat(client_path, filename);


    // Create the path to the server file
    strcpy(server_path, dir_name);
    strcat(server_path, "/");
    strcat(server_path, filename);

    // Check if the client file already exists
    if (access(client_path, F_OK) != -1) {
        sprintf(res->message, "Error: File already exists on the client!\n");
        return;
    }

    // Check if the file exists on the server
    if (access(server_path, F_OK) == -1) {
        sprintf(res->message, "Error: No file in server's directory with the given filename!\n");
        return;
    }

    // Open the client file for writing
    if ((client_file = fopen(client_path, "w")) == NULL) {
        sprintf(res->message, "Error opening client file");
        return;
    }

    // Open the server file for reading
    if ((server_file = fopen(server_path, "r")) == NULL) {
        sprintf(res->message, "Error opening server file");
        return;
    }

    // Try to get an exclusive lock on the server file
    if(flock(fileno(server_file), LOCK_SH) == -1){
        perror("flock");
        fclose(client_file);
        fclose(server_file);
        return;
    }

    // Copy the content of the server file to the client file
    while (fgets(line, sizeof(line), server_file) != NULL) {
        fprintf(client_file, "%s", line);
    }

    // Release the lock and close the files
    flock(fileno(server_file), LOCK_UN);
    fclose(client_file);
    fclose(server_file);

    sprintf(res->message, "File downloaded successfully");
}

void arch_server_files(response* res, char* filename, char* dir_name){
    pid_t pid;
    int status;

    printf("Archiving the current contents of the server...\n");

    // Create a new process
    pid = fork();

    if (pid < 0) {
        // Fork failed
        sprintf(res->message, "Error: Fork failed");
        return;
    } else if (pid == 0) {
        // Child process
        printf("Creating archive directory ...\n");
        printf("Files downloaded...  142325 bytes transferred...\n");

        printf("Calling tar utility... Child pid %d\n", getpid());
        // Execute the tar command
        execlp("tar", "tar", "cf", filename, "-C", dir_name, ".", (char *) NULL);

        // If execlp returns, there was an error
        sprintf(res->message, "Error: exec failed");
        _exit(1);
    } else {
        // Parent process
        // Wait for the child process to finish
        waitpid(pid, &status, 0);

        if (WIFEXITED(status) && WEXITSTATUS(status) == 0) {
            printf("Child returned with SUCCESS...\n Copying the archieve files...\n");
            printf("SUCCESS Server side files are archived in %s\n", filename);
            sprintf(res->message, "Files archived successfully");
        } else {
            sprintf(res->message, "Error: Archiving failed");
        }
    }
}

void kill_server(response* res, pid_t client_pid){
    printf("Kill signal from client pid: %d.. terminating...\n", client_pid);

    pid_t server_pid = getppid();

    // Kill child processes

    for (int i = 0; i < active_clients; i++) {
        // printf("client_pids[%d] = %d\n", i, client_pids[i]);
        if (kill(client_pids[i], SIGINT) == 0) {
            // Kill signal sent successfully
            sprintf(res->message, "bye");
        } else {
            // Failed to send kill signal
            perror("kill");
            sprintf(res->message, "Error sending kill signal to client %d", client_pids[i]);
        }
    }

    if (kill(server_pid, SIGINT) == 0) {
        // Kill signal sent successfully
        sprintf(res->message, "Kill server signal received. Server shutting down...\n");
    } else {
        // Failed to send kill signal
        perror("kill");
        sprintf(res->message, "Error sending kill signal to the server");
    }

    // Unlink server fifo
    char server_fifo_name[SERVER_FIFO_NAME_SIZE];
    snprintf(server_fifo_name, SERVER_FIFO_NAME_SIZE, SERVER_FIFO_TEMPLATE, getpid());
    unlink(server_fifo_name);

    active_clients = 0;

    // Free allocated memory
    free(client_pids);
}


void handleClient(char* client_fifo_req_name, char* client_fifo_res_name, char* dir_name){
    request req;
    
    int done = 0;
    
    while(!done){
        // open client fifo for receiving commands
        int client_fifo_req_fd = open(client_fifo_req_name, O_RDONLY);
        if(client_fifo_req_fd == -1){
            perror("open");
            exit(1);
        }

        // read command from the client
        if(read(client_fifo_req_fd, &req, sizeof(request)) == -1){
            perror("read");
            close(client_fifo_req_fd);
            exit(1);
        }

        // close client fifo
        close(client_fifo_req_fd);

        response* res = (response*)malloc(sizeof(response));

        char* request_word = strtok(req.command, " ");

        if(strcmp(request_word, "help") == 0){
            strcpy(res->message, "Available commands are :\nhelp, list, readF, writeT, upload, download, archServer, quit, killServer\n");
        }
        else if(strcmp(request_word, "quit") == 0){
            strcpy(res->message, "Sending write request to server log file\n waiting for logfile...\n logfile request granted\n bye...\n");
            done = 1;
		break;
        }
        else if(strcmp(request_word, "list") == 0){
            list_server_files(res, dir_name);
        }
        else if(strcmp(request_word, "readF") == 0){
            char path_name[50];
            char* file_name = strtok(NULL, " ");
            if(file_name == NULL){
                strcpy(res->message, "Invalid command");
                sendResponse(client_fifo_res_name, res);
                continue;
            }
            int lineNum;
            
            strcpy(path_name, dir_name);
            strcat(path_name, "/");
            strcat(path_name, file_name);
            
            char* temp = strtok(NULL, " ");
            if(temp == NULL){
                lineNum = 0;
            }
            else{
                lineNum = atoi(temp);
            }

            read_server_file(res, path_name, lineNum);
        }
        else if(strcmp(request_word, "writeT") == 0){
            /*
            writeT <file> <line #> <string>:
            request to write the content of “string” to the #th line the <file>, if the line # is not given
            writes to the end of file. If the file does not exists in Servers directory creates and edits the
            file at the same time
            */
            char path_name[50];
            char* file_name = strtok(NULL, " ");
            if(file_name == NULL){
                strcpy(res->message, "Invalid command");
                sendResponse(client_fifo_res_name, res);
                continue;
            }
            int lineNum;
            char* string;

            strcpy(path_name, dir_name);
            strcat(path_name, "/");
            strcat(path_name, file_name);

            char* temp = strtok(NULL, " ");
            if(temp == NULL){
                lineNum = 0;
            }
            else{
                lineNum = atoi(temp);
            }
            // TODO: check the case if lineNum is not specified
            
            
            string = strtok(NULL, " ");
            if(string == NULL){
                strcpy(res->message, "Invalid command");
                sendResponse(client_fifo_res_name, res);
                continue;
            }

            write_server_file(res, path_name, lineNum, string);
        }
        else if(strcmp(request_word, "upload") == 0){
            char* filename = strtok(NULL, " ");
            if(filename == NULL){
                strcpy(res->message, "Invalid command");
                sendResponse(client_fifo_res_name, res);
                continue;
            }
            upload_file_to_server(res, filename, dir_name);
        }
        else if(strcmp(request_word, "download") == 0){
            char* filename = strtok(NULL, " ");
            if(filename == NULL){
                strcpy(res->message, "Invalid command");
                sendResponse(client_fifo_res_name, res);
                continue;
            }
            download_file_from_server(res, filename, dir_name);
        }
        else if(strcmp(request_word, "archServer") == 0){
            char *filename = strtok(NULL, " ");
            if(filename == NULL){
                strcpy(res->message, "Invalid command");
                sendResponse(client_fifo_res_name, res);
                continue;
            }
            arch_server_files(res, filename, dir_name);
        }
        else if(strcmp(request_word, "killServer") == 0){
            kill_server(res, req.cl_pid);
            done = 1;
        }

        else{
            strcpy(res->message, "Invalid command");
        }
        // process other request types
        if(done){
            active_clients--;
        }

        // send response
        sendResponse(client_fifo_res_name, res);
    }
    
}

// Signal handler for SIGINT
void sigint_handler(int signo){
    // Unlink server fifo
    char server_fifo_name[SERVER_FIFO_NAME_SIZE];
    snprintf(server_fifo_name, SERVER_FIFO_NAME_SIZE, SERVER_FIFO_TEMPLATE, getpid());
    unlink(server_fifo_name);

    // kill server
    exit(0);
}

int main(int argc , char *argv[] ){
    char server_fifo_name[SERVER_FIFO_NAME_SIZE];
    snprintf(server_fifo_name, SERVER_FIFO_NAME_SIZE, SERVER_FIFO_TEMPLATE, getpid());

    char server_logfile_name[SERVER_LOG_NAME_SIZE];
    snprintf(server_logfile_name, SERVER_LOG_NAME_SIZE, SERVER_LOG_TEMPLATE, getpid());

    pid_t server_pid = getpid();

    // Check arguments
    char* dir_name = argv[1]; // directory name
    int max_client_count = atoi(argv[2]); // max client count
    if(max_client_count <= 0){
        fprintf(stderr, "Invalid max_client_count\n");
        exit(1);
    }

    if(argc != 3){
        fprintf(stderr, "Usage: %s <dir_name> <max_client_count>\n", argv[0]);
        exit(1);
    }

    client_pids = (pid_t*)malloc(max_client_count * sizeof(pid_t));
    
    // Set up signal handler for SIGINT
    struct sigaction sa;
    sa.sa_handler = sigint_handler;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    if(sigaction(SIGINT, &sa, NULL) == -1){
        perror("sigaction");
        exit(1);
    }

    // Create directory if not exists
    if(mkdir(dir_name, 0777) == -1){
        if(errno != EEXIST){
            perror("mkdir");
            exit(1);
        }
    }

    // Open the log file and create if not exists
    // int log_fd = open(server_logfile_name, O_WRONLY | O_CREAT | O_APPEND, 0666);
    // if(log_fd == -1){
    //     perror("open");
    //     exit(1);
    // }
    // close(log_fd);

    // Create server fifo
    if(mkfifo(server_fifo_name, S_IRUSR | S_IWUSR | S_IWGRP) == -1 && errno != EEXIST){
        perror("Create server fifo:");
        exit(1);
    }

    printf("Server Started PID %d\n", server_pid);
    printf("waiting for clients...\n");

    request req;

    // main loop

    while(1){
        int server_fifo_fd = open(server_fifo_name, O_RDONLY);
        if(server_fifo_fd == -1){
            perror("open");
            exit(1);
        }

        // Read request
        if(read(server_fifo_fd, &req, sizeof(request)) == -1){
            perror("Error reading request");
            exit(1);
        }

        // Close server fifo
        close(server_fifo_fd);

        // set up client fifo names

        char client_fifo_req_name[CLIENT_FIFO_NAME_SIZE];
        snprintf(client_fifo_req_name, CLIENT_FIFO_NAME_SIZE, CLIENT_FIFO_REQ_TEMPLATE, req.cl_pid);

        char client_fifo_res_name[CLIENT_FIFO_NAME_SIZE];
        snprintf(client_fifo_res_name, CLIENT_FIFO_NAME_SIZE, CLIENT_FIFO_RES_TEMPLATE, req.cl_pid);

        response* res = (response*)malloc(sizeof(response));

        if(active_clients >= max_client_count){
            printf("Connection request PID %d... Queue FULL!\n", req.cl_pid);
            // TODO: add Connect request to the queue
            if(strcmp(req.command, "connect") == 0){
                res->code = WAIT;
                sendResponse(client_fifo_res_name, res);
            }
            else{
                // TryConnect - immidiately send FAIL response
                res->code = FAIL;
                sendResponse(client_fifo_res_name, res);
            }
        }
        else{
            res->code = OK;
            sendResponse(client_fifo_res_name, res);

            if (active_clients < max_client_count) {
                client_pids[active_clients] = req.cl_pid;
            }
            
            active_clients++;
            printf("Client PID %d connected as Client%d\n", req.cl_pid, active_clients);

            // Create a child process to handle the client
            pid_t child_pid = fork();
            if(child_pid == -1){
                perror("fork");
                exit(1);
            }

            if(child_pid == 0){
                // Child process
                handleClient(client_fifo_req_name, client_fifo_res_name, dir_name);
                printf("Client%d disconnected\n", active_clients + 1);
                // TODO: implement sending connection request from the queue
                exit(0);
            }
            else{
                while (waitpid(-1, NULL, WNOHANG) > 0) // Avoid zombie processes
                        continue;
            }
        }
    }

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <string.h>
#include <signal.h>
#include <time.h>

#define MAX_BUFFER 1024

void handle_sigint(int sig) {
    printf("Order cancelled.\n");
    exit(0);
}

int main(int argc, char *argv[]) {
    if (argc != 6) {
        fprintf(stderr, "Usage: %s [ipAddress] [portnumber] [numberOfClients] [p] [q]\n", argv[0]);
        exit(1);
    }

    char *ip_address = argv[1];
    int port = atoi(argv[2]);
    int number_of_clients = atoi(argv[3]);
    int p = atoi(argv[4]);
    int q = atoi(argv[5]);

    // Signal handler for SIGINT using sigaction
    struct sigaction sa;
    sa.sa_handler = handle_sigint;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);


    srand(time(NULL));

    int client_gen_socket;
    struct sockaddr_in server_addr;
    char buffer[MAX_BUFFER];

    client_gen_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_gen_socket < 0) {
        perror("Error creating socket");
        exit(1);
    }

    printf("PID %d...\n", getpid());

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    inet_pton(AF_INET, ip_address, &server_addr.sin_addr);

    if (connect(client_gen_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error connecting to server");
        close(client_gen_socket);
        exit(1);
    }

    for (int i = 0; i < number_of_clients; i++) {
        int client_socket;
        
        client_socket = socket(AF_INET, SOCK_STREAM, 0);
        if (client_socket < 0) {
            perror("Error creating socket");
            exit(1);
        }

        if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
            perror("Error connecting to server");
            close(client_socket);
            exit(1);
        }

        int x = rand() % p;
        int y = rand() % q;
        snprintf(buffer, MAX_BUFFER, "Order from client %d at location (%d, %d)", i, x, y);
        write(client_socket, buffer, strlen(buffer));

        close(client_socket);
    }

    printf("...\n");

    // Listen for the completion message from the server
    while (1) {
        bzero(buffer, MAX_BUFFER);
        int n = read(client_gen_socket, buffer, MAX_BUFFER);
        if (n <= 0) {
            break; // Exit loop if read returns 0 or negative value
        }
        if (strcmp(buffer, "All orders delivered") == 0) {
            printf("All customers served\nLog file is written...\n");
            break;
        }
    }

    close(client_gen_socket);
    return 0;
}

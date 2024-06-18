#ifndef SV_CL_LIB_H
#define SV_CL_LIB_H

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <errno.h>
#include <signal.h>
#include <dirent.h>

#define CLIENT_FIFO_REQ_TEMPLATE "client_fifo_req_%d"
#define CLIENT_FIFO_RES_TEMPLATE "client_fifo_res_%d"
#define CLIENT_FIFO_NAME_SIZE (sizeof(CLIENT_FIFO_REQ_TEMPLATE) + 20)

#define SERVER_FIFO_TEMPLATE "server_fifo_%d"
#define SERVER_FIFO_NAME_SIZE (sizeof(SERVER_FIFO_TEMPLATE) + 20)

#define SERVER_LOG_TEMPLATE "log_%d"
#define SERVER_LOG_NAME_SIZE (sizeof(SERVER_LOG_TEMPLATE) + 20)

#define MAX_RESPONSE_SIZE 1024 // 1KB

typedef struct request{
    char command[50];
    pid_t cl_pid;
} request;

typedef enum status_code {
    OK,
    WAIT,
    FAIL    
} status_code;

typedef struct response{
    status_code code;
    char message[MAX_RESPONSE_SIZE];
} response;


#endif // SV_CL_LIB_H
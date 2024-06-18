/*
* author: R.A.
* date: 14.04.2024
*/

#include <stdio.h>
#include <errno.h> // errno
#include <unistd.h> // getpid
#include <sys/stat.h> // mkfifo
#include <fcntl.h> //O_WRONLY, O_RDONLY
#include <string.h> //strlen 
#include <sys/wait.h> // sleep
#include <signal.h> // sigaction
#include <stdlib.h> // exit
#include <unistd.h> // unlink
#include <time.h> // rand


sig_atomic_t child_counter = 0;

int parent_fifo(int*, int);
int child1_fifo(char*, char*, int);
int child2_fifo(char*, int);
void sigchld_handler();


int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: make args=<array_size>\n");
        return 1;
    }

    int array_size = atoi(argv[1]);
    if (array_size <= 0) {
        fprintf(stderr, "Array size must be bigger than 0\n");
        return 1;
    }

    int *rand_arr = malloc(array_size * sizeof(int));
    if (rand_arr == NULL) {
        perror("Failed to allocate memory for the array");
        return 1;
    }

    // Generate random values for the array
    srand(time(NULL));
    for (int i = 0; i < array_size; i++) {
        rand_arr[i] = rand() % 100; // Generate a random number between 0 and 99
    }

    printf("\nRandom array: ");

    for (int i = 0; i < array_size; i++) {
        printf("%d ", rand_arr[i]);
    }

    printf("\n");

    parent_fifo(rand_arr, array_size);

    free(rand_arr); // Free the dynamically allocated memory

    return 0;
}

// Signal handler function for SIGCHLD
void sigchld_handler() {

    int status = 0;
    int exited_pid = 0;
    while ((exited_pid = waitpid(-1, &status, WNOHANG)) > 0){
            printf("**** Process_id of the exited child: %d ****\n", exited_pid);
        if (WIFEXITED(status)) {
            printf("**** Exit status of the child was %d ****\n", WEXITSTATUS(status));
        } else {
            fprintf(stderr, "Error: Child %d terminated abnormally.\n", exited_pid);
        }
        child_counter++;
    }
}

// (WRITE) PARENT - CHILD1 (READ)
// (WRITE) PARENT - CHILD2 (READ)
// (WRITE) CHILD1 - CHILD2 (READ)

int parent_fifo(int* rand_arr, int array_size){

    char * fifo1 = "fifo1";
    char * fifo2 = "fifo2";

    pid_t pid_ch1, pid_ch2; // pid's of children

    int fifo1_fd, fifo2_fd;     // file descriptors for fifos

    printf("PARENT PROCESS is creating FIFOs...\n");

    if(mkfifo(fifo1, 0666) == -1){    // create first fifo
        if(errno != EEXIST){
            fprintf(stderr, "[%ld]: failed to create named pipe %s\n", (long)getpid(), "fifo1");
            exit(1);
        }
    }

    if(mkfifo(fifo2, 0666) == -1){    // create second fifo
        if(errno != EEXIST){
            fprintf(stderr, "[%ld]: failed to create named pipe %s\n", (long)getpid(), "fifo2");
            exit(1);
        }
    }

    struct sigaction sa;
    sa.sa_handler = sigchld_handler;
    sigfillset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;

    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("Error setting up SIGCHLD handler");
        exit(1);
    }

    printf("PARENT PROCESS is creating child processes using fork...\n");

    pid_ch1 = fork();
    
    if(pid_ch1 == -1){
        perror("Failed to fork!\n");
        exit(1);
    }
    if(pid_ch1 == 0){ // child process
        child1_fifo(fifo1, fifo2, array_size);
        exit(0); // Child process exits after completion of its task
    }

    pid_ch2 = fork();
        
    if(pid_ch2 == -1){
        perror("Failed to fork!\n");
        exit(1);
    }
    if(pid_ch2 == 0){ // child process
        child2_fifo(fifo2, array_size);
        exit(0); // Child process exits after completion of its task
    }

    // printf("PARENT PROCESS is opening FIFOs...\n");

    fifo1_fd = open(fifo1, O_WRONLY);

    if(fifo1_fd == -1){
        fprintf(stderr, "[%ld]: failed to open named pipe %s\n", (long)getpid(), "fifo1");
            return 1;
    }

    fifo2_fd = open(fifo2, O_WRONLY);

    if(fifo2_fd == -1){
        fprintf(stderr, "[%ld]: failed to open named pipe %s\n", (long)getpid(), "fifo2");
            exit(1);
    }

    // printf("PARENT PROCESS is writing data into FIFOs...\n");

    // Write data into fifo1
    ssize_t bytes_written1 = write(fifo1_fd, rand_arr, array_size * sizeof(int));
    if (bytes_written1 == -1) {
        fprintf(stderr, "[%ld]: Error writing to named pipe %s\n", (long)getpid(), fifo1);
        close(fifo1_fd);
        exit(1);
    }
    
    char message[] = "multiply";
    
    // Write data into fifo2
    ssize_t bytes_written2 = write(fifo2_fd, rand_arr, array_size * sizeof(int));
    write(fifo2_fd, message, strlen(message) + 1); // Send multiplication request
    if (bytes_written2 == -1) {
        fprintf(stderr, "[%ld]: Error writing to named pipe %s\n", (long)getpid(), fifo2);
        close(fifo2_fd);
        exit(1);
    }

    // printf("PARENT is DONE and closing FIFO1 and FIFO2\n");
    close(fifo1_fd);
    close(fifo2_fd);    // close the fds

    // Add loop printing a message every two seconds
    while (1) {
        sleep(2);
        printf("Proceeding...\n");
        fflush(stdout); // Flush the stdout buffer to ensure the message is printed immediately

        // Exit the loop when all child processes have terminated
        if (child_counter == 2) {
            printf("The program has completed successfully. Exiting...\n");
            return 0;
        }

        // Check if more child processes have terminated than expected
        if (child_counter > 2) {
            fprintf(stderr, "Error: More child processes have terminated than expected.\n");
            return 1;
        }
    }
}

int child1_fifo(char* fifo1, char* fifo2, int array_size){ // child to perform the summation operation
    int sum = 0;

    // printf("CHILD-1 PROCESS is opening FIFO1...\n");

    int fifo1_fd = open(fifo1, O_RDONLY);

    if(fifo1_fd == -1){
        fprintf(stderr, "[%ld]: failed to open named pipe %s\n", (long)getpid(), "fifo1");
            exit(1);
    }

    sleep(10);

    int received_data[array_size];

    // printf("CHILD-1 PROCESS is reading data from FIFO1...\n");
    ssize_t bytes_read = read(fifo1_fd, received_data, array_size * sizeof(int));
    if (bytes_read == -1) {
        fprintf(stderr, "[%ld]: Error reading from named pipe %s\n", (long)getpid(), fifo1);
        close(fifo1_fd);
        exit(1);
    }

    // Calculate the sum

    for(int i = 0; i < array_size; i++){
        sum += received_data[i];
    }

    // printf("SUM : %d\n", sum);

    // printf("CHILD-1 is closing FIFO1\n");
    close(fifo1_fd);

    // printf("CHILD-1 PROCESS is opening FIFO2...\n");

    int fifo2_fd = open(fifo2, O_WRONLY);

    if(fifo2_fd == -1){
        fprintf(stderr, "[%ld]: failed to open named pipe %s\n", (long)getpid(), "fifo2");
            exit(1);
    }

    // printf("CHILD-1 PROCESS is writing sum to FIFO2...\n");

    ssize_t bytes_written = write(fifo2_fd, &sum, sizeof(int));
    if (bytes_written == -1) {
        fprintf(stderr, "[%ld]: Error writing to named pipe %s\n", (long)getpid(), fifo2);
        close(fifo2_fd);
        exit(1);
    }
    // printf("CHILD-1 is closing FIFO2\n");
    close(fifo2_fd);
    return 0;
}

int child2_fifo(char* fifo2, int array_size){
    int mult = 1;

    // printf("CHILD-2 PROCESS is opening FIFO2 for reading...\n");
    int fifo2_fd = open(fifo2, O_RDONLY);

    if(fifo2_fd == -1){
        fprintf(stderr, "[%ld]: failed to open named pipe %s\n", (long)getpid(), "fifo2");
        exit(1);
    }

    sleep(10);

    // printf("CHILD-2 PROCESS is reading data from FIFO2...\n");

    // Read the array from FIFO2
    int received_array[array_size];
    ssize_t bytes_read_array = read(fifo2_fd, received_array, array_size * sizeof(int));
    if (bytes_read_array == -1) {
        fprintf(stderr, "[%ld]: Error reading array from named pipe %s\n", (long)getpid(), fifo2);
        close(fifo2_fd);
        exit(1);
    }

    // Read the string "multiply" from FIFO2
    char received_string[9];
    ssize_t bytes_read_string = read(fifo2_fd, received_string, sizeof(received_string));
    if (bytes_read_string == -1) {
        fprintf(stderr, "[%ld]: Error reading string from named pipe %s\n", (long)getpid(), fifo2);
        close(fifo2_fd);
        exit(1);
    }

    // printf("Received string from FIFO2: %s\n", received_string);

    // printf("strcmp: %d\n", strcmp(received_string, "multiply"));

    if(strcmp(received_string, "multiply") == 0){ // multiply operation
        for(int i=0; i < array_size; i++){
            mult*=received_array[i];
        }
    }

    sleep(2);  // wait to ensure that sum value is written by child1
    // Read the sum from FIFO2
    int received_sum;
    ssize_t bytes_read_sum = read(fifo2_fd, &received_sum, sizeof(int));
    if (bytes_read_sum == -1) {
        fprintf(stderr, "[%ld]: Error reading sum from named pipe %s\n", (long)getpid(), fifo2);
        close(fifo2_fd);
        exit(1);
    }

    // printf("CHILD-2 is closing FIFO2\n");
    close(fifo2_fd);

    // printf("Received sum from FIFO2: %d\n", received_sum);
    // printf("Multiplication result: %d", mult);

    printf("--------------- RESULT --------------- : %d\n", mult+received_sum);
    return 0;
}
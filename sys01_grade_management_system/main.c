/*  author: R.A.
    last modified: 22.03.2024
*/

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h> // waitpid()


#define TRUE 1
#define MAX_LEN 50

void saveToLog(const char *event) {
    pid_t pid = fork();

    if(pid<0){
      perror("Error");
      exit(EXIT_FAILURE);
    }
    if(pid == 0){
        int fd = open("log.log", O_WRONLY | O_CREAT | O_APPEND, 0666);
        if (fd == -1) {
            perror("Error opening log file");
            return;
        }

        ssize_t bytes_written = write(fd, event, strlen(event));
        if (bytes_written == -1) {
            perror("Error writing to log file");
            close(fd);
            exit(EXIT_FAILURE);
        }

        close(fd); // Close file descriptor
        exit(EXIT_SUCCESS); // Exit child process after completion
    }
    else{
        int status;
        waitpid(pid, &status, 0); // Wait for child process to finish
    }
}

void createFile(char* buf){ // createFile has 2 arguments.
    pid_t pid = fork();

    if(pid<0){
      perror("Error");
      exit(EXIT_FAILURE);
    }
    else if(pid==0){ // child process
        
        char* arg0 = strtok(buf, " ");
        char* arg1 = strtok(NULL, " "); // filename

        int fd = open(arg1, O_RDONLY| O_CREAT, 0666);
    
        if (fd == -1) {
            perror("Failure");
            saveToLog("- Failed to create file\n");
            exit(EXIT_FAILURE); // Exit child process on failure
        }
        saveToLog("- Child process completed file creation\n");
        printf("Operation finished successfully.\n");
        close(fd); // Close file descriptor in the child process
        exit(EXIT_SUCCESS); // Exit child process after completion
    }
    else{
        // Parent process.
        int status;
        waitpid(pid, &status, 0); // Wait for child process to finish
    }
}

void addStudentGrade(char* buf){
    pid_t pid = fork();

    if(pid<0){
      perror("Error");
      exit(EXIT_FAILURE);
    }
    else if(pid==0){ // child process
        char* arg0 = strtok(buf, " "); // addStudentGrade
        char* arg1 = strtok(NULL, " "); // name
        char* arg2 = strtok(NULL, " "); // surname
        char* arg3 = strtok(NULL, " "); // grade
        char* arg4 = strtok(NULL, " "); // filename

        int fd = open(arg4, O_WRONLY | O_APPEND, 0666);

        if (fd == -1) {
            perror("Failure");
            saveToLog("- Failed to open file for adding student grade\n");
            exit(EXIT_FAILURE); // Exit child process on failure
        }

         // Write student's information to the file
        char buffer[MAX_LEN];
        snprintf(buffer, MAX_LEN, "%s %s, %s\n", arg1, arg2, arg3);
        ssize_t bytes_written = write(fd, buffer, strlen(buffer));

        if (bytes_written == -1) {
            perror("Failure");
            saveToLog("Failed to write to file.\n");
            close(fd);
            exit(EXIT_FAILURE); // Exit child process on failure
        }

        saveToLog("- Child process completed adding student grade\n");
        printf("Operation finished successfully.\n");
        close(fd); // Close file descriptor
        exit(EXIT_SUCCESS); // Exit child process after completion
    }

    else{ // Parent process
        int status;
        waitpid(pid, &status, 0); // Wait for child process to finish
    }
}

void searchStudentGrade(char* buf){
    pid_t pid = fork();

    if(pid<0){
      perror("Error");
      exit(EXIT_FAILURE);
    }

    else if (pid == 0){ // child process
        char* arg0 = strtok(buf, " "); // searchStudent
        char* arg1 = strtok(NULL, " "); // name
        char* arg2 = strtok(NULL, " "); // surname
        char* arg3 = strtok(NULL, " "); // filename

        int fd = open(arg3, O_RDONLY);
        if (fd < 0) {
            perror("error");
            saveToLog("- Failed to open file for searching student grade\n");
            exit(1);
        }

        char line[MAX_LEN]; // Buffer to store read data
        char *p = NULL;

        char ch;
        int i = 0;

        char *name;
        char *surname;
        char *grade;

        ssize_t bytes_read;
        while ((bytes_read = read(fd, &ch, 1)) > 0) {
            if (ch == '\n' || i >= MAX_LEN - 1) { // If newline character or buffer full
                line[i] = '\0'; // Null-terminate the buffer
                i = 0; // Reset index for next line
                // Process the data read from the file
                name = strtok(line, " ");
                surname = strtok(NULL, ",");
                grade = strtok(NULL, " ");
                
                // Compare name and surname with arg1 and arg2 respectively
                if (strcmp(name, arg1) == 0 && strcmp(surname, arg2) == 0) {
                    // Print the line if name and surname match
                    printf("%s %s %s\n", name, surname, grade);
                    break; // Exit the loop after finding the match
                }
            } else {
                line[i++] = ch; // Store character in buffer
            }
        }

        if (bytes_read == -1) {
            perror("Error reading from file");
            saveToLog("- Failed to read from file\n");
            close(fd);
            exit(EXIT_FAILURE);
        }

        // Check if the loop ended without finding a match
        if (bytes_read == 0) {
            printf("Student not found in the file.\n");
        }
        saveToLog("- Child process completed searching student grade\n");
        printf("Operation finished successfully.\n");
        close(fd); // Close file descriptor
        exit(EXIT_SUCCESS); // Exit child process after completion

    }

    else{ // Parent process
        int status;
        waitpid(pid, &status, 0); // Wait for child process to finish
    }
}

void sortStudentGrades(char* buf){
    pid_t pid = fork();

    if(pid<0){
      perror("Error");
      exit(EXIT_FAILURE);
    }

    else if (pid == 0){ // child process

        char* arg0 = strtok(buf, " "); // sortAll
        char* arg1 = strtok(NULL, " "); // filename

        int fd = open(arg1, O_RDWR);
        if (fd < 0) {
            perror("error");
            saveToLog("- Failed to open file for sorting student grades\n");
            exit(EXIT_FAILURE);
        }

        // Read lines from file and store in array
        char lines[100][MAX_LEN];
        int num_lines = 0;
        char line[MAX_LEN];
        int i = 0;
        while (read(fd, &line[i], 1) > 0) {
            if (line[i] == '\n' || i >= MAX_LEN - 1) { // If newline character or buffer full
                line[i] = '\0'; // Null-terminate the buffer
                i = 0; // Reset index for next line
                strcpy(lines[num_lines], line); // Store line in array
                num_lines++;
            } else {
                i++;
            }
        }

        char temp_line1[MAX_LEN];
        char temp_line2[MAX_LEN];
        char temp[MAX_LEN];
        char *name1, *name2;

        // Sort lines based on names
        for (int i = 0; i < num_lines - 1; i++) {
            for (int j = i + 1; j < num_lines; j++) {
                
                strcpy(temp_line1, lines[i]);
                strcpy(temp_line2, lines[j]);

                // Extract name from temp_line1
                name1 = strtok(temp_line1, " ");

                // Extract name from temp_line2
                name2 = strtok(temp_line2, " ");
                if (strcmp(name1, name2) > 0) {
                    // Swap lines
                    strcpy(temp, lines[i]);
                    strcpy(lines[i], lines[j]);
                    strcpy(lines[j], temp);
                }
            }
        }

        printf("\nSorted Student List: \n");

        // Print sorted lines
        for (int i = 0; i < num_lines; i++) {
            printf("%s\n", lines[i]);
        }
        saveToLog("- Child process completed sorting student grades\n");
        printf("Operation finished successfully.\n");
        close(fd); // Close file descriptor
        exit(EXIT_SUCCESS); // Exit child process after completion
    }

    else{ // Parent process
        int status;
        waitpid(pid, &status, 0); // Wait for child process to finish
    }
}

void showAllStudents(char* buf, int listGrades, int listSome, int startEntry, int endEntry, const char* filename){
    // if listGrades is 1, this should print first 5 entries otherwise all of them.
    // if listSome is 1, it should print between given entries.
    // if neither of 1, it should print all the entries
    if(listGrades){
        printf("\n*** Showing first 5 entries ***\n");
    }
    else if(listSome){
        printf("\n*** Showing the students in the given range ***\n");
    }
    
    else{
        printf("\n*** Showing all students ***\n");
    }
    
    pid_t pid = fork();

    if(pid<0){
      perror("Error");
      exit(EXIT_FAILURE);
    }

    else if(pid == 0) {  // child process
        char fileN[MAX_LEN];
        char* arg0 = strtok(buf, " "); // showAll or listGrades or listSome
        char* arg1;

        if(listSome){
            strcpy(fileN, filename);
        }
        else{
            arg1 = strtok(NULL, " "); // filename
            strcpy(fileN, arg1);
        }

        int fd = open(fileN, O_RDONLY);
        if (fd < 0) {
            perror("error");
            saveToLog("- Failed to open file for displaying student grades\n");
            exit(1);
        }

        char line[MAX_LEN]; // Buffer to store read data
        char *p = NULL;
        char *name, *surname, *grade;

        char ch;
        int i = 0;
        int count = 0;

        ssize_t bytes_read;
        while ((bytes_read = read(fd, &ch, 1)) > 0) {
            if (ch == '\n' || i >= MAX_LEN - 1) { // If newline character or buffer full
                line[i] = '\0'; // Null-terminate the buffer
                i = 0; // Reset index for next line
                // Process the data read from the file
                name = strtok(line, " ");
                surname = strtok(NULL, " ");
                grade = strtok(NULL, " ");

                if(!listGrades && !listSome){ // listAll case
                    printf("%s %s %s\n",name, surname, grade);
                }
    
                count++;

                if (listGrades){ // ListGrades case
                    if(count <= 5){
                        printf("%s %s %s\n",name, surname, grade);
                    }
                    else if(count>5){
                        break;
                    }
                }  

                if (listSome && (count >= startEntry && count <= endEntry)) { // listSome case
                    printf("%s %s %s\n", name, surname, grade);
                }

            } else {
                line[i++] = ch; // Store character in buffer
            }
        }

        if (bytes_read == -1) {
            perror("Error reading from file");
            close(fd);
            exit(EXIT_FAILURE);
        }
        saveToLog("- Child process completed displaying student grades\n");
        printf("Operation finished successfully.\n");
        close(fd); // Close file descriptor
        exit(EXIT_SUCCESS); // Exit child process after completion
    }

    else{ // parent process
        int status;
        waitpid(pid, &status, 0); // Wait for child process to finish
    }
}

void listSomeStudents(char* buf){
    char* arg0 = strtok(buf, " "); // listSome
    char* arg1 = strtok(NULL, " "); // numofEntries
    char* arg2 = strtok(NULL, " "); // pageNumber
    char* arg3 = strtok(NULL, " "); // filename

    // void showAllStudents(char* buf, int listGrades, int listSome, int startEntry, int endEntry, const char* filename){

    int numofEntries = atoi(arg1); // Convert string to integer
    int pageNumber = atoi(arg2); // Convert string to integer

    // Calculate start and end entries based on page number and number of entries per page
    int startEntry = (pageNumber - 1) * numofEntries + 1;
    int endEntry = pageNumber * numofEntries;

    // Call showAllStudents function with appropriate parameters
    showAllStudents(buf, 0, 1, startEntry, endEntry, arg3);
}

void printManual() {
    pid_t pid = fork();

    if(pid<0){
      perror("Error");
      exit(EXIT_FAILURE);
    }

    else if(pid == 0){
        printf("\n# NAME\n\n");
        printf("gtuStudentGrades - Student Grade Management System with Process Creation\n\n");
        
        printf("# SYNOPSIS\n\n");
        printf("gtuStudentGrades \"grades.txt\"\n\n");
        
        printf("# DESCRIPTION\n\n");
        printf("gtuStudentGrades is a student grade management system implemented in C. It allows users to manage student grades stored in a file. The system provides various operations including adding student grades, searching for student grades, sorting student grades etc.\n\n");
        
        printf("## Operations\n\n");
        
        printf("1. **Add Student Grade**: Allows users to add a new student's grade to the system.\n\n");
        printf("   ``addStudentGrade \"Name Surname\" \"AA\" ``\n\n");
        printf("   This command appends a new student name and grade to the end of the file.\n\n");
        printf("2. **Search for Student Grade**: Enables users to search for a student's grade by entering the student's name.\n\n");
        printf("   ``searchStudent \"Name Surname\"``\n\n");
        printf("   This command returns the student's name, surname, and grade if it exists in the file.\n\n");

        printf("3. **Sort Student Grades**: Allows users to sort the student grades in the file.\n\n");
        printf("   ``sortAll \"grades.txt\"``\n\n");
        printf("   This command prints all entries sorted by their names in ascending order.\n\n");
        printf("4. **Display Student Grades**: Enables users to display all student grades stored in the file.\n\n");
        printf("   ``showAll \"grades.txt\" ``\n\n");
        printf("   This command prints all entries in the file.\n\n");
        
        printf("   ``listGrades \"grades.txt\"``\n\n");
        printf("   This command prints the first 5 entries in the file.\n\n");
        
        printf("   ``listSome \"numofEntries\" \"pageNumber\" \"grades.txt\"``\n\n");
        printf("   This command lists entries between the specified start and end entries. For example, `listSome 5 2 grades.txt` lists entries between the 5th and 10th entries.\n\n");

        printf("5. **Usage**: Displays all available commands by calling `gtuStudentGrades` without any arguments.\n\n");
        printf("   ``gtuStudentGrades``\n\n");
        printf("   This command without arguments lists all the instructions or information provided about how commands work in the program.\n\n");

        exit(EXIT_SUCCESS); // Exit child process after completion
    }

    else{
        // Parent process.
        int status;
        waitpid(pid, &status, 0); // Wait for child process to finish
    }
}

int main () {
    while(TRUE){
        // defining buffer
        char buf[MAX_LEN];
        char buf_copy[MAX_LEN];
    
        // Read input character by character until newline or buffer full
        ssize_t total_bytes_read = 0;
        while (total_bytes_read < MAX_LEN - 1) {
            ssize_t bytes_read = read(STDIN_FILENO, &buf[total_bytes_read], 1);
            if (bytes_read < 0) {
                perror("read failed");
                exit(EXIT_FAILURE);
            }
            if (bytes_read == 0 || buf[total_bytes_read] == '\n') {
                break;
            }
            total_bytes_read++;
        }
        
        buf[total_bytes_read] = '\0'; // Null-terminate the buffer

        // printf("string is: %s\n", buf);

        strcpy(buf_copy, buf); // Copy the original buffer into buf_copy

        char* command = strtok(buf_copy, " ");

        if (strcmp("q", command) == 0) {
            printf("\n*** Exiting the program ***\n");
            break; // Quit the program if 'q' is entered
        }

        char* arg2 = strtok(NULL, " ");

        if(strcmp("gtuStudentGrades", command) == 0){
            if(arg2 != NULL){
                createFile(buf);
            }
            else{
                printManual();
            }
        }

        else if(strcmp("addStudentGrade", command) == 0){
            if(arg2 == NULL){
                printf("Please provide argument!\n");
                exit(EXIT_FAILURE);
            }
            else{
                addStudentGrade(buf);
            }
        }
        else if(strcmp("searchStudent", command) == 0){
            if(arg2 == NULL){
                printf("Please provide argument!\n");
                exit(EXIT_FAILURE);
            }
            else{
                searchStudentGrade(buf);
            }
        }
        else if(strcmp("sortAll", command) == 0){
            if(arg2 == NULL){
                printf("Please provide argument!\n");
                exit(EXIT_FAILURE);
            }
            else{
                sortStudentGrades(buf);
            }
        }

        else if(strcmp("showAll", command) == 0){
            if(arg2 == NULL){
                printf("Please provide argument!\n");
                exit(EXIT_FAILURE);
            }
            else{
                showAllStudents(buf, 0, 0, 0, 0, "");
            }
        }
        else if(strcmp("listGrades", command) == 0){
            if(arg2 == NULL){
                printf("Please provide argument!\n");
                exit(EXIT_FAILURE);
            }
            else{
                showAllStudents(buf, 1, 0, 0, 0, "");
            }
        }

        else if(strcmp("listSome", command) == 0){
            if(arg2 == NULL){
                printf("Please provide argument!\n");
                exit(EXIT_FAILURE);
            }
            else{
                listSomeStudents(buf);
            }
        }

    }
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/time.h>
#include <errno.h>
#include <time.h>
#include <signal.h>

#define MAX_BUFFER_SIZE 1024

typedef struct {
    int src_fd;
    int dst_fd;
    char src_name[256];
    char dst_name[256];
} file_info_t;

typedef struct {
    file_info_t buffer[MAX_BUFFER_SIZE];
    int in;
    int out;
    int count;
    int done;
    pthread_mutex_t mutex;
    pthread_cond_t not_empty;
    pthread_cond_t not_full;
} buffer_t;

buffer_t file_buffer;
int buffer_size;
int num_workers;
int total_files = 0;
int files_copied = 0;
int regular_files = 0;
int fifo_files = 0;
int directories = 0;
int symlinks = 0;
size_t total_bytes_copied = 0;
volatile sig_atomic_t stop = 0;

void handle_signal(int signal) {
    stop = 1;
}

void *worker_thread(void *arg) {
    while (1) {
        pthread_mutex_lock(&file_buffer.mutex);

        while (file_buffer.count == 0 && !file_buffer.done && !stop) {
            pthread_cond_wait(&file_buffer.not_empty, &file_buffer.mutex);
        }

        if (file_buffer.count == 0 && (file_buffer.done || stop)) {
            pthread_mutex_unlock(&file_buffer.mutex);
            break;
        }

        file_info_t file_info = file_buffer.buffer[file_buffer.out];
        file_buffer.out = (file_buffer.out + 1) % buffer_size;
        file_buffer.count--;

        pthread_cond_signal(&file_buffer.not_full);
        pthread_mutex_unlock(&file_buffer.mutex);

        char buffer[4096];
        ssize_t bytes_read, bytes_written;
        size_t total_bytes = 0;
        while ((bytes_read = read(file_info.src_fd, buffer, sizeof(buffer))) > 0) {
            bytes_written = write(file_info.dst_fd, buffer, bytes_read);
            if (bytes_written != bytes_read) {
                perror("write");
                break;
            }
            total_bytes += bytes_written;
        }

        close(file_info.src_fd);
        close(file_info.dst_fd);

        pthread_mutex_lock(&file_buffer.mutex);
        files_copied++;
        total_bytes_copied += total_bytes;
        pthread_mutex_unlock(&file_buffer.mutex);
    }

    return NULL;
}

void process_directory(const char *src_dir, const char *dst_dir) {
    DIR *dir = opendir(src_dir);
    if (!dir) {
        perror("opendir");
        return;
    }

    struct dirent *entry;
    while ((entry = readdir(dir)) != NULL && !stop) {
        if (strcmp(entry->d_name, ".") == 0 || strcmp(entry->d_name, "..") == 0) {
            continue;
        }

        char src_path[512], dst_path[512];
        snprintf(src_path, sizeof(src_path), "%s/%s", src_dir, entry->d_name);
        snprintf(dst_path, sizeof(dst_path), "%s/%s", dst_dir, entry->d_name);

        struct stat statbuf;
        if (lstat(src_path, &statbuf) == -1) {
            perror("lstat");
            continue;
        }

        if (S_ISREG(statbuf.st_mode)) {
            int src_fd = open(src_path, O_RDONLY);
            if (src_fd == -1) {
                perror("open src");
                continue;
            }

            int dst_fd = open(dst_path, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (dst_fd == -1) {
                close(src_fd);
                perror("open dst");
                continue;
            }

            pthread_mutex_lock(&file_buffer.mutex);

            while (file_buffer.count == buffer_size && !stop) {
                pthread_cond_wait(&file_buffer.not_full, &file_buffer.mutex);
            }

            if (stop) {
                close(src_fd);
                close(dst_fd);
                pthread_mutex_unlock(&file_buffer.mutex);
                break;
            }

            file_buffer.buffer[file_buffer.in].src_fd = src_fd;
            file_buffer.buffer[file_buffer.in].dst_fd = dst_fd;
            strncpy(file_buffer.buffer[file_buffer.in].src_name, src_path, sizeof(file_buffer.buffer[file_buffer.in].src_name));
            strncpy(file_buffer.buffer[file_buffer.in].dst_name, dst_path, sizeof(file_buffer.buffer[file_buffer.in].dst_name));
            file_buffer.in = (file_buffer.in + 1) % buffer_size;
            file_buffer.count++;

            total_files++;
            regular_files++;

            pthread_cond_signal(&file_buffer.not_empty);
            pthread_mutex_unlock(&file_buffer.mutex);
        } else if (S_ISDIR(statbuf.st_mode)) {
            mkdir(dst_path, 0755);
            directories++;
            process_directory(src_path, dst_path); // Recursive call to process the directory
        } else if (S_ISFIFO(statbuf.st_mode)) {
            fifo_files++;
        } else if (S_ISLNK(statbuf.st_mode)) {
            char link_target[512];
            ssize_t len = readlink(src_path, link_target, sizeof(link_target) - 1);
            if (len != -1) {
                link_target[len] = '\0';
                symlink(link_target, dst_path);
            }
            symlinks++;
        }
    }

    closedir(dir);
}

void *manager_thread(void *arg) {
    char **dirs = (char **)arg;
    char *src_dir = dirs[0];
    char *dst_dir = dirs[1];

    process_directory(src_dir, dst_dir);

    pthread_mutex_lock(&file_buffer.mutex);
    file_buffer.done = 1;
    pthread_cond_broadcast(&file_buffer.not_empty);
    pthread_mutex_unlock(&file_buffer.mutex);

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s <buffer_size> <num_workers> <src_dir> <dst_dir>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    buffer_size = atoi(argv[1]);
    num_workers = atoi(argv[2]);
    char *src_dir = argv[3];
    char *dst_dir = argv[4];

    if (buffer_size <= 0 || num_workers <= 0) {
        fprintf(stderr, "Usage: %s <buffer_size> <num_workers> <src_dir> <dst_dir>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    mkdir(dst_dir, 0755);

    file_buffer.in = 0;
    file_buffer.out = 0;
    file_buffer.count = 0;
    file_buffer.done = 0;
    pthread_mutex_init(&file_buffer.mutex, NULL);
    pthread_cond_init(&file_buffer.not_empty, NULL);
    pthread_cond_init(&file_buffer.not_full, NULL);

    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);

    // set up signal handler using sigaction
    struct sigaction sa;
    sa.sa_handler = handle_signal;
    sa.sa_flags = 0;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    char *dirs[2] = {src_dir, dst_dir};
    pthread_t manager;
    pthread_create(&manager, NULL, manager_thread, (void *)dirs);

    pthread_t workers[num_workers];
    for (int i = 0; i < num_workers; i++) {
        pthread_create(&workers[i], NULL, worker_thread, NULL);
    }

    pthread_join(manager, NULL);
    for (int i = 0; i < num_workers; i++) {
        pthread_join(workers[i], NULL);
    }

    gettimeofday(&end_time, NULL);
    double elapsed_time = (end_time.tv_sec - start_time.tv_sec) + (end_time.tv_usec - start_time.tv_usec) / 1e6;
    int minutes = (int)(elapsed_time / 60);
    double seconds = elapsed_time - (minutes * 60);

    pthread_mutex_destroy(&file_buffer.mutex);
    pthread_cond_destroy(&file_buffer.not_empty);
    pthread_cond_destroy(&file_buffer.not_full);

    printf("\n---------------STATISTICS---------------\n");
    printf("Workers: %d - Buffer Size: %d\n", num_workers, buffer_size);
    printf("Number of Regular Files: %d\n", regular_files);
    printf("Number of FIFOs: %d\n", fifo_files);
    printf("Number of Directories: %d\n", directories);
    printf("Number of Symbolic Links: %d\n", symlinks);
    printf("TOTAL BYTES COPIED: %zu\n", total_bytes_copied);
    printf("TOTAL TIME(min:sec.mili): %d:%06.3f\n", minutes, seconds);

    return 0;
}
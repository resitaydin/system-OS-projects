#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <signal.h>
#include <string.h>
#include <netinet/in.h>
#include <sys/time.h>

#define MAX_BUFFER 1024
#define MAX_ORDERS 100
#define MAX_COOKS 10
#define MAX_DELIVERIES 10
#define MAX_OVEN_SLOTS 6
#define MAX_DELIVERY_CAPACITY 3

typedef struct {
    int id;
    struct sockaddr_in client_addr;
    int x;
    int y;
} Client;

typedef struct {
    int id;
    int busy;
    pthread_t thread;
} Cook;

typedef struct {
    int id;
    int busy;
    int deliveries[MAX_DELIVERY_CAPACITY];
    int delivery_count;
    pthread_t thread;
} DeliveryPerson;

typedef struct {
    int id;
    int is_being_prepared;
    int is_in_oven;
    int is_ready;
    int is_delivered;
    int cancelled;
    Client client;
    struct timeval start_time;
    struct timeval end_time;
    int delivery_person_id;
} Order;

pthread_mutex_t orders_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t orders_cond = PTHREAD_COND_INITIALIZER;
Order orders[MAX_ORDERS];
int order_count = 0;
int delivered_count = 0; // Track the number of delivered orders

Cook cooks[MAX_COOKS];
DeliveryPerson delivery_persons[MAX_DELIVERIES];
int cook_pool_size;
int delivery_pool_size;
int delivery_speed;

FILE *log_file;

int client_gen_socket; // Socket to communicate with client generator

void handle_client(int client_socket, struct sockaddr_in client_addr);
void *cook_function(void *arg);
void *delivery_function(void *arg);
void pseudo_inverse_calculation();
void prepare_order(Order *order);
void cook_order(Order *order);
void deliver_order(Order *order);
void log_event(const char *event);
void check_and_notify_all_delivered();

void handle_sigint(int sig) {
    printf("Pide shop is shutting down...\n");
    log_event("Pide shop is shutting down");
    fclose(log_file);
    exit(0);
}

void *client_handler(void *arg) {
    int client_socket = *((int *)arg);
    free(arg); // Free the allocated memory for the client socket
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    getpeername(client_socket, (struct sockaddr *)&client_addr, &client_len);
    handle_client(client_socket, client_addr);
    close(client_socket);
    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 5) {
        fprintf(stderr, "Usage: %s [portnumber] [CookthreadPoolSize] [DeliveryPoolSize] [k]\n", argv[0]);
        exit(1);
    }

    int port = atoi(argv[1]);
    cook_pool_size = atoi(argv[2]);
    delivery_pool_size = atoi(argv[3]);
    delivery_speed = atoi(argv[4]);

    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_len = sizeof(client_addr);

    // Signal handler for SIGINT using sigaction
    struct sigaction act;
    act.sa_handler = handle_sigint;
    act.sa_flags = 0;
    if ((sigemptyset(&act.sa_mask) == -1) || (sigaction(SIGINT, &act, NULL) == -1)) {
        perror("Failed to set SIGINT signal handler");
        return 1;
    }

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error creating socket");
        exit(1);
    }

    printf("PideShop active, waiting for connection...\n");

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Error binding socket");
        exit(1);
    }

    if (listen(server_socket, 10) < 0) {
        perror("Error listening on socket");
        exit(1);
    }

    // Open log file for writing
    log_file = fopen("pideshop.log", "a");
    if (!log_file) {
        perror("Error opening log file");
        exit(1);
    }

    for (int i = 0; i < cook_pool_size; i++) {
        cooks[i].id = i;
        cooks[i].busy = 0;
        pthread_create(&cooks[i].thread, NULL, cook_function, &cooks[i]);
    }

    for (int i = 0; i < delivery_pool_size; i++) {
        delivery_persons[i].id = i;
        delivery_persons[i].busy = 0;
        delivery_persons[i].delivery_count = 0;
        pthread_create(&delivery_persons[i].thread, NULL, delivery_function, &delivery_persons[i]);
    }

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr, &client_len);
        if (client_socket < 0) {
            perror("Error accepting client");
            continue;
        }

        int *new_client_socket = malloc(sizeof(int)); // Allocate memory for the new client socket
        if (new_client_socket == NULL) {
            perror("Failed to allocate memory");
            close(client_socket);
            continue;
        }
        *new_client_socket = client_socket;

        if (client_gen_socket == 0) {
            client_gen_socket = client_socket;
            printf("Order from %s:%d\n", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
            // Handle client generator connection in a new thread
            pthread_t client_thread;
            pthread_create(&client_thread, NULL, client_handler, new_client_socket);
            pthread_detach(client_thread);
        } else {
            pthread_t client_thread;
            pthread_create(&client_thread, NULL, client_handler, new_client_socket);
            pthread_detach(client_thread);
        }
    }

    close(server_socket);
    fclose(log_file); // Close log file before exiting
    return 0;
}

void handle_client(int client_socket, struct sockaddr_in client_addr) {
    char buffer[MAX_BUFFER];
    bzero(buffer, MAX_BUFFER);
    read(client_socket, buffer, MAX_BUFFER);

    pthread_mutex_lock(&orders_mutex);

    if (order_count < MAX_ORDERS) {
        Order order;
        order.id = order_count;
        order.is_being_prepared = 0;
        order.is_in_oven = 0;
        order.is_ready = 0;
        order.is_delivered = 0;
        order.cancelled = 0;
        order.delivery_person_id = -1;
        order.client.client_addr = client_addr;
        gettimeofday(&order.start_time, NULL);

        // Initialize client coordinates to avoid uninitialized values
        order.client.x = 0;
        order.client.y = 0;
        
        sscanf(buffer, "Order from client %*d at location (%d, %d)", &order.client.x, &order.client.y);

        orders[order_count] = order;
        order_count++;

        char log_message[MAX_BUFFER];
        snprintf(log_message, MAX_BUFFER, "Received order from client: Order %d at location (%d, %d)", 
                 order.id, order.client.x, order.client.y);
        log_event(log_message);

        pthread_cond_signal(&orders_cond);
    } else {
        printf("Order limit reached. Cannot accept more orders.\n");
    }

    pthread_mutex_unlock(&orders_mutex);

    close(client_socket);
}

void *cook_function(void *arg) {
    Cook *cook = (Cook *)arg;
    while (1) {
        pthread_mutex_lock(&orders_mutex);

        int found_order = 0;
        for (int i = 0; i < order_count; i++) {
            if (!orders[i].is_being_prepared && !orders[i].is_in_oven && !orders[i].is_ready && !orders[i].is_delivered && !orders[i].cancelled) {
                orders[i].is_being_prepared = 1;
                cook->busy = 1;
                found_order = 1;

                pthread_mutex_unlock(&orders_mutex);

                char log_message[MAX_BUFFER];
                snprintf(log_message, MAX_BUFFER, "Cooker %d is preparing order %d", 
                         cook->id, orders[i].id);
                log_event(log_message);

                pseudo_inverse_calculation();
                prepare_order(&orders[i]);
                cook_order(&orders[i]);

                pthread_mutex_lock(&orders_mutex);
                orders[i].is_ready = 1;
                orders[i].is_in_oven = 0;
                cook->busy = 0;
                pthread_cond_broadcast(&orders_cond);
                gettimeofday(&orders[i].end_time, NULL);
                pthread_mutex_unlock(&orders_mutex);
                break;
            }
        }

        if (!found_order) {
            pthread_cond_wait(&orders_cond, &orders_mutex);
            pthread_mutex_unlock(&orders_mutex);
        }
    }
    return NULL;
}

void *delivery_function(void *arg) {
    DeliveryPerson *delivery_person = (DeliveryPerson *)arg;
    while (1) {
        pthread_mutex_lock(&orders_mutex);

        for (int i = 0; i < order_count; i++) {
            if (orders[i].is_ready && !orders[i].is_delivered && !orders[i].cancelled &&
                orders[i].delivery_person_id == -1 && !delivery_person->busy) {
                orders[i].delivery_person_id = delivery_person->id;
                delivery_person->deliveries[delivery_person->delivery_count++] = orders[i].id;
                if (delivery_person->delivery_count == MAX_DELIVERY_CAPACITY) {
                    break;
                }
            }
        }

        if (delivery_person->delivery_count > 0) {
            delivery_person->busy = 1;

            // Log the delivery person's status message
            char log_message[MAX_BUFFER];
            snprintf(log_message, MAX_BUFFER, "Delivery person %d is carrying orders: ", delivery_person->id);
            for (int i = 0; i < delivery_person->delivery_count; i++) {
                char order_message[MAX_BUFFER];
                snprintf(order_message, MAX_BUFFER, "%d ", delivery_person->deliveries[i]);
                strcat(log_message, order_message);
            }
            log_event(log_message);

            pthread_mutex_unlock(&orders_mutex);

            for (int i = 0; i < delivery_person->delivery_count; i++) {
                deliver_order(&orders[delivery_person->deliveries[i]]);
            }

            pthread_mutex_lock(&orders_mutex);
            delivery_person->delivery_count = 0;
            delivery_person->busy = 0;
            pthread_cond_broadcast(&orders_cond);
            pthread_mutex_unlock(&orders_mutex);
        } else {
            pthread_cond_wait(&orders_cond, &orders_mutex);
            pthread_mutex_unlock(&orders_mutex);
        }
    }
    return NULL;
}

void pseudo_inverse_calculation() {
    sleep(3);
}

void prepare_order(Order *order) {
    sleep(2);
}

void cook_order(Order *order) {
    sleep(2);
}

void deliver_order(Order *order) {
    sleep(delivery_speed);

    pthread_mutex_lock(&orders_mutex);
    order->is_delivered = 1;
    delivered_count++; // Increment delivered orders count
    check_and_notify_all_delivered();
    pthread_mutex_unlock(&orders_mutex);

    char log_message[MAX_BUFFER];
    snprintf(log_message, MAX_BUFFER, "Delivered %d to location (%d, %d)", 
             order->id, order->client.x, order->client.y);
    log_event(log_message);
}

void check_and_notify_all_delivered() {
    if (delivered_count == order_count) {
        char buffer[MAX_BUFFER];
        snprintf(buffer, MAX_BUFFER, "All orders delivered");

        // Log the event
        //log_event(buffer);

        // Print the message to the server console
        printf("Done serving client @ XXX\n");
        printf("PideShop is active, waiting for connection...\n");

        // Send the notification to the client generator
        write(client_gen_socket, buffer, strlen(buffer));
        close(client_gen_socket); // Close the connection after notifying
        client_gen_socket = 0; // Reset the client generator socket for the next session
        delivered_count = 0; // Reset delivered count for next session
        order_count = 0; // Reset order count for next session
    }
}

void log_event(const char *event) {
    struct timeval tv;
    gettimeofday(&tv, NULL);

    struct tm *tm_info;
    char time_buffer[26];
    tm_info = localtime(&tv.tv_sec);
    strftime(time_buffer, 26, "%Y-%m-%d %H:%M:%S", tm_info);

    fprintf(log_file, "[%s] %s\n", time_buffer, event);
    fflush(log_file);
}

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <semaphore.h>
#include <unistd.h>

// Constants for parking capacities
#define MAX_PICKUPS 4
#define MAX_AUTOMOBILES 8

// Semaphore declarations
sem_t newPickup;
sem_t newAutomobile;
sem_t inChargeforPickup;
sem_t inChargeforAutomobile;

// Mutexes for shared resource protection
pthread_mutex_t mtxFreePickup = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t mtxFreeAutomobile = PTHREAD_MUTEX_INITIALIZER;

// Counters for free parking spaces
int mFree_pickup = MAX_PICKUPS;
int mFree_automobile = MAX_AUTOMOBILES;

void* carOwner(void* arg) {
    int vehicleType = *(int*)arg;
    free(arg);
    
    if (vehicleType % 2 == 0) {
        // Pickup
        pthread_mutex_lock(&mtxFreePickup);
        if (mFree_pickup > 0) {
            mFree_pickup--;
            sleep(1);
            printf("Pickup owner parked in a temporary lot. Available pickup spots: %d\n", mFree_pickup);
            if(mFree_pickup == 0) {
                printf("No available pickup spots.\n");
            }
            pthread_mutex_unlock(&mtxFreePickup);
            sem_post(&newPickup);  // Signal that there is a new pickup
            sem_wait(&inChargeforPickup);  // Wait for the valet to take charge
        } else {
            printf("Pickup owner leaves due to no available temporary spots.\n");
            pthread_mutex_unlock(&mtxFreePickup);
        }
    } else {
        // Automobile
        pthread_mutex_lock(&mtxFreeAutomobile);
        if (mFree_automobile > 0) {
            mFree_automobile--;
            sleep(1);
            printf("Automobile owner parked in a temporary lot. Available automobile spots: %d\n", mFree_automobile);
            if(mFree_automobile == 0) {
                printf("No available automobile spots.\n");
            }
            pthread_mutex_unlock(&mtxFreeAutomobile);
            sem_post(&newAutomobile);  // Signal that there is a new automobile
            sem_wait(&inChargeforAutomobile);  // Wait for the valet to take charge
        } else {
            printf("Automobile owner leaves due to no available temporary spots.\n");
            pthread_mutex_unlock(&mtxFreeAutomobile);
        }
    }
    return NULL;
}


void* carAttendant(void* arg) {
    int vehicleType = *(int*)arg;

    while (1) {
        if (vehicleType == 0) {
            // Pickup valet
            sem_wait(&newPickup);  // Wait for a new pickup to arrive
            sleep(1);  // Simulate time to park the vehicle
            pthread_mutex_lock(&mtxFreePickup);
            mFree_pickup++;
            printf("Pickup taken by the valet. Available pickup spots: %d\n", mFree_pickup);
            pthread_mutex_unlock(&mtxFreePickup);
            sem_post(&inChargeforPickup);  // Signal that the valet has taken charge
        } else {
            // Automobile valet
            sem_wait(&newAutomobile);  // Wait for a new automobile to arrive
            sleep(1);  // Simulate time to park the vehicle
            pthread_mutex_lock(&mtxFreeAutomobile);
            mFree_automobile++;
            printf("Automobile taken by the valet. Available automobile spots: %d\n", mFree_automobile);
            pthread_mutex_unlock(&mtxFreeAutomobile);
            sem_post(&inChargeforAutomobile);  // Signal that the valet has taken charge
        }
    }
    return NULL;
}

void simulateNoTemporarySpots() {
    pthread_t ownerThreads[MAX_PICKUPS + MAX_AUTOMOBILES];
    int total_vehicles = MAX_PICKUPS + MAX_AUTOMOBILES;
    int parked_vehicles = 0;

    // Simulate car owners filling all temporary spots
    for (int i = 0; i < MAX_PICKUPS; i++) {
        int *vehicleType = malloc(sizeof(int));
        *vehicleType = 0;  // Pickup
        pthread_create(&ownerThreads[i], NULL, carOwner, vehicleType);
        parked_vehicles++;
    }

    for (int i = 0; i < MAX_AUTOMOBILES; i++) {
        int *vehicleType = malloc(sizeof(int));
        *vehicleType = 1;  // Automobile
        pthread_create(&ownerThreads[MAX_PICKUPS + i], NULL, carOwner, vehicleType);
        parked_vehicles++;
    }

    // Wait for all car owner threads to finish
    while (parked_vehicles > 0) {
        parked_vehicles--;
        pthread_join(ownerThreads[parked_vehicles], NULL);
    }

    // Attempt to park one more vehicle of each type
    int *extraPickup = malloc(sizeof(int));
    *extraPickup = 0;  // Pickup

    pthread_t extraPickupThread;
    pthread_create(&extraPickupThread, NULL, carOwner, extraPickup);
    pthread_join(extraPickupThread, NULL);

    int *extraAutomobile = malloc(sizeof(int));
    *extraAutomobile = 1;  // Automobile

    pthread_t extraAutomobileThread;
    pthread_create(&extraAutomobileThread, NULL, carOwner, extraAutomobile);
    pthread_join(extraAutomobileThread, NULL);
}

int main() {
    pthread_t pickupAttendant, automobileAttendant;

    // Initialize semaphores
    sem_init(&newPickup, 0, 0);
    sem_init(&newAutomobile, 0, 0);
    sem_init(&inChargeforPickup, 0, 0);
    sem_init(&inChargeforAutomobile, 0, 0);

    // Create car attendant threads
    int *pickupArg = malloc(sizeof(int));
    *pickupArg = 0;
    pthread_create(&pickupAttendant, NULL, carAttendant, pickupArg);
    
    int *automobileArg = malloc(sizeof(int));
    *automobileArg = 1;
    pthread_create(&automobileAttendant, NULL, carAttendant, automobileArg);

    simulateNoTemporarySpots();

    // while (1) {
    //     int *vehicleType = malloc(sizeof(int));
    //     *vehicleType = rand() % 2;

    //     pthread_t ownerThread;
    //     pthread_create(&ownerThread, NULL, carOwner, vehicleType);
    //     pthread_detach(ownerThread);  // to clean up the thread resources

    //     sleep(1);  // Simulate arrival of vehicles
    // }

    // Cleanup
    pthread_mutex_destroy(&mtxFreePickup);
    pthread_mutex_destroy(&mtxFreeAutomobile);
    sem_destroy(&newPickup);
    sem_destroy(&newAutomobile);
    sem_destroy(&inChargeforPickup);
    sem_destroy(&inChargeforAutomobile);

    return 0;
}

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_BOOKINGS 100
#define MAX_RESOURCES 3
#define PARKING_SLOTS 10

typedef struct {
    char memberName[20];
    char date[11];
    char time[6];
    float duration;
    char essentials[3][20];
    int priority;           
    int parkingSlot;        
    int accepted;           
} Booking;

typedef struct {
    Booking bookings[MAX_BOOKINGS];
    int count;
} BookingList;

Booking bookings[MAX_BOOKINGS];
int totalBookings = 0;
int parkingAvailability[PARKING_SLOTS];
int resourceAvailability[MAX_RESOURCES] = {3, 3, 3}; 

pthread_mutex_t lock;

// Prototypes
void addBooking(char *memberName, char *date, char *time, float duration, char essentials[3][20], int priority);
void processBookingsFCFS();
void processBookingsPriority();
void processBookingsOptimized();
void printBookings(const char *algorithm);
int allocateResources(char essentials[3][20]);
void releaseResources(char essentials[3][20]);


void addBooking(char *memberName, char *date, char *time, float duration, char essentials[3][20], int priority) {
    // printf("\n"); // padding with newline
    // printf("===== Booking details ===== \n");
    // printf("Member name: %s\n", memberName);
    // printf("Booking date: %s\n", date);
    // printf("Booking time: %s\n", time);
    // printf("Booking duration: %d\n", duration);
    // printf("Essentials booked: %s\n", essentials);
    // printf("Priority: %i\n", priority);

    
    pthread_mutex_lock(&lock);
    if (totalBookings >= MAX_BOOKINGS) {
        printf("Booking limit reached (Maximum: %i). Cannot add more bookings.\n", MAX_BOOKINGS);
    } else {
        Booking *b = &bookings[totalBookings];
        strcpy(b->memberName, memberName);
        strcpy(b->date, date);
        strcpy(b->time, time);
        b->duration = duration;
        b->priority = priority;
        b->parkingSlot = -1; // Has not been assigned yet
        b->accepted = 0;    // Default to rejected status

        for (int i = 0; i < 3; i++) {
            strcpy(b->essentials[i], essentials[i]);
        }

        totalBookings++;
        printf("Booking added: %s on %s at %s for %.1f hours.\n", memberName, date, time, duration);

        printf("Essentials booked: ");
        if (essentials[1] == NULL && essentials[2] == NULL) {
            printf("None\n");
        }
        else {
            printf("%s", essentials[1]);
            if (essentials[2] != NULL) {
                printf(", %s", essentials[2]);
            }
        } 

    }

    pthread_mutex_unlock(&lock);
}

void processBooking_FCFC() {
    pthread_mutex_lock(&lock);

    for (int i = 0; i < totalBookings; i++) {
        Booking *b = &bookings[i];

        // Check if a parking slot is available
        int slotFound = 0;
        for (int j = 0; j < PARKING_SLOTS; j++) {
            if (parkingAvailability[j] == 1) { // Slot is available
                parkingAvailability[j] = 0;   // Mark as occupied
                b->parkingSlot = j;
                slotFound = 1;
                break;
            }
        }

        // Check if resources are available
        int resourcesAllocated = allocateResources(b->essentials);

        if (slotFound && resourcesAllocated) {
            b->accepted = 1; // Booking accepted
        } 
        else {
            // Revert parking slot if resources cannot be allocated
            if (slotFound) {
                parkingAvailability[b->parkingSlot] = 1;
                b->parkingSlot = -1;
            }
            b->accepted = 0; // Booking rejected
        }
    }

    pthread_mutex_unlock(&lock);
}


int compareBookings(const void *a, const void *b) {
    Booking *bookingA = (Booking *)a;
    Booking *bookingB = (Booking *)b;
    return bookingA->priority - bookingB->priority;
}

// Smaller priority value means higher priority.
void processBookingPriority() {
    pthread_mutex_lock(&lock);

    // Sort bookings based on priority

    // Placeholder bubble sort with O(n^2) time complexity
    //
    // for (int i = 0; i < totalBookings - 1; i++) {
    //     for (int j = i + 1; j < totalBookings; j++) {
    //         if (bookings[i].priority > bookings[j].priority) {
    //             Booking temp = bookings[i];
    //             bookings[i] = bookings[j];
    //             bookings[j] = temp;
    //         }
    //     }
    // }

    // use quick sort (better since time complexity is O(n log n)) (haven't tested yet)
    qsort(bookings, totalBookings, sizeof(Booking), compareBookings);

    processBooking_FCFC(); // Use FCFS processing after sorting by priority

    pthread_mutex_unlock(&lock);
}

// Function to process bookings using Optimized
void processBookingsOptimized() {
    pthread_mutex_lock(&lock);

    // TODO: Implement optimized scheduling logic (e.g., rescheduling rejected bookings)

    pthread_mutex_unlock(&lock);
}

// Function to allocate resources for a booking
int allocateResources(char essentials[3][20]) {
    int resourceCount[MAX_RESOURCES] = {0};

    for (int i = 0; i < 3; i++) {
        if (strcmp(essentials[i], "battery") == 0) resourceCount[0]++;
        else if (strcmp(essentials[i], "cable") == 0) resourceCount[1]++;
        else if (strcmp(essentials[i], "locker") == 0) resourceCount[2]++;
    }

    // Check if resources are available
    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (resourceCount[i] > resourceAvailability[i]) {
            return 0; // Resources not available
        }
    }

    // Deduct resources
    for (int i = 0; i < MAX_RESOURCES; i++) {
        resourceAvailability[i] -= resourceCount[i];
    }

    return 1; // Resources allocated
}

// Function to release resources after a booking
void releaseResources(char essentials[3][20]) {
    for (int i = 0; i < 3; i++) {
        if (strcmp(essentials[i], "battery") == 0) resourceAvailability[0]++;
        else if (strcmp(essentials[i], "cable") == 0) resourceAvailability[1]++;
        else if (strcmp(essentials[i], "locker") == 0) resourceAvailability[2]++;
    }
}

// Function to print bookings
void printBookings(const char *algorithm) {
    pthread_mutex_lock(&lock);

    printf("\n*** Booking Schedule (%s) ***\n", algorithm);
    for (int i = 0; i < totalBookings; i++) {
        Booking *b = &bookings[i];
        if (b->accepted) {
            printf("ACCEPTED: %s on %s at %s for %.1f hours. Slot: %d\n", b->memberName, b->date, b->time, b->duration, b->parkingSlot);
        } else {
            printf("REJECTED: %s on %s at %s for %.1f hours.\n", b->memberName, b->date, b->time, b->duration);
        }
    }

    pthread_mutex_unlock(&lock);
}


int main() {
    printf("~~ WELCOME TO POLYU! ~~\n");
    
    char command[128];
    char memberName[20];
    char date[11]; 
    char time[6];
    char essentials[3][20];
    float duration;
    int priority;


    for (int i = 0; i < PARKING_SLOTS; i++) {
        parkingAvailability[i] = 1; 
    }

    // Mutex initialization
    pthread_mutex_init(&lock, NULL);


    while (1) {
        // printf("%s\n", token); 
        printf("Please enter booking: \n");
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = 0;


        if (strncmp(command, "addParking", 10) == 0) {
            sscanf(command, "addParking -%s %s %s %f %s %s", memberName, date, time, &duration, essentials[0], essentials[1]);
            // Priority for parking is 3
            // Sample command: addParking -Taylor 2025-05-10 09:00 2.0 8 locker umbrella 
            addBooking(memberName, date, time, duration, essentials, 3);
        }
        else if (strncmp(command, "addReservation", 14) == 0) {
            // Priority for reservation is 2
        } 
        else if (strncmp(command, "addEvent", 8) == 0) {
            // Priority for event is 1
        }
        else if (strncmp(command, "processBookings -fcfs", 21) == 0) {
            // processBookings_FCFS();
        } 
        else if (strncmp(command, "processBookings -prio", 21) == 0) {
            // processBookings_Priority();
        } 
        else if (strncmp(command, "processBookings -opti", 21) == 0) {
            // processBookings_Optimized();
        } 
        else if (strncmp(command, "printBookings", 13) == 0) {
            // printBookings("ALL");
        } 
        else if (strncmp(command, "endProgram", 10) == 0) {
            printf("Bye!\n");
            break;
        } 
        else {
            printf("Command is not recognized. Please try again.\n");
        }
    }

    pthread_mutex_destroy(&lock);
    return 0;
}


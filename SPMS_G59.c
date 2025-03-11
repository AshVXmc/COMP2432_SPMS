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
            printf("None\n")
        }
        else {
            printf("%s", essentials[1]);
            if (essentials[2] != NULL) {
                printf(", %s", essentials[2])
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
    qsort(bookings, totalBookings, sizeof(Booking), compareBookings)

    processBookingsFCFS(); // Use FCFS processing after sorting by priority

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
            // processBookingsFCFS();
        } 
        else if (strncmp(command, "processBookings -prio", 21) == 0) {
            // processBookingsPriority();
        } 
        else if (strncmp(command, "processBookings -opti", 21) == 0) {
            // processBookingsOptimized();
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



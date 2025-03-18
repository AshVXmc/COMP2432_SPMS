#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_BOOKINGS 100
#define MAX_RESOURCES 6
#define PARKING_SLOTS 10
#define TIME_SLOTS 24

typedef struct {
    char memberName[20];
    char date[11];
    char time[6];
    float duration;
    char essentials[MAX_RESOURCES][20];
    int priority;           
    int parkingSlot;        
    int accepted;           // 1 = accepted, 0 = rejected
} Booking;

typedef struct {
    Booking bookings[MAX_BOOKINGS];
    int count;
} BookingList;

Booking bookings[MAX_BOOKINGS];
int totalBookings = 0;
int parkingAvailability[TIME_SLOTS][PARKING_SLOTS] = {0}; // 0 means available
int resourceAvailability[TIME_SLOTS][MAX_RESOURCES] = {0}; 

enum PRIORITIES {
    PRIORITY_PARKING = 3,
    PRIORITY_EVENT = 1,
    PRIORITY_RESERVATION = 2
}

// Mutex variables
pthread_mutex_t lock;

// Prototypes
void addBooking(char *memberName, char *date, char *time, float duration, char essentials[MAX_RESOURCES][20], int priority);
void processBookingsFCFS();
void processBookingsPriority();
void processBookingsOptimized();
void printBookings(const char *algorithm);
int allocateResources(int startSlot, int durationSlots, char essentials[MAX_RESOURCES][20]);
void releaseResources(int startSlot, int durationSlots, char essentials[MAX_RESOURCES][20]);
int timeToSlot(char *time);
int compareBookings(const void *a, const void *b);


int main() {
    char command[128];
    char memberName[20];
    char date[11]; 
    char time[6];
    char essentials[MAX_RESOURCES][20];
    float duration;
    int priority;

    // Set parking and resource availability to its initial state.
    memset(parkingAvailability, 1, sizeof(parkingAvailability)); // 1 means available
    for (int i = 0; i < TIME_SLOTS; i++) {
        for (int j = 0; j < MAX_RESOURCES; j++) {
            resourceAvailability[i][j] = MAX_RESOURCES;
        }
    }
    // Initialize Mutex
    pthread_mutex_init(&lock, NULL);


    printf("~~ WELCOME TO POLYU! ~~\n");
    while (1) {
        // printf("%s\n", token); 
        printf("Please enter booking: \n");
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = 0;


        if (strncmp(command, "addParking", 10) == 0) {
            // Priority for parking is 3
            // Sample command: addParking -Castorice 2025-05-10 09:00 2.0 8 locker umbrella 
            sscanf(command, "addParking -%s %s %s %f %s %s", memberName, date, time, &duration, essentials[0], essentials[1]);
            addBooking(memberName, date, time, duration, essentials, PRIORITY_PARKING);
        }
        else if (strncmp(command, "addReservation", 14) == 0) {
            // Priority for reservation is 2
            // Sample command: addReservation -Mydei 2025-05-14 08:00 3.0 battery cable
            sscanf(command, "addReservation -%s %s %s %f %s %s", memberName, date, time, &duration, essentials[0], essentials[1]);
            addBooking(memberName, date, time, duration, essentials, PRIORITY_RESERVATION); 
        } 
        else if (strncmp(command, "addEvent", 8) == 0) {
            // Priority for event is 1

            sscanf(command, "addEvent -%s %s %s %f %s %s", memberName, date, time, &duration, essentials[0], essentials[1]);
            addBooking(memberName, date, time, duration, essentials, PRIORITY_EVENT); 
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


void addBooking(char *memberName, char *date, char *time, float duration, char essentials[MAX_RESOURCES][20], int priority) {
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
    } 
    else {
        Booking *b = &bookings[totalBookings];
        strcpy(b->memberName, memberName);
        strcpy(b->date, date);
        strcpy(b->time, time);
        b->duration = duration;
        b->priority = priority;
        b->parkingSlot = -1; // Has not been assigned yet
        b->accepted = 0;    // Default to rejected status

        for (int i = 0; i < MAX_RESOURCES; i++) {
            strcpy(b->essentials[i], essentials[i]);
        }

        totalBookings++;
        printf("Booking added: %s on %s at %s for %.2f hours.\n", memberName, date, time, duration);
    }

    pthread_mutex_unlock(&lock);
}

// First Come First Serve scheduling.
void processBookings_FCFS() {
    pthread_mutex_lock(&lock);

    for (int i = 0; i < totalBookings; i++) {
        Booking *b = &bookings[i];
        int startSlot = timeToSlot(b->time);
        int durationSlots = (int)b->duration;

        // Check parking slot availability
        int slotFound = -1;
        for (int j = 0; j < PARKING_SLOTS; j++) {
            int available = 1;
            for (int k = startSlot; k < startSlot + durationSlots; k++) {
                if (parkingAvailability[k][j] == 0) {
                    available = 0;
                    break;
                }
            }
            if (available) {
                slotFound = j;
                break;
            }
        }

        // Check resource availability
        int resourcesAllocated = allocateResources(startSlot, durationSlots, b->essentials);

        if (slotFound != -1 && resourcesAllocated) {
            for (int k = startSlot; k < startSlot + durationSlots; k++) {
                parkingAvailability[k][slotFound] = 0; // Mark parking slot as occupied
            }
            b->parkingSlot = slotFound; 
            b->accepted = 1; // Booking accepted
        } 
        else {
            releaseResources(startSlot, durationSlots, b->essentials); // Release resources if not allocated
            b->accepted = 0; // Booking rejected
        }
    }

    pthread_mutex_unlock(&lock);
}

// Higher priority value means higher priority.
void processBookings_Priority() {
    pthread_mutex_lock(&lock);

    // Sort bookings based on priority
    // quicksort average time complexity is O(n log n)
    qsort(bookings, totalBookings, sizeof(Booking), compareBookings);

    processBookings_FCFS(); // Use FCFS processing after sorting by priority

    pthread_mutex_unlock(&lock);
}


// Optimized (allegedly)
void processBookings_Optimized() {
    pthread_mutex_lock(&lock);

    // TODO: Implement optimized scheduling logic (e.g., rescheduling rejected bookings)
    // First step: Process all bookings using FCFS
    processBookings_FCFS();

    // Second step: Attempt to reschedule rejected bookings
    for (int i = 0; i < totalBookings; i++) {
        Booking *b = &bookings[i];
        if (b->accepted == 0) { // If rejected
            int startSlot = timeToSlot(b->time);
            int durationSlots = (int)b->duration;

            // Try to find a new time slot and resources
            for (int newStartSlot = 0; newStartSlot < TIME_SLOTS - durationSlots; newStartSlot++) {
                int slotFound = -1;

                // Check parking slot availability
                for (int j = 0; j < PARKING_SLOTS; j++) {
                    int available = 1;
                    for (int k = newStartSlot; k < newStartSlot + durationSlots; k++) {
                        if (parkingAvailability[k][j] == 0) {
                            available = 0;
                            break;
                        }
                    }
                    if (available) {
                        slotFound = j;
                        break;
                    }
                }

                // Check resource availability
                int resourcesAllocated = allocateResources(newStartSlot, durationSlots, b->essentials);

                if (slotFound != -1 && resourcesAllocated) {
                    for (int k = newStartSlot; k < newStartSlot + durationSlots; k++) {
                        parkingAvailability[k][slotFound] = 0; // Mark parking slot as occupied
                    }
                    b->parkingSlot = slotFound;
                    b->accepted = 1; // Booking accepted
                    break;
                } else {
                    releaseResources(newStartSlot, durationSlots, b->essentials); // Release resources if not allocated
                }
            }
        }
    }

    pthread_mutex_unlock(&lock);
}

// utility function for qsort()
int compareBookings(const void *a, const void *b) {
    Booking *bookingA = (Booking *)a;
    Booking *bookingB = (Booking *)b;
    return bookingA->priority - bookingB->priority;
}



// Function to allocate resources for a booking
int allocateResources(int startSlot, int durationSlots, char essentials[MAX_RESOURCES][20]) {
    int resourceCount[MAX_RESOURCES] = {0};

    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (strcmp(essentials[i], "battery") == 0) resourceCount[0]++;
        else if (strcmp(essentials[i], "cable") == 0) resourceCount[1]++;
        else if (strcmp(essentials[i], "locker") == 0) resourceCount[2]++;
        else if (strcmp(essentials[i], "umbrella") == 0) resourceCount[3]++;
        else if (strcmp(essentials[i], "inflation_service") == 0) resourceCount[4]++;
        else if (strcmp(essentials[i], "valet_parking") == 0) resourceCount[5]++;
    }

    // Check if resources are available
    for (int i = 0; i < MAX_RESOURCES; i++) {
        for (int j = startSlot; j < startSlot + durationSlots; j++) {
            if (resourceCount[i] > resourceAvailability[j][i]) {
                return 0; // Resources not available
            }
        }
    }

    // Deduct resources
    for (int i = 0; i < MAX_RESOURCES; i++) {
        for (int j = startSlot; j < startSlot + durationSlots; j++) {
            resourceAvailability[j][i] -= resourceCount[i];
        }
    }

    return 1; // Resources allocated
}

// Function to release resources after a booking
void releaseResources(int startSlot, int durationSlots, char essentials[MAX_RESOURCES][20]) {
    for (int i = 0; i < 3; i++) {
        int resourceType = -1;
        if (strcmp(essentials[i], "battery") == 0) resourceType = 0;
        else if (strcmp(essentials[i], "cable") == 0) resourceType = 1;
        else if (strcmp(essentials[i], "locker") == 0) resourceType = 2;
        else if (strcmp(essentials[i], "umbrella") == 0) resourceType = 3;
        else if (strcmp(essentials[i], "inflation_service") == 0) resourceType = 4;
        else if (strcmp(essentials[i], "valet_parking") == 0) resourceType = 5;
    

        if (resourceType != -1) {
            for (int j = startSlot; j < startSlot + durationSlots; j++) {
                resourceAvailability[j][resourceType]++;
            }
        }
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

// Function to convert time to a time slot index
int timeToSlot(char *time) {
    int hour;
    sscanf(time, "%d", &hour);
    return hour;
}


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>

#define MAX_BOOKINGS 100
#define MAX_RESOURCES 6
#define PARKING_SLOTS 10
#define TIME_SLOTS 24
#define RESOURCE_STOCK 3 // 3 of each resource/essential category is available.

typedef struct {
    char memberName[20];
    char date[11];
    char time[6];
    float duration;
    char essentials[MAX_RESOURCES][20];
    int priority;           
    int parkingSlot;        
    int accepted;           // 1 = accepted, 0 = rejected
    char reasonForRejection[256]; // why the booking is rejected (if applicable)
} Booking;

typedef struct {
    Booking bookings[MAX_BOOKINGS];
    int count;
} BookingList;

Booking bookings[MAX_BOOKINGS];
int totalBookings = 0;
int parkingAvailability[TIME_SLOTS][PARKING_SLOTS] = {0}; // 0 means available
int resourceAvailability[TIME_SLOTS][MAX_RESOURCES] = {0}; 

// the lower the priority value, the higher the priority.
enum PRIORITIES {
    PRIORITY_EVENT = 1,
    PRIORITY_RESERVATION = 2,
    PRIORITY_PARKING = 3,
    PRIORITY_ESSENTIAL = 4
};

const char *resourceNames[MAX_RESOURCES] = {
    "battery",
    "cable",
    "locker",
    "umbrella",
    "inflation",
    "valetpark"
};

// Prototypes
void addBooking(char *memberName, char *date, char *time, float duration, char essentials[MAX_RESOURCES][20], int priority);
void processBookings_FCFS();
void processBookings_Priority();
void processBookings_Optimized();
void printBookings(const char *algorithm);
int allocateResources(int startSlot, int durationSlots, char essentials[MAX_RESOURCES][20]);
void releaseResources(int startSlot, int durationSlots, char essentials[MAX_RESOURCES][20]);
int timeToSlot(char *time);
int compareBookings(const void *a, const void *b);
int getResourceIndex(const char *resourceName);
int contains(char essentials[MAX_RESOURCES][20], const char *item);
void suggestAlternativeSlots(int durationSlots, char *memberName, char *date, char *time);
char* calculateEndTime(const char* startTime, float duration);
const char* getBookingType(int priority);
void generateSummaryReport();

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
            resourceAvailability[i][j] = RESOURCE_STOCK;
        }
    }

    printf("~~ WELCOME TO POLYU! ~~\n");
    while (1) {
        printf("Please enter booking: \n");
        fgets(command, sizeof(command), stdin);
        command[strcspn(command, "\n")] = 0;

        if (strncmp(command, "addParking", 10) == 0) {
            sscanf(command, "addParking -%s %s %s %f %s %s %s %s %s %s %s", memberName, date, time, &duration, essentials[0], essentials[1], essentials[2], essentials[3], essentials[4], essentials[5], essentials[6]);
            addBooking(memberName, date, time, duration, essentials, PRIORITY_PARKING);
        }
        else if (strncmp(command, "addReservation", 14) == 0) {
            sscanf(command, "addReservation -%s %s %s %f %s %s %s %s %s %s %s", memberName, date, time, &duration, essentials[0], essentials[1], essentials[2], essentials[3], essentials[4], essentials[5], essentials[6]);
            addBooking(memberName, date, time, duration, essentials, PRIORITY_RESERVATION); 
        } 
        else if (strncmp(command, "addEvent", 8) == 0) {
            sscanf(command, "addEvent -%s %s %s %f %s %s %s %s %s %s %s", memberName, date, time, &duration, essentials[0], essentials[1], essentials[2], essentials[3], essentials[4], essentials[5], essentials[6]);
            addBooking(memberName, date, time, duration, essentials, PRIORITY_EVENT); 
        }
        else if (strncmp(command, "bookEssentials", 14) == 0) {
            for (int i = 0; i < MAX_RESOURCES; i++) {
                strcpy(essentials[i], "");
            }
            sscanf(command, "bookEssentials -%s %s %s %f %s", memberName, date, time, &duration, essentials[0]);
            addBooking(memberName, date, time, duration, essentials, PRIORITY_ESSENTIAL);
            printf("-> [Pending]\n");
        }
        else if (strncmp(command, "processBookings -fcfs", 21) == 0) {
            if (totalBookings > 0) {
                int pipe_fd[2];
                if (pipe(pipe_fd) == -1) {
                    perror("Pipe creation failed");
                    exit(1);
                }
                pid_t pid = fork();
                if (pid < 0) {
                    perror("Fork failed");
                    exit(1);
                }
                if (pid == 0) { // child process
                    close(pipe_fd[0]);
                    processBookings_FCFS();
                    write(pipe_fd[1], bookings, sizeof(bookings));
                    close(pipe_fd[1]);
                    exit(0);
                } else {
                    close(pipe_fd[1]);
                    wait(NULL);
                    read(pipe_fd[0], bookings, sizeof(bookings));
                    close(pipe_fd[0]);
                }
            } else {
                printf("No booking(s) have been made.\n");
            }
        } 
        else if (strncmp(command, "processBookings -prio", 21) == 0) {
            if (totalBookings > 0) {
                int pipe_fd[2];
                if (pipe(pipe_fd) == -1) {
                    perror("Pipe creation failed");
                    exit(1);
                }
                pid_t pid = fork();
                if (pid < 0) {
                    perror("Fork failed");
                    exit(1);
                }
                if (pid == 0) {
                    close(pipe_fd[0]);
                    processBookings_Priority();
                    write(pipe_fd[1], bookings, sizeof(bookings));
                    close(pipe_fd[1]);
                    exit(0);
                } else {
                    close(pipe_fd[1]);
                    wait(NULL);
                    read(pipe_fd[0], bookings, sizeof(bookings));
                    close(pipe_fd[0]);
                }
            } else {
                printf("No booking(s) have been made.\n");
            }
        } 
        else if (strncmp(command, "processBookings -opti", 21) == 0) {
            if (totalBookings > 0) {
                int pipe_fd[2];
                if (pipe(pipe_fd) == -1) {
                    perror("Pipe creation failed");
                    exit(1);
                }
                pid_t pid = fork();
                if (pid < 0) {
                    perror("Fork failed");
                    exit(1);
                }
                if (pid == 0) {
                    close(pipe_fd[0]);
                    processBookings_Optimized();
                    write(pipe_fd[1], bookings, sizeof(bookings));
                    close(pipe_fd[1]);
                    exit(0);
                } else {
                    close(pipe_fd[1]);
                    wait(NULL);
                    read(pipe_fd[0], bookings, sizeof(bookings));
                    close(pipe_fd[0]);
                }
            } else {
                printf("No booking(s) have been made.\n");
            }
        } 
        else if (strncmp(command, "printBookings -fcfs", 19) == 0) {
            if (totalBookings > 0) {
                int pipe_fd[2];
                if (pipe(pipe_fd) == -1) {
                    perror("Pipe creation failed");
                    exit(1);
                }
                pid_t pid = fork();
                if (pid < 0) {
                    perror("Fork failed");
                    exit(1);
                }
                if (pid == 0) {
                    close(pipe_fd[0]);
                    processBookings_FCFS();
                    write(pipe_fd[1], bookings, sizeof(bookings));
                    close(pipe_fd[1]);
                    exit(0);
                } else {
                    close(pipe_fd[1]);
                    wait(NULL);
                    read(pipe_fd[0], bookings, sizeof(bookings));
                    close(pipe_fd[0]);
                    printBookings("FCFS");
                }
            } else {
                printf("No booking(s) have been made.\n");
            }
        }
        else if (strncmp(command, "printBookings -prio", 19) == 0) {
            if (totalBookings > 0) {
                int pipe_fd[2];
                if (pipe(pipe_fd) == -1) {
                    perror("Pipe creation failed");
                    exit(1);
                }
                pid_t pid = fork();
                if (pid < 0) {
                    perror("Fork failed");
                    exit(1);
                }
                if (pid == 0) {
                    close(pipe_fd[0]);
                    processBookings_Priority();
                    write(pipe_fd[1], bookings, sizeof(bookings));
                    close(pipe_fd[1]);
                    exit(0);
                } else {
                    close(pipe_fd[1]);
                    wait(NULL);
                    read(pipe_fd[0], bookings, sizeof(bookings));
                    close(pipe_fd[0]);
                    printBookings("PRIORITY");
                }
            } else {
                printf("No booking(s) have been made.\n");
            }
        }
        else if (strncmp(command, "printBookings -opti", 19) == 0) {
            if (totalBookings > 0) {
                int pipe_fd[2];
                if (pipe(pipe_fd) == -1) {
                    perror("Pipe creation failed");
                    exit(1);
                }
                pid_t pid = fork();
                if (pid < 0) {
                    perror("Fork failed");
                    exit(1);
                }
                if (pid == 0) {
                    close(pipe_fd[0]);
                    processBookings_Optimized();
                    write(pipe_fd[1], bookings, sizeof(bookings));
                    close(pipe_fd[1]);
                    exit(0);
                } else {
                    close(pipe_fd[1]);
                    wait(NULL);
                    read(pipe_fd[0], bookings, sizeof(bookings));
                    close(pipe_fd[0]);
                    printBookings("OPTIMIZED");
                }
            } else {
                printf("No booking(s) have been made.\n");
            }
        }  
        else if (strncmp(command, "printBookings -ALL", 18) == 0) {
            if (totalBookings > 0) {
                // Process and print for FCFS
                int pipe_fd[2];
                if (pipe(pipe_fd) == -1) {
                    perror("Pipe creation failed");
                    exit(1);
                }
                pid_t pid = fork();
                if (pid < 0) {
                    perror("Fork failed");
                    exit(1);
                }
                if (pid == 0) {
                    close(pipe_fd[0]);
                    processBookings_FCFS();
                    write(pipe_fd[1], bookings, sizeof(bookings));
                    close(pipe_fd[1]);
                    exit(0);
                } else {
                    close(pipe_fd[1]);
                    wait(NULL);
                    read(pipe_fd[0], bookings, sizeof(bookings));
                    close(pipe_fd[0]);
                    printBookings("FCFS");
                }

                // Process and print for PRIORITY
                if (pipe(pipe_fd) == -1) {
                    perror("Pipe creation failed");
                    exit(1);
                }
                pid = fork();
                if (pid < 0) {
                    perror("Fork failed");
                    exit(1);
                }
                if (pid == 0) {
                    close(pipe_fd[0]);
                    processBookings_Priority();
                    write(pipe_fd[1], bookings, sizeof(bookings));
                    close(pipe_fd[1]);
                    exit(0);
                } else {
                    close(pipe_fd[1]);
                    wait(NULL);
                    read(pipe_fd[0], bookings, sizeof(bookings));
                    close(pipe_fd[0]);
                    printBookings("PRIORITY");
                }

                // Process and print for OPTIMIZED
                if (pipe(pipe_fd) == -1) {
                    perror("Pipe creation failed");
                    exit(1);
                }
                pid = fork();
                if (pid < 0) {
                    perror("Fork failed");
                    exit(1);
                }
                if (pid == 0) {
                    close(pipe_fd[0]);
                    processBookings_Optimized();
                    write(pipe_fd[1], bookings, sizeof(bookings));
                    close(pipe_fd[1]);
                    exit(0);
                } else {
                    close(pipe_fd[1]);
                    wait(NULL);
                    read(pipe_fd[0], bookings, sizeof(bookings));
                    close(pipe_fd[0]);
                    printBookings("OPTIMIZED");
                    generateSummaryReport(); // Print summary report after all schedules
                }
            } else {
                printf("No booking(s) have been made.\n");
            }
        } 
        else if (strncmp(command, "printBookings", 13) == 0) {
            if (totalBookings > 0) {
                int pipe_fd[2];
                if (pipe(pipe_fd) == -1) {
                    perror("Pipe creation failed");
                    exit(1);
                }
                pid_t pid = fork();
                if (pid < 0) {
                    perror("Fork failed");
                    exit(1);
                }
                if (pid == 0) {
                    close(pipe_fd[0]);
                    processBookings_FCFS();
                    write(pipe_fd[1], bookings, sizeof(bookings));
                    close(pipe_fd[1]);
                    exit(0);
                } else {
                    close(pipe_fd[1]);
                    wait(NULL);
                    read(pipe_fd[0], bookings, sizeof(bookings));
                    close(pipe_fd[0]);
                    printBookings("FCFS");
                }

                if (pipe(pipe_fd) == -1) {
                    perror("Pipe creation failed");
                    exit(1);
                }
                pid = fork();
                if (pid < 0) {
                    perror("Fork failed");
                    exit(1);
                }
                if (pid == 0) {
                    close(pipe_fd[0]);
                    processBookings_Priority();
                    write(pipe_fd[1], bookings, sizeof(bookings));
                    close(pipe_fd[1]);
                    exit(0);
                } else {
                    close(pipe_fd[1]);
                    wait(NULL);
                    read(pipe_fd[0], bookings, sizeof(bookings));
                    close(pipe_fd[0]);
                    printBookings("PRIORITY");
                }
            } else {
                printf("No booking(s) have been made.\n");
            }
        } 
        else if (strncmp(command, "addBatch", 8) == 0) {
            char batchFile[20];
            sscanf(command, "addBatch -%s", batchFile);
            FILE *file = fopen(batchFile, "r");
            if (file == NULL) {
                printf("Cannot open batch file: %s\n", batchFile);
                printf("-> [Pending]\n");
                continue;
            }
            char line[128];
            while (fgets(line, sizeof(line), file)) {
                line[strcspn(line, "\n")] = 0;
                for (int i = 0; i < MAX_RESOURCES; i++) {
                    strcpy(essentials[i], "");
                }
                if (strncmp(line, "addParking", 10) == 0) {
                    sscanf(line, "addParking -%s %s %s %f %s %s %s %s %s %s %s", 
                           memberName, date, time, &duration, essentials[0], essentials[1], essentials[2], essentials[3], essentials[4], essentials[5], essentials[6]);
                    addBooking(memberName, date, time, duration, essentials, PRIORITY_PARKING);
                } 
                else if (strncmp(line, "addReservation", 14) == 0) {
                    sscanf(line, "addReservation -%s %s %s %f %s %s %s %s %s %s %s", 
                           memberName, date, time, &duration, essentials[0], essentials[1], essentials[2], essentials[3], essentials[4], essentials[5], essentials[6]);
                    addBooking(memberName, date, time, duration, essentials, PRIORITY_RESERVATION);
                } 
                else if (strncmp(line, "addEvent", 8) == 0) {
                    sscanf(line, "addEvent -%s %s %s %f %s %s %s %s %s %s %s", 
                           memberName, date, time, &duration, essentials[0], essentials[1], essentials[2], essentials[3], essentials[4], essentials[5], essentials[6]);
                    addBooking(memberName, date, time, duration, essentials, PRIORITY_EVENT);
                } 
                else if (strncmp(line, "bookEssentials", 14) == 0) {
                    sscanf(line, "bookEssentials -%s %s %s %f %s", 
                           memberName, date, time, &duration, essentials[0]);
                    addBooking(memberName, date, time, duration, essentials, PRIORITY_ESSENTIAL);
                }
            }
            fclose(file);
        }
        else if (strncmp(command, "endProgram", 10) == 0) {
            printf("Bye!\n");
            break;
        } 
        else {
            printf("Command is not recognized. Please try again.\n");
        }
    }
    return 0;
}

void addBooking(char *memberName, char *date, char *time, float duration, char essentials[MAX_RESOURCES][20], int priority) {
    if (totalBookings >= MAX_BOOKINGS) {
        printf("Booking limit reached (Maximum: %i). Cannot create more bookings.\n", MAX_BOOKINGS);
    } 
    else {
        Booking *b = &bookings[totalBookings];
        strcpy(b->memberName, memberName);
        strcpy(b->date, date);
        strcpy(b->time, time);
        b->duration = duration;
        b->priority = priority;
        b->parkingSlot = -1;
        b->accepted = 0;
        
        int i;
        for (i = 0; i < MAX_RESOURCES; i++) {
            strcpy(b->essentials[i], essentials[i]);
        }

        if (contains(b->essentials, "battery") >= 0 && contains(b->essentials, "cable") == -1) {
            for (i = 0; i < MAX_RESOURCES; i++) {
                if (strcmp(essentials[i], "locker") == 0 || strcmp(essentials[i], "battery") == 0 || strcmp(essentials[i], "cable") == 0 || strcmp(essentials[i], "umbrella") == 0 || strcmp(essentials[i], "inflation") == 0 || strcmp(essentials[i], "valetpark") == 0) {
                    continue;
                } else {
                    strcpy(essentials[i], "cable");
                    break;
                }
            }
        }
        if (contains(b->essentials, "cable") >= 0 && contains(b->essentials, "battery") == -1) {
            for (i = 0; i < MAX_RESOURCES; i++) {
                if (strcmp(essentials[i], "locker") == 0 || strcmp(essentials[i], "battery") == 0 || strcmp(essentials[i], "cable") == 0 || strcmp(essentials[i], "umbrella") == 0 || strcmp(essentials[i], "inflation") == 0 || strcmp(essentials[i], "valetpark") == 0) {
                    continue;
                } else {
                    strcpy(essentials[i], "battery");
                    break;
                }
            }  
        }
        if (contains(b->essentials, "locker") >= 0 && contains(b->essentials, "umbrella") == -1) {
            for (i = 0; i < MAX_RESOURCES; i++) {
                if (strcmp(essentials[i], "locker") == 0 || strcmp(essentials[i], "battery") == 0 || strcmp(essentials[i], "cable") == 0 || strcmp(essentials[i], "umbrella") == 0 || strcmp(essentials[i], "inflation") == 0 || strcmp(essentials[i], "valetpark") == 0) {
                    continue;
                } else {
                    strcpy(essentials[i], "umbrella");
                    break;
                }
            }
        }
        if (contains(b->essentials, "umbrella") >= 0 && contains(b->essentials, "locker") == -1) {
            for (i = 0; i < MAX_RESOURCES; i++) {
                if (strcmp(essentials[i], "locker") == 0 || strcmp(essentials[i], "battery") == 0 || strcmp(essentials[i], "cable") == 0 || strcmp(essentials[i], "umbrella") == 0 || strcmp(essentials[i], "inflation") == 0 || strcmp(essentials[i], "valetpark") == 0) {
                    continue;
                } else {
                    strcpy(essentials[i], "locker");
                    break;
                }
            }
        } 
        if (contains(b->essentials, "inflation") >= 0 && contains(b->essentials, "valetpark") == -1) {
            for (i = 0; i < MAX_RESOURCES; i++) {
                if (strcmp(essentials[i], "locker") == 0 || strcmp(essentials[i], "battery") == 0 || strcmp(essentials[i], "cable") == 0 || strcmp(essentials[i], "umbrella") == 0 || strcmp(essentials[i], "inflation") == 0 || strcmp(essentials[i], "valetpark") == 0) {
                    continue;
                } else {
                    strcpy(essentials[i], "valetpark");
                    break;
                }
            }
        }
        if (contains(b->essentials, "valetpark") >= 0 && contains(b->essentials, "inflation") == -1) {
            for (i = 0; i < MAX_RESOURCES; i++) {
                if (strcmp(essentials[i], "locker") == 0 || strcmp(essentials[i], "battery") == 0 || strcmp(essentials[i], "cable") == 0 || strcmp(essentials[i], "umbrella") == 0 || strcmp(essentials[i], "inflation") == 0 || strcmp(essentials[i], "valetpark") == 0) {
                    continue;
                } else {
                    strcpy(essentials[i], "inflation");
                    break;
                }
            }
        }

        totalBookings++;
        printf("Booking added: %s on %s at %s for %.2f hours. ", memberName, date, time, duration);
        printf("(");
        for (i = 0; i < MAX_RESOURCES; i++) {
            if (i > 0 && strlen(essentials[i]) > 0) printf(", ");
            if (strcmp(essentials[i], "locker") == 0 || strcmp(essentials[i], "battery") == 0 || strcmp(essentials[i], "cable") == 0 || strcmp(essentials[i], "umbrella") == 0 || strcmp(essentials[i], "inflation") == 0 || strcmp(essentials[i], "valetpark") == 0) {
                printf("%s", essentials[i]);
            }
        }
        printf(")\n");
    }
}

void processBookings_FCFS() {
    int i, j, k;
    for (i = 0; i < totalBookings; i++) {
        Booking *b = &bookings[i];
        int startSlot = timeToSlot(b->time);
        int durationSlots = (int)b->duration;

        int slotFound = -1;
        for (j = 0; j < PARKING_SLOTS; j++) {
            int available = 1;
            for (k = startSlot; k < startSlot + durationSlots; k++) {
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

        int resourcesAllocated = allocateResources(startSlot, durationSlots, b->essentials);

        if (slotFound != -1 && resourcesAllocated) {
            for (k = startSlot; k < startSlot + durationSlots; k++) {
                parkingAvailability[k][slotFound] = 0;
            }
            b->parkingSlot = slotFound; 
            b->accepted = 1;
        } 
        else {
            releaseResources(startSlot, durationSlots, b->essentials);
            b->accepted = 0;
            if (slotFound == -1) {
                snprintf(b->reasonForRejection, sizeof(b->reasonForRejection), "No available parking slots.");
            } 
            if (!resourcesAllocated) {
                snprintf(b->reasonForRejection, sizeof(b->reasonForRejection), "One or more booked essentials are unavailable.");
            }
            suggestAlternativeSlots(durationSlots, b->memberName, b->date, b->time);
        }
    }
}

void processBookings_Priority() {
    qsort(bookings, totalBookings, sizeof(bookings[0]), compareBookings);
    processBookings_FCFS();
}

void processBookings_Optimized() {
    processBookings_FCFS();

    int i, j, k, newStartSlot;
    for (i = 0; i < totalBookings; i++) {
        Booking *b = &bookings[i];
        if (b->accepted == 0) {
            int startSlot = timeToSlot(b->time);
            int durationSlots = (int)b->duration;

            for (newStartSlot = 0; newStartSlot < TIME_SLOTS - durationSlots; newStartSlot++) {
                int slotFound = -1;
                for (j = 0; j < PARKING_SLOTS; j++) {
                    int available = 1;
                    for (k = newStartSlot; k < newStartSlot + durationSlots; k++) {
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

                int resourcesAllocated = allocateResources(newStartSlot, durationSlots, b->essentials);

                if (slotFound != -1 && resourcesAllocated) {
                    for (k = newStartSlot; k < newStartSlot + durationSlots; k++) {
                        parkingAvailability[k][slotFound] = 0;
                    }
                    b->parkingSlot = slotFound;
                    b->accepted = 1;
                    sprintf(b->time, "%02d:00", newStartSlot); // update the time
                    break;
                } else {
                    releaseResources(newStartSlot, durationSlots, b->essentials);
                }
            }
        }
    }
}

int compareBookings(const void *a, const void *b) {
    Booking *bookingA = (Booking *)a;
    Booking *bookingB = (Booking *)b;
    return bookingA->priority - bookingB->priority;
}

int allocateResources(int startSlot, int durationSlots, char essentials[MAX_RESOURCES][20]) {
    int resourceCount[MAX_RESOURCES] = {0};
    int i, j;
    for (i = 0; i < MAX_RESOURCES; i++) {
        if (strcmp(essentials[i], "battery") == 0) resourceCount[0]++;
        else if (strcmp(essentials[i], "cable") == 0) resourceCount[1]++;
        else if (strcmp(essentials[i], "locker") == 0) resourceCount[2]++;
        else if (strcmp(essentials[i], "umbrella") == 0) resourceCount[3]++;
        else if (strcmp(essentials[i], "inflation") == 0) resourceCount[4]++;
        else if (strcmp(essentials[i], "valetpark") == 0) resourceCount[5]++;
    }

    for (i = 0; i < MAX_RESOURCES; i++) {
        for (j = startSlot; j < startSlot + durationSlots; j++) {
            if (resourceCount[i] > resourceAvailability[j][i]) {
                return 0;
            }
        }
    }

    for (i = 0; i < MAX_RESOURCES; i++) {
        for (j = startSlot; j < startSlot + durationSlots; j++) {
            resourceAvailability[j][i] -= resourceCount[i];
        }
    }

    return 1;
}

void releaseResources(int startSlot, int durationSlots, char essentials[MAX_RESOURCES][20]) {
    int i, j;
    for (i = 0; i < MAX_RESOURCES; i++) {
        int resourceType = -1;
        if (strcmp(essentials[i], "battery") == 0) resourceType = 0;
        else if (strcmp(essentials[i], "cable") == 0) resourceType = 1;
        else if (strcmp(essentials[i], "locker") == 0) resourceType = 2;
        else if (strcmp(essentials[i], "umbrella") == 0) resourceType = 3;
        else if (strcmp(essentials[i], "inflation") == 0) resourceType = 4;
        else if (strcmp(essentials[i], "valetpark") == 0) resourceType = 5;
    
        if (resourceType != -1) {
            for (j = startSlot; j < startSlot + durationSlots; j++) {
                resourceAvailability[j][resourceType]++;
            }
        }
    }
}

char* calculateEndTime(const char* startTime, float duration) {
    int startHour, startMin;
    sscanf(startTime, "%d:%d", &startHour, &startMin);
    int totalMin = startHour * 60 + startMin + (int)(duration * 60);
    int endHour = totalMin / 60 % 24;
    int endMin = totalMin % 60;
    char* endTime = (char*)malloc(6 * sizeof(char));
    sprintf(endTime, "%02d:%02d", endHour, endMin);
    return endTime;
}

const char* getBookingType(int priority) {
    switch (priority) {
        case PRIORITY_EVENT: return "Event";
        case PRIORITY_RESERVATION: return "Reservation";
        case PRIORITY_PARKING: return "Parking";
        case PRIORITY_ESSENTIAL: return "Essentials";
        default: return "Unknown";
    }
}

void printBookings(const char *algorithm) {
    printf("\n*** Booking Schedule (%s) ***\n", algorithm);

    // List of members (fixed as per PDF assumptions)
    char members[5][20] = {"member_A", "member_B", "member_C", "member_D", "member_E"};

    // Print ACCEPTED bookings
    printf("\n*** ACCEPTED Bookings ***\n");
    for (int m = 0; m < 5; m++) {
        char* member = members[m];
        int hasBookings = 0;
        for (int i = 0; i < totalBookings; i++) {
            Booking *b = &bookings[i];
            if (strcmp(b->memberName, member) == 0 && b->accepted) {
                if (!hasBookings) {
                    printf("%s has the following bookings:\n", member);
                    printf("%-12s %-6s %-6s %-12s %-20s\n", "Date", "Start", "End", "Type", "Device");
                    printf("===============================================================\n");
                    hasBookings = 1;
                }
                char* endTime = calculateEndTime(b->time, b->duration);
                char devices[100] = "";
                for (int j = 0; j < MAX_RESOURCES; j++) {
                    if (strlen(b->essentials[j]) > 0) {
                        strcat(devices, b->essentials[j]);
                        strcat(devices, ", ");
                    }
                }
                if (strlen(devices) > 0) devices[strlen(devices) - 2] = '\0'; // Remove trailing ", "
                else strcpy(devices, "*"); // Default for no essentials
                printf("%-12s %-6s %-6s %-12s %-20s\n", b->date, b->time, endTime, getBookingType(b->priority), devices);
                free(endTime);
            }
        }
        if (hasBookings) printf("- End -\n");
    }

    // Print REJECTED bookings
    printf("\n*** Parking Booking - REJECTED / %s ***\n", algorithm);
    for (int m = 0; m < 5; m++) {
        char* member = members[m];
        int rejectedCount = 0;
        for (int i = 0; i < totalBookings; i++) {
            if (strcmp(bookings[i].memberName, member) == 0 && !bookings[i].accepted) {
                rejectedCount++;
            }
        }
        if (rejectedCount > 0) {
            printf("%s (there are %d bookings rejected):\n", member, rejectedCount);
            printf("%-12s %-6s %-6s %-12s %-20s\n", "Date", "Start", "End", "Type", "Essentials");
            printf("===============================================================\n");
            for (int i = 0; i < totalBookings; i++) {
                Booking *b = &bookings[i];
                if (strcmp(b->memberName, member) == 0 && !b->accepted) {
                    char* endTime = calculateEndTime(b->time, b->duration);
                    char essentials[100] = "";
                    for (int j = 0; j < MAX_RESOURCES; j++) {
                        if (strlen(b->essentials[j]) > 0) {
                            strcat(essentials, b->essentials[j]);
                            strcat(essentials, ", ");
                        }
                    }
                    if (strlen(essentials) > 0) essentials[strlen(essentials) - 2] = '\0';
                    else strcpy(essentials, "-"); // Default for no essentials
                    printf("%-12s %-6s %-6s %-12s %-20s\n", b->date, b->time, endTime, getBookingType(b->priority), essentials);
                    free(endTime);
                }
            }
            printf("- End -\n");
        }
    }
    printf("===============================================================\n");
}

void generateSummaryReport() {
    // Temporary storage for bookings to restore state
    Booking originalBookings[MAX_BOOKINGS];
    memcpy(originalBookings, bookings, sizeof(bookings));
    int originalParking[TIME_SLOTS][PARKING_SLOTS];
    int originalResources[TIME_SLOTS][MAX_RESOURCES];
    memcpy(originalParking, parkingAvailability, sizeof(parkingAvailability));
    memcpy(originalResources, resourceAvailability, sizeof(resourceAvailability));

    // Calculate stats for FCFS
    int fcfsAccepted = 0;
    processBookings_FCFS();
    for (int i = 0; i < totalBookings; i++) {
        if (bookings[i].accepted) fcfsAccepted++;
    }

    // Restore state
    memcpy(bookings, originalBookings, sizeof(bookings));
    memcpy(parkingAvailability, originalParking, sizeof(parkingAvailability));
    memcpy(resourceAvailability, originalResources, sizeof(resourceAvailability));

    // Calculate stats for PRIO
    int prioAccepted = 0;
    processBookings_Priority();
    for (int i = 0; i < totalBookings; i++) {
        if (bookings[i].accepted) prioAccepted++;
    }

    // Restore state
    memcpy(bookings, originalBookings, sizeof(bookings));
    memcpy(parkingAvailability, originalParking, sizeof(parkingAvailability));
    memcpy(resourceAvailability, originalResources, sizeof(resourceAvailability));

    // Calculate stats for OPTI
    int optiAccepted = 0;
    processBookings_Optimized();
    for (int i = 0; i < totalBookings; i++) {
        if (bookings[i].accepted) optiAccepted++;
    }

    // Calculate utilization (simplified as percentage of slots used)
    float totalSlots = TIME_SLOTS * PARKING_SLOTS * 7; // 7 days from May 10-16
    float lockerUsage = 0, batteryUsage = 0;
    for (int i = 0; i < totalBookings; i++) {
        if (bookings[i].accepted) {
            int slots = (int)bookings[i].duration;
            if (contains(bookings[i].essentials, "locker") != -1) lockerUsage += slots;
            if (contains(bookings[i].essentials, "battery") != -1) batteryUsage += slots;
        }
    }
    float lockerUtilization = (lockerUsage / (totalSlots * RESOURCE_STOCK)) * 100;
    float batteryUtilization = (batteryUsage / (totalSlots * RESOURCE_STOCK)) * 100;

    // Print summary report matching PDF format
    printf("\n*** Parking Booking Manager - Summary Report ***\n");
    printf("Performance:\n");
    
    printf("For FCFS:\n");
    printf("    Total Number of Bookings Received: %d (100%%)\n", totalBookings);
    printf("    Number of Bookings Assigned: %d (%.1f%%)\n", fcfsAccepted, (float)fcfsAccepted / totalBookings * 100);
    printf("    Number of Bookings Rejected: %d (%.1f%%)\n", totalBookings - fcfsAccepted, (float)(totalBookings - fcfsAccepted) / totalBookings * 100);
    printf("    Utilization of Time slot:\n");
    printf("    locker - %.1f%%\n", lockerUtilization);
    printf("    Battery - %.1f%%\n", batteryUtilization);
    printf("    Invalid request(s) made: 0\n"); // Assuming all inputs are valid per PDF assumption

    printf("For PRIO:\n");
    printf("    Total Number of Bookings Received: %d (100%%)\n", totalBookings);
    printf("    Number of Bookings Assigned: %d (%.1f%%)\n", prioAccepted, (float)prioAccepted / totalBookings * 100);
    printf("    Number of Bookings Rejected: %d (%.1f%%)\n", totalBookings - prioAccepted, (float)(totalBookings - prioAccepted) / totalBookings * 100);
    printf("    Utilization of Time slot:\n");
    printf("    locker - %.1f%%\n", lockerUtilization);
    printf("    Battery - %.1f%%\n", batteryUtilization);
    printf("    Invalid request(s) made: 0\n");

    printf("For OPTI:\n");
    printf("    Total Number of Bookings Received: %d (100%%)\n", totalBookings);
    printf("    Number of Bookings Assigned: %d (%.1f%%)\n", optiAccepted, (float)optiAccepted / totalBookings * 100);
    printf("    Number of Bookings Rejected: %d (%.1f%%)\n", totalBookings - optiAccepted, (float)(totalBookings - optiAccepted) / totalBookings * 100);
    printf("    Utilization of Time slot:\n");
    printf("    locker - %.1f%%\n", lockerUtilization);
    printf("    Battery - %.1f%%\n", batteryUtilization);
    printf("    Invalid request(s) made: 0\n");

    // Restore original state
    memcpy(bookings, originalBookings, sizeof(bookings));
    memcpy(parkingAvailability, originalParking, sizeof(parkingAvailability));
    memcpy(resourceAvailability, originalResources, sizeof(resourceAvailability));
}

int timeToSlot(char *time) {
    int hour;
    sscanf(time, "%d", &hour);
    return hour;
}

int getResourceIndex(const char *resourceName) {
    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (strcmp(resourceNames[i], resourceName) == 0) {
            return i;
        }
    }
    return -1;
}

int contains(char essentials[MAX_RESOURCES][20], const char *item) {
    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (strcmp(essentials[i], item) == 0) {
            return i;
        }
    }
    return -1;
}

void suggestAlternativeSlots(int durationSlots, char *memberName, char *date, char *time) {
    printf("Suggested alternative booking slots for %s on %s at %s:\n", memberName, date, time);
    for (int newStartSlot = 0; newStartSlot <= TIME_SLOTS - durationSlots; newStartSlot++) {
        int available = 1;
        for (int j = 0; j < PARKING_SLOTS; j++) {
            int slotAvailable = 1;
            for (int k = newStartSlot; k < newStartSlot + durationSlots; k++) {
                if (parkingAvailability[k][j] == 0) {
                    slotAvailable = 0;
                    break;
                }
            }
            if (slotAvailable) {
                available = 1;
                break;
            } else {
                available = 0;
            }
        }
        if (available) {
            printf(" -> Time slot: %02d:00\n", newStartSlot);
        }
    }
}
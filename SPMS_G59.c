#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <stdbool.h>
#include <time.h>

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
    char date[11];
    int startMinutes;
    int durationMinutes;
    int resourceCount[MAX_RESOURCES]; // record of how many of each resource is needed
} OptimizedSlot;

OptimizedSlot optimizedSlots[MAX_BOOKINGS];
int optimizedSlotCount = 0;

Booking initialBookings[MAX_BOOKINGS]; // Initial bookings that are read from the report
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

const char *members[5] = {"member_A", "member_B", "member_C", "member_D", "member_E"};

// Prototypes
void addBooking(char *memberName, char *date, char *time, float duration, char essentials[MAX_RESOURCES][20], int priority, int isEssentialBooking);
void processBookings_FCFS();
void processBookings_Priority();
void processBookings_Optimized();
void printBookings(const char *algorithm);
int allocateResources(int startMinutes, int durationMinutes, char essentials[MAX_RESOURCES][20]);
void releaseResources(int startMinutes, int durationMinutes, char essentials[MAX_RESOURCES][20]);
int timeToMinutes(char *time);
int durationToMinutes(float duration);
int compareBookings(const void *a, const void *b);
int getResourceIndex(const char *resourceName);
int contains(char essentials[MAX_RESOURCES][20], const char *item);
void suggestAlternativeSlots(float durationHours, char *memberName, char *date, char *time);
char* calculateEndTime(const char* startTime, float duration);
const char* getBookingType(int priority);
void generateSummaryReport();
int isValidDate(char *date);
int isValidTime(char *time, float duration);
int isValidMember(char *memberName);
int isValidResource(char *resource);

int isValidDate(char *date) {
    int year, month, day;
    if (sscanf(date, "%4d-%2d-%2d", &year, &month, &day) != 3) {
        return 0;
    }
    if (year < 1900 || year > 2100 || month < 1 || month > 12 || day < 1 || day > 31) {
        return 0;
    }
    if ((month == 4 || month == 6 || month == 9 || month == 11) && day > 30) {
        return 0;
    }
    if (month == 2 && day > 28) { 
        return 0;
    }
    return 1;
}

int isValidTime(char *time, float duration) {
    int hour, minute;
    if (sscanf(time, "%2d:%2d", &hour, &minute) != 2) {
        return 0;
    }
    if (hour < 0 || hour > 23 || minute < 0 || minute > 59) {
        return 0;
    }
    return 1;
}

int isValidMember(char *memberName) {
    for (int i = 0; i < 5; i++) {
        if (strcmp(members[i], memberName) == 0) {
            return 1;
        }
    }
    return 0;
}

int isValidResource(char *resource) {
    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (strcmp(resourceNames[i], resource) == 0) {
            return 1;
        }
    }
    return 0;
}

int main() {
    char command[128];
    char memberName[20];
    char date[11]; 
    char time[6];
    char essentials[MAX_RESOURCES][20];
    float duration;

    // Set parking and resource availability to its initial state.
    memset(parkingAvailability, 1, sizeof(parkingAvailability)); // 1 means available
    memset(essentials, 0, sizeof(essentials)); // Clear the essentials array (make it empty)
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
            if (!isValidMember(memberName)) {
                printf("Invalid member name: %s\n", memberName);
                continue;
            }
            if (!isValidDate(date)) {
                printf("Invalid date format: %s (Expected: YYYY-MM-DD)\n", date);
                continue;
            }
            if (!isValidTime(time, duration)) {
                printf("Invalid time format: %s (Expected: HH:MM, 00:00-23:59) or invalid duration\n", time);
                continue;
            }
            for (int i = 0; i < MAX_RESOURCES; i++) {
                if (strlen(essentials[i]) > 0 && !isValidResource(essentials[i])) {
                    printf("Invalid resource: %s\n", essentials[i]);
                    goto skip_booking;
                }
            }
            addBooking(memberName, date, time, duration, essentials, PRIORITY_PARKING, 0);
            printf("-> [Pending]\n");
        }
        else if (strncmp(command, "addReservation", 14) == 0) {
            sscanf(command, "addReservation -%s %s %s %f %s %s %s %s %s %s %s", memberName, date, time, &duration, essentials[0], essentials[1], essentials[2], essentials[3], essentials[4], essentials[5], essentials[6]);
            if (!isValidMember(memberName)) {
                printf("Invalid member name: %s\n", memberName);
                continue;
            }
            if (!isValidDate(date)) {
                printf("Invalid date format: %s (Expected: YYYY-MM-DD)\n", date);
                continue;
            }
            if (!isValidTime(time, duration)) {
                printf("Invalid time format: %s (Expected: HH:MM, 00:00-23:59) or invalid duration\n", time);
                continue;
            }
            for (int i = 0; i < MAX_RESOURCES; i++) {
                if (strlen(essentials[i]) > 0 && !isValidResource(essentials[i])) {
                    printf("Invalid resource: %s\n", essentials[i]);
                    goto skip_booking;
                }
            }
            addBooking(memberName, date, time, duration, essentials, PRIORITY_RESERVATION, 0); 
            printf("-> [Pending]\n");
        } 
        else if (strncmp(command, "addEvent", 8) == 0) {
            sscanf(command, "addEvent -%s %s %s %f %s %s %s %s %s %s %s", memberName, date, time, &duration, essentials[0], essentials[1], essentials[2], essentials[3], essentials[4], essentials[5], essentials[6]);
            if (!isValidMember(memberName)) {
                printf("Invalid member name: %s\n", memberName);
                continue;
            }
            if (!isValidDate(date)) {
                printf("Invalid date format: %s (Expected: YYYY-MM-DD)\n", date);
                continue;
            }
            if (!isValidTime(time, duration)) {
                printf("Invalid time format: %s (Expected: HH:MM, 00:00-23:59) or invalid duration\n", time);
                continue;
            }
            for (int i = 0; i < MAX_RESOURCES; i++) {
                if (strlen(essentials[i]) > 0 && !isValidResource(essentials[i])) {
                    printf("Invalid resource: %s\n", essentials[i]);
                    goto skip_booking;
                }
            }
            addBooking(memberName, date, time, duration, essentials, PRIORITY_EVENT, 0); 
            printf("-> [Pending]\n");
        }
        else if (strncmp(command, "bookEssentials", 14) == 0) {
            for (int i = 0; i < MAX_RESOURCES; i++) {
                strcpy(essentials[i], "");
            }
            sscanf(command, "bookEssentials -%s %s %s %f %s", memberName, date, time, &duration, essentials[0]);
            if (!isValidMember(memberName)) {
                printf("Invalid member name: %s\n", memberName);
                continue;
            }
            if (!isValidDate(date)) {
                printf("Invalid date format: %s (Expected: YYYY-MM-DD)\n", date);
                continue;
            }
            if (!isValidTime(time, duration)) {
                printf("Invalid time format: %s (Expected: HH:MM, 00:00-23:59) or invalid duration\n", time);
                continue;
            }
            if (strlen(essentials[0]) > 0 && !isValidResource(essentials[0])) {
                printf("Invalid resource: %s\n", essentials[0]);
                continue;
            }
            addBooking(memberName, date, time, duration, essentials, PRIORITY_ESSENTIAL, 1);
            printf("-> [Pending]\n");
        }
        else if (strncmp(command, "printBookings -fcfs", 21) == 0) {
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
                    generateSummaryReport();
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
            int lineNum = 0;
            while (fgets(line, sizeof(line), file)) {
                lineNum++;
                line[strcspn(line, "\n")] = 0;
                for (int i = 0; i < MAX_RESOURCES; i++) {
                    strcpy(essentials[i], "");
                }
                if (strncmp(line, "addParking", 10) == 0) {
                    sscanf(line, "addParking -%s %s %s %f %s %s %s %s %s %s %s",
                           memberName, date, time, &duration, essentials[0], essentials[1], essentials[2],
                           essentials[3], essentials[4], essentials[5], essentials[6]);
                    if (!isValidMember(memberName)) {
                        printf("Error in batch file %s at line %d: Invalid member name '%s'\n", batchFile, lineNum, memberName);
                        continue;
                    }
                    if (!isValidDate(date)) {
                        printf("Error in batch file %s at line %d: Invalid date '%s' (Expected: YYYY-MM-DD)\n", batchFile, lineNum, date);
                        continue;
                    }
                    if (!isValidTime(time, duration)) {
                        printf("Error in batch file %s at line %d: Invalid time '%s' (Expected: HH:MM) or invalid duration\n", batchFile, lineNum, time);
                        continue;
                    }
                    for (int i = 0; i < MAX_RESOURCES && strlen(essentials[i]) > 0; i++) {
                        if (!isValidResource(essentials[i])) {
                            printf("Error in batch file %s at line %d: Invalid resource '%s'\n", batchFile, lineNum, essentials[i]);
                            continue;
                        }
                    }
                    addBooking(memberName, date, time, duration, essentials, PRIORITY_PARKING, 0);
                    printf("-> [Pending] %s\n", line);
                } else if (strncmp(line, "addReservation", 14) == 0) {
                    sscanf(line, "addReservation -%s %s %s %f %s %s %s %s %s %s %s",
                           memberName, date, time, &duration, essentials[0], essentials[1], essentials[2],
                           essentials[3], essentials[4], essentials[5], essentials[6]);
                    if (!isValidMember(memberName)) {
                        printf("Error in batch file %s at line %d: Invalid member name '%s'\n", batchFile, lineNum, memberName);
                        continue;
                    }
                    if (!isValidDate(date)) {
                        printf("Error in batch file %s at line %d: Invalid date '%s' (Expected: YYYY-MM-DD)\n", batchFile, lineNum, date);
                        continue;
                    }
                    if (!isValidTime(time, duration)) {
                        printf("Error in batch file %s at line %d: Invalid time '%s' (Expected: HH:MM) or invalid duration\n", batchFile, lineNum, time);
                        continue;
                    }
                    for (int i = 0; i < MAX_RESOURCES && strlen(essentials[i]) > 0; i++) {
                        if (!isValidResource(essentials[i])) {
                            printf("Error in batch file %s at line %d: Invalid resource '%s'\n", batchFile, lineNum, essentials[i]);
                            continue;
                        }
                    }
                    addBooking(memberName, date, time, duration, essentials, PRIORITY_RESERVATION, 0);
                    printf("-> [Pending] %s\n", line);
                } else if (strncmp(line, "addEvent", 8) == 0) {
                    sscanf(line, "addEvent -%s %s %s %f %s %s %s %s %s %s %s",
                           memberName, date, time, &duration, essentials[0], essentials[1], essentials[2],
                           essentials[3], essentials[4], essentials[5], essentials[6]);
                    if (!isValidMember(memberName)) {
                        printf("Error in batch file %s at line %d: Invalid member name '%s'\n", batchFile, lineNum, memberName);
                        continue;
                    }
                    if (!isValidDate(date)) {
                        printf("Error in batch file %s at line %d: Invalid date '%s' (Expected: YYYY-MM-DD)\n", batchFile, lineNum, date);
                        continue;
                    }
                    if (!isValidTime(time, duration)) {
                        printf("Error in batch file %s at line %d: Invalid time '%s' (Expected: HH:MM) or invalid duration\n", batchFile, lineNum, time);
                        continue;
                    }
                    for (int i = 0; i < MAX_RESOURCES && strlen(essentials[i]) > 0; i++) {
                        if (!isValidResource(essentials[i])) {
                            printf("Error in batch file %s at line %d: Invalid resource '%s'\n", batchFile, lineNum, essentials[i]);
                            continue;
                        }
                    }
                    addBooking(memberName, date, time, duration, essentials, PRIORITY_EVENT, 0);
                    printf("-> [Pending] %s\n", line);
                } else if (strncmp(line, "bookEssentials", 14) == 0) {
                    sscanf(line, "bookEssentials -%s %s %s %f %s",
                           memberName, date, time, &duration, essentials[0]);
                    if (!isValidMember(memberName)) {
                        printf("Error in batch file %s at line %d: Invalid member name '%s'\n", batchFile, lineNum, memberName);
                        continue;
                    }
                    if (!isValidDate(date)) {
                        printf("Error in batch file %s at line %d: Invalid date '%s' (Expected: YYYY-MM-DD)\n", batchFile, lineNum, date);
                        continue;
                    }
                    if (!isValidTime(time, duration)) {
                        printf("Error in batch file %s at line %d: Invalid time '%s' (Expected: HH:MM) or invalid duration\n", batchFile, lineNum, time);
                        continue;
                    }
                    if (strlen(essentials[0]) > 0 && !isValidResource(essentials[0])) {
                        printf("Error in batch file %s at line %d: Invalid resource '%s'\n", batchFile, lineNum, essentials[0]);
                        continue;
                    }
                    addBooking(memberName, date, time, duration, essentials, PRIORITY_ESSENTIAL, 0);
                    printf("-> [Pending] %s\n", line);
                } else {
                    printf("Error in batch file %s at line %d: Unrecognized command '%s'\n", batchFile, lineNum, line);
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
    skip_booking:
        continue;
    }
    return 0;
}

void addBooking(char *memberName, char *date, char *time, float duration, char essentials[MAX_RESOURCES][20], int priority, int isEssentialBooking) {
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
        
        if (!isEssentialBooking) {
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
        }
        totalBookings++;
        initialBookings[totalBookings - 1] = *b;
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
    for (int i = 0; i < totalBookings; i++) {
        Booking *b = &bookings[i];
        int startMinutes = timeToMinutes(b->time);
        int durationMinutes = durationToMinutes(b->duration);
        int startSlot = startMinutes / 60;
        int endSlot = (startMinutes + durationMinutes) / 60 + ((startMinutes + durationMinutes) % 60 > 0 ? 1 : 0);

        int slotFound = -1;
        if (b->priority != PRIORITY_ESSENTIAL) {
            for (int j = 0; j < PARKING_SLOTS; j++) {
                int available = 1;
                for (int k = startSlot; k < endSlot; k++) {
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
        }

        int resourcesAllocated = allocateResources(startMinutes, durationMinutes, b->essentials);

        if (b->priority == PRIORITY_ESSENTIAL) {
            if (resourcesAllocated) {
                b->accepted = 1;
            } else {
                b->accepted = 0;
                snprintf(b->reasonForRejection, sizeof(b->reasonForRejection), "One or more essentials unavailable.");
                suggestAlternativeSlots(b->duration, b->memberName, b->date, b->time);
            }
        } else {
            if (slotFound != -1 && resourcesAllocated) {
                for (int k = startSlot; k < endSlot; k++) {
                    parkingAvailability[k][slotFound] = 0;
                }
                b->parkingSlot = slotFound;
                b->accepted = 1;
            } else if (slotFound == -1) {
                // Try to displace lower-priority bookings
                for (int j = 0; j < totalBookings; j++) {
                    Booking *other = &bookings[j];
                    if (other != b && other->accepted && other->priority > b->priority &&
                        strcmp(other->date, b->date) == 0) {
                        int otherStart = timeToMinutes(other->time);
                        int otherDuration = durationToMinutes(other->duration);
                        int otherEnd = otherStart + otherDuration;
                        if (startMinutes < otherEnd && otherStart < (startMinutes + durationMinutes)) {
                            releaseResources(otherStart, otherDuration, other->essentials);
                            int otherEndSlot = (otherStart + otherDuration) / 60 + ((otherStart + otherDuration) % 60 > 0 ? 1 : 0);
                            for (int k = otherStart / 60; k < otherEndSlot; k++) {
                                parkingAvailability[k][other->parkingSlot] = 1;
                            }
                            other->accepted = 0;
                            snprintf(other->reasonForRejection, sizeof(other->reasonForRejection),
                                     "Displaced by higher priority booking.");
                            slotFound = other->parkingSlot;
                            break;
                        }
                    }
                }
                if (slotFound != -1 && (resourcesAllocated || (resourcesAllocated = allocateResources(startMinutes, durationMinutes, b->essentials)))) {
                    for (int k = startSlot; k < endSlot; k++) {
                        parkingAvailability[k][slotFound] = 0;
                    }
                    b->parkingSlot = slotFound;
                    b->accepted = 1;
                } else {
                    releaseResources(startMinutes, durationMinutes, b->essentials);
                    b->accepted = 0;
                    snprintf(b->reasonForRejection, sizeof(b->reasonForRejection),
                             resourcesAllocated ? "No available parking slots." : "One or more essentials unavailable.");
                    suggestAlternativeSlots(b->duration, b->memberName, b->date, b->time);
                }
            } else {
                releaseResources(startMinutes, durationMinutes, b->essentials);
                b->accepted = 0;
                snprintf(b->reasonForRejection, sizeof(b->reasonForRejection), "One or more essentials unavailable.");
                suggestAlternativeSlots(b->duration, b->memberName, b->date, b->time);
            }
        }
    }
}

void processBookings_Priority() {
    qsort(bookings, totalBookings, sizeof(Booking), compareBookings);
    for (int i = 0; i < totalBookings; i++) {
        Booking *b = &bookings[i];
        int startMinutes = timeToMinutes(b->time);
        int durationMinutes = durationToMinutes(b->duration);
        int startSlot = startMinutes / 60;
        int endSlot = (startMinutes + durationMinutes) / 60 + ((startMinutes + durationMinutes) % 60 > 0 ? 1 : 0);

        int slotFound = -1;
        if (b->priority != PRIORITY_ESSENTIAL) {
            for (int j = 0; j < PARKING_SLOTS; j++) {
                int available = 1;
                for (int k = startSlot; k < endSlot; k++) {
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
        }

        int resourcesAllocated = allocateResources(startMinutes, durationMinutes, b->essentials);

        if (b->priority == PRIORITY_ESSENTIAL) {
            if (resourcesAllocated) {
                b->accepted = 1;
            } else {
                b->accepted = 0;
                snprintf(b->reasonForRejection, sizeof(b->reasonForRejection), "One or more essentials unavailable.");
                suggestAlternativeSlots(b->duration, b->memberName, b->date, b->time);
            }
        } else {
            if (slotFound != -1 && resourcesAllocated) {
                for (int k = startSlot; k < endSlot; k++) {
                    parkingAvailability[k][slotFound] = 0;
                }
                b->parkingSlot = slotFound;
                b->accepted = 1;
            } else if (slotFound == -1) {
                for (int j = 0; j < totalBookings; j++) {
                    Booking *other = &bookings[j];
                    if (other != b && other->accepted && other->priority > b->priority &&
                        strcmp(other->date, b->date) == 0) {
                        int otherStart = timeToMinutes(other->time);
                        int otherDuration = durationToMinutes(other->duration);
                        int otherEnd = otherStart + otherDuration;
                        if (startMinutes < otherEnd && otherStart < (startMinutes + durationMinutes)) {
                            releaseResources(otherStart, otherDuration, other->essentials);
                            int otherEndSlot = (otherStart + otherDuration) / 60 + ((otherStart + otherDuration) % 60 > 0 ? 1 : 0);
                            for (int k = otherStart / 60; k < otherEndSlot; k++) {
                                parkingAvailability[k][other->parkingSlot] = 1;
                            }
                            other->accepted = 0;
                            snprintf(other->reasonForRejection, sizeof(other->reasonForRejection),
                                     "Displaced by higher priority booking.");
                            slotFound = other->parkingSlot;
                            break;
                        }
                    }
                }
                if (slotFound != -1 && (resourcesAllocated || (resourcesAllocated = allocateResources(startMinutes, durationMinutes, b->essentials)))) {
                    for (int k = startSlot; k < endSlot; k++) {
                        parkingAvailability[k][slotFound] = 0;
                    }
                    b->parkingSlot = slotFound;
                    b->accepted = 1;
                } else {
                    releaseResources(startMinutes, durationMinutes, b->essentials);
                    b->accepted = 0;
                    snprintf(b->reasonForRejection, sizeof(b->reasonForRejection),
                             resourcesAllocated ? "No available parking slots." : "One or more essentials unavailable.");
                    suggestAlternativeSlots(b->duration, b->memberName, b->date, b->time);
                }
            } else {
                releaseResources(startMinutes, durationMinutes, b->essentials);
                b->accepted = 0;
                snprintf(b->reasonForRejection, sizeof(b->reasonForRejection), "One or more essentials unavailable.");
                suggestAlternativeSlots(b->duration, b->memberName, b->date, b->time);
            }
        }
    }
}

void processBookings_Optimized() {
    // Step 1: Run FCFS to get initial allocation
    processBookings_FCFS();

    // Save initial state
    int tempParking[TIME_SLOTS][PARKING_SLOTS];
    int tempResources[TIME_SLOTS][MAX_RESOURCES];
    memcpy(tempParking, parkingAvailability, sizeof(parkingAvailability));
    memcpy(tempResources, resourceAvailability, sizeof(resourceAvailability));

    // Step 2: Process rejected bookings with optimization
    for (int m = 0; m < 5; m++) {
        const char *member = members[m];
        int rejectedBookings[MAX_BOOKINGS];
        int rejectedCount = 0;

        for (int i = 0; i < totalBookings; i++) {
            Booking *b = &bookings[i];
            if (strcmp(b->memberName, member) == 0 && !b->accepted) {
                rejectedBookings[rejectedCount++] = i;
            }
        }

        if (rejectedCount > 0) {
            int durationMinutes = durationToMinutes(bookings[rejectedBookings[0]].duration);
            int processed = 0;

            for (int startMinutes = 0; startMinutes <= 1440 - durationMinutes && processed < rejectedCount; startMinutes += 60) {
                memcpy(parkingAvailability, tempParking, sizeof(parkingAvailability));
                memcpy(resourceAvailability, tempResources, sizeof(resourceAvailability));

                for (int j = 0; j < totalBookings; j++) {
                    if (bookings[j].accepted) {
                        int start = timeToMinutes(bookings[j].time);
                        int dur = durationToMinutes(bookings[j].duration);
                        allocateResources(start, dur, bookings[j].essentials);
                        if (bookings[j].priority != PRIORITY_ESSENTIAL) {
                            int sSlot = start / 60;
                            int eSlot = (start + dur) / 60 + ((start + dur) % 60 > 0 ? 1 : 0);
                            for (int k = sSlot; k < eSlot; k++) {
                                parkingAvailability[k][bookings[j].parkingSlot] = 0;
                            }
                        }
                    }
                }

                int bookingsToFit = 0;
                int resourceCount[MAX_RESOURCES] = {0};
                for (int r = 0; r < rejectedCount && processed + bookingsToFit < rejectedCount; r++) {
                    Booking *b = &bookings[rejectedBookings[r + processed]];
                    for (int k = 0; k < MAX_RESOURCES; k++) {
                        if (strlen(b->essentials[k]) > 0) {
                            int idx = getResourceIndex(b->essentials[k]);
                            if (idx != -1) resourceCount[idx]++;
                        }
                    }
                    int startSlot = startMinutes / 60;
                    int endSlot = (startMinutes + durationMinutes) / 60 + ((startMinutes + durationMinutes) % 60 > 0 ? 1 : 0);
                    int canFit = 1;
                    for (int i = 0; i < MAX_RESOURCES; i++) {
                        if (resourceCount[i] > 0) {
                            for (int slot = startSlot; slot < endSlot; slot++) {
                                if (resourceAvailability[slot][i] < resourceCount[i]) {
                                    canFit = 0;
                                    break;
                                }
                            }
                            if (!canFit) break;
                        }
                    }
                    if (canFit) bookingsToFit++;
                    else break;
                }

                if (bookingsToFit > 0) {
                    int resourcesAllocated = allocateResources(startMinutes, durationMinutes, bookings[rejectedBookings[processed]].essentials);
                    if (resourcesAllocated) {
                        // 記錄成功的時段
                        OptimizedSlot *slot = &optimizedSlots[optimizedSlotCount++];
                        strcpy(slot->date, bookings[rejectedBookings[processed]].date);
                        slot->startMinutes = startMinutes;
                        slot->durationMinutes = durationMinutes;
                        memcpy(slot->resourceCount, resourceAvailability[startMinutes / 60], sizeof(resourceCount));

                        for (int r = 0; r < bookingsToFit; r++) {
                            Booking *b = &bookings[rejectedBookings[processed + r]];
                            b->accepted = 1;
                            sprintf(b->time, "%02d:%02d", startMinutes / 60, startMinutes % 60);
                            snprintf(b->reasonForRejection, sizeof(b->reasonForRejection), "Rescheduled to optimized slot");
                        }
                        processed += bookingsToFit;
                        memcpy(tempParking, parkingAvailability, sizeof(parkingAvailability));
                        memcpy(tempResources, resourceAvailability, sizeof(resourceAvailability));
                    }
                }
            }

            for (int r = processed; r < rejectedCount; r++) {
                Booking *b = &bookings[rejectedBookings[r]];
                b->accepted = 0;
                snprintf(b->reasonForRejection, sizeof(b->reasonForRejection), "No suitable slot found with available resources");
                suggestAlternativeSlots(b->duration, b->memberName, b->date, b->time);
            }
        }
    }
}

int compareBookings(const void *a, const void *b) {
    Booking *bookingA = (Booking *)a;
    Booking *bookingB = (Booking *)b;
    return bookingA->priority - bookingB->priority;
}

int allocateResources(int startMinutes, int durationMinutes, char essentials[MAX_RESOURCES][20]) {
    int resourceCount[MAX_RESOURCES] = {0};
    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (strcmp(essentials[i], "battery") == 0) {
            resourceCount[0]++;  // battery
            resourceCount[1]++;  // cable (dependency)
        } else if (strcmp(essentials[i], "cable") == 0) {
            resourceCount[1]++;  // cable
            resourceCount[0]++;  // battery (dependency)
        } else if (strcmp(essentials[i], "locker") == 0) {
            resourceCount[2]++;  // locker
            resourceCount[3]++;  // umbrella (dependency)
        } else if (strcmp(essentials[i], "umbrella") == 0) {
            resourceCount[3]++;  // umbrella
            resourceCount[2]++;  // locker (dependency)
        } else if (strcmp(essentials[i], "inflation") == 0) {
            resourceCount[4]++;  // inflation
            resourceCount[5]++;  // valetpark (dependency)
        } else if (strcmp(essentials[i], "valetpark") == 0) {
            resourceCount[5]++;  // valetpark
            resourceCount[4]++;  // inflation (dependency)
        }
    }

    int startSlot = startMinutes / 60;
    int endSlot = (startMinutes + durationMinutes) / 60 + ((startMinutes + durationMinutes) % 60 > 0 ? 1 : 0);

    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (resourceCount[i] > 0) {
            for (int slot = startSlot; slot < endSlot; slot++) {
                if (resourceCount[i] > resourceAvailability[slot][i]) {
                    return 0;
                }
            }
        }
    }

    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (resourceCount[i] > 0) {
            for (int slot = startSlot; slot < endSlot; slot++) {
                resourceAvailability[slot][i] -= resourceCount[i];
            }
        }
    }
    return 1;
}

void releaseResources(int startMinutes, int durationMinutes, char essentials[MAX_RESOURCES][20]) {
    int resourceCount[MAX_RESOURCES] = {0};
    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (strcmp(essentials[i], "battery") == 0) {
            resourceCount[0]++;  // battery
            resourceCount[1]++;  // cable (dependency)
        } else if (strcmp(essentials[i], "cable") == 0) {
            resourceCount[1]++;  // cable
            resourceCount[0]++;  // battery (dependency)
        } else if (strcmp(essentials[i], "locker") == 0) {
            resourceCount[2]++;  // locker
            resourceCount[3]++;  // umbrella (dependency)
        } else if (strcmp(essentials[i], "umbrella") == 0) {
            resourceCount[3]++;  // umbrella
            resourceCount[2]++;  // locker (dependency)
        } else if (strcmp(essentials[i], "inflation") == 0) {
            resourceCount[4]++;  // inflation
            resourceCount[5]++;  // valetpark (dependency)
        } else if (strcmp(essentials[i], "valetpark") == 0) {
            resourceCount[5]++;  // valetpark
            resourceCount[4]++;  // inflation (dependency)
        }
    }

    int startSlot = startMinutes / 60;
    int endSlot = (startMinutes + durationMinutes) / 60 + ((startMinutes + durationMinutes) % 60 > 0 ? 1 : 0);

    for (int i = 0; i < MAX_RESOURCES; i++) {
        if (resourceCount[i] > 0) {
            for (int slot = startSlot; slot < endSlot; slot++) {
                resourceAvailability[slot][i] += resourceCount[i];
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

    char members[5][20] = {"member_A", "member_B", "member_C", "member_D", "member_E"};

    printf("\n*** ACCEPTED Bookings ***\n");
    for (int m = 0; m < 5; m++) {
        char *member = members[m];
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
                char *endTime = calculateEndTime(b->time, b->duration);
                char devices[100] = "";
                for (int j = 0; j < MAX_RESOURCES; j++) {
                    if (strlen(b->essentials[j]) > 0) {
                        strcat(devices, b->essentials[j]);
                        strcat(devices, ", ");
                    }
                }
                if (strlen(devices) > 0) devices[strlen(devices) - 2] = '\0';
                else strcpy(devices, "*");
                printf("%-12s %-6s %-6s %-12s %-20s\n", b->date, b->time, endTime, getBookingType(b->priority), devices);
                free(endTime);
            }
        }
        if (hasBookings) printf("\n");
    }
    printf("- End -\n");

    printf("\n*** Parking Booking - REJECTED / %s ***\n", algorithm);
    for (int m = 0; m < 5; m++) {
        char *member = members[m];
        int rejectedCount = 0;
        for (int i = 0; i < totalBookings; i++) {
            if (strcmp(bookings[i].memberName, member) == 0 && !bookings[i].accepted) {
                rejectedCount++;
            }
        }
        if (rejectedCount > 0) {
            printf("%s (there are %d bookings rejected):\n", member, rejectedCount);
            printf("%-12s %-6s %-6s %-12s %-20s %-30s\n", "Date", "Start", "End", "Type", "Essentials", "Reason");
            printf("================================================================================\n");
            for (int i = 0; i < totalBookings; i++) {
                Booking *b = &bookings[i];
                if (strcmp(b->memberName, member) == 0 && !b->accepted) {
                    char *endTime = calculateEndTime(b->time, b->duration);
                    char essentials[100] = "";
                    for (int j = 0; j < MAX_RESOURCES; j++) {
                        if (strlen(b->essentials[j]) > 0) {
                            strcat(essentials, b->essentials[j]);
                            strcat(essentials, ", ");
                        }
                    }
                    if (strlen(essentials) > 0) essentials[strlen(essentials) - 2] = '\0';
                    else strcpy(essentials, "-");
                    printf("%-12s %-6s %-6s %-12s %-20s %-30s\n", b->date, b->time, endTime,
                           getBookingType(b->priority), essentials, b->reasonForRejection);
                    free(endTime);
                }
            }
            printf("\n");
        }
    }
    printf("- End -\n");
}

void generateSummaryReport() {
    Booking originalBookings[MAX_BOOKINGS];
    int originalParking[TIME_SLOTS][PARKING_SLOTS];
    int originalResources[TIME_SLOTS][MAX_RESOURCES];
    
    // Save the original state of bookings
    memcpy(originalBookings, initialBookings, sizeof(bookings));
    memcpy(originalParking, parkingAvailability, sizeof(parkingAvailability));
    memcpy(originalResources, resourceAvailability, sizeof(resourceAvailability));

    int fcfsAccepted = 0, prioAccepted = 0, optiAccepted = 0;
    float fcfsResourceUsage[MAX_RESOURCES] = {0};
    float prioResourceUsage[MAX_RESOURCES] = {0};
    float optiResourceUsage[MAX_RESOURCES] = {0};

    // Reset availability to initial state (all parking slots available, resources at full stock)
    int initialParking[TIME_SLOTS][PARKING_SLOTS];
    int initialResources[TIME_SLOTS][MAX_RESOURCES];
    memset(initialParking, 1, sizeof(initialParking)); // 1 means available
    for (int i = 0; i < TIME_SLOTS; i++) {
        for (int j = 0; j < MAX_RESOURCES; j++) {
            initialResources[i][j] = RESOURCE_STOCK;
        }
    }

    // FCFS Evaluation
    memcpy(bookings, originalBookings, sizeof(bookings));
    memcpy(parkingAvailability, initialParking, sizeof(initialParking));
    memcpy(resourceAvailability, initialResources, sizeof(initialResources));
    processBookings_FCFS();
    for (int i = 0; i < totalBookings; i++) {
        if (bookings[i].accepted) {
            fcfsAccepted++;
            int slots = (int)bookings[i].duration;
            for (int j = 0; j < MAX_RESOURCES; j++) {
                if (contains(bookings[i].essentials, resourceNames[j]) != -1) {
                    fcfsResourceUsage[j] += slots;
                }
            }
        }
    }

    // Priority Evaluation
    memcpy(bookings, originalBookings, sizeof(bookings));
    memcpy(parkingAvailability, initialParking, sizeof(initialParking));
    memcpy(resourceAvailability, initialResources, sizeof(initialResources));
    processBookings_Priority();
    for (int i = 0; i < totalBookings; i++) {
        if (bookings[i].accepted) {
            prioAccepted++;
            int slots = (int)bookings[i].duration;
            for (int j = 0; j < MAX_RESOURCES; j++) {
                if (contains(bookings[i].essentials, resourceNames[j]) != -1) {
                    prioResourceUsage[j] += slots;
                }
            }
        }
    }

    // Optimized Evaluation
    memcpy(bookings, originalBookings, sizeof(bookings));
    memcpy(parkingAvailability, initialParking, sizeof(initialParking));
    memcpy(resourceAvailability, initialResources, sizeof(initialResources));
    processBookings_Optimized();
    for (int i = 0; i < totalBookings; i++) {
        if (bookings[i].accepted) {
            optiAccepted++;
            int slots = (int)bookings[i].duration;
            for (int j = 0; j < MAX_RESOURCES; j++) {
                if (contains(bookings[i].essentials, resourceNames[j]) != -1) {
                    optiResourceUsage[j] += slots;
                }
            }
        }
    }

    // Calculate utilization for each algorithm
    float totalSlots = TIME_SLOTS * PARKING_SLOTS * 7; // Assuming 7 days for simplicity
    float fcfsUtilization[MAX_RESOURCES];
    float prioUtilization[MAX_RESOURCES];
    float optiUtilization[MAX_RESOURCES];
    for (int i = 0; i < MAX_RESOURCES; i++) {
        fcfsUtilization[i] = (fcfsResourceUsage[i] / (totalSlots * RESOURCE_STOCK)) * 100;
        prioUtilization[i] = (prioResourceUsage[i] / (totalSlots * RESOURCE_STOCK)) * 100;
        optiUtilization[i] = (optiResourceUsage[i] / (totalSlots * RESOURCE_STOCK)) * 100;
    }

    // Print the report
    printf("\n*** Parking Booking Manager - Summary Report ***\n");
    printf("Performance:\n");
    printf("For FCFS:\n");
    printf("    Total Number of Bookings Received: %d (100%%)\n", totalBookings);
    printf("    Number of Bookings Assigned: %d (%.1f%%)\n", fcfsAccepted, (float)fcfsAccepted / totalBookings * 100);
    printf("    Number of Bookings Rejected: %d (%.1f%%)\n", totalBookings - fcfsAccepted, (float)(totalBookings - fcfsAccepted) / totalBookings * 100);
    printf("    Utilization of Time slot:\n");
    for (int i = 0; i < MAX_RESOURCES; i++) {
        printf("    %s - %.1f%%\n", resourceNames[i], fcfsUtilization[i]);
    }
    printf("    Invalid request(s) made: 0\n");

    printf("For PRIO:\n");
    printf("    Total Number of Bookings Received: %d (100%%)\n", totalBookings);
    printf("    Number of Bookings Assigned: %d (%.1f%%)\n", prioAccepted, (float)prioAccepted / totalBookings * 100);
    printf("    Number of Bookings Rejected: %d (%.1f%%)\n", totalBookings - prioAccepted, (float)(totalBookings - prioAccepted) / totalBookings * 100);
    printf("    Utilization of Time slot:\n");
    for (int i = 0; i < MAX_RESOURCES; i++) {
        printf("    %s - %.1f%%\n", resourceNames[i], prioUtilization[i]);
    }
    printf("    Invalid request(s) made: 0\n");

    printf("For OPTI:\n");
    printf("    Total Number of Bookings Received: %d (100%%)\n", totalBookings);
    printf("    Number of Bookings Assigned: %d (%.1f%%)\n", optiAccepted, (float)optiAccepted / totalBookings * 100);
    printf("    Number of Bookings Rejected: %d (%.1f%%)\n", totalBookings - optiAccepted, (float)(totalBookings - optiAccepted) / totalBookings * 100);
    printf("    Utilization of Time slot:\n");
    for (int i = 0; i < MAX_RESOURCES; i++) {
        printf("    %s - %.1f%%\n", resourceNames[i], optiUtilization[i]);
    }
    printf("    Invalid request(s) made: 0\n");

    // Restore original state
    memcpy(bookings, originalBookings, sizeof(bookings));
    memcpy(parkingAvailability, originalParking, sizeof(parkingAvailability));
    memcpy(resourceAvailability, originalResources, sizeof(resourceAvailability));
}

int timeToMinutes(char *time) {
    int hour, minute;
    sscanf(time, "%d:%d", &hour, &minute);
    return hour * 60 + minute;
}

int durationToMinutes(float duration) {
    return (int)(duration * 60); 
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

void suggestAlternativeSlots(float durationHours, char *memberName, char *date, char *time) {
    int durationMinutes = durationToMinutes(durationHours);
    int originalStart = timeToMinutes(time);
    int suggestions = 0;

    printf("Suggested alternative booking slots for %s on %s at %s:\n", memberName, date, time);

    // first suggest optimized successive slots
    for (int i = 0; i < optimizedSlotCount && suggestions < 3; i++) {
        OptimizedSlot *slot = &optimizedSlots[i];
        if (strcmp(slot->date, date) == 0 && slot->durationMinutes >= durationMinutes) {
            int startSlot = slot->startMinutes / 60;
            int endSlot = (slot->startMinutes + durationMinutes) / 60 + ((slot->startMinutes + durationMinutes) % 60 > 0 ? 1 : 0);
            int batteryIdx = getResourceIndex("battery");
            if (slot->resourceCount[batteryIdx] >= 1) { 
                printf(" -> Time slot: %02d:%02d (Optimized)\n", slot->startMinutes / 60, slot->startMinutes % 60);
                suggestions++;
            }
        }
    }

    // if suggestions < 3, suggest alternative time slots
    if (suggestions < 3) {
        for (int newStartMinutes = 0; newStartMinutes <= 1440 - durationMinutes && suggestions < 3; newStartMinutes += 60) {
            int startSlot = newStartMinutes / 60;
            int endSlot = (newStartMinutes + durationMinutes) / 60 + ((newStartMinutes + durationMinutes) % 60 > 0 ? 1 : 0);
            int availableParking = 0;
            for (int j = 0; j < PARKING_SLOTS; j++) {
                int slotAvailable = 1;
                for (int k = startSlot; k < endSlot; k++) {
                    if (parkingAvailability[k][j] == 0) {
                        slotAvailable = 0;
                        break;
                    }
                }
                if (slotAvailable) {
                    availableParking = 1;
                    break;
                }
            }
            int batteryIdx = getResourceIndex("battery");
            int resourcesAvailable = 1;
            for (int slot = startSlot; slot < endSlot; slot++) {
                if (resourceAvailability[slot][batteryIdx] < 1) {
                    resourcesAvailable = 0;
                    break;
                }
            }
            if (availableParking && resourcesAvailable) {
                int alreadySuggested = 0;
                for (int j = 0; j < optimizedSlotCount; j++) {
                    if (optimizedSlots[j].startMinutes == newStartMinutes && strcmp(optimizedSlots[j].date, date) == 0) {
                        alreadySuggested = 1;
                        break;
                    }
                }
                if (!alreadySuggested) {
                    printf(" -> Time slot: %02d:%02d\n", newStartMinutes / 60, newStartMinutes % 60);
                    suggestions++;
                }
            }
        }
    }

    if (suggestions == 0) {
        printf(" -> No suitable slots available.\n");
    }
}
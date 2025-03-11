#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#include "inputModule/commands.c"
#define MAX_BOOKINGS 999
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

Booking bookings[MAX_BOOKINGS];
int totalBookings = 0;
int parkingAvailability[PARKING_SLOTS];
int resourceAvailability[MAX_RESOURCES] = {3, 3, 3}; 

pthread_mutex_t lock;



int main() {
    printf("~~ WELCOME TO POLYU! ~~\n");
    printf("Please enter booking: \n");
    char input[100];
    char command[256];
    char memberName[20], date[11], time[6], essentials[3][20];
    float duration;
    int priority;


    for (int i = 0; i < PARKING_SLOTS; i++) {
        parkingAvailability[i] = 1; // 1 means available
    }

    // Mutex initialization
    pthread_mutex_init(&lock, NULL);
    fgets(input, sizeof(input), stdin);


    char command_copy[strlen(input)];
    strcpy(command_copy, input);
    char *delimiter = " ";
    char *token = strtok(command_copy, delimiter);
    // counts the index of the token. starts from 0
    int counter = 0;

    while (token != NULL) {
        // printf("%s\n", token); // prints current token.
        
        if (strcmp(token, "addParking")) {
            sscanf(input, "addParking -%s %s %s %f %s %s", memberName, date, time, &duration, essentials[0], essentials[1]);
            addBooking(memberName, date, time, duration, essentials, 3);
        }
        else if (strcmp(token, "addReservation")) {
            
        }
        else if (strcmp(token, "addEvent")) {

        }
       
        else if (strcmp(token, "bookEssentials")) {

        }
        else if (strcmp(token, "addBatch")) {
             
        }
        
        else if (strcmp(token, "printBookings")) {

        }
        else if (strcmp(token, "endProgram")) {

        }
        counter++;
        token = strtok(NULL, delimiter); // Get the next token
    }
    
    return 0;
}


void addBooking(char *memberName, char *date, char *time, float duration, char essentials[3][20], int priority) {
    printf("Booking details: \n");
    printf("Member name: %s\n", memberName);
    printf("Booking date: %s\n", date);
    printf("Booking time: %s\n", time);
    printf("Booking duration: %f\n", duration);
    printf("Essentials booked: %s\n", essentials);
    printf("Priority: %s\n", priority);
}



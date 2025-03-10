#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "inputModule/commands.c"

void parseCommand(char command[]) {
    char command_copy[strlen(command)];
    strcpy(command_copy, command);
    char *delimiter = " ";

    char *token = strtok(command_copy, delimiter);
    
    // counts the index of the token. starts from 0
    int counter = 0;

    while (token != NULL) {
        // printf("%s\n", token); // prints current token.
        
        if (strcmp(token, "addParking")) {
            add_parking();
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
}
    

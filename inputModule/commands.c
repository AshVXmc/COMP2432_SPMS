#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

// Syntax: addParking -aaa YYYY-MM-DD hh:mm n.n p bbb ccc;
// Example usage with both essential arguments
// add_parking("John Doe", "2025-03-10", "10:00", "2 hours", 2, "Wheelchair", "Signage");

// Example usage with no essential arguments
// add_parking("Jane Smith", "2025-03-11", "11:00", "1 hour", 0);
void add_parking(char member_name[], char date[], char time[], char duration[], char essential1[], int num_essentials, ...) {
    double duration = atof(duration);
    va_list args;
    va_start(args, num_essentials);

    // Print member details
    printf("Member Name: %s\n", member_name);
    printf("Date: %s\n", date);
    printf("Time: %s\n", time);
    printf("Duration: %s\n", duration);

    // Handle optional essential arguments
    for (int i = 0; i < num_essentials; i++) {
        char *essential = va_arg(args, char*);
        printf("Essential %d: %s\n", i + 1, essential);
    }

    va_end(args);

    printf("Add parking command");
}


#include <stdio.h>
#include "commandParser.c"


int main() {
    printf("~~ WELCOME TO POLYU! ~~\n");
    printf("Please enter booking: \n");
    char input[100];

    fgets(input, sizeof(input), stdin);
    parseCommand(input);
    
    return 0;
}




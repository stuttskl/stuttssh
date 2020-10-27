#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include "smallsh.h"


void commandPrompt(char* arr[], int* background, char inFile[], char outFile[], int pid) {
    char fullCommand[2048];


    // print colon to the user
    printf("%s",":");
    fflush(stdout);
    // get full command from input from user
    fgets(fullCommand, 2048, stdin); 
    // strip newline
    fullCommand[strcspn(fullCommand, "\n")] = 0;
    printf("Your command is %s ", fullCommand);

    // add handling for if input is blank..?

    // for use with strtok_r
    char *saveptr;
    char *token = strtok_r(fullCommand, " ", &saveptr); 
    	for (int i=0; token; i++) {
		// Check for & to be a background process
		if (strcmp(token, "&") == 0) {
			*background = 1;
		}
		// Check for < to denote input file
		else if (strcmp(token, "<") == 0) {
			token = strtok_r(NULL, " ", &saveptr);
			strcpy(inFile, token);
		}
		// Check for > to denote output file
		else if (strcmp(token, ">") == 0) {
			token = strtok_r(NULL, " ", &saveptr);
			strcpy(outFile, token);
		}
		// Otherwise, it's part of the command!
		else {
			arr[i] = strdup(token);
			// Replace $$ with pid
			// Only occurs at end of string in testscirpt
			for (int j=0; arr[i][j]; j++) {
				if (arr[i][j] == '$' &&
					 arr[i][j+1] == '$') {
					arr[i][j] = '\0';
					snprintf(arr[i], 256, "%s%d", arr[i], pid);
				}
			}
		}
		// Next!
		token = strtok_r(NULL, " ", &saveptr);
    }
}



int main() {
    // printf("Hey");
    int pid = getpid();
    int bg = 0;
    char inFile[256] = "";
    char outFile[256] = "";
    char* args[512];
    int continueLoop = 1;


    while(continueLoop) {
        // get input and parse
        commandPrompt(args, &bg, inFile, outFile, pid);

        // check if a comment line or blank line entered
        if (args[0][0] == '#' || args[0][0] == '\0') {
            printf("comment or blank line given \n");
        } else if (strcmp(args[0], "exit") == 0) {
            continueLoop = 0;
            printf("BYEEEEE\n");
        } else {
            printf("in loops \n");
        }
    }
    
    
    return 0;
}


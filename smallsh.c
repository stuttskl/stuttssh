#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h> // pid_t
#include <unistd.h> // fork

#define MAX_LEN 2048
#define MAX_ARGS 512

char *argList[MAX_ARGS]; // handles 512 arguments with "X" amount of characters, 
int numArgs = 0;
const char *builtInCommands[] = {"exit", "cd", "status"};
int processPID[1000];

void smallshRead(char*[], int*, char[], char[], int);
void smallshExecute(char*[], int*, struct sigaction, int*, char[], char[]);




void smallshRead(char* arr[], int* background, char inputName[], char outputName[], int pid) {
    // PARSE
    char buffer[MAX_LEN]; // was fullCommand

    // print colon to the user
    printf("%s",": ");
    fflush(stdout);
    // get full command from input from user
    fgets(buffer, MAX_LEN, stdin); 
    // strip newline
    buffer[strcspn(buffer, "\n")] = 0;
    printf("Your command is %s \n", buffer);

    // add handling for if input is blank..?

    // for use with strtok_r
    char *saveptr;
    char *token = strtok_r(buffer, " ", &saveptr); 
    
    for (int i=0; token; i++) {
		// Check for & to be a background process
		if (strcmp(token, "&") == 0) {
			*background = 1;
		}
		// Check for < to denote input file
		else if (strcmp(token, "<") == 0) {
			token = strtok_r(NULL, " ", &saveptr);
			strcpy(inputName, token);
		}
		// Check for > to denote output file
		else if (strcmp(token, ">") == 0) {
			token = strtok_r(NULL, " ", &saveptr);
			strcpy(outputName, token);
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

    // // checking if command ends in $$ for variable expansion
    // for (int i = 0; i < counter; i++) {
    //     for (int j = 0; j < strlen(argList[i]); j++) {
    //         if (argList[i][j] == '$' && argList[i][j+1] == '$') {
    //             argList[i][j] = '\0';
    //             argList[i][j+1] = '\0';

    //             snprintf(buffer, MAX_LEN, "%s%d", argList[i], getpid());
    //             argList[i] = buffer;
    //         }
    //     }
    // }
}

pid_t spawnpid = -5;
	int intVal = 10;
  // If fork is successful, the value of spawnpid will be 0 in the child, the child's pid in the parent
	spawnpid = fork();
	switch (spawnpid){
		case -1:
      // Code in this branch will be exected by the parent when fork() fails and the creation of child process fails as well
			perror("fork() failed!");
			exit(1);
			break;
		case 0:
      // spawnpid is 0. This means the child will execute the code in this branch
			intVal = intVal + 1;
			printf("I am the child! intVal = %d\n", intVal);
			break;
		default:
      // spawnpid is the pid of the child. This means the parent will execute the code in this branch
			intVal = intVal - 1;
			printf("I am the parent! ten = %d\n", intVal);
			break;
	}
	printf("This will be executed by both of us!\n");
}

void smallshExecute(char* arr[], int* childExitStatus, struct sigaction sa, int* background, char inputName[], char outputName[]) {
    int input, output, res;
    // much of this code was from the Canvas module/replit:https://repl.it/@cs344/4forkexamplec#main.c
    pid_t = spawnpid = -5;


    spawnpid = fork();
    switch(spawnpid) {
        // Code in this branch will be exected by the parent when fork() fails and the creation of child process fails as well
        case -1:
            perror("fork() failed!");
            exit(1);
            break;
        // spawnpid is 0. This means the child will execute the code in this branch
        case 0:
            printf("case 0\n");
            break;
        // spawnpid is the pid of the child. This means the parent will execute the code in this branch
        default:
            printf("case default\n");
			break;
    }

    }

void smallshExitStatus(int childExitMethod) {
    if (WIFEXITED(childExitMethod)) {
        printf("Exit value %d\n", WEXITSTATUS(childExitMethod));
    } else {
        printf("Terminated by signal %d\n", WTERMSIG(childExitMethod));
    }
}

int main() {
    const char *builtIns[] = {"exit", "ed", "status"};
    int pid = getpid();
	int continueLoop = 1;
	int i;
	int exitStatus = 0;
	int background = 0;

	char inFile[256] = "";
	char outFile[256] = "";
	char* input[512];
	for (i=0; i<512; i++) {
		input[i] = NULL;
	}

    while(continueLoop) {
        smallshRead(input, &background, inFile, outFile, pid);
        
        // COMMENT OR BLANK
        if (input[0][0] == '#' || input[0][0] == '\0') {
            continue;
        }
        // EXIT
        else if (strcmp(input[0], builtIns[0]) == 0) {
            printf("BYE\n");
            continueLoop = 0;
        }

        // CD
        else if (strcmp(input[0], builtIns[1]) == 0) {
            // Change to the directory specified
            if (input[1]) {
                if (chdir(input[1]) == -1) {
                    printf("cd: no such file or directory found: %s\n", input[1]);
                    fflush(stdout);
                }
            } else {
            // If directory is not specified, go to ~
                chdir(getenv("HOME"));
            }
        } else if (strcmp(input[0], "status") == 0) {
            smallshExitStatus(exitStatus);
        } else {
            smallshExecute(input, &exitStatus, sa_sigint, &background, inFile, outFile);
        }

        // Reset variables
		for (int i=0; input[i]; i++) {
			input[i] = NULL;
		}
		background = 0;
		inFile[0] = '\0';
		outFile[0] = '\0';
    }

    return 0;
}


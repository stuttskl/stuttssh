/*
Katie Stutts
Nov 3rd, 2020
CS 344: Operating Systems
Assignment 3: smallsh
*/

#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "smallsh.h"

#define MAX_LEN 2048 // for max command line length
#define MAX_ARGS 512 // for max arguments length

// function prototypes
void smallshRead(char*[], char[], char[]);
void smallshExecute(char*[], int*, struct sigaction, char[], char[]);
void smallshExitStatus(int);
void catchSIGTSTP();


// global variable to flag if background processes are allowed
int allowBackground = 1;
int fg_mode = 0;
int bg_process = 0;

/*
catchSIGTSTP checks whether to enter or exit foreground-only mode
*/
void catchSIGTSTP() {
	if (fg_mode == 0) {
		fg_mode = 1; 
		char* message = "Entering foreground-only mode (& is now ignored)\n";
		write(1, message, 49);
		fflush(stdout); //ensure output
	} else {
		fg_mode = 0;
		char* message = "Exiting foreground-only mode\n";
		write(1, message, 29);
		fflush(stdout);
	}
}

/*
smallshRead handles the reading of input from standard in. Utilizes strtok to 
parse words from the input buffer and stores into variables for later use
*/
void smallshRead(char* arr[], char inputName[], char outputName[]) {
	// buffer that can store up to 2048 elements
	char buffer[MAX_LEN]; 
	
	// print prompt (:) to the user
	printf(": ");
	// flush the standard out
	fflush(stdout);
	// get full command from standard in using fgets
	fgets(buffer, MAX_LEN, stdin); 
	// strip newline
	buffer[strcspn(buffer, "\n")] = 0;

	// read the buffer and save each word into tokens
	// this is adapted from bits in Assignment 1 and Assignment 2 
	char *token = strtok(buffer, " "); 

	for (int i = 0; token; i++) {
	// if a & is encountered, flip the bgProcess boolean flag to "true"
	if (strcmp(token, "&") == 0) { 
		bg_process = 1;
	}
	// if a < is encountered, input redirection is occuring
	else if (strcmp(token, "<") == 0) {
		token = strtok(NULL, " "); 
		// copy to variable inputName
		strcpy(inputName, token);
	}
	// if a > is encountered, outout redirection is occuring
	else if (strcmp(token, ">") == 0) {
		token = strtok(NULL, " "); 
		// copy to variable outputName
		strcpy(outputName, token);
	}
	else {
		// else, it is another command
		arr[i] = strdup(token); // ls, cd, mkdir etc
		// this checks for $$ to see if variable expansion is necessary
		for (int j = 0; arr[i][j]; j++) {
			// check if last two elems are $
			if (arr[i][j] == '$' && arr[i][j+1] == '$') {
				// null terminate string at $$
				arr[i][j] = '\0';
				// write to the output stream, "testdir PID"
				sprintf(arr[i], "%s%d", arr[i], getpid()); 
			}
		}
	}
	// advance to the next token
	token = strtok(NULL, " "); 
	}
}

/*
smallshExecute handles 
*/
void smallshExecute(char* arr[], int* childStatus, struct sigaction sa, char inFileName[], char outFileName[]) {
    // to track whether stdin/stdout redirection is successful, or if an err occured
		int result;
    // much of this code was from the Canvas module/replit: 
		// https://repl.it/@cs344/4forkexamplec#main.c
    pid_t spawnpid = -5;

    spawnpid = fork();
    switch (spawnpid) {
			// Code in this branch will be exected by the parent when fork() fails and the creation of child process fails as well
			case -1:
				perror("fork() failed!");
				// fflush(stdout);
				exit(1);
				break;
			// spawnpid is 0. This means the child will execute the code in this branch
			case 0:	
			// set signal handler back to default (cntrl C)
			// sigaction(SIGTSTP, &sa, NULL);
			sa.sa_handler = SIG_DFL;
			sigaction(SIGINT, &sa, NULL);

			// redirect output to /dev/null if otherwise not specified
			if(bg_process == 1) {
				int targetFD = open("/dev/null", O_WRONLY);
				if (targetFD == -1) {
					fprintf(stderr, "cannot access /dev/null\n");
					fflush(stderr);
					exit(1);
				}
			}

			// https://repl.it/@cs344/54redirectc#main.c
			// https://repl.it/@cs344/54sortViaFilesc
			if (strcmp(inFileName, "")) {
				// open the source file as read only
				int sourceFD = open(inFileName, O_RDONLY);
				// if this failed, print an error message and exit
				if (sourceFD == -1) {
					perror("source open()");
					exit(1);
				}
				// Redirect stdin to source file, 0 is reading from terminal
				result = dup2(sourceFD, 0);
				// if this failed, print an error message and exit
				if (result == -1) {
					perror("source dup2()");
					exit(2);
				}
				// close the file descriptor
				fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
			}

			if (strcmp(outFileName, "")) {
				// open the target file
				int targetFD = open(outFileName, O_WRONLY | O_CREAT | O_TRUNC, 0666); 
				// if this failed, print an error message and exit
				if (targetFD == -1) {
					perror("target open()");
					exit(1);
			}
			// Redirect stdout to target file, 1 is writing to terminal
			result = dup2(targetFD, 1);
			// if this failed, print an error message and exit
			if (result == -1) {
				perror("target dup2()"); 
				exit(2);
			}
			// close the file descriptor 
			// https://oregonstate.instructure.com/courses/1784217/pages/exploration-processes-and-i-slash-o?module_item_id=19893106
			fcntl(targetFD, F_SETFD, FD_CLOEXEC);
			}
			
			// use execvp() to execute command given
			// https://oregonstate.instructure.com/courses/1784217/pages/exploration-process-api-executing-a-new-program?module_item_id=19893097
			if (execvp(arr[0], (char* const*)arr)) {
				printf("%s: no such file or directory\n", arr[0]);
				fflush(stdout);
				exit(2);
			}
			break;
		default:	
			// if bg processes are allowed 
			if (bg_process == 1 && allowBackground == 1) {
				// https://repl.it/@cs344/42waitpidnohangc
				pid_t actualPid = waitpid(spawnpid, childStatus, WNOHANG);
				printf("background pid is %d\n", spawnpid);
				fflush(stdout);
			}
			// else, execute command w/o dealing with bg process
			else {
				pid_t actualPid = waitpid(spawnpid, childStatus, 0);
				// waitpid(spawnpid, childStatus, 0);
			}
		}

			// https://repl.it/@cs344/42waitpidexitc#main.c
			// -1 to wait for any child process
			while ((spawnpid = waitpid(-1, childStatus, WNOHANG)) > 0) {
				printf("background pid %d is done: ", spawnpid); // not sure if this needs to be here
				fflush(stdout);
				smallshExitStatus(*childStatus);
				// fflush(stdout);
			}
	}




/*
Checks for terimation status of a child process. 
Prints messages if the child terminated normally by 
printing an exit value, or abnormally printing the
terminating signal
https://oregonstate.instructure.com/courses/1784217/pages/exploration-process-api-monitoring-child-processes?module_item_id=19893096
*/
// https://repl.it/@cs344/42waitpidexitc#main.c
void smallshExitStatus(int childStatus) {
	// WIFEXITED returns true if child was terminated normally
	if (WIFEXITED(childStatus) != 0) {
		// WEXITSTATUS returns the status value the child passed to exit()
		printf("Exit value %d\n", WEXITSTATUS(childStatus));
		fflush(stdout);
	} else {
		// WTERMSIG will return the signal number that caused the child to terminate
		printf("Terminated by signal %d\n", WTERMSIG(childStatus));
		fflush(stdout);
	}
}

/*
main function of program, contains all of the variables needed throughout the
execution of the program. Sets all signal handlers, and runs
the main shell loops, making function call to helper function
to actually read and parse input from standard in. At the end of
each loop, resets all variables. 
*/
int main() {
	// to store built in commands 
	const char *builtIns[] = {"exit", "cd", "status"};
	// the shell loop will continue until this is 0
	int continueLoop = 1;
	int childExitStatus = 0;
	// this acts as a boolean flag to indicate whether background processes
	// int bgProcess = 0;
	// to hold input file name
	char inFile[256] = "";
	// to hold output file name
	char outFile[256] = "";
	char* args[512];
	for (int i = 0; i < MAX_ARGS; i++) { args[i] = NULL; }

	/*******************--SIGNAL HANDLERS--****************************/
	// https://repl.it/@cs344/53siguserc#signalexample.c	
	// declare sigaction STRUCT 
	struct sigaction SIGINT_action = { 0 };
	struct sigaction SIGTSTP_action = { 0 };

	// pass in sig handler with constant, indicating it should be ignored
	SIGINT_action.sa_handler = SIG_IGN;
	// bg/fg mode checker and switcher
	SIGTSTP_action.sa_handler = catchSIGTSTP;

	// Block all catchable signals while handle_SIGINT is running
	sigfillset(&SIGINT_action.sa_mask);
	sigfillset(&SIGTSTP_action.sa_mask);

	// No flags set
	SIGINT_action.sa_flags = 0;
	SIGTSTP_action.sa_flags = 0;

	sigaction(SIGINT, &SIGINT_action, NULL);
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	while(continueLoop) {
		smallshRead(args, inFile, outFile);
		// to check if this is a comment or blank line
		if (args[0][0] == '\0' || args[0][0] == '#'){
			// continue with next prompt
		}
		// if command given is "exit"
		else if (strcmp(args[0], builtIns[0]) == 0) {
			// set continueLoop boolean flag to False - ends shell loop
			continueLoop = 0;
		}

		// if command given is "cd"
		else if (strcmp(args[0], builtIns[1]) == 0) {
			// check args for target directory
			if (args[1]) {
				// use chdir() system call, andcheck if successful
				if (chdir(args[1]) == -1) {
					// if unsuccessful, print error message to user
					printf("cd: no such file or directory found: %s\n", args[1]);
					fflush(stdout);
				}
			} else {
				// if there is no target directory, cd to home dir (~)
				chdir(getenv("HOME"));
			}
		}
		// if command given in "status"
		else if (strcmp(args[0], builtIns[2]) == 0) {
			// get and print exit status
			smallshExitStatus(childExitStatus);
		} else {
			// or, execute whatever command given
			// ideally didn't want to pass around so many parameters, but chose to use this
			// method instead of using a bunch of global variables
			smallshExecute(args, &childExitStatus, SIGINT_action, inFile, outFile);
		}

		// go through and reset variables at the end of each shell loop
		for (int i = 0; args[i]; i++) { args[i] = NULL; }
		
		// bgProcess = 0;
		bg_process = 0;
		inFile[0] = '\0';
		outFile[0] = '\0';
		}
		return 0;
}

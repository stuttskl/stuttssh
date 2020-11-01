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

int allowBackground = 1;

void smallshRead(char* arr[], int* bgProcess, char inputName[], char outputName[]) {

    char buffer[MAX_LEN]; // buffer that can store up to 2048 elements
		int i, j;
    // print : in cyan 
		// printf("\033[0;36m");
		// print prompt (:) to the user
    printf(": ");
		// reset text color
		// printf("\033[0m");
    fflush(stdout);
    // get full command from standard in
    fgets(buffer, MAX_LEN, stdin); 
    // strip newline
    buffer[strcspn(buffer, "\n")] = 0;
    // printf("Your command is %s \n", buffer);
		// If it's blank, return blank
		if (!strcmp(buffer, "")) {
			arr[0] = strdup("");
			return;
		}

	
    // for use with strtok_r
    // char *saveptr;

    // read the buffer and save each word into tokens
    // this is adapted from bits in Assignment 1 and Assignment 2 
    char *token = strtok(buffer, " "); 

    for (i = 0; token; i++) {
			// printf("i = %d\n", i);
		// Check for & to be a background process
		if (strcmp(token, "&") == 0) { // should this be !strcmp or == 0?
			printf("background process allowed \n");
			*bgProcess = 1;
		}
		// Check for < to denote input file
		else if (strcmp(token, "<") == 0) {
			// printf("input redirection here \n");
			token = strtok(NULL, " "); 
			strcpy(inputName, token);

		}
		// Check for > to denote output file
		else if (strcmp(token, ">") == 0) {
			// printf("output redirection here \n");
			token = strtok(NULL, " "); 
			strcpy(outputName, token);
		}
		// Otherwise, it's part of the command!
		else {
			// printf("another command i guess \n");
			arr[i] = strdup(token); // ls, cd, mkdir etc
			// printf("arr[i] = %s \n", arr[i]);	
			// checking if command ends in $$ for variable expansion
			// printf("buffer: %s\n", buffer);
			
			for (j = 0; arr[i][j]; j++) {
				if (arr[i][j] == '$' && arr[i][j+1] == '$') {
					// null terminate string at $$
					arr[i][j] = '\0';
					// write to the output stream, "testdir PID"
					sprintf(arr[i], "%s%d", arr[i], getpid()); 
				}
			}
		}
		// // Next!
		token = strtok(NULL, " "); 
    }
}

void smallshExecute(char* arr[], int* childStatus, struct sigaction sa, int* bgProcess, char inFileName[], char outFileName[]) {
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
				exit(1);
				break;
			// spawnpid is 0. This means the child will execute the code in this branch
			case 0:	
			// The process will now take ^C as default
			sa.sa_handler = SIG_DFL;
			sigaction(SIGINT, &sa, NULL);

			// https://repl.it/@cs344/54redirectc#main.c
			// https://repl.it/@cs344/54sortViaFilesc
			if (strcmp(inFileName, "")) {
				// open the source file
				int sourceFD = open(inFileName, O_RDONLY);
				if (sourceFD == -1) {
					perror("source open()");
					exit(1);
				}
				// Redirect stdin to source file, 0 is reading from terminal
				result = dup2(sourceFD, 0);
				if (result == -1) {
					perror("source dup2()");
					exit(2);
				}
				// close the file descriptor
				fcntl(sourceFD, F_SETFD, FD_CLOEXEC);
			}

			if (strcmp(outFileName, "")) {
				// open the target file
				int targetFD = open(outFileName, O_WRONLY | O_CREAT | O_TRUNC, 0666); // or 0644
				if (targetFD == -1) {
					perror("target open()");
					exit(1);
			}
			// Redirect stdout to target file, 1 is writing to terminal
			result = dup2(targetFD, 1);
			if (result == -1) {
				perror("target dup2()"); 
				exit(2);
			}
			// close the file descriptor 
			// https://oregonstate.instructure.com/courses/1784217/pages/exploration-processes-and-i-slash-o?module_item_id=19893106
			fcntl(targetFD, F_SETFD, FD_CLOEXEC);
			}
			
			// Execute it!
			if (execvp(arr[0], (char* const*)arr)) {
				printf("%s: no such file or directory\n", arr[0]);
				fflush(stdout);
				exit(2);
			}
			break;
		
		default:	
			// Execute a process in the background ONLY if allowBackground
			if (*bgProcess && allowBackground) {
				pid_t actualPid = waitpid(spawnpid, childStatus, WNOHANG);
				printf("background pid is %d\n", spawnpid);
				fflush(stdout);
			}
			// Otherwise execute it like normal
			else {
				pid_t actualPid = waitpid(spawnpid, childStatus, 0);
			}

		// Check for terminated background processes!	
		while ((spawnpid = waitpid(-1, childStatus, WNOHANG)) > 0) {
			printf("child %d terminated\n", spawnpid);
			smallshExitStatus(*childStatus);
			fflush(stdout);
		}
	}

}

void catchSIGTSTP(int signo) {
	// If it's 1, set it to 0 and display a message reentrantly
	if (allowBackground == 1) {
		char* message = "Entering foreground-only mode (& is now ignored)\n";
		write(1, message, 49); // 50?
		fflush(stdout);
		allowBackground = 0;
	}
	// If it's 0, set it to 1 and display a message reentrantly
	else {
		char* message = "Exiting foreground-only mode\n";
		write (1, message, 29); // 30?
		fflush(stdout);
		allowBackground = 1;
	}
}

// https://repl.it/@cs344/42waitpidexitc#main.c
void smallshExitStatus(int childStatus) {
	// WIFEXITED returns true if child was terminated normally
	if (WIFEXITED(childStatus)) {
		// WEXITSTATUS returns the status value the child passed to exit()
		printf("Exit value %d\n", WEXITSTATUS(childStatus));
		fflush(stdout);
	} else {
		// WTERMSIG will return the signal number that caused the child to terminate
		printf("Terminated by signal %d\n", WTERMSIG(childStatus));
		fflush(stdout);
	}
}

int main() {
	// to store built in commands 
	const char *builtIns[] = {"exit", "cd", "status"};
	int pid = getpid();
	// the shell loop will continue until this is 0
	int continueLoop = 1;
	int exitStatus = 0;
	// this acts as a boolean flag to indicate whether background processes
	int bgProcess = 0;
	// to hold input file name
	char inFile[256] = "";
	// to hold output file name
	char outFile[256] = "";
	char* args[512];
	for (int i = 0; i < MAX_ARGS; i++) {
		args[i] = NULL;
	}

	// Signal Handlers
	// https://repl.it/@cs344/53siguserc#signalexample.c	
	// declare sigaction STRUCT 
	struct sigaction SIGINT_action = { 0 };
	struct sigaction SIGTSTP_action = { 0 };

	// pass in sig handler with constant, indicating it should be ignored
	SIGINT_action.sa_handler = SIG_IGN;
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
		smallshRead(args, &bgProcess, inFile, outFile);
		// to check if this is a comment or blank line
		if (args[0][0] == '\0' || args[0][0] == '#'){
			// continue
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
				// printf("args[1] aka the dir we are trying to cd into is %s\n", args[1]);
				// use chdir() system call, andcheck if successful
				if (chdir(args[1]) == -1) {
					// if unsuccessful, print error message to user
					printf("cd: no such file or directory found: %s\n", args[1]);
					fflush(stdout);
				}
			} else {
				// if there is no target directory, cd to home
				chdir(getenv("HOME"));
			}
		}
		// if command given in "status"
		else if (strcmp(args[0], builtIns[2]) == 0) {
			// get and print exit status
			smallshExitStatus(exitStatus);
		} else {
			// or, execute whatever command given
			smallshExecute(args, &exitStatus, SIGINT_action, &bgProcess, inFile, outFile);
		}

		// Reset variables
		for (int i = 0; args[i]; i++) { args[i] = NULL; }
		bgProcess = 0;
		inFile[0] = '\0';
		outFile[0] = '\0';
	}
	return 0;
}
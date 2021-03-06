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

#define MAX_LEN 2048 // for max command line length
#define MAX_ARGS 512 // for max arguments length

// global variable to flag if background processes are allowed
int allowBackground = 1;

// function prototypes
void smallshRead(char* arr[], int* bgProcess, char inputName[], char outputName[]);
void smallshExecute(char* arr[], int* childStatus, struct sigaction sa, int* bgProcess, char inFileName[], char outFileName[]);
void smallshExitStatus(int childStatus);
void catchSIGTSTP(int signo);

/*
smallshRead handles the reading of input from standard in. Utilizes strtok to 
parse words from the input buffer and stores into variables for later use
*/
void smallshRead(char* arr[], int* bgProcess, char inputName[], char outputName[]) {
    char buffer[MAX_LEN]; // buffer that can store up to 2048 elements

	printf("\033[0;36m");    
		// print prompt (:) to the user
    printf(": ");
	printf("\033[0m");
		
    fflush(stdout);
    // get full command from standard in
    fgets(buffer, MAX_LEN, stdin); 
    // strip newline
    buffer[strcspn(buffer, "\n")] = 0;
    // printf("Your command is %s \n", buffer);
		
		// if input is blank, return empty string
		if (!strcmp(buffer, "")) {
			arr[0] = strdup("");
			return;
		}

    // read the buffer and save each word into tokens
    // this is adapted from bits in Assignment 1 and Assignment 2 
    char *token = strtok(buffer, " "); 

    for (int i = 0; token; i++) {
			// if a & is encountered and if it's the last char in the string
			// flip the bgProcess boolean flag to "true"
			if (strcmp(token, "&") == 0 && strrchr(token, '\0')[-1]) { 
				// printf("background process allowed \n");
				*bgProcess = 1;
			}
			// if a < is encountered, input redirection is occuring
			else if (strcmp(token, "<") == 0) {
				// printf("input redirection here \n");
				token = strtok(NULL, " "); 
				strcpy(inputName, token);
			}
			// if a > is encountered, outout redirection is occuring
			else if (strcmp(token, ">") == 0) {
				// printf("output redirection here \n");
				token = strtok(NULL, " "); 
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
		// move along to next token
		token = strtok(NULL, " "); 
    }
}

/*
smallshExecute handles the main execution of commands. 
makes use of a switch statement to control the flow over whether a child or parent
process will be running a given branch of code. Also handles any I/O redirection
*/
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
			// set signal handler back to default (cntrl C)
			sa.sa_handler = SIG_DFL;
			sigaction(SIGINT, &sa, NULL);

			// redirect output to /dev/null if otherwise not specified
			if(allowBackground == 1) {
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
				// open the source file
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
				int targetFD = open(outFileName, O_WRONLY | O_CREAT | O_TRUNC, 0666); // or 0644
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
				printf("\033[1;31m");
				printf("%s: no such file or directory\n", arr[0]);
				printf("\033[0m");
				fflush(stdout);
				exit(2);
			}
			break;
		default:	
			// execute a process in the background ONLY if allowBackground
			if (*bgProcess && allowBackground) {
				pid_t actualPid = waitpid(spawnpid, childStatus, WNOHANG);
				printf("background pid is %d\n", spawnpid);
				fflush(stdout);
			}
			// else, execute command w/o dealing with bg process
			else {
				pid_t actualPid = waitpid(spawnpid, childStatus, 0);
			}

		// https://repl.it/@cs344/42waitpidexitc#main.c
		// -1 to wait for any child process
		while ((spawnpid = waitpid(-1, childStatus, WNOHANG)) > 0) {
			printf("child %d terminated: ", spawnpid);
			smallshExitStatus(*childStatus);
			fflush(stdout);
		}
	}
}

/*
catchSIGTSTP checks whether to enter or exit foreground-only mode,
and toggles accordingly
*/
void catchSIGTSTP(int signo) {
	// enter into fg mode, write message than flip bool flag
	if (allowBackground == 1) {
        printf("\033[1;31m");
		char* message = "Entering foreground-only mode (& is now ignored)\n";
        printf("\033[0m");
		write(1, message, 49); 
		fflush(stdout);
		allowBackground = 0;
	}
	// else, do the inverse
	else {
        printf("\033[1;31m");
		char* message = "Exiting foreground-only mode\n";
        printf("\033[0m");
		write (1, message, 29); 
		fflush(stdout);
		allowBackground = 1;
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
	if (WIFEXITED(childStatus)) {
		// WEXITSTATUS returns the status value the child passed to exit()
		printf("exit value %d\n", WEXITSTATUS(childStatus));
		fflush(stdout);
	} else {
		// WTERMSIG will return the signal number that caused the child to terminate
		printf("terminated by signal %d\n", WTERMSIG(childStatus));
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
	int exitStatus = 0;
	// this acts as a boolean flag to indicate whether background processes
	int bgProcess = 0;
	// to hold input file name
	char inFile[256] = "";
	// to hold output file name
	char outFile[256] = "";
	char* args[512];
	// set args array to null
	for (int i = 0; i < MAX_ARGS; i++) {
		args[i] = NULL;
	}

	/*******************--SIGNAL HANDLERS--****************************/	
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

	/*******************--MAIN SHELL LOOP--************************/
	while(continueLoop) {
		smallshRead(args, &bgProcess, inFile, outFile);
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
				// printf("args[1] aka the dir we are trying to cd into is %s\n", args[1]);
				// use chdir() system call, andcheck if successful
				if (chdir(args[1]) == -1) {
					// if unsuccessful, print error message to user
                    printf("\033[1;31m");
					printf("cd: no such file or directory found: %s\n", args[1]);
                    printf("\033[0m");
					fflush(stdout);
				}
			} else {
				// if there is no target directory, cd to home (~)
				chdir(getenv("HOME"));
			}
		}
		// if command given in "status"
		else if (strcmp(args[0], builtIns[2]) == 0) {
			// get and print exit status
			smallshExitStatus(exitStatus);
		} else {
			// or, execute whatever command given
			// ideally didn't want to pass around so many parameters, 
			//  but chose to use this
			// method instead of using a bunch of global variables
			smallshExecute(args, &exitStatus, SIGINT_action, &bgProcess, inFile, outFile);
		}

		// go through and reset variables at the end of each shell loop
		for (int i = 0; args[i]; i++) { args[i] = NULL; }
		bgProcess = 0;
		inFile[0] = '\0';
		outFile[0] = '\0';
	}
	return 0;
}

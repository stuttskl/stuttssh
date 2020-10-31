#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/wait.h>
#include "smallsh.h"

#define MAX_LEN 2049 // for max command line length
#define MAX_ARGS 512 // for max arguments length

int allowBackground = 1;

void smallshRead(char* arr[], int* bgProcess, char inputName[], char outputName[], int pid) {

    char buffer[MAX_LEN]; // buffer that can store up to 2048 elements

    // print prompt (:) to the user
    printf("%s",": ");
    fflush(stdout);
    // get full command from standard in
    fgets(buffer, MAX_LEN, stdin); 
    // strip newline
    buffer[strcspn(buffer, "\n")] = 0;
    // printf("Your command is %s \n", buffer);

    // add handling for if input is blank..?

    // for use with strtok_r
    char *saveptr;

    // read the buffer and save each word into tokens
    // this is adapted from bits in Assignment 1 and Assignment 2 
    char *token = strtok_r(buffer, " ", &saveptr); 
    
    for (int i = 0; token; i++) {
		// Check for & to be a background process
		if (strcmp(token, "&") == 0) { // should this be !strcmp or == 0?
			*bgProcess = 1;
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
      // checking if command ends in $$ for variable expansion
			for (int j = 0; arr[i][j]; j++) {
				if (arr[i][j] == '$' && arr[i][j+1] == '$') {
					arr[i][j] = '\0';
          arr[i][j+1] = '\0'; // maybe this doesn't need to be here?
					snprintf(arr[i], 256, "%s%d", arr[i], pid); // pid or getpid()? 
				}
			}
		}
		// Next!
		token = strtok_r(NULL, " ", &saveptr);
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
				// trigger its close
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
	} else {
		// WTERMSIG will return the signal number that caused the child to terminate
		printf("Terminated by signal %d\n", WTERMSIG(childStatus));
	}
}

int main() {
	// to store built in commands 
	const char *builtIns[] = {"exit", "ed", "status"};
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
	// Ignore ^C
	// declare sigaction STRUCT 
	struct sigaction SIGINT_action = { 0 };
	// pass in sig handler with constant, indicating it should be ignored
	SIGINT_action.sa_handler = SIG_IGN;
	// Block all catchable signals while handle_SIGINT is running
	sigfillset(&SIGINT_action.sa_mask);
	// No flags set
	SIGINT_action.sa_flags = 0;
	sigaction(SIGINT, &SIGINT_action, NULL);

	// Redirect ^Z to catchSIGTSTP()
	struct sigaction SIGTSTP_action = { 0 };
	SIGTSTP_action.sa_handler = catchSIGTSTP;
	sigfillset(&SIGTSTP_action.sa_mask);
    // No flags set
	SIGTSTP_action.sa_flags = 0;
	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

    while(continueLoop) {
        smallshRead(args, &bgProcess, inFile, outFile, pid);
        
        // to check if this is a comment or blank line
        if (args[0][0] == '\n' || args[0][0] == '#'){
					// nothing goes here becuase it is used to ignore stuff
        }
        // if command given is "exit"
        else if (strcmp(args[0], builtIns[0]) == 0) {
					printf("BYE\n");
					// set continueLoop boolean flag to False - ends shell loop
					continueLoop = 0;
        }

        // if command given is "cd"
        else if (strcmp(args[0], builtIns[1]) == 0) {
					// check args for target directory
					if (args[1]) {
						// use chdir() system call, and check if successful
						if (chdir(args[1]) == -1) {
							// if unsuccessful, print error message to user
							printf("cd: no such file or directory found: %s\n", args[1]);
							fflush(stdout);
						}
					} else {
            // if there is no target directory, cd to home
						chdir(getenv("HOME"));
					}
				// if command given in "status"
        } else if (strcmp(args[0], "status") == 0) {
					// get and print exit status
					smallshExitStatus(exitStatus);
        } else {
					// or, execute whatever command given
					smallshExecute(args, &exitStatus, SIGINT_action, &bgProcess, inFile, outFile);
        }

    // Reset variables
		for (int i = 0; args[i]; i++) {
			args[i] = NULL;
		}
		bgProcess = 0;
		inFile[0] = '\0';
		outFile[0] = '\0';
    }

    return 0;
}


// ./p3testscript > mytestresults 2>&1 
/*
PRE-SCRIPT INFO
  Grading Script PID: 199774
  Note: your smallsh will report a different PID when evaluating $$
: BEGINNING TEST SCRIPT
: 
: --------------------
: Using comment (5 points if only next prompt is displayed next)
: : 
: 
: --------------------
: ls (10 points for returning dir contents)
: junk
junk2
makefile
mytestresults
otherfile.c
p3testscript
README.md
smallsh
smallsh.c
smallsh.h
uneditedFile.c
: 
: 
: --------------------
: ls out junk
: : 
: 
: --------------------
: cat junk (15 points for correctly returning contents of junk)
: junk
junk2
makefile
mytestresults
otherfile.c
p3testscript
README.md
smallsh
smallsh.c
smallsh.h
uneditedFile.c
: 
: 
: --------------------
: wc in junk (15 points for returning correct numbers from wc)
:  11  11 112
: 
: 
: --------------------
: wc in junk out junk2; cat junk2 (10 points for returning correct numbers from wc)
: :  11  11 112
: 
: 
: --------------------
: test -f badfile (10 points for returning error value of 1, note extraneous &)
: : Exit value 1
: 
: 
: --------------------
: wc in badfile (10 points for returning text error)
: Unable to open input file
: No such file or directory
: 
: 
: --------------------
: badfile (10 points for returning text error)
: badfile: no such file or directory
: 
: 
: --------------------
: sleep 100 background (10 points for returning process ID of sleeper)
: background pid is 199835
: 
: 
: --------------------
: pkill -signal SIGTERM sleep (10 points for pid of killed process, 10 points for signal)
: (Ignore message about Operation Not Permitted)
: pkill: killing pid 188796 failed: Operation not permitted
pkill: killing pid 190913 failed: Operation not permitted
pkill: killing pid 190978 failed: Operation not permitted
pkill: killing pid 191457 failed: Operation not permitted
pkill: killing pid 192856 failed: Operation not permitted
pkill: killing pid 194305 failed: Operation not permitted
pkill: killing pid 197830 failed: Operation not permitted
pkill: killing pid 198164 failed: Operation not permitted
pkill: killing pid 199467 failed: Operation not permitted
pkill: killing pid 222355 failed: Operation not permitted
pkill: killing pid 236863 failed: Operation not permitted
child 199835 terminated
Terminated by signal 15
: 
: 
: --------------------
: sleep 1 background (10 pts for pid of bg ps when done, 10 for exit value)
: background pid is 199877
: : 
child 199877 terminated
Exit value 0
: 
: --------------------
: pwd
: /nfs/stak/users/stuttsk/cs344/A3
: 
: 
: --------------------
: cd
: : 
: 
: --------------------
: pwd (10 points for being in the HOME dir)
: /nfs/stak/users/stuttsk/cs344/A3
: 
: 
: --------------------
: mkdir 199781
: : 
: 
: --------------------
: cd 199781
: : 
: 
: --------------------
: pwd (5 points for being in the newly created dir)
: /nfs/stak/users/stuttsk/cs344/A3
: --------------------
: background pid is 200036
: Testing foreground-only mode (20 points for entry
Entering foreground-only mode (& is now ignored)
child 200036 terminated
Exit value 0
: Wed Oct 28 19:42:30 PDT 2020
child 200037 terminated
Exit value 0
: : Wed Oct 28 19:42:35 PDT 2020
: Exiting foreground-only mode
: BYE

*/
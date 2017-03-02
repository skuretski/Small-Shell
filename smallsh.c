/*
 *  smallsh.c
 *
 *  Created on: Feb 16, 2016
 *  Author: Susan Kuretski
 *  CS344-400, Winter 2016
 *
 *  Implementation of a simple C-shell like program.
 *  cd, status, and exit are program handled; all other
 *  commands are fork and exec'd as child processes to
 *  native shell i.e. ls, wc, sleep
 *
 *  Allows background processes as denoted by "&"
 *  Note: some code structure has influence from lecture sample
 *  code specifically in regards to fork, exec, and sighandler sections
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>

char** getArgs(char *input, int *numArgs);
void printStatus(int status);

int main(){
	int exitShell = 0;
	int status;
	int is_bg;
	char **args;
	int numArgs;
	int *pArgs = &numArgs;
	char userInput[2048];
	int fd;
	char *inFile = NULL;
	char *outFile = NULL;
	pid_t spawnpid;
	struct sigaction sa;
	//Set sigaction member variables
	//SIGINT ignore is set (default is set for foreground process later)
	sa.sa_handler = SIG_IGN;
	sigfillset(&sa.sa_mask);
	sa.sa_flags = 0;
	sigaction(SIGINT, &sa, NULL);

	do{
		//Set and/or reset is_bg flag
		is_bg = 0;
		numArgs = 0;
		printf("%s", ": ");
		fflush(stdout);
		args = getArgs(userInput, pArgs);
		//If & exists, set background flag
		if(numArgs == 3 && args[2] != NULL && (strcmp(args[2], "&")) == 0)
			is_bg = 1;
		//If file redirection is indicated set input/output files
		else if(numArgs == 3 && args[2] != NULL && (strcmp(args[1], "<") == 0 || (strcmp(args[1], ">") == 0))){
			if(strcmp(args[1], "<") == 0)
				inFile = strdup(args[2]);
			else if(strcmp(args[1], ">") == 0)
				outFile = strdup(args[2]);
		}
		//No arguments
		if(args[0] == NULL || strcmp(args[0], "#") == 0){
			status = 0;
		}
		//exit
		else if(strcmp(args[0], "exit") == 0){
			status = 1;
			exitShell = 1;
		}
		//cd
		else if(strcmp(args[0], "cd") == 0){
			//cd only command
			if(args[1] == NULL){
				chdir(getenv("HOME"));
				status = 0;
			}
			//cd to a location
			else if(args[1] != NULL){
				if(chdir(args[1]) != 0){
					fprintf(stderr, "No such file or directory.\n");
					fflush(stderr);
					status = 1;
				}
				//cd to location
				else{
					chdir(args[1]);
				}
			}
		}
		//status
		else if(strcmp(args[0], "status") == 0){
			printStatus(status);
		}
		//Fork, check if redirection is needed, set SIGINT handlers as needed, then exec
		else{
			spawnpid = fork();
			switch(spawnpid){
			//Handle fork error
			case -1:
				perror("Error with fork.");
				status = 1;
				exit(1);
				break;
			//Child process
			case 0:
				if(is_bg == 0){
					//Default SIGINT in non-background processes
					sa.sa_handler = SIG_DFL;
					sigaction(SIGINT, &sa, NULL);
				}
				//stdin redirection if needed
				if(inFile != NULL){
					fd = open(inFile, O_RDONLY);
					//Will exit if cannot open or error and will NOT execvp
					if(fd == -1){;
						printf("%s\n", "smallsh: cannot open file.");
						fflush(stdout);
						status = 1;
						_exit(1);
					}
						if(dup2(fd, 0) == -1){
							perror("dup2");
							_exit(1);
						}
						args[1] = NULL;
						close(fd);
				}
				//Redirection of stdin & stdout to dev/null if background process
				else if(is_bg == 1){
					//Will exit if cannot open or error and will NOT execvp
					fd = open("/dev/null", O_RDONLY);
					if(fd == -1){
						perror("open");
						_exit(1);
					}
					if(dup2(fd, 0) == -1){
						perror("dup2");
						_exit(1);
					}
					args[2] = NULL;		//set next argument to NULL for execvp
					close(fd);
				}
				//Redirection of stdout if needed
				if(outFile != NULL){
					fd = open(outFile, O_WRONLY | O_CREAT | O_TRUNC, 0744);
					if(fd == -1){
						printf("%s\n", "smallsh: cannot open file.");
						fflush(stdout);
						status = 1;
						_exit(1);
					}
					if(dup2(fd, 1) == -1){
						perror("dup2");
						_exit(1);
					}
					args[1] = NULL;		//set next argument to NULL for execvp
					close(fd);
				}
				//Exec commands (Will exit if error)
				if(execvp(args[0], args)){
					perror(args[0]);
					_exit(1);
				}
				break;
			//Parent process
			default:
				//If not background, wait for foreground process
				if(!is_bg){
					waitpid(spawnpid, &status, 0);
				}
				//If background process, print out pid
				else{
					printf("%s%d\n", "background pid is ", spawnpid);
					fflush(stdout);
					break;
				}
			}
		}
		cleanup:
		//Dynamic memory clean up
		free(args);
		//reset files
		inFile = NULL;
		outFile = NULL;
		//Check for background processes
		spawnpid = waitpid(-1, &status, WNOHANG);
		//Credit from Canvas discussions (Chad Johnson re: waitpid(-1, &status, WNOHANG) and
		//handling background processes in a while-loop)
		while(spawnpid > 0){
			printf("%s%d%s", "background pid ", spawnpid, " is done: ");
			fflush(stdout);
			printStatus(status);
			spawnpid = waitpid(-1, &status, WNOHANG);
		}
	}while(exitShell == 0);
	exit(0);
}
/* Description: returns array of char* arguments, number of arguments
 * is passed by reference and will reflect resulting number
 * Pre-condition: input is char* array[2048] and numArgs = 0
 * Post-condition: returns char** dynamic array of arguments
 */
char** getArgs(char *input, int *numArgs){
	int buffer = 50;
	char **args = malloc(buffer * sizeof(char*));
	args[1] = NULL;
	char *token;
	fgets(input, 2048, stdin);
	token = strtok(input, "\t\n\a\r ");
	while(token != NULL){
		args[(*numArgs)] = token;
		(*numArgs)++;
		//Reallocates if numArgs > buffer to twice buffer size
		if((*numArgs) >= buffer){
			buffer = buffer * 2;
			args = realloc(args, buffer * sizeof(char*));
		}
		token = strtok(NULL, "\t\n\r ");
	}
	args[*numArgs] = NULL;
	return args;
}
/* Description: prints status of process end
 * Pre-condition: "status" use with waitpid or wait
 * Post-condition: prints exit value, terminated signal or
 * error exit value
 */
void printStatus(int status){
	if(WIFEXITED(status)){
		printf("%s%d\n", "exit value ", WEXITSTATUS(status));
		fflush(stdout);
	}
	else if(WIFSIGNALED(status)){
		printf("%s%d\n", "terminated by signal ", WTERMSIG(status));
		fflush(stdout);
	}
	else
		printf("%s%d\n", "exit value ", 1);
}



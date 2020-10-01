//Aishwarya Vellanki
//CS344- Operating Systems 1
//Assignment 3: smallsh
//DUE: 11-20-2019


#include<stdio.h>
#include<stdlib.h>
#include<fcntl.h>
#include<unistd.h>
#include<time.h>
#include<signal.h>
#include<string.h>
#include<stdbool.h>
#include<stddef.h>
#include<sys/wait.h>
#include<sys/types.h>

//global variables
pid_t thePID;
char process_id[6];
int backP[100] = {0};
int backPCount = 0;
volatile sig_atomic_t bPermitted = 1;
int theStatus = 0; 
bool isSignal;
int sigOrstatNum = 0;
bool isBackground = false;
bool redirectionGood = true;
char* dolladolla = NULL;
sigset_t sigtstpSet;

//function prototypes
void smallsh_exit();
void prompt();
char* get_userInput(char**);
void parse_input(char *, char**);
void change_directory();
void smallsh_exit();
int check_ifBuiltIn(char**);
bool check_ifBackgroundP(char**);
char* expand_userInput(char**);
void smallsh_status();
void ignore_SIGINT(int sig);
int check_ifReadIn(char**);
int check_ifReadOut(char**);
int find_lastString(char**);
int get_RedirectionIndex(char**);
char** handle_BRedirection(char**);
char** handle_Redirection(char**);
char** trunc_ParsedWords(char**, int);
void call_builtIn(int funcNum, char**);
char** do_Redirection(char**, int);



//////////////////////////////////////////////////////////////////////////////
//Function Name: check_children
//Description:This function will find background processes taht have been killed
//clean up after them, and print their exit or terminating signal status
//Parameters:none
//Pre-conditions: a background process array must be initialized and counter set
//Post-conditions: n/a
//////////////////////////////////////////////////////////////////////////////
void check_children(){
	int i,j, deadChild = 0;
	int tempPID;
	//find a dead child
	while((tempPID = waitpid(-1, &theStatus, WNOHANG)) > 0){
		//identify the where this dead child is within the background process array
		for (i = 0; i < backPCount; i++){
			if(tempPID == backP[i]){
				deadChild = i;	
				//get either the exit/signal termination status and print it out
				if(WIFEXITED(theStatus) != 0){
					sigOrstatNum = WEXITSTATUS(theStatus);
					printf("background pid %d is done: exit value %d\n", backP[i], sigOrstatNum);
					fflush(stdout);
				}
				else if(WIFSIGNALED(theStatus) != 0){
					sigOrstatNum = WTERMSIG(theStatus);
					printf("background pid %d is done: terminated by signal %d\n", backP[i], sigOrstatNum);
					fflush(stdout);
				}
				//from where the deadchild was identified, move the process ids up by one
				for(j = deadChild; j<backPCount; j++){
					backP[j] = backP[j+1];
				}
				//decrement the background process child count
				backPCount--;
				//set the last index of the array to 0
				backP[backPCount] = 0;
			}
		}
	}
		
}

//////////////////////////////////////////////////////////////////////////////
//Function Name: SIGTSTPcatch
//Description: This is the SIGTSTP signal handler function which handles the 
//ctrl-Z command which  will notify the user of whether foreground only mode is turned 
//on or off
//Parameters: int sig
//Pre-conditions: a global "boolean" variable to specify whether background
//processes are allowed, a SIGTSTP handler and set
//Post-conditions: n/a
//////////////////////////////////////////////////////////////////////////////
void SIGTSTPcatch(int sig){
	//toggle the boolean
	bPermitted =! bPermitted;
	//if background processes are not permitted
	if(bPermitted == 1){
		write(1, "\nExiting foreground only mode\n:", 30);
		fflush(stdout);
	}
	//if background processes are not permitted
	else if(bPermitted == 0){
		write(1, "\nEntering foreground only mode (& is now ignored)\n:", 50);
		fflush(stdout);
	}
}

//////////////////////////////////////////////////////////////////////////////
//Function Name: prompt
//Description: this function prints the colon prompt for the user to indicate 
//command line access to the user
//Parameters: n/a
//Pre-conditions: n/a
//Post-conditions: n/a
//////////////////////////////////////////////////////////////////////////////
void prompt(){
	printf(": ");
	fflush(stdout);
}

//////////////////////////////////////////////////////////////////////////////
//Function Name: get_userInput
//Description: this function recieves up to 2048 characters from the command line
//from the user
//Parameters: a char** array which will hold the input
//Pre-conditions: a char** array needs to be 
//Post-conditions: n/a
/////////////////////////////////////////////////////////////////////////////
char* get_userInput(char **userInput){
	//max length set to 2048 characters
	size_t maxLength = 2048;
	//get userinput and store in variable userInput
	getline(userInput, &maxLength, stdin);
	return expand_userInput(userInput);
}

//////////////////////////////////////////////////////////////////////////////
//Function Name: expand_userInput
//Description: this function will check for the '$$' within the user input and 
//be replaced with the process pid
//Parameters: char** of the userInput
//Pre-conditions: input must be stored within a variable of type char**
//Post-conditions: n/a
//////////////////////////////////////////////////////////////////////////////
char* expand_userInput(char** userInput){
	//get the process id
	thePID = getpid();
	//convert the process id into a char
	sprintf(process_id, "%d", thePID);
	//identify all the $$ and replace with process id
	while ((dolladolla = strstr(*userInput, "$$"))){
		//malloc space for a new string that is bigger
		char* newString =  malloc(strlen(*userInput)+strlen(process_id)-1);
		strncat(newString, *userInput, dolladolla - *userInput);
		strcat(newString, process_id);
		strcat(newString, dolladolla+2);
		free(*userInput);
		*userInput = newString;
	}	
	return *userInput;
}

//////////////////////////////////////////////////////////////////////////////
//Function Name: parse_input
//Description: this function will identify all white spaces and deliminate with
//a null terminator creating an array of individually null terminated strings
//Parameters: userInput char** array, and the array in which the parsed words
//are going to be stored
//Pre-conditions: boht the paramters must be declared and initialized
//Post-conditions: n/a
//////////////////////////////////////////////////////////////////////////////
void parse_input(char *userInput, char** parsedWords){
	int count = 0;
	char* word = strtok(userInput, "\n ");
	//go through the user input, and replace all spaces and newlines with NULL
	while (word !=NULL){
		//printf("string: %s\n",word);
		parsedWords[count] = word;
		word = strtok(NULL, "\n ");
		count++;
	}
}

//////////////////////////////////////////////////////////////////////////////
//Function Name: change_directory
//Description: this function will handle the cd built in command
//Parameters: the parsed words array of type char**
//Pre-conditions: the array must contain the parsed user input
//Post-conditions: check built in function and a function to call this function
//must be created
//////////////////////////////////////////////////////////////////////////////
void change_directory(char** parsedWords){
	char* path;
	//if the user doesn't provide a path then get the path set from the environment
	if(parsedWords[1] == 0){
		path = getenv("HOME");
	}
	//get the path from what the user entered
	else{
		path = parsedWords[1];
		strtok(parsedWords[1], "\n");
	}
	//change the directory
	chdir(path);
}

//////////////////////////////////////////////////////////////////////////////
//Function Name: smallsh_exit
//Description: this function is for the exit built in command, which will exit 
//the shell
//Parameters: none
//Pre-conditions: there must be an array and count for the number of background
//processes
//Post-conditions: all background processes are killed before exiting
//////////////////////////////////////////////////////////////////////////////
void smallsh_exit(){
	int i = 0;
	//if there are children processes
	if(backPCount > 0){
		//kill them
		for(i = 0; i < backPCount; i++){
			kill(backP[i], SIGKILL);
		}
	}
	exit(0);
}

//////////////////////////////////////////////////////////////////////////////
//Function Name: smallsh_status
//Description: this is the function for the status built in command which will 
//print the exit status of the last foreground process 
//Parameters: none
//Pre-conditions: the exit status of a foreground process must be caught with the
//global variable 'sigOrstatNum' which will be recieved from teh sigstat or sigterm
//functions
//Post-conditions: n/a
//////////////////////////////////////////////////////////////////////////////
void smallsh_status(){
	//terminated by signal so print htis message
	if(isSignal){
	  	printf("terminated by signal %d\n", sigOrstatNum);
		fflush(stdout);
	}
	//terminated with exit status so print this message
	else{
		printf("exit value %d\n", sigOrstatNum);
		fflush(stdout);
	}
}

//////////////////////////////////////////////////////////////////////////////
//Function Name: get_RedirectionIndex
//Description: this function will start from the end of the parsed user input array
//and identify the index of where the redirection symbol would be depenendent on 
//whether a & exists
//Parameters: parsed words array
//Pre-conditions: n/a
//Post-conditions: n/a
//////////////////////////////////////////////////////////////////////////////
int get_RedirectionIndex(char** parsedWords){
	int end;
	int redirectionIndex;
	//find the index of the last string
	end = find_lastString(parsedWords);
	//if there is a ampersand then the redirection index would be two away from the end
	if (check_ifBackgroundP(parsedWords) == true){
		redirectionIndex = end - 2;
	}
	//otherwise the redirection index would be one away from the end
	else{
		redirectionIndex = end - 1;
	}
	return redirectionIndex;
}

//////////////////////////////////////////////////////////////////////////////
//Function Name: handle_BReduction
//Description: this function will take care of all background redirection
//Parameters: the parsed words array
//Pre-conditions: the user input must have been identified to be a background
//process
//Post-conditions: none
//////////////////////////////////////////////////////////////////////////////
char** handle_BRedirection(char** parsedWords){
	int devNull = open("/dev/null/", 0);
	int redirectionIndex = get_RedirectionIndex(parsedWords);
	//redirect into /dev/null if valid file is not provided for both redirections
	if(strcmp(parsedWords[get_RedirectionIndex(parsedWords)], ">") == 0){
		if(strcmp(parsedWords[redirectionIndex + 1], "NULL") == 0){
			dup2(devNull, 1);
		}
		return trunc_ParsedWords(parsedWords, redirectionIndex);
	}
	else if(strcmp(parsedWords[get_RedirectionIndex(parsedWords)], "<") == 0){
		if(strcmp(parsedWords[redirectionIndex + 1], "NULL") == 0){
			dup2(devNull, 0);
		}
		return trunc_ParsedWords(parsedWords, redirectionIndex);
	}
	else{
		//if no redirection then get rid of the ampersand on the first time around
		if(check_ifBackgroundP(parsedWords) == true){
			redirectionIndex = find_lastString(parsedWords);
			return trunc_ParsedWords(parsedWords, redirectionIndex);
		}
		else{//second check through, just return the parsed array as is
			return parsedWords;
		}
	}
	
}

//////////////////////////////////////////////////////////////////////////////
//Function Name: handle_Redirection
//Description: this function will take care of redirection for processes that 
//are done in the foreground
//Parameters: parsed words array
//Pre-conditions: must not be a background process
//Post-conditions: n/a
//////////////////////////////////////////////////////////////////////////////
char** handle_Redirection(char** parsedWords){
	//variable declaration and initialization
	int fileDescOut, fileDescIn = 5;
	int redirectionIndex = get_RedirectionIndex(parsedWords);
	
	//check whether the index has a input or output redirection symbol and handle accordingly
	if(strcmp(parsedWords[get_RedirectionIndex(parsedWords)], ">") == 0){
		
		fileDescOut = open(parsedWords[redirectionIndex+1], O_CREAT | O_WRONLY | O_TRUNC, 0770);
		//if the file descriptor from open return a value less than 0 then the file was not valid
		if(fileDescOut < 0){
			//print error message
			printf("%s: no such file or directory\n", parsedWords[redirectionIndex+1]);
			fflush(stdout);
			//set boolean to false indicating that the redirection was unsuccessful
			redirectionGood = false;
			exit(1);
		}
		//have the file descriptor from open point to the same as stdout
		dup2(fileDescOut, 1);
		return trunc_ParsedWords(parsedWords, redirectionIndex);
	}

	else if(strcmp(parsedWords[get_RedirectionIndex(parsedWords)], "<") == 0){
		//if the file descriptor from open return a value less than 0 then the file was not valid
		fileDescIn = open(parsedWords[redirectionIndex+1], O_RDONLY);
		if(fileDescIn < 0){
			//print error message
			printf("cannot open %s for input\n", parsedWords[redirectionIndex +1]);
			fflush(stdout);
			//set boolean to false indicating that the redirection was unsuccessful
			redirectionGood = false;
			exit(1);
		}
		//have the file descriptor from open point to the same as stdout
		dup2(fileDescIn, 0);
		return trunc_ParsedWords(parsedWords, redirectionIndex);
	}
	else{
		//there is no redirection, return the array as is
		return parsedWords;
	}
}

//////////////////////////////////////////////////////////////////////////////
//Function Name: trunc_ParsedWords
//Description: this function will "truncate" the parsed words array from the spec
//ifed index to the end of the array by turning those indices to NULL
//Parameters: the parsed words array, a index to begin the truncating from
//Pre-conditions: a index must be recieved from the get_RedirectionIndex function
//Post-conditions: a new variable to hold the returned parsed words array must be 
//declared
//////////////////////////////////////////////////////////////////////////////
char** trunc_ParsedWords(char** parsedWords, int Index){
	//variable declaration
	int end, i = 0;
	//get the index of the last string
	end = find_lastString(parsedWords);
	for(i = Index; i <= end; i++){
		parsedWords[i] = NULL;
	}
	//return the "truncated" array 
	return parsedWords;
}

//////////////////////////////////////////////////////////////////////////////
//Function Name: do_Redirection
//Description: this ois one funtion that will handle all redirection calling
//either the background or foreground process redirection functions
//Parameters: parsed words array, and the pid of the child process
//Pre-conditions: a child process must have been started and pid of that 
//child process must be stored in variable, whether a process is background
//or not must have been identified
//Post-conditions: n/a
//////////////////////////////////////////////////////////////////////////////
char** do_Redirection(char ** parsedWords, int childPID){
	//variable declaration
	int i, j, end = 0;
	char** newParsedWords;	
	//create a SIGINT handler
	struct sigaction SIGINT_action = {{0}};
	//if a foreground process then allow SIGINT to work in default mode
	if(((check_ifBackgroundP(parsedWords)) == false) || bPermitted == 0){	
		if(check_ifBackgroundP(parsedWords) == true){
			end = find_lastString(parsedWords);
			parsedWords[end] = NULL;
		}
		SIGINT_action.sa_handler = SIG_DFL;
		sigemptyset(&SIGINT_action.sa_mask);
		sigaddset(&SIGINT_action.sa_mask, SIGINT);
		SIGINT_action.sa_flags = 0;
		sigaction(SIGINT, &SIGINT_action, NULL);
		//make sure the redirection index is not the last string
		if(get_RedirectionIndex(parsedWords) != find_lastString(parsedWords)){
			//iterate through the array twice (there could be two redirections to be handled)
			for(i = 1; i < 3; i++){
				newParsedWords = handle_Redirection(parsedWords);
			}
		}	
		else{
			newParsedWords = trunc_ParsedWords(parsedWords, find_lastString(parsedWords));
		}
	}
	//call redirction for background process
	else if(((check_ifBackgroundP(parsedWords)) == true) && bPermitted == 1){
		backP[backPCount] = childPID;
		backPCount++;
		//print out the background process id of child
		printf("background pid is %d\n:", childPID);
		fflush(stdout);
		//check for redirection twice
		if(get_RedirectionIndex(parsedWords) != find_lastString(parsedWords)){
			for(j = 1; j < 3; j++){
				newParsedWords = handle_BRedirection(parsedWords);
			}
		}
	}
	//return the array which could be truncated
	return newParsedWords;
}

//////////////////////////////////////////////////////////////////////////////
//Function Name: fork_aChild
//Description: this function will take of all commands and redirections that are not 
//built in function
//Parameters: parsed words array
//Pre-conditions: it must be determined that the user input is not a comment, or 
//a built in command
//Post-conditions: n/a
//////////////////////////////////////////////////////////////////////////////
void fork_aChild(char** parsedWords){
	//variable declaration
	int pid, childPID;
	char** newParsedWords;	
	int stat;
	//empty the sig set 
	sigemptyset(&sigtstpSet);
	//add the SIGTSTP to this set
	sigaddset(&sigtstpSet, SIGTSTP);
	//check if this is a bacground process and set the boolean to true
	if(check_ifBackgroundP(parsedWords) == true){
		isBackground = true;
	}
	//using sigblock to block the SIGTSTP signal within child processes
	sigprocmask(SIG_BLOCK, &sigtstpSet, NULL);
	//fork process
	pid = fork();
	switch (pid){
		//if fork returned -1 then the forking failed, print error message
		case -1:
			printf("Error forking child\n");
			fflush(stdout);
			break;
		//child process
		case 0:
			//get child's pid
			childPID = getpid();
			//get the new array after redirection
			newParsedWords = do_Redirection(parsedWords, childPID);
			//if the redirection was good then continue on to execvp
			if(redirectionGood == true){
				//if execvp fails print error message
				if(execvp(newParsedWords[0],newParsedWords) == -1){
					printf("%s: no such file or directory\n",newParsedWords[0]);
					fflush(stdout);
					exit(1);
				}
			}
			else{
				//redirection was bad so child kills itself instead of going to exec
				raise(SIGTERM);
			}
			break;
		default:
			//unblock the SIGTSTP signal
			sigprocmask(SIG_UNBLOCK, &sigtstpSet, NULL);
			//if foreground process and background processes are not permitted then print exit status
			if((isBackground == false) || bPermitted == 0){
				waitpid(pid, &stat, 0);
				if(WIFEXITED(stat) == 1){
					isSignal = false;
					sigOrstatNum = WEXITSTATUS(stat);
				}
				else{
					isSignal = true;
					sigOrstatNum = WTERMSIG(stat);
					printf("terminated by signal %d\n", sigOrstatNum);
					
				}	
			}
			//otherwise it is a background process
			else{
				//add the child pid to the background process array
				backP[backPCount] = pid;
				//increment the background child process count by 1
				backPCount++;
			}
			break;
	}
	
}

//////////////////////////////////////////////////////////////////////////////
//Function Name: check_ifBuiltIn
//Description: this function will check the user input and check whether the 
//input is a comment, a built in function, or a non-built in command
//Parameters: parsedWords array
//Pre-conditions: array must be filled with user input
//Post-conditions: n/a
//////////////////////////////////////////////////////////////////////////////
int check_ifBuiltIn(char** parsedWords){
	//return -1 if user input should be ignored
	if(parsedWords[0] == NULL){
		return -1;
	}
	else if(parsedWords[0][0] == '#'){
		return -1;
	}
	else if(strcmp(parsedWords[0], "exit") == 0){
		return 1;
	}
	else if(strcmp(parsedWords[0], "cd") == 0){
		return 2;
	}
	else if(strcmp(parsedWords[0], "status") == 0){
		return 3;
	}
	//this is a non-built in command
	else{
		return 0;
	}
}

//////////////////////////////////////////////////////////////////////////////
//Function Name: call_builtIn
//Description: this function will call the built in fucntion specified by the 
//integer that was recieved from the check_ifBuiltIn function
//Parameters: a integer
//Pre-conditions: a funcNum value must be set, and parsed words array must be
//filled
//Post-conditions: n/a
//////////////////////////////////////////////////////////////////////////////
void call_builtIn(int funcNum, char** parsedWords){
	//depending on what funcNum is call the corresponding function
	if(funcNum == 1){
		smallsh_exit();
	}
	else if(funcNum == 2){
		change_directory(parsedWords);
	}
	else if(funcNum == 3){
		smallsh_status(&theStatus);
	}
}

//////////////////////////////////////////////////////////////////////////////
//Function Name: check_ifBackgroundP
//Description: this function will look at the last string in the parsed words 
//array and will check to see if it is an ampersand 
//Parameters: a char pointer to a parsed words array
//Pre-conditions: parsed words array is declared and initialized
//Post-conditions: n/a
//////////////////////////////////////////////////////////////////////////////
bool check_ifBackgroundP(char** parsedWords){
	//get the index of the last string
	int lastString = find_lastString(parsedWords);
	//check if the last string is the ampersand
	if(strcmp(parsedWords[lastString], "&") == 0){
		return true;
	}
	else{
		return false;
	}	
}


//////////////////////////////////////////////////////////////////////////////
//Function Name: find_lastString
//Description: this function will return the index of the last string in the 
//array
//Parameters: pointer to the char parsed words array
//Pre-conditions: parsed words array must be memset with values of 0
//Post-conditions: n/a
//////////////////////////////////////////////////////////////////////////////
int find_lastString(char **parsedWords){
	int i = 0;
	//the parsed words array is memset to 0, so when 0 is hit then this is the
	//end
	while (parsedWords[i] != 0){
		i++;
	}
	return i-1;
}

//////////////////////////////////////////////////////////////////////////////
//Function Name: main function
//////////////////////////////////////////////////////////////////////////////
int main(){
	//setting up signal handlers for SIGINT and SIGTSTP
	struct sigaction SIGINT_action = {{0}}, SIGTSTP_action = {{0}};

	SIGINT_action.sa_handler = SIG_IGN;
	sigemptyset(&SIGINT_action.sa_mask);
	sigaddset(&SIGINT_action.sa_mask, SIGINT);
	SIGINT_action.sa_flags = SA_RESTART;

	sigaction(SIGINT, &SIGINT_action, NULL);

	SIGTSTP_action.sa_handler = SIGTSTPcatch;
	sigfillset(&SIGTSTP_action.sa_mask);
	SIGTSTP_action.sa_flags = SA_RESTART; 

	sigaction(SIGTSTP, &SIGTSTP_action, NULL);

	//variable declaration
	char* userInput = NULL;
	char* parsedWords[513];
	int funcNum = -100;
	bool exitCalled = false;

	//run the program until the user exits with exit command
	while(exitCalled == false){
		//clear out parsed words array 
		memset(parsedWords, 0, sizeof(parsedWords));
		//reset isBackground variable to false
		isBackground = false;	
		//check for any background children that need to be cleaned up before prompt
		check_children();
		//prompt the user
		prompt();
		//before storing the user input, free the memory allocated if it is filled
		if(userInput !=NULL){
			free(userInput);
			userInput = NULL;
		}
		//get user input
		get_userInput(&userInput);
		//parse the user's input
		parse_input(userInput, parsedWords);
		//check what the user input and return a value corresponding to what needs to be done
		funcNum = check_ifBuiltIn(parsedWords);
		//the user called a built in command 
		if(funcNum > 0){
			//call the built in command
			call_builtIn(funcNum, parsedWords);
			//if the funcNum is 1 then user entered exit
			if(funcNum == 1){
				//set boolean to true, which will cause loop to end
				exitCalled = true;
			}
		}
		//user did not enter a built in command or comment
		else if(funcNum == 0){
			//fork a process
			fork_aChild(parsedWords);
		}
		//set redirection boolean to true
		redirectionGood = true;
		//check background processes and take care of zombie/killed children
		check_children();
	}	

	return 0;
}


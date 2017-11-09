#include <sys/types.h>
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <fcntl.h>


#define INPUT_LENGTH 2048
#define MAX_ARGS  512
#define MAX_PIDS 1000
typedef enum {false, true} bool;

char enteredLine[INPUT_LENGTH];
char finalLine[10000];
char* arguments[MAX_ARGS];
bool outputRedirect;
bool inputRedirect;
pid_t spawnpid = -5;
int statusCode = -5;
bool background;
int execCode;
int argc;
pid_t backgroundProcesses[MAX_PIDS];
int inputFile;
int outputFile;


/************************************************************************
*  Retrieves input from the user and trims off the newline character
************************************************************************/
void getInput()
{
    if(fgets(enteredLine, sizeof(enteredLine), stdin) != NULL)
       {
            char *newLine = strchr(enteredLine, '\n');
            *newLine = '\0';
       }
}

/************************************************************************
*  Sets the entire array of background pids to an empty value (-1)
*  to accomodate checking if a process is held in that element or not
************************************************************************/
void initializeBackgroundArray()
{
    int i;
    for (i = 0; i < MAX_PIDS; i++)
    {
        backgroundProcesses[i] = -1;
    }
}

/************************************************************************
*  Clears the arguments array to make sure that nothing is being kept
*  in between interations of the shell loops
************************************************************************/
void clearArguments()
{
    int i;
    for (i = 0; i < MAX_ARGS; i++)
    {
        arguments[i] = NULL;
    }
    argc = 0;
}

/*************************************************************************
*  This function executes at the beginning of each shell loop.
*  It iterates through the array of background processes and waits with
*  a WNOHANG flag to see if the process has finished or not.
*  It outputs the exit value or terminating signal value for each process
*  that has completed.
*************************************************************************/
void reapZombies()
{
    int i;
    int backgroundExitStatus;
    int result;
    for(i = 0; i < MAX_PIDS; i++)
    {
        if (backgroundProcesses[i] != -1)
        {
            result = waitpid(backgroundProcesses[i], &backgroundExitStatus, WNOHANG);
            if (result == backgroundProcesses[i])  //this line means that the process has ended
            {
                if (WIFEXITED(backgroundExitStatus)) //this checks if the process was ended by exiting
                {
                    printf("Background pid %d is done: exit value %d\n", backgroundProcesses[i], WEXITSTATUS(backgroundExitStatus));
                    fflush(stdout);
                    backgroundProcesses[i] = -1;
                }
                else if (WIFSIGNALED(backgroundExitStatus)) //this checks if the process was ended by a signal
                {
                    printf("Background pid %d is done: terminated by signal %d\n", backgroundProcesses[i], WTERMSIG(backgroundExitStatus));
                    backgroundProcesses[i] = -1;
                }
            }
        }
    }
}

/****************************************************************************
*  This function takes the original text typed in by the user and tokenizes
*  it by white space into an array of individual arguments
****************************************************************************/
void tokenizeInput()
{
    clearArguments();
    char* token;
    token = strtok(finalLine, " ");
    arguments[0] = token;
    
    while(token != NULL)
    {
        token = strtok(NULL, " ");
        arguments[++argc] = token;
    }
    
    if (strcmp(arguments[argc-1], "&") == 0)
    {
        background = true;
        arguments[argc-1] = NULL;
        argc--;
    }
}

/************************************************************************
*  This function serves as the bulit-in directory navigation.  If the
*  user enters a string of text that begins with the command "cd", this
*  function executes.  
************************************************************************/
void changeDirectory()
{
    int result;
    if (argc > 2)
    {
        printf("Too many arguments.\n");
    }
    else if (argc == 1)
    {
        result = chdir(getenv("HOME"));
    }
    else
    {
        result = chdir(arguments[1]);
    }
    if (result == -1)
    {
        printf("Error: unable to find directory\n");
    }
}

/************************************************************************
*  This function simply looks for the first null value in the background
*  pid array and stores the latest background child pid in it.
************************************************************************/
void addPidToArray(int pid)
{
    int i;
    for (i = 0; i < MAX_PIDS; i++)
    {
        if (backgroundProcesses[i] == -1)
        {
            backgroundProcesses[i] = pid;
            break;
        }
    }
}

/************************************************************************
*  This function handles all redirection needs.  It redirects input and
*  output as needed if the calling process is a foreground process. If
*  it is a background process and no redirection is specified, both
*  stdin and stdout redirect to /dev/null.
************************************************************************/
void performRedirection()
{
    char outputFileName[255];
    char inputFileName[255];
    while (argc > 2)
    {
            /*this block of statements looks to see if the second to last argument is a redirection.
            If it is, the last argument must be a file name or location to redirect from/to.*/        
            if (strcmp(arguments[argc-2], ">") == 0)
            {
                strcpy(outputFileName, arguments[argc-1]);
                arguments[argc-1] = NULL;
                arguments[argc-2] = NULL;
                argc = argc - 2;
                outputRedirect = true;
            }
            else if (strcmp(arguments[argc-2], "<") == 0)
            {
                strcpy(inputFileName, arguments[argc-1]);
                arguments[argc-1] = NULL;
                arguments[argc-2] = NULL;
                argc = argc - 2;
                inputRedirect = true;
            }
        }
    
        if (background == false)
        {
            if (outputRedirect == true)
            {
                outputFile = open(outputFileName, O_WRONLY | O_TRUNC | O_CREAT, 0700);
                
                if (outputFile < 0)
                {
                    printf("Output file could not be created.\n"); fflush(stdout);
                    exit(1);
                }
                else
                {
                    dup2(outputFile, 1);
                }
            }
            if (inputRedirect == true)
            {
                inputFile = open(inputFileName, O_RDONLY, 0);
                
                if(inputFile < 0)
                {
                    printf("Input file could not be found.\n"); fflush(stdout);
                    exit(1);
                }
                else
                {
                    dup2(inputFile, 0);
                }
            }
        }
        else 
        {
            if (outputRedirect == false)
            {
                outputFile = open("/dev/null", O_RDONLY, 0);
            }
            else
            {
                outputFile = open(outputFileName, O_WRONLY | O_TRUNC | O_CREAT, 0700);
                if (outputFile < 0)
                {
                    printf("Output file could not be created.\n"); fflush(stdout);
                    exit(1);
                }
                else
                {
                    dup2(outputFile, 1);
                }
            }
            
            if (inputRedirect == false)
            {
                inputFile = open("/dev/null", O_WRONLY, 0700);
            }
            else
            {
                inputFile = open(inputFileName, O_RDONLY, 0);
                
                if(inputFile < 0)
                {
                    printf("Input file could not be found.\n"); fflush(stdout);
                    exit(1);
                }
                else
                {
                    dup2(inputFile, 0);
                }
            }
        }
}

/************************************************************************
*  This function replaces all instances of "$$" in the user's input
*  with the shell process id
************************************************************************/
void expandPids()
{
   char final[10000];
    char firstHalf[5000];
    char secondHalf[5000];
    char *ptr = enteredLine;
    char *ptr2;
    
    ptr2 = strstr(enteredLine, "$$");   //points to address of first instance of "$$", or to NULL if one doesn't exist
    while (ptr2 != NULL)                //while there is still an instance of "$$" remaining, execute loop
    {
        int chars = ptr2-ptr;           //pointer math.  Returns the number of characters in the string that leads up to the first instance of "$$"
        strncpy(firstHalf, ptr, chars); //store this string to a variable
        ptr = ptr + chars + 2;          //increment the ptr by the number of chars in the initial string + the number of chars in the "$$" string
        strcpy(secondHalf, ptr);        //copy the remaining characters after the '$$" to a different variable'
        sprintf(final, "%s%d%s", firstHalf, getpid(), secondHalf); //print a string that consists of: firstHalf + pid + secondHalf
        printf("%s\n", final);
        ptr = final;                    //ptr set to the expanded string to prep for further while loop iterations
        ptr2 = strstr(final, "$$");     //ptr2 set to the next instance of "$$" for further while loop iteration.
    }
        strcpy(finalLine, final);       //copies the final result to the global variable "finalLine" for use in the main function
}


void main()
{
    bool timeToExit = false;
    outputRedirect = false;
    inputRedirect = false;
    int i;
    initializeBackgroundArray();
    
    while(!timeToExit)                                  //loops until the exit command is called
    {
        reapZombies();
        background = false;                             //resets the background process value to false
        printf(": ");                                   //Prints command prompt
        fflush(stdout);
        getInput();                                     //Gets user input
        
        if (strstr(enteredLine, "$$") == NULL)          //If there is no "$$" to expand, simply continue on
        {
            strcpy(finalLine, enteredLine);
        }
        else                                            //Otherwise, expand pids
        {
            expandPids();
        }
        
        if (strcmp(finalLine, "status") == 0)           //Checks for built-in status command
        {
            printf("exit value %d\n", statusCode);
        }
        else if (strncmp(finalLine, "#", 1) == 0 || strcmp(finalLine, "") == 0)         //Checks for a blank or comment line
        {
            continue;
        }
        else if (strcmp(finalLine, "exit") == 0)                                        //Checks for exit command
        {
            exit(0);
        }
        else
        {
            tokenizeInput();                                                            //If not a blank/comment, exit, or status command, tokenize input for cd/exec
            if(strcmp(arguments[0], "cd") == 0)                                         
            {
                changeDirectory();
                clearArguments();
                continue;
            }
            else
            {
                spawnpid = fork();                                                      //Fork off a child process to run the exec command
                
                if (spawnpid == -1)                                                     //if spawnpid == -1, an error occurred
                {
                    perror("Forking failed!\n");
                    exit(1);
                }
                else if (spawnpid == 0)                                                 //if spawnpid == 0, we are in the child process.  Perform redirection and execute.
                {
                    performRedirection();
                    execCode = execvp(arguments[0], arguments);
                    if (execCode == -1)                                                 //if exec returns -1, we have a problem.
                    {
                        printf("Command could not be executed.\n");
                        statusCode = 1;
                    }
                }
                else                                                                    //if spawnpid != 0 or -1, we are in the parent process (the shell)
                {
                    if (background == false)                                            //if this is a foreground process   
                    {
                        waitpid(spawnpid, &statusCode, 0);                              //We wait for the process to complete
                    }
                    else                                                                //If it is a background process
                    {
                        printf("background pid is %d\n", spawnpid);                     //Output the process id and
                        addPidToArray(spawnpid);                                        //add it to the background process array
                    }
                }
            }
        }
    }
}
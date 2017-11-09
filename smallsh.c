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

void getInput()
{
    if(fgets(enteredLine, sizeof(enteredLine), stdin) != NULL)
       {
            char *newLine = strchr(enteredLine, '\n');
            *newLine = '\0';
       }
}

void initializeBackgroundArray()
{
    int i;
    for (i = 0; i < MAX_PIDS; i++)
    {
        backgroundProcesses[i] = -1;
    }
}

void clearArguments()
{
    int i;
    for (i = 0; i < MAX_ARGS; i++)
    {
        arguments[i] = NULL;
    }
    argc = 0;
}

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
            if (result == backgroundProcesses[i])
            {
                if (WIFEXITED(backgroundExitStatus))
                {
                    printf("Background pid %d is done: exit value %d\n", backgroundProcesses[i], WEXITSTATUS(backgroundExitStatus));
                    fflush(stdout);
                    backgroundProcesses[i] = -1;
                }
                else if (WIFSIGNALED(backgroundExitStatus))
                {
                    printf("Background pid %d is done: terminated by signal %d\n", backgroundProcesses[i], WTERMSIG(backgroundExitStatus));
                    backgroundProcesses[i] = -1;
                }
            }
        }
    }
}

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

void performRedirection()
{
    char outputFileName[255];
    char inputFileName[255];
    while (argc > 2)
    {
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

void expandPids()
{
   char final[10000];
    char firstHalf[5000];
    char secondHalf[5000];
    char *ptr = enteredLine;
    char *ptr2;
    
    ptr2 = strstr(enteredLine, "$$");
    while (ptr2 != NULL)
    {
        int chars = ptr2-ptr;
        strncpy(firstHalf, ptr, chars);
        ptr = ptr + chars + 2;
        strcpy(secondHalf, ptr);
        sprintf(final, "%s%d%s", firstHalf, getpid(), secondHalf);
        printf("%s\n", final);
        ptr = final;
        ptr2 = strstr(final, "$$");
    }
        strcpy(finalLine, final);
}


void main()
{
    bool timeToExit = false;
    outputRedirect = false;
    inputRedirect = false;
    int i;
    initializeBackgroundArray();
    
    while(!timeToExit)
    {
        reapZombies();
        background = false;
        printf(": ");
        fflush(stdout);
        getInput();
        
        if (strstr(enteredLine, "$$") == NULL)
        {
            strcpy(finalLine, enteredLine);
        }
        else
        {
            expandPids();
        }
        
        if (strcmp(finalLine, "status") == 0)
        {
            printf("exit value %d\n", statusCode);
        }
        else if (strncmp(finalLine, "#", 1) == 0 || strcmp(finalLine, "") == 0)
        {
            continue;
        }
        else if (strcmp(finalLine, "exit") == 0)
        {
            exit(0);
        }
        else
        {
            tokenizeInput();
            if(strcmp(arguments[0], "cd") == 0)
            {
                changeDirectory();
                clearArguments();
                continue;
            }
            else
            {
                spawnpid = fork();
                
                if (spawnpid == -1)
                {
                    perror("Forking failed!\n");
                    exit(1);
                }
                else if (spawnpid == 0)
                {
                    performRedirection();
                    execCode = execvp(arguments[0], arguments);
                    if (execCode == -1)
                    {
                        printf("Command could not be executed.\n");
                        statusCode = 1;
                    }
                }
                else
                {
                    if (background == false)
                    {
                        waitpid(spawnpid, &statusCode, 0);
                    }
                    else
                    {
                        printf("background pid is %d\n", spawnpid);
                        addPidToArray(spawnpid);
                    }
                }
            }
        }
    }
}
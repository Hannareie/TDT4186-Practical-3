#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <readline/readline.h>
#include <readline/history.h>
  
#define MAXCOM 1000 
#define MAXLIST 100
  
// Clearing the shell using escape sequences
#define clear() {
    printf("\033[H\033[J")
}
  
void init_shell()
{
    clear();
    printf("****Welcime to the Shell****");

    char* username = getenv("USER");
    printf("\n\nUser is: @%s", username);
    printf("\n");
    sleep(1);
    clear();
}
  
// Function to take input
int userInput(char* str) {
    char* buf;

    buf = readline(": ");

    if (strlen(buf) != 0) {
        add_history(buf);
        strcpy(str, buf);
        return 0;
    } else {
        return 1;
    }
}
  
// Function to print Current Directory.
void printDirectory() {
    char cwd[1024];
    getcwd(cwd, sizeof(cwd));
    printf("\n%s", cwd);
}
  
// Function where the system command is executed
void executeArgs(char** parsed) {
    // Forking a child
    pid_t pid = fork(); 
    if (pid == -1) {
        printf("\nFork failed..");
        return;
    } else if (pid == 0) {
        if (execvp(parsed[0], parsed) < 0) {
            printf("\nCould not execute command..");
        }
        exit(0);
    } else {
        // waiting for child to terminate
        wait(NULL); 
        return;
    }
}
  
// Function where the piped system commands is executed
void executePipedArgs(char** parsed, char** parsedpipe) {
    // 0 is read end, 1 is write end
    int pipefd[2]; 
    pid_t p1, p2;
  
    if (pipe(pipefd) < 0) {
        printf("\nPipe could not be initialized");
        return;
    }
    p1 = fork();
    if (p1 < 0) {
        printf("\nFork failed..");
        return;
    }
  
    if (p1 == 0) {
        // Child 1 executing..
        // It only needs to write at the write end
        close(pipefd[0]);
        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[1]);
  
        if (execvp(parsed[0], parsed) < 0) {
            printf("\nCould not execute command 1..");
            exit(0);
        }
    } else {
        p2 = fork();
        if (p2 < 0) {
            printf("\nFork failed..");
            return;
        }
  
        if (p2 == 0) {
            close(pipefd[1]);
            dup2(pipefd[0], STDIN_FILENO);
            close(pipefd[0]);
            if (execvp(parsedpipe[0], parsedpipe) < 0) {
                printf("\nCould not execute command 2..");
                exit(0);
            }
        } else {
            wait(NULL);
            wait(NULL);
        }
    }
}
  
// Function to execute builtin commands
int cmdHandler(char** parsed) {
    int numCmds = 4, i, switchArgs = 0;
    char* cmdsList[numCmds];
    char* username;
  
    cmdsList[0] = "exit";
    cmdsList[1] = "cd";
  
    for (i = 0; i < numCmds; i++) {
        if (strcmp(parsed[0], cmdsList[i]) == 0) {
            switchArgs = i + 1;
            break;
        }
    }
  
    switch (switchArgs) {
    case 1:
        printf("\nGoodbye\n");
        exit(0);
    case 2:
        chdir(parsed[1]);
        return 1;
    default:
        break;
    }
    return 0;
}
  
// function for finding pipe
int parsePipe(char* str, char** strpiped) {
    int i;
    for (i = 0; i < 2; i++) {
        strpiped[i] = strsep(&str, "|");
        if (strpiped[i] == NULL)
            break;
    }
    // returns zero if no pipe is found.
    if (strpiped[1] == NULL) {
        return 0;
    }
    else {
        return 1;
    }
}
  
// function for parsing command words
void parseSpace(char* str, char** parsed) {
    int i;
    for (i = 0; i < MAXLIST; i++) {
        parsed[i] = strsep(&str, " ");
  
        if (parsed[i] == NULL)
            break;
        if (strlen(parsed[i]) == 0)
            i--;
    }
}
  
int processString(char* str, char** parsed, char** parsedpipe) {
    char* strpiped[2];
    int piped = 0;
  
    piped = parsePipe(str, strpiped);
  
    if (piped) {
        parseSpace(strpiped[0], parsed);
        parseSpace(strpiped[1], parsedpipe);
  
    } else {
        parseSpace(str, parsed);
    }
    if (cmdHandler(parsed)) {
        return 0;
    } else {
        return 1 + piped;
    }
}
  
int main() {
    char inputString[MAXCOM], *parsedArgs[MAXLIST];
    char* parsedArgsPiped[MAXLIST];
    int execFlag = 0;
    init_shell();
  
    while (1) {
        printDirectory();
        if (userInput(inputString)) {
            continue;
        }

        execFlag = processString(inputString, parsedArgs, parsedArgsPiped);
        if (execFlag == 1)
            executeArgs(parsedArgs);
  
        if (execFlag == 2)
            executePipedArgs(parsedArgs, parsedArgsPiped);
    }
    return 0;
}
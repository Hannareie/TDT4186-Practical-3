#include <time.h>
#include <stdio.h>
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>

#define MAXLEN 200

/* Struvt for the linked list process */
struct linkedProcess {
    int pid;
    char name[MAXLEN];
    struct linkedProcess *previous;
    struct linkedProcess *next;
};

/* Global variables for linked list of background tasks */
struct linkedProcess *head = NULL;
struct linkedProcess *tail = NULL;

/* Initializing the flush to the user */
void init_shell() {
    printf("\033[H\033[J");
    printf("****Welcome to flush****");

    char* username = getenv("USER");
    printf("\n\nUser is: @%s", username);
    printf("\n");
    sleep(1);
    printf("\033[H\033[J");
}

/* Method to check the status og an exited task */
void checkStatus(int status, char *input) {
    input[strcspn(input, "\n")] = 0;

    if (WIFEXITED(status)) {
        int exitStatus = WEXITSTATUS(status);
        printf("Exit status [%s] = %d\n", input, exitStatus);
    }
}

/* Method to add a node to the linked list*/
void addProcess(int pid, char *name) {
    struct linkedProcess *newProcess = (struct linkedProcess*) malloc(sizeof(struct linkedProcess));

    /* Updates the values */
    newProcess->pid = pid;
    strcpy(newProcess->name, name);

    /* Updates the previous pointer */
    newProcess->previous = tail;
    if (tail != NULL) {
        tail->next = newProcess;
    }
    tail = newProcess;

    /* Updates the next pointer */
    newProcess->next = NULL;
    if (head == NULL) {
        head = newProcess;
    }
}

/* Prints all nodes in the linked list. The method is called as the promt "jobs" */
void printAllProcesses() {
    struct linkedProcess *process = head;
    while (process != NULL) {
        printf("[pid %d] %s\n", process->pid, process->name);
        process = process->next;
    } 
}

/* Method to remove a given process from the linked list */
void removeProcess(struct linkedProcess *process) {
    /* Checks if the linked process is the first process */
    if (process->previous != NULL) {
        process->previous->next = process->next;
    }
    else {
        head = process->next;
    }

    /* Checks if the linked process is the last node */
    if (process->next != NULL) {
        process->next->previous = process->previous;
    }
    else {
        tail = process->previous;
    }

    free(process);
}

/* Method to check for complete processes. If a process is teminated it gets removed */
void checkForCompleteProcesses() {
    struct linkedProcess *process = head;
    while (process != NULL) {
        int status;

        /* Checks if processes are complete */
        if (waitpid(process->pid, &status, WNOHANG)) {
            checkStatus(status, process->name);
            removeProcess(process);
        }
        process = process->next;
    }
}

/* Method to parse a string into args */
void parseString(char *str, char **args) {
    char delim[4] = "\t \n";

    for (int i = 0; i < MAXLEN; i++) {
        args[i] = strsep(&str, delim);
        if (args[i] == NULL)
            break;
        if (strlen(args[i]) == 0)
            i--;
    }
}

/* Method to check whether a task is to be executed as a background task */
int checkIfBackgroundTask(char input[MAXLEN]) {
    /* Removes newline from input string */
    input[strcspn(input, "\n")] = 0;
    int length = strlen(input) - 1;

    /* Checks for '&' and removes it from string */
    int check = input[length] == '&';
    if ((length > 0) && (input[length] == '&')) {
        input[length] = '\0';
        length--;
    }
    /* Removes trailing whitespaces after removing '&' */
    while (length > -1) {
        if (input[length] == ' ' || input[length] == '\t') {
            length--;
        }
        else {
            break;
        }
        input[length + 1] = '\0';
    }
    return check;
} 

/* Method to execute the command from the user */
void execute(char **args, char *input) {
    int background = checkIfBackgroundTask(input);

    /* Internal commands */
    if (strcmp(args[0], "help") == 0) {
        printf("flush: enter a Linux command, or 'exit' to quit\n");
        return;
    } 

    if (strcmp(args[0], "cd") == 0) {
        if (args[1] != NULL) {
            int cd = chdir(args[1]);
            if (cd != 0) {
                perror("flush: cd error\n");
            }  
        }
        else {
            printf("flush: no argument was given for cd\n");
        }
        return;
    }

    if (strcmp(args[0], "jobs") == 0) {
        printAllProcesses();
        return;
    }

    int status;
    pid_t pid = fork();

    if (pid < -1) {
        perror("flush: fork error\n");
        return;
    }

    if (pid == 0) {
        int index = 0, fd;

         /* I/O redirection. Parse args for '<' or '>' and filename */
        while (args[index]) {  
            if (*args[index] == '>' && args[index+1]) {
                if ((fd = open(args[index+1], 
                            O_WRONLY | O_CREAT, 
                            S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH)) == -1) {
                    perror(args[index+1]);
                    exit(1);
                }
                dup2(fd, 1);
                dup2(fd, 2);
                close(fd);
                /* Adjust the rest of the arguments in the array */
                while (args[index]) {
                    args[index] = args[index+2];
                    index++; 
                }
                break;
            }
            else if (*args[index] == '<' && args[index+1]) {
                if ((fd = open(args[index+1], O_RDONLY)) == -1) {
                    perror(args[index+1]);
                    exit(1);
                }
                dup2(fd, 0);
                close(fd);
                /* Adjust the rest of the arguments in the array */
                while (args[index]) {
                    args[index] = args[index+2];
                    index++; 
                }
                break;
            }
            index++;
        }

        /* Exectute the command */
        if (execvp(args[0], args) == -1) {
            perror("flush: execvp error\n");
        }
        exit(1);
    }
    /* Inside arent process. If the command is flagged as a background task, it is added to the linked list. */                           
    else {
        if (background) {
            addProcess(pid, input);
        }
        else {
            waitpid(pid, &status, 0);
            checkStatus(status, input);
        }
    }
}

/* Method that runs the shell in a loop. It requests an input from the user and calls the execute method. */
int main(void) {
    char cwd[MAXLEN];
    char input[MAXLEN];
    char inputString[MAXLEN];
    char *args[MAXLEN];

    init_shell();

    while (1) {        
        /* Checks for complete background processes */
        checkForCompleteProcesses();

        /* Gets the current working directory and asks for an input from the user */
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s: ", cwd);
        }
        else {
            perror("flush: cwd error\n");
        }

        if (!fgets(input, sizeof(input), stdin)) {
            if (feof(stdin)) {
                printf("flush: EOF signal is received\n");
                break;
            } else {
                perror("flush: fgets error\n");
                exit(1);
            }
        }

        strcpy(inputString, input); 

        /* Parses the input into arguments to be executed */
        parseString(input, args);
        for (int i = 0; i < MAXLEN; i++) {
            if (!args[i]) {
                break;
            }
        }

        if (strcmp(args[0], "quit") == 0 || strcmp(args[0], "exit") == 0) {
            break;
        }
        
        /* Execute the command */
        execute(args, inputString);
    }
    return 0;
}
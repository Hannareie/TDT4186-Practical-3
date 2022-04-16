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

#define MAXLEN 256

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

/* Initializing of shell */
void init_shell() {
    printf("\033[H\033[J");
    printf("****Welcime to the Shellpadde****");

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
        int es = WEXITSTATUS(status);
        printf("Exit status [%s] = %d\n", input, es);
    }
}

/* Method to add a node to the linked list*/
void addProcess(int pid, char *name) {
    struct linkedProcess *newProcess = (struct linkedProcess*) malloc(sizeof(struct linkedProcess));

    /* Update the values */
    newProcess->pid = pid;
    strcpy(newProcess->name, name);

    /* Update the previous pointer */
    newProcess->previous = tail;
    if (tail != NULL)
        tail->next = newProcess;
    tail = newProcess;

    /* Update the next pointer */
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
    /* checks if the linked process is the first process */
    if (process->previous != NULL) {
        process->previous->next = process->next;
    }
    else {
        head = process->next;
    }

    /* checks if the linked process is the last node */
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

        /* checks if processes are complete */
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
    /* removes newline from input string */
    input[strcspn(input, "\n")] = 0;
    int length = strlen(input) - 1;

    /* checks for '&' and removes it from string */
    int check = input[length] == '&';
    if ((length > 0) && (input[length] == '&')) {
        input[length] = '\0';
        length--;
    }
    /* removes trailing whitespaces after removing '&' */
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

    /* internal commands */
    if (strcmp(args[0], "help") == 0) {
        // help 
        printf("enter a Linux command, or 'exit' to quit\n");
        return;
    } 

    if (strcmp(args[0], "cd") == 0) {
        if (args[1]) {
            int cd = chdir(args[1]);
            switch(cd) {
                case ENOENT:
                    printf("msh: cd: '%s' does not exist\n", args[1]);
                    break;
                case ENOTDIR:
                    printf("msh: cd: '%s' not a directory\n", args[1]);
                    break;
                default:
                    printf("msh: cd: bad path\n");
            }   
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
        perror("fork error");
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
                    perror (args[index+1]);
                    exit (EXIT_FAILURE);
                }
                dup2 (fd, 1);
                dup2 (fd, 2);
                close (fd);
                /* Adjust the rest of the arguments in the array */
                while (args[index]) {
                    args[index] = args[index+2];
                    index++; 
                }
                break;
            }
            else if (*args[index] == '<' && args[index+1]) {
                if ((fd = open (args[index+1], O_RDONLY)) == -1) {
                    perror (args[index+1]);
                    exit (EXIT_FAILURE);
                }
                dup2 (fd, 0);
                close (fd);
                // Adjust the rest of the arguments in the array
                while (args[index]) {
                    args[index] = args[index+2];
                    index++; 
                }
                break;
            }
            index++;
        }
        /* Exectute the command */
        if (execvp (args[0], args) == -1) {
            perror ("execvp");
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
        /* checks background processes */
        checkForCompleteProcesses();

        /* Gets the current working directory and asks for an input from the user */
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("\n%s: ", cwd);
        }
        else {
            perror("Error while getting cwd\n");
        }

        if (fgets(input, sizeof(input), stdin) == 0) {
            if (feof(stdin)) {
                exit(0);
            } else {
                perror("basic shell: fgets error\n");
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
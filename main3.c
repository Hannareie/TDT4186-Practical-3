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

//Linked list struct
struct linkedProcess {
    int pid;
    char name[MAXLEN];
    struct linkedProcess *previous;
    struct linkedProcess *next;
};

// Global variables for linked list of background tasks
struct linkedProcess *head = NULL;
struct linkedProcess *tail = NULL;

//Initializing of shell
void init_shell() {
    printf("\033[H\033[J");
    printf("****Welcime to the Shellpadde****");

    char* username = getenv("USER");
    printf("\n\nUser is: @%s", username);
    printf("\n");
    sleep(1);
    printf("\033[H\033[J");
}

void checkStatus(int status, char *input) {
    input[strcspn(input, "\n")] = 0;

    if (WIFEXITED(status)) {
        int es = WEXITSTATUS(status);
        printf("Exit status [%s] = %d\n", input, es);
    }
}

//Adds a process to the linked list
void addProcess(int pid, char *name) {
    struct linkedProcess *newProcess = (struct linkedProcess*) malloc(sizeof(struct linkedProcess));

    /* update values */
    newProcess->pid = pid;
    strcpy(newProcess->name, name);

    /* update previous pointer */
    newProcess->previous = tail;
    if (tail != NULL)
        tail->next = newProcess;
    tail = newProcess;

    /* update next pointer */
    newProcess->next = NULL;
    if (head == NULL) {
        head = newProcess;
    }
}

//Prints all nodes in the linked list, is called by the prompt "jobs"
void printAllProcesses() {
    struct linkedProcess *process = head;
    while (process != NULL) {
        printf("[pid %d] %s\n", process->pid, process->name);
        process = process->next;
    } 
}

//Removes a given process from the linked list
void removeProcess(struct linkedProcess *process) {
    // checks if the linked process is the first process
    if (process->previous != NULL) {
        process->previous->next = process->next;
    }
    else {
        head = process->next;
    }

    /* checks if last node */
    if (process->next != NULL) {
        process->next->previous = process->previous;
    }
    else {
        tail = process->previous;
    }
    free(process);
}

//Checks for complete processes 
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

        while (args[index]) {   /* parse args for '<' or '>' and filename */
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
                // Adjust the rest of the arguments in the array
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
        if (execvp (args[0], args) == -1) {
            perror ("execvp");
        }
        _exit(1);
    }                           
    else {
        /* parent process */
        if (background) {
            addProcess(pid, input);
        }
        else {
            waitpid(pid, &status, 0);
            checkStatus(status, input);
        }
    }
}

int main(void) {
    char cwd[MAXLEN];
    char input[MAXLEN];
    char inputString[MAXLEN];
    char *args[MAXLEN];

    init_shell();

    while (1) {        
        /* checks background processes */
        checkForCompleteProcesses();

        /* gets current working directory and asks for input */
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

        strcpy(inputString, input); // Make a copy of input string

        // parses input into arguments
        parseString(input, args);
        for (int i = 0; i < MAXLEN; i++) {
            if (!args[i]) {
                break;
            }
        }

        if (strcmp(args[0], "quit") == 0 || strcmp(args[0], "exit") == 0) {
            break;
        }
        // execute the command 
        execute(args, inputString);
    }

    return 0;
}
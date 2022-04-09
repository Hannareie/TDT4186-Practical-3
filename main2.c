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

#define MAXLEN 256

/**
 * @brief Linked list where the background tasks is stored
 */
struct linkedProcess
{
    int pid;
    char name[MAXLEN];
    struct linkedProcess *previous;
    struct linkedProcess *next;
};

/* Global variables for linked list of background tasks */
struct linkedProcess *head = NULL;
struct linkedProcess *tail = NULL;

void init_shell()
{
    printf("\033[H\033[J");
    printf("****Welcime to the Shellpadde****");

    char* username = getenv("USER");
    printf("\n\nUser is: @%s", username);
    printf("\n");
    sleep(1);
    printf("\033[H\033[J");
}

void checkStatus(int status, char *input)
{
    input[strcspn(input, "\n")] = 0;

    if (WIFEXITED(status))
    {
        int es = WEXITSTATUS(status);
        printf("Exit status [%s] = %d\n", input, es);
    }
}


void addProcess(int pid, char *name)
{
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
    if (head == NULL)
    {
        head = newProcess;
    }
}

/**
 * @brief Prints all nodes in the linked list,
 * is called by the prompt "jobs"
 */
void printAllProcesses()
{
    struct linkedProcess *process = head;
    while (process != NULL)
    {
        printf("[pid %d] %s\n", process->pid, process->name);
        process = process->next;
    }
}

/**
 *@brief Removes given node from linked list and rebinds the
 * previous and next node together.
 * If next or previous is NULL, head/tail is updated.
 * Frees the node
 */

void removeProcess(struct linkedProcess *process)
{
    // checks if the linked process is the first process
    if (process->previous != NULL)
    {
        process->previous->next = process->next;
    }
    else
    {
        head = process->next;
    }

    /* checks if last node */
    if (process->next != NULL)
    {
        process->next->previous = process->previous;
    }
    else
    {
        tail = process->previous;
    }
    free(process);
}

void checkForCompleteProcesses()
{
    struct linkedProcess *process = head;
    while (process != NULL)
    {
        int status;

        /* checks if processes are complete */
        if (waitpid(process->pid, &status, WNOHANG))
        {
            checkStatus(status, process->name);
            removeProcess(process);
        }
        process = process->next;
    }
}

void parseString(char *str, char **parsedArgs) {
    char delim[4] = "\t \n";

    for (int i = 0; i < MAXLEN; i++)
    {
        parsedArgs[i] = strsep(&str, delim);
        if (parsedArgs[i] == NULL)
            break;
        if (strlen(parsedArgs[i]) == 0)
            i--;
    }
}

char* readLine() {
    char* line = (char*) malloc(MAXLEN + 1); // one extra for the terminating null character
    // read the input
    if (fgets(line, MAXLEN + 1, stdin) == NULL) {
        if (feof(stdin)) {
            exit(EXIT_SUCCESS);
        } else {
            perror("basic shell: fgets error\n");
            exit(EXIT_FAILURE);
        }
    }
    return line;
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
        if (input[length] == ' ' || input[length] == '\t')
            length--;
        else
            break;
        input[length + 1] = '\0';
    }

    return check;
}

int checkInRedirect(char** args)
{
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            ++i;
            return i;
        }
    }
    return -1;
}

int checkOutRedirect(char** args)
{
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            ++i;
            return i;
        }
    }
    return -1;
}

void execute(char **args, char *input) {
    char* inFile;
    char* outFile;
    int background = checkIfBackgroundTask(input);
    int outRedirect = checkOutRedirect(args);
    int inRedirect = checkInRedirect(args);

    /* internal commands */
    if (strcmp(args[0], "exit") == 0 ) {
        printf("\nGoodbye\n");
        exit(0);
    }

    if (strcmp(args[0], "cd") == 0) {
        if (args[1]) {
            chdir(args[1]);
        }
        return;
    }
    if (strcmp(args[0], "jobs") == 0) {
        printAllProcesses();
        return;
    }

    int status;
    pid_t pid = fork();

    if (pid == -1) {
        perror("fork error");
    }

    if (pid == 0) {
        int in_descriptor;
        int out_descriptor;

        if (inRedirect >= 0) {
            inFile = args[inRedirect];
            args[inRedirect] = NULL;

            if ((in_descriptor = open(inFile, O_RDONLY)) < 0) {   // open file for reading
                fprintf(stderr, "error opening file\n");
            }

            dup2(in_descriptor, STDIN_FILENO);
            close(in_descriptor); 
        }

        if (outRedirect >= 0) {
            outFile = args[outRedirect];
            args[outRedirect] = NULL;
            
            out_descriptor = creat(outFile, 0644);
            dup2(out_descriptor, STDOUT_FILENO);
            close(out_descriptor); 
        }

        else {
            execvp(args[0], args);
        }
        exit(0);
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

/**
 * @brief runs the shell in a loop. Gets the input from user and
 * calls the execute function. If background task is finished,
 * it is shown before the next prompt is given.
 */
int main(void)
{
    char cwd[MAXLEN];
    char input[MAXLEN + 1];
    char inputString[MAXLEN];
    char *args[MAXLEN];

    init_shell();

    while (1)
    {
        /* checks background processes */
        checkForCompleteProcesses();

        /* gets current working directory and asks for input */
        if (getcwd(cwd, sizeof(cwd)) != NULL) {
            printf("%s: ", cwd);
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
        // execute the command 
        execute(args, inputString);
    }
}
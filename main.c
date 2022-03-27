#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <sys/types.h>
#include <unistd.h>
#include <signal.h>

#define MAXLEN 512
#define MAXLIST 10

char* read_line();
int execute(char**);
char** get_args(char*);

int checkInRedirect(char** args);
int checkOutRedirect(char** args);
int checkBackgroundProcesses(char** args);
int printAllBackgroundProcesses(char** args); //Not yet implemented

#define clear() printf("\033[H\033[J")
  
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

void handler(int sig) {
    int status;
    waitpid(-1, &status, WNOHANG);
}

int main()
{
    char *input;
    char **args;
    int status, count;
    char *prompt;

    signal(SIGCHLD, handler);
    init_shell();

    do {
        char* cwd = getcwd(NULL, 0);
        prompt = malloc((strlen(cwd) + 3) * sizeof(char));
        strcpy(prompt, cwd);
        strcat(prompt, ": ");

        free(cwd);

        printf("%s", prompt);
        input = read_line();
        args = get_args(input);
        status = execute(args);

        free(args);
        free(input);
        free(prompt);
    } while (status);

    return 0;
}

char* read_line()
{
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

int execute(char** args) 
{
    pid_t pid;
    int status;
    char* inFile;
    char* outFile;
    FILE* infp;
    FILE* outfp;
    int inRedirect;
    int outRedirect;
    int background;

    // if command is empty
    if (args[0] == NULL) {
        return 1;
    }

    // exit command
    if (strcmp(args[0], "exit") == 0 ) {
        printf("\nGoodbye\n");
        return 0;
    }

    // cd command
    if (strcmp(args[0], "cd") == 0) {
        if (args[1] == NULL) {
            fprintf(stderr, "shell output: expected argument to \"cd\"\n");
        } else {
            if (chdir(args[1]) != 0) {
              perror("shell output");
            }
        }
        return 1;
    }
    //TODO: Not yet implemented
    if (strcmp(args[0], "jobs") == 0) {
        printAllBackgroundProcesses(args);
    }

    // check for redirection or background process
    inRedirect = checkInRedirect(args);
    outRedirect = checkOutRedirect(args);
    background = checkBackgroundProcesses(args);

    // create a child process
    pid = fork();

    // the child process gets 0 in return from fork()
    if (pid == 0) {
        // redirection
        if (inRedirect >= 0) {
            inFile = args[inRedirect + 1];
            args[inRedirect] = NULL;

            infp = freopen(inFile, "r", stdin);
        }

        if (outRedirect >= 0) {
            outFile = args[outRedirect + 1];
            args[outRedirect] = NULL;

            outfp = freopen(outFile, "w", stdout);
        }

        // check for background processes
        if (background >= 0) {
            args[background] = NULL;
        }

        // call execvp() to execute the user command
        if (execvp(args[0], args) == -1) {
            perror("shell output");
        }
        exit(EXIT_FAILURE);
    }
    else if (pid < 0) {
        perror("shell output");
    }
    else {
        if (background < 0) {
            waitpid(pid, &status, 0);
        }
    }
    return 1;
}

char** get_args(char* line) {
    int index = 0;

    char delim[] = " \t\r\n";
    char **charList = malloc(MAXLIST * sizeof(char*));
    char *get_line;

    if (!charList) {
        perror("simple shell: allocation error\n");
        exit(EXIT_FAILURE);
    }

    get_line = strtok(line, delim);
    while (get_line != NULL) {
        // iteratively add each token to tokenList
        charList[index] = get_line;
        index++;

        if (index >= MAXLIST - 1) {
            break;
        }
        get_line = strtok(NULL, delim);
    }
    charList[index] = NULL;
    return charList;
}

int checkInRedirect(char** args)
{
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "<") == 0) {
            return i;
        }
    }
    return -1;
}

int checkOutRedirect(char** args)
{
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], ">") == 0) {
            return i;
        }
    }
    return -1;
}

int checkBackgroundProcesses(char** args)
{
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "&") == 0) {
            return i;
        }
    }
    return -1;
}

//TODO: Not yet implemented
int printAllBackgroundProcesses(char** args)
{
    for (int i = 0; args[i] != NULL; i++) {
        if (strcmp(args[i], "&")) {
            printf("job: %s \n", args[i]);
        }
    }
    return 0;
}
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_ARGS 64

// Function to parse input line into arguments
int parse_input(char *line, char **args) {
    int arg_count = 0;
    char *token = strtok(line, " \t\n");
    
    while (token != NULL && arg_count < MAX_ARGS - 1) {
        args[arg_count++] = token;
        token = strtok(NULL, " \t\n");
    }
    
    args[arg_count] = NULL;  // Null-terminate the argument list
    return arg_count;
}

int main() {
    char line[MAX_LINE];
    char *args[MAX_ARGS];
    pid_t pid;
    int status;

    while (1) {
        // Print shell prompt
        printf("myshell> ");
        
        // Read input line
        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;  // Exit on EOF
        }

        // Remove newline character
        line[strcspn(line, "\n")] = 0;

        // Skip empty lines
        if (strlen(line) == 0) {
            continue;
        }

        // Parse input into arguments
        int arg_count = parse_input(line, args);
        
        // Check for exit command
        if (strcmp(args[0], "exit") == 0) {
            break;
        }

        // Handle built-in cd command
        if (strcmp(args[0], "cd") == 0) {
            if (arg_count > 1) {
                if (chdir(args[1]) != 0) {
                    perror("cd error");
                }
            } else {
                // If no argument, go to home directory
                chdir(getenv("HOME"));
            }
            continue;
        }

        // Fork a child process
        pid = fork();

        if (pid < 0) {
            // Fork failed
            fprintf(stderr, "Fork failed\n");
            exit(1);
        } else if (pid == 0) {
            // Child process
            if (execvp(args[0], args) == -1) {
                perror("Command not found");
                exit(EXIT_FAILURE);
            }
        } else {
            // Parent process
            waitpid(pid, &status, 0);
        }
    }

    return 0;
}
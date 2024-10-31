#define _POSIX_C_SOURCE 200809L

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>

#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_TOKENS 128
#define MAX_WORD_LEN 256

// token "labels"
typedef enum
{
    TOKEN_WORD,
    TOKEN_PIPE,
    TOKEN_REDIRECT_IN,  // <
    TOKEN_REDIRECT_OUT, // >
    TOKEN_BACKGROUND,   // &
    TOKEN_EOL,
    TOKEN_EOF
} TokenType;

typedef struct
{
    TokenType type;
    char *value;
} Token;

typedef struct command
{
    char *args[MAX_ARGS];
    int arg_count;
    char *input_file;     // For input redirection
    char *output_file;    // For output redirection
    int background;       // For background processes
    struct command *next; // For piping
} Command;

typedef struct command_list {
    Command **commands;  // command pointers
    int count;          // # of commands
} CommandList;

CommandList* new_command_list() {
    CommandList* list = malloc(sizeof(CommandList));
    list->commands = malloc(sizeof(Command*) * MAX_ARGS);
    list->count = 0;
    return list;
}

Command *new_command()
{
    Command *cmd = malloc(sizeof(Command));
    if (!cmd)
        return NULL;

    cmd->arg_count = 0;
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->background = 0;
    cmd->next = NULL;

    for (int i = 0; i < MAX_ARGS; i++)
    {
        cmd->args[i] = NULL;
    }

    return cmd;
}

Token *tokenize_input(char *line, int *token_count)
{
    static Token tokens[MAX_TOKENS];
    *token_count = 0;
    char *current = line;

    while (*current != '\0' && *token_count < MAX_TOKENS)
    {
        // handling whitespace
        while (*current == ' ' || *current == '\t')
        {
            current++;
        }

        if (*current == '\0')
            break;

        Token *tok = &tokens[*token_count];

        // declare word buffer outside the switch
        char word[MAX_WORD_LEN];
        int i = 0;

        // special characters checking
        switch (*current)
        {
        case '|':
            tok->type = TOKEN_PIPE;
            tok->value = strdup("|");
            current++;
            break;
        case '<':
            tok->type = TOKEN_REDIRECT_IN;
            tok->value = strdup("<");
            current++;
            break;
        case '>':
            tok->type = TOKEN_REDIRECT_OUT;
            tok->value = strdup(">");
            current++;
            break;
        case '&':
            tok->type = TOKEN_BACKGROUND;
            tok->value = strdup("&");
            current++;
            break;
        default:
            // handle word tokens
            while (*current != '\0' && *current != ' ' &&
                   *current != '\t' && *current != '|' &&
                   *current != '<' && *current != '>' &&
                   *current != '&' && i < MAX_WORD_LEN - 1)
            {
                word[i++] = *current++;
            }
            word[i] = '\0';
            tok->type = TOKEN_WORD;
            tok->value = strdup(word);
            break;
        }

        (*token_count)++;
    }

    return tokens;
}

Command* parse_single_command(Token *tokens, int *current_pos, int token_count) {
    Command *first_cmd = new_command();
    Command *current_cmd = first_cmd;

    while (*current_pos < token_count) {
        Token token = tokens[*current_pos];
        
        // if we hit a background token, stop parsing this command
        if (token.type == TOKEN_BACKGROUND) {
            (*current_pos)++;
            break;
        }

        switch (token.type) {
            case TOKEN_WORD:
                current_cmd->args[current_cmd->arg_count++] = token.value;
                break;

            case TOKEN_PIPE:
                current_cmd->args[current_cmd->arg_count] = NULL;
                current_cmd->next = new_command();
                current_cmd = current_cmd->next;
                break;

            case TOKEN_REDIRECT_IN:
                if (*current_pos + 1 < token_count && tokens[*current_pos + 1].type == TOKEN_WORD) {
                    current_cmd->input_file = tokens[++(*current_pos)].value;
                }
                break;

            case TOKEN_REDIRECT_OUT:
                if (*current_pos + 1 < token_count && tokens[*current_pos + 1].type == TOKEN_WORD) {
                    current_cmd->output_file = tokens[++(*current_pos)].value;
                }
                break;

            default:
                break;
        }
        (*current_pos)++;
    }

    current_cmd->args[current_cmd->arg_count] = NULL;
    return first_cmd;
}

CommandList* parse_tokens(Token *tokens, int token_count) {
    CommandList* list = new_command_list();
    int current_pos = 0;

    while (current_pos < token_count) {
        Command* cmd = parse_single_command(tokens, &current_pos, token_count);
        list->commands[list->count++] = cmd;
    }

    return list;
}

void free_command_list(CommandList* list) {
    if (!list) return;
    for (int i = 0; i < list->count; i++) {
        Command* cmd = list->commands[i];
        while (cmd) {
            Command* next = cmd->next;
            free(cmd);
            cmd = next;
        }
    }
    free(list->commands);
    free(list);
}

// execute a single command
void execute_command(Command *cmd) {
    // handling built-in commands
    if (strcmp(cmd->args[0], "cd") == 0) {
        if (cmd->arg_count > 1) {
            if (chdir(cmd->args[1]) != 0) {
                perror("cd error");
            }
        } else {
            chdir(getenv("HOME"));
        }
        return;
    }

    pid_t pid = fork();

    if (pid == 0) {  // Child process
        // set up process group for background processes
        if (cmd->background) {
            setpgid(0, 0);  // put the process in its own process group
        }

        // input redirection
        if (cmd->input_file) {
            int fd = open(cmd->input_file, O_RDONLY);
            if (fd == -1) {
                perror("Input redirection failed");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        // output redirection
        if (cmd->output_file) {
            int fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1) {
                perror("Output redirection failed");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        execvp(cmd->args[0], cmd->args);
        perror("Command execution failed");
        exit(EXIT_FAILURE);
    } else if (pid < 0) {
        perror("Fork failed");
        return;
    }

    // parent process
    if (cmd->background) {
        printf("[%d] Running in background\n", pid);
    } else {
        int status;
        waitpid(pid, &status, 0);
    }
}

int main() {
    char line[MAX_LINE];

    while (1) {
        printf("mish> ");

        if (fgets(line, sizeof(line), stdin) == NULL) {
            break;
        }
        
        // remove newline
        line[strcspn(line, "\n")] = 0;
        
        // skip empty lines
        if (strlen(line) == 0) {
            continue;
        }

        // exit command
        if (strcmp(line, "exit") == 0) {
            break;
        }

        int token_count;
        Token *tokens = tokenize_input(line, &token_count);
        CommandList *cmd_list = parse_tokens(tokens, token_count);

        // execute each command
        for (int i = 0; i < cmd_list->count; i++) {
            Command *cmd = cmd_list->commands[i];
            while (cmd) {
                execute_command(cmd);
                cmd = cmd->next;
            }
        }

        free_command_list(cmd_list);
    }
    
    return 0;
}

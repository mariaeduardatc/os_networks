#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/wait.h>

#define MAX_LINE 1024
#define MAX_ARGS 64
#define MAX_TOKENS 128
#define MAX_WORD_LEN 256

// token "labels"
typedef enum {
    TOKEN_WORD,
    TOKEN_PIPE,
    TOKEN_REDIRECT_IN,  // <
    TOKEN_REDIRECT_OUT, // >
    TOKEN_BACKGROUND,   // &
    TOKEN_EOL,
    TOKEN_EOF
} TokenType;

typedef struct {
    TokenType type;
    char *value;
} Token;

typedef struct command {
    char *args[MAX_ARGS];
    int arg_count;
    char *input_file;    // For input redirection
    char *output_file;   // For output redirection
    int background;      // For background processes
    struct command *next;  // For piping
} Command;

Command* new_command() {
    Command *cmd = malloc(sizeof(Command));
    if (!cmd) return NULL;
    
    cmd->arg_count = 0;
    cmd->input_file = NULL;
    cmd->output_file = NULL;
    cmd->background = 0;
    cmd->next = NULL;
    
    for (int i = 0; i < MAX_ARGS; i++) {
        cmd->args[i] = NULL;
    }
    
    return cmd;
}

Token* tokenize_input(char *line, int *token_count) {
    static Token tokens[MAX_TOKENS];
    *token_count = 0;
    char *current = line;
    
    while (*current != '\0' && *token_count < MAX_TOKENS) {
        // handling whitespace
        while (*current == ' ' || *current == '\t') {
            current++;
        }
        
        if (*current == '\0') break;
        
        Token *tok = &tokens[*token_count];
        
        // declare word buffer outside the switch
        char word[MAX_WORD_LEN];
        int i = 0;

        // special characters checking
        switch (*current) {
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
                       *current != '&' && i < MAX_WORD_LEN - 1) {
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

Command* parse_tokens(Token *tokens, int token_count) {
    Command *first_cmd = new_command();
    Command *current_cmd = first_cmd;
    
    for (int i = 0; i < token_count; i++) {
        switch (tokens[i].type) {
            case TOKEN_WORD:
                current_cmd->args[current_cmd->arg_count++] = tokens[i].value;
                break;
                
            case TOKEN_PIPE:
                current_cmd->args[current_cmd->arg_count] = NULL;
                current_cmd->next = new_command();
                current_cmd = current_cmd->next;
                break;
                
            case TOKEN_REDIRECT_IN:
                if (i + 1 < token_count && tokens[i + 1].type == TOKEN_WORD) {
                    current_cmd->input_file = tokens[++i].value;
                }
                break;
                
            case TOKEN_REDIRECT_OUT:
                if (i + 1 < token_count && tokens[i + 1].type == TOKEN_WORD) {
                    current_cmd->output_file = tokens[++i].value;
                }
                break;
                
            case TOKEN_BACKGROUND:
                current_cmd->background = 1;
                break;
                
            default:
                break;
        }
    }
    
    current_cmd->args[current_cmd->arg_count] = NULL;
    return first_cmd;
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
        // input redirection
        if (cmd->input_file) {
            freopen(cmd->input_file, "r", stdin);
        }
        
        // output redirection
        if (cmd->output_file) {
            freopen(cmd->output_file, "w", stdout);
        }
        
        execvp(cmd->args[0], cmd->args);
        perror("Command execution failed");
        exit(EXIT_FAILURE);
    }
    
    if (!cmd->background) {
        waitpid(pid, NULL, 0);
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
        
        Command *cmd = parse_tokens(tokens, token_count);

        while (cmd) {
            execute_command(cmd);
            Command *next = cmd->next;
            free(cmd);
            cmd = next;
        }
    }
    
    return 0;
}
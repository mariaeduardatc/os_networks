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
#define MAX_PATHS 10

char *search_paths[MAX_PATHS] = {"/bin", NULL};

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
    char *input_file;     // input redirection
    char *output_file;    // output redirection
    int background;       // background processes
    struct command *next; // piping
} Command;

typedef struct command_list
{
    Command **commands; // command pointers
    int count;          // # of commands
} CommandList;

CommandList *new_command_list()
{
    CommandList *list = malloc(sizeof(CommandList));
    list->commands = malloc(sizeof(Command *) * MAX_ARGS);
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

Command *parse_single_command(Token *tokens, int *current_pos, int token_count)
{
    Command *first_cmd = new_command();
    Command *current_cmd = first_cmd;
    int output_redirect_count = 0;

    while (*current_pos < token_count)
    {
        Token token = tokens[*current_pos];

        // if we hit a background token, stop parsing this command
        if (token.type == TOKEN_BACKGROUND)
        {
            (*current_pos)++;
            break;
        }

        switch (token.type)
        {
        case TOKEN_WORD:
            current_cmd->args[current_cmd->arg_count++] = token.value;
            break;

        case TOKEN_PIPE:
            current_cmd->args[current_cmd->arg_count] = NULL;
            current_cmd->next = new_command();
            current_cmd = current_cmd->next;
            // reset output redirection count for new command in pipe
            output_redirect_count = 0;
            break;

        case TOKEN_REDIRECT_IN:
            if (*current_pos + 1 < token_count && tokens[*current_pos + 1].type == TOKEN_WORD)
            {
                current_cmd->input_file = tokens[++(*current_pos)].value;
            }
            break;

        case TOKEN_REDIRECT_OUT:
            output_redirect_count++;
            if (output_redirect_count > 1)
            {
                fprintf(stderr, "Error: Multiple output redirections not allowed\n");
                while (first_cmd)
                {
                    Command *next = first_cmd->next;
                    free(first_cmd);
                    first_cmd = next;
                }
                return NULL;
            }
            
            if (*current_pos + 1 < token_count && tokens[*current_pos + 1].type == TOKEN_WORD)
            {
                current_cmd->output_file = tokens[++(*current_pos)].value;
            }
            else
            {
                fprintf(stderr, "Error: Missing output file after >\n");
                while (first_cmd)
                {
                    Command *next = first_cmd->next;
                    free(first_cmd);
                    first_cmd = next;
                }
                return NULL;
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


CommandList *parse_tokens(Token *tokens, int token_count)
{
    CommandList *list = new_command_list();
    int current_pos = 0;

    while (current_pos < token_count)
    {
        Command *cmd = parse_single_command(tokens, &current_pos, token_count);
        if (cmd == NULL)
        {
            // clean up already parsed commands
            for (int i = 0; i < list->count; i++)
            {
                Command *current = list->commands[i];
                while (current)
                {
                    Command *next = current->next;
                    free(current);
                    current = next;
                }
            }
            free(list->commands);
            free(list);
            return NULL;
        }
        list->commands[list->count++] = cmd;
    }

    return list;
}


int search_path(char *command) {
    const char *directories[] = {"/bin", "/usr/bin", NULL};  // directories to search
    char full_path[256];

    for (int i = 0; directories[i] != NULL; i++) {
        // Construct the full path by combining directory and command
        snprintf(full_path, sizeof(full_path), "%s/%s", directories[i], command);

        // Check if the command is accessible and executable
        if (access(full_path, X_OK) == 0) {
            strcpy(command, full_path);  // Update command with full path
            return 1;                    // Command found and executable
        }
    }
    return 0;  // Command not found in any directory
}

void free_command_list(CommandList *list)
{
    if (!list)
        return;
    for (int i = 0; i < list->count; i++)
    {
        Command *cmd = list->commands[i];
        while (cmd)
        {
            Command *next = cmd->next;
            free(cmd);
            cmd = next;
        }
    }
    free(list->commands);
    free(list);
}

// execute a single command
void execute_command(Command *cmd)
{
    // handling built-in commands
    if (strcmp(cmd->args[0], "cd") == 0)
    {
        if (cmd->arg_count > 1)
        {
            if (chdir(cmd->args[1]) != 0)
            {
                perror("cd error");
            }
        }
        else
        {
            chdir(getenv("HOME"));
        }
        return;
    }

    pid_t pid = fork();

    if (pid == 0)
    { // child process
        // set up process group for background processes
        if (cmd->background)
        {
            setpgid(0, 0); // put the process in its own process group
        }

        // input redirection
        if (cmd->input_file)
        {
            int fd = open(cmd->input_file, O_RDONLY);
            if (fd == -1)
            {
                perror("Input redirection failed");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDIN_FILENO);
            close(fd);
        }

        // output redirection
        if (cmd->output_file)
        {
            int fd = open(cmd->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
            if (fd == -1)
            {
                perror("Output redirection failed");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        char *path = NULL; 
        if (search_path(cmd->args[0]) == 1)
        {
            path = cmd->args[0];
            execvp(path, cmd->args);
        }
        else
        {
            fprintf(stderr, "Command not found: %s\n", cmd->args[0]);
        }

        perror("Command execution failed");
        exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
        perror("Fork failed");
        return;
    }

    // parent process
    if (cmd->background)
    {
        printf("[%d] Running in background\n", pid);
    }
    else
    {
        int status;
        waitpid(pid, &status, 0);
    }
}

void execute_pipeline(Command *cmd)
{
    if (!cmd->next) {
        // no pipe -> execute the single command
        execute_command(cmd);
        return;
    }

    int num_commands = 0;
    Command *current = cmd;
    while (current) {
        num_commands++;
        current = current->next;
    }

    int pipes[num_commands - 1][2];
    pid_t pids[num_commands];

    // create all necessary pipes
    for (int i = 0; i < num_commands - 1; i++) {
        if (pipe(pipes[i]) == -1) {
            perror("Pipe creation failed");
            return;
        }
    }

    // execute all commands in pipeline
    current = cmd;
    for (int i = 0; i < num_commands; i++) {
        pids[i] = fork();

        if (pids[i] == 0) { // child process
            // input redirection for first command
            if (i == 0 && current->input_file) {
                int fd = open(current->input_file, O_RDONLY);
                if (fd == -1) {
                    perror("Input redirection failed");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            // output redirection for last command
            if (i == num_commands - 1 && current->output_file) {
                int fd = open(current->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1) {
                    perror("Output redirection failed");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            // setting up pipes
            if (i > 0) { // not 1st command -> read from previous pipe
                dup2(pipes[i-1][0], STDIN_FILENO);
            }
            if (i < num_commands - 1) { // not last command -> write to next pipe
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            // close pipe file descriptors
            for (int j = 0; j < num_commands - 1; j++) {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            if (search_path(current->args[0])) {
                execvp(current->args[0], current->args);
            } else {
                fprintf(stderr, "Command not found: %s\n", current->args[0]);
                exit(EXIT_FAILURE);
            }
        }
        else if (pids[i] < 0) {
            perror("Fork failed");
            return;
        }

        current = current->next;
    }

    // parent process -> close pipe file descriptors
    for (int i = 0; i < num_commands - 1; i++) {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // wait child processes to complete
    for (int i = 0; i < num_commands; i++) {
        waitpid(pids[i], NULL, 0);
    }
}

void process_line(char *line)
{
    line[strcspn(line, "\n")] = 0;

    if (strlen(line) == 0)
    {
        return;
    }

    if (strcmp(line, "exit") == 0)
    {
        exit(0);
    }

    int token_count;
    Token *tokens = tokenize_input(line, &token_count);
    CommandList *cmd_list = parse_tokens(tokens, token_count);

    // proceed if parsing was successful
    if (cmd_list != NULL)
    {
        for (int i = 0; i < cmd_list->count; i++)
        {
            execute_pipeline(cmd_list->commands[i]);
        }
        free_command_list(cmd_list);
    }
}

void execute_batch_file(const char *filename)
{
    FILE *batch_file = fopen(filename, "r");
    if (!batch_file)
    {
        perror("Error opening batch file");
        return;
    }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), batch_file) != NULL)
    {
        printf("wish(batch)> %s", line);
        process_line(line);
    }

    fclose(batch_file);
    exit(0);
}

int main(int argc, char *argv[])
{
    // batch file provided as an argument
    if (argc > 1)
    {
        // first argument after the program name -> treated as a batch file
        execute_batch_file(argv[1]);
        exit(0);
    }

    // interactive mode
    char line[MAX_LINE];
    while (1)
    {
        printf("wish> ");

        if (fgets(line, sizeof(line), stdin) == NULL)
        {
            printf("\n");
            exit(0);
        }

        // remove newline and process the line
        process_line(line);
    }

    return 0;
}
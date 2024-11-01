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

char *search_paths[MAX_PATHS];
int path_count = 0;

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
    int input_redirect_count = 0;
    int has_command = 0;  // flag to track if we have a command before redirection

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
            // no redirection yet -> this word is part of the command
            if (input_redirect_count == 0 && output_redirect_count == 0)
            {
                has_command = 1;
            }
            current_cmd->args[current_cmd->arg_count++] = token.value;
            break;

        case TOKEN_PIPE:
            if (!has_command)
            {
                fprintf(stderr, "An error has occurred\n");
                while (first_cmd)
                {
                    Command *next = first_cmd->next;
                    free(first_cmd);
                    first_cmd = next;
                }
                return NULL;
            }
            current_cmd->args[current_cmd->arg_count] = NULL;
            current_cmd->next = new_command();
            current_cmd = current_cmd->next;
            // reset redirection counts and has_command flag for new command in pipe
            output_redirect_count = 0;
            input_redirect_count = 0;
            has_command = 0;
            break;

        case TOKEN_REDIRECT_IN:
            if (!has_command)
            {
                fprintf(stderr, "An error has occurred\n");
                while (first_cmd)
                {
                    Command *next = first_cmd->next;
                    free(first_cmd);
                    first_cmd = next;
                }
                return NULL;
            }
            
            input_redirect_count++;
            if (input_redirect_count > 1)
            {
                fprintf(stderr, "An error has occurred\n");
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
                current_cmd->input_file = tokens[++(*current_pos)].value;
                // if next token is also a word
                if (*current_pos + 1 < token_count && 
                    tokens[*current_pos + 1].type == TOKEN_WORD &&
                    tokens[*current_pos + 1].type != TOKEN_PIPE &&
                    tokens[*current_pos + 1].type != TOKEN_REDIRECT_IN &&
                    tokens[*current_pos + 1].type != TOKEN_REDIRECT_OUT &&
                    tokens[*current_pos + 1].type != TOKEN_BACKGROUND)
                {
                    fprintf(stderr, "An error has occurred\n");
                    while (first_cmd)
                    {
                        Command *next = first_cmd->next;
                        free(first_cmd);
                        first_cmd = next;
                    }
                    return NULL;
                }
            }
            else
            {
                fprintf(stderr, "An error has occurred\n");
                while (first_cmd)
                {
                    Command *next = first_cmd->next;
                    free(first_cmd);
                    first_cmd = next;
                }
                return NULL;
            }
            break;

        case TOKEN_REDIRECT_OUT:
            if (!has_command)
            {
                fprintf(stderr, "An error has occurred\n");
                while (first_cmd)
                {
                    Command *next = first_cmd->next;
                    free(first_cmd);
                    first_cmd = next;
                }
                return NULL;
            }
            
            output_redirect_count++;
            if (output_redirect_count > 1)
            {
                fprintf(stderr, "An error has occurred\n");
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
                // if next token is also a word (multiple files after redirection)
                if (*current_pos + 1 < token_count && 
                    tokens[*current_pos + 1].type == TOKEN_WORD &&
                    tokens[*current_pos + 1].type != TOKEN_PIPE &&
                    tokens[*current_pos + 1].type != TOKEN_REDIRECT_IN &&
                    tokens[*current_pos + 1].type != TOKEN_REDIRECT_OUT &&
                    tokens[*current_pos + 1].type != TOKEN_BACKGROUND)
                {
                    fprintf(stderr, "An error has occurred\n");
                    while (first_cmd)
                    {
                        Command *next = first_cmd->next;
                        free(first_cmd);
                        first_cmd = next;
                    }
                    return NULL;
                }
            }
            else
            {
                fprintf(stderr, "An error has occurred\n");
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

    // final check to ensure we had a command
    if (!has_command)
    {
        while (first_cmd)
        {
            Command *next = first_cmd->next;
            free(first_cmd);
            first_cmd = next;
        }
        return NULL;
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

void initialize_paths()
{
    // clear existing paths
    for (int i = 0; i < MAX_PATHS; i++)
    {
        search_paths[i] = NULL;
    }
    // default path to /bin
    search_paths[0] = strdup("/bin");
    path_count = 1;
}

void handle_path_command(Command *cmd)
{
    // Free existing paths
    for (int i = 0; i < path_count; i++)
    {
        free(search_paths[i]);
        search_paths[i] = NULL;
    }
    path_count = 0;

    // Add new paths
    for (int i = 1; i < cmd->arg_count && path_count < MAX_PATHS; i++)
    {
        search_paths[path_count++] = strdup(cmd->args[i]);
    }
}

int search_path(char *command)
{
    char full_path[256];

    // First check if the command is an absolute path or relative path
    if (strchr(command, '/') != NULL)
    {
        if (access(command, X_OK) == 0)
        {
            return 1;
        }
        return 0;
    }

    // Search in all paths
    for (int i = 0; i < path_count; i++)
    {
        if (search_paths[i] == NULL)
            continue;

        snprintf(full_path, sizeof(full_path), "%s/%s", search_paths[i], command);
        if (access(full_path, X_OK) == 0)
        {
            strcpy(command, full_path);
            return 1;
        }
    }
    return 0;
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

void execute_command(Command *cmd)
{
    // handling built-in commands
    if (strcmp(cmd->args[0], "cd") == 0)
    {
        if (cmd->arg_count == 1)
        {
            fprintf(stderr, "An error has occurred\n");
            return;
        }
        if (chdir(cmd->args[1]) != 0)
        {
            fprintf(stderr, "An error has occurred\n");
        }
        return;
    }

    // Add path command handling
    if (strcmp(cmd->args[0], "path") == 0)
    {
        handle_path_command(cmd);
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
                fprintf(stderr, "An error has occurred\n");
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
                fprintf(stderr, "An error has occurred\n");
                exit(EXIT_FAILURE);
            }
            dup2(fd, STDOUT_FILENO);
            close(fd);
        }

        if (search_path(cmd->args[0]))
        {
            execv(cmd->args[0], cmd->args);
        }

        fprintf(stderr, "An error has occurred\n");
        exit(EXIT_FAILURE);
    }
    else if (pid < 0)
    {
        fprintf(stderr, "An error has occurred\n");
        return;
    }

    // parent process
    if (!cmd->background)
    {
        int status;
        waitpid(pid, &status, 0);
    }
}

void execute_pipeline(Command *cmd)
{
    if (!cmd->next)
    {
        // no pipe -> execute the single command
        execute_command(cmd);
        return;
    }

    int num_commands = 0;
    Command *current = cmd;
    while (current)
    {
        num_commands++;
        current = current->next;
    }

    int pipes[num_commands - 1][2];
    pid_t pids[num_commands];

    // create all necessary pipes
    for (int i = 0; i < num_commands - 1; i++)
    {
        if (pipe(pipes[i]) == -1)
        {
            perror("Pipe creation failed");
            return;
        }
    }

    // execute all commands in pipeline
    current = cmd;
    for (int i = 0; i < num_commands; i++)
    {
        pids[i] = fork();

        if (pids[i] == 0)
        { // child process
            // input redirection for first command
            if (i == 0 && current->input_file)
            {
                int fd = open(current->input_file, O_RDONLY);
                if (fd == -1)
                {
                    perror("Input redirection failed");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDIN_FILENO);
                close(fd);
            }

            // output redirection for last command
            if (i == num_commands - 1 && current->output_file)
            {
                int fd = open(current->output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                if (fd == -1)
                {
                    perror("Output redirection failed");
                    exit(EXIT_FAILURE);
                }
                dup2(fd, STDOUT_FILENO);
                close(fd);
            }

            // setting up pipes
            if (i > 0)
            { // not 1st command -> read from previous pipe
                dup2(pipes[i - 1][0], STDIN_FILENO);
            }
            if (i < num_commands - 1)
            { // not last command -> write to next pipe
                dup2(pipes[i][1], STDOUT_FILENO);
            }

            // close pipe file descriptors
            for (int j = 0; j < num_commands - 1; j++)
            {
                close(pipes[j][0]);
                close(pipes[j][1]);
            }

            if (search_path(current->args[0]))
            {
                execvp(current->args[0], current->args);
            }
            else
            {
                fprintf(stderr, "An error has occurred\n");
                exit(EXIT_FAILURE);
            }
        }
        else if (pids[i] < 0)
        {
            perror("Fork failed");
            return;
        }

        current = current->next;
    }

    // parent process -> close pipe file descriptors
    for (int i = 0; i < num_commands - 1; i++)
    {
        close(pipes[i][0]);
        close(pipes[i][1]);
    }

    // wait child processes to complete
    for (int i = 0; i < num_commands; i++)
    {
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
        perror("An error has occurred");
        return;
    }

    char line[MAX_LINE];
    while (fgets(line, sizeof(line), batch_file) != NULL)
    {
        process_line(line);
    }

    fclose(batch_file);
    exit(0);
}

int main(int argc, char *argv[])
{
    initialize_paths();
    
    // if more than one argument is provided
    if (argc > 2)
    {
        fprintf(stderr, "An error has occurred\n");
        exit(1);
    }
    
    // batch file provided as an argument
    if (argc == 2)
    {
        // first argument after the program name -> treated as a batch file
        FILE *batch_file = fopen(argv[1], "r");
        if (!batch_file)
        {
            fprintf(stderr, "An error has occurred\n");
            exit(1);
        }
        
        char line[MAX_LINE];
        while (fgets(line, sizeof(line), batch_file) != NULL)
        {
            process_line(line);
        }

        fclose(batch_file);
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
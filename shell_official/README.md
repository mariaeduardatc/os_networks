# IL181 - Build your own shell
In this documentation, I outline my journey in developing a simple shell, focusing on the essential components and functionalities that make it operational. My goal was to grasp the intricacies of shell design while ensuring that my implementation is informed by my understanding rather than merely replicating existing resources.
[For the code documentation, scroll down]
## Understanding the Shell
Before starting any code and tutorial, I wanted to understand first what a shell is, its main components, and how people come to build these blocks. I am starting this project with no background knowledge in the application nor in C.
A shell is a user program that provides an interactive interface for accessing services from the operating system. As defined on Geeks for Geeks (2024), it translates user commands into instructions that the kernel can understand. The process of command execution follows several steps:
- If command length is not null, keep it in history
- The command will be parsed
- The command will be checked for special characters (like pipes) and built-in commands
- Special characters etc will be handled
- System commands and libraries are executed by forking a child and calling “execvp”
- The shell asks for the following input

As explained by Rodriguez-Rivera and Ennen (2014), shell implementation can be divided into three main parts: the parser, the executor, and various shell subsystems.
- **Parser**: This component processes command line inputs (e.g., "ls -al") and organizes them into a data structure known as the Command Table, which holds the commands to be executed.
- **Executor**: The executor takes this Command Table and initiates a new process for each SimpleCommand in the array. It sets up pipes to route output from one process to the input of another if necessary. Additionally, it handles the redirection of standard input, output, and error streams as specified. Other shell subsystems manage environment variables, built-in commands, and handle redirection and pipes.

## Project Phases
### Initial Steps
For the assignment there are four main structures I had to work on. First, the basic functionality, second the built-in commands, then the redirection, and finally the parallel commands. To start, I followed Hackernoon’s tutorial. This tutorial tries to answer some of the main questions I myself had when it comes to this project (”How does the shell parse my commands, convert them to executable instructions, and then perform these commands?”).


## THE FIRST SHELL - building up the first steps of the project
<img width="560" alt="image" src="https://github.com/user-attachments/assets/2dd61071-8dd9-4cea-b036-d37908d3bdd1">

Following this part of the tutorial was important so I could start understanding what would be needed for the shell to read commands. Dynamically assigning memory to the command string is something I am not generally used to do when I code in languages such as Python and JavaScript, so having a simple hands on in how the process looks like and the different considerations I have to take (NULL inputs, multi-lines, and how to capture those), was an informative way to start getting the hang of C as a language.

However, at this stage I only echo what the user gives me as a command. I don’t actually parse it into an actual useful output, for example, if I type “ls” I won’t have the directories and files of my path given back to me. To start building this function into the shell, I had to go back to Rodriguez-Rivera and Ennen (2014) to understand the blocks that form this aspect of the shell.

To parse a command we need two things to work together, a Lexical Analyzer (Lexer) and a Parser. The Lexer, takes input characters and groups them into units known as tokens. Then, the Parser processes these tokens based on a defined grammar to construct the command table (Rodriguez-Rivera & Ennen, 2014).

### Lexical Analysis
I learned that the lexical analyzer scans each character of the input sequentially, distinguishing between characters that can form tokens and those that signal the end of a token. It is crucial to "peek" at the next character without consuming it, allowing the parser to differentiate between potential tokens that share prefixes (e.g., distinguishing between a variable "i" and the keyword "if") (CS143 Lecture 3 Lexical Analysis, n.d.). This foresight is vital for correctly identifying the boundaries of tokens, especially for handling new lines and token termination.

### Building the Abstract Syntax Tree
After implementing the lexical scanner, I moved on to creating the parser, which constructs an abstract syntax tree for execution.
Understanding the need for child processes and the fork() system call was critical:
- When a process wants to execute another program, it forks itself to create a duplicate known as a "child process." This child process then uses the exec system call to replace its image with the new program, effectively ceasing execution of the original program.
- The fork operation creates a separate address space for the child process, which has a copy of the parent process's memory segments.
- Typically, the child performs a limited set of actions before ceasing execution to begin the new program, requiring few, if any, of the parent's data structures.
- After the fork, both processes run the same program and can check the call's return value to determine whether they are the child or parent process.

### Enhancing the Shell's Functionality
With a foundational understanding in place, I began tweaking the code to fit the assignment's requirements. I added the built-in commands, redirection, and pipelines. The next annotations are the main areas I struggled a bit more to understand, and my final thoughts on them.

#### Tokenization Approach
At first, I wanted to simplify my code using strtok() to split input strings. In the tutorial, I created a tree structure where each word is a node, using custom structures like node_s, and then did the memory management more explicitly. I dig around to try to understand what approach would be better in the context of the assignment and decided to use the tutorial and adapt it:

- strtok() can’t distinguish special symbols from words. In the tutorial’s approach, tokenizing is like putting labels on each part of the input, allowing us to identify if something is a command, an argument, or a control operator like | (pipe) or < (input redirection). This labeling is essential to handle commands correctly in a structured way.
- strtok() modifies the original string directly, which can be problematic if we need to keep the original input intact. The tutorial’s method protects the original string by making explicit copies of each token. This way, we can manage and free memory later without altering the input string, avoiding unwanted side effects.
- Parsing structures like pipes and redirection (|, <, >) requires tokens to represent these symbols independently, something strtok() doesn’t handle because it treats all delimiters as mere separators. The tutorial’s approach captures these elements as individual tokens, which allows building more complex commands with clear links between components, like chaining commands in a pipeline.

In general, I opted for borrowing the tutorial’s memory management approach. I explicitly allocate memory for each command and check for allocation failures. This can be slower, but it is safer and more flexible—now commands of any size within system limits can be handled.

#### Search Path Function
I included a search_path function to initialize /bin in my shell's command lookup explicitly. At first, I didn’t fully understand why the assignment asked for this, and the general purpose of explicitly creating a function to do something the system could do (initially, my shell relied on the system’s PATH variable to locate executables). After researching more about it I understood that with incorporating /bin directly my shell gained greater control over command execution. This enhancement improved portability and functionality by ensuring the shell operates independently of the system’s environmental configurations.

# Code Documentation

#### CommandList *new_command_list()
This function initializes a new CommandList structure, which is designed to store a list of commands for execution.
#### Command *new_command()
This function creates a new Command structure, which represents a single command with its arguments and associated properties.
#### Token *tokenize_input(char *line, int *token_count)
This function takes an input line (a command line string) and splits it into tokens, which represent individual elements such as commands, arguments, and special characters. It uses a static array tokens to store the parsed tokens, allowing the function to return a pointer to a token list. Special characters create specific tokens (e.g., TOKEN_PIPE for |), while alphanumeric sequences become TOKEN_WORD tokens. 
#### Command *parse_single_command(Token *tokens, int *current_pos, int token_count)
This function parses tokens into a single Command or linked list of Command structures, setting up individual commands with their respective arguments, input/output redirection, and pipeline connections as required. For each token, it examines the token type and updates the command structure accordingly, handling cases where the token represents a command argument, pipe (|), input redirection (<), output redirection (>), or background execution (&).
#### CommandList *parse_tokens(Token *tokens, int token_count)
This function parses an array of tokens into a CommandList structure, where each command is stored sequentially in the list for later execution. 
#### void initialize_paths()
This function initializes the default search paths for command execution, clearing any previously stored paths and setting the initial path to /bin. This setup is crucial for locating executable files if commands are not given with an explicit path.
#### void handle_path_command(Command *cmd)
This function updates the shell’s search paths based on a path command, modifying the global search_paths array according to user-specified directories. It first frees all currently stored paths to clear previous settings. Each new path is saved in the array and increments the path_count, creating a fresh list of directories the shell can search for executable files.
#### int search_path(char *command)
This function checks if a given command can be executed by searching for it in the specified paths or determining if it's an absolute or relative path.
#### void free_command_list(CommandList *list)
This function deallocates memory associated with a CommandList, ensuring that all commands within the list are properly freed. Once all commands have been freed, the function deallocates the commands array itself and finally frees the CommandList. This cleanup is essential for preventing memory leaks in the application.
#### void execute_command(Command *cmd)
This function executes a command represented by a Command structure, handling built-in commands and forking a child process for external commands.
#### void execute_pipeline(Command *cmd)
This function executes a series of commands connected by pipes, handling both single and multiple commands in a pipeline. If there’s only one command (no pipe), it delegates the execution to the execute_command function. For multiple commands, it first counts the number of commands in the pipeline and creates the necessary pipes for inter-process communication. It then forks a new process for each command. Each child process sets up input and output redirection based on its position in the pipeline and handles any specified input/output files. The child processes also redirect the input from the previous pipe (if applicable) and the output to the next pipe. Once all commands are forked, the parent process closes the pipe file descriptors and waits for all child processes to complete, ensuring proper execution of the entire pipeline.
#### void process_line(char *line)
This function processes a single line of input from the user, performing necessary transformations and handling specific commands.
#### void execute_batch_file(const char *filename)
This function executes a batch file specified by its filename, reading and processing each line as a command.

# References
1. Arpaci-Dusseau, R. H., Jr. (2008). Interlude: Process API. In THREE EASY PIECES. https://pages.cs.wisc.edu/~remzi/OSTEP/cpu-api.pdf
2. Brennan, S. (2015, January 16). Tutorial - Write a shell in C - Stephen Brennan. Stephen Brennan’s Blog. https://brennan.io/2015/01/16/write-a-shell-in-c/
3. CS143 Lecture 3 Lexical Analysis (By Prof. Alex Aiken). (n.d.). https://web.stanford.edu/class/cs143/lectures/lecture03.pdf
4. GeeksforGeeks. (2024a, October 11). fork() in C. GeeksforGeeks. https://www.geeksforgeeks.org/fork-system-call/
5. GeeksforGeeks. (2024b, October 11). Making your own Linux Shell in C. GeeksforGeeks. https://www.geeksforgeeks.org/making-linux-shell-c/
6. Isam, M. (2020, June 8). Let's Build a Linux Shell [Part I]. https://hackernoon.com/lets-build-a-linux-shell-part-i-bz3n3vg1
7. Minimizing memory usage for creating application subprocesses. (n.d.). https://web.archive.org/web/20190922113430/https://www.oracle.com/technetwork/server-storage/solaris10/subprocess-136439.html
8. Rodriguez-Rivera, G., & Ennen, J. (2014). Chapter 5. Writing Your Own Shell. In Introduction to Systems Programming: a Hands-on Approach. https://www.cs.purdue.edu/homes/grr/SystemsProgrammingBook/Book/Chapter5-WritingYourOwnShell.pdf

# AI Usage Policy
Chat GPT and Claude were used to assist me in creating the code, analyzing the tutorial, and explaining concepts such as methods and functions I could not find easily on the internet. Claude helped me in adapting the tokenization approach used on the tutorial to the new code I was creating based on Stephen Brennan's tutorial and GPT suggestions. Claude also gave me the pseudo-code for the redirection and built-in commands section of the code.

#include <stdio.h>
#include "shell.h"

void print_prompt1(void) // 1st prompt string PS1 (when the shell waits for u to enter a command)
{
    fprintf(stderr, "$ ");
}

void print_prompt2(void) // PS2 (when u enter a multi-line command)
{
    fprintf(stderr, "> ");
}
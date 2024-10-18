#include <stdio.h> //io functions like printf
#include <stdlib.h> // memory alloc, process control like exit
#include <errno.h> // error handling 
#include <string.h> // manipulate string functions
#include "shell.h" // custom for shell-spcific fucntions like read_cmd

int main(int argc, char **argv)
{
    char *cmd; // declare a pointer to hold the user's command

    do
    {
        print_prompt1();

        cmd = read_cmd();

        if (!cmd)
        {
            exit(EXIT_SUCCESS); // checks for NULL
        }

        if (cmd[0] == '\0' || strcmp(cmd, "\n") == 0)
        {
            free(cmd); // if the command is empty or just a new line
            continue;
        }

        if (strcmp(cmd, "exit\n") == 0)
        {
            free(cmd);
            break;
        }

        printf("%s\n", cmd);

        free(cmd);

    } while (1);

    exit(EXIT_SUCCESS);
}

// reads the user's command
char *read_cmd(void)
{
    char buf[1024]; // buffer to store chunks of input from the user (as it reads)
    char *ptr = NULL; // pointer to dynamically allocate memory for the command
    char ptrlen = 0; // current length of the data in the pointer (how much has been read so far)

    while(fgets(buf, 1024, stdin))
    {
        int buflen = strlen(buf); // 

        if(!ptr)
        {
            ptr = malloc(buflen+1); // extra byte for \0 that was not counted in buflen
        }
        else
        {
            char *ptr2 = realloc(ptr, ptrlen+buflen+1); // (resizing) reallocating memory for when there's already info on ptr

            if(ptr2)
            {
                ptr = ptr2; // update ptr so we have it pointing to the new memory block
            }
            else
            {
                free(ptr);
                ptr = NULL;
            }
        }

        if(!ptr)
        {
            fprintf(stderr, "error: failed to alloc buffer: %s\n", strerror(errno));
            return NULL;
        }

        strcpy(ptr+ptrlen, buf);

        if(buf[buflen-1] == '\n') // checks if user pressed enter
        {
            if(buflen == 1 || buf[buflen-2] != '\\') // checks for (not) line continuation
            {
                return ptr;
            }

            ptr[ptrlen+buflen-2] = '\0';
            buflen -= 2;
            print_prompt2();
        }

        ptrlen += buflen; 
    }

    return ptr;
}

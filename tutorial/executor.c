#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include "shell.h"
#include "node.h"
#include "executor.h"

char *search_path(char *file)
{
    char *PATH = getenv("PATH");
    char *p = PATH;
    char *p2;

    while (p && *p)
    {
        p2 = p;

        while (*p2 && *p2 != ':') // iteration through rhe chars in PATH
        {
            p2++;
        }

        int plen = p2 - p;
        if (!plen) // ensures there is always at least one byte for buildiing the path
        {
            plen = 1;
        }

        int alen = strlen(file);
        char path[plen + 1 + alen + 1]; // holds the full path

        strncpy(path, p, p2 - p); // dir name is copied
        path[p2 - p] = '\0';      // to ensure the string is null-terminated

        if (p2[-1] != '/')
        {
            strcat(path, "/");
        }

        strcat(path, file);

        struct stat st;
        if (stat(path, &st) == 0)
        {
            if (!S_ISREG(st.st_mode)) // checks if it is a special file (like a directory)
            {
                errno = ENOENT;
                p = p2;
                if (*p2 == ':')
                {
                    p++;
                }
                continue;
            }

            p = malloc(strlen(path) + 1);
            if (!p)
            {
                return NULL;
            }

            strcpy(p, path);
            return p;
        }
        else // move to the next directory if the file still hasnt been found
        {
            p = p2;
            if (*p2 == ':')
            {
                p++;
            }
        }
    }

    errno = ENOENT;
    return NULL;
}

int do_exec_cmd(int argc, char **argv)
{
    if (strchr(argv[0], '/')) // here the user provided a specific path to the command
    {
        execv(argv[0], argv);
    }
    else
    {
        char *path = search_path(argv[0]); // search for the command in the directories listed in the PATH
        if (!path)
        {
            return 0;
        }
        execv(path, argv);
        free(path);
    }
    return 0;
}

static inline void free_argv(int argc, char **argv)
{
    if (!argc)
    {
        return;
    }

    while (argc--)
    {
        free(argv[argc]);
    }
}

int do_simple_command(struct node_s *node)
{
    if (!node)
    {
        return 0;
    }

    struct node_s *child = node->first_child;
    if (!child)
    {
        return 0;
    }

    // keep track of the number of arguments
    int argc = 0;
    long max_args = 255;
    char *argv[max_args + 1];
    char *str;

    while (child)
    {
        str = child->val.str; // extract the actual command
        argv[argc] = malloc(strlen(str) + 1);

        if (!argv[argc])
        {
            free_argv(argc, argv);
            return 0;
        }
        
        // copy the string into the allocated space
        strcpy(argv[argc], str);
        if (++argc >= max_args)
        {
            break;
        }
        child = child->next_sibling;
    }
    argv[argc] = NULL;

    pid_t child_pid = 0;
    if ((child_pid = fork()) == 0)
    {
        do_exec_cmd(argc, argv);
        fprintf(stderr, "error: failed to execute command: %s\n", strerror(errno));
        if (errno == ENOEXEC)
        {
            exit(126);
        }
        else if (errno == ENOENT)
        {
            exit(127);
        }
        else
        {
            exit(EXIT_FAILURE);
        }
    }
    else if (child_pid < 0)
    {
        fprintf(stderr, "error: failed to fork command: %s\n", strerror(errno));
        return 0;
    }

    int status = 0;
    waitpid(child_pid, &status, 0); // suspends the parent process until the child's is completed
    free_argv(argc, argv);

    return 1;
}
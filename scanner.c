#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "shell.h"
#include "scanner.h"
#include "source.h"

char *tok_buf = NULL; // pointer to store the current token
int tok_bufsize = 0; // bytes we're alloc to buffer
int tok_bufindex = -1; // where to add the next input char in the buffer

// to signal the end of the input
struct token_s eof_token =
    {
        .text_len = 0,
};

void add_to_buf(char c)
{
    tok_buf[tok_bufindex++] = c; // add the char to the next position

    if (tok_bufindex >= tok_bufsize) // realloc for more memory space
    {
        char *tmp = realloc(tok_buf, tok_bufsize * 2); // tries first then assign values

        if (!tmp)
        {
            errno = ENOMEM;
            return;
        }

        tok_buf = tmp;
        tok_bufsize *= 2;
    }
}

struct token_s *create_token(char *str)
{
    struct token_s *tok = malloc(sizeof(struct token_s));

    if (!tok)
    {
        return NULL;
    }

    memset(tok, 0, sizeof(struct token_s)); // initializing bytes
    tok->text_len = strlen(str); // accessing text_len on the struct

    char *nstr = malloc(tok->text_len + 1);

    if (!nstr)
    {
        free(tok);
        return NULL;
    }

    strcpy(nstr, str);
    tok->text = nstr;

    return tok;
}

// free the memory of the token's text 
void free_token(struct token_s *tok)
{
    if (tok->text)
    {
        free(tok->text);
    }
    free(tok); // free the memory alloc to the structure
}

struct token_s *tokenize(struct source_s *src) // will generate tokens based on whitespace and newline characters
{
    int endloop = 0;

    if (!src || !src->buffer || !src->bufsize)
    {
        errno = ENODATA;
        return &eof_token;
    }

    if (!tok_buf)
    {
        tok_bufsize = 1024;
        tok_buf = malloc(tok_bufsize);
        if (!tok_buf)
        {
            errno = ENOMEM;
            return &eof_token;
        }
    }

    tok_bufindex = 0;
    tok_buf[0] = '\0';

    char nc = next_char(src);

    if (nc == ERRCHAR || nc == EOF)
    {
        return &eof_token;
    }

    do
    {
        switch (nc)
        {
            // end cases
        case ' ':
        case '\t':
            if (tok_bufindex > 0)
            {
                endloop = 1;
            }
            break;

        // newline case
        case '\n':
            if (tok_bufindex > 0)
            {
                unget_char(src); // here the new line would prob indicate the end of a token
            }
            else
            {
                add_to_buf(nc); // here the new line prob represents a token itself
            }
            endloop = 1;
            break;

        default:
            add_to_buf(nc);
            break;
        }

        if (endloop)
        {
            break;
        }

    } while ((nc = next_char(src)) != EOF);

    if (tok_bufindex == 0)
    {
        return &eof_token;
    }

    if (tok_bufindex >= tok_bufsize)
    {
        tok_bufindex--;
    }
    tok_buf[tok_bufindex] = '\0';

    struct token_s *tok = create_token(tok_buf);
    if (!tok)
    {
        fprintf(stderr, "error: failed to alloc buffer: %s\n", strerror(errno));
        return &eof_token;
    }

    tok->src = src;
    return tok;
}

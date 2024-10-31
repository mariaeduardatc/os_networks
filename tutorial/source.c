#include <errno.h>
#include "shell.h"
#include "source.h"

void unget_char(struct source_s *src)
{
    if(src->curpos < 0)
    {
        return;
    }

    src->curpos--;
}


char next_char(struct source_s *src)
{
    if(!src || !src->buffer) // checks if src pointer or buffer it points to is NULL
    {
        errno = ENODATA;
        return ERRCHAR;
    }

    char c1 = 0;
    if(src->curpos == INIT_SRC_POS) //
    {
        src->curpos  = -1;
    }
    else
    {
        c1 = src->buffer[src->curpos]; // capturing the current character before advancing (check if necessary)
    }

    if(++src->curpos >= src->bufsize) // increments to move forward
    {
        src->curpos = src->bufsize;
        return EOF;
    }

    return src->buffer[src->curpos]; // access the char at index curpos
}


char peek_char(struct source_s *src)
{
    if(!src || !src->buffer)
    {
        errno = ENODATA;
        return ERRCHAR;
    }

    long pos = src->curpos; // important for the whole peek idea!

    if(pos == INIT_SRC_POS)
    {
        pos++;
    }
    pos++;

    if(pos >= src->bufsize)
    {
        return EOF;
    }

    return src->buffer[pos];
}


void skip_white_spaces(struct source_s *src)
{
    char c;

    if(!src || !src->buffer)
    {
        return;
    }

    while(((c = peek_char(src)) != EOF) && (c == ' ' || c == '\t'))
    {
        next_char(src);
    }
}
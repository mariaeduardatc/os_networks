#ifndef SOURCE_H
#define SOURCE_H

#define EOF (-1) // end of the input stream
#define ERRCHAR (0) // invalid or error char
#define INIT_SRC_POS (-2) // not started marker (initial position of the source before reading)

// structure (user-defined data)
struct source_s
{
    char *buffer; // input (pointer)
    long bufsize;
    long curpos; // absolute char position in source
};

// #src is storing the memory address of source_s
char next_char(struct source_s *src);
void unget_char(struct source_s *src); // backtracking
char peek_char(struct source_s *src);
void skip_white_spaces(struct source_s *src);

#endif
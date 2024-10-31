#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <ctype.h>
#include "shell.h"
#include "scanner.h"
#include "source.h"

struct token_s {
    char *text;
    size_t text_len;
    struct source_s *src;
};

struct source_s {
    char *buffer;
    size_t bufsize;
    size_t curpos;
};

// to signal the end of the input
struct token_s eof_token =
    {
        .text_len = 0,
};

struct token_s *create_token(char *str)
{
    struct token_s *tok = malloc(sizeof(struct token_s));
    if (!tok) {
        return NULL;
    }

    tok->text_len = strlen(str);
    tok->text = strdup(str);
    if (!tok->text) {
        free(tok);
        return NULL;
    }

    return tok;
}

void free_token(struct token_s *tok)
{
    if (tok->text) {
        free(tok->text);
    }
    free(tok);
}

struct token_s *tokenize(struct source_s *src)
{
    static char *line = NULL;
    static size_t len = 0;
    static ssize_t read = 0;
    static size_t pos = 0;

    // Read a new line if we've reached the end of the current one
    if (pos >= read || !line) {
        pos = 0;
        read = getline(&line, &len, stdin);
        if (read == -1) {
            free(line);
            line = NULL;
            return &eof_token;
        }
    }

    // Skip leading whitespace
    while (pos < read && isspace(line[pos])) {
        pos++;
    }

    // Check if we've reached the end of the line
    if (pos >= read) {
        struct token_s *tok = create_token("\n");
        if (!tok) {
            fprintf(stderr, "error: failed to allocate token: %s\n", strerror(errno));
            return &eof_token;
        }
        tok->src = src;
        pos++; // Move past the newline
        return tok;
    }

    // Find the end of the token
    size_t start = pos;
    while (pos < read && !isspace(line[pos])) {
        pos++;
    }

    // Create the token
    char *token_text = strndup(line + start, pos - start);
    struct token_s *tok = create_token(token_text);
    free(token_text);

    if (!tok) {
        fprintf(stderr, "error: failed to allocate token: %s\n", strerror(errno));
        return &eof_token;
    }

    tok->src = src;
    return tok;
}

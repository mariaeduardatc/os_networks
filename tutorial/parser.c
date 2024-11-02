#include <unistd.h>
#include "shell.h"
#include "parser.h"
#include "scanner.h"
#include "node.h"
#include "source.h"

// builds a tree and each token is a child of the main node
struct node_s *parse_simple_command(struct token_s *tok)
{
    if(!tok)
    {
        return NULL;
    }
    
    struct node_s *cmd = new_node(NODE_COMMAND);
    if(!cmd)
    {
        free_token(tok);
        return NULL;
    }
    
    struct source_s *src = tok->src;
    
    do
    {
        if(tok->text[0] == '\n')
        {
            free_token(tok);
            break;
        }

        // creating a node for each word
        struct node_s *word = new_node(NODE_VAR);
        if(!word)
        {
            free_node_tree(cmd);
            free_token(tok);
            return NULL;
        }
        set_node_val_str(word, tok->text); // associates the text of the token with the word node
        add_child_node(cmd, word); // adds this word as a child to the main node

        free_token(tok); // freeing the memory alloc

    } while((tok = tokenize(src)) != &eof_token);

    return cmd; // returns the root node (what a terrible name "cmd")
}

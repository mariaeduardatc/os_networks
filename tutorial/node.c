#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include "shell.h"
#include "node.h"
#include "parser.h"


struct node_s *new_node(enum node_type_e type)
{
    struct node_s *node = malloc(sizeof(struct node_s));

    if(!node)
    {
        return NULL;
    }
    
    memset(node, 0, sizeof(struct node_s)); // sets all bytes of the newly allocated node_s to zero
    node->type = type; // setting type to the node field (understanding the syntax)
    
    return node;
}


void add_child_node(struct node_s *parent, struct node_s *child)
{
    if(!parent || !child)
    {
        return;
    }

    if(!parent->first_child)
    {
        parent->first_child = child;
    }
    else
    {
        struct node_s *sibling = parent->first_child;
    
    	while(sibling->next_sibling) // iteration to find the last child
        {
            sibling = sibling->next_sibling;
        }
    
    	sibling->next_sibling = child;
        child->prev_sibling = sibling;
    }
    parent->children++;
}

// set typing
void set_node_val_str(struct node_s *node, char *val)
{
    node->val_type = VAL_STR;

    if(!val)
    {
        node->val.str = NULL;
    }
    else
    {
        char *val2 = malloc(strlen(val)+1);
    
    	if(!val2)
        {
            node->val.str = NULL;
        }
        else
        {
            strcpy(val2, val);
            node->val.str = val2;
        }
    }
}

// ensures all the nodes allocated memory are freed
// traverses the tree touching upon all children
void free_node_tree(struct node_s *node)
{
    if(!node)
    {
        return;
    }

    struct node_s *child = node->first_child;
    
    while(child)
    {
        struct node_s *next = child->next_sibling;
        free_node_tree(child);
        child = next;
    }
    
    if(node->val_type == VAL_STR)
    {
        if(node->val.str)
        {
            free(node->val.str);
        }
    }
    free(node);
}
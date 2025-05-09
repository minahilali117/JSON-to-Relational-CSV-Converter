%{
/**
 * parser.y - JSON parser for json2relcsv using Yacc/Bison
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h"

// Lexer functions and variables
extern int yylex();
extern int line_num;
extern int col_num;
extern FILE* yyin;

// Error handling function
void yyerror(const char* s);

// Root of the AST
AST_Node* ast_root = NULL;

// Helper functions for building the AST
Pair_Node* create_pair_list(Pair_Node* first, Pair_Node* rest);
Value_Node* create_value_list(Value_Node first, Value_Node* rest, int* count);

%}

/* YYSTYPE union for passing values between lexer and parser */
%union {
    char* string_val;
    double number_val;
    int boolean_val;
    struct AST_Node* ast_node;
    struct Object_Node* object_node;
    struct Array_Node* array_node;
    struct Pair_Node* pair_node;
    struct Value_Node value_node;
    struct Value_Node* value_node_list;
}

/* Token definitions */
%token <string_val> STRING
%token <number_val> NUMBER
%token <boolean_val> BOOLEAN
%token NUL

/* Non-terminal types */
%type <ast_node> json
%type <value_node> value
%type <object_node> object
%type <array_node> array
%type <pair_node> pair pairs
%type <value_node_list> values
%type <value_node> array_value

/* Starting rule */
%start json

%%

json
    : value {
        ast_root = create_ast_node(NODE_OBJECT);
        
        switch ($1.type) {
            case VALUE_OBJECT:
                ast_root->type = NODE_OBJECT;
                ast_root->object = $1.object_val;
                break;
            case VALUE_ARRAY:
                ast_root->type = NODE_ARRAY;
                ast_root->object = (Object_Node*)$1.array_val;
                break;
            case VALUE_STRING:
                ast_root->type = NODE_STRING;
                ast_root->string_val = $1.string_val;
                break;
            case VALUE_NUMBER:
                ast_root->type = NODE_NUMBER;
                ast_root->number_val = $1.number_val;
                break;
            case VALUE_BOOLEAN:
                ast_root->type = NODE_BOOLEAN;
                ast_root->boolean_val = $1.boolean_val;
                break;
            case VALUE_NULL:
                ast_root->type = NODE_NULL;
                break;
        }
        
        $$ = ast_root;
    }
    ;

object
    : '{' '}' {
        $$ = create_object_node();
    }
    | '{' pairs '}' {
        $$ = create_object_node();
        
        /* Add all pairs to the object */
        Pair_Node* current = $2;
        while (current) {
            Pair_Node* next = current->next;
            current->next = NULL;
            add_pair_to_object($$, current);
            current = next;
        }
    }
    ;

pairs
    : pair {
        $$ = $1;
    }
    | pair ',' pairs {
        $1->next = $3;
        $$ = $1;
    }
    ;

pair
    : STRING ':' value {
        $$ = create_pair_node($1, $3);
    }
    ;

array
    : '[' ']' {
        $$ = create_array_node(0);
    }
    | '[' values ']' {
        /* Count number of values */
        int count = 0;
        Value_Node* current = $2;
        while (current) {
            count++;
            current = (Value_Node*)current->string_val; /* Reusing string_val as next pointer */
        }
        
        /* Create array node */
        $$ = create_array_node(count);
        
        /* Add values to array */
        current = $2;
        for (int i = 0; i < count; i++) {
            Value_Node* next = (Value_Node*)current->string_val;
            current->string_val = NULL; /* Clear the next pointer */
            add_element_to_array($$, i, *current);
            free(current); /* Free the temporary node */
            current = next;
        }
    }
    ;

values
    : array_value {
        Value_Node* node = malloc(sizeof(Value_Node));
        if (!node) {
            yyerror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }
        *node = $1;
        node->string_val = NULL; /* Will use string_val as next pointer */
        $$ = node;
    }
    | array_value ',' values {
        Value_Node* node = malloc(sizeof(Value_Node));
        if (!node) {
            yyerror("Memory allocation failed");
            exit(EXIT_FAILURE);
        }
        *node = $1;
        node->string_val = (char*)$3; /* Use string_val as next pointer */
        $$ = node;
    }
    ;

array_value
    : value {
        $$ = $1;
    }
    ;

value
    : object {
        $$ = create_object_value($1);
    }
    | array {
        $$ = create_array_value($1);
    }
    | STRING {
        $$ = create_string_value($1);
    }
    | NUMBER {
        $$ = create_number_value($1);
    }
    | BOOLEAN {
        $$ = create_boolean_value($1);
    }
    | NUL {
        $$ = create_null_value();
    }
    ;

%%

void yyerror(const char* s) {
    fprintf(stderr, "Error: %s at line %d, column %d\n", s, line_num, col_num);
    exit(EXIT_FAILURE);
}
%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast.h" // Assuming ast.h defines all AST node types and ValueType enum

// Lexer functions and variables
extern int yylex();
extern int line_num; // Ensure these are correctly defined and updated in scanner.l
extern int col_num;  // Ensure these are correctly defined and updated in scanner.l
extern FILE* yyin;

// Error handling function
void yyerror(const char* s);

// Root of the AST
AST_Node* ast_root = NULL;

// Helper structure for building a list of values in the parser
typedef struct ValueHolder {
    Value_Node value_item;      // The actual Value_Node (copied)
    struct ValueHolder* next;
} ValueHolder;

// Helper functions (can be defined below yyparse or in a separate .c file)
// Pair_Node* create_pair_list(Pair_Node* first, Pair_Node* rest); // Original, not used by current grammar for pairs
// Value_Node* create_value_list(Value_Node* first, Value_Node* rest, int* count); // Original, will be replaced by ValueHolder logic

%}

/* YYSTYPE union for passing values between lexer and parser */
%union {
    char* string_val;       // From STRING token
    double number_val;      // From NUMBER token
    int boolean_val;        // From BOOLEAN token (0 or 1)
    // AST specific structures
    struct AST_Node* ast_node;
    struct Object_Node* object_node;
    struct Array_Node* array_node;
    struct Pair_Node* pair_node;
    struct Value_Node* value_node_ptr; // For individual Value_Node* returned by 'value' and 'array_value' rules
    struct ValueHolder* value_holder_list; // For the list of values in an array
}

/* Token definitions */
%token <string_val> STRING
%token <number_val> NUMBER
%token <boolean_val> BOOLEAN
%token NUL // For JSON null

/* Non-terminal types */
%type <ast_node> json
%type <value_node_ptr> value        // Rule 'value' returns a pointer to a heap-allocated Value_Node
%type <object_node> object      // Rule 'object' returns an Object_Node*
%type <array_node> array        // Rule 'array' returns an Array_Node*
%type <pair_node> pair          // Rule 'pair' returns a Pair_Node*
%type <pair_node> pairs         // Rule 'pairs' returns a linked list of Pair_Node*
%type <value_holder_list> values // Rule 'values' returns a ValueHolder* list
%type <value_node_ptr> array_value // Rule 'array_value' is an alias for 'value' essentially

/* Starting rule */
%start json

%%

json
    : value { // $1 is a Value_Node* (heap-allocated by the 'value' rule)
        ast_root = create_ast_node(NODE_NULL); // Create a shell, type will be set
        if ($1 != NULL) {
            // Transfer ownership of contents from the Value_Node pointed to by $1
            // to ast_root. The ast_root itself is an AST_Node.
            switch ($1->type) {
                case VALUE_OBJECT:
                    ast_root->type = NODE_OBJECT;
                    ast_root->object = $1->object_val; // Transfer ownership of Object_Node*
                    $1->object_val = NULL; // Avoid double free if $1 is complexly freed later
                    break;
                case VALUE_ARRAY:
                    ast_root->type = NODE_ARRAY;
                    ast_root->array = $1->array_val;   // Transfer ownership of Array_Node*
                    $1->array_val = NULL;
                    break;
                case VALUE_STRING:
                    ast_root->type = NODE_STRING;
                    ast_root->string_val = $1->string_val; // Transfer ownership of char*
                    $1->string_val = NULL;
                    break;
                case VALUE_NUMBER:
                    ast_root->type = NODE_NUMBER;
                    ast_root->number_val = $1->number_val;
                    break;
                case VALUE_BOOLEAN:
                    ast_root->type = NODE_BOOLEAN;
                    ast_root->boolean_val = $1->boolean_val;
                    break;
                case VALUE_NULL:
                    ast_root->type = NODE_NULL;
                    break;
                default:
                    yyerror("Unknown value type in json rule");
                    // Potentially free $1 and ast_root before aborting
                    free($1);
                    free(ast_root); // or free_ast(ast_root) if partially populated
                    ast_root = NULL;
                    YYABORT;
            }
            free($1); // Free the Value_Node struct itself, its contents are now owned by ast_root
        } else {
            // This case should ideally not be reached if 'value' always returns a valid pointer or YYABORTs.
            // If it can be NULL, ast_root remains NODE_NULL or an empty default.
            // free(ast_root); // If $1 is NULL, ast_root might be an empty shell.
            // ast_root = create_ast_node(NODE_NULL); // Or ensure it's a valid empty AST
            yyerror("Internal error: value rule returned NULL to json rule");
            free(ast_root); ast_root = NULL; YYABORT;
        }
        $$ = ast_root;
    }
    ;

object
    : '{' '}' {
        $$ = create_object_node();
    }
    | '{' pairs '}' { // $2 is Pair_Node* (list of pairs)
        $$ = create_object_node();
        // Add pairs to the object. The 'pairs' rule returns a linked list.
        Pair_Node* current_pair_item = $2;
        while (current_pair_item) {
            Pair_Node* next_pair_item = current_pair_item->next;
            current_pair_item->next = NULL; // Detach from list before adding
            add_pair_to_object($$, current_pair_item);
            current_pair_item = next_pair_item;
        }
        // The Pair_Node items themselves are now owned by the Object_Node.
        // The list structure of $2 is consumed.
    }
    ;

pairs
    : pair { // $1 is Pair_Node*
        $$ = $1;
        $$->next = NULL; // Ensure it's a single-item list initially
    }
    | pair ',' pairs { // $1 is Pair_Node*, $3 is Pair_Node* (rest of the list)
        $1->next = $3; // Prepend $1 to the list $3
        $$ = $1;
    }
    ;

pair
    : STRING ':' value { // $1 is char* (key), $3 is Value_Node* (value wrapper)
        // create_pair_node takes (char* key, Value_Node value_struct).
        // It does NOT take Value_Node*. So we dereference $3.
        // The key ($1) is from yylval.string_val, processed by scanner's process_string (heap).
        // The Value_Node from $3 contains the actual data (e.g., Object_Node*, char*).
        $$ = create_pair_node($1, *$3); // $1 (key) is now owned by Pair_Node.
                                        // Contents of *$3 are copied or pointers transferred by create_string/object/array_value logic.
        free($3); // Free the Value_Node struct wrapper pointed to by $3. Its contents are now part of the Pair_Node.
    }
    ;

array
    : '[' ']' {
        $$ = create_array_node(0);
    }
    | '[' values ']' { // $2 is ValueHolder* (linked list of ValueHolder)
        int count = 0;
        ValueHolder* current_vh = $2;
        while (current_vh) {
            count++;
            current_vh = current_vh->next;
        }

        $$ = create_array_node(count); // $$ is Array_Node*
        current_vh = $2; // Reset to head of list

        // The list built by 'values' rule is in reverse parse order.
        // Example: [a, b, c] -> values rule: c -> b -> a (holder(a) is head)
        // So, to fill array elements in correct order [a,b,c], iterate list and fill from 0 to count-1
        // OR, fill from count-1 down to 0.
        // Let's assume 'values' rule: value ',' values results in $1 (new head) -> $3 (tail)
        // So, [a,b,c] -> holder(a) -> holder(b) -> holder(c). Iterate normally.

        for (int i = 0; i < count; i++) {
            if (!current_vh) { // Should not happen if count is correct
                yyerror("Internal error: Mismatch in array value count");
                // Free $$ (Array_Node) and any already added elements if possible
                // For simplicity, YYABORT. Proper cleanup is complex here.
                // free_ast( (AST_Node*) $$ ); // This is wrong, $$ is Array_Node*
                // Need to free Array_Node $$ and its elements if partially filled.
                // For now, rely on higher level free_ast(ast_root) on error exit.
                YYABORT;
            }
            // add_element_to_array takes (Array_Node*, int index, Value_Node value_struct)
            // current_vh->value_item is a Value_Node struct.
            add_element_to_array($$, i, current_vh->value_item); 
            // The contents of current_vh->value_item (like char* for string, Object_Node*)
            // are now "owned" by the Array_Node due to the copy/transfer in add_element_to_array.
            // Or rather, add_element_to_array copies the Value_Node struct. If that struct contains
            // pointers (string_val, object_val, array_val), those pointers are copied.
            // The actual data pointed to (the string, the object, the array) must have its ownership
            // correctly managed. The Value_Node itself (current_vh->value_item) is on the ValueHolder's stack/struct.

            ValueHolder* temp_vh = current_vh;
            current_vh = current_vh->next;
            // We must NOT free contents of temp_vh->value_item here if they were transferred to Array_Node.
            // The Value_Node structs created by 'value' rule are copied into ValueHolder.
            // The original Value_Node* from 'value' rule was freed when creating the ValueHolder.
            // So, temp_vh->value_item's contents (pointers to string/object/array data) are the ones
            // that are now also pointed to by the Array_Node.
            // This is fine as long as add_element_to_array correctly handles the Value_Node struct copy.
            // The ast.c create_xxx_value functions ensure the Value_Node struct is properly formed.
            free(temp_vh); // Free the ValueHolder list node itself.
        }
        if (current_vh != NULL) { // Should be NULL if all processed
             yyerror("Internal error: Array values list not fully processed.");
             // Free remaining ValueHolder items to prevent leaks
             while(current_vh) {
                 ValueHolder* temp_vh = current_vh;
                 current_vh = current_vh->next;
                 // Important: If value_item contains heap data (string, object, array)
                 // that was NOT successfully transferred to the final AST (e.g. due to error),
                 // it needs to be freed here. However, with current logic, they *are* transferred.
                 // So, only free the holder.
                 // free_value_node_contents(&temp_vh->value_item); // ONLY if data not transferred
                 free(temp_vh);
             }
        }
    }
    ;

values  // This rule returns a ValueHolder* list, in PARSED order (head is first item parsed)
    : array_value { // $1 is Value_Node* (heap-allocated wrapper)
        ValueHolder* vh = (ValueHolder*)malloc(sizeof(ValueHolder));
        if (!vh) { yyerror("malloc failed for ValueHolder"); YYABORT; }
        vh->value_item = *$1; // Copy the Value_Node struct
        vh->next = NULL;
        $$ = vh;
        free($1); // Free the Value_Node* wrapper, its contents are now in vh->value_item
    }
    | values ',' array_value { // $1 is ValueHolder* (list so far), $3 is Value_Node* (new item wrapper)
        ValueHolder* new_vh = (ValueHolder*)malloc(sizeof(ValueHolder));
        if (!new_vh) { yyerror("malloc failed for new ValueHolder"); YYABORT; }
        new_vh->value_item = *$3; // Copy the Value_Node struct
        new_vh->next = NULL;

        // Append new_vh to the end of the list $1
        ValueHolder* current_vh = $1;
        while(current_vh->next != NULL) {
            current_vh = current_vh->next;
        }
        current_vh->next = new_vh;
        $$ = $1; // Return the original head of the list
        free($3); // Free the Value_Node* wrapper
    }
    ;

array_value // This is just an alias for 'value' in the context of an array
    : value { // $1 is Value_Node*
        $$ = $1; // Pass through the Value_Node*
    }
    ;

value   // This rule returns a pointer to a NEWLY HEAP-ALLOCATED Value_Node
    : object { // $1 is Object_Node*
        Value_Node* vn = (Value_Node*)malloc(sizeof(Value_Node));
        if (!vn) { yyerror("malloc failed for Value_Node (object)"); YYABORT; }
        *vn = create_object_value($1); // create_object_value returns a Value_Node struct
        $$ = vn;
    }
    | array { // $1 is Array_Node*
        Value_Node* vn = (Value_Node*)malloc(sizeof(Value_Node));
        if (!vn) { yyerror("malloc failed for Value_Node (array)"); YYABORT; }
        *vn = create_array_value($1);
        $$ = vn;
    }
    | STRING { // $1 is char* (heap-allocated by scanner's process_string)
        Value_Node* vn = (Value_Node*)malloc(sizeof(Value_Node));
        if (!vn) { yyerror("malloc failed for Value_Node (string)"); free($1); YYABORT; }
        *vn = create_string_value($1); // $1 (char*) is now owned by this Value_Node's contents
        $$ = vn;
    }
    | NUMBER { // $1 is double
        Value_Node* vn = (Value_Node*)malloc(sizeof(Value_Node));
        if (!vn) { yyerror("malloc failed for Value_Node (number)"); YYABORT; }
        *vn = create_number_value($1);
        $$ = vn;
    }
    | BOOLEAN { // $1 is int (0 or 1)
        Value_Node* vn = (Value_Node*)malloc(sizeof(Value_Node));
        if (!vn) { yyerror("malloc failed for Value_Node (boolean)"); YYABORT; }
        *vn = create_boolean_value($1); // $1 is a simple int, copied
        $$ = vn;
    }
    | NUL {
        Value_Node* vn = (Value_Node*)malloc(sizeof(Value_Node));
        if (!vn) { yyerror("malloc failed for Value_Node (null)"); YYABORT; }
        *vn = create_null_value();
        $$ = vn;
    }
    ;

%%

void yyerror(const char* s) {
    // Make sure line_num and col_num are accurately updated by the lexer for all tokens
    fprintf(stderr, "Parser Error: %s at line %d, column %d\n", s, line_num, col_num);
    // Consider a more graceful exit or error recovery if this is part of a larger system.
    // For a standalone tool, exit is common.
    // exit(EXIT_FAILURE); // YYABORT might be preferred within parser actions
}

// If create_pair_list and create_value_list were used, their definitions would go here.
// Since they are not used by the revised grammar, they can be removed.

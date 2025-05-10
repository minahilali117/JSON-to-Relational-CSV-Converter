#include "ast.h" // Using the provided ast.h

// Create a new AST node of given type
AST_Node* create_ast_node(NodeType type) {
    // Use calloc to initialize memory to zero, which helps with unions
    // ensuring pointers are NULL and primitive types are zero initially.
    AST_Node* node = (AST_Node*)calloc(1, sizeof(AST_Node));
    if (!node) {
        fprintf(stderr, "Error: Memory allocation failed for AST node\n");
        exit(EXIT_FAILURE);
    }
    node->type = type;
    // Specific initializations if calloc isn't sufficient (though it usually is for this)
    // For example, if a non-zero default was needed for a union member:
    // switch (type) {
    //     case NODE_NUMBER: node->number_val = 0.0; break;
    //     // etc.
    // }
    return node;
}

// Create a new object node
Object_Node* create_object_node() {
    Object_Node* obj = (Object_Node*)calloc(1, sizeof(Object_Node));
    if (!obj) {
        fprintf(stderr, "Error: Memory allocation failed for Object node\n");
        exit(EXIT_FAILURE);
    }
    // obj->pairs, obj->pair_count, obj->node_id, obj->next are zeroed by calloc
    return obj;
}

// Create a new array node with given size
Array_Node* create_array_node(int size) {
    Array_Node* arr = (Array_Node*)calloc(1, sizeof(Array_Node));
    if (!arr) {
        fprintf(stderr, "Error: Memory allocation failed for Array node\n");
        exit(EXIT_FAILURE);
    }
    arr->size = size;
    if (size > 0) {
        // Use calloc for elements to initialize them properly (e.g. type to 0, pointers to NULL)
        arr->elements = (Value_Node*)calloc(size, sizeof(Value_Node));
        if (!arr->elements) {
            fprintf(stderr, "Error: Memory allocation failed for Array elements\n");
            free(arr);
            exit(EXIT_FAILURE);
        }
        // Initialize elements' type to VALUE_NULL by default if desired,
        // though calloc sets type to 0 (which might or might not map to VALUE_NULL)
        // It's safer to explicitly set the type for each element if 0 isn't VALUE_NULL
        for (int i = 0; i < size; i++) {
             arr->elements[i].type = VALUE_NULL; // Explicitly set type
        }
    } else {
        arr->elements = NULL; // For zero-size arrays
    }
    return arr;
}

// Create a string value
Value_Node create_string_value(char* value) {
    Value_Node val_node;
    val_node.type = VALUE_STRING;
    val_node.string_val = value; // Assumes 'value' is already heap-allocated (e.g. strdup)
    return val_node;
}

// Create a number value
Value_Node create_number_value(double value) {
    Value_Node val_node;
    val_node.type = VALUE_NUMBER;
    val_node.number_val = value;
    return val_node;
}

// Create a boolean value
Value_Node create_boolean_value(bool value) {
    Value_Node val_node;
    val_node.type = VALUE_BOOLEAN;
    val_node.boolean_val = value;
    return val_node;
}

// Create a null value
Value_Node create_null_value() {
    Value_Node val_node;
    val_node.type = VALUE_NULL;
    // For anonymous union, no specific member needs to be set for NULL type.
    // If we wanted to be extremely explicit, we could zero a part of it:
    // val_node.string_val = NULL; // or any pointer member
    return val_node;
}

// Create an object value
Value_Node create_object_value(Object_Node* obj) {
    Value_Node val_node;
    val_node.type = VALUE_OBJECT;
    val_node.object_val = obj;
    return val_node;
}

// Create an array value
Value_Node create_array_value(Array_Node* arr) {
    Value_Node val_node;
    val_node.type = VALUE_ARRAY;
    val_node.array_val = arr;
    return val_node;
}

// Create a key-value pair node
Pair_Node* create_pair_node(char* key, Value_Node value_node) {
    Pair_Node* pair = (Pair_Node*)calloc(1, sizeof(Pair_Node));
    if (!pair) {
        fprintf(stderr, "Error: Memory allocation failed for Pair node\n");
        exit(EXIT_FAILURE);
    }
    pair->key = key; // Assumes 'key' is already heap-allocated (e.g. strdup)
    pair->value = value_node;
    // pair->next is NULL due to calloc
    return pair;
}

// Add a pair to an object
void add_pair_to_object(Object_Node* obj, Pair_Node* pair) {
    if (!obj || !pair) return;
    if (!obj->pairs) {
        obj->pairs = pair;
    } else {
        Pair_Node* current = obj->pairs;
        while (current->next) {
            current = current->next;
        }
        current->next = pair;
    }
    obj->pair_count++;
}

// Add an element to an array at given index
void add_element_to_array(Array_Node* arr, int index, Value_Node value_node) {
    if (!arr || !arr->elements || index < 0 || index >= arr->size) {
        fprintf(stderr, "Error: Array index out of bounds or null array/elements in add_element_to_array\n");
        // Consider how to handle this error. Exiting might be too drastic.
        // For now, to prevent crashes if called incorrectly:
        if (arr && arr->elements && index >=0 && index < arr->size) {
            // If we are overwriting, we should ideally free the existing element's contents first.
            // This function is typically used during parsing for initial population.
            // If used for updates, the caller needs to manage memory of the old element.
        } else {
             // exit(EXIT_FAILURE); // Or handle error more gracefully
             return; // Avoid crash
        }
    }
    // Before assigning, if arr->elements[index] already holds dynamic content, it should be freed.
    // Assuming this function is for initial build or the caller handles prior content.
    // free_value_node_contents(&arr->elements[index]); // If overwriting, free old content first.

    arr->elements[index] = value_node;
}


// --- Forward declarations for recursive freeing ---
static void free_value_node_contents(Value_Node* value_node);
static void free_object_node_contents(Object_Node* obj_node); // Frees contents, not obj_node itself
static void free_array_node_contents(Array_Node* arr_node);   // Frees contents, not arr_node itself

// --- Implementation of freeing functions ---

static void free_value_node_contents(Value_Node* value_node) {
    if (!value_node) return;

    switch (value_node->type) {
        case VALUE_STRING:
            if (value_node->string_val) {
                free(value_node->string_val);
                value_node->string_val = NULL;
            }
            break;
        case VALUE_OBJECT:
            if (value_node->object_val) {
                free_object_node_contents(value_node->object_val); // Free pairs within the object
                free(value_node->object_val);                     // Free the Object_Node struct itself
                value_node->object_val = NULL;
            }
            break;
        case VALUE_ARRAY:
            if (value_node->array_val) {
                free_array_node_contents(value_node->array_val); // Free elements within the array
                free(value_node->array_val);                    // Free the Array_Node struct itself
                value_node->array_val = NULL;
            }
            break;
        case VALUE_NUMBER:
        case VALUE_BOOLEAN:
        case VALUE_NULL:
            // No dynamic memory owned directly by these value types' payload
            break;
        default:
            // This case should ideally not be reached if types are handled correctly.
            // fprintf(stderr, "Warning: Unknown value type in free_value_node_contents: %d\n", value_node->type);
            break;
    }
    // Reset type to prevent accidental reuse, or mark as invalid
    // value_node->type = VALUE_NULL; // Or some other indicator
}

// Frees the *contents* of an Object_Node (pairs, keys, values)
// Does NOT free the Object_Node struct itself, as that's handled by its owner
// (either an AST_Node or a Value_Node).
static void free_object_node_contents(Object_Node* obj_node) {
    if (!obj_node) return;

    Pair_Node* current_pair = obj_node->pairs;
    while (current_pair) {
        Pair_Node* next_pair = current_pair->next;

        if (current_pair->key) {
            free(current_pair->key); 
            current_pair->key = NULL;
        }
        // Recursively free the contents of the value associated with the pair
        free_value_node_contents(&(current_pair->value));

        free(current_pair); // Free the pair node struct itself
        current_pair = next_pair;
    }
    obj_node->pairs = NULL; 
    obj_node->pair_count = 0;

    // The `Object_Node* next` is for linking objects with the same schema.
    // It's assumed that this list is managed and freed elsewhere (e.g., by schema logic),
    // not as part of freeing a single object's direct contents.
    // If an Object_Node *owned* its `next` successor in a simple linked list sense for AST purposes,
    // then you would recursively free it here. But given the comment in ast.h, it's likely not the case.
    // if (obj_node->next) {
    //     free_object_node_contents(obj_node->next); // Free its contents
    //     free(obj_node->next);                      // Free the next Object_Node struct
    //     obj_node->next = NULL;
    // }
}

// Frees the *contents* of an Array_Node (elements and their values)
// Does NOT free the Array_Node struct itself.
static void free_array_node_contents(Array_Node* arr_node) {
    if (!arr_node) return;

    if (arr_node->elements) {
        for (int i = 0; i < arr_node->size; i++) {
            // Recursively free the contents of each element in the array
            free_value_node_contents(&(arr_node->elements[i]));
        }
        free(arr_node->elements); // Free the C-array holding Value_Node structs
        arr_node->elements = NULL;
    }
    arr_node->size = 0;
}

// Main function to free the entire AST
void free_ast(AST_Node* root) {
    if (!root) return;

    switch (root->type) {
        case NODE_OBJECT:
            if (root->object) { // Accessing anonymous union member directly
                free_object_node_contents(root->object); // Free its internal pairs, keys, values
                free(root->object);                      // Free the Object_Node struct itself
                root->object = NULL;
            }
            break;
        case NODE_ARRAY:
            if (root->array) { // Accessing anonymous union member directly
                free_array_node_contents(root->array);  // Free its internal elements and their values
                free(root->array);                      // Free the Array_Node struct itself
                root->array = NULL;
            }
            break;
        case NODE_STRING:
            if (root->string_val) { // Accessing anonymous union member directly
                free(root->string_val); 
                root->string_val = NULL;
            }
            break;
        case NODE_NUMBER:
        case NODE_BOOLEAN:
        case NODE_NULL:
            // No dynamic memory associated directly with the AST_Node's union part for these types
            break;
        default:
            // fprintf(stderr, "Warning: Unknown AST node type in free_ast: %d\n", root->type);
            break;
    }
    free(root); // Finally, free the AST_Node shell itself
}

// Helper function to print a Value_Node for print_ast
// This avoids creating temporary AST_Node wrappers in print_ast
static void print_value_node(Value_Node* val, int indent, const char* indent_str) {
    if (!val) {
        printf("null_value_ptr"); // Should not happen if called correctly
        return;
    }
    switch (val->type) {
        case VALUE_STRING:
            // Ensure string_val is not NULL before printing, or print "(null)"
            printf("\"%s\"", val->string_val ? val->string_val : "(null)");
            break;
        case VALUE_NUMBER:
            printf("%g", val->number_val);
            break;
        case VALUE_BOOLEAN:
            printf("%s", val->boolean_val ? "true" : "false");
            break;
        case VALUE_NULL:
            printf("null");
            break;
        case VALUE_OBJECT: {
            // Temporarily wrap Object_Node in an AST_Node for recursive print_ast call
            AST_Node temp_node;
            temp_node.type = NODE_OBJECT;
            temp_node.object = val->object_val; // Direct anonymous union access
            print_ast(&temp_node, indent); // Note: print_ast handles its own newlines for {}
            break;
        }
        case VALUE_ARRAY: {
            // Temporarily wrap Array_Node in an AST_Node for recursive print_ast call
            AST_Node temp_node;
            temp_node.type = NODE_ARRAY;
            temp_node.array = val->array_val; // Direct anonymous union access
            print_ast(&temp_node, indent); // Note: print_ast handles its own newlines for []
            break;
        }
        default:
            printf("UNKNOWN_VALUE_TYPE");
    }
}


// Print the AST (for --print-ast option)
void print_ast(AST_Node* root, int indent) {
    if (!root) return;

    char* current_indent_str = (char*)malloc((indent * 2 + 1) * sizeof(char));
    if (!current_indent_str) {
        fprintf(stderr, "Error: Memory allocation failed for indent string in print_ast\n");
        // Fallback or exit
        strcpy(current_indent_str, ""); // No indent on error
    } else {
        for (int i = 0; i < indent * 2; i++) {
            current_indent_str[i] = ' ';
        }
        current_indent_str[indent * 2] = '\0';
    }

    switch (root->type) {
        case NODE_OBJECT: {
            // For an object directly in AST_Node, or an object value being printed recursively
            if (indent > 0 && root->object != NULL ) printf("\n%s", current_indent_str); // Newline before nested object starts
            else if (root->object == NULL && indent > 0) printf("\n%s", current_indent_str);


            printf("{");
            Object_Node* obj = root->object; // Direct anonymous union access
            if (obj && obj->pairs) {
                 printf("\n"); // Newline after '{' if there are pairs
                Pair_Node* current_pair = obj->pairs;
                while (current_pair) {
                    printf("%s  \"%s\": ", current_indent_str, current_pair->key ? current_pair->key : "null_key");
                    
                    // Use helper to print the value node
                    // Pass indent + 1 for the value's own potential nesting
                    print_value_node(&current_pair->value, indent + 1, current_indent_str);

                    if (current_pair->next) {
                        printf(",\n");
                    } else {
                        printf("\n");
                    }
                    current_pair = current_pair->next;
                }
                printf("%s}", current_indent_str);
            } else {
                 printf("}"); // Empty object {}
            }
            break;
        }
        case NODE_ARRAY: {
             if (indent > 0 && root->array != NULL) printf("\n%s", current_indent_str);
             else if (root->array == NULL && indent > 0) printf("\n%s", current_indent_str);


            printf("[");
            Array_Node* arr = root->array; // Direct anonymous union access
            if (arr && arr->elements && arr->size > 0) {
                printf("\n"); // Newline after '[' if there are elements
                for (int i = 0; i < arr->size; i++) {
                    printf("%s  ", current_indent_str); // Indent for element
                    
                    // Use helper to print the value node
                    // Pass indent + 1 for the value's own potential nesting
                    print_value_node(&arr->elements[i], indent + 1, current_indent_str);
                    
                    if (i < arr->size - 1) {
                        printf(",\n");
                    } else {
                        printf("\n");
                    }
                }
                printf("%s]", current_indent_str);
            } else {
                printf("]"); // Empty array []
            }
            break;
        }
        // These cases are for when an AST_Node directly holds a primitive type
        // (e.g., if the root of the JSON is just a string or number).
        // If print_ast is only ever called with NODE_OBJECT or NODE_ARRAY at the top level,
        // these might not be strictly necessary here but are good for completeness.
        case NODE_STRING:
            printf("\"%s\"", root->string_val ? root->string_val : "(null)");
            break;
        case NODE_NUMBER:
            printf("%g", root->number_val);
            break;
        case NODE_BOOLEAN:
            printf("%s", root->boolean_val ? "true" : "false");
            break;
        case NODE_NULL:
            printf("null");
            break;
        default:
             printf("UNKNOWN_NODE_TYPE_IN_PRINT_AST");
             break;
    }

    free(current_indent_str);
}

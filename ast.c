/**
 * ast.c - Abstract Syntax Tree implementation for json2relcsv
 */

 #include "ast.h"

 // Create a new AST node of given type
 AST_Node* create_ast_node(NodeType type) {
     AST_Node* node = (AST_Node*)malloc(sizeof(AST_Node));
     if (!node) {
         fprintf(stderr, "Error: Memory allocation failed for AST node\n");
         exit(EXIT_FAILURE);
     }
     node->type = type;
     return node;
 }
 
 // Create a new object node
 Object_Node* create_object_node() {
     Object_Node* obj = (Object_Node*)malloc(sizeof(Object_Node));
     if (!obj) {
         fprintf(stderr, "Error: Memory allocation failed for Object node\n");
         exit(EXIT_FAILURE);
     }
     obj->pairs = NULL;
     obj->pair_count = 0;
     obj->node_id = 0;  // Will be assigned during schema generation
     obj->next = NULL;
     return obj;
 }
 
 // Create a new array node with given size
 Array_Node* create_array_node(int size) {
     Array_Node* arr = (Array_Node*)malloc(sizeof(Array_Node));
     if (!arr) {
         fprintf(stderr, "Error: Memory allocation failed for Array node\n");
         exit(EXIT_FAILURE);
     }
     arr->size = size;
     arr->elements = (Value_Node*)malloc(size * sizeof(Value_Node));
     if (!arr->elements) {
         fprintf(stderr, "Error: Memory allocation failed for Array elements\n");
         free(arr);
         exit(EXIT_FAILURE);
     }
     return arr;
 }
 
 // Create a string value
 Value_Node create_string_value(char* value) {
     Value_Node val;
     val.type = VALUE_STRING;
     val.string_val = value;
     return val;
 }
 
 // Create a number value
 Value_Node create_number_value(double value) {
     Value_Node val;
     val.type = VALUE_NUMBER;
     val.number_val = value;
     return val;
 }
 
 // Create a boolean value
 Value_Node create_boolean_value(bool value) {
     Value_Node val;
     val.type = VALUE_BOOLEAN;
     val.boolean_val = value;
     return val;
 }
 
 // Create a null value
 Value_Node create_null_value() {
     Value_Node val;
     val.type = VALUE_NULL;
     return val;
 }
 
 // Create an object value
 Value_Node create_object_value(Object_Node* obj) {
     Value_Node val;
     val.type = VALUE_OBJECT;
     val.object_val = obj;
     return val;
 }
 
 // Create an array value
 Value_Node create_array_value(Array_Node* arr) {
     Value_Node val;
     val.type = VALUE_ARRAY;
     val.array_val = arr;
     return val;
 }
 
 // Create a key-value pair node
 Pair_Node* create_pair_node(char* key, Value_Node value) {
     Pair_Node* pair = (Pair_Node*)malloc(sizeof(Pair_Node));
     if (!pair) {
         fprintf(stderr, "Error: Memory allocation failed for Pair node\n");
         exit(EXIT_FAILURE);
     }
     pair->key = key;
     pair->value = value;
     pair->next = NULL;
     return pair;
 }
 
 // Add a pair to an object
 void add_pair_to_object(Object_Node* obj, Pair_Node* pair) {
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
 void add_element_to_array(Array_Node* arr, int index, Value_Node value) {
     if (index >= 0 && index < arr->size) {
         arr->elements[index] = value;
     } else {
         fprintf(stderr, "Error: Array index out of bounds\n");
         exit(EXIT_FAILURE);
     }
 }
 
 // Free the AST
 void free_ast(AST_Node* root) {
     if (!root) return;
     
     switch (root->type) {
         case NODE_OBJECT: {
             Object_Node* obj = root->object;
             if (obj) {
                 Pair_Node* current = obj->pairs;
                 while (current) {
                     Pair_Node* next = current->next;
                     
                     // Free the key
                     if (current->key) {
                         free(current->key);
                     }
                     
                     // Free the value based on its type
                     switch (current->value.type) {
                         case VALUE_STRING:
                             if (current->value.string_val) {
                                 free(current->value.string_val);
                             }
                             break;
                         case VALUE_OBJECT: {
                             AST_Node temp;
                             temp.type = NODE_OBJECT;
                             temp.object = current->value.object_val;
                             free_ast(&temp);
                             break;
                         }
                         case VALUE_ARRAY: {
                             AST_Node temp;
                             temp.type = NODE_ARRAY;
                             temp.object = (Object_Node*)current->value.array_val;
                             free_ast(&temp);
                             break;
                         }
                         default:
                             break;
                     }
                     
                     free(current);
                     current = next;
                 }
                 free(obj);
             }
             break;
         }
         case NODE_ARRAY: {
             Array_Node* arr = (Array_Node*)root->object;
             if (arr) {
                 for (int i = 0; i < arr->size; i++) {
                     Value_Node val = arr->elements[i];
                     switch (val.type) {
                         case VALUE_STRING:
                             if (val.string_val) {
                                 free(val.string_val);
                             }
                             break;
                         case VALUE_OBJECT: {
                             AST_Node temp;
                             temp.type = NODE_OBJECT;
                             temp.object = val.object_val;
                             free_ast(&temp);
                             break;
                         }
                         case VALUE_ARRAY: {
                             AST_Node temp;
                             temp.type = NODE_ARRAY;
                             temp.object = (Object_Node*)val.array_val;
                             free_ast(&temp);
                             break;
                         }
                         default:
                             break;
                     }
                 }
                 if (arr->elements) {
                     free(arr->elements);
                 }
                 free(arr);
             }
             break;
         }
         case NODE_STRING:
             if (root->string_val) {
                 free(root->string_val);
             }
             break;
         default:
             break;
     }
     
     free(root);
 }
 
 // Print the AST (for --print-ast option)
 void print_ast(AST_Node* root, int indent) {
     if (!root) return;
     
     char* indent_str = (char*)malloc((indent * 2 + 1) * sizeof(char));
     if (!indent_str) {
         fprintf(stderr, "Error: Memory allocation failed for indent string\n");
         exit(EXIT_FAILURE);
     }
     
     for (int i = 0; i < indent * 2; i++) {
         indent_str[i] = ' ';
     }
     indent_str[indent * 2] = '\0';
     
     switch (root->type) {
         case NODE_OBJECT: {
             printf("%s{\n", indent_str);
             Object_Node* obj = root->object;
             if (obj) {
                 Pair_Node* current = obj->pairs;
                 while (current) {
                     printf("%s  \"%s\": ", indent_str, current->key);
                     
                     switch (current->value.type) {
                         case VALUE_STRING:
                             printf("\"%s\"", current->value.string_val);
                             break;
                         case VALUE_NUMBER:
                             printf("%g", current->value.number_val);
                             break;
                         case VALUE_BOOLEAN:
                             printf("%s", current->value.boolean_val ? "true" : "false");
                             break;
                         case VALUE_NULL:
                             printf("null");
                             break;
                         case VALUE_OBJECT: {
                             AST_Node temp;
                             temp.type = NODE_OBJECT;
                             temp.object = current->value.object_val;
                             print_ast(&temp, indent + 1);
                             goto skip_newline; // Skip the newline after printing the object
                         }
                         case VALUE_ARRAY: {
                             AST_Node temp;
                             temp.type = NODE_ARRAY;
                             temp.object = (Object_Node*)current->value.array_val;
                             print_ast(&temp, indent + 1);
                             goto skip_newline; // Skip the newline after printing the array
                         }
                     }
                     
                     printf("%s", current->next ? ",\n" : "\n");
                 skip_newline:
                     current = current->next;
                 }
             }
             printf("%s}", indent_str);
             break;
         }
         case NODE_ARRAY: {
             printf("%s[\n", indent_str);
             Array_Node* arr = (Array_Node*)root->object;
             if (arr) {
                 for (int i = 0; i < arr->size; i++) {
                     Value_Node val = arr->elements[i];
                     printf("%s  ", indent_str);
                     
                     switch (val.type) {
                         case VALUE_STRING:
                             printf("\"%s\"", val.string_val);
                             break;
                         case VALUE_NUMBER:
                             printf("%g", val.number_val);
                             break;
                         case VALUE_BOOLEAN:
                             printf("%s", val.boolean_val ? "true" : "false");
                             break;
                         case VALUE_NULL:
                             printf("null");
                             break;
                         case VALUE_OBJECT: {
                             AST_Node temp;
                             temp.type = NODE_OBJECT;
                             temp.object = val.object_val;
                             print_ast(&temp, indent + 1);
                             goto skip_array_newline; // Skip the newline after printing the object
                         }
                         case VALUE_ARRAY: {
                             AST_Node temp;
                             temp.type = NODE_ARRAY;
                             temp.object = (Object_Node*)val.array_val;
                             print_ast(&temp, indent + 1);
                             goto skip_array_newline; // Skip the newline after printing the array
                         }
                     }
                     
                     printf("%s", i < arr->size - 1 ? ",\n" : "\n");
                 skip_array_newline:
                     continue;
                 }
             }
             printf("%s]", indent_str);
             break;
         }
         case NODE_STRING:
             printf("\"%s\"", root->string_val);
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
     }
     
     free(indent_str);
 }
/**
 * ast.h - Abstract Syntax Tree definitions for json2relcsv
 */

 #ifndef AST_H
 #define AST_H
 
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <stdbool.h>
 
 // Forward declarations
 struct AST_Node;
 struct Object_Node;
 struct Array_Node;
 struct Pair_Node;
 struct Value_Node;
 
// Value types
typedef enum {
    VALUE_STRING,
    VALUE_NUMBER,
    VALUE_BOOLEAN,
    VALUE_NULL,
    VALUE_OBJECT,
    VALUE_ARRAY
} ValueType;

// JSON value structure
typedef struct Value_Node {
    ValueType type;
    union {
        char* string_val;
        double number_val;
        bool boolean_val;
        struct Object_Node* object_val;
        struct Array_Node* array_val;
    };
} Value_Node;


 // AST node types
 typedef enum {
     NODE_OBJECT,
     NODE_ARRAY,
     NODE_STRING,
     NODE_NUMBER,
     NODE_BOOLEAN,
     NODE_NULL
 } NodeType;
 
 // Key-value pair structure
 typedef struct Pair_Node {
     char* key;
     Value_Node value;
     struct Pair_Node* next;
 } Pair_Node;
 
 // JSON object structure
 typedef struct Object_Node {
     Pair_Node* pairs;
     int pair_count;
     int node_id;              // Used for primary key in CSV
     struct Object_Node* next; // Used for linking objects with same schema
 } Object_Node;
 
 // JSON array structure
 typedef struct Array_Node {
     Value_Node* elements;
     int size;
 } Array_Node;
 
 // Root AST node structure
 typedef struct AST_Node {
     NodeType type;
     union {
         Object_Node* object;
         Array_Node* array;
         char* string_val;
         double number_val;
         bool boolean_val;
     };
 } AST_Node;
 
 // Function declarations
 AST_Node* create_ast_node(NodeType type);
 Object_Node* create_object_node();
 Array_Node* create_array_node(int size);
 Value_Node create_string_value(char* value);
 Value_Node create_number_value(double value);
 Value_Node create_boolean_value(bool value);
 Value_Node create_null_value();
 Value_Node create_object_value(Object_Node* obj);
 Value_Node create_array_value(Array_Node* arr);
 Pair_Node* create_pair_node(char* key, Value_Node value);
 void add_pair_to_object(Object_Node* obj, Pair_Node* pair);
 void add_element_to_array(Array_Node* arr, int index, Value_Node value);
 void free_ast(AST_Node* root);
 void print_ast(AST_Node* root, int indent);
 
 // Table schema structures
 typedef struct {
     char* name;
     char** columns;
     int column_count;
     Object_Node* objects;  // Objects with same schema
 } TableSchema;
 
 typedef struct {
     TableSchema* tables;
     int table_count;
 } Schema;
 
 // Schema functions
 Schema* generate_schema(AST_Node* root);
 void free_schema(Schema* schema);
 void write_csv_files(Schema* schema, const char* out_dir);
 
 #endif /* AST_H */
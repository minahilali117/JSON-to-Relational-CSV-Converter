/**
 * schema.c - Schema generation for json2relcsv
 */

 #include "ast.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <stdbool.h>
 
 // Tables collection for schema generation
 typedef struct TableCollection {
     TableSchema* tables;
     int table_count;
     int capacity;
 } TableCollection;
 
 // Key list for comparing object schemas
 typedef struct KeyList {
     char** keys;
     int count;
 } KeyList;
 
 // Forward declarations of internal functions
 static void collect_table_schemas(AST_Node* root, TableCollection* tables, char* parent_table_name, int parent_id);
 static TableSchema* find_or_create_table(TableCollection* tables, const char* name, KeyList* keys);
 static KeyList* collect_object_keys(Object_Node* obj);
 static bool compare_key_lists(KeyList* list1, KeyList* list2);
 static void free_key_list(KeyList* list);
 static char* get_table_name_for_array(const char* parent_name, const char* key);
 static void add_object_to_table(TableSchema* table, Object_Node* obj);
 static void process_object_node(AST_Node* node, TableCollection* tables, char* parent_table_name, int parent_id);
 static void process_array_node(AST_Node* node, TableCollection* tables, char* parent_table_name, const char* key, int parent_id);
 
 // Initialize a new table collection
 static TableCollection* create_table_collection() {
     TableCollection* collection = (TableCollection*)malloc(sizeof(TableCollection));
     if (!collection) {
         fprintf(stderr, "Error: Memory allocation failed for table collection\n");
         exit(EXIT_FAILURE);
     }
     
     collection->capacity = 10;
     collection->table_count = 0;
     collection->tables = (TableSchema*)malloc(collection->capacity * sizeof(TableSchema));
     if (!collection->tables) {
         fprintf(stderr, "Error: Memory allocation failed for tables array\n");
         free(collection);
         exit(EXIT_FAILURE);
     }
     
     return collection;
 }
 
 // Add a new table to the collection, resizing if necessary
 static void add_table(TableCollection* collection, TableSchema table) {
     if (collection->table_count >= collection->capacity) {
         collection->capacity *= 2;
         collection->tables = (TableSchema*)realloc(collection->tables, 
                                               collection->capacity * sizeof(TableSchema));
         if (!collection->tables) {
             fprintf(stderr, "Error: Memory reallocation failed for tables array\n");
             exit(EXIT_FAILURE);
         }
     }
     
     collection->tables[collection->table_count++] = table;
 }
 
 // Collect keys from an object into a KeyList
 static KeyList* collect_object_keys(Object_Node* obj) {
     KeyList* list = (KeyList*)malloc(sizeof(KeyList));
     if (!list) {
         fprintf(stderr, "Error: Memory allocation failed for key list\n");
         exit(EXIT_FAILURE);
     }
     
     list->count = obj->pair_count;
     list->keys = (char**)malloc(list->count * sizeof(char*));
     if (!list->keys) {
         fprintf(stderr, "Error: Memory allocation failed for key array\n");
         free(list);
         exit(EXIT_FAILURE);
     }
     
     Pair_Node* current = obj->pairs;
     int i = 0;
     while (current && i < list->count) {
         list->keys[i] = strdup(current->key);
         if (!list->keys[i]) {
             fprintf(stderr, "Error: Memory allocation failed for key string\n");
             free_key_list(list);
             exit(EXIT_FAILURE);
         }
         current = current->next;
         i++;
     }
     
     return list;
 }
 
 // Compare two key lists for equality
 static bool compare_key_lists(KeyList* list1, KeyList* list2) {
     if (list1->count != list2->count) {
         return false;
     }
     
     // For each key in list1, check if it exists in list2
     for (int i = 0; i < list1->count; i++) {
         bool found = false;
         for (int j = 0; j < list2->count; j++) {
             if (strcmp(list1->keys[i], list2->keys[j]) == 0) {
                 found = true;
                 break;
             }
         }
         if (!found) {
             return false;
         }
     }
     
     return true;
 }
 
 // Free a key list
 static void free_key_list(KeyList* list) {
     if (!list) return;
     
     if (list->keys) {
         for (int i = 0; i < list->count; i++) {
             if (list->keys[i]) {
                 free(list->keys[i]);
             }
         }
         free(list->keys);
     }
     
     free(list);
 }
 
 // Generate a table name for an array based on parent table and key
 static char* get_table_name_for_array(const char* parent_name, const char* key) {
     // Special case for root object
     if (parent_name == NULL || strcmp(parent_name, "root") == 0) {
         return strdup(key);
     }
     
     // Allocate memory for combined name (parent_name + "_" + key)
     char* name = (char*)malloc(strlen(parent_name) + strlen(key) + 2);
     if (!name) {
         fprintf(stderr, "Error: Memory allocation failed for table name\n");
         exit(EXIT_FAILURE);
     }
     
     sprintf(name, "%s_%s", parent_name, key);
     return name;
 }
 
 // Find a table by schema or create a new one
 static TableSchema* find_or_create_table(TableCollection* tables, const char* name, KeyList* keys) {
     // First, look for an existing table with the same schema
     for (int i = 0; i < tables->table_count; i++) {
         TableSchema* table = &tables->tables[i];
         
         // Check if the column count matches (minus id and optional parent_id)
         if (table->column_count == keys->count + 1 || table->column_count == keys->count + 2) {
             bool keys_match = true;
             
             // Check each key against columns (skipping id and optional parent_id)
             for (int j = 0; j < keys->count; j++) {
                 bool found = false;
                 for (int k = 0; k < table->column_count; k++) {
                     if (strcmp(table->columns[k], "id") == 0 || 
                         strstr(table->columns[k], "_id") == table->columns[k]) {
                         continue;
                     }
                     
                     if (strcmp(keys->keys[j], table->columns[k]) == 0) {
                         found = true;
                         break;
                     }
                 }
                 
                 if (!found) {
                     keys_match = false;
                     break;
                 }
             }
             
             if (keys_match) {
                 return table;
             }
         }
     }
     
     // Create a new table
     TableSchema new_table;
     new_table.name = strdup(name);
     if (!new_table.name) {
         fprintf(stderr, "Error: Memory allocation failed for table name\n");
         exit(EXIT_FAILURE);
     }
     
     // Calculate column count: keys + id + (parent_id if not root)
     bool is_root = (strcmp(name, "root") == 0);
     new_table.column_count = keys->count + (is_root ? 1 : 2);
     
     // Allocate memory for columns
     new_table.columns = (char**)malloc(new_table.column_count * sizeof(char*));
     if (!new_table.columns) {
         fprintf(stderr, "Error: Memory allocation failed for columns array\n");
         free(new_table.name);
         exit(EXIT_FAILURE);
     }
     
     // Set first column to id
     new_table.columns[0] = strdup("id");
     if (!new_table.columns[0]) {
         fprintf(stderr, "Error: Memory allocation failed for column name\n");
         free(new_table.columns);
         free(new_table.name);
         exit(EXIT_FAILURE);
     }
     
     // Set parent_id column if not root
     int column_index = 1;
     if (!is_root) {
         // Extract parent table name from full table name (before last '_')
         char* parent_name = strdup(name);
         char* last_underscore = strrchr(parent_name, '_');
         if (last_underscore) {
             *last_underscore = '\0';
         }
         
         // Create parent_id column name
         char parent_id_col[100];
         sprintf(parent_id_col, "%s_id", parent_name);
         new_table.columns[1] = strdup(parent_id_col);
         if (!new_table.columns[1]) {
             fprintf(stderr, "Error: Memory allocation failed for column name\n");
             free(new_table.columns[0]);
             free(new_table.columns);
             free(new_table.name);
             free(parent_name);
             exit(EXIT_FAILURE);
         }
         
         column_index = 2;
         free(parent_name);
     }
     
     // Add key columns
     for (int i = 0; i < keys->count; i++) {
         new_table.columns[column_index + i] = strdup(keys->keys[i]);
         if (!new_table.columns[column_index + i]) {
             fprintf(stderr, "Error: Memory allocation failed for column name\n");
             for (int j = 0; j < column_index + i; j++) {
                 free(new_table.columns[j]);
             }
             free(new_table.columns);
             free(new_table.name);
             exit(EXIT_FAILURE);
         }
     }
     
     // Initialize objects list
     new_table.objects = NULL;
     
     // Add table to collection
     add_table(tables, new_table);
     
     // Return pointer to the newly added table
     return &tables->tables[tables->table_count - 1];
 }
 
 // Add an object to a table
 static void add_object_to_table(TableSchema* table, Object_Node* obj) {
     // Set object id
     static int next_id = 1;
     obj->node_id = next_id++;
     
     // Add to the table's objects list
     obj->next = table->objects;
     table->objects = obj;
 }
 
 // Process an object node
 static void process_object_node(AST_Node* node, TableCollection* tables, char* parent_table_name, int parent_id) {
     if (!node || node->type != NODE_OBJECT || !node->object) {
         return;
     }
     
     Object_Node* obj = node->object;
     
     // Collect keys from object
     KeyList* keys = collect_object_keys(obj);
     
     // Find or create a table for this object structure
     TableSchema* table = find_or_create_table(tables, parent_table_name ? parent_table_name : "root", keys);
     
     // Add object to table
     add_object_to_table(table, obj);
     
     // Process nested objects and arrays
     Pair_Node* current = obj->pairs;
     while (current) {
         switch (current->value.type) {
             case VALUE_OBJECT: {
                 AST_Node nested_node;
                 nested_node.type = NODE_OBJECT;
                 nested_node.object = current->value.object_val;
                 char* nested_table_name = get_table_name_for_array(table->name, current->key);
                 process_object_node(&nested_node, tables, nested_table_name, obj->node_id);
                 free(nested_table_name);
                 break;
             }
             case VALUE_ARRAY: {
                 AST_Node nested_node;
                 nested_node.type = NODE_ARRAY;
                 nested_node.object = (Object_Node*)current->value.array_val;
                 process_array_node(&nested_node, tables, table->name, current->key, obj->node_id);
                 break;
             }
             default:
                 break;
         }
         
         current = current->next;
     }
     
     free_key_list(keys);
 }
 
 // Process an array node
 static void process_array_node(AST_Node* node, TableCollection* tables, char* parent_table_name, const char* key, int parent_id) {
     if (!node || node->type != NODE_ARRAY) {
         return;
     }
     
     Array_Node* arr = (Array_Node*)node->object;
     if (!arr || arr->size == 0) {
         return;
     }
     
     // Generate table name for this array
     char* array_table_name = get_table_name_for_array(parent_table_name, key);
     
     // Check if array contains objects or scalars
     if (arr->size > 0 && arr->elements[0].type == VALUE_OBJECT) {
         // For arrays of objects, create a table with parent_id and seq columns
         
         // Process each object in the array
         for (int i = 0; i < arr->size; i++) {
             if (arr->elements[i].type == VALUE_OBJECT) {
                 AST_Node nested_node;
                 nested_node.type = NODE_OBJECT;
                 nested_node.object = arr->elements[i].object_val;
                 process_object_node(&nested_node, tables, array_table_name, parent_id);
             }
         }
     } else {
         // For arrays of scalars, create a junction table with parent_id, index, value columns
         
         // Define table schema for junction table
         TableSchema junction_table;
         junction_table.name = array_table_name;
         junction_table.column_count = 3; // parent_id, index, value
         
         junction_table.columns = (char**)malloc(junction_table.column_count * sizeof(char*));
         if (!junction_table.columns) {
             fprintf(stderr, "Error: Memory allocation failed for junction table columns\n");
             free(array_table_name);
             exit(EXIT_FAILURE);
         }
         
         // Create column names
         char parent_id_col[100];
         sprintf(parent_id_col, "%s_id", parent_table_name);
         
         junction_table.columns[0] = strdup(parent_id_col);
         junction_table.columns[1] = strdup("index");
         junction_table.columns[2] = strdup("value");
         
         if (!junction_table.columns[0] || !junction_table.columns[1] || !junction_table.columns[2]) {
             fprintf(stderr, "Error: Memory allocation failed for junction table column names\n");
             for (int i = 0; i < 3; i++) {
                 if (junction_table.columns[i]) {
                     free(junction_table.columns[i]);
                 }
             }
             free(junction_table.columns);
             free(array_table_name);
             exit(EXIT_FAILURE);
         }
         
         // Initialize objects list
         junction_table.objects = NULL;
         
         // Add table to collection
         add_table(tables, junction_table);
         
         // No objects to add to this table directly - we'll handle scalar values during CSV generation
     }
     
     free(array_table_name);
 }
 
 // Main schema generation function
 Schema* generate_schema(AST_Node* root) {
     if (!root) {
         return NULL;
     }
     
     // Create table collection
     TableCollection* collection = create_table_collection();
     
     // Process the root node
     if (root->type == NODE_OBJECT) {
         process_object_node(root, collection, "root", 0);
     } else if (root->type == NODE_ARRAY) {
         process_array_node(root, collection, "root", "items", 0);
     } else {
         fprintf(stderr, "Error: Root node must be an object or array\n");
         exit(EXIT_FAILURE);
     }
     
     // Create schema from collection
     Schema* schema = (Schema*)malloc(sizeof(Schema));
     if (!schema) {
         fprintf(stderr, "Error: Memory allocation failed for schema\n");
         exit(EXIT_FAILURE);
     }
     
     schema->tables = collection->tables;
     schema->table_count = collection->table_count;
     
     // Free collection structure but keep tables
     free(collection);
     
     return schema;
 }
 
 // Free schema memory
 void free_schema(Schema* schema) {
     if (!schema) return;
     
     if (schema->tables) {
         for (int i = 0; i < schema->table_count; i++) {
             TableSchema* table = &schema->tables[i];
             
             if (table->name) {
                 free(table->name);
             }
             
             if (table->columns) {
                 for (int j = 0; j < table->column_count; j++) {
                     if (table->columns[j]) {
                         free(table->columns[j]);
                     }
                 }
                 free(table->columns);
             }
             
             // Note: Objects are part of the AST and will be freed separately
         }
         
         free(schema->tables);
     }
     
     free(schema);
 }
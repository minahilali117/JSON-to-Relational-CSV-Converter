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
// static void collect_table_schemas(AST_Node* root, TableCollection* tables, char* parent_table_name, int parent_id); // Not used in provided code, can be removed if not needed elsewhere
static TableSchema* find_or_create_table(TableCollection* tables, const char* name, KeyList* keys, const char* current_parent_table_name_for_fk); // Added param for FK
static KeyList* collect_object_keys(Object_Node* obj);
static bool compare_key_lists(KeyList* list1, KeyList* list2); // Not used in provided code, can be removed if not needed elsewhere
static void free_key_list(KeyList* list);
static char* get_table_name_for_array(const char* parent_name, const char* key);
static void add_object_to_table(TableSchema* table, Object_Node* obj); // Consider if obj->node_id needs to be globally unique or per-table unique
static void process_object_node(AST_Node* node, TableCollection* tables, char* current_table_name, int parent_id_value, const char* parent_table_name_for_fk);
static void process_array_node(AST_Node* node, TableCollection* tables, char* parent_table_name_of_array_owner, const char* key_of_array, int parent_id_value_of_array_owner);

// Initialize a new table collection
static TableCollection* create_table_collection() {
    TableCollection* collection = (TableCollection*)calloc(1, sizeof(TableCollection)); // Use calloc
    if (!collection) {
        fprintf(stderr, "Error: Memory allocation failed for table collection\n");
        exit(EXIT_FAILURE);
    }
    
    collection->capacity = 10;
    // collection->table_count = 0; // Done by calloc
    collection->tables = (TableSchema*)calloc(collection->capacity, sizeof(TableSchema)); // Use calloc
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
        TableSchema* new_tables_ptr = (TableSchema*)realloc(collection->tables, 
                                               collection->capacity * sizeof(TableSchema));
        if (!new_tables_ptr) {
            fprintf(stderr, "Error: Memory reallocation failed for tables array\n");
            // Consider freeing existing collection->tables and table's contents if realloc fails and we exit
            exit(EXIT_FAILURE);
        }
        collection->tables = new_tables_ptr;
    }
    
    collection->tables[collection->table_count++] = table; // table is copied
}

// Collect keys from an object into a KeyList
static KeyList* collect_object_keys(Object_Node* obj) {
    if (!obj) { // Handle null object node gracefully
        KeyList* list = (KeyList*)calloc(1, sizeof(KeyList));
        if (!list) {
            fprintf(stderr, "Error: Memory allocation failed for empty key list\n");
            exit(EXIT_FAILURE);
        }
        list->count = 0;
        list->keys = NULL;
        return list;
    }

    KeyList* list = (KeyList*)calloc(1, sizeof(KeyList)); // Use calloc
    if (!list) {
        fprintf(stderr, "Error: Memory allocation failed for key list\n");
        exit(EXIT_FAILURE);
    }
    
    list->count = obj->pair_count;
    if (list->count > 0) {
        list->keys = (char**)calloc(list->count, sizeof(char*)); // Use calloc
        if (!list->keys) {
            fprintf(stderr, "Error: Memory allocation failed for key array\n");
            free(list);
            exit(EXIT_FAILURE);
        }
        
        Pair_Node* current = obj->pairs;
        int i = 0;
        while (current && i < list->count) {
            if (current->key) { // Ensure key is not null
                list->keys[i] = strdup(current->key);
                if (!list->keys[i]) {
                    fprintf(stderr, "Error: Memory allocation failed for key string strdup\n");
                    // Free previously strdup'd keys in this list
                    for (int k = 0; k < i; k++) free(list->keys[k]);
                    free(list->keys);
                    free(list);
                    exit(EXIT_FAILURE);
                }
            } else {
                // Handle null key if that's possible by parser, e.g., assign a placeholder or error
                list->keys[i] = strdup(""); // Or some other placeholder
                 fprintf(stderr, "Warning: Object key is null, using empty string.\n");
            }
            current = current->next;
            i++;
        }
    } else {
        list->keys = NULL;
    }
    
    return list;
}

// Compare two key lists for equality (order insensitive)
// Not directly used by the schema generation logic as find_or_create_table uses a more complex match
// static bool compare_key_lists(KeyList* list1, KeyList* list2) {
//     if (!list1 || !list2 || list1->count != list2->count) {
//         return false;
//     }
//     if (list1->count == 0) return true; // Both empty

//     for (int i = 0; i < list1->count; i++) {
//         bool found = false;
//         for (int j = 0; j < list2->count; j++) {
//             if (list1->keys[i] && list2->keys[j] && strcmp(list1->keys[i], list2->keys[j]) == 0) {
//                 found = true;
//                 break;
//             }
//         }
//         if (!found) {
//             return false;
//         }
//     }
//     return true;
// }

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
    if (!key) { // Handle null key
        key = "unknown_array_key"; 
        fprintf(stderr, "Warning: Null key provided for array table name, using '%s'.\n", key);
    }
    // Special case for root object's arrays
    if (parent_name == NULL || strcmp(parent_name, "root") == 0) {
        char* new_name = strdup(key);
        if (!new_name) {
            fprintf(stderr, "Error: strdup failed for array table name (root parent)\n");
            exit(EXIT_FAILURE);
        }
        return new_name;
    }
    
    // Allocate memory for combined name (parent_name + "_" + key)
    // Max length check could be useful here if names can be very long.
    size_t parent_len = strlen(parent_name);
    size_t key_len = strlen(key);
    char* name = (char*)malloc(parent_len + key_len + 2); // +1 for '_' and +1 for '\0'
    if (!name) {
        fprintf(stderr, "Error: Memory allocation failed for array table name\n");
        exit(EXIT_FAILURE);
    }
    
    sprintf(name, "%s_%s", parent_name, key);
    return name;
}

// Find a table by schema or create a new one.
// 'name_candidate' is the proposed name for the table if it's newly created.
// 'current_parent_table_name_for_fk' is the name of the table that would be the parent in a FK relationship.
static TableSchema* find_or_create_table(TableCollection* tables, const char* name_candidate, KeyList* keys, const char* current_parent_table_name_for_fk) {
    // Try to find an existing table with the same set of keys (column names, excluding PK/FKs)
    for (int i = 0; i < tables->table_count; i++) {
        TableSchema* existing_table = &tables->tables[i];
        // A simple heuristic: if names match, assume schema matches.
        // A more robust check would compare KeyList 'keys' against existing_table's columns.
        // For now, if name_candidate matches an existing table name, we use that.
        // This part needs careful consideration for how schemas are truly identified as "the same".
        // The original logic compared key counts and key names.
        if (strcmp(existing_table->name, name_candidate) == 0) {
            // TODO: Add a more robust schema comparison here if needed,
            // e.g., comparing 'keys' with existing_table->columns.
            // For now, if name matches, we assume it's the correct table.
            return existing_table;
        }
    }
    
    // If not found, create a new table
    TableSchema new_table;
    new_table.name = strdup(name_candidate);
    if (!new_table.name) {
        fprintf(stderr, "Error: strdup failed for new table name '%s'\n", name_candidate);
        exit(EXIT_FAILURE);
    }
    
    // Determine if this table is a "root" table (no parent FK)
    // A table is effectively a root table in its own context if current_parent_table_name_for_fk is NULL or "root"
    bool has_parent_fk = (current_parent_table_name_for_fk != NULL && strcmp(current_parent_table_name_for_fk, "root") != 0);
    
    new_table.column_count = keys->count + 1 + (has_parent_fk ? 1 : 0); // +1 for 'id', +1 for 'parent_id' if applicable
    
    new_table.columns = (char**)calloc(new_table.column_count, sizeof(char*)); // Use calloc
    if (!new_table.columns) {
        fprintf(stderr, "Error: Memory allocation failed for columns array for table '%s'\n", new_table.name);
        free(new_table.name);
        exit(EXIT_FAILURE);
    }
    
    int current_col_idx = 0;
    new_table.columns[current_col_idx++] = strdup("id"); // Primary Key

    if (has_parent_fk) {
        // Max length for FK column name: parent_name + "_id" + \0
        char fk_col_name[256]; // Assuming table names won't make this overflow
        sprintf(fk_col_name, "%s_id", current_parent_table_name_for_fk);
        new_table.columns[current_col_idx++] = strdup(fk_col_name);
    }
    
    // Add actual data columns from keys
    for (int i = 0; i < keys->count; i++) {
        if (keys->keys[i]) { // Ensure key string is not null
             new_table.columns[current_col_idx + i] = strdup(keys->keys[i]);
        } else {
            // This case should ideally be prevented by collect_object_keys
            new_table.columns[current_col_idx + i] = strdup(""); // Placeholder for null key
            fprintf(stderr, "Warning: Null key encountered when creating table columns for '%s'.\n", new_table.name);
        }
    }

    // Check all column allocations
    bool col_alloc_ok = true;
    for(int i=0; i < new_table.column_count; ++i) {
        if(!new_table.columns[i]) {
            col_alloc_ok = false;
            break;
        }
    }

    if (!col_alloc_ok) {
        fprintf(stderr, "Error: Memory allocation failed for one or more column names for table '%s'\n", new_table.name);
        for (int j = 0; j < new_table.column_count; j++) { // Free successfully allocated columns
            if(new_table.columns[j]) free(new_table.columns[j]);
        }
        free(new_table.columns);
        free(new_table.name);
        exit(EXIT_FAILURE);
    }
    
    new_table.objects = NULL; // Initialize list of objects belonging to this table
    add_table(tables, new_table); // Adds a copy of new_table
    
    return &tables->tables[tables->table_count - 1]; // Return pointer to the table in the collection
}

// Add an object to a table's linked list of objects
static int global_next_node_id = 1; // For globally unique IDs across all tables

static void add_object_to_table(TableSchema* table, Object_Node* obj) {
    if (!table || !obj) return;
    
    obj->node_id = global_next_node_id++; // Assign a globally unique ID
    
    // Add to the head of the table's objects list
    obj->next = table->objects;
    table->objects = obj;
}

// Process an object node.
// 'current_table_name' is the name of the table this object belongs to.
// 'parent_id_value' is the ID of the parent object if this object is nested.
// 'parent_table_name_for_fk' is the name of the direct parent table for FK generation purposes.
static void process_object_node(AST_Node* node, TableCollection* tables, char* current_table_name, int parent_id_value, const char* parent_table_name_for_fk) {
    if (!node || node->type != NODE_OBJECT || !node->object) {
        return;
    }
    Object_Node* obj = node->object;
    
    KeyList* keys = collect_object_keys(obj);
    TableSchema* table = find_or_create_table(tables, current_table_name, keys, parent_table_name_for_fk);
    // 'table' now points to the schema this object conforms to.
    
    add_object_to_table(table, obj); // obj gets its node_id here
    // Note: parent_id_value might need to be stored in obj if it's to be written to CSV for this object.
    // This is typically handled by CSV generator by looking up the FK column.

    Pair_Node* current_pair = obj->pairs;
    while (current_pair) {
        if (current_pair->key == NULL) { // Skip null keys
            fprintf(stderr, "Warning: Skipping null key in object processing for table '%s'.\n", table->name);
            current_pair = current_pair->next;
            continue;
        }
        switch (current_pair->value.type) {
            case VALUE_OBJECT: {
                if (current_pair->value.object_val) {
                    AST_Node nested_ast_node;
                    nested_ast_node.type = NODE_OBJECT;
                    nested_ast_node.object = current_pair->value.object_val;
                    
                    // The nested object forms a new table, named based on its key and parent table (current_table_name)
                    char* nested_obj_table_name = get_table_name_for_array(table->name /* parent is current table */, current_pair->key);
                    // The parent for FK purposes for this nested object's table is 'table->name'
                    process_object_node(&nested_ast_node, tables, nested_obj_table_name, obj->node_id /* PK of current obj is FK for nested */, table->name);
                    free(nested_obj_table_name);
                }
                break;
            }
            case VALUE_ARRAY: {
                if (current_pair->value.array_val) {
                    AST_Node nested_ast_node;
                    nested_ast_node.type = NODE_ARRAY;
                    nested_ast_node.array = current_pair->value.array_val; // Correct member access
                    // The parent table of the array owner is 'table->name'.
                    // The parent ID for elements of the array (or its junction table) is 'obj->node_id'.
                    process_array_node(&nested_ast_node, tables, table->name, current_pair->key, obj->node_id);
                }
                break;
            }
            default:
                // Scalar values are handled by populating the current object's row in its table.
                break;
        }
        current_pair = current_pair->next;
    }
    free_key_list(keys);
}

// Process an array node.
// 'parent_table_name_of_array_owner' is the name of the table that owns the object containing this array.
// 'key_of_array' is the key under which this array is found in its parent object.
// 'parent_id_value_of_array_owner' is the ID of the object that owns this array (for FKs).
static void process_array_node(AST_Node* node, TableCollection* tables, char* parent_table_name_of_array_owner, const char* key_of_array, int parent_id_value_of_array_owner) {
    if (!node || node->type != NODE_ARRAY || !node->array) { // Correct member access
        return;
    }
    Array_Node* arr = node->array; // Correct member access
    if (arr->size == 0 || !arr->elements) {
        return;
    }

    // This name is for the table representing the array's contents or a junction table.
    char* table_name_for_array_elements = get_table_name_for_array(parent_table_name_of_array_owner, key_of_array);
    if (!table_name_for_array_elements) { exit(EXIT_FAILURE); }

    // Determine if array contains objects or scalars by checking the first element
    if (arr->elements[0].type == VALUE_OBJECT) {
        // Array of objects: each object goes into the table 'table_name_for_array_elements'
        // The parent for FK purposes for this table is 'parent_table_name_of_array_owner'.
        for (int i = 0; i < arr->size; i++) {
            if (arr->elements[i].type == VALUE_OBJECT && arr->elements[i].object_val) {
                AST_Node element_obj_node;
                element_obj_node.type = NODE_OBJECT;
                element_obj_node.object = arr->elements[i].object_val;
                // Each object in the array belongs to the 'table_name_for_array_elements' table.
                // Its parent for FK is the owner of the array.
                process_object_node(&element_obj_node, tables, table_name_for_array_elements, parent_id_value_of_array_owner, parent_table_name_of_array_owner);
            } else {
                // Handle mixed-type arrays or non-object elements if necessary.
                // Current logic assumes if first is object, all relevant ones are.
                 fprintf(stderr, "Warning: Array '%s' expected objects but found non-object at index %d.\n", table_name_for_array_elements, i);
            }
        }
    } else {
        // Array of scalars: create a junction table.
        // Table name is 'table_name_for_array_elements'.
        TableSchema junction_table;
        junction_table.name = strdup(table_name_for_array_elements);
        if (!junction_table.name) {
            fprintf(stderr, "Error: strdup failed for junction table name '%s'\n", table_name_for_array_elements);
            free(table_name_for_array_elements);
            exit(EXIT_FAILURE);
        }

        junction_table.column_count = 3; // parent_id, index, value
        junction_table.columns = (char**)calloc(junction_table.column_count, sizeof(char*));
        if (!junction_table.columns) {
            fprintf(stderr, "Error: calloc failed for junction table columns for '%s'\n", junction_table.name);
            free(junction_table.name);
            free(table_name_for_array_elements);
            exit(EXIT_FAILURE);
        }
        
        char fk_col_name_buffer[256]; // Buffer for FK column name
        // The FK points to the table that owns the object which has this array.
        sprintf(fk_col_name_buffer, "%s_id", parent_table_name_of_array_owner); 
        
        junction_table.columns[0] = strdup(fk_col_name_buffer);
        junction_table.columns[1] = strdup("item_index"); // Renamed from "index" to avoid SQL keyword clash
        junction_table.columns[2] = strdup("value");

        bool cols_ok = junction_table.columns[0] && junction_table.columns[1] && junction_table.columns[2];
        if (!cols_ok) {
            fprintf(stderr, "Error: strdup failed for one or more junction table column names for '%s'\n", junction_table.name);
            for(int c=0; c<3; ++c) if(junction_table.columns[c]) free(junction_table.columns[c]);
            free(junction_table.columns);
            free(junction_table.name);
            free(table_name_for_array_elements);
            exit(EXIT_FAILURE);
        }
        
        junction_table.objects = NULL; // Junction tables for scalars don't directly store Object_Nodes from AST.
                                       // Their rows are generated during CSV writing.
        add_table(tables, junction_table); // Adds a copy of junction_table
    }
    
    // The local_array_table_name was either strdup'd by find_or_create_table (via process_object_node)
    // or strdup'd directly for junction_table.name. So, the original can be freed.
    free(table_name_for_array_elements);
}
 
// Main schema generation function
Schema* generate_schema(AST_Node* root) {
    if (!root) {
        return NULL;
    }
    
    TableCollection* collection = create_table_collection();
    global_next_node_id = 1; // Reset global ID for each schema generation run

    if (root->type == NODE_OBJECT) {
        // The root object belongs to a table named "root". It has no parent FK.
        process_object_node(root, collection, "root", 0, NULL); // NULL for parent_table_name_for_fk
    } else if (root->type == NODE_ARRAY) {
        // A root array. Elements will go into a table named "root_items" (or similar, based on key "items").
        // The parent context for this array is "root".
        process_array_node(root, collection, "root", "items", 0);
    } else {
        fprintf(stderr, "Error: Root of JSON data must be an object or an array.\n");
        // Free collection before exiting
        free(collection->tables);
        free(collection);
        exit(EXIT_FAILURE);
    }
    
    Schema* schema = (Schema*)malloc(sizeof(Schema));
    if (!schema) {
        fprintf(stderr, "Error: Memory allocation failed for schema structure\n");
        // Free collection & its tables before exiting
        for (int i = 0; i < collection->table_count; i++) {
            if (collection->tables[i].name) free(collection->tables[i].name);
            if (collection->tables[i].columns) {
                for (int j = 0; j < collection->tables[i].column_count; j++) {
                    if (collection->tables[i].columns[j]) free(collection->tables[i].columns[j]);
                }
                free(collection->tables[i].columns);
            }
        }
        free(collection->tables);
        free(collection);
        exit(EXIT_FAILURE);
    }
    
    schema->tables = collection->tables;       // Transfer ownership of tables array
    schema->table_count = collection->table_count;
    
    free(collection); // Free the collection shell, not the tables array itself.
    
    return schema;
}
 
// Free schema memory
void free_schema(Schema* schema) {
    if (!schema) return;
    
    if (schema->tables) {
        for (int i = 0; i < schema->table_count; i++) {
            TableSchema* table = &schema->tables[i]; // Get address of table
            
            if (table->name) {
                free(table->name);
                table->name = NULL;
            }
            
            if (table->columns) {
                for (int j = 0; j < table->column_count; j++) {
                    if (table->columns[j]) {
                        free(table->columns[j]);
                        table->columns[j] = NULL;
                    }
                }
                free(table->columns);
                table->columns = NULL;
            }
            // Objects (Object_Node linked list in table->objects) are part of the main AST.
            // The AST is freed separately by free_ast().
            // TableSchema does not own the Object_Node data itself, only pointers to them.
            table->objects = NULL; // Clear the pointer
        }
        free(schema->tables); // Free the array of TableSchema structs
        schema->tables = NULL;
    }
    free(schema); // Free the Schema struct itself
}

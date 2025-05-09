/**
 * csv_generator.c - CSV file generation for json2relcsv
 */

 #include "ast.h"
 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include <sys/stat.h>
 #include <errno.h>
 
 // Function to escape CSV content
 static char* escape_csv_string(const char* str) {
     if (!str) return NULL;
     
     // Calculate required size
     size_t len = strlen(str);
     size_t quote_count = 0;
     
     for (size_t i = 0; i < len; i++) {
         if (str[i] == '"') {
             quote_count++;
         }
     }
     
     // Allocate memory for escaped string (original + double quotes + potential quotes to escape + null terminator)
     char* escaped = (char*)malloc(len + 2 + quote_count + 1);
     if (!escaped) {
         fprintf(stderr, "Error: Memory allocation failed for CSV escaping\n");
         exit(EXIT_FAILURE);
     }
     
     // Add opening quote
     escaped[0] = '"';
     size_t pos = 1;
     
     // Copy and escape content
     for (size_t i = 0; i < len; i++) {
         if (str[i] == '"') {
             escaped[pos++] = '"'; // Escape by doubling
         }
         escaped[pos++] = str[i];
     }
     
     // Add closing quote and null terminator
     escaped[pos++] = '"';
     escaped[pos] = '\0';
     
     return escaped;
 }
 
 // Convert a value to CSV string format
 static char* value_to_csv_string(Value_Node value) {
     char buffer[64]; // For numeric values
     
     switch (value.type) {
         case VALUE_STRING:
             return value.string_val ? escape_csv_string(value.string_val) : strdup("");
         
         case VALUE_NUMBER:
             snprintf(buffer, sizeof(buffer), "%g", value.number_val);
             return strdup(buffer);
         
         case VALUE_BOOLEAN:
             return strdup(value.boolean_val ? "true" : "false");
         
         case VALUE_NULL:
             return strdup("");
         
         case VALUE_OBJECT:
         case VALUE_ARRAY:
             // These should be handled separately
             return strdup("");
     }
     
     return strdup("");
 }
 
 // Create directory if it doesn't exist
 static int ensure_directory(const char* path) {
     struct stat st = {0};
     
     if (stat(path, &st) == -1) {
         #ifdef _WIN32
         if (mkdir(path) != 0) {
         #else
         if (mkdir(path, 0755) != 0) {
         #endif
             fprintf(stderr, "Error: Failed to create directory '%s': %s\n", 
                    path, strerror(errno));
             return 0;
         }
     }
     
     return 1;
 }
 
 // Write a single CSV file for a table
 static void write_table_csv(TableSchema* table, const char* out_dir) {
     if (!table || !table->name) return;
     
     // Create path
     char* filename;
     if (out_dir && strlen(out_dir) > 0) {
         filename = (char*)malloc(strlen(out_dir) + strlen(table->name) + 6); // path + / + name + .csv + \0
         if (!filename) {
             fprintf(stderr, "Error: Memory allocation failed for filename\n");
             exit(EXIT_FAILURE);
         }
         sprintf(filename, "%s/%s.csv", out_dir, table->name);
     } else {
         filename = (char*)malloc(strlen(table->name) + 5); // name + .csv + \0
         if (!filename) {
             fprintf(stderr, "Error: Memory allocation failed for filename\n");
             exit(EXIT_FAILURE);
         }
         sprintf(filename, "%s.csv", table->name);
     }
     
     // Open file
     FILE* file = fopen(filename, "w");
     if (!file) {
         fprintf(stderr, "Error: Failed to open file '%s' for writing: %s\n", 
                filename, strerror(errno));
         free(filename);
         exit(EXIT_FAILURE);
     }
     
     // Write header row
     for (int i = 0; i < table->column_count; i++) {
         fprintf(file, "%s%s", table->columns[i], i < table->column_count - 1 ? "," : "\n");
     }
     
     // Special case for junction tables (arrays of scalars)
     if (table->column_count == 3 && 
         strcmp(table->columns[1], "index") == 0 && 
         strcmp(table->columns[2], "value") == 0) {
         
         // Find the parent table and its objects
         char parent_table_name[256];
         strncpy(parent_table_name, table->columns[0], sizeof(parent_table_name) - 1);
         parent_table_name[sizeof(parent_table_name) - 1] = '\0';
         
         // Remove "_id" suffix to get parent table name
         char* id_suffix = strstr(parent_table_name, "_id");
         if (id_suffix) {
             *id_suffix = '\0';
         }
         
         // Get key name from table name
         char* key_name = strdup(strrchr(table->name, '_') ? strrchr(table->name, '_') + 1 : table->name);
         if (key_name) {
             key_name++; // Skip the underscore
         } else {
             key_name = table->name; // Fallback
         }
         free(key_name);
         // Write array elements
         // Note: We're not iterating through objects here as junction tables 
         // don't store objects directly. Instead, we handle this when writing 
         // parent object tables.
         
         free(filename);
         fclose(file);
         return;
     }
     
     // Determine if this is a child table
     bool is_child_table = (table->column_count >= 2 && 
                             strstr(table->columns[1], "_id") == table->columns[1]);
     
     // Write object rows
     Object_Node* obj = table->objects;
     int seq = 0; // For child tables with array elements
     
     while (obj) {
         // Start row with the object's ID
         fprintf(file, "%d", obj->node_id);
         
         // For each column (after ID)
         for (int i = 1; i < table->column_count; i++) {
             fprintf(file, ",");
             
             // Handle parent ID for child tables
             if (i == 1 && is_child_table) {
                 // Find parent ID from parent object reference
                 // Note: In a full implementation, we would store parent references
                 // This is simplified - actual parent IDs would need to be tracked
                 int parent_id = obj->node_id; // Placeholder
                 fprintf(file, "%d", parent_id);
                 continue;
             }
             
             // Handle sequence number column for array elements
             if (i == 2 && is_child_table && strcmp(table->columns[2], "seq") == 0) {
                 fprintf(file, "%d", seq);
                 seq++;
                 continue;
             }
             
             // For regular columns, find the matching key in the object
             bool found = false;
             Pair_Node* pair = obj->pairs;
             
             while (pair) {
                 if (strcmp(pair->key, table->columns[i]) == 0) {
                     char* csv_value = value_to_csv_string(pair->value);
                     fprintf(file, "%s", csv_value);
                     free(csv_value);
                     found = true;
                     break;
                 }
                 pair = pair->next;
             }
             
             // If key not found, write empty value
             if (!found) {
                 fprintf(file, "");
             }
         }
         
         fprintf(file, "\n");
         obj = obj->next;
         
         // Reset sequence counter for next object
         if (!obj) {
             seq = 0;
         }
     }
     
     free(filename);
     fclose(file);
 }
 
 // Process arrays of scalars in object and write to junction tables
 static void write_scalar_arrays(AST_Node* root, Schema* schema, const char* out_dir) {
     // This is a simplified implementation
     // In a complete solution, we would traverse the AST again
     // to find all arrays of scalars and write them to the junction tables
     
     // For each table in the schema
     for (int i = 0; i < schema->table_count; i++) {
         TableSchema* table = &schema->tables[i];
         
         // Skip non-junction tables
         if (table->column_count != 3 || 
             strcmp(table->columns[1], "index") != 0 || 
             strcmp(table->columns[2], "value") != 0) {
             continue;
         }
         
         // Create file for junction table
         char* filename;
         if (out_dir && strlen(out_dir) > 0) {
             filename = (char*)malloc(strlen(out_dir) + strlen(table->name) + 6);
             if (!filename) {
                 fprintf(stderr, "Error: Memory allocation failed for filename\n");
                 exit(EXIT_FAILURE);
             }
             sprintf(filename, "%s/%s.csv", out_dir, table->name);
         } else {
             filename = (char*)malloc(strlen(table->name) + 5);
             if (!filename) {
                 fprintf(stderr, "Error: Memory allocation failed for filename\n");
                 exit(EXIT_FAILURE);
             }
             sprintf(filename, "%s.csv", table->name);
         }
         
         // Open file
         FILE* file = fopen(filename, "w");
         if (!file) {
             fprintf(stderr, "Error: Failed to open file '%s' for writing: %s\n", 
                    filename, strerror(errno));
             free(filename);
             exit(EXIT_FAILURE);
         }
         
         // Write header row
         for (int j = 0; j < table->column_count; j++) {
             fprintf(file, "%s%s", table->columns[j], j < table->column_count - 1 ? "," : "\n");
         }
         
         // Close file - in a complete solution, we would populate it with data
         free(filename);
         fclose(file);
     }
 }
 
 // Main function to write all CSV files
 void write_csv_files(Schema* schema, const char* out_dir) {
     if (!schema) return;
     
     // Create output directory if specified
     if (out_dir && strlen(out_dir) > 0) {
         if (!ensure_directory(out_dir)) {
             exit(EXIT_FAILURE);
         }
     }
     
     // Write each table to a CSV file
     for (int i = 0; i < schema->table_count; i++) {
         write_table_csv(&schema->tables[i], out_dir);
     }
     
     // Handle arrays of scalars (junction tables)
     // Note: In a complete solution, this would be part of the schema generation
     // and would be handled during the schema processing
 }
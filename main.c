/**
 * main.c - Entry point for json2relcsv tool
 */

 #include <stdio.h>
 #include <stdlib.h>
 #include <string.h>
 #include "ast.h"
 
 // External declarations for flex/bison
 extern FILE* yyin;
 extern int yyparse();
 extern AST_Node* ast_root;
 
 // Command line argument parsing
 typedef struct {
     int print_ast;
     char* out_dir;
 } CommandLineArgs;
 
 // Parse command line arguments
 static CommandLineArgs parse_args(int argc, char** argv) {
     CommandLineArgs args = {0};
     
     for (int i = 1; i < argc; i++) {
         if (strcmp(argv[i], "--print-ast") == 0) {
             args.print_ast = 1;
         } else if (strcmp(argv[i], "--out-dir") == 0) {
             if (i + 1 < argc) {
                 args.out_dir = argv[++i];
             } else {
                 fprintf(stderr, "Error: --out-dir requires a directory path\n");
                 exit(EXIT_FAILURE);
             }
         } else {
             fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
             fprintf(stderr, "Usage: %s [--print-ast] [--out-dir DIR]\n", argv[0]);
             exit(EXIT_FAILURE);
         }
     }
     
     return args;
 }
 
 int main(int argc, char** argv) {
     // Parse command line arguments
     CommandLineArgs args = parse_args(argc, argv);
     
     // Use stdin for JSON input
     yyin = stdin;
     
     // Parse JSON input
     if (yyparse() != 0) {
         // Error handling is done in yyerror, just exit
         return EXIT_FAILURE;
     }

         
     // Check if we have a valid AST
     if (!ast_root) {
         fprintf(stderr, "Error: No AST generated\n");
         return EXIT_FAILURE;
     }
     
     // Print AST if requested
     if (args.print_ast) {
         print_ast(ast_root, 0);
         printf("\n");
     }
     
     // Generate schema from AST
     Schema* schema = generate_schema(ast_root);
     if (!schema) {
         fprintf(stderr, "Error: Failed to generate schema\n");
         free_ast(ast_root);
         return EXIT_FAILURE;
     }
     
     // Write CSV files
     write_csv_files(schema, args.out_dir);
     
     // Cleanup
     if (schema) free_schema(schema);
     if (ast_root) free_ast(ast_root);
     
     return EXIT_SUCCESS;
 }
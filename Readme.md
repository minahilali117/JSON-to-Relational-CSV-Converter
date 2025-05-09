# JSON to Relational CSV Converter

Contributers:
Minahil Ali - 22i-0849
Ayaan Khan - 22i-0832
This tool converts JSON data into relational CSV tables following specific rules for mapping JSON structures to relational data.

## Overview

The `json2relcsv` tool reads a JSON file from standard input and outputs multiple CSV files, one for each identified table in the relational schema. The tool:

1. Reads and validates JSON input
2. Builds an Abstract Syntax Tree (AST)
3. Analyzes the structure to identify tables
4. Assigns primary and foreign keys
5. Outputs CSV files following the relational mapping rules

## Building the Project

Required tools:
- C compiler (GCC recommended)
- Flex (for lexical analysis)
- Bison (for parsing)
```bash
sudo apt-get install flex bison build-essential
```
- Make

To build the project:

```bash
make
```

This will create the `json2relcsv` executable.

## Usage

```bash
./json2relcsv < input.json [--print-ast] [--out-dir DIR]
```

Options:
- `--print-ast`: Print the AST to stdout
- `--out-dir DIR`: Write CSV files to directory DIR (default: current directory)

## Conversion Rules

1. **Object → table row**: Objects with the same keys go in one table
2. **Array of objects → child table**: One row per element, with foreign key to parent
3. **Array of scalars → junction table**: Columns parent_id, index, value
4. **Scalars → columns**: JSON null becomes empty
5. **Every row gets an id**: Foreign keys are `<parent>_id`
6. **File name = table name + .csv**: Include header row

## Example Outputs

### Example 1 – Flat object

Input:
```json
{ "id": 1, "name": "Ali", "age": 19 }
```

Output (`people.csv`):
```
id,name,age
1,Ali,19
```

### Example 2 – Array of scalars

Input:
```json
{
  "movie": "Inception",
  "genres": ["Action", "Sci-Fi", "Thriller"]
}
```

Output (`movies.csv`):
```
id,movie
1,Inception
```

Output (`genres.csv`):
```
movie_id,index,value
1,0,Action
1,1,Sci-Fi
1,2,Thriller
```

### Example 3 – Array of objects

Input:
```json
{
  "orderId": 7,
  "items": [
    {"sku": "X1", "qty": 2},
    {"sku": "Y9", "qty": 1}
  ]
}
```

Output (`orders.csv`):
```
id,orderId
1,7
```

Output (`items.csv`):
```
order_id,seq,sku,qty
1,0,X1,2
1,1,Y9,1
```

### Example 4 – Nested objects + reused shape

Input:
```json
{
  "postId": 101,
  "author": {"uid": "u1", "name": "Sara"},
  "comments": [
    {"uid": "u2", "text": "Nice!"},
    {"uid": "u3", "text": "+1"}
  ]
}
```

Output (`posts.csv`):
```
id,postId,author_id
1,101,1
```

Output (`users.csv`):
```
id,uid,name
1,u1,Sara
2,u2,
3,u3,
```

Output (`comments.csv`):
```
post_id,seq,user_id,text
1,0,2,"Nice!"
1,1,3,"+1"
```

## Test Cases

The repository includes several test cases in the `tests` directory:

1. `test1.json`: Simple flat object
2. `test2.json`: Array of scalars
3. `test3.json`: Array of objects
4. `test4.json`: Nested objects
5. `test5.json`: Complex combination of JSON structures

To run the tests:

```bash
./run_tests.sh
```

## Implementation Details

The project consists of several components:

- **Lexer (scanner.l)**: Tokenizes JSON input using Flex
- **Parser (parser.y)**: Validates JSON structure and builds AST using Bison
- **AST (ast.c/h)**: Defines and implements the Abstract Syntax Tree
- **Schema (schema.c)**: Analyzes AST to identify tables
- **CSV Generator (csv_generator.c)**: Outputs relational data as CSV files
- **Main (main.c)**: Entry point and command-line processing

## Memory Management

The tool handles large JSON files (up to 30 MiB) by:
- Streaming CSV output without large memory buffers
- Proper memory allocation and cleanup
- Efficient data structures for table schema and rows

## Error Handling

The tool reports errors with line and column numbers for:
- Lexical errors (invalid tokens)
- Syntax errors (invalid JSON structure)
- Memory allocation failures
- File I/O errors

## Limitations

- Unicode support is limited to basic ASCII characters
- Complex nested array structures might create many tables
- Performance considerations for very large files (30 MiB limit)

## License

This project is provided as an educational example.
# Makefile for json2relcsv

CC = gcc
CFLAGS = -Wall -Wextra -g
LDFLAGS =
FLEX = flex
BISON = bison

# Output executable
TARGET = json2relcsv

# Source files
FLEX_SRC = scanner.l
BISON_SRC = parser.y
C_SRCS = ast.c schema.c csv_generator.c main.c

# Generated source files
FLEX_C = lex.yy.c
BISON_C = parser.tab.c
BISON_H = parser.tab.h

# Object files
OBJS = $(FLEX_C:.c=.o) $(BISON_C:.c=.o) $(C_SRCS:.c=.o)

# Default target
all: $(TARGET)

# Build the executable
$(TARGET): $(OBJS)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)

# Generate lexer from flex source
$(FLEX_C): $(FLEX_SRC) $(BISON_H)
	$(FLEX) -o $@ $<

# Generate parser from bison source
$(BISON_C) $(BISON_H): $(BISON_SRC)
	$(BISON) -d -o $(BISON_C) $<

# Compile C source files
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean up
clean:
	rm -f $(TARGET) $(OBJS) $(FLEX_C) $(BISON_C) $(BISON_H) *.csv

# Special dependencies
ast.o: ast.c ast.h
schema.o: schema.c ast.h
csv_generator.o: csv_generator.c ast.h
main.o: main.c ast.h
$(FLEX_C:.c=.o): $(FLEX_C) $(BISON_H)
$(BISON_C:.c=.o): $(BISON_C)

.PHONY: all clean
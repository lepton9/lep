#ifndef SSAIR_H
#define SSAIR_H

#include <stdbool.h>
#include <stdio.h>

#include "ast.h"
#include "symtab.h"

typedef enum {
  OP_COPY,
  OP_ADD,
  OP_SUB,
  OP_MUL,
  OP_DIV,
  OP_FADD,
  OP_FSUB,
  OP_FMUL,
  OP_FDIV,
  OP_CALL,
  OP_RET,
  OP_PHI
} opcode;

typedef enum {
  SSA_OPERAND_NONE,
  SSA_OPERAND_VALUE,
  SSA_OPERAND_INT,
  SSA_OPERAND_FLOAT,
  SSA_OPERAND_BOOL,
  SSA_OPERAND_CHAR,
  SSA_OPERAND_STRING
} operand_type;

typedef struct {
  operand_type kind;
  TYPE type;
  union {
    struct {
      char *name;
      unsigned version;
    } value;
    long int_value;
    double float_value;
    bool bool_value;
    unsigned char char_value;
    char *string_value;
  } data;
} operand;

typedef struct instruction {
  opcode op;
  operand dest;
  operand src1;
  operand src2;
  char *callee;
  operand *args;
  size_t arg_count;
  struct instruction *next;
} instruction;

typedef struct basic_block {
  unsigned id;
  instruction *first;
  instruction *last;
  struct basic_block **successors;
  size_t successor_count;
  struct basic_block **predecessors;
  size_t predecessor_count;
  struct basic_block *next;
} basic_block;

typedef struct {
  char *name;
  TYPE type;
  bool has_initializer;
  operand initializer;
} global_var;

typedef struct {
  char *name;
  TYPE return_type;
  operand *params;
  size_t param_count;
  basic_block *entry_block;
  basic_block *blocks;
} function;

typedef enum {
  GLOBAL_VAR,
  FUNCTION
} node_type;

typedef struct ssa_node {
  node_type type;
  union {
    global_var *global;
    function *function;
  } value;
  struct ssa_node *next;
} ssa_node;

typedef struct {
  ssa_node *entry;
  ssa_node *last;
  bool valid;
  char *error;
} ssa;

ssa *generate_ssair(AST *root);
void print_ssair(FILE *out, const ssa *ir);
void free_ssair(ssa *ir);

/* Returns an owned SSA identifier. */
char *var_name(const char *name, unsigned version);

#endif

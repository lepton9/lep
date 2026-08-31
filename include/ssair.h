#ifndef SSAIR_H
#define SSAIR_H

#include <stdbool.h>
#include <stdio.h>

#include "ast.h"
#include "analyzer.h"
#include "array_list.h"
#include "symtab.h"
#include "diagnostic.h"

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
  OP_EQ,
  OP_NEQ,
  OP_LT,
  OP_LE,
  OP_GT,
  OP_GE,
  OP_NOT,
  OP_CALL,
  OP_RET,
  OP_PHI,
  OP_BR,
  OP_CBR
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
  char *name;
} ssa_name;

typedef struct {
  unsigned name_index;
  unsigned version;
  TYPE type;
} value_info;

typedef struct {
  operand_type kind;
  TYPE type;
  union {
    struct {
      unsigned id;
    } value;
    long int_value;
    double float_value;
    bool bool_value;
    unsigned char char_value;
    char *string_value;
  } data;
} operand;

typedef struct {
  operand value;
  struct basic_block *block;
} phi_entry;

typedef struct instruction {
  opcode op;
  operand dest;
  union {
    struct {
      operand src1;
      operand src2;
    } operands;
    struct {
      char *callee;
      operand *args;
      size_t arg_count;
    } call;
    struct {
      struct basic_block *target;
    } branch;
    struct {
      operand condition;
      struct basic_block *true_target;
      struct basic_block *false_target;
    } conditional_branch;
    struct {
      phi_entry *entries;
      size_t count;
    } phi;
  } data;
  struct instruction *next;
} instruction;

typedef struct basic_block {
  unsigned id;
  instruction *first;
  instruction *last;
  ArrayList successors;
  ArrayList predecessors;
  bool terminated;
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
  ArrayList params;
  // List of values inside the function.
  ArrayList values;
  // List of names for the values.
  ArrayList names;
  // First block of the function.
  basic_block *entry_block;
  // All the blocks of the function.
  basic_block *blocks;
  // Last block of the function.
  basic_block *last_block;
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
  DiagnosticList *diagnostics;
} ssa;

// Lowers a semantically validated program.
ssa *generate_ssair(AST *root, const SemanticResult *semantic, DiagnosticList *diagnostics);
void print_ssair(FILE *out, const ssa *ir);
void free_ssair(ssa *ir);

#endif

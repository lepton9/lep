#ifndef SYMTAB_H
#define SYMTAB_H

#include "../include/ast.h"
#include "../include/hashtab.h"
#include "../include/LList.h"
#include <stdio.h>

typedef enum {
  INT = 200,
  FLOAT,
  CHAR,
  STR,
  BOOL,
  F,
  VOID
} TYPE;

typedef struct {
  char* name;
  TYPE type;
} parameter;

typedef struct {
  int ret_type;
  size_t n_params;
  parameter* params;
} func_info;

typedef struct {
  char* name;
  TYPE type;
  int declLine;
  bool is_initialized;
  func_info* f_info;
} stEntry;

typedef hashtab symtab;

typedef struct {
  LList* s;
  int cur_scope;
} symtabStack;

symtabStack* init_st_stack();
void free_st_stack(symtabStack* sts);

symtab* initSymbolTable();
void freeSymbolTable(symtab* st);
void free_stEntry(stEntry* entry);

stEntry* st_insert(symtab* st, const char* key);
stEntry* st_lookup(symtab* st, const char* identifier);

void enter_scope(symtabStack *sts);
void exit_scope(symtabStack *sts);

const char* typeToStr(const TYPE type);

void print_symtab(FILE* out, const symtab* st);
void print_stEntry(FILE* out, const stEntry* e);


#endif

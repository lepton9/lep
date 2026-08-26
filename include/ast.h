#ifndef AST_H
#define AST_H

#include "../include/token.h"

typedef struct AST_NODE {
  enum {
    AST_PROGRAM = 100,
    AST_MAIN,
    AST_BLOCK,
    AST_STATEMENT,
    AST_VARIABLE,
    AST_TYPE,
    AST_FCALL,
    AST_ARGS,
    AST_FUNCTION,
    AST_PARAMETERS,
    AST_PARAMETER,
    AST_FHEADER,
    AST_FBODY,
    AST_FARGUMENTS,
    AST_ASSIGNMENT,
    AST_EXPR,
    AST_ID,
    AST_VALUE,
    AST_OPERATOR,
    AST_RET,
  } type;

  token* tok;

  struct AST_NODE* l;
  struct AST_NODE* r;
  struct AST_NODE* next;
} AST;

AST *initAST(int type, token* t);
void freeAST(AST *ast);
void printAST(AST* ast, int indent);
void print_ast(AST* ast, int indent);

#endif

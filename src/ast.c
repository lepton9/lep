#include "../include/ast.h"
#include <stdio.h>
#include <stdlib.h>

AST *initAST(int type, token *t) {
  AST *ast = malloc(sizeof(AST));
  ast->type = type;
  // ast->tok = malloc(sizeof(token));
  ast->tok = t;
  ast->l = NULL;
  ast->r = NULL;
  ast->next = NULL;
  return ast;
}

void freeAST(AST *ast) {
  if (ast) {
    freeAST(ast->l);
    freeAST(ast->r);
    freeAST(ast->next);
    free(ast);
  }
}

void printAST(AST *node, int indent) {
  if (!node)
    return;

  for (int i = 0; i < indent; i++)
    printf("  ");

  switch (node->type) {
    case AST_PROGRAM:
      printf("Program\n");
      break;
    case AST_FUNCTION:
      printf("Function\n");
      break;
    case AST_STATEMENT:
      printf("Statement\n");
      break;
    case AST_FHEADER:
      printf("Header\n");
      break;
    case AST_FBODY:
      printf("Body\n");
      break;
    case AST_PARAMETERS:
      printf("Parameters\n");
      break;
    case AST_PARAMETER:
      printf("Parameter\n");
      break;
    case AST_FARGUMENTS:
      printf("Arguments\n");
      break;
    case AST_FCALL:
      printf("FCall\n");
      break;
    case AST_VARIABLE:
      printf("Variable\n");
      break;
    case AST_ASSIGNMENT:
      printf("Assignment\n");
      break;
    case AST_RET:
      printf("Return\n");
      break;
    case AST_EXPR:
      printf("Expression\n");
      break;
    case AST_TYPE:
      printf("Type: %s\n", (char*)node->tok->value);
      break;
    case AST_ID:
      printf("Identifier: %s\n", node->tok->value);
      break;
    case AST_MAIN:
      printf("MAIN: %s\n", node->tok->value);
      break;
    case AST_OPERATOR:
      printf("Operator: %s\n", node->tok->value);
      break;
    case AST_VALUE:
      printf("Value: %s\n", node->tok->value);
      break;
    default:
      printf("Undefined\n");
  }

  if (indent == 0) indent++;

  printAST(node->l, indent + 1);
  printAST(node->r, indent + 1);
  printAST(node->next, indent);
}

void print_ast(AST* node, int indent) {
  if (!node)
    return;
  for (int i = 0; i < indent; i++)
    printf("  ");

  if (node->tok) {
    printf("%s\n", node->tok->value);
  } else {
    printf("U\n");
  }

  print_ast(node->l, indent + 1);
  print_ast(node->r, indent + 1);
  print_ast(node->next, indent);
}

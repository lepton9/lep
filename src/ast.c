#include "../include/ast.h"
#include <stdio.h>
#include <stdlib.h>

AST *init_ast(int type, const token *t) {
  AST *ast = malloc(sizeof(AST));
  ast->type = type;
  ast->has_token = t != NULL;
  if (t) ast->token = *t;
  ast->l = NULL;
  ast->r = NULL;
  ast->next = NULL;
  return ast;
}

void free_ast(AST *ast) {
  if (ast) {
    free_ast(ast->l);
    free_ast(ast->r);
    free_ast(ast->next);
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
      printf("Type: %.*s\n", (int)node->token.value.length, node->token.value.start);
      break;
    case AST_ID:
      printf("Identifier: %.*s\n", (int)node->token.value.length, node->token.value.start);
      break;
    case AST_MAIN:
      printf("MAIN: %.*s\n", (int)node->token.value.length, node->token.value.start);
      break;
    case AST_OPERATOR:
      printf("Operator: %.*s\n", (int)node->token.value.length, node->token.value.start);
      break;
    case AST_VALUE:
      printf("Value: %.*s\n", (int)node->token.value.length, node->token.value.start);
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

  if (node->has_token) {
    printf("%.*s\n", (int)node->token.value.length, node->token.value.start);
  } else {
    printf("U\n");
  }

  print_ast(node->l, indent + 1);
  print_ast(node->r, indent + 1);
  print_ast(node->next, indent);
}

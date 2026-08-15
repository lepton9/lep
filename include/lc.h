#ifndef LC_H
#define LC_H
#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/ast.h"

typedef struct {
  Lexer *lexer;
  parser *parser;
  AST *root;
} lc;

lc *initLC();
void freeLC(lc *lc);

bool readSrcFile(const char *fileName, char **buffer, int *length);
void lclex(lc *lc);
void lcparse(lc *lc);
void lccompile(lc *lc);

#endif

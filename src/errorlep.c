#include "../include/errorlep.h"
#include <stdio.h>


void print_errline(Lexer* l, const cLoc loc) {
  char *line = getLine(l->src, l->srcLen, loc.line);
  fprintf(ERR_OUT, "%d: %s\n", loc.line, line);
  free(line);
  fprintf(ERR_OUT, "%*c^\n", loc.column + digits(loc.line) + 1, ' ');
}

void error_lex(Lexer* l, const char* msg) {
  fprintf(ERR_OUT, "[LexError] %s\n", msg);
  exit(1);
}

void error_parse(parser* p, const char *msg, const char* expected) {
  if (expected) {
    fprintf(ERR_OUT, "[SyntaxError] %s : %s != %s, at L%-2d C%d\n", msg, tokenTypeToStr(p->token->type), expected, p->token->loc.line, p->token->loc.column);
  } else {
    fprintf(ERR_OUT, "[SyntaxError] %s : %s, at L%-2d C%d\n", msg, tokenTypeToStr(p->token->type), p->token->loc.line, p->token->loc.column);
  }
  print_errline(p->lexer, p->token->loc);
  exit(1);
}

void error_semantic(const char* msg, const cLoc loc) {
  fprintf(ERR_OUT, "[Error] %s, line %d\n", msg, loc.line);
  exit(1);
}

void error_ret(const char* msg, const char* id) {
  fprintf(ERR_OUT, "[RetError] Non void function '%s' %s\n", id, msg);
  exit(1);
}

void error_redef(const char* id, const TYPE type, const cLoc loc) {
  fprintf(ERR_OUT, "[Error] Redefinition of '%s:%s' on line %d\n", typeToStr(type), id, loc.line);
  exit(1);
}

void error_fnotfound(const char* id, const cLoc loc) {
  fprintf(ERR_OUT, "[DeclError] No function named '%s' found, line %d\n", id, loc.line);
  exit(1);
}

void error_nodecl(const char* msg, const char* id, const cLoc loc) {
  fprintf(ERR_OUT, "[DeclError] Variable '%s' not declared before use (%s), line %d\n", id, msg, loc.line);
  exit(1);
}

void error_value(const char* id, const TYPE type, const cLoc loc) {
  fprintf(ERR_OUT, "[ValueError] Variable '%s:%s' has no assigned value, line %d\n", typeToStr(type), id, loc.line);
  exit(1);
}

void error_type(const char* msg, const cLoc loc, const TYPE actual, const TYPE expected) {
  fprintf(ERR_OUT, "[TypeError] %s (%s != %s), line %d\n"
          , msg, typeToStr(actual), typeToStr(expected), loc.line);
  exit(1);
}

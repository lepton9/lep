#include "../include/token.h"
#include <stdlib.h>
#include <stdio.h>

token *makeToken(const tokenType type, const char *value, const cLoc codeLoc) {
  token *tok = malloc(sizeof(token));
  *tok = (token){type, (char*)value, codeLoc};
  return tok;
}

void freeToken(token *tok) {
  if (tok == NULL) return;
  free(tok->value);
  free(tok);
}

const char* tokenTypeToStr(tokenType type) {
  switch (type) {
    case T_ARROW:
      return "T_ARROW";
    case T_SPACE:
      return "T_SPACE";
    case T_NEWLINE:
      return "T_NEWLINE";
    case T_OPERATOR:
      return "T_OPERATOR";
    case T_COLON:
      return "T_COLON";
    case T_SEMICOLON:
      return "T_SEMICOLON";
    case T_PAREN_L:
      return "T_PAREN_L";
    case T_PAREN_R:
      return "T_PAREN_R";
    case T_BRACE_L:
      return "T_BRACE_L";
    case T_BRACE_R:
      return "T_BRACE_R";
    case T_DOT:
      return "T_DOT";
    case T_COMMA:
      return "T_COMMA";
    case T_EQUALS:
      return "T_EQUALS";
    case T_PLUS:
      return "T_PLUS";
    case T_MINUS:
      return "T_MINUS";
    case T_ASTERISK:
      return "T_ASTERISK";
    case T_SLASH:
      return "T_SLASH";
    case T_IDENTIFIER:
      return "T_IDENTIFIER";
    case T_LIT_INT:
      return "T_LIT_INT";
    case T_LIT_CHAR:
      return "T_LIT_CHAR";
    case T_LIT_BOOL:
      return "T_LIT_BOOL";
    case T_LIT_STR:
      return "T_LIT_STR";
    case T_LIT_FLOAT:
      return "T_LIT_FLOAT";
    case T_KW_INT:
      return "T_KW_INT";
    case T_KW_F:
      return "T_KW_F";
    case T_KW_CHAR:
      return "T_KW_CHAR";
    case T_KW_BOOL:
      return "T_KW_BOOL";
    case T_KW_STR:
      return "T_KW_STR";
    case T_KW_FLOAT:
      return "T_KW_FLOAT";
    case T_KW_VOID:
      return "T_KW_VOID";
    case T_KW_TRUE:
      return "T_KW_TRUE";
    case T_KW_FALSE:
      return "T_KW_FALSE";
    case T_KW_MAIN:
      return "T_KW_MAIN";
    case T_KW_RET:
      return "T_KW_RET";
    case T_ERROR:
      return "T_ERROR";
    case T_EOF:
      return "T_EOF";
  }
  return "UNDEFINED";
}


void printToken(token *tok) {
  printf("%-13s %-10s L%-2d C%d\n", tokenTypeToStr(tok->type), tok->value, tok->loc.line, tok->loc.column);
}

#include "../include/token.h"
#include <stdio.h>

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
    case T_EQ: return "T_EQ";
    case T_NEQ: return "T_NEQ";
    case T_LT: return "T_LT";
    case T_LE: return "T_LE";
    case T_GT: return "T_GT";
    case T_GE: return "T_GE";
    case T_BANG: return "T_BANG";
    case T_AND: return "T_AND";
    case T_OR: return "T_OR";
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
    case T_KW_IF: return "T_KW_IF";
    case T_KW_ELSE: return "T_KW_ELSE";
    case T_KW_WHILE: return "T_KW_WHILE";
    case T_ERROR:
      return "T_ERROR";
    case T_EOF:
      return "T_EOF";
  }
  return "UNDEFINED";
}


void printToken(token *tok) {
  printf("%-13s %.*s L%-2d C%d\n", tokenTypeToStr(tok->type), (int)tok->value.length,
         tok->value.start, tok->loc.line, tok->loc.column);
}

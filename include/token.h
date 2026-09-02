#ifndef TOKEN_H
#define TOKEN_H

#include "../include/strview.h"

#define COMMENT_CHAR '#'

typedef enum {
  T_ARROW,
  T_COLON,
  T_SEMICOLON,
  T_PAREN_L,
  T_PAREN_R,
  T_BRACE_L,
  T_BRACE_R,
  T_DOT,
  T_COMMA,
  T_SPACE,
  T_NEWLINE,

  // Operators
  T_OPERATOR,
  T_EQUALS,
  T_EQ,
  T_NEQ,
  T_LT,
  T_LE,
  T_GT,
  T_GE,
  T_BANG,
  T_AND,
  T_OR,
  T_PLUS,
  T_MINUS,
  T_ASTERISK,
  T_SLASH,

  // Literals
  T_IDENTIFIER,
  T_LIT_INT,
  T_LIT_CHAR,
  T_LIT_BOOL,
  T_LIT_STR,
  T_LIT_FLOAT,

  // Keywords
  T_KW_INT,
  T_KW_F,
  T_KW_CHAR,
  T_KW_BOOL,
  T_KW_STR,
  T_KW_FLOAT,
  T_KW_VOID,
  T_KW_TRUE,
  T_KW_FALSE,
  T_KW_MAIN,
  T_KW_RET,
  T_KW_IF,
  T_KW_ELSE,
  T_KW_WHILE,

  T_ERROR,

  T_EOF

} tokenType;

typedef struct {
  int line;
  int column;
} cLoc;

typedef struct {
  tokenType type;
  cLoc loc;
  strview value;
} token;

static inline strview token_view(const token *tok) {
  return tok->value;
}

static inline char *token_strdup(const token *tok) {
  return strview_strdup(token_view(tok));
}

const char* tokenTypeToStr(tokenType type);
void printToken(token* token);

#endif

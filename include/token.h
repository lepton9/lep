#ifndef TOKEN_H
#define TOKEN_H

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

  T_ERROR,

  T_EOF

} tokenType;

// char keywords[20] = {"int", "float", "char", "bool", "str", "void", "f", "while", "return", "main", "if", "else"};

typedef struct {
  int line;
  int column;
} cLoc;

typedef struct {
  tokenType type;
  char *value;
  cLoc loc;
} token;

// Maybe better
// typedef struct {
//   tokenType type;
//   int lenght;
//   CLoc loc; // start of the token
// } Token;

token *makeToken(const tokenType type, const char *value, const cLoc codeLoc);
const char* tokenTypeToStr(tokenType type);
void printToken(token* token);

#endif

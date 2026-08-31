#ifndef LEXER_H
#define LEXER_H

#include "../include/LList.h"
#include "../include/token.h"
#include "../include/diagnostic.h"
#include <stdbool.h>
#include <string.h>

#define COMMENT_CHAR '#'

typedef struct {
  LList *tokens;
  char *src;
  int srcLen;
  int srcPos;
  cLoc codeLoc;
  DiagnosticList *diagnostics;
} Lexer;

Lexer *initLexer(DiagnosticList *diagnostics);
void freeLexer(Lexer *lexer);

LList* lex(Lexer *lexer); // Tokenize

token *getNextToken(Lexer *lexer);
void addToken(Lexer *lexer, token *token);
bool atEnd(Lexer *lexer);
char peek(Lexer *lexer);
char advance(Lexer *lexer);
void nextLine(Lexer *lexer);
token* makeTokenN(Lexer *lexer, const tokenType type, const int beg, const cLoc cl);

void printTokens(Lexer *lexer);

char* malStrncpy(const char *s, const size_t n);

#endif

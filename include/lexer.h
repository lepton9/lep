#ifndef LEXER_H
#define LEXER_H

#include "../include/token.h"
#include "../include/diagnostic.h"
#include <stdbool.h>
#include <string.h>

#define COMMENT_CHAR '#'

typedef struct {
  char *src;
  int srcLen;
  int srcPos;
  cLoc codeLoc;
  DiagnosticList *diagnostics;
} Lexer;

Lexer *init_lexer(DiagnosticList *diagnostics);
void free_lexer(Lexer *lexer);

token next_token(Lexer *lexer);

static inline bool at_end(const Lexer *lexer) {
  return lexer->srcPos >= lexer->srcLen;
}

static inline char peek(const Lexer *lexer) {
  return at_end(lexer) ? '\0' : lexer->src[lexer->srcPos];
}

static inline char advance(Lexer *lexer) {
  if (at_end(lexer)) return '\0';
  lexer->codeLoc.column++;
  return lexer->src[lexer->srcPos++];
}

void next_line(Lexer *lexer);

#endif

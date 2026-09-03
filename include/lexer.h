#ifndef LEXER_H
#define LEXER_H

#include "../include/token.h"
#include "../include/diagnostic.h"
#include <stdbool.h>
#include <string.h>

#define COMMENT_CHAR '#'

typedef struct {
  // Borrowed source file content.
  const char *src;
  // Length of the source file.
  size_t srcLen;
  // Current index in the source file.
  size_t srcPos;
  // Current code location in the source file.
  cLoc codeLoc;
  DiagnosticList *diagnostics;
} Lexer;

Lexer init_lexer(DiagnosticList *diagnostics);
void reset_lexer(Lexer *lexer);
void set_source_file(Lexer* lexer, const char *src, size_t src_len);

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

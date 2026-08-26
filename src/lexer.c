#include "../include/lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

Lexer *initLexer(DiagnosticList *diagnostics) {
  Lexer *lexer = malloc(sizeof(Lexer));
  lexer->tokens = create_list();
  lexer->src = NULL;
  lexer->srcLen = 0;
  lexer->srcPos = 0;
  lexer->codeLoc.line = 1;
  lexer->codeLoc.column = 1;
  lexer->diagnostics = diagnostics;
  return lexer;
}

void freeLexer(Lexer *lexer) {
  while (!is_empty(lexer->tokens)) freeToken(pop_front(lexer->tokens));
  free(lexer->tokens);
  free(lexer->src);
  free(lexer);
}

LList* lex(Lexer *lexer) {
  token *tok;
  while (!atEnd(lexer)) {
    tok = getNextToken(lexer);
    if (tok) {
      addToken(lexer, tok);
    }
  }

  token *t_eof = makeToken(T_EOF, NULL, lexer->codeLoc);
  addToken(lexer, t_eof);
  return lexer->tokens;
}

void comment(Lexer* lexer) {
  while (!atEnd(lexer) && peek(lexer) != '\n') advance(lexer);
}

static void lexer_error(Lexer *lexer, cLoc location, const char *message) {
  add_diagnostic(lexer->diagnostics, DIAGNOSTIC_ERROR, "lex", &location, "%s", message);
}

token *getNextToken(Lexer *lexer) {
  int begI = lexer->srcPos;
  cLoc cLocB = lexer->codeLoc;
  char c = advance(lexer);
  char* tv;
  switch (c) {
    case ' ':
    case 9: // Tab
      // token = makeToken(T_SPACE, c, cLocB);
      return NULL;
    case '\n':
      // token = makeToken(T_NEWLINE, c, cLocB);
      nextLine(lexer);
      return NULL;
    case '#':
      comment(lexer);
      return NULL;
    case ':': return makeTokenN(lexer, T_COLON, begI, cLocB);
    case ';': return makeTokenN(lexer, T_SEMICOLON, begI, cLocB);
    case '(': return makeTokenN(lexer, T_PAREN_L, begI, cLocB);
    case ')': return makeTokenN(lexer, T_PAREN_R, begI, cLocB);
    case '{': return makeTokenN(lexer, T_BRACE_L, begI, cLocB);
    case '}': return makeTokenN(lexer, T_BRACE_R, begI, cLocB);
    case '.': return makeTokenN(lexer, T_DOT, begI, cLocB);
    case ',': return makeTokenN(lexer, T_COMMA, begI, cLocB);
    case '=': return makeTokenN(lexer, T_EQUALS, begI, cLocB);
    case '+': return makeTokenN(lexer, T_PLUS, begI, cLocB);
    case '*': return makeTokenN(lexer, T_ASTERISK, begI, cLocB);
    case '/': return makeTokenN(lexer, T_SLASH, begI, cLocB);
    case '-':
      if (peek(lexer) == '>') {
        advance(lexer);
        return makeTokenN(lexer, T_ARROW, begI, cLocB);
      }
      return makeTokenN(lexer, T_MINUS, begI, cLocB);
    case '\"':
      while (!atEnd(lexer) && peek(lexer) != '\"') advance(lexer);
      if (atEnd(lexer)) {
        lexer_error(lexer, cLocB, "unterminated string literal");
        return NULL;
      }
      advance(lexer);
      tv = malStrncpy(lexer->src + begI, lexer->srcPos - begI);
      return makeToken(T_LIT_STR, tv, cLocB);
    case '\'':
      if (atEnd(lexer) || advance(lexer) == '\'' || atEnd(lexer) || advance(lexer) != '\'') {
        lexer_error(lexer, cLocB, "invalid character literal");
        return NULL;
      }
      tv = malStrncpy(lexer->src + begI, lexer->srcPos - begI);
      return makeToken(T_LIT_CHAR, tv, cLocB);
    default: {
      if (isalpha((unsigned char)c)) {
        while (isalpha(peek(lexer))) {
          advance(lexer);
        }
        tv = malStrncpy(lexer->src + begI, lexer->srcPos - begI);
        tokenType tt = isKeyword(tv);
        if (tt == T_KW_TRUE || tt == T_KW_FALSE) tt = T_LIT_BOOL;
        // tt = (tt >= 0) ? tt : T_IDENTIFIER;
        return makeToken(tt, tv, cLocB);
      }
      else if (isdigit((unsigned char)c)) {
        tokenType tt = T_LIT_INT;
        while (isdigit((unsigned char)peek(lexer))) advance(lexer);
        if (peek(lexer) == '.') {
          advance(lexer);
          if (isdigit(peek(lexer))) {
            tt = T_LIT_FLOAT;
            while (isdigit((unsigned char)peek(lexer))) advance(lexer);
          } else {
            lexer_error(lexer, cLocB, "invalid float literal");
            return NULL;
          }
        }
        return makeTokenN(lexer, tt, begI, cLocB);
      }
      break;
    }
  }

  lexer_error(lexer, cLocB, "unrecognized token");
  return NULL;
}

tokenType isKeyword(const char* value) {
  if (strcmp(value, "int") == 0) return T_KW_INT;
  if (strcmp(value, "char") == 0) return T_KW_CHAR;
  if (strcmp(value, "bool") == 0) return T_KW_BOOL;
  if (strcmp(value, "str") == 0) return T_KW_STR;
  if (strcmp(value, "float") == 0) return T_KW_FLOAT;
  if (strcmp(value, "void") == 0) return T_KW_VOID;
  if (strcmp(value, "f") == 0) return T_KW_F;
  if (strcmp(value, "true") == 0) return T_KW_TRUE;
  if (strcmp(value, "false") == 0) return T_KW_FALSE;
  if (strcmp(value, "main") == 0) return T_KW_MAIN;
  if (strcmp(value, "return") == 0) return T_KW_RET;
  return T_IDENTIFIER;
}

char* malStrncpy(const char *s, const size_t n) {
  char *d = malloc(n + 1);
  assert(d != NULL);
  strncpy(d, s, n);
  d[n] = '\0';
  return d;
}

token* makeTokenN(Lexer* lexer, const tokenType type, const int beg, const cLoc cl) {
  char* tval = malStrncpy(lexer->src + beg, lexer->srcPos - beg);
  return makeToken(type, tval, cl);
}

void printTokens(Lexer *lexer) {
  for (node *head=lexer->tokens->head; head != NULL; head = head->next) {
    printToken(head->data);
  }
}

void addToken(Lexer *lexer, token *token) { add_to_end(lexer->tokens, token); }

bool atEnd(Lexer *lexer) { return lexer->srcPos >= lexer->srcLen; }

char peek(Lexer *lexer) {
  if (atEnd(lexer)) {
    return '\0';
  }
  return lexer->src[lexer->srcPos];
  // return lexer->src[lexer->srcPos + 1];
}

char advance(Lexer *lexer) {
  if (atEnd(lexer)) return '\0';
  lexer->codeLoc.column++;
  return lexer->src[lexer->srcPos++];
}

void nextLine(Lexer *lexer) {
  lexer->codeLoc.line++;
  lexer->codeLoc.column = 1;
}

#include "../include/lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

struct Keyword {
  const char *name;
  tokenType type;
};

static const struct Keyword keywords[] = {
  {"int", T_KW_INT},
  {"char", T_KW_CHAR},
  {"bool", T_KW_BOOL},
  {"str", T_KW_STR},
  {"float", T_KW_FLOAT},
  {"void", T_KW_VOID},
  {"f", T_KW_F},
  {"true", T_KW_TRUE},
  {"false", T_KW_FALSE},
  {"main", T_KW_MAIN},
  {"return", T_KW_RET},
  {"if", T_KW_IF},
  {"else", T_KW_ELSE},
  {"while", T_KW_WHILE},
};

#define KEYWORD_TABLE_SIZE 64

static unsigned char keyword_slots[KEYWORD_TABLE_SIZE];
static bool keyword_table_initialized = false;

static size_t keyword_hash(const char *value) {
  size_t hash = 5381;
  unsigned char c;

  while ((c = (unsigned char)*value++) != '\0') {
    hash = ((hash << 5) + hash) + c;
  }

  return hash;
}

static void keyword_table_init(void) {
  if (keyword_table_initialized) return;

  for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
    size_t slot = keyword_hash(keywords[i].name) % KEYWORD_TABLE_SIZE;

    while (keyword_slots[slot] != 0) {
      slot = (slot + 1) % KEYWORD_TABLE_SIZE;
    }

    keyword_slots[slot] = (unsigned char)(i + 1);
  }

  keyword_table_initialized = true;
}

static tokenType lookup_keyword(const char *value) {
  size_t slot = keyword_hash(value) % KEYWORD_TABLE_SIZE;

  while (1) {
    unsigned char entry = keyword_slots[slot];
    if (entry == 0) return T_IDENTIFIER;

    const struct Keyword *keyword = &keywords[entry - 1];
    if (strcmp(value, keyword->name) == 0) return keyword->type;

    slot = (slot + 1) % KEYWORD_TABLE_SIZE;
  }

  return T_IDENTIFIER;
}

Lexer *initLexer(DiagnosticList *diagnostics) {
  keyword_table_init();

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
  // TODO: tokenize while parsing. No need to tokenize the whole file
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
    case '=':
      if (peek(lexer) == '=') {
        advance(lexer);
        return makeTokenN(lexer, T_EQ, begI, cLocB);
      }
      return makeTokenN(lexer, T_EQUALS, begI, cLocB);
    case '!':
      if (peek(lexer) == '=') {
        advance(lexer);
        return makeTokenN(lexer, T_NEQ, begI, cLocB);
      }
      return makeTokenN(lexer, T_BANG, begI, cLocB);
    case '<':
      if (peek(lexer) == '=') {
        advance(lexer);
        return makeTokenN(lexer, T_LE, begI, cLocB);
      }
      return makeTokenN(lexer, T_LT, begI, cLocB);
    case '>':
      if (peek(lexer) == '=') {
        advance(lexer);
        return makeTokenN(lexer, T_GE, begI, cLocB);
      }
      return makeTokenN(lexer, T_GT, begI, cLocB);
    case '&':
      if (peek(lexer) == '&') {
        advance(lexer);
        return makeTokenN(lexer, T_AND, begI, cLocB);
      }
      lexer_error(lexer, cLocB, "expected '&' after '&'");
      return NULL;
    case '|':
      if (peek(lexer) == '|') {
        advance(lexer);
        return makeTokenN(lexer, T_OR, begI, cLocB);
      }
      lexer_error(lexer, cLocB, "expected '|' after '|'");
      return NULL;
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
        tokenType tt = lookup_keyword(tv);
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

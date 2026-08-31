#include "../include/lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
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

static size_t keyword_hash(const char *value, size_t length) {
  size_t hash = 5381;

  for (size_t i = 0; i < length; i++) {
    hash = ((hash << 5) + hash) + (unsigned char)value[i];
  }

  return hash;
}

static void keyword_table_init(void) {
  if (keyword_table_initialized) return;

  for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++) {
    size_t slot = keyword_hash(keywords[i].name, strlen(keywords[i].name)) % KEYWORD_TABLE_SIZE;

    while (keyword_slots[slot] != 0) {
      slot = (slot + 1) % KEYWORD_TABLE_SIZE;
    }

    keyword_slots[slot] = (unsigned char)(i + 1);
  }

  keyword_table_initialized = true;
}

static tokenType lookup_keyword(const char *value, size_t length) {
  size_t slot = keyword_hash(value, length) % KEYWORD_TABLE_SIZE;

  while (1) {
    unsigned char entry = keyword_slots[slot];
    if (entry == 0) return T_IDENTIFIER;

    const struct Keyword *keyword = &keywords[entry - 1];
    if (strlen(keyword->name) == length && memcmp(value, keyword->name, length) == 0) {
      return keyword->type;
    }

    slot = (slot + 1) % KEYWORD_TABLE_SIZE;
  }

  return T_IDENTIFIER;
}

Lexer *init_lexer(DiagnosticList *diagnostics) {
  keyword_table_init();

  Lexer *lexer = malloc(sizeof(Lexer));
  lexer->src = NULL;
  lexer->srcLen = 0;
  lexer->srcPos = 0;
  lexer->codeLoc.line = 1;
  lexer->codeLoc.column = 1;
  lexer->diagnostics = diagnostics;
  return lexer;
}

void free_lexer(Lexer *lexer) {
  free(lexer->src);
  free(lexer);
}

void comment(Lexer* lexer) {
  while (!at_end(lexer) && peek(lexer) != '\n') advance(lexer);
}

static void lexer_error(Lexer *lexer, cLoc location, const char *message) {
  add_diagnostic(lexer->diagnostics, DIAGNOSTIC_ERROR, "lex", &location, "%s", message);
}

static token make_token(const Lexer *lexer, tokenType type, int beg, cLoc location) {
  return (token){.type = type, .loc = location, .start = lexer->src + beg,
                 .length = lexer->srcPos - beg};
}

token next_token(Lexer *lexer) {
  while (!at_end(lexer)) {
    int begI = lexer->srcPos;
    cLoc cLocB = lexer->codeLoc;
    char c = advance(lexer);
    switch (c) {
    case ' ':
    case 9: // Tab
      continue;
    case '\n':
      next_line(lexer);
      continue;
    case '#':
      comment(lexer);
      continue;
    case ':': return make_token(lexer, T_COLON, begI, cLocB);
    case ';': return make_token(lexer, T_SEMICOLON, begI, cLocB);
    case '(': return make_token(lexer, T_PAREN_L, begI, cLocB);
    case ')': return make_token(lexer, T_PAREN_R, begI, cLocB);
    case '{': return make_token(lexer, T_BRACE_L, begI, cLocB);
    case '}': return make_token(lexer, T_BRACE_R, begI, cLocB);
    case '.': return make_token(lexer, T_DOT, begI, cLocB);
    case ',': return make_token(lexer, T_COMMA, begI, cLocB);
    case '=':
      if (peek(lexer) == '=') {
        advance(lexer);
        return make_token(lexer, T_EQ, begI, cLocB);
      }
      return make_token(lexer, T_EQUALS, begI, cLocB);
    case '!':
      if (peek(lexer) == '=') {
        advance(lexer);
        return make_token(lexer, T_NEQ, begI, cLocB);
      }
      return make_token(lexer, T_BANG, begI, cLocB);
    case '<':
      if (peek(lexer) == '=') {
        advance(lexer);
        return make_token(lexer, T_LE, begI, cLocB);
      }
      return make_token(lexer, T_LT, begI, cLocB);
    case '>':
      if (peek(lexer) == '=') {
        advance(lexer);
        return make_token(lexer, T_GE, begI, cLocB);
      }
      return make_token(lexer, T_GT, begI, cLocB);
    case '&':
      if (peek(lexer) == '&') {
        advance(lexer);
        return make_token(lexer, T_AND, begI, cLocB);
      }
      lexer_error(lexer, cLocB, "expected '&' after '&'");
      continue;
    case '|':
      if (peek(lexer) == '|') {
        advance(lexer);
        return make_token(lexer, T_OR, begI, cLocB);
      }
      lexer_error(lexer, cLocB, "expected '|' after '|'");
      continue;
    case '+': return make_token(lexer, T_PLUS, begI, cLocB);
    case '*': return make_token(lexer, T_ASTERISK, begI, cLocB);
    case '/': return make_token(lexer, T_SLASH, begI, cLocB);
    case '-':
      if (peek(lexer) == '>') {
        advance(lexer);
        return make_token(lexer, T_ARROW, begI, cLocB);
      }
      return make_token(lexer, T_MINUS, begI, cLocB);
    case '\"':
      while (!at_end(lexer) && peek(lexer) != '\"') advance(lexer);
      if (at_end(lexer)) {
        lexer_error(lexer, cLocB, "unterminated string literal");
        continue;
      }
      advance(lexer);
      return make_token(lexer, T_LIT_STR, begI, cLocB);
    case '\'':
      if (at_end(lexer) || advance(lexer) == '\'' || at_end(lexer) || advance(lexer) != '\'') {
        lexer_error(lexer, cLocB, "invalid character literal");
        continue;
      }
      return make_token(lexer, T_LIT_CHAR, begI, cLocB);
    default: {
      if (isalpha((unsigned char)c)) {
        while (isalpha(peek(lexer))) {
          advance(lexer);
        }
        tokenType tt = lookup_keyword(lexer->src + begI, (size_t)(lexer->srcPos - begI));
        if (tt == T_KW_TRUE || tt == T_KW_FALSE) tt = T_LIT_BOOL;
        return make_token(lexer, tt, begI, cLocB);
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
            continue;
          }
        }
        return make_token(lexer, tt, begI, cLocB);
      }
      break;
    }
  }

    lexer_error(lexer, cLocB, "unrecognized token");
  }

  return (token){.type = T_EOF, .loc = lexer->codeLoc,
                 .start = lexer->src + lexer->srcPos, .length = 0};
}

void next_line(Lexer *lexer) {
  lexer->codeLoc.line++;
  lexer->codeLoc.column = 1;
}

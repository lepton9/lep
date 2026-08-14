#include "../include/lexer.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <string.h>

Lexer *initLexer() {
  Lexer *lexer = malloc(sizeof(Lexer));
  lexer->tokens = create_list();
  lexer->errors = create_list();
  lexer->srcLen = 0;
  lexer->srcPos = 0;
  lexer->codeLoc.line = 1;
  lexer->codeLoc.column = 1;
  return lexer;
}

void freeLexer(Lexer *lexer) {
  list_clear(lexer->tokens);
  free(lexer->tokens);
  free(lexer->errors); // Add free err->msg
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
  while(peek(lexer) != '\n') {
    getNextToken(lexer);
  }
}

token *getNextToken(Lexer *lexer) {
  int begI = lexer->srcPos;
  cLoc cLocB = lexer->codeLoc;
  char* c = malloc(sizeof(char) + 1);
  char* tv;
  c[1] = '\0',
  *c = advance(lexer);
  switch (*c) {
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
    case ':': return makeToken(T_COLON, c, cLocB);
    case ';': return makeToken(T_SEMICOLON, c, cLocB);
    case '(': return makeToken(T_PAREN_L, c, cLocB);
    case ')': return makeToken(T_PAREN_R, c, cLocB);
    case '{': return makeToken(T_BRACE_L, c, cLocB);
    case '}': return makeToken(T_BRACE_R, c, cLocB);
    case '.': return makeToken(T_DOT, c, cLocB);
    case ',': return makeToken(T_COMMA, c, cLocB);
    case '=': return makeToken(T_EQUALS, c, cLocB);
    case '+': return makeToken(T_PLUS, c, cLocB);
    case '*': return makeToken(T_ASTERISK, c, cLocB);
    case '/': return makeToken(T_SLASH, c, cLocB);
    case '-':
      if (peek(lexer) == '>') {
        advance(lexer);
        return makeTokenN(lexer, T_ARROW, begI, cLocB);
      }
      return makeToken(T_MINUS, c, cLocB);
    case '\"':
      while (advance(lexer) != '\"') {
        if (atEnd(lexer)) {
          addSynError(lexer, lexerError(lexer, "No closing \"", begI, lexer->srcPos - begI));
          break;
        }
      }
      tv = malStrncpy(lexer->src + begI, lexer->srcPos - begI);
      return makeToken(T_LIT_STR, tv, cLocB);
    case '\'':
      if (advance(lexer) != '\'') {
        if (advance(lexer) != '\'') {
          addSynError(lexer, lexerError(lexer, "Invalid char", begI, lexer->srcPos - begI));
        }
      }
      tv = malStrncpy(lexer->src + begI, lexer->srcPos - begI);
      return makeToken(T_LIT_CHAR, tv, cLocB);
    default: {
      if (isalpha(*c)) {
        while (isalpha(peek(lexer))) {
          advance(lexer);
        }
        tv = malStrncpy(lexer->src + begI, lexer->srcPos - begI);
        tokenType tt = isKeyword(lexer, tv);
        if (tt == T_KW_TRUE || tt == T_KW_FALSE) tt = T_LIT_BOOL;
        // tt = (tt >= 0) ? tt : T_IDENTIFIER;
        return makeToken(tt, tv, cLocB);
      }
      else if (isdigit(*c)) {
        tokenType tt = T_LIT_INT;
        while ((*c = peek(lexer)) && isdigit(*c)) {
          advance(lexer);
        }
        if (peek(lexer) == '.') {
          advance(lexer);
          if (isdigit(peek(lexer))) {
            tt = T_LIT_FLOAT;
            while ((*c = peek(lexer)) && isdigit(*c)) {
              advance(lexer);
            }
          } else {
            addSynError(lexer, lexerError(lexer, "Invalid float", begI, lexer->srcPos - begI));
            return NULL;
          }
        }
        return makeTokenN(lexer, tt, begI, cLocB);
      }
      break;
    }
  }

  addSynError(lexer, lexerError(lexer, "No token match", begI, lexer->srcPos - begI));
  return NULL;
}

tokenType isKeyword(Lexer *lexer, const char* value) {
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

lexError* lexerError(Lexer *lexer, const char* msg, const int beg, const int len) {
  lexError *err = malloc(sizeof(lexError));
  err->beg = beg;
  err->len = len;
  err->codeLoc = lexer->codeLoc;
  err->msg = malloc(strlen(msg) * sizeof(char));
  strcpy(err->msg, msg);
  return err;
}

void addSynError(Lexer *lexer, lexError * err) {
  add_to_end(lexer->errors, err);
}

void printError(Lexer *lexer, lexError *err) {
  printf("(%.*s) | %s at L%d C%d\n", err->len, lexer->src + err->beg, err->msg, err->codeLoc.line, err->codeLoc.column);
}

void printErrors(Lexer *lexer) {
  // printf("Lexing errors:\n");
  for (node *head=lexer->errors->head; head != NULL; head = head->next) {
    printError(lexer, head->data);
  }
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
  lexer->codeLoc.column++;
  return lexer->src[lexer->srcPos++];
}

void nextLine(Lexer *lexer) {
  lexer->codeLoc.line++;
  lexer->codeLoc.column = 1;
}

char* getLine(const char* src, const size_t srcLen, const int ln) {
  int startI = 0;
  int endI = -1;
  int curLine = 1;
  char* line;
  for (int i = 0; i < srcLen; i++) {
    if (curLine == ln) {
      startI = i;
      for (int ie = i; ie < srcLen; ie++) {
        if (src[ie] == '\n') {
          endI = ie - 1;
          break;
        }
      }
      break;
    }
    if (src[i] == '\n') curLine++;
  }
  int len = endI - startI + 1;
  line = malloc(len + 1);
  memcpy(line, src + startI, len);
  return line;
}

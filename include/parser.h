#ifndef PARSER_H
#define PARSER_H

#include "../include/lexer.h"
#include "../include/ast.h"
#include "../include/token.h"

typedef struct {
  Lexer *lexer;

  node *node;
  token *token;
  DiagnosticList *diagnostics;
} parser;

parser *initParser(Lexer* lexer);
void freeParser(parser *p);
AST *parse(parser *p);
// void match(tokenType type);
int isType(parser* p);
int isLiteral(parser* p);
int acceptOp(parser* p);
int acceptLiteral(parser* p);
int acceptType(parser* p);
int accept(parser *p, tokenType type);
int expect(parser *p, tokenType type);
token *nextToken(parser *p);
token* peekToken(parser* p);
int digits(int n);

AST* parse_program(parser* p);

AST* parse_block(parser* p);

AST* parse_func_def(parser* p);
AST* parse_func_header(parser* p);
AST* parse_func_body(parser* p);
AST* parse_func_params(parser* p);
AST* parse_func_param(parser* p);

AST* parse_fcall(parser* p);
AST* parse_fcall_args(parser* p);

AST* parse_assignment(parser* p);
AST* parse_var_decl(parser* p);

AST* parse_type(parser* p);
AST* parse_id(parser* p);
AST* parse_statements(parser *p);
AST* parse_statement(parser *p);
AST* parse_term(parser *p);
AST* parse_primary(parser *p);
AST* parse_multiplicative_expr(parser *p);
AST* parse_additive_expr(parser *p);
AST* parse_expr(parser *p);
AST* parse_return(parser *p);

#endif

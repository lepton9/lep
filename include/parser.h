#ifndef PARSER_H
#define PARSER_H

#include "../include/lexer.h"
#include "../include/ast.h"
#include "../include/token.h"

typedef struct {
  Lexer *lexer;

  token token;
  token lookahead;
  DiagnosticList *diagnostics;
} parser;

parser *init_parser(Lexer* lexer);
void free_parser(parser *p);
AST *parse(parser *p);
int is_type(parser* p);
int is_literal(parser* p);
int accept_operator(parser* p);
int accept_literal(parser* p);
int accept_type(parser* p);
int accept(parser *p, tokenType type);
int expect(parser *p, tokenType type);
token *get_next(parser *p);
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
AST* parse_if(parser *p);
AST* parse_while(parser *p);
AST* parse_logical_or_expr(parser *p);
AST* parse_logical_and_expr(parser *p);
AST* parse_equality_expr(parser *p);
AST* parse_relational_expr(parser *p);

#endif

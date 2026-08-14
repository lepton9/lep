#include "../include/parser.h"
#include "../include/errorlep.h"
#include <stdlib.h>
#include <stdio.h>

parser *initParser(Lexer* lexer) {
  parser *p = malloc(sizeof(parser));
  p->lexer = lexer;
  return p;
}

void freeParser(parser *p) { free(p); }

AST *parse(parser *p) {
  p->node = p->lexer->tokens->head;
  p->token = p->lexer->tokens->head->data;

  AST* root = parse_program(p);
  return root;
}

token* peekToken(parser* p) {
  return p->node->next->data;
}

int accept(parser* p, tokenType type) {
  if (p->token->type == type) {
    nextToken(p);
    return 1;
  }
  return 0;
}

int expect(parser *p, tokenType type) {
  if (accept(p, type)) {
    return 1;
  }
  error_parse(p, "Unexpected token", tokenTypeToStr(type));
  return 0;
}

int acceptOp(parser* p) {
  switch(p->token->type) {
    case T_OPERATOR:
      return accept(p, T_OPERATOR);
    case T_PLUS:
      return accept(p, T_PLUS);
    case T_MINUS:
      return accept(p, T_MINUS);
    case T_ASTERISK:
      return accept(p, T_ASTERISK);
    case T_SLASH:
      return accept(p, T_SLASH);
    default:
      return 0;
  }
}

int acceptType(parser* p) {
  if (isType(p)) {
    return accept(p, p->token->type);
  } else {
    return 0;
  }
}

int isType(parser* p) {
  switch(p->token->type) {
    case T_KW_INT:
    case T_KW_F:
    case T_KW_CHAR:
    case T_KW_BOOL:
    case T_KW_STR:
    case T_KW_FLOAT:
    case T_KW_VOID:
      return 1;
    default:
      return 0;
  }
}

int acceptLiteral(parser* p) {
  if (isLiteral(p)) {
    return accept(p, p->token->type);
  } else {
    return 0;
  }
}

int isLiteral(parser* p) {
  switch (p->token->type) {
    case T_LIT_INT:
    case T_LIT_CHAR:
    case T_LIT_STR:
    case T_LIT_FLOAT:
    case T_LIT_BOOL:
    case T_KW_TRUE:
    case T_KW_FALSE:
      return 1;
    default:
      return 0;
  }
}

token *nextToken(parser *p) {
  if (p->node->next != NULL) {
    p->node = p->node->next;
    p->token = p->node->data;
    return p->token;
  }
  return NULL;
}

AST* parse_program(parser* p) {
  AST* program = initAST(AST_PROGRAM, NULL);
  AST* cur = program;
  while(p->token->type != T_EOF) {
    AST* d = NULL;
    if (p->token->type == T_KW_F) {
      d = parse_func_def(p);
    }
    else if (isType(p)) {
      d = parse_var_decl(p);
      expect(p, T_SEMICOLON);
    } else {
      error_parse(p, "Unexpected definition", tokenTypeToStr(p->token->type));
    }
    if (d) {
      cur->next = d;
      cur = d;
    }
  }
  return program;
}

AST* parse_id(parser* p) {
  token* t = p->token;
  expect(p, T_IDENTIFIER);
  AST* ast = initAST(AST_ID, t);
  return ast;
}

AST* parse_type(parser* p) {
  AST* type = initAST(AST_TYPE, p->token);
  acceptType(p);
  return type;
}

AST* parse_func_def(parser* p) {
  token* t = p->token;
  expect(p, T_KW_F);
  expect(p, T_COLON);
  AST* header = parse_func_header(p);
  AST* body = parse_func_body(p);
  AST* func = initAST(AST_FUNCTION, t);
  func->l = header;
  func->r = body;
  return func;
}

AST* parse_func_header(parser* p) {
  AST* name = NULL;
  if (p->token->type == T_KW_MAIN) {
    name = initAST(AST_MAIN, p->token);
    expect(p, T_KW_MAIN);
  } else {
    name = parse_id(p);
  }
  expect(p, T_PAREN_L);
  AST* params = parse_func_params(p);
  expect(p, T_PAREN_R);
  expect(p, T_ARROW);
  AST* ret_type = parse_type(p);

  AST* header = initAST(AST_FHEADER, NULL);
  header->l = name;
  name->next = params;
  params->next = ret_type;
  return header;
}

AST* parse_func_body(parser* p) {
  AST* body = parse_block(p);
  body->type = AST_FBODY;
  return body;
}

AST* parse_func_params(parser* p) {
  AST* params = initAST(AST_PARAMETERS, NULL);
  if (p->token->type == T_PAREN_R) return params;

  params->l = parse_func_param(p);
  AST* cur = params->l;
  while(accept(p, T_COMMA)) {
    AST* param = parse_func_param(p);
    cur->next = param;
    cur = param;
  }
  return params;
}

AST* parse_func_param(parser* p) {
  return (isType(p)) ? parse_var_decl(p) : NULL;
}

AST* parse_primary(parser *p) {
  AST* term = NULL;
  token* t = p->token;
  if (p->token->type == T_IDENTIFIER && peekToken(p)->type == T_PAREN_L) {
    term = parse_fcall(p);
  } else if (accept(p, T_IDENTIFIER)) {
    term = initAST(AST_ID, t);
  } else if (acceptLiteral(p)) {
    term = initAST(AST_VALUE, t);
  } else if (accept(p, T_PAREN_L)) {
    term = parse_expr(p);
    expect(p, T_PAREN_R);
  } else {
    error_parse(p, "Unknown term", NULL);
  }
  return term;
}

AST* parse_term(parser *p) {
  return parse_primary(p);
}

AST* parse_var_decl(parser* p) {
  AST* type = initAST(AST_TYPE, p->token);
  acceptType(p);
  expect(p, T_COLON);
  AST* id = initAST(AST_ID, p->token);
  expect(p, T_IDENTIFIER);
  AST* var = initAST(AST_VARIABLE, NULL);
  var->l = type;
  var->r = id;
  if (p->token->type == T_EQUALS) {
    AST* assignment = initAST(AST_ASSIGNMENT, p->token);
    expect(p, T_EQUALS);
    AST* expr = parse_expr(p);
    assignment->l = id;
    assignment->r = expr;
    var->r = assignment;
  }
  return var;
}

AST* parse_assignment(parser* p) {
  AST* id = initAST(AST_ID, p->token);
  expect(p, T_IDENTIFIER);
  AST* assignment = initAST(AST_ASSIGNMENT, p->token);
  expect(p, T_EQUALS);
  AST* expr = parse_expr(p);
  assignment->l = id;
  assignment->r = expr;
  return assignment;
}

AST* parse_fcall_args(parser* p) {
  AST* args = initAST(AST_FARGUMENTS, NULL);
  if (p->token->type == T_PAREN_R) args->l = NULL;
  else args->l = parse_expr(p);
  AST* cur = args->l;
  while(accept(p, T_COMMA)) {
    AST* arg = parse_expr(p);
    cur->next = arg;
    cur = arg;
  }
  return args;
}

AST* parse_fcall(parser* p) {
  AST* id = initAST(AST_ID, p->token);
  expect(p, T_IDENTIFIER);
  expect(p, T_PAREN_L);
  AST* args = parse_fcall_args(p);
  AST* func_call = initAST(AST_FCALL, NULL);
  func_call->l = id;
  func_call->r = args;
  expect(p, T_PAREN_R);
  return func_call;
}

AST* parse_block(parser* p) {
  expect(p, T_BRACE_L);
  AST* block = initAST(AST_BLOCK, NULL);
  block->l = parse_statements(p);
  expect(p, T_BRACE_R);
  return block;
}

AST* parse_statements(parser *p) {
  // AST *statement = parse_statement(p);
  AST *statements = initAST(AST_STATEMENT, NULL);
  AST *cur = statements;
  while (p->token->type != T_BRACE_R) {
    AST *next_statement = parse_statement(p);
    cur->next = next_statement;
    cur = next_statement;
  }
  return statements->next;
}

AST* parse_statement(parser *p) {
  AST *statement = NULL;
  token* t = p->token;
  while (accept(p, T_SEMICOLON));
  if (isType(p)) {
    statement = parse_var_decl(p);
  } else if (p->token->type == T_IDENTIFIER && peekToken(p)->type == T_PAREN_L) {
    statement = parse_fcall(p);
  } else if (p->token->type == T_IDENTIFIER) {
    statement = parse_assignment(p);
  } else if (p->token->type == T_BRACE_L) {
    statement = parse_block(p);
    accept(p, T_SEMICOLON);
    return statement;
  } else if (p->token->type == T_KW_RET) {
    statement = parse_return(p);
  } else {
    error_parse(p, "Unknown statement", NULL);
    exit(1);
  }
  expect(p, T_SEMICOLON);
  return statement;
}

AST* parse_return(parser *p) {
  token* t = p->token;
  expect(p, T_KW_RET);
  AST* expr_node;
  if (p->token->type == T_SEMICOLON) expr_node = NULL;
  else expr_node = parse_expr(p);
  AST* ret_node = initAST(AST_RET, t);
  ret_node->l = expr_node;
  return ret_node;
}

AST* parse_multiplicative_expr(parser* p) {
  AST* expr = parse_primary(p);
  while (p->token->type == T_ASTERISK || p->token->type == T_SLASH) {
    token* op = p->token;
    nextToken(p);
    AST* op_node = initAST(AST_OPERATOR, op);
    op_node->l = expr;
    op_node->r = parse_primary(p);
    expr = op_node;
  }
  return expr;
}

AST* parse_additive_expr(parser* p) {
  AST* expr = parse_multiplicative_expr(p);
  while (p->token->type == T_PLUS || p->token->type == T_MINUS) {
    token* op = p->token;
    nextToken(p);
    AST* op_node = initAST(AST_OPERATOR, op);
    op_node->l = expr;
    op_node->r = parse_multiplicative_expr(p);
    expr = op_node;
  }
  return expr;
}

AST* parse_expr(parser* p) {
  return parse_additive_expr(p);
}

int digits(int n)
{
  int count = 0;
  if (n > 0) {
      count++;
      digits(n / 10);
  }
  return count;
}

// void errorSyntax(parser* p, const char *msg, const char* expected) {
//   if (expected) {
//     printf("[Syntax error] %s : %s != %s, at L%-2d C%d\n", msg, tokenTypeToStr(p->token->type), expected, p->token->loc.line, p->token->loc.column);
//   } else {
//     printf("[Syntax error] %s : %s, at L%-2d C%d\n", msg, tokenTypeToStr(p->token->type), p->token->loc.line, p->token->loc.column);
//   }
//   printf("%d: %s\n", p->token->loc.line, getLine(p->lexer, p->token->loc.line));
//   printf("%*c^\n", p->token->loc.column + digits(p->token->loc.line) + 1, ' ');
//   exit(1);
// }


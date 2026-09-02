#ifndef ANALYZER_H
#define ANALYZER_H

#include "../include/ast.h"
#include "../include/symtab.h"
#include "../include/stack.h"
#include "../include/diagnostic.h"

typedef struct {
  char* func_name;
  TYPE ret_type;
  bool returned;
  int ret_scope;
  cLoc declaration_location;
} context;

typedef struct {
  symtabStack* symbols;
  context* current_function;
  DiagnosticList *diagnostics;
} SemanticAnalyzer;

typedef struct {
  symtabStack* symbols;
} SemanticResult;

symtab* current_scope(symtabStack* sts);
void enter_scope(symtabStack* sts);
void exit_scope(symtabStack* sts);

TYPE token_to_type(const tokenType type);

SemanticResult* analyze_ast(AST* root, DiagnosticList *diagnostics);
void free_semantic_result(SemanticResult* result);
void semantic_analysis(SemanticAnalyzer* analyzer, AST* ast);

TYPE expr_type(SemanticAnalyzer *analyzer, AST* expr);

bool typecheck_fcall(SemanticAnalyzer *analyzer, AST* fcall);
bool typecheck_assignment(SemanticAnalyzer *analyzer, AST* lhs, AST* rhs);
TYPE typecheck_operator(SemanticAnalyzer *analyzer, AST* op);


#endif

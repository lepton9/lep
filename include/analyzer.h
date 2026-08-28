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

symtab* currentScope(symtabStack* sts);
void enter_scope(symtabStack* sts);
void exit_scope(symtabStack* sts);

bool matchType(const int a, const int b);
int funcRetType(AST* f); // To be deleted
TYPE convertType(const int type);

stEntry* newVariable(symtabStack* sts, const char* id, const TYPE type);

stEntry* lookup_all(symtabStack* sts, const char* key);
stEntry* lookup_scope(symtab* st, const char* key);

SemanticResult* analyzeAST(AST* root, DiagnosticList *diagnostics);
void freeSemanticResult(SemanticResult* result);
void semanticAnalysis(SemanticAnalyzer* analyzer, AST* ast);

TYPE expr_type(SemanticAnalyzer *analyzer, AST* expr);

bool typecheck_fcall(SemanticAnalyzer *analyzer, AST* fcall);
bool typecheck_assignment(SemanticAnalyzer *analyzer, AST* lhs, AST* rhs);
int typecheck_operator(SemanticAnalyzer *analyzer, AST* op);


#endif

#ifndef LEP_H
#define LEP_H

#include "../include/lexer.h"
#include "../include/parser.h"
#include "../include/ast.h"
#include "../include/diagnostic.h"
#include "../include/analyzer.h"
#include "../include/ssair.h"

typedef struct {
  double parse_seconds;
  double analyze_seconds;
  double lower_seconds;
} CompilerTimings;

typedef struct {
  const char *input_path;
  // Owned by Compiler; lexer borrows this immutable source view.
  char *source;
  size_t source_length;
  Lexer lexer;
  parser parser;
  AST *root;
  SemanticResult *semantic;
  ssa *ir;
  DiagnosticList diagnostics;
  CompilerTimings timings;
} Compiler;

typedef enum {
  COMPILER_STAGE_PARSE,
  COMPILER_STAGE_ANALYZE,
  COMPILER_STAGE_LOWER,
} CompilerStage;

Compiler *compiler_init(void);
void compiler_free(Compiler *compiler);

bool compiler_read_source(Compiler *compiler, const char *path);
bool compiler_parse(Compiler *compiler);
bool compiler_analyze(Compiler *compiler);
bool compiler_lower(Compiler *compiler);
bool compiler_run_to_stage(Compiler *compiler, CompilerStage stage);
bool compiler_run(Compiler *compiler);
void compiler_print_diagnostics(const Compiler *compiler, FILE *out);
int run_cli(int argc, char **argv);

#endif

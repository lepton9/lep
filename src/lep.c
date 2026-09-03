#include "../include/cli.h"
#include "../include/lep.h"

#include <errno.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct {
  bool show_time;
  const char *output_path;
} LepCommandOptions;

typedef bool (*CommandRenderer)(const Compiler *compiler, const LepCommandOptions *options);

static double elapsed_seconds(clock_t begin) {
  return (double)(clock() - begin) / CLOCKS_PER_SEC;
}

Compiler *compiler_init(void) {
  Compiler *compiler = calloc(1, sizeof(*compiler));
  if (!compiler) return NULL;

  init_diagnostics(&compiler->diagnostics);
  compiler->lexer = init_lexer(&compiler->diagnostics);
  compiler->parser = init_parser(&compiler->lexer);
  return compiler;
}

void compiler_free(Compiler *compiler) {
  if (!compiler) return;
  if (compiler->ir) free_ssair(compiler->ir);
  if (compiler->semantic) free_semantic_result(compiler->semantic);
  free_ast(compiler->root);
  free(compiler->source);
  free_diagnostics(&compiler->diagnostics);
  free(compiler);
}

bool compiler_read_source(Compiler *compiler, const char *path) {
  FILE *file = fopen(path, "rb");
  if (!file) {
    add_diagnostic(&compiler->diagnostics, DIAGNOSTIC_ERROR, "io", NULL,
                   "could not read '%s': %s", path, strerror(errno));
    return false;
  }

  bool success = false;
  if (fseek(file, 0, SEEK_END) != 0) goto done;
  long file_length = ftell(file);
  if (file_length < 0 || fseek(file, 0, SEEK_SET) != 0) goto done;

  size_t length = (size_t)file_length;
  if ((long)length != file_length || length == SIZE_MAX) {
    errno = EFBIG;
    goto done;
  }

  char *source = malloc(length + 1);
  if (!source) goto done;
  if (fread(source, 1, length, file) != length) {
    if (!ferror(file)) errno = EIO;
    free(source);
    goto done;
  }
  source[length] = '\0';
  free(compiler->source);
  compiler->source = source;
  compiler->source_length = length;
  compiler->input_path = path;
  success = true;

done:
  if (!success) {
    add_diagnostic(&compiler->diagnostics, DIAGNOSTIC_ERROR, "io", NULL,
                   "could not read '%s': %s", path, strerror(errno));
  }
  fclose(file);
  return success;
}

bool compiler_parse(Compiler *compiler) {
  clock_t begin = clock();
  compiler->root = parse(&compiler->parser);
  compiler->timings.parse_seconds = elapsed_seconds(begin);
  return compiler->root && compiler->diagnostics.error_count == 0;
}

bool compiler_analyze(Compiler *compiler) {
  clock_t begin = clock();
  compiler->semantic = analyze_ast(compiler->root, &compiler->diagnostics);
  compiler->timings.analyze_seconds = elapsed_seconds(begin);
  return compiler->semantic && compiler->diagnostics.error_count == 0;
}

bool compiler_lower(Compiler *compiler) {
  clock_t begin = clock();
  compiler->ir = generate_ssair(compiler->root, compiler->semantic, &compiler->diagnostics);
  compiler->timings.lower_seconds = elapsed_seconds(begin);
  return compiler->ir && compiler->ir->valid && compiler->diagnostics.error_count == 0;
}

bool compiler_run_to_stage(Compiler *compiler, CompilerStage stage) {
  if (!compiler_parse(compiler)) return false;
  if (stage == COMPILER_STAGE_PARSE) return true;
  if (!compiler_analyze(compiler)) return false;
  if (stage == COMPILER_STAGE_ANALYZE) return true;
  return compiler_lower(compiler);
}

bool compiler_run(Compiler *compiler) {
  return compiler_run_to_stage(compiler, COMPILER_STAGE_LOWER);
}

void compiler_print_diagnostics(const Compiler *compiler, FILE *out) {
  print_diagnostics(out, &compiler->diagnostics, compiler->source, compiler->source_length);
}

static void print_timings(const Compiler *compiler, CompilerStage completed_stage) {
  if (completed_stage >= COMPILER_STAGE_PARSE) printf("parse:   %.6f s\n", compiler->timings.parse_seconds);
  if (completed_stage >= COMPILER_STAGE_ANALYZE) printf("analyze: %.6f s\n", compiler->timings.analyze_seconds);
  if (completed_stage >= COMPILER_STAGE_LOWER) printf("lower:   %.6f s\n", compiler->timings.lower_seconds);
}

static bool render_ast(const Compiler *compiler, const LepCommandOptions *options) {
  (void)options;
  print_ast_typed(compiler->root, 0);
  return true;
}

static bool render_ssa(const Compiler *compiler, const LepCommandOptions *options) {
  if (!options->output_path) {
    print_ssair(stdout, compiler->ir);
    return true;
  }

  FILE *output = fopen(options->output_path, "wb");
  if (!output) {
    fprintf(stderr, "could not write '%s': %s\n", options->output_path, strerror(errno));
    return false;
  }
  print_ssair(output, compiler->ir);
  bool success = !ferror(output);
  if (fclose(output) != 0) success = false;
  if (!success) fprintf(stderr, "could not write '%s': %s\n", options->output_path, strerror(errno));
  return success;
}

static int run_command(LepCommandOptions *options, int argc, char **argv, CompilerStage stage,
                       CommandRenderer renderer) {
  if (argc != 1) {
    fprintf(stderr, "expected exactly one input file\n");
    return ERROR_CLI;
  }

  Compiler *compiler = compiler_init();
  if (!compiler) {
    fputs("failed to initialize compiler\n", stderr);
    return ERROR_COMPILER;
  }

  bool success = compiler_read_source(compiler, argv[0]);
  if (success) {
    set_source_file(&compiler->lexer, compiler->source, compiler->source_length);
    success = compiler_run_to_stage(compiler, stage);
  }
  if (success && renderer) success = renderer(compiler, options);

  compiler_print_diagnostics(compiler, stderr);
  if (options->show_time) print_timings(compiler, stage);
  compiler_free(compiler);
  return success ? OK : ERROR_COMPILER;
}

static int run_build(void *args, int argc, char **argv) {
  return run_command(args, argc, argv, COMPILER_STAGE_LOWER, NULL);
}

static int run_parse(void *args, int argc, char **argv) {
  return run_command(args, argc, argv, COMPILER_STAGE_PARSE, render_ast);
}

static int run_check(void *args, int argc, char **argv) {
  return run_command(args, argc, argv, COMPILER_STAGE_ANALYZE, NULL);
}

static int run_ir(void *args, int argc, char **argv) {
  return run_command(args, argc, argv, COMPILER_STAGE_LOWER, render_ssa);
}

static LepCommandOptions command_options;

static const CliOption common_flags[] = {
    {"time", 't', CLI_FLAG, NULL, "Print compiler phase timings.", &command_options.show_time},
    {0},
};

static const CliOption ir_flags[] = {
    {"output", 'o', CLI_STRING, "FILE", "Write SSA IR to FILE.", &command_options.output_path},
    {"time", 't', CLI_FLAG, NULL, "Print compiler phase timings.", &command_options.show_time},
    {0},
};

static const CliCommand commands[] = {
    {"build", "Compile a source file.", common_flags, &command_options, run_build},
    {"parse", "Parse and print the syntax tree.", common_flags, &command_options, run_parse},
    {"check", "Parse and type-check a source file.", common_flags, &command_options, run_check},
    {"ir", "Lower a source file to SSA IR.", ir_flags, &command_options, run_ir},
    {0},
};

int run_cli(int argc, char **argv) {
  command_options = (LepCommandOptions){0};
  return cli_run(argc, argv, commands);
}

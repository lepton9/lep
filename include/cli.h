#ifndef CLI_H
#define CLI_H

#include <stdio.h>

typedef enum {
  OK = 0,
  ERROR_COMPILER = 1,
  ERROR_CLI = 2,
} EXIT_CODE;

typedef enum {
  CLI_FLAG,
  CLI_STRING,
} CliOptionKind;

typedef struct {
  const char *name;
  char short_name;
  CliOptionKind kind;
  const char *value_hint;
  const char *help;
  void *target;
} CliOption;

typedef struct CliCommand {
  const char *name;
  const char *summary;
  const CliOption *options;
  void *args;
  int (*run)(void *args, int argc, char **argv);
} CliCommand;

int cli_run(int argc, char **argv, const CliCommand *commands);
void cli_print_usage(FILE *out, const char *program, const CliCommand *commands);
void cli_print_command_help(FILE *out, const char *program, const CliCommand *command);

#endif

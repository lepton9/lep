#include "../include/cli.h"

#include <stdbool.h>
#include <stdio.h>
#include <string.h>

static const CliCommand *find_command(const CliCommand *commands, const char *name) {
  for (const CliCommand *command = commands; command->name; command++) {
    if (strcmp(command->name, name) == 0) return command;
  }
  return NULL;
}

static const CliOption *find_long_option(const CliOption *options, const char *name, size_t length) {
  for (const CliOption *option = options; option && option->name; option++) {
    if (strlen(option->name) == length && strncmp(option->name, name, length) == 0) return option;
  }
  return NULL;
}

static const CliOption *find_short_option(const CliOption *options, char short_name) {
  for (const CliOption *option = options; option && option->name; option++) {
    if (option->short_name == short_name) return option;
  }
  return NULL;
}

static bool set_option(const CliOption *option, const char *value, const char *program,
                       const CliCommand *command) {
  if (option->kind == CLI_FLAG) {
    if (value) {
      fprintf(stderr, "%s: option '--%s' does not take a value\n", program, option->name);
      return false;
    }
    *(bool *)option->target = true;
    return true;
  }

  if (!value) {
    fprintf(stderr, "%s: option '--%s' requires %s\n", program, option->name,
            option->value_hint ? option->value_hint : "a value");
    cli_print_command_help(stderr, program, command);
    return false;
  }
  *(const char **)option->target = value;
  return true;
}

void cli_print_usage(FILE *out, const char *program, const CliCommand *commands) {
  fprintf(out, "Usage: %s <command> [options] <file>\n", program);
  fputc('\n', out);
  fputs("Commands:\n", out);
  for (const CliCommand *command = commands; command->name; command++) {
    fprintf(out, "  %-10s %s\n", command->name, command->summary);
  }
  fputs("\nUse '<command> --help' for command options.\n", out);
}

void cli_print_command_help(FILE *out, const char *program, const CliCommand *command) {
  fprintf(out, "Usage: %s %s [options] <file>\n\n%s\n", program, command->name,
          command->summary);
  if (!command->options) return;

  fputs("\nOptions:\n", out);
  for (const CliOption *option = command->options; option->name; option++) {
    if (option->short_name) {
      fprintf(out, "  -%c, --%-14s %s\n", option->short_name, option->name, option->help);
    } else {
      fprintf(out, "      --%-14s %s\n", option->name, option->help);
    }
  }
  fputs("  -h, --help           Show this help.\n", out);
}

int cli_run(int argc, char **argv, const CliCommand *commands) {
  const char *program = argc > 0 ? argv[0] : "lep";
  if (argc < 2 || strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
    cli_print_usage(argc < 2 ? stderr : stdout, program, commands);
    return argc < 2 ? ERROR_CLI : OK;
  }

  const CliCommand *command = find_command(commands, argv[1]);
  if (!command) {
    fprintf(stderr, "%s: unknown command '%s'\n", program, argv[1]);
    cli_print_usage(stderr, program, commands);
    return ERROR_CLI;
  }

  int argument_start = 2;
  char *positionals[argc];
  int positional_count = 0;
  bool end_of_options = false;

  // Parse CLI arguments
  for (int index = argument_start; index < argc; index++) {
    char *argument = argv[index];

    if (!end_of_options && strcmp(argument, "--") == 0) {
      end_of_options = true;
      continue;
    }

    if (!end_of_options && (strcmp(argument, "-h") == 0 || strcmp(argument, "--help") == 0)) {
      cli_print_command_help(stdout, program, command);
      return OK;
    }

    // Long options
    if (!end_of_options && strncmp(argument, "--", 2) == 0) {
      char *name = argument + 2;
      char *equals = strchr(name, '=');
      size_t name_length = equals ? (size_t)(equals - name) : strlen(name);

      const CliOption *option = find_long_option(command->options, name, name_length);
      if (!option) {
        fprintf(stderr, "%s: unknown option '--%.*s'\n", program, (int)name_length, name);
        cli_print_command_help(stderr, program, command);
        return ERROR_CLI;
      }

      const char *value = equals ? equals + 1 : NULL;
      if (option->kind == CLI_STRING && !value && index + 1 < argc) value = argv[++index];
      if (!set_option(option, value, program, command)) return ERROR_CLI;
      continue;
    }

    // Short options
    if (!end_of_options && argument[0] == '-' && argument[1] != '\0') {
      const CliOption *option = find_short_option(command->options, argument[1]);

      if (!option || (argument[2] != '\0' && option->kind == CLI_FLAG)) {
        fprintf(stderr, "%s: unknown option '%s'\n", program, argument);
        cli_print_command_help(stderr, program, command);
        return ERROR_CLI;
      }
      const char *value = argument[2] ? argument + 2 : NULL;
      if (option->kind == CLI_STRING && !value && index + 1 < argc) value = argv[++index];
      if (!set_option(option, value, program, command)) return ERROR_CLI;
      continue;
    }
    positionals[positional_count++] = argument;
  }

  return command->run(command->args, positional_count, positionals);
}

#include "../include/ssair.h"
#include "../include/analyzer.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

typedef struct binding {
  char *source_name;
  operand value;
  struct binding *next;
} binding;

typedef struct version_counter {
  char *name;
  unsigned next_version;
  struct version_counter *next;
} version_counter;

typedef struct {
  ssa *ir;
  function *function;
  basic_block *block;
  binding *bindings;
  version_counter *versions;
  unsigned next_block_id;
} builder;

static char *duplicate_string(const char *source) {
  size_t length = strlen(source) + 1;
  char *copy = malloc(length);
  memcpy(copy, source, length);
  return copy;
}

static void set_error(ssa *ir, const char *format, ...) {
  va_list args;
  char buffer[256];

  if (!ir->valid) return;
  va_start(args, format);
  vsnprintf(buffer, sizeof(buffer), format, args);
  va_end(args);
  ir->valid = false;
  ir->error = duplicate_string(buffer);
}

static operand none_operand(void) {
  return (operand){.kind = SSA_OPERAND_NONE, .type = VOID};
}

static operand value_operand(const char *name, unsigned version, TYPE type) {
  return (operand){.kind = SSA_OPERAND_VALUE, .type = type, .name = duplicate_string(name), .version = version};
}

static operand int_operand(long value) {
  return (operand){.kind = SSA_OPERAND_INT, .type = INT, .int_value = value};
}

static operand copy_operand(operand source) {
  operand copy = source;
  if (source.kind == SSA_OPERAND_VALUE) copy.name = duplicate_string(source.name);
  return copy;
}

static void free_operand(operand operand_value) {
  if (operand_value.kind == SSA_OPERAND_VALUE) free(operand_value.name);
}

char *var_name(const char *name, unsigned version) {
  int length = snprintf(NULL, 0, "%s.%u", name, version);
  char *result = malloc((size_t)length + 1);
  snprintf(result, (size_t)length + 1, "%s.%u", name, version);
  return result;
}

static basic_block *create_block(unsigned id) {
  basic_block *block = calloc(1, sizeof(*block));
  block->id = id;
  return block;
}

static instruction *append_instruction(builder *build, opcode op, operand dest,
                                       operand src1, operand src2) {
  instruction *instruction_value = calloc(1, sizeof(*instruction_value));
  instruction_value->op = op;
  instruction_value->dest = dest;
  instruction_value->src1 = src1;
  instruction_value->src2 = src2;
  if (build->block->last) build->block->last->next = instruction_value;
  else build->block->first = instruction_value;
  build->block->last = instruction_value;
  return instruction_value;
}

static binding *lookup_binding(builder *build, const char *name) {
  for (binding *current = build->bindings; current; current = current->next) {
    if (strcmp(current->source_name, name) == 0) return current;
  }
  return NULL;
}

static unsigned next_version(builder *build, const char *name) {
  for (version_counter *current = build->versions; current; current = current->next) {
    if (strcmp(current->name, name) == 0) return current->next_version++;
  }

  version_counter *counter = calloc(1, sizeof(*counter));
  counter->name = duplicate_string(name);
  counter->next_version = 1;
  counter->next = build->versions;
  build->versions = counter;
  return 0;
}

static void bind_value(builder *build, const char *name, operand value) {
  binding *binding_value = lookup_binding(build, name);
  if (binding_value) {
    free_operand(binding_value->value);
    binding_value->value = copy_operand(value);
    return;
  }

  binding_value = calloc(1, sizeof(*binding_value));
  binding_value->source_name = duplicate_string(name);
  binding_value->value = copy_operand(value);
  binding_value->next = build->bindings;
  build->bindings = binding_value;
}

static TYPE ast_type(const AST *type_node) {
  return type_node && type_node->tok ? convertType(type_node->tok->type) : VOID;
}

static operand emit_expression(builder *build, AST *expression, const operand *target) {
  if (!expression || !build->ir->valid) return none_operand();

  if (expression->type == AST_VALUE) {
    if (expression->tok->type != T_LIT_INT) {
      set_error(build->ir, "SSA currently supports integer literals only");
      return none_operand();
    }
    return int_operand(strtol(expression->tok->value, NULL, 10));
  }

  if (expression->type == AST_ID) {
    binding *binding_value = lookup_binding(build, expression->tok->value);
    if (!binding_value) {
      set_error(build->ir, "no SSA value is available for '%s'", expression->tok->value);
      return none_operand();
    }
    return copy_operand(binding_value->value);
  }

  if (expression->type == AST_OPERATOR) {
    opcode operation;
    switch (expression->tok->type) {
      case T_PLUS: operation = OP_ADD; break;
      case T_MINUS: operation = OP_SUB; break;
      case T_ASTERISK: operation = OP_MUL; break;
      case T_SLASH: operation = OP_DIV; break;
      default:
        set_error(build->ir, "unsupported expression operator '%s'", expression->tok->value);
        return none_operand();
    }
    operand lhs = emit_expression(build, expression->l, NULL);
    operand rhs = emit_expression(build, expression->r, NULL);
    if (!build->ir->valid) {
      free_operand(lhs);
      free_operand(rhs);
      return none_operand();
    }
    if (lhs.type != INT || rhs.type != INT) {
      set_error(build->ir, "SSA arithmetic currently supports int operands only");
      free_operand(lhs);
      free_operand(rhs);
      return none_operand();
    }
    operand result = target ? copy_operand(*target) :
        value_operand("tmp", next_version(build, "tmp"), INT);
    append_instruction(build, operation, copy_operand(result), lhs, rhs);
    return result;
  }

  set_error(build->ir, "unsupported expression in SSA generation");
  return none_operand();
}

static void emit_assignment(builder *build, AST *assignment, TYPE declared_type) {
  const char *name = assignment->l->tok->value;
  binding *previous = lookup_binding(build, name);
  TYPE type = declared_type;
  if (type == VOID && previous) type = previous->value.type;
  if (type != INT) {
    set_error(build->ir, "SSA currently supports int variables only ('%s')", name);
    return;
  }

  operand destination = value_operand(name, next_version(build, name), type);
  operand result = emit_expression(build, assignment->r, &destination);
  if (!build->ir->valid) {
    free_operand(destination);
    free_operand(result);
    return;
  }
  if (result.kind != SSA_OPERAND_VALUE ||
      strcmp(result.name, destination.name) != 0 || result.version != destination.version) {
    append_instruction(build, OP_COPY, copy_operand(destination), result, none_operand());
  }
  bind_value(build, name, destination);
  free_operand(destination);
  free_operand(result);
}

static void emit_statement_list(builder *build, AST *statement);

static void emit_statement(builder *build, AST *statement) {
  if (!statement || !build->ir->valid) return;
  switch (statement->type) {
    case AST_VARIABLE:
      if (statement->r->type == AST_ASSIGNMENT) {
        emit_assignment(build, statement->r, ast_type(statement->l));
      }
      break;
    case AST_ASSIGNMENT:
      emit_assignment(build, statement, VOID);
      break;
    case AST_RET: {
      operand result = statement->l ? emit_expression(build, statement->l, NULL) : none_operand();
      if (build->ir->valid) append_instruction(build, OP_RET, none_operand(), result, none_operand());
      break;
    }
    case AST_BLOCK:
    case AST_FBODY:
      emit_statement_list(build, statement->l);
      break;
    default:
      set_error(build->ir, "unsupported statement in SSA generation");
      break;
  }
}

static void emit_statement_list(builder *build, AST *statement) {
  for (AST *current = statement; current && build->ir->valid; current = current->next) {
    emit_statement(build, current);
  }
}

static function *emit_function(ssa *ir, AST *ast) {
  AST *header = ast->l;
  AST *name_node = header->l;
  AST *params_node = name_node->next;
  AST *return_type_node = params_node->next;
  function *function_value = calloc(1, sizeof(*function_value));
  builder build = {.ir = ir, .function = function_value};

  function_value->name = duplicate_string(name_node->tok->value);
  function_value->return_type = ast_type(return_type_node);
  build.block = create_block(build.next_block_id++);
  function_value->entry_block = build.block;
  function_value->blocks = build.block;

  for (AST *parameter = params_node->l; parameter; parameter = parameter->next) {
    if (ast_type(parameter->l) != INT) {
      set_error(ir, "SSA currently supports int parameters only");
      break;
    }
    size_t index = function_value->param_count++;
    function_value->params = realloc(function_value->params,
                                     function_value->param_count * sizeof(*function_value->params));
    const char *parameter_name = parameter->r->tok->value;
    function_value->params[index] = value_operand(parameter_name,
                                                   next_version(&build, parameter_name), INT);
    bind_value(&build, parameter_name, function_value->params[index]);
  }
  emit_statement_list(&build, ast->r->l);

  while (build.bindings) {
    binding *next = build.bindings->next;
    free(build.bindings->source_name);
    free_operand(build.bindings->value);
    free(build.bindings);
    build.bindings = next;
  }
  while (build.versions) {
    version_counter *next = build.versions->next;
    free(build.versions->name);
    free(build.versions);
    build.versions = next;
  }
  return function_value;
}

static global_var *emit_global(ssa *ir, AST *ast) {
  global_var *global = calloc(1, sizeof(*global));
  TYPE type = ast_type(ast->l);
  AST *identifier = ast->r;

  if (identifier->type == AST_ASSIGNMENT) identifier = identifier->l;
  if (type != INT) {
    set_error(ir, "SSA currently supports int global variables only");
    return global;
  }
  global->name = duplicate_string(identifier->tok->value);
  global->type = type;
  if (ast->r->type != AST_ASSIGNMENT) return global;

  AST *initializer = ast->r->r;
  if (initializer->type != AST_VALUE || initializer->tok->type != T_LIT_INT) {
    set_error(ir, "global '%s' requires an integer literal initializer", global->name);
    return global;
  }
  global->has_initializer = true;
  global->initializer = int_operand(strtol(initializer->tok->value, NULL, 10));
  return global;
}

ssa *generate_ssair(AST *root) {
  ssa *ir = calloc(1, sizeof(*ir));
  ir->valid = true;
  if (!root || root->type != AST_PROGRAM) {
    set_error(ir, "SSA generation requires a program AST");
    return ir;
  }

  for (AST *node = root->next; node && ir->valid; node = node->next) {
    ssa_node *module_node = calloc(1, sizeof(*module_node));
    if (node->type == AST_FUNCTION) {
      module_node->type = FUNCTION;
      module_node->value.function = emit_function(ir, node);
    } else if (node->type == AST_VARIABLE) {
      module_node->type = GLOBAL_VAR;
      module_node->value.global = emit_global(ir, node);
    } else {
      free(module_node);
      set_error(ir, "unsupported top-level declaration in SSA generation");
      break;
    }
    if (ir->last) ir->last->next = module_node;
    else ir->entry = module_node;
    ir->last = module_node;
  }
  return ir;
}

static const char *opcode_name(opcode op) {
  switch (op) {
    case OP_COPY: return "copy";
    case OP_ADD: return "add";
    case OP_SUB: return "sub";
    case OP_MUL: return "mul";
    case OP_DIV: return "sdiv";
    case OP_CALL: return "call";
    case OP_RET: return "ret";
    case OP_PHI: return "phi";
  }
  return "invalid";
}

static void print_operand(FILE *out, operand operand_value) {
  switch (operand_value.kind) {
    case SSA_OPERAND_NONE: fputs("void", out); break;
    case SSA_OPERAND_VALUE: fprintf(out, "%%%s.%u", operand_value.name, operand_value.version); break;
    case SSA_OPERAND_INT: fprintf(out, "%ld", operand_value.int_value); break;
  }
}

void print_ssair(FILE *out, const ssa *ir) {
  if (!ir) return;
  if (!ir->valid) {
    fprintf(out, "SSA error: %s\n", ir->error ? ir->error : "unknown error");
    return;
  }
  for (const ssa_node *node = ir->entry; node; node = node->next) {
    if (node->type == GLOBAL_VAR) {
      const global_var *global = node->value.global;
      fprintf(out, "global %s @%s", typeToStr(global->type), global->name);
      if (global->has_initializer) {
        fputs(" = ", out);
        print_operand(out, global->initializer);
      }
      fputc('\n', out);
      continue;
    }
    const function *function_value = node->value.function;
    fprintf(out, "function %s(", function_value->name);
    for (size_t index = 0; index < function_value->param_count; index++) {
      if (index) fputs(", ", out);
      fprintf(out, "%s ", typeToStr(function_value->params[index].type));
      print_operand(out, function_value->params[index]);
    }
    fprintf(out, ") -> %s {\n", typeToStr(function_value->return_type));
    for (const basic_block *block = function_value->blocks; block; block = block->next) {
      fprintf(out, "  block.%u:\n", block->id);
      for (const instruction *instruction_value = block->first; instruction_value;
           instruction_value = instruction_value->next) {
        fputs("    ", out);
        if (instruction_value->op != OP_RET) {
          print_operand(out, instruction_value->dest);
          fputs(" = ", out);
        }
        fputs(opcode_name(instruction_value->op), out);
        if (instruction_value->op == OP_RET) {
          if (instruction_value->src1.kind != SSA_OPERAND_NONE) {
            fputc(' ', out);
            print_operand(out, instruction_value->src1);
          }
        } else {
          fputc(' ', out);
          print_operand(out, instruction_value->src1);
          if (instruction_value->src2.kind != SSA_OPERAND_NONE) {
            fputs(", ", out);
            print_operand(out, instruction_value->src2);
          }
        }
        fputc('\n', out);
      }
    }
    fputs("}\n", out);
  }
}

static void free_instruction_list(instruction *instruction_value) {
  while (instruction_value) {
    instruction *next = instruction_value->next;
    free_operand(instruction_value->dest);
    free_operand(instruction_value->src1);
    free_operand(instruction_value->src2);
    for (size_t index = 0; index < instruction_value->arg_count; index++) {
      free_operand(instruction_value->args[index]);
    }
    free(instruction_value->args);
    free(instruction_value);
    instruction_value = next;
  }
}

void free_ssair(ssa *ir) {
  if (!ir) return;
  for (ssa_node *node = ir->entry; node;) {
    ssa_node *next_node = node->next;
    if (node->type == GLOBAL_VAR) {
      global_var *global = node->value.global;
      if (global) {
        free(global->name);
        free_operand(global->initializer);
        free(global);
      }
    } else {
      function *function_value = node->value.function;
      if (function_value) {
        free(function_value->name);
        for (size_t index = 0; index < function_value->param_count; index++) {
          free_operand(function_value->params[index]);
        }
        free(function_value->params);
        for (basic_block *block = function_value->blocks; block;) {
          basic_block *next_block = block->next;
          free_instruction_list(block->first);
          free(block->successors);
          free(block->predecessors);
          free(block);
          block = next_block;
        }
        free(function_value);
      }
    }
    free(node);
    node = next_node;
  }
  free(ir->error);
  free(ir);
}

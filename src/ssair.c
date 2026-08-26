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

typedef struct function_signature {
  char *name;
  TYPE return_type;
  TYPE *params;
  size_t param_count;
  struct function_signature *next;
} function_signature;

typedef struct {
  ssa *ir;
  function *function;
  basic_block *block;
  binding *bindings;
  version_counter *versions;
  function_signature *signatures;
  unsigned next_block_id;
  bool terminated;
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
  return (operand){
      .kind = SSA_OPERAND_VALUE,
      .type = type,
      .data.value = {.name = duplicate_string(name), .version = version},
  };
}

static operand int_operand(long value) {
  return (operand){.kind = SSA_OPERAND_INT, .type = INT, .data.int_value = value};
}

static operand float_operand(double value) {
  return (operand){.kind = SSA_OPERAND_FLOAT, .type = FLOAT, .data.float_value = value};
}

static operand bool_operand(bool value) {
  return (operand){.kind = SSA_OPERAND_BOOL, .type = BOOL, .data.bool_value = value};
}

static operand char_operand(unsigned char value) {
  return (operand){.kind = SSA_OPERAND_CHAR, .type = CHAR, .data.char_value = value};
}

static operand string_operand(const char *value) {
  return (operand){.kind = SSA_OPERAND_STRING, .type = STR, .data.string_value = duplicate_string(value)};
}

// Returns an owned copy of the operand.
static operand copy_operand(operand source) {
  operand copy = source;
  if (source.kind == SSA_OPERAND_VALUE) copy.data.value.name = duplicate_string(source.data.value.name);
  if (source.kind == SSA_OPERAND_STRING) copy.data.string_value = duplicate_string(source.data.string_value);
  return copy;
}

static void free_operand(operand operand_value) {
  if (operand_value.kind == SSA_OPERAND_VALUE) free(operand_value.data.value.name);
  if (operand_value.kind == SSA_OPERAND_STRING) free(operand_value.data.string_value);
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

// Appends an instruction with operand copies owned by the block.
static instruction *append_instruction(builder *build, opcode op, operand dest,
                                       operand src1, operand src2) {
  instruction *instruction_value = calloc(1, sizeof(*instruction_value));
  instruction_value->op = op;
  instruction_value->dest = copy_operand(dest);
  instruction_value->src1 = copy_operand(src1);
  instruction_value->src2 = copy_operand(src2);
  if (build->block->last) build->block->last->next = instruction_value;
  else build->block->first = instruction_value;
  build->block->last = instruction_value;
  return instruction_value;
}

// Builds a call instruction.
static instruction *append_call(builder *build, const char *callee, operand dest,
                                operand *args, size_t arg_count) {
  instruction *instruction_value = append_instruction(build, OP_CALL, dest,
                                                       none_operand(), none_operand());
  instruction_value->callee = duplicate_string(callee);
  instruction_value->arg_count = arg_count;
  if (arg_count) {
    instruction_value->args = calloc(arg_count, sizeof(*instruction_value->args));
    for (size_t index = 0; index < arg_count; index++) {
      instruction_value->args[index] = copy_operand(args[index]);
    }
  }
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

static bool is_value_type(TYPE type) {
  return type == INT || type == FLOAT || type == CHAR || type == BOOL || type == STR;
}

// Finds a module-wide function signature with the given name.
static function_signature *lookup_signature(builder *build, const char *name) {
  for (function_signature *current = build->signatures; current; current = current->next) {
    if (strcmp(current->name, name) == 0) return current;
  }
  return NULL;
}

static operand literal_operand(builder *build, AST *expression) {
  switch (expression->tok->type) {
    case T_LIT_INT:
      return int_operand(strtol(expression->tok->value, NULL, 10));
    case T_LIT_FLOAT:
      return float_operand(strtod(expression->tok->value, NULL));
    case T_LIT_BOOL:
      return bool_operand(strcmp(expression->tok->value, "true") == 0);
    case T_LIT_CHAR:
      return char_operand((unsigned char)expression->tok->value[1]);
    case T_LIT_STR:
      return string_operand(expression->tok->value);
    default:
      set_error(build->ir, "unsupported literal in SSA generation");
      return none_operand();
  }
}

static operand emit_expression(builder *build, AST *expression, const operand *target);

// Lowers a function call and returns its SSA result.
static operand emit_call(builder *build, AST *call, const operand *target) {
  const char *callee = call->l->tok->value;
  function_signature *signature = lookup_signature(build, callee);
  if (!signature) {
    set_error(build->ir, "no SSA function signature is available for '%s'", callee);
    return none_operand();
  }
  if (signature->return_type == VOID && target) {
    set_error(build->ir, "void function '%s' cannot be used as an expression", callee);
    return none_operand();
  }

  operand *args = NULL;
  size_t arg_count = 0;
  for (AST *argument = call->r->l; argument && build->ir->valid; argument = argument->next) {
    args = realloc(args, (arg_count + 1) * sizeof(*args));
    args[arg_count] = emit_expression(build, argument, NULL);
    if (build->ir->valid &&
        (arg_count >= signature->param_count ||
         args[arg_count].type != signature->params[arg_count])) {
      set_error(build->ir, "call to '%s' has an incompatible argument", callee);
    }
    arg_count++;
  }
  if (build->ir->valid && arg_count != signature->param_count) {
    set_error(build->ir, "call to '%s' has the wrong number of arguments", callee);
  }
  if (!build->ir->valid) {
    for (size_t index = 0; index < arg_count; index++) free_operand(args[index]);
    free(args);
    return none_operand();
  }

  operand result = none_operand();
  if (signature->return_type != VOID) {
    const char* result_var = "tmp";
    result = target ? copy_operand(*target) :
        value_operand(result_var, next_version(build, result_var), signature->return_type);
  }
  append_call(build, callee, result, args, arg_count);
  for (size_t index = 0; index < arg_count; index++) free_operand(args[index]);
  free(args);
  return result;
}

// Lowers an expression.
static operand emit_expression(builder *build, AST *expression, const operand *target) {
  if (!expression || !build->ir->valid) return none_operand();

  if (expression->type == AST_VALUE) {
    return literal_operand(build, expression);
  }

  if (expression->type == AST_ID) {
    binding *binding_value = lookup_binding(build, expression->tok->value);
    if (!binding_value) {
      set_error(build->ir, "no SSA value is available for '%s'", expression->tok->value);
      return none_operand();
    }
    return copy_operand(binding_value->value);
  }

  if (expression->type == AST_FCALL) return emit_call(build, expression, target);

  if (expression->type == AST_OPERATOR) {
    opcode operation;
    operand lhs = emit_expression(build, expression->l, NULL);
    operand rhs = emit_expression(build, expression->r, NULL);
    if (!build->ir->valid) {
      free_operand(lhs);
      free_operand(rhs);
      return none_operand();
    }
    if (lhs.type != rhs.type || (lhs.type != INT && lhs.type != FLOAT)) {
      set_error(build->ir, "arithmetic requires two int or two float operands");
      free_operand(lhs);
      free_operand(rhs);
      return none_operand();
    }
    switch (expression->tok->type) {
      case T_PLUS: operation = lhs.type == FLOAT ? OP_FADD : OP_ADD; break;
      case T_MINUS: operation = lhs.type == FLOAT ? OP_FSUB : OP_SUB; break;
      case T_ASTERISK: operation = lhs.type == FLOAT ? OP_FMUL : OP_MUL; break;
      case T_SLASH: operation = lhs.type == FLOAT ? OP_FDIV : OP_DIV; break;
      default: return none_operand();
    }
    operand result = target ? copy_operand(*target) :
        value_operand("tmp", next_version(build, "tmp"), lhs.type);
    append_instruction(build, operation, result, lhs, rhs);
    free_operand(lhs);
    free_operand(rhs);
    return result;
  }

  set_error(build->ir, "unsupported expression in SSA generation");
  return none_operand();
}

// Creates the next version of an assigned variable and updates its current binding.
static void emit_assignment(builder *build, AST *assignment, TYPE declared_type) {
  const char *name = assignment->l->tok->value;
  binding *previous = lookup_binding(build, name);
  TYPE type = declared_type;
  if (type == VOID && previous) type = previous->value.type;
  if (!is_value_type(type)) {
    set_error(build->ir, "SSA cannot assign values to '%s' of type %s", name, typeToStr(type));
    return;
  }

  operand destination = value_operand(name, next_version(build, name), type);
  operand result = emit_expression(build, assignment->r, &destination);
  if (!build->ir->valid) {
    free_operand(destination);
    free_operand(result);
    return;
  }
  if (result.type != type) {
    set_error(build->ir, "assignment to '%s' has incompatible SSA types", name);
    free_operand(destination);
    free_operand(result);
    return;
  }
  if (result.kind != SSA_OPERAND_VALUE ||
      strcmp(result.data.value.name, destination.data.value.name) != 0 ||
      result.data.value.version != destination.data.value.version) {
    append_instruction(build, OP_COPY, destination, result, none_operand());
  }
  bind_value(build, name, destination);
  free_operand(destination);
  free_operand(result);
}

static void emit_statement_list(builder *build, AST *statement);

static void emit_statement(builder *build, AST *statement) {
  if (!statement || !build->ir->valid) return;
  if (build->terminated) {
    set_error(build->ir, "instruction follows a return in function '%s'", build->function->name);
    return;
  }
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
      if (build->ir->valid && result.type != build->function->return_type) {
        set_error(build->ir, "return value has an incompatible type in function '%s'", build->function->name);
      }
      if (build->ir->valid) {
        append_instruction(build, OP_RET, none_operand(), result, none_operand());
        build->terminated = true;
      }
      free_operand(result);
      break;
    }
    case AST_FCALL: {
      operand result = emit_call(build, statement, NULL);
      free_operand(result);
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

// Lowers one function body into its entry block.
static function *emit_function(ssa *ir, AST *ast, function_signature *signatures) {
  AST *header = ast->l;
  AST *name_node = header->l;
  AST *params_node = name_node->next;
  AST *return_type_node = params_node->next;
  function *function_value = calloc(1, sizeof(*function_value));
  builder build = {.ir = ir, .function = function_value, .signatures = signatures};

  function_value->name = duplicate_string(name_node->tok->value);
  function_value->return_type = ast_type(return_type_node);
  build.block = create_block(build.next_block_id++);
  function_value->entry_block = build.block;
  function_value->blocks = build.block;

  for (AST *parameter = params_node->l; parameter; parameter = parameter->next) {
    TYPE parameter_type = ast_type(parameter->l);
    if (!is_value_type(parameter_type)) {
      set_error(ir, "SSA parameters must have a value type");
      break;
    }
    size_t index = function_value->param_count++;
    function_value->params = realloc(function_value->params,
                                     function_value->param_count * sizeof(*function_value->params));
    const char *parameter_name = parameter->r->tok->value;
    function_value->params[index] = value_operand(parameter_name,
                                                   next_version(&build, parameter_name), parameter_type);
    bind_value(&build, parameter_name, function_value->params[index]);
  }
  emit_statement_list(&build, ast->r->l);
  if (ir->valid && !build.terminated) {
    if (function_value->return_type == VOID) {
      append_instruction(&build, OP_RET, none_operand(), none_operand(), none_operand());
    } else {
      set_error(ir, "function '%s' has no return instruction", function_value->name);
    }
  }

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

// Collects all headers before body lowering so forward calls are valid.
static function_signature *collect_signatures(AST *root) {
  function_signature *signatures = NULL;
  for (AST *node = root->next; node; node = node->next) {
    if (node->type != AST_FUNCTION) continue;
    AST *header = node->l;
    AST *name_node = header->l;
    AST *params_node = name_node->next;
    AST *return_type_node = params_node->next;
    function_signature *signature = calloc(1, sizeof(*signature));
    signature->name = duplicate_string(name_node->tok->value);
    signature->return_type = ast_type(return_type_node);
    for (AST *parameter = params_node->l; parameter; parameter = parameter->next) {
      signature->params = realloc(signature->params,
                                  (signature->param_count + 1) * sizeof(*signature->params));
      signature->params[signature->param_count++] = ast_type(parameter->l);
    }
    signature->next = signatures;
    signatures = signature;
  }
  return signatures;
}

static void free_signatures(function_signature *signatures) {
  while (signatures) {
    function_signature *next = signatures->next;
    free(signatures->name);
    free(signatures->params);
    free(signatures);
    signatures = next;
  }
}

static global_var *emit_global(ssa *ir, AST *ast) {
  global_var *global = calloc(1, sizeof(*global));
  TYPE type = ast_type(ast->l);
  AST *identifier = ast->r;

  if (identifier->type == AST_ASSIGNMENT) identifier = identifier->l;
  if (!is_value_type(type)) {
    set_error(ir, "SSA globals must have a value type");
    return global;
  }
  global->name = duplicate_string(identifier->tok->value);
  global->type = type;
  if (ast->r->type != AST_ASSIGNMENT) return global;

  AST *initializer = ast->r->r;
  if (initializer->type != AST_VALUE) {
    set_error(ir, "global '%s' requires a literal initializer", global->name);
    return global;
  }
  builder build = {.ir = ir};
  global->initializer = literal_operand(&build, initializer);
  if (!ir->valid || global->initializer.type != type) {
    if (ir->valid) set_error(ir, "global '%s' has an incompatible initializer", global->name);
    return global;
  }
  global->has_initializer = true;
  return global;
}

// Generates a complete module.
ssa *generate_ssair(AST *root) {
  ssa *ir = calloc(1, sizeof(*ir));
  ir->valid = true;
  if (!root || root->type != AST_PROGRAM) {
    set_error(ir, "SSA generation requires a program AST");
    return ir;
  }
  function_signature *signatures = collect_signatures(root);

  for (AST *node = root->next; node && ir->valid; node = node->next) {
    ssa_node *module_node = calloc(1, sizeof(*module_node));
    if (node->type == AST_FUNCTION) {
      module_node->type = FUNCTION;
      module_node->value.function = emit_function(ir, node, signatures);
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
  free_signatures(signatures);
  return ir;
}

static const char *opcode_name(opcode op) {
  switch (op) {
    case OP_COPY: return "copy";
    case OP_ADD: return "add";
    case OP_SUB: return "sub";
    case OP_MUL: return "mul";
    case OP_DIV: return "sdiv";
    case OP_FADD: return "fadd";
    case OP_FSUB: return "fsub";
    case OP_FMUL: return "fmul";
    case OP_FDIV: return "fdiv";
    case OP_CALL: return "call";
    case OP_RET: return "ret";
    case OP_PHI: return "phi";
  }
  return "invalid";
}

static void print_operand(FILE *out, operand operand_value) {
  switch (operand_value.kind) {
    case SSA_OPERAND_NONE: fputs("void", out); break;
    case SSA_OPERAND_VALUE:
      fprintf(out, "%%%s.%u", operand_value.data.value.name, operand_value.data.value.version);
      break;
    case SSA_OPERAND_INT: fprintf(out, "%ld", operand_value.data.int_value); break;
    case SSA_OPERAND_FLOAT: fprintf(out, "%g", operand_value.data.float_value); break;
    case SSA_OPERAND_BOOL: fputs(operand_value.data.bool_value ? "true" : "false", out); break;
    case SSA_OPERAND_CHAR: fprintf(out, "'%c'", operand_value.data.char_value); break;
    case SSA_OPERAND_STRING: fputs(operand_value.data.string_value, out); break;
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
        if (instruction_value->op != OP_RET && instruction_value->dest.kind != SSA_OPERAND_NONE) {
          print_operand(out, instruction_value->dest);
          fputs(" = ", out);
        }
        fputs(opcode_name(instruction_value->op), out);
        if (instruction_value->op == OP_RET) {
          if (instruction_value->src1.kind != SSA_OPERAND_NONE) {
            fputc(' ', out);
            print_operand(out, instruction_value->src1);
          }
        } else if (instruction_value->op == OP_CALL) {
          fprintf(out, " @%s(", instruction_value->callee);
          for (size_t index = 0; index < instruction_value->arg_count; index++) {
            if (index) fputs(", ", out);
            print_operand(out, instruction_value->args[index]);
          }
          fputc(')', out);
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
    free(instruction_value->callee);
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

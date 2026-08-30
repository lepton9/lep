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
  // TODO: use hashmap
  binding *bindings;
  version_counter *versions;
  const SemanticResult *semantic;
  unsigned next_block_id;
  bool terminated;
} builder;

static char *duplicate_string(const char *source) {
  size_t length = strlen(source) + 1;
  char *copy = malloc(length);
  memcpy(copy, source, length);
  return copy;
}

static const token *source_token(const AST *node) {
  if (!node) return NULL;
  if (node->tok) return node->tok;
  const token *left = source_token(node->l);
  return left ? left : source_token(node->r);
}

static void set_error(ssa *ir, const AST *node, const char *format, ...) {
  va_list args;

  if (!ir->valid) return;
  va_start(args, format);
  ir->valid = false;
  const token *source = source_token(node);
  add_diagnosticv(ir->diagnostics, DIAGNOSTIC_ERROR, "ssa",
                  source ? &source->loc : NULL, format, args);
  va_end(args);
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
  instruction_value->data.operands.src1 = copy_operand(src1);
  instruction_value->data.operands.src2 = copy_operand(src2);
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
  instruction_value->data.call.callee = duplicate_string(callee);
  instruction_value->data.call.arg_count = arg_count;
  if (arg_count) {
    instruction_value->data.call.args = calloc(arg_count, sizeof(*instruction_value->data.call.args));
    for (size_t index = 0; index < arg_count; index++) {
      instruction_value->data.call.args[index] = copy_operand(args[index]);
    }
  }
  return instruction_value;
}

static binding *lookup_binding_in(binding *bindings, const char *name) {
  for (binding *current = bindings; current; current = current->next) {
    if (strcmp(current->source_name, name) == 0) return current;
  }
  return NULL;
}


static binding *lookup_binding(builder *build, const char *name) {
  return lookup_binding_in(build->bindings, name);
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

static binding *copy_bindings(const binding *source) {
  binding *result = NULL;
  for (const binding *current = source; current; current = current->next) {
    binding *copy = calloc(1, sizeof(*copy));
    copy->source_name = duplicate_string(current->source_name);
    copy->value = copy_operand(current->value);
    copy->next = result;
    result = copy;
  }
  return result;
}

static void free_bindings(binding *bindings) {
  while (bindings) {
    binding *next = bindings->next;
    free(bindings->source_name);
    free_operand(bindings->value);
    free(bindings);
    bindings = next;
  }
}

static void append_block(function *function_value, basic_block *block) {
  basic_block *last = function_value->blocks;
  while (last->next) last = last->next;
  last->next = block;
}

static basic_block *new_block(builder *build) {
  basic_block *block = create_block(build->next_block_id++);
  append_block(build->function, block);
  return block;
}

static void link_blocks(basic_block *from, basic_block *to) {
  from->successors = realloc(from->successors, (from->successor_count + 1) * sizeof(*from->successors));
  from->successors[from->successor_count++] = to;
  to->predecessors = realloc(to->predecessors, (to->predecessor_count + 1) * sizeof(*to->predecessors));
  to->predecessors[to->predecessor_count++] = from;
}

static void emit_branch(builder *build, basic_block *target) {
  instruction *branch = append_instruction(build, OP_BR, none_operand(), none_operand(), none_operand());
  branch->data.branch.target = target;
  link_blocks(build->block, target);
  build->block->terminated = true;
  build->terminated = true;
}

static void emit_conditional_branch(builder *build, operand condition, basic_block *true_target,
                                    basic_block *false_target) {
  instruction *branch = append_instruction(build, OP_CBR, none_operand(), condition, none_operand());
  branch->data.conditional_branch.true_target = true_target;
  branch->data.conditional_branch.false_target = false_target;
  link_blocks(build->block, true_target);
  link_blocks(build->block, false_target);
  build->block->terminated = true;
  build->terminated = true;
}

static instruction *emit_phi(builder *build, operand destination, operand *values,
                             basic_block **blocks, size_t count) {
  instruction *phi = append_instruction(build, OP_PHI, destination, none_operand(), none_operand());
  phi->data.phi.count = count;
  phi->data.phi.entries = calloc(count, sizeof(*phi->data.phi.entries));
  for (size_t index = 0; index < count; index++) {
    phi->data.phi.entries[index].value = copy_operand(values[index]);
    phi->data.phi.entries[index].block = blocks[index];
  }
  return phi;
}

static bool same_operand(operand left, operand right) {
  if (left.kind != right.kind || left.type != right.type) return false;
  switch (left.kind) {
    case SSA_OPERAND_NONE: return true;
    case SSA_OPERAND_VALUE:
      return left.data.value.version == right.data.value.version &&
             strcmp(left.data.value.name, right.data.value.name) == 0;
    case SSA_OPERAND_INT: return left.data.int_value == right.data.int_value;
    case SSA_OPERAND_FLOAT: return left.data.float_value == right.data.float_value;
    case SSA_OPERAND_BOOL: return left.data.bool_value == right.data.bool_value;
    case SSA_OPERAND_CHAR: return left.data.char_value == right.data.char_value;
    case SSA_OPERAND_STRING:
      return strcmp(left.data.string_value, right.data.string_value) == 0;
  }
  return false;
}

static TYPE ast_type(const AST *type_node) {
  return type_node && type_node->tok ? convertType(type_node->tok->type) : VOID;
}

static bool is_value_type(TYPE type) {
  return type == INT || type == FLOAT || type == CHAR || type == BOOL || type == STR;
}

static stEntry *lookup_function(builder *build, const char *name) {
  if (!build->semantic || !build->semantic->symbols) return NULL;
  stEntry *entry = lookup_scope(currentScope(build->semantic->symbols), name);
  return entry && entry->type == F ? entry : NULL;
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
      set_error(build->ir, expression, "unsupported literal in SSA generation");
      return none_operand();
  }
}

static operand emit_expression(builder *build, AST *expression, const operand *target);

// Lowers a function call and returns its SSA result.
static operand emit_call(builder *build, AST *call, const operand *target) {
  const char *callee = call->l->tok->value;
  stEntry *function_value = lookup_function(build, callee);
  if (!function_value || !function_value->f_info) {
    set_error(build->ir, call->l, "no SSA function signature is available for '%s'", callee);
    return none_operand();
  }
  func_info *signature = function_value->f_info;
  if (signature->ret_type == VOID && target) {
    set_error(build->ir, call->l, "void function '%s' cannot be used as an expression", callee);
    return none_operand();
  }

  operand *args = NULL;
  size_t arg_count = 0;
  for (AST *argument = call->r->l; argument && build->ir->valid; argument = argument->next) {
    args = realloc(args, (arg_count + 1) * sizeof(*args));
    args[arg_count] = emit_expression(build, argument, NULL);
    if (build->ir->valid &&
        (arg_count >= signature->n_params ||
         args[arg_count].type != signature->params[arg_count].type)) {
      set_error(build->ir, argument, "call to '%s' has an incompatible argument", callee);
    }
    arg_count++;
  }
  if (build->ir->valid && arg_count != signature->n_params) {
    set_error(build->ir, call->l, "call to '%s' has the wrong number of arguments", callee);
  }
  if (!build->ir->valid) {
    for (size_t index = 0; index < arg_count; index++) free_operand(args[index]);
    free(args);
    return none_operand();
  }

  operand result = none_operand();
  if (signature->ret_type != VOID) {
    result = target ? copy_operand(*target) :
        value_operand("tmp", next_version(build, "tmp"), signature->ret_type);
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
      set_error(build->ir, expression, "no SSA value is available for '%s'", expression->tok->value);
      return none_operand();
    }
    return copy_operand(binding_value->value);
  }

  if (expression->type == AST_FCALL) return emit_call(build, expression, target);

  if (expression->type == AST_OPERATOR) {
    const tokenType expression_type = expression->tok->type;
    if (expression_type == T_AND || expression_type == T_OR) {
      operand left = emit_expression(build, expression->l, NULL);
      if (!build->ir->valid || left.type != BOOL) {
        free_operand(left);
        return none_operand();
      }
      basic_block *decision = build->block;
      basic_block *right_block = new_block(build);
      basic_block *merge_block = new_block(build);
      if (expression_type == T_AND) emit_conditional_branch(build, left, right_block, merge_block);
      else emit_conditional_branch(build, left, merge_block, right_block);

      build->block = right_block;
      build->terminated = false;
      operand right = emit_expression(build, expression->r, NULL);
      if (!build->ir->valid || right.type != BOOL) {
        free_operand(left);
        free_operand(right);
        return none_operand();
      }
      basic_block *right_end = build->block;
      if (!right_end->terminated) emit_branch(build, merge_block);

      build->block = merge_block;
      build->terminated = false;
      operand result = target ? copy_operand(*target) :
          value_operand("tmp", next_version(build, "tmp"), BOOL);
      operand values[2] = {bool_operand(expression_type == T_OR), right};
      basic_block *blocks[2] = {decision, right_end};
      emit_phi(build, result, values, blocks, 2);
      free_operand(left);
      free_operand(right);
      return result;
    }
    opcode operation;
    operand lhs = emit_expression(build, expression->l, NULL);
    operand rhs = expression->r ? emit_expression(build, expression->r, NULL) : none_operand();
    if (!build->ir->valid) {
      free_operand(lhs);
      free_operand(rhs);
      return none_operand();
    }

    if (expression_type == T_BANG) {
      if (lhs.type != BOOL) {
        set_error(build->ir, expression, "'!' requires a bool operand");
        free_operand(lhs);
        return none_operand();
      }
      operand result = target ? copy_operand(*target) :
          value_operand("tmp", next_version(build, "tmp"), BOOL);
      append_instruction(build, OP_NOT, result, lhs, none_operand());
      free_operand(lhs);
      return result;
    }

    if (lhs.type != rhs.type) {
      set_error(build->ir, expression, "operator operands have incompatible SSA types");
      free_operand(lhs);
      free_operand(rhs);
      return none_operand();
    }

    switch (expression_type) {
      case T_PLUS: operation = lhs.type == FLOAT ? OP_FADD : OP_ADD; break;
      case T_MINUS: operation = lhs.type == FLOAT ? OP_FSUB : OP_SUB; break;
      case T_ASTERISK: operation = lhs.type == FLOAT ? OP_FMUL : OP_MUL; break;
      case T_SLASH: operation = lhs.type == FLOAT ? OP_FDIV : OP_DIV; break;
      case T_EQ: operation = OP_EQ; break;
      case T_NEQ: operation = OP_NEQ; break;
      case T_LT: operation = OP_LT; break;
      case T_LE: operation = OP_LE; break;
      case T_GT: operation = OP_GT; break;
      case T_GE: operation = OP_GE; break;
      default:
        set_error(build->ir, expression, "unsupported arithmetic operator in SSA generation");
        free_operand(lhs);
        free_operand(rhs);
        return none_operand();
    }
    TYPE result_type = expression_type == T_EQ || expression_type == T_NEQ ||
                       expression_type == T_LT || expression_type == T_LE ||
                       expression_type == T_GT || expression_type == T_GE ? BOOL : lhs.type;
    operand result = target ? copy_operand(*target) :
        value_operand("tmp", next_version(build, "tmp"), result_type);
    append_instruction(build, operation, result, lhs, rhs);
    free_operand(lhs);
    free_operand(rhs);
    return result;
  }

  set_error(build->ir, expression, "unsupported expression in SSA generation");
  return none_operand();
}

// Creates the next version of an assigned variable and updates its current binding.
static void emit_assignment(builder *build, AST *assignment, TYPE declared_type) {
  const char *name = assignment->l->tok->value;
  binding *previous = lookup_binding(build, name);
  TYPE type = declared_type;
  if (type == VOID && previous) type = previous->value.type;
  if (!is_value_type(type)) {
    set_error(build->ir, assignment->l, "SSA cannot assign values to '%s' of type %s", name, typeToStr(type));
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
    set_error(build->ir, assignment->l, "assignment to '%s' has incompatible SSA types", name);
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
static void emit_statement(builder *build, AST *statement);

static void emit_if(builder *build, AST *statement) {
  operand condition = emit_expression(build, statement->l, NULL);
  if (!build->ir->valid || condition.type != BOOL) {
    free_operand(condition);
    return;
  }
  binding *incoming = build->bindings;
  build->bindings = NULL;
  basic_block *then_block = new_block(build);
  basic_block *else_block = new_block(build);
  basic_block *merge_block = new_block(build);
  emit_conditional_branch(build, condition, then_block, else_block);
  free_operand(condition);

  // Then branch
  build->block = then_block;
  build->terminated = false;
  build->bindings = copy_bindings(incoming);
  emit_statement(build, statement->r);
  binding *then_bindings = build->bindings;
  basic_block *then_end = build->block;
  bool then_reaches_merge = !then_end->terminated;
  if (then_reaches_merge) emit_branch(build, merge_block);

  // Else branch
  build->block = else_block;
  build->terminated = false;
  build->bindings = copy_bindings(incoming);
  AST *else_statement = statement->r->next;
  if (else_statement) emit_statement(build, else_statement);
  binding *else_bindings = build->bindings;
  basic_block *else_end = build->block;
  bool else_reaches_merge = !else_end->terminated;
  if (else_reaches_merge) emit_branch(build, merge_block);

  // Merge then and else branches
  build->block = merge_block;
  build->terminated = false;
  build->bindings = copy_bindings(incoming);
  for (binding *original = incoming; original; original = original->next) {
    // Handle possible variable reassignments from inside the branches
    binding *then_value = lookup_binding_in(then_bindings, original->source_name);
    binding *else_value = lookup_binding_in(else_bindings, original->source_name);
    // The variable defined outside of the blocks was not reassigned in 
    // either of the branches.
    if (!then_value || !else_value) continue;

    if (then_reaches_merge && else_reaches_merge &&
        !same_operand(then_value->value, else_value->value)) {
      // Both branches modify the variable
      operand destination = value_operand(original->source_name,
                                          next_version(build, original->source_name),
                                          original->value.type);
      operand values[2] = {then_value->value, else_value->value};
      basic_block *blocks[2] = {then_end, else_end};
      emit_phi(build, destination, values, blocks, 2);
      bind_value(build, original->source_name, destination);
      free_operand(destination);
    } else if (then_reaches_merge && !else_reaches_merge) {
      bind_value(build, original->source_name, then_value->value);
    } else if (!then_reaches_merge && else_reaches_merge) {
      bind_value(build, original->source_name, else_value->value);
    }
  }
  free_bindings(incoming);
  free_bindings(then_bindings);
  free_bindings(else_bindings);
}

static void emit_while(builder *build, AST *statement) {
  binding *incoming = build->bindings;
  build->bindings = NULL;
  basic_block *header = new_block(build);
  basic_block *body = new_block(build);
  basic_block *exit = new_block(build);
  emit_branch(build, header);

  build->block = header;
  build->terminated = false;
  build->bindings = copy_bindings(incoming);
  for (binding *original = incoming; original; original = original->next) {
    operand destination = value_operand(original->source_name,
                                        next_version(build, original->source_name),
                                        original->value.type);
    operand values[2] = {original->value, none_operand()};
    basic_block *blocks[2] = {header->predecessors[0], NULL};
    emit_phi(build, destination, values, blocks, 2);
    bind_value(build, original->source_name, destination);
    free_operand(destination);
  }
  operand condition = emit_expression(build, statement->l, NULL);
  if (build->ir->valid && condition.type == BOOL) emit_conditional_branch(build, condition, body, exit);
  else if (build->ir->valid) set_error(build->ir, statement->l, "while condition must have bool SSA type");
  free_operand(condition);
  binding *header_bindings = copy_bindings(build->bindings);

  build->block = body;
  build->terminated = false;
  emit_statement(build, statement->r);
  binding *body_bindings = build->bindings;
  basic_block *body_end = build->block;
  // Infinite while loop
  if (!body_end->terminated) emit_branch(build, header);

  for (instruction *instruction_value = header->first; instruction_value &&
       instruction_value->op == OP_PHI; instruction_value = instruction_value->next) {
    binding *body_value = lookup_binding_in(body_bindings, instruction_value->dest.data.value.name);
    if (!body_value) body_value = lookup_binding_in(incoming, instruction_value->dest.data.value.name);
    free_operand(instruction_value->data.phi.entries[1].value);
    instruction_value->data.phi.entries[1].value = copy_operand(body_value->value);
    instruction_value->data.phi.entries[1].block = body_end;
  }
  build->block = exit;
  build->terminated = false;
  build->bindings = header_bindings;
  free_bindings(incoming);
  free_bindings(body_bindings);
}

static void emit_statement(builder *build, AST *statement) {
  if (!statement || !build->ir->valid) return;
  if (build->block->terminated) {
    set_error(build->ir, statement, "instruction follows a return in function '%s'", build->function->name);
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
        set_error(build->ir, statement, "return value has an incompatible type in function '%s'", build->function->name);
      }
      if (build->ir->valid) {
        append_instruction(build, OP_RET, none_operand(), result, none_operand());
        build->block->terminated = true;
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
    case AST_IF:
      emit_if(build, statement);
      break;
    case AST_WHILE:
      emit_while(build, statement);
      break;
    case AST_BLOCK:
    case AST_FBODY:
      emit_statement_list(build, statement->l);
      break;
    default:
      set_error(build->ir, statement, "unsupported statement in SSA generation");
      break;
  }
}

static void emit_statement_list(builder *build, AST *statement) {
  for (AST *current = statement; current && build->ir->valid; current = current->next) {
    emit_statement(build, current);
  }
}

// Lowers one function body using its semantic function contract.
static function *emit_function(ssa *ir, AST *ast, const SemanticResult *semantic) {
  AST *header = ast->l;
  AST *name_node = header->l;
  AST *params_node = name_node->next;
  function *function_value = calloc(1, sizeof(*function_value));
  builder build = {.ir = ir, .function = function_value, .semantic = semantic};

  function_value->name = duplicate_string(name_node->tok->value);
  stEntry *function_symbol = lookup_function(&build, function_value->name);
  if (!function_symbol || !function_symbol->f_info) {
    set_error(ir, name_node, "no semantic function signature is available for '%s'", function_value->name);
    return function_value;
  }
  function_value->return_type = function_symbol->f_info->ret_type;
  build.block = create_block(build.next_block_id++);
  function_value->entry_block = build.block;
  function_value->blocks = build.block;

  for (AST *parameter = params_node->l; parameter; parameter = parameter->next) {
    if (function_value->param_count >= function_symbol->f_info->n_params) {
      set_error(ir, parameter, "semantic parameter count does not match function '%s'", function_value->name);
      break;
    }
    TYPE parameter_type = function_symbol->f_info->params[function_value->param_count].type;
    if (!is_value_type(parameter_type)) {
      set_error(ir, parameter, "SSA parameters must have a value type");
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
  if (ir->valid && function_value->param_count != function_symbol->f_info->n_params) {
    set_error(ir, name_node, "semantic parameter count does not match function '%s'", function_value->name);
  }
  emit_statement_list(&build, ast->r->l);
  if (ir->valid && !build.block->terminated) {
    if (function_value->return_type == VOID) {
      append_instruction(&build, OP_RET, none_operand(), none_operand(), none_operand());
    } else {
      set_error(ir, name_node, "function '%s' has no return instruction", function_value->name);
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

static global_var *emit_global(ssa *ir, AST *ast) {
  global_var *global = calloc(1, sizeof(*global));
  TYPE type = ast_type(ast->l);
  AST *identifier = ast->r;

  if (identifier->type == AST_ASSIGNMENT) identifier = identifier->l;
  if (!is_value_type(type)) {
    set_error(ir, identifier, "SSA globals must have a value type");
    return global;
  }
  global->name = duplicate_string(identifier->tok->value);
  global->type = type;
  if (ast->r->type != AST_ASSIGNMENT) return global;

  AST *initializer = ast->r->r;
  if (initializer->type != AST_VALUE) {
    set_error(ir, initializer, "global '%s' requires a literal initializer", global->name);
    return global;
  }
  builder build = {.ir = ir};
  global->initializer = literal_operand(&build, initializer);
  if (!ir->valid || global->initializer.type != type) {
    if (ir->valid) set_error(ir, initializer, "global '%s' has an incompatible initializer", global->name);
    return global;
  }
  global->has_initializer = true;
  return global;
}

// Generates a complete module.
ssa *generate_ssair(AST *root, const SemanticResult *semantic, DiagnosticList *diagnostics) {
  ssa *ir = calloc(1, sizeof(*ir));
  ir->valid = true;
  ir->diagnostics = diagnostics;
  if (!root || root->type != AST_PROGRAM || !semantic || !semantic->symbols) {
    set_error(ir, root, "SSA generation requires a program AST and semantic result");
    return ir;
  }

  for (AST *node = root->next; node && ir->valid; node = node->next) {
    ssa_node *module_node = calloc(1, sizeof(*module_node));
    if (node->type == AST_FUNCTION) {
      module_node->type = FUNCTION;
      module_node->value.function = emit_function(ir, node, semantic);
    } else if (node->type == AST_VARIABLE) {
      module_node->type = GLOBAL_VAR;
      module_node->value.global = emit_global(ir, node);
    } else {
      free(module_node);
      set_error(ir, node, "unsupported top-level declaration in SSA generation");
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
    case OP_FADD: return "fadd";
    case OP_FSUB: return "fsub";
    case OP_FMUL: return "fmul";
    case OP_FDIV: return "fdiv";
    case OP_EQ: return "eq";
    case OP_NEQ: return "neq";
    case OP_LT: return "lt";
    case OP_LE: return "le";
    case OP_GT: return "gt";
    case OP_GE: return "ge";
    case OP_NOT: return "not";
    case OP_CALL: return "call";
    case OP_RET: return "ret";
    case OP_PHI: return "phi";
    case OP_BR: return "br";
    case OP_CBR: return "cbr";
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
  if (!ir || !ir->valid) return;
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
          if (instruction_value->data.operands.src1.kind != SSA_OPERAND_NONE) {
            fputc(' ', out);
            print_operand(out, instruction_value->data.operands.src1);
          }
        } else if (instruction_value->op == OP_CALL) {
          fprintf(out, " @%s(", instruction_value->data.call.callee);
          for (size_t index = 0; index < instruction_value->data.call.arg_count; index++) {
            if (index) fputs(", ", out);
            print_operand(out, instruction_value->data.call.args[index]);
          }
          fputc(')', out);
        } else if (instruction_value->op == OP_BR) {
          fprintf(out, " block.%u", instruction_value->data.branch.target->id);
        } else if (instruction_value->op == OP_CBR) {
          fputc(' ', out);
          print_operand(out, instruction_value->data.conditional_branch.condition);
          fprintf(out, ", block.%u, block.%u", instruction_value->data.conditional_branch.true_target->id,
                  instruction_value->data.conditional_branch.false_target->id);
        } else if (instruction_value->op == OP_PHI) {
          fputc(' ', out);
          for (size_t index = 0; index < instruction_value->data.phi.count; index++) {
            if (index) fputs(", ", out);
            fputc('[', out);
            print_operand(out, instruction_value->data.phi.entries[index].value);
            fprintf(out, ", block.%u]", instruction_value->data.phi.entries[index].block->id);
          }
        } else {
          fputc(' ', out);
          print_operand(out, instruction_value->data.operands.src1);
          if (instruction_value->data.operands.src2.kind != SSA_OPERAND_NONE) {
            fputs(", ", out);
            print_operand(out, instruction_value->data.operands.src2);
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
    switch (instruction_value->op) {
      case OP_CALL:
        free(instruction_value->data.call.callee);
        for (size_t index = 0; index < instruction_value->data.call.arg_count; index++) {
          free_operand(instruction_value->data.call.args[index]);
        }
        free(instruction_value->data.call.args);
        break;
      case OP_CBR:
        free_operand(instruction_value->data.conditional_branch.condition);
        break;
      case OP_PHI:
        for (size_t index = 0; index < instruction_value->data.phi.count; index++) {
          free_operand(instruction_value->data.phi.entries[index].value);
        }
        free(instruction_value->data.phi.entries);
        break;
      case OP_BR:
        break;
      default:
        free_operand(instruction_value->data.operands.src1);
        free_operand(instruction_value->data.operands.src2);
        break;
    }
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
  free(ir);
}

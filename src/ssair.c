#include "../include/ssair.h"
#include "../include/analyzer.h"

#include <limits.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#define INVALID_VALUE UINT_MAX

typedef struct {
  size_t *slots;
  size_t capacity;
  size_t count;
} name_index;

typedef struct {
  unsigned *values;
  size_t count;
} binding_snapshot;

typedef struct {
  // Module receiving generated functions and diagnostics.
  ssa *ir;
  // Function currently being lowered.
  function *function;
  // Basic block that receives newly emitted instructions.
  basic_block *block;
  // Index from source names to function->names entries.
  name_index names;
  // Current SSA value ID bound to each name, or INVALID_VALUE when unbound.
  unsigned *bindings;
  size_t binding_capacity;
  // Next SSA version number to assign for each name.
  unsigned *next_versions;
  size_t next_version_capacity;
  // Semantic information used to resolve function signatures.
  const SemanticResult *semantic;
  // ID assigned to the next basic block created for this function.
  unsigned next_block_id;
  // Whether the current block has emitted a terminator.
  bool terminated;
} builder;

static const token *source_token(const AST *node) {
  if (!node) return NULL;
  if (node->has_token) return &node->token;
  const token *left = source_token(node->l);
  return left ? left : source_token(node->r);
}

static char *token_cstr(const token *tok) {
  return strndup(tok->start, (size_t)tok->length);
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

static operand value_operand(unsigned id, TYPE type) {
  return (operand){
      .kind = SSA_OPERAND_VALUE,
      .type = type,
      .data.value = {.id = id},
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

static operand string_operand(const token *value) {
  return (operand){.kind = SSA_OPERAND_STRING, .type = STR,
                   .data.string_value = strndup(value->start, (size_t)value->length)};
}

static operand copy_operand(operand source) {
  operand copy = source;
  if (source.kind == SSA_OPERAND_STRING) copy.data.string_value = strdup(source.data.string_value);
  return copy;
}

static void free_operand(operand operand_value) {
  if (operand_value.kind == SSA_OPERAND_STRING) free(operand_value.data.string_value);
}

static basic_block *create_block(unsigned id) {
  basic_block *block = calloc(1, sizeof(*block));
  block->id = id;
  array_list_init(&block->successors, sizeof(basic_block *));
  array_list_init(&block->predecessors, sizeof(basic_block *));
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
                                ArrayList *args) {
  instruction *instruction_value = append_instruction(build, OP_CALL, dest,
                                                        none_operand(), none_operand());
  instruction_value->data.call.callee = strdup(callee);
  instruction_value->data.call.arg_count = args->count;
  instruction_value->data.call.args = args->data;
  *args = (ArrayList){0};
  return instruction_value;
}

static uint64_t hash_name(const char *name) {
  uint64_t hash = 5381;
  for (unsigned char character; (character = (unsigned char)*name++);) {
    hash = (hash << 5) + hash + character;
  }
  return hash;
}

static void grow_array(void **items, size_t *capacity, size_t required, size_t item_size) {
  if (*capacity >= required) return;
  size_t new_capacity = *capacity ? *capacity : 4;
  while (new_capacity < required) new_capacity *= 2;
  *items = realloc(*items, new_capacity * item_size);
  *capacity = new_capacity;
}

static inline ssa_name *names_data(function *function_value) {
  return function_value->names.data;
}

static inline const ssa_name *names_data_const(const function *function_value) {
  return function_value->names.data;
}

static inline const value_info *values_data_const(const function *function_value) {
  return function_value->values.data;
}

static void rehash_names(builder *build, size_t capacity) {
  size_t *slots = calloc(capacity, sizeof(*slots));
  for (size_t index = 0; index < build->function->names.count; index++) {
    size_t slot = hash_name(names_data_const(build->function)[index].name) % capacity;
    while (slots[slot]) slot = (slot + 1) % capacity;
    slots[slot] = index + 1;
  }
  free(build->names.slots);
  build->names.slots = slots;
  build->names.capacity = capacity;
}

static bool lookup_name(const builder *build, const char *name, size_t *index) {
  if (!build->names.capacity) return false;
  size_t slot = hash_name(name) % build->names.capacity;
  while (build->names.slots[slot]) {
    size_t candidate = build->names.slots[slot] - 1;
    if (strcmp(names_data_const(build->function)[candidate].name, name) == 0) {
      *index = candidate;
      return true;
    }
    slot = (slot + 1) % build->names.capacity;
  }
  return false;
}

static size_t intern_name(builder *build, const char *name) {
  size_t index;
  if (lookup_name(build, name, &index)) return index;
  if (!build->names.capacity || (build->names.count + 1) * 2 > build->names.capacity) {
    rehash_names(build, build->names.capacity ? build->names.capacity * 2 : 16);
  }
  grow_array((void **)&build->bindings, &build->binding_capacity,
             build->function->names.count + 1, sizeof(*build->bindings));
  grow_array((void **)&build->next_versions, &build->next_version_capacity,
             build->function->names.count + 1, sizeof(*build->next_versions));
  index = build->function->names.count;
  ssa_name entry = {.name = strdup(name)};
  array_list_push(&build->function->names, &entry);
  build->bindings[index] = INVALID_VALUE;
  build->next_versions[index] = 0;
  size_t slot = hash_name(name) % build->names.capacity;
  while (build->names.slots[slot]) slot = (slot + 1) % build->names.capacity;
  build->names.slots[slot] = index + 1;
  build->names.count++;
  return index;
}

static inline operand value_from_id(const function *function_value, unsigned id) {
  return value_operand(id, values_data_const(function_value)[id].type);
}

static operand new_value(builder *build, const char *name, TYPE type) {
  size_t name_index = intern_name(build, name);
  unsigned id = (unsigned)build->function->values.count;
  value_info value = {
      .name_index = (unsigned)name_index,
      .version = build->next_versions[name_index]++,
      .type = type,
  };
  array_list_push(&build->function->values, &value);
  return value_operand(id, type);
}

static bool lookup_binding(const builder *build, const char *name, operand *value) {
  size_t name_index;
  if (!lookup_name(build, name, &name_index) || build->bindings[name_index] == INVALID_VALUE) return false;
  *value = value_from_id(build->function, build->bindings[name_index]);
  return true;
}

static bool lookup_binding_at(const builder *build, const binding_snapshot *bindings,
                              size_t name_index, operand *value) {
  if (name_index >= bindings->count || bindings->values[name_index] == INVALID_VALUE) return false;
  *value = value_from_id(build->function, bindings->values[name_index]);
  return true;
}

static void bind_value(builder *build, const char *name, operand value) {
  size_t name_index = intern_name(build, name);
  build->bindings[name_index] = value.data.value.id;
}

static binding_snapshot copy_bindings(const builder *build) {
  binding_snapshot result = {.count = build->function->names.count};
  if (result.count) {
    result.values = malloc(result.count * sizeof(*result.values));
    memcpy(result.values, build->bindings, result.count * sizeof(*result.values));
  }
  return result;
}

static void restore_bindings(builder *build, const binding_snapshot *snapshot) {
  grow_array((void **)&build->bindings, &build->binding_capacity,
             build->function->names.count, sizeof(*build->bindings));
  memcpy(build->bindings, snapshot->values, snapshot->count * sizeof(*build->bindings));
  for (size_t index = snapshot->count; index < build->function->names.count; index++) {
    build->bindings[index] = INVALID_VALUE;
  }
}

static void free_bindings(binding_snapshot *bindings) {
  free(bindings->values);
  bindings->values = NULL;
  bindings->count = 0;
}

static void free_builder_storage(builder *build) {
  free(build->names.slots);
  free(build->bindings);
  free(build->next_versions);
}

static void append_block(function *function_value, basic_block *block) {
  function_value->last_block->next = block;
  function_value->last_block = block;
}

static basic_block *new_block(builder *build) {
  basic_block *block = create_block(build->next_block_id++);
  append_block(build->function, block);
  return block;
}

static void link_blocks(basic_block *from, basic_block *to) {
  array_list_push(&from->successors, &to);
  array_list_push(&to->predecessors, &from);
}

static inline basic_block *predecessor_at(const basic_block *block, size_t index) {
  return *(basic_block *const *)array_list_get_const(&block->predecessors, index);
}

static inline operand *parameter_at(function *function_value, size_t index) {
  return array_list_get(&function_value->params, index);
}

static inline const operand *parameter_at_const(const function *function_value, size_t index) {
  return array_list_get_const(&function_value->params, index);
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
    case SSA_OPERAND_VALUE: return left.data.value.id == right.data.value.id;
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
  return type_node && type_node->has_token ? convertType(type_node->token.type) : VOID;
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
  switch (expression->token.type) {
    case T_LIT_INT:
      return int_operand(strtol(expression->token.start, NULL, 10));
    case T_LIT_FLOAT:
      return float_operand(strtod(expression->token.start, NULL));
    case T_LIT_BOOL:
      return bool_operand(expression->token.length == 4 &&
                          memcmp(expression->token.start, "true", 4) == 0);
    case T_LIT_CHAR:
      return char_operand((unsigned char)expression->token.start[1]);
    case T_LIT_STR:
      return string_operand(&expression->token);
    default:
      set_error(build->ir, expression, "unsupported literal in SSA generation");
      return none_operand();
  }
}

static operand emit_expression(builder *build, AST *expression, const operand *target);

// Lowers a function call and returns its SSA result.
static operand emit_call(builder *build, AST *call, const operand *target) {
  char *callee = token_cstr(&call->l->token);
  stEntry *function_value = lookup_function(build, callee);
  if (!function_value || !function_value->f_info) {
    set_error(build->ir, call->l, "no SSA function signature is available for '%s'", callee);
    free(callee);
    return none_operand();
  }
  func_info *signature = function_value->f_info;
  if (signature->ret_type == VOID && target) {
    set_error(build->ir, call->l, "void function '%s' cannot be used as an expression", callee);
    free(callee);
    return none_operand();
  }

  ArrayList args;
  array_list_init(&args, sizeof(operand));
  for (AST *argument = call->r->l; argument && build->ir->valid; argument = argument->next) {
    operand argument_value = emit_expression(build, argument, NULL);
    if (build->ir->valid &&
        (args.count >= signature->n_params ||
         argument_value.type != signature->params[args.count].type)) {
      set_error(build->ir, argument, "call to '%s' has an incompatible argument", callee);
    }
    array_list_push(&args, &argument_value);
  }
  if (build->ir->valid && args.count != signature->n_params) {
    set_error(build->ir, call->l, "call to '%s' has the wrong number of arguments", callee);
  }
  if (!build->ir->valid) {
    for (size_t index = 0; index < args.count; index++) {
      free_operand(*(operand *)array_list_get(&args, index));
    }
    array_list_free(&args);
    free(callee);
    return none_operand();
  }

  operand result = none_operand();
  if (signature->ret_type != VOID) {
    result = target ? copy_operand(*target) : new_value(build, "tmp", signature->ret_type);
  }
  append_call(build, callee, result, &args);
  free(callee);
  return result;
}

// Lowers an expression.
static operand emit_expression(builder *build, AST *expression, const operand *target) {
  if (!expression || !build->ir->valid) return none_operand();

  if (expression->type == AST_VALUE) {
    return literal_operand(build, expression);
  }

  if (expression->type == AST_ID) {
    operand value;
    char *name = token_cstr(&expression->token);
    if (!lookup_binding(build, name, &value)) {
      set_error(build->ir, expression, "no SSA value is available for '%s'", name);
      free(name);
      return none_operand();
    }
    free(name);
    return value;
  }

  if (expression->type == AST_FCALL) return emit_call(build, expression, target);

  if (expression->type == AST_OPERATOR) {
    const tokenType expression_type = expression->token.type;
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
      operand result = target ? copy_operand(*target) : new_value(build, "tmp", BOOL);
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
      operand result = target ? copy_operand(*target) : new_value(build, "tmp", BOOL);
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
    operand result = target ? copy_operand(*target) : new_value(build, "tmp", result_type);
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
  char *name = token_cstr(&assignment->l->token);
  operand previous;
  TYPE type = declared_type;
  if (type == VOID && lookup_binding(build, name, &previous)) type = previous.type;
  if (!is_value_type(type)) {
    set_error(build->ir, assignment->l, "SSA cannot assign values to '%s' of type %s", name, typeToStr(type));
    free(name);
    return;
  }

  operand destination = new_value(build, name, type);
  operand result = emit_expression(build, assignment->r, &destination);
  if (!build->ir->valid) {
    free_operand(destination);
    free_operand(result);
    free(name);
    return;
  }
  if (result.type != type) {
    set_error(build->ir, assignment->l, "assignment to '%s' has incompatible SSA types", name);
    free_operand(destination);
    free_operand(result);
    free(name);
    return;
  }
  if (result.kind != SSA_OPERAND_VALUE || result.data.value.id != destination.data.value.id) {
    append_instruction(build, OP_COPY, destination, result, none_operand());
  }
  bind_value(build, name, destination);
  free_operand(destination);
  free_operand(result);
  free(name);
}

static void emit_statement_list(builder *build, AST *statement);
static void emit_statement(builder *build, AST *statement);

static void emit_if(builder *build, AST *statement) {
  operand condition = emit_expression(build, statement->l, NULL);
  if (!build->ir->valid || condition.type != BOOL) {
    free_operand(condition);
    return;
  }
  binding_snapshot incoming = copy_bindings(build);
  basic_block *then_block = new_block(build);
  basic_block *else_block = new_block(build);
  basic_block *merge_block = new_block(build);
  emit_conditional_branch(build, condition, then_block, else_block);
  free_operand(condition);

  // Then branch
  build->block = then_block;
  build->terminated = false;
  restore_bindings(build, &incoming);
  emit_statement(build, statement->r);
  binding_snapshot then_bindings = copy_bindings(build);
  basic_block *then_end = build->block;
  bool then_reaches_merge = !then_end->terminated;
  if (then_reaches_merge) emit_branch(build, merge_block);

  // Else branch
  build->block = else_block;
  build->terminated = false;
  restore_bindings(build, &incoming);
  AST *else_statement = statement->r->next;
  if (else_statement) emit_statement(build, else_statement);
  binding_snapshot else_bindings = copy_bindings(build);
  basic_block *else_end = build->block;
  bool else_reaches_merge = !else_end->terminated;
  if (else_reaches_merge) emit_branch(build, merge_block);

  // Merge then and else branches
  build->block = merge_block;
  build->terminated = false;
  restore_bindings(build, &incoming);
  for (size_t name_index = 0; name_index < incoming.count; name_index++) {
    // Handle possible variable reassignments from inside the branches
    operand original;
    operand then_value;
    operand else_value;
    if (!lookup_binding_at(build, &incoming, name_index, &original) ||
        !lookup_binding_at(build, &then_bindings, name_index, &then_value) ||
        !lookup_binding_at(build, &else_bindings, name_index, &else_value)) continue;
    // The variable defined outside of the blocks was not reassigned in
    // either of the branches.
    if (then_reaches_merge && else_reaches_merge &&
        !same_operand(then_value, else_value)) {
      // Both branches modify the variable
      const char *name = names_data_const(build->function)[name_index].name;
      operand destination = new_value(build, name, original.type);
      operand values[2] = {then_value, else_value};
      basic_block *blocks[2] = {then_end, else_end};
      emit_phi(build, destination, values, blocks, 2);
      bind_value(build, name, destination);
      free_operand(destination);
    } else if (then_reaches_merge && !else_reaches_merge) {
      bind_value(build, names_data_const(build->function)[name_index].name, then_value);
    } else if (!then_reaches_merge && else_reaches_merge) {
      bind_value(build, names_data_const(build->function)[name_index].name, else_value);
    }
  }
  free_bindings(&incoming);
  free_bindings(&then_bindings);
  free_bindings(&else_bindings);
}

static void emit_while(builder *build, AST *statement) {
  binding_snapshot incoming = copy_bindings(build);
  basic_block *header = new_block(build);
  basic_block *body = new_block(build);
  basic_block *exit = new_block(build);
  emit_branch(build, header);

  build->block = header;
  build->terminated = false;
  restore_bindings(build, &incoming);
  for (size_t name_index = 0; name_index < incoming.count; name_index++) {
    operand original;
    if (!lookup_binding_at(build, &incoming, name_index, &original)) continue;
    const char *name = names_data_const(build->function)[name_index].name;
    operand destination = new_value(build, name, original.type);
    operand values[2] = {original, none_operand()};
    basic_block *blocks[2] = {predecessor_at(header, 0), NULL};
    emit_phi(build, destination, values, blocks, 2);
    bind_value(build, name, destination);
    free_operand(destination);
  }
  operand condition = emit_expression(build, statement->l, NULL);
  if (build->ir->valid && condition.type == BOOL) emit_conditional_branch(build, condition, body, exit);
  else if (build->ir->valid) set_error(build->ir, statement->l, "while condition must have bool SSA type");
  free_operand(condition);
  binding_snapshot header_bindings = copy_bindings(build);

  build->block = body;
  build->terminated = false;
  emit_statement(build, statement->r);
  binding_snapshot body_bindings = copy_bindings(build);
  basic_block *body_end = build->block;
  // Infinite while loop
  if (!body_end->terminated) emit_branch(build, header);

  for (instruction *instruction_value = header->first; instruction_value &&
       instruction_value->op == OP_PHI; instruction_value = instruction_value->next) {
    size_t name_index = values_data_const(build->function)[instruction_value->dest.data.value.id].name_index;
    operand body_value;
    if (!lookup_binding_at(build, &body_bindings, name_index, &body_value) &&
        !lookup_binding_at(build, &incoming, name_index, &body_value)) continue;
    free_operand(instruction_value->data.phi.entries[1].value);
    instruction_value->data.phi.entries[1].value = copy_operand(body_value);
    instruction_value->data.phi.entries[1].block = body_end;
  }
  build->block = exit;
  build->terminated = false;
  restore_bindings(build, &header_bindings);
  free_bindings(&incoming);
  free_bindings(&header_bindings);
  free_bindings(&body_bindings);
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
  array_list_init(&function_value->params, sizeof(operand));
  array_list_init(&function_value->values, sizeof(value_info));
  array_list_init(&function_value->names, sizeof(ssa_name));

  function_value->name = token_cstr(&name_node->token);
  stEntry *function_symbol = lookup_function(&build, function_value->name);
  if (!function_symbol || !function_symbol->f_info) {
    set_error(ir, name_node, "no semantic function signature is available for '%s'", function_value->name);
    return function_value;
  }
  function_value->return_type = function_symbol->f_info->ret_type;
  build.block = create_block(build.next_block_id++);
  function_value->entry_block = build.block;
  function_value->blocks = build.block;
  function_value->last_block = build.block;

  for (AST *parameter = params_node->l; parameter; parameter = parameter->next) {
    if (function_value->params.count >= function_symbol->f_info->n_params) {
      set_error(ir, parameter, "semantic parameter count does not match function '%s'", function_value->name);
      break;
    }
    TYPE parameter_type = function_symbol->f_info->params[function_value->params.count].type;
    if (!is_value_type(parameter_type)) {
      set_error(ir, parameter, "SSA parameters must have a value type");
      break;
    }
    char *parameter_name = token_cstr(&parameter->r->token);
    operand parameter_value = new_value(&build, parameter_name, parameter_type);
    array_list_push(&function_value->params, &parameter_value);
    bind_value(&build, parameter_name, parameter_value);
    free(parameter_name);
  }
  if (ir->valid && function_value->params.count != function_symbol->f_info->n_params) {
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

  free_builder_storage(&build);
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
  global->name = token_cstr(&identifier->token);
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

static void print_operand(FILE *out, const function *function_value, operand operand_value) {
  switch (operand_value.kind) {
    case SSA_OPERAND_NONE: fputs("void", out); break;
    case SSA_OPERAND_VALUE: {
      const value_info *value = &values_data_const(function_value)[operand_value.data.value.id];
      fprintf(out, "%%%s.%u", names_data_const(function_value)[value->name_index].name, value->version);
      break;
    }
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
        print_operand(out, NULL, global->initializer);
      }
      fputc('\n', out);
      continue;
    }
    const function *function_value = node->value.function;
    fprintf(out, "function %s(", function_value->name);
    for (size_t index = 0; index < function_value->params.count; index++) {
      if (index) fputs(", ", out);
      const operand *parameter = parameter_at_const(function_value, index);
      fprintf(out, "%s ", typeToStr(parameter->type));
      print_operand(out, function_value, *parameter);
    }
    fprintf(out, ") -> %s {\n", typeToStr(function_value->return_type));
    for (const basic_block *block = function_value->blocks; block; block = block->next) {
      fprintf(out, "  block.%u:\n", block->id);
      for (const instruction *instruction_value = block->first; instruction_value;
           instruction_value = instruction_value->next) {
        fputs("    ", out);
        if (instruction_value->op != OP_RET && instruction_value->dest.kind != SSA_OPERAND_NONE) {
          print_operand(out, function_value, instruction_value->dest);
          fputs(" = ", out);
        }
        fputs(opcode_name(instruction_value->op), out);
        if (instruction_value->op == OP_RET) {
          if (instruction_value->data.operands.src1.kind != SSA_OPERAND_NONE) {
            fputc(' ', out);
            print_operand(out, function_value, instruction_value->data.operands.src1);
          }
        } else if (instruction_value->op == OP_CALL) {
          fprintf(out, " @%s(", instruction_value->data.call.callee);
          for (size_t index = 0; index < instruction_value->data.call.arg_count; index++) {
            if (index) fputs(", ", out);
            print_operand(out, function_value, instruction_value->data.call.args[index]);
          }
          fputc(')', out);
        } else if (instruction_value->op == OP_BR) {
          fprintf(out, " block.%u", instruction_value->data.branch.target->id);
        } else if (instruction_value->op == OP_CBR) {
          fputc(' ', out);
          print_operand(out, function_value, instruction_value->data.conditional_branch.condition);
          fprintf(out, ", block.%u, block.%u", instruction_value->data.conditional_branch.true_target->id,
                  instruction_value->data.conditional_branch.false_target->id);
        } else if (instruction_value->op == OP_PHI) {
          fputc(' ', out);
          for (size_t index = 0; index < instruction_value->data.phi.count; index++) {
            if (index) fputs(", ", out);
            fputc('[', out);
            print_operand(out, function_value, instruction_value->data.phi.entries[index].value);
            fprintf(out, ", block.%u]", instruction_value->data.phi.entries[index].block->id);
          }
        } else {
          fputc(' ', out);
          print_operand(out, function_value, instruction_value->data.operands.src1);
          if (instruction_value->data.operands.src2.kind != SSA_OPERAND_NONE) {
            fputs(", ", out);
            print_operand(out, function_value, instruction_value->data.operands.src2);
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
        for (size_t index = 0; index < function_value->params.count; index++) {
          free_operand(*parameter_at(function_value, index));
        }
        array_list_free(&function_value->params);
        array_list_free(&function_value->values);
        for (size_t index = 0; index < function_value->names.count; index++) {
          free(names_data(function_value)[index].name);
        }
        array_list_free(&function_value->names);
        for (basic_block *block = function_value->blocks; block;) {
          basic_block *next_block = block->next;
          free_instruction_list(block->first);
          array_list_free(&block->successors);
          array_list_free(&block->predecessors);
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

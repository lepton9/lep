#include "../include/analyzer.h"
#include <assert.h>
#include <stdarg.h>
#include <string.h>

static inline stEntry* lookup_scope(symtab* st, const char* key) {
  return st_lookup(st, key);
}

static inline stEntry* lookup_scope_view(symtab* st, strview key) {
  return st_lookup_view(st, key);
}

static inline stEntry* lookup_all(symtabStack* sts, const char* key) {
  for (node* scope = sts->s->head; scope != NULL; scope = scope->next) {
    stEntry* e = st_lookup(scope->data, key);
    if (e) return e;
  }
  return NULL;
}

static inline stEntry* lookup_all_view(symtabStack* sts, strview key) {
  for (node* scope = sts->s->head; scope != NULL; scope = scope->next) {
    stEntry* e = st_lookup_view(scope->data, key);
    if (e) return e;
  }
  return NULL;
}

symtab* current_scope(symtabStack* sts) {
  return sts->s->head->data;
}

static void semantic_error(SemanticAnalyzer *analyzer, const cLoc *location,
                           const char *format, ...) {
  va_list args;
  va_start(args, format);
  add_diagnosticv(analyzer->diagnostics, DIAGNOSTIC_ERROR, "semantic", location, format, args);
  va_end(args);
}

// Check if the block including the statement returns.
static bool definitely_returns(AST *statement) {
  for (AST *current = statement; current; current = current->next) {
    if (current->type == AST_RET) return true;
    if (current->type == AST_BLOCK && definitely_returns(current->l)) return true;
    if (current->type == AST_IF) {
      AST *else_branch = current->r ? current->r->next : NULL;
      if (else_branch && definitely_returns(current->r->l) &&
          definitely_returns(else_branch->type == AST_IF ? else_branch : else_branch->l)) {
        return true;
      }
    }
  }
  return false;
}

symtabStack* init_st_stack() {
  symtabStack* sts = malloc(sizeof(symtabStack));
  sts->cur_scope = 0;
  sts->s = create_list();
  symtab* global_st = initSymbolTable();
  add_to_begin(sts->s, global_st);
  return sts;
}

void free_st_stack(symtabStack* sts) {
  while(!is_empty(sts->s)) {
    symtab* st = pop_front(sts->s);
    freeSymbolTable(st);
  }
  free(sts->s);
  free(sts);
}

static stEntry* new_variable(symtabStack* sts, const char* id, const TYPE type) {
  stEntry* var = st_insert(current_scope(sts), id);
  var->type = type;
  var->is_initialized = false;
  return var;
}

static stEntry* new_variable_view(symtabStack* sts, strview id, TYPE type) {
  stEntry* var = st_insert_view(current_scope(sts), id);
  var->type = type;
  var->is_initialized = false;
  return var;
}

// Collect all the function declarations from the AST.
static void declare_functions(SemanticAnalyzer *analyzer, AST *root) {
  for (AST *node = root->next; node; node = node->next) {
    if (node->type != AST_FUNCTION) continue;
    AST *header = node->l;
    AST *name_node = header->l;
    AST *params_node = name_node->next;
    AST *return_type_node = params_node->next;
    token *name = &name_node->token;
    strview name_value = token_view(name);
    if (lookup_scope_view(current_scope(analyzer->symbols), name_value)) {
      semantic_error(analyzer, &name->loc, "redefinition of function '%.*s'",
                     (int)name_value.length, name_value.start);
      continue;
    }
    stEntry *function_value = new_variable_view(analyzer->symbols, name_value, F);
    function_value->declLine = name->loc.line;
    function_value->f_info = calloc(1, sizeof(*function_value->f_info));
    function_value->f_info->ret_type = token_to_type(return_type_node->token.type);
    for (AST *parameter_node = params_node->l; parameter_node;
         parameter_node = parameter_node->next) {
      size_t index = function_value->f_info->n_params++;
      function_value->f_info->params = realloc(function_value->f_info->params,
          function_value->f_info->n_params * sizeof(*function_value->f_info->params));
      function_value->f_info->params[index] = (parameter){
          token_strdup(&parameter_node->r->token),
          token_to_type(parameter_node->l->token.type),
      };
    }
  }
}

static bool match_type(const TYPE a, const TYPE b) {
  if (a == TYPE_ERROR || b == TYPE_ERROR) return true;
  return a == b;
}

void semantic_analysis(SemanticAnalyzer* analyzer, AST* ast) {
  if (ast == NULL) return;
  symtabStack* sts = analyzer->symbols;

  switch(ast->type) {
    case AST_FUNCTION: {
      AST* header = ast->l;
      AST* body = ast->r;
      token* name = &header->l->token;
      stEntry* f = lookup_scope_view(current_scope(sts), token_view(name));
      assert(f && f->type == F && f->f_info);
      enter_scope(sts);
      {
        context* previous_function = analyzer->current_function;
        context* c = malloc(sizeof(context));
        c->ret_type = f->f_info->ret_type;
        c->func_name = f->name;
        c->returned = false;
        c->ret_scope = -1;
        c->declaration_location = name->loc;
        analyzer->current_function = c;

        semantic_analysis(analyzer, header);
        semantic_analysis(analyzer, body);

        if (f->f_info->ret_type != VOID && !definitely_returns(body->l)) {
          semantic_error(analyzer, &c->declaration_location,
                         "function '%s' does not return from all code paths", f->name);
        }

        analyzer->current_function = previous_function;
        free(c);
      }
      exit_scope(sts);
      semantic_analysis(analyzer, ast->next);
      return;
    }
    case AST_RET: {
      TYPE ret_type = (ast->l) ? expr_type(analyzer, ast->l) : VOID;
      TYPE func_ret_type = TYPE_ERROR;
      if (analyzer->current_function != NULL) {
        context* c = analyzer->current_function;
        func_ret_type = c->ret_type;
        c->returned = true;
        if (c->ret_scope < 0 || c->ret_scope > sts->cur_scope) {
          c->ret_scope = sts->cur_scope;
        }
      } else {
        semantic_error(analyzer, &ast->token.loc, "return statement outside of a function");
      }
      if (ret_type != TYPE_ERROR && func_ret_type != TYPE_ERROR && !match_type(ret_type, func_ret_type)) {
        semantic_error(analyzer, &ast->token.loc, "wrong return type: expected %s, found %s",
                       typeToStr(func_ret_type), typeToStr(ret_type));
      }
      break;
    }
    case AST_FCALL: {
      typecheck_fcall(analyzer, ast);
      break;
    }
    case AST_IF:
    case AST_WHILE: {
      TYPE condition_type = expr_type(analyzer, ast->l);
      if (condition_type != TYPE_ERROR && condition_type != BOOL) {
        semantic_error(analyzer, &ast->token.loc, "%s condition must have type bool",
                       ast->type == AST_IF ? "if" : "while");
      }
      semantic_analysis(analyzer, ast->r);
      semantic_analysis(analyzer, ast->next);
      return;
    }
    case AST_BLOCK: {
      enter_scope(sts);
      semantic_analysis(analyzer, ast->l);
      exit_scope(sts);
      semantic_analysis(analyzer, ast->next);
      return;
    }
    case AST_VARIABLE: {
      AST* id = ast->r;
      if (id->type == AST_ASSIGNMENT) id = ast->r->l;
      strview id_value = token_view(&id->token);
      if (lookup_all_view(sts, id_value)) {
        semantic_error(analyzer, &id->token.loc, "redefinition of '%.*s'",
                       (int)id_value.length, id_value.start);
        break;
      }
      TYPE variable_type = token_to_type(ast->l->token.type);
      if (variable_type == VOID || variable_type == F) {
        semantic_error(analyzer, &id->token.loc, "variables cannot have type %s", typeToStr(variable_type));
        break;
      }
      stEntry* var = new_variable_view(sts, id_value, variable_type);
      var->declLine = id->token.loc.line;

      if (analyzer->current_function != NULL) {
        context* c = analyzer->current_function;
        stEntry* f = lookup_all(sts, c->func_name);
        for (size_t i = 0; i < f->f_info->n_params; i++) {
          if (strcmp(f->f_info->params[i].name, var->name) == 0) {
            var->is_initialized = true;
            break;
          }
        }
      }
      break;
    }
    case AST_ASSIGNMENT: {
      typecheck_assignment(analyzer, ast->l, ast->r);
      semantic_analysis(analyzer, ast->next);
      return;
    }
    default:
      break;
  }
  semantic_analysis(analyzer, ast->l);
  semantic_analysis(analyzer, ast->r);
  semantic_analysis(analyzer, ast->next);
}

// TODO: needed?
static int funcRetType(AST* f) {
  return f->l->l->next->next->token.type;
}

TYPE token_to_type(const tokenType type) {
  switch (type) {
    case T_KW_INT:
    case T_LIT_INT:
      return INT;
    case T_KW_FLOAT:
    case T_LIT_FLOAT:
      return FLOAT;
    case T_KW_CHAR:
    case T_LIT_CHAR:
      return CHAR;
    case T_KW_STR:
    case T_LIT_STR:
      return STR;
    case T_KW_BOOL:
    case T_LIT_BOOL:
      return BOOL;
    case T_KW_MAIN:
    case T_KW_F:
      return F;
    case T_KW_VOID:
      return VOID;
    default:
      return TYPE_ERROR;
  }
}

TYPE expr_type(SemanticAnalyzer *analyzer, AST* expr) {
  symtabStack *sts = analyzer->symbols;
  if (expr->type == AST_OPERATOR) {
    return typecheck_operator(analyzer, expr);
  }
  else if (expr->type == AST_FCALL) {
    if (!typecheck_fcall(analyzer, expr)) return TYPE_ERROR;
    strview f_id = token_view(&expr->l->token);
    stEntry* f = lookup_all_view(sts, f_id);
    TYPE type = f->f_info->ret_type;
    if (match_type(type, VOID)) {
      semantic_error(analyzer, &expr->l->token.loc,
                     "void function '%.*s' cannot be used as an expression",
                     (int)f_id.length, f_id.start);
      return TYPE_ERROR;
    }
    return type;
  }
  else if (expr->type == AST_ID) {
    strview id_value = token_view(&expr->token);
    stEntry* var = lookup_all_view(sts, id_value);
    if (!var) {
      semantic_error(analyzer, &expr->token.loc, "use of undeclared variable '%.*s'",
                     (int)id_value.length, id_value.start);
      return TYPE_ERROR;
    }
    if (!var->is_initialized) {
      semantic_error(analyzer, &expr->token.loc, "variable '%s' has no assigned value", var->name);
      return TYPE_ERROR;
    }
    return var->type;
  }
  return token_to_type(expr->token.type);
}

bool typecheck_fcall(SemanticAnalyzer *analyzer, AST* fcall) {
  symtabStack *sts = analyzer->symbols;
  AST* arg = fcall->r->l;
  strview name = token_view(&fcall->l->token);
  stEntry* f = lookup_all_view(sts, name);
  if (!f) {
    semantic_error(analyzer, &fcall->l->token.loc, "no function named '%.*s' found",
                   (int)name.length, name.start);
    return false;
  }
  size_t i = 0;
  while(arg && i < f->f_info->n_params) {
    TYPE type = expr_type(analyzer, arg);
    if (!match_type(type, f->f_info->params[i].type)) {
      semantic_error(analyzer, &arg->token.loc, "wrong argument type: expected %s, found %s",
                     typeToStr(f->f_info->params[i].type), typeToStr(type));
    }
    i++;
    arg = arg->next;
  }
  if (arg != NULL) {
    semantic_error(analyzer, &arg->token.loc, "too many arguments for function '%s'", f->name);
  }
  else if (i < f->f_info->n_params) {
    semantic_error(analyzer, &fcall->l->token.loc, "too few arguments for function '%s'", f->name);
  }

  return true;
}

bool typecheck_assignment(SemanticAnalyzer *analyzer, AST* lhs, AST* rhs) {
  symtabStack *sts = analyzer->symbols;
  TYPE lhs_type, rhs_type;
  strview id = token_view(&lhs->token);
  stEntry* var = lookup_all_view(sts, id);
  if (!var) {
    semantic_error(analyzer, &lhs->token.loc, "assignment to undeclared variable '%.*s'",
                   (int)id.length, id.start);
    expr_type(analyzer, rhs);
    return false;
  }
  lhs_type = var->type;
  rhs_type = expr_type(analyzer, rhs);

  if (rhs_type == TYPE_ERROR) return false;
  if (!match_type(lhs_type, rhs_type)) {
    semantic_error(analyzer, &lhs->token.loc, "incompatible assignment: expected %s, found %s",
                   typeToStr(lhs_type), typeToStr(rhs_type));
  }
  var->is_initialized = true;
  return true;
}

TYPE typecheck_operator(SemanticAnalyzer *analyzer, AST *op) {
  AST *lhs = op->l;
  AST *rhs = op->r;
  TYPE lt = expr_type(analyzer, lhs);
  if (op->token.type == T_BANG) {
    if (lt != TYPE_ERROR && lt != BOOL) {
      semantic_error(analyzer, &op->token.loc, "'!' requires a bool operand");
      return TYPE_ERROR;
    }
    return BOOL;
  }

  TYPE rt = expr_type(analyzer, rhs);
  if (lt == TYPE_ERROR || rt == TYPE_ERROR) return TYPE_ERROR;

  if (!match_type(lt, rt)) {
    semantic_error(analyzer, &lhs->token.loc, "operator operands must have the same type: %s and %s",
                   typeToStr(lt), typeToStr(rt));
    return TYPE_ERROR;
  }

  TYPE type = lt;
  switch (op->token.type) {
    case T_EQ:
    case T_NEQ:
      if (type == STR || type == VOID || type == F) {
        semantic_error(analyzer, &op->token.loc, "equality does not support %s operands", typeToStr(type));
        return TYPE_ERROR;
      }
      return BOOL;
    case T_LT:
    case T_LE:
    case T_GT:
    case T_GE:
      if (type != INT && type != FLOAT && type != CHAR) {
        semantic_error(analyzer, &op->token.loc, "relational operators require int, float, or char operands");
        return TYPE_ERROR;
      }
      return BOOL;
    case T_AND:
    case T_OR:
      if (type != BOOL) {
        semantic_error(analyzer, &op->token.loc, "logical operators require bool operands");
        return TYPE_ERROR;
      }
      return BOOL;
    default:
      if (type == INT || type == FLOAT) return type;
      semantic_error(analyzer, &op->token.loc, "arithmetic operators require int or float operands");
      return TYPE_ERROR;
  }
}

SemanticResult* analyze_ast(AST* root, DiagnosticList *diagnostics) {
  SemanticAnalyzer analyzer = {init_st_stack(), NULL, diagnostics};
  if (root && root->type == AST_PROGRAM) declare_functions(&analyzer, root);
  semantic_analysis(&analyzer, root);
  assert(analyzer.symbols->cur_scope == 0);

  SemanticResult* result = malloc(sizeof(SemanticResult));
  result->symbols = analyzer.symbols;
  return result;
}

void free_semantic_result(SemanticResult* result) {
  if (result == NULL) return;
  free_st_stack(result->symbols);
  free(result);
}

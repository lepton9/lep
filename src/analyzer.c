#include "../include/analyzer.h"
#include <assert.h>
#include <stdarg.h>
#include <string.h>

static char *token_cstr(const token *token) {
  return strndup(token->start, (size_t)token->length);
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

stEntry* lookup_all(symtabStack* sts, const char* key) {
  node* scope = sts->s->head;
  stEntry* e;
  while(scope != NULL) {
    e = lookup_scope(scope->data, key);
    if (e) return e;
    scope = scope->next;
  }
  return NULL;
}

stEntry* lookup_scope(symtab* st, const char* key) {
  return st_lookup(st, key);
}

symtab* currentScope(symtabStack* sts) {
  return sts->s->head->data;
}

stEntry* newVariable(symtabStack* sts, const char* id, const TYPE type) {
  stEntry* var = st_insert(currentScope(sts), id);
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
    char *name_value = token_cstr(name);
    if (lookup_scope(currentScope(analyzer->symbols), name_value)) {
        semantic_error(analyzer, &name->loc, "redefinition of function '%s'", name_value);
        free(name_value);
       continue;
    }
    stEntry *function_value = newVariable(analyzer->symbols, name_value, F);
    free(name_value);
    function_value->declLine = name->loc.line;
    function_value->f_info = calloc(1, sizeof(*function_value->f_info));
    function_value->f_info->ret_type = convertType(return_type_node->token.type);
    for (AST *parameter_node = params_node->l; parameter_node;
         parameter_node = parameter_node->next) {
      size_t index = function_value->f_info->n_params++;
      function_value->f_info->params = realloc(function_value->f_info->params,
          function_value->f_info->n_params * sizeof(*function_value->f_info->params));
      function_value->f_info->params[index] = (parameter){
          token_cstr(&parameter_node->r->token),
          convertType(parameter_node->l->token.type),
      };
    }
  }
}

void semanticAnalysis(SemanticAnalyzer* analyzer, AST* ast) {
  if (ast == NULL) return;
  symtabStack* sts = analyzer->symbols;

  switch(ast->type) {
    case AST_FUNCTION: {
      AST* header = ast->l;
      AST* body = ast->r;
      token* name = &header->l->token;
      char *name_value = token_cstr(name);
      stEntry* f = lookup_scope(currentScope(sts), name_value);
      free(name_value);
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

        semanticAnalysis(analyzer, header);
        semanticAnalysis(analyzer, body);

        if (f->f_info->ret_type != VOID && !definitely_returns(body->l)) {
          semantic_error(analyzer, &c->declaration_location,
                         "function '%s' does not return from all code paths", f->name);
        }

        analyzer->current_function = previous_function;
        free(c);
      }
      exit_scope(sts);
      semanticAnalysis(analyzer, ast->next);
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
      if (ret_type != TYPE_ERROR && func_ret_type != TYPE_ERROR && !matchType(ret_type, func_ret_type)) {
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
      semanticAnalysis(analyzer, ast->r);
      semanticAnalysis(analyzer, ast->next);
      return;
    }
    case AST_BLOCK: {
      enter_scope(sts);
      semanticAnalysis(analyzer, ast->l);
      exit_scope(sts);
      semanticAnalysis(analyzer, ast->next);
      return;
    }
    case AST_VARIABLE: {
      AST* id = ast->r;
      if (id->type == AST_ASSIGNMENT) id = ast->r->l;
      char *id_value = token_cstr(&id->token);
      if (lookup_all(sts, id_value)) {
        semantic_error(analyzer, &id->token.loc, "redefinition of '%s'", id_value);
        free(id_value);
        break;
      }
      TYPE variable_type = convertType(ast->l->token.type);
      if (variable_type == VOID || variable_type == F) {
        semantic_error(analyzer, &id->token.loc, "variables cannot have type %s", typeToStr(variable_type));
        free(id_value);
        break;
      }
      stEntry* var = newVariable(sts, id_value, variable_type);
      free(id_value);
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
      semanticAnalysis(analyzer, ast->next);
      return;
    }
    default:
      break;
  }
  semanticAnalysis(analyzer, ast->l);
  semanticAnalysis(analyzer, ast->r);
  semanticAnalysis(analyzer, ast->next);
}

// To be deleted
int funcRetType(AST* f) {
  return f->l->l->next->next->token.type;
}

TYPE convertType(const int type) {
  switch (type) {
    case T_KW_INT:
    case T_LIT_INT:
    case INT:
      return INT;
    case T_KW_FLOAT:
    case T_LIT_FLOAT:
    case FLOAT:
      return FLOAT;
    case T_KW_CHAR:
    case T_LIT_CHAR:
    case CHAR:
      return CHAR;
    case T_KW_STR:
    case T_LIT_STR:
    case STR:
      return STR;
    case T_KW_BOOL:
    case T_LIT_BOOL:
    case BOOL:
      return BOOL;
    case T_KW_MAIN:
    case T_KW_F:
    case F:
      return F;
    case T_KW_VOID:
    case VOID:
      return VOID;
    default:
      return TYPE_ERROR;
  }
}

bool matchType(const int a, const int b) {
  if (a == TYPE_ERROR || b == TYPE_ERROR) return true;
  return convertType(a) == convertType(b);
}

TYPE expr_type(SemanticAnalyzer *analyzer, AST* expr) {
  symtabStack *sts = analyzer->symbols;
  int type;
  if (expr->type == AST_OPERATOR) {
    type = typecheck_operator(analyzer, expr);
  }
  else if (expr->type == AST_FCALL) {
    if (!typecheck_fcall(analyzer, expr)) return TYPE_ERROR;
    char* f_id = token_cstr(&expr->l->token);
    stEntry* f = lookup_all(sts, f_id);
    type = f->f_info->ret_type;
    if (matchType(type, VOID)) {
      semantic_error(analyzer, &expr->l->token.loc,
                     "void function '%s' cannot be used as an expression", f_id);
      free(f_id);
      return TYPE_ERROR;
    }
    free(f_id);
  }
  else if (expr->type == AST_ID) {
    char *id_value = token_cstr(&expr->token);
    stEntry* var = lookup_all(sts, id_value);
    if (!var) {
      semantic_error(analyzer, &expr->token.loc, "use of undeclared variable '%s'", id_value);
      free(id_value);
      return TYPE_ERROR;
    }
    free(id_value);
    if (!var->is_initialized) {
      semantic_error(analyzer, &expr->token.loc, "variable '%s' has no assigned value", var->name);
      return TYPE_ERROR;
    }
    type = var->type;
  } else {
    type = expr->token.type;
  }
  return convertType(type);
}

bool typecheck_fcall(SemanticAnalyzer *analyzer, AST* fcall) {
  symtabStack *sts = analyzer->symbols;
  AST* arg = fcall->r->l;
  char *name = token_cstr(&fcall->l->token);
  stEntry* f = lookup_all(sts, name);
  if (!f) {
    semantic_error(analyzer, &fcall->l->token.loc, "no function named '%s' found", name);
    free(name);
    return false;
  }
  free(name);
  size_t i = 0;
  while(arg && i < f->f_info->n_params) {
    TYPE type = expr_type(analyzer, arg);
    if (!matchType(type, f->f_info->params[i].type)) {
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
  int lhs_type, rhs_type;
  char* id = token_cstr(&lhs->token);
  stEntry* var = lookup_all(sts, id);
  if (!var) {
    semantic_error(analyzer, &lhs->token.loc, "assignment to undeclared variable '%s'", id);
    free(id);
    expr_type(analyzer, rhs);
    return false;
  }
  free(id);
  lhs_type = var->type;
  rhs_type = expr_type(analyzer, rhs);

  if (rhs_type == TYPE_ERROR) return false;
  if (!matchType(lhs_type, rhs_type)) {
    semantic_error(analyzer, &lhs->token.loc, "incompatible assignment: expected %s, found %s",
                   typeToStr(lhs_type), typeToStr(rhs_type));
  }
  var->is_initialized = true;
  return true;
}

int typecheck_operator(SemanticAnalyzer *analyzer, AST *op) {
  AST *lhs = op->l;
  AST *rhs = op->r;
  int lt = expr_type(analyzer, lhs);
  if (op->token.type == T_BANG) {
    if (lt != TYPE_ERROR && lt != BOOL) {
      semantic_error(analyzer, &op->token.loc, "'!' requires a bool operand");
      return TYPE_ERROR;
    }
    return BOOL;
  }

  int rt = expr_type(analyzer, rhs);
  if (lt == TYPE_ERROR || rt == TYPE_ERROR) return TYPE_ERROR;

  if (!matchType(lt, rt)) {
    semantic_error(analyzer, &lhs->token.loc, "operator operands must have the same type: %s and %s",
                   typeToStr(lt), typeToStr(rt));
    return TYPE_ERROR;
  }

  TYPE type = convertType(lt);
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

SemanticResult* analyzeAST(AST* root, DiagnosticList *diagnostics) {
  SemanticAnalyzer analyzer = {init_st_stack(), NULL, diagnostics};
  if (root && root->type == AST_PROGRAM) declare_functions(&analyzer, root);
  semanticAnalysis(&analyzer, root);
  assert(analyzer.symbols->cur_scope == 0);

  SemanticResult* result = malloc(sizeof(SemanticResult));
  result->symbols = analyzer.symbols;
  return result;
}

void freeSemanticResult(SemanticResult* result) {
  if (result == NULL) return;
  free_st_stack(result->symbols);
  free(result);
}

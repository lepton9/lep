#include "../include/analyzer.h"
#include "../include/errorlep.h"
#include <assert.h>
#include <string.h>

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

void semanticAnalysis(SemanticAnalyzer* analyzer, AST* ast) {
  if (ast == NULL) return;
  symtabStack* sts = analyzer->symbols;

  switch(ast->type) {
    case AST_FUNCTION: {
      AST* header = ast->l;
      AST* body = ast->r;
      AST* param = header->l->next->l;
      token* name = header->l->tok;
      if (lookup_all(sts, name->value)) {
        error_redef(name->value, F, name->loc);
      }
      stEntry* f = newVariable(sts, name->value, F);
      f->declLine = name->loc.line;
      f->f_info = malloc(sizeof(func_info));
      f->f_info->ret_type = convertType(ast->l->l->next->next->tok->type);
      f->f_info->n_params = 0;
      f->f_info->params = NULL;
      while (param) {
        f->f_info->params = realloc(f->f_info->params, sizeof(parameter) * (f->f_info->n_params + 1));
        f->f_info->params[f->f_info->n_params] = (parameter){param->r->tok->value, convertType(param->l->tok->type)};
        assert(param->type == AST_VARIABLE);
        f->f_info->n_params++;
        param = param->next;
      }
      enter_scope(sts);
      {
        context* previous_function = analyzer->current_function;
        context* c = malloc(sizeof(context));
        c->ret_type = f->f_info->ret_type;
        c->func_name = f->name;
        c->returned = false;
        c->ret_scope = -1;
        analyzer->current_function = c;

        semanticAnalysis(analyzer, header);
        semanticAnalysis(analyzer, body);

        if (f->f_info->ret_type != VOID) {
          if (!c->returned) {
            error_ret("not returning a value", f->name);
          }
          if (c->ret_scope != sts->cur_scope) {
            error_ret("not returning from all code paths", f->name);
          }
        }

        analyzer->current_function = previous_function;
        free(c);
      }
      exit_scope(sts);
      semanticAnalysis(analyzer, ast->next);
      return;
    }
    case AST_RET: {
      TYPE ret_type = (ast->l) ? expr_type(sts, ast->l) : VOID;
      TYPE func_ret_type;
      if (analyzer->current_function != NULL) {
        context* c = analyzer->current_function;
        func_ret_type = c->ret_type;
        c->returned = true;
        if (c->ret_scope < 0 || c->ret_scope > sts->cur_scope) {
          c->ret_scope = sts->cur_scope;
        }
      } else {
        error_semantic("Return statement outside of a function\n", ast->tok->loc);
      }
      if (!matchType(ret_type, func_ret_type)) {
        error_type("Wrong function return type", ast->tok->loc, ret_type, func_ret_type);
      }
      break;
    }
    case AST_FCALL: {
      typecheck_fcall(sts, ast);
      break;
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
      if (lookup_all(sts, id->tok->value)) {
        error_redef(id->tok->value, convertType(ast->l->tok->type), id->tok->loc);
      }
      stEntry* var = newVariable(sts, id->tok->value, convertType(ast->l->tok->type));
      var->declLine = id->tok->loc.line;

      if (analyzer->current_function != NULL) {
        context* c = analyzer->current_function;
        stEntry* f = lookup_all(sts, c->func_name);
        for (int i = 0; i < f->f_info->n_params; i++) {
          if (strcmp(f->f_info->params[i].name, var->name) == 0) {
            var->is_initialized = true;
            break;
          }
        }
      }
      break;
    }
    case AST_ASSIGNMENT: {
      typecheck_assignment(sts, ast->l, ast->r);
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
  return f->l->l->next->next->tok->type;
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
      return -1;
  }
}

bool matchType(const int a, const int b) {
  return convertType(a) == convertType(b);
}

TYPE expr_type(symtabStack* sts, AST* expr) {
  int type;
  if (expr->type == AST_OPERATOR) {
    type = typecheck_operator(sts, expr->l, expr->r);
  }
  else if (expr->type == AST_FCALL) {
    typecheck_fcall(sts, expr);
    char* f_id = expr->l->tok->value;
    stEntry* f = lookup_all(sts, f_id);
    type = f->f_info->ret_type;
    if (matchType(type, VOID)) {
      error_semantic("Function with ret type 'void' called in an expression", expr->l->tok->loc);
    }
  }
  else if (expr->type == AST_ID) {
    stEntry* var = lookup_all(sts, expr->tok->value);
    if (!var) {
      error_nodecl("Access", expr->tok->value, expr->tok->loc);
    }
    if (!var->is_initialized) {
      error_value(var->name, var->type, expr->tok->loc);
    }
    type = var->type;
  } else {
    type = expr->tok->type;
  }
  return convertType(type);
}

bool typecheck_fcall(symtabStack* sts, AST* fcall) {
  AST* arg = fcall->r->l;
  stEntry* f = lookup_all(sts, fcall->l->tok->value);
  if (!f) {
    error_fnotfound(fcall->l->tok->value, fcall->l->tok->loc);
  }
  int i = 0;
  while(arg && i < f->f_info->n_params) {
    TYPE type = expr_type(sts, arg);
    if (!matchType(type, f->f_info->params[i].type)) {
      error_type("Wrong function argument type", arg->tok->loc, type, f->f_info->params[i].type);
    }
    i++;
    arg = arg->next;
  }
  if (arg != NULL) {
    error_semantic("Too many arguments given to fcall", arg->tok->loc);
  }
  else if (i < f->f_info->n_params) {
    error_semantic("Too few arguments given to fcall", fcall->l->tok->loc);
  }

  return true;
}

bool typecheck_assignment(symtabStack* sts, AST* lhs, AST* rhs) {
  int lhs_type, rhs_type;
  char* id = lhs->tok->value;
  stEntry* var = lookup_all(sts, id);
  if (!var) {
    error_nodecl("Assignment", id, lhs->tok->loc);
  }
  lhs_type = var->type;
  rhs_type = expr_type(sts, rhs);

  if (!matchType(lhs_type, rhs_type)) {
    error_type("Trying to assign a value of the wrong type", lhs->tok->loc, rhs_type, lhs_type);
  }
  var->is_initialized = true;
  return true;
}

int typecheck_operator(symtabStack* sts, AST* lhs, AST* rhs) {
  int lt = expr_type(sts, lhs);
  int rt = expr_type(sts, rhs);

  if (matchType(lt, rt)) return lt;
  else return -1;
}

SemanticResult* analyzeAST(AST* root) {
  SemanticAnalyzer analyzer = {init_st_stack(), NULL};
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

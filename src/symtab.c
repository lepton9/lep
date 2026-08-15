#include "../include/symtab.h"
#include <string.h>
#include <assert.h>


symtab* initSymbolTable() {
  symtab* st = init_hashtabn(41);
  return st;
}

void freeSymbolTable(symtab* st) {
  for (size_t i = 0; i < st->size; i++) {
    if (st->items[i]) {
      free_stEntry(st->items[i]->value);
      free_item(st->items[i]);
    }
  }
  free(st->items);
  free(st);
}

void free_stEntry(stEntry* entry) {
  free(entry->name);
  if (entry->f_info) {
    free(entry->f_info->params);
    free(entry->f_info);
  }
  free(entry);
}

stEntry* st_insert(symtab* st, const char* key) {
  stEntry* e = calloc(1, sizeof(stEntry));
  e->name = malloc(strlen(key) + 1);
  strcpy(e->name, key);
  ht_e* hte = ht_insert(st, key, e);
  return hte->value;
}

stEntry* st_lookup(symtab* st, const char* identifier) {
  stEntry* e = (stEntry*)ht_get(st, identifier);
  return e;
}

void enter_scope(symtabStack* sts) {
  symtab* st = initSymbolTable();
  add_to_begin(sts->s, st);
  sts->cur_scope++;
}

void exit_scope(symtabStack* sts) {
  // if (sts->cur_scope == 0) return;
  assert(sts->cur_scope > 0);
  symtab* st = pop_front(sts->s);
  freeSymbolTable(st);
  sts->cur_scope--;
}

const char* typeToStr(const TYPE type) {
  switch (type) {
    case INT: return "int";
    case FLOAT: return "float";
    case CHAR: return "char";
    case STR: return "str";
    case BOOL: return "bool";
    case F: return "f";
    case VOID: return "void";
  }
  return NULL;
}

void print_symtab(FILE* out, const symtab* st) {
  fprintf(out, "\n<-------------------->\n");
  for (size_t i = 0; i < st->size; i++) {
    if (st->items[i]) print_stEntry(out, st->items[i]->value);
  }
  fprintf(out, ">--------------------<\n");
}

void print_stEntry(FILE* out, const stEntry* e) {
  fprintf(out, "%s:%s | decl: %d", typeToStr(e->type), e->name, e->declLine);
  if (e->f_info) {
    fprintf(out, " | ret: %s args: ( ", typeToStr(e->f_info->ret_type));
    for (size_t i = 0; i < e->f_info->n_params; i++) {
      fprintf(out, "%s ", typeToStr(e->f_info->params[i].type));
    }
    fprintf(out, ")");
  }
  fprintf(out, "\n");
}

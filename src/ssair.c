#include "../include/ssair.h"
#include <string.h>

ssa* init_ssa() {
  ssa* ssaIR = malloc(sizeof(ssa));
  return ssaIR;
}

function* create_function(const char* name, const int param_n) {
  function* f = malloc(sizeof(function));
  f->name = strdup(name);
  f->param_n = param_n;
  f->params = malloc(param_n * sizeof(var*));
  for (int i = 0; i < param_n; i++) f->params[i] = malloc(sizeof(var));
  return f;
}

// TODO: or from stEntry
char* var_name(const char* name, const int version) {
  if (version == 0) return (char*)name;
  char* ssa_id = malloc(strlen(name) + 10);
  sprintf(ssa_id, "%s_%d", name, version);
  return ssa_id;
}

// TODO: needed? Or keep the base names in stEntry
char* getBaseName(const char* var) {
  char* baseName = strdup(var);
  char* underscore = strrchr(baseName, '_');
  if (underscore != NULL) {
    *underscore = '\0';
  }
  return baseName;
}

// TODO: add type of the operand and dest vars
// TODO: modify to allow constants as operands
instruction* create_instruction(opcode op, char* dest, char* src1, char* src2) {
  instruction* i = malloc(sizeof(instruction));
  i->next = NULL;
  i->op = op;
  i->dest.op_type = SSA_VAR;
  i->src1.op_type = SSA_VAR;
  i->src2.op_type = SSA_VAR;
  i->dest.value.var.name = strdup(dest);
  i->src1.value.var.name = strdup(src1);
  i->src2.value.var.name = strdup(src2);

  return i;
}

basic_block* create_block(int id) {

}

ssa* generate_ssair(AST* root) {
  ssa* ssair = init_ssa();
  ssa_node* back = ssair->entry;
  symtabStack* sts = init_st_stack();
  AST* node = root;
  while(node) {
    ssa_node* ssaNode;
    switch (node->type) {
      case AST_FUNCTION:
        ssaNode = addSSA_func(ssair, sts, node);
        break;
      case AST_VARIABLE:
        ssaNode = addSSA_var_g(ssair, sts, node);
        break;
      case AST_ASSIGNMENT:
        ssaNode = addSSA_var_g(ssair, sts, node);
        break;
    }
    if (back) {
      back->next = ssaNode;
      back = back->next;
    } else {
      back = ssaNode;
    }
    node = node->next;
  }
  return ssair;
}

ssa_node* addSSA_func(ssa* ssa, symtabStack* sts, AST* ast) {
  enter_scope(sts);
  ssa_node* ssaNode = malloc(sizeof(ssa_node));
  ssaNode->type = FUNCTION;
  ssaNode->next = NULL;
  // ssaNode->value.f = create_function(const char *name, const int param_n);
  // exit_scope(sts);
  return ssaNode;
}

ssa_node* addSSA_var_g(ssa* ssa, symtabStack* sts, AST* ast) {
  ssa_node* ssaNode = malloc(sizeof(ssa_node));
  ssaNode->type = GLOBAL_VAR;
  ssaNode->next = NULL;
  // ssaNode->value.i = create_instruction(opcode op, int dest, int src1, int src2);
  return ssaNode;
}

ssa_node* addSSA_assign_g(ssa* ssa, symtabStack* sts, AST* ast) {
  ssa_node* ssaNode = malloc(sizeof(ssa_node));
  ssaNode->type = GLOBAL_VAR;
  ssaNode->next = NULL;
  // ssaNode->value.i = create_instruction(opcode op, int dest, int src1, int src2);
  return ssaNode;
}

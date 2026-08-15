#include "../include/codegen.h"
#include <stdio.h>
#include <string.h>

asmf* initAsmfile(const char* fname) {
  asmf* af = malloc(sizeof(asmf));
  af->file = fopen(fname, "w");

  af->size = 1000;
  af->n = 0;
  af->out = malloc(af->size * sizeof(char));

  return af;
}

void freeAsmfile(asmf* af) {
  if (af == NULL) return;
  if (af->file != NULL) fclose(af->file);
  free(af->out);
  free(af);
}

void append_line(asmf* af, const char* line) {
  int len = strlen(line);
  if (af->size - af->n < len + 2) {
    af->size *= 2;
    af->out = realloc(af->out, af->size);
  }
  // strcat(af->out, line);
  memcpy(af->out + af->n, line, len);
  af->n += len;
  if (line[len-1] != '\n') {
    // strcat(af->out, "\n");
    af->out[af->n] = '\n';
    af->n++;
  }
  af->out[af->n] = '\0';
}


void init_stackframe(asmf* af, const size_t size) {
  fprintf(af->file, "pushq %%rbp\n");
  fprintf(af->file, "movq %%rsp, %%rbp\n");
  fprintf(af->file, "subq $%d, %%rsp\n", (int)size);
}

void exit_stackframe(asmf* af) {
  fprintf(af->file, "movq %%rbp, %%rsp\n");
  fprintf(af->file, "popq %%rbp\n");
}

char* inst(const char* instruction, const size_t size) {
  char* instructionSized = malloc(strlen(instruction) + 1);
  strcpy(instructionSized, instruction);
  if (size == 8) strcat(instructionSized, "q");
  else if (size == 4) strcat(instructionSized, "l");
  else if (size == 2) strcat(instructionSized, "w");
  else if (size == 1) strcat(instructionSized, "b");
  return instructionSized;
}

void set_var_value(asmf* af, stEntry* e, void* value) {
  char* instruction = inst("mov", e->size);
  fprintf(af->file, "%s $%d, -%d(%%rbp)\n", instruction, (value) ? *(int*)value : 0, e->address);
  free(instruction);
}

void compile_var(asmf* af, stEntry* e) {
  set_var_value(af, e, NULL);
}

void compile_param(asmf* af, stEntry* e, const int regI) {
  char* instruction = inst("mov", e->size);
  fprintf(af->file, "%s %%%s, -%d(%%rbp)\n", instruction, selReg(regI, e->size), e->address);
  freeReg(regI);
  free(instruction);
}

void compile_global_var(asmf* af, stEntry* e, void* value) {
    fprintf(af->file, "  .globl %s\n", e->name);
    fprintf(af->file, "  .data\n");
    fprintf(af->file, "  .align 4\n");
    fprintf(af->file, "  .type %s, @object\n", e->name);
    fprintf(af->file, "  .size %s, %d\n", e->name, (int)e->size); // 4 for int
    fprintf(af->file, "%s:\n", e->name);
    if (e->type == CHAR) {
      fprintf(af->file, "  .byte %d\n", (value) ? *(int*)value : 0);
    } 
    else if (e->type == STR) {
      fprintf(af->file, "  .ascii \"%s\"\n", (value) ? *(char**)value : "");
    } 
    else {
      fprintf(af->file, "  .long %d\n", (value) ? *(int*)value : 0);
    }
  fprintf(af->file, "\n");
}

// Maybe give return register as param
void compile_expr(asmf* af, symtabStack* sts, AST* expr, int typeSize) {
    fprintf(af->file, "movq $%d, %%rax\n", 0); // Ret value
}

void compile_ret(asmf* af, symtabStack* sts, AST* expr, int typeSize) {

  if (expr) {
    compile_expr(af, sts, expr, typeSize);
  }
  exit_stackframe(af);
  fprintf(af->file, "ret\n");
}

void compile_func_def(asmf* af, symtabStack* sts, stEntry* f) {
  size_t stackframe_size = 2048; // Calculate needed size
  fprintf(af->file, "\n%s:\n", f->name);
  init_stackframe(af, stackframe_size);

  // fprintf(af->file, "movq $%d, %%rax\n", 0); // Ret value
  // exit_stackframe(af);
  // fprintf(af->file, "ret\n");
}



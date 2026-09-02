#include "../include/lc.h"
#include "../include/analyzer.h"
#include "../include/ssair.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

lc* initLC() {
  lc* lepC = malloc(sizeof(lc));
  init_diagnostics(&lepC->diagnostics);
  lepC->lexer = init_lexer(&lepC->diagnostics);
  lepC->parser = init_parser(lepC->lexer);
  lepC->root = NULL;
  return lepC;
}

void freeLC(lc *lc) {
  free_ast(lc->root);
  free_parser(lc->parser);
  free_lexer(lc->lexer);
  free_diagnostics(&lc->diagnostics);
  free(lc);
}

bool lccompile(lc *lc) {
  lc->root = parse(lc->parser);
  if (lc->diagnostics.error_count) return false;

  SemanticResult *semantic_result = analyze_ast(lc->root, &lc->diagnostics);
  if (lc->diagnostics.error_count) {
    free_semantic_result(semantic_result);
    return false;
  }

  ssa *ir = generate_ssair(lc->root, semantic_result, &lc->diagnostics);
  if (ir->valid) print_ssair(stdout, ir);
  free_ssair(ir);
  free_semantic_result(semantic_result);
  return lc->diagnostics.error_count == 0;
}


bool readSrcFile(const char *fileName, char **buffer, int *length) {
  *buffer = NULL;
  *length = 0;
  FILE *f = fopen(fileName, "rb");
  if (!f) return false;

  if (fseek(f, 0, SEEK_END) != 0) goto failure;
  long length_long = ftell(f);
  if (length_long < 0 || fseek(f, 0, SEEK_SET) != 0) goto failure;

  *length = (int)length_long;
  *buffer = malloc((size_t)*length + 1);
  if (*buffer == NULL) goto failure;
  if (fread(*buffer, 1, *length, f) != (size_t)*length) {
    free(*buffer);
    *buffer = NULL;
    *length = 0;
    goto failure;
  }
  (*buffer)[*length] = '\0';
  fclose(f);
  return true;

failure:
  fclose(f);
  return false;
}

int main(int argc, char **argv) {
  const char *file = "test.lep";
  if (argc < 2) {
    printf("Pass .lep file to compile. Usage:\n");
    printf("lc <file.lep>\n");
    // return 1;
  }
  const char *fileName = (argc < 2) ? file : argv[1];

  clock_t begin = clock();

  lc* lc = initLC();
  if (!readSrcFile(fileName, &lc->lexer->src, &lc->lexer->srcLen)) {
    fprintf(stderr, "Could not read '%s'\n", fileName);
    freeLC(lc);
    return 1;
  }

  bool compiled = lccompile(lc);

  clock_t end = clock();
  double time = (double)(end - begin) / CLOCKS_PER_SEC;

  print_diagnostics(stderr, &lc->diagnostics, lc->lexer->src, (size_t)lc->lexer->srcLen);

  printf("Compiled in %f s\n", time);

  freeLC(lc);

  return compiled ? 0 : 1;
}

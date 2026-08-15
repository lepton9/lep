#include "../include/lc.h"
#include "../include/analyzer.h"
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

lc* initLC() {
  lc* lepC = malloc(sizeof(lc));
  lepC->lexer = initLexer();
  lepC->parser = initParser(lepC->lexer);
  lepC->root = NULL;
  return lepC;
}

void freeLC(lc *lc) {
  freeAST(lc->root);
  freeParser(lc->parser);
  freeLexer(lc->lexer);
  free(lc);
}

void lccompile(lc *lc) {
  lex(lc->lexer);
  if (lc->lexer->errors->size != 0) {
    return;
  }
  lc->root = parse(lc->parser);

  checkAST(lc->root);
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

  lccompile(lc);

  clock_t end = clock();
  double time = (double)(end - begin) / CLOCKS_PER_SEC;

  printf("Read %d characters\n", lc->lexer->srcLen);
  // printTokens(lc->lexer);
  printErrors(lc->lexer);
  // printAST(lc->root, 0);
  // print_ast(lc->root, 0);
  // printf("%s\n", lc->lexer->src);

  printf("Compiled in %f s\n", time);

  freeLC(lc);

  return 0;
}

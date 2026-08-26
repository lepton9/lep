#ifndef DIAGNOSTIC_H
#define DIAGNOSTIC_H

#include <stdbool.h>
#include <stddef.h>
#include <stdarg.h>
#include <stdio.h>

#include "array_list.h"
#include "token.h"

typedef enum {
  DIAGNOSTIC_ERROR,
  DIAGNOSTIC_WARNING,
  DIAGNOSTIC_NOTE
} DiagnosticSeverity;

typedef struct {
  DiagnosticSeverity severity;
  const char *phase;
  char *message;
  cLoc location;
  bool has_location;
} Diagnostic;

typedef struct {
  ArrayList items;
  size_t error_count;
} DiagnosticList;

void init_diagnostics(DiagnosticList *diagnostics);
void free_diagnostics(DiagnosticList *diagnostics);
void add_diagnosticv(DiagnosticList *diagnostics, DiagnosticSeverity severity,
                     const char *phase, const cLoc *location,
                     const char *format, va_list args);
void add_diagnostic(DiagnosticList *diagnostics, DiagnosticSeverity severity,
                    const char *phase, const cLoc *location,
                    const char *format, ...);
void print_diagnostics(FILE *out, const DiagnosticList *diagnostics,
                       const char *source, size_t source_length);

#endif

#include "../include/diagnostic.h"

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

void init_diagnostics(DiagnosticList *diagnostics) {
  *diagnostics = (DiagnosticList){0};
  array_list_init(&diagnostics->items, sizeof(Diagnostic));
}

void free_diagnostics(DiagnosticList *diagnostics) {
  for (size_t index = 0; index < diagnostics->items.count; index++) {
    Diagnostic *diagnostic = array_list_get(&diagnostics->items, index);
    free(diagnostic->message);
  }
  array_list_free(&diagnostics->items);
  *diagnostics = (DiagnosticList){0};
}

void add_diagnosticv(DiagnosticList *diagnostics, DiagnosticSeverity severity,
                     const char *phase, const cLoc *location,
                     const char *format, va_list args) {
  va_list copy;
  va_copy(copy, args);
  int length = vsnprintf(NULL, 0, format, copy);
  va_end(copy);
  if (length < 0) return;
  char *message = malloc((size_t)length + 1);
  if (!message) return;
  va_copy(copy, args);
  vsnprintf(message, (size_t)length + 1, format, copy);
  va_end(copy);

  Diagnostic diagnostic = {
      .severity = severity,
      .phase = phase,
      .message = message,
      .has_location = location != NULL,
  };
  if (location) diagnostic.location = *location;
  if (!array_list_push(&diagnostics->items, &diagnostic)) {
    free(message);
    return;
  }
  if (severity == DIAGNOSTIC_ERROR) diagnostics->error_count++;
}

void add_diagnostic(DiagnosticList *diagnostics, DiagnosticSeverity severity,
                    const char *phase, const cLoc *location,
                    const char *format, ...) {
  va_list args;
  va_start(args, format);
  add_diagnosticv(diagnostics, severity, phase, location, format, args);
  va_end(args);
}

static const char *severity_name(DiagnosticSeverity severity) {
  switch (severity) {
    case DIAGNOSTIC_ERROR: return "error";
    case DIAGNOSTIC_WARNING: return "warning";
    case DIAGNOSTIC_NOTE: return "note";
  }
  return "diagnostic";
}

static void print_source_line(FILE *out, const char *source, size_t source_length, cLoc location) {
  if (!source || location.line < 1 || location.column < 1) return;
  size_t start = 0;
  int line = 1;
  while (start < source_length && line < location.line) {
    if (source[start++] == '\n') line++;
  }
  if (line != location.line) return;
  size_t end = start;
  while (end < source_length && source[end] != '\n') end++;
  fprintf(out, "  |\n%d | %.*s\n  | ", location.line, (int)(end - start), source + start);
  for (int column = 1; column < location.column; column++) fputc(' ', out);
  fputc('^', out);
  fputc('\n', out);
}

void print_diagnostics(FILE *out, const DiagnosticList *diagnostics,
                       const char *source, size_t source_length) {
  for (size_t index = 0; index < diagnostics->items.count; index++) {
    const Diagnostic *diagnostic = array_list_get_const(&diagnostics->items, index);
    fprintf(out, "%s[%s]: %s", severity_name(diagnostic->severity),
            diagnostic->phase, diagnostic->message);
    if (diagnostic->has_location) {
      fprintf(out, "\n --> %d:%d\n", diagnostic->location.line, diagnostic->location.column);
      print_source_line(out, source, source_length, diagnostic->location);
    } else {
      fputc('\n', out);
    }
  }
  if (diagnostics->error_count) {
    fprintf(out, "Compilation failed with %zu error%s.\n", diagnostics->error_count,
            diagnostics->error_count == 1 ? "" : "s");
  }
}

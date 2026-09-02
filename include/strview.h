#ifndef STRVIEW_H
#define STRVIEW_H

#include <string.h>

typedef struct {
  const char *start;
  size_t length;
} strview;

static inline strview strview_from_cstr(const char *value) {
  return (strview){.start = value, .length = strlen(value)};
}

static inline bool strview_eq_cstr(strview view, const char *value) {
  return strlen(value) == view.length && memcmp(view.start, value, view.length) == 0;
}

static inline char *strview_strdup(strview view) {
  return strndup(view.start, view.length);
}

#endif

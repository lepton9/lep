#ifndef ARRAY_LIST_H
#define ARRAY_LIST_H

#include <stdbool.h>
#include <stddef.h>

typedef struct {
  void *data;
  size_t count;
  size_t capacity;
  size_t item_size;
} ArrayList;

bool array_list_init(ArrayList *list, size_t item_size);
void array_list_free(ArrayList *list);

bool array_list_reserve(ArrayList *list, size_t capacity);
bool array_list_push(ArrayList *list, const void *item);
void *array_list_get(ArrayList *list, size_t index);
const void *array_list_get_const(const ArrayList *list, size_t index);

bool array_list_pop(ArrayList *list, void *out_item);
bool array_list_remove(ArrayList *list, size_t index, void *out_item);
bool array_list_swap(ArrayList *list, size_t first, size_t second);
bool array_list_shrink_to_fit(ArrayList *list);

#endif

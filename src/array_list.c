#include "../include/array_list.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static bool can_allocate(const ArrayList *list, size_t capacity) {
  return capacity <= SIZE_MAX / list->item_size;
}

bool array_list_init(ArrayList *list, size_t item_size) {
  if (!list || item_size == 0) return false;
  *list = (ArrayList){.item_size = item_size};
  return true;
}

void array_list_free(ArrayList *list) {
  if (!list) return;
  free(list->data);
  *list = (ArrayList){0};
}

bool array_list_reserve(ArrayList *list, size_t capacity) {
  if (!list || list->item_size == 0) return false;
  if (capacity <= list->capacity) return true;
  if (!can_allocate(list, capacity)) return false;
  void *data = realloc(list->data, capacity * list->item_size);
  if (!data) return false;
  list->data = data;
  list->capacity = capacity;
  return true;
}

bool array_list_push(ArrayList *list, const void *item) {
  if (!list || !item) return false;
  if (list->count == list->capacity) {
    size_t capacity = list->capacity ? list->capacity * 2 : 8;
    if (capacity < list->capacity || !array_list_reserve(list, capacity)) return false;
  }
  memcpy((char *)list->data + list->count * list->item_size, item, list->item_size);
  list->count++;
  return true;
}

void *array_list_get(ArrayList *list, size_t index) {
  if (!list || index >= list->count) return NULL;
  return (char *)list->data + index * list->item_size;
}

const void *array_list_get_const(const ArrayList *list, size_t index) {
  if (!list || index >= list->count) return NULL;
  return (const char *)list->data + index * list->item_size;
}

bool array_list_remove(ArrayList *list, size_t index, void *out_item) {
  void *item = array_list_get(list, index);
  if (!item) return false;
  if (out_item) memcpy(out_item, item, list->item_size);
  if (index + 1 < list->count) {
    memmove(item, (char *)item + list->item_size,
            (list->count - index - 1) * list->item_size);
  }
  list->count--;
  return true;
}

bool array_list_pop(ArrayList *list, void *out_item) {
  if (!list || list->count == 0) return false;
  return array_list_remove(list, list->count - 1, out_item);
}

bool array_list_swap(ArrayList *list, size_t first, size_t second) {
  if (!list || first >= list->count || second >= list->count) return false;
  if (first == second) return true;
  void *temporary = malloc(list->item_size);
  if (!temporary) return false;
  void *first_item = array_list_get(list, first);
  void *second_item = array_list_get(list, second);
  memcpy(temporary, first_item, list->item_size);
  memcpy(first_item, second_item, list->item_size);
  memcpy(second_item, temporary, list->item_size);
  free(temporary);
  return true;
}

bool array_list_shrink_to_fit(ArrayList *list) {
  if (!list) return false;
  if (list->count == list->capacity) return true;
  if (list->count == 0) {
    free(list->data);
    list->data = NULL;
    list->capacity = 0;
    return true;
  }
  void *data = realloc(list->data, list->count * list->item_size);
  if (!data) return false;
  list->data = data;
  list->capacity = list->count;
  return true;
}

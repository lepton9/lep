#include "../include/hashtab.h"
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


ht_e* init_item(const char* key, void* value) {
  ht_e* item = malloc(sizeof(ht_e));
  item->key = malloc(strlen(key) + 1);
  item->value = value;
  strcpy(item->key, key);
  return item;
}

void free_item(ht_e* item) {
  free(item->key);
  free(item);
}

hashtab* init_hashtab() {
  return init_hashtabn(INIT_SIZE);
}

hashtab* init_hashtabn(const size_t size) {
  hashtab* ht = malloc(sizeof(hashtab));
  ht->size = size;
  ht->n = 0;
  ht->items = calloc(ht->size, sizeof(ht_e*));
  return ht;
}

void free_hashtab(hashtab* ht) {
  for (size_t i = 0; i < ht->size; i++) {
    if (ht->items[i]) free_item(ht->items[i]);
  }
  free(ht->items);
  free(ht);
}

// djb2
static uint64_t hash_key(const char* key) {
  uint64_t hash = 5381;
  int c;
  while ((c = *key++)) {
    hash = ((hash << 5) + hash) + c; /* hash * 33 + c */
  }
  return hash;
}

static size_t key_index(const char* key, const size_t size) {
  return hash_key(key) % size;
}

static ht_e* ht_set(ht_e** items, const size_t size, size_t* n, const char* key, void* value) {
  size_t ind = key_index(key, size);
  while(items[ind] != NULL) {
    if (strcmp(key, items[ind]->key) == 0) {
      items[ind]->value = value;
      return items[ind];
    }
    // Linear probing
    ind = (ind + 1) % size;
  }
  items[ind] = init_item(key, value);
  (*n)++;
  return items[ind];
}

static bool ht_expand(hashtab* ht) {
  size_t new_size = ht->size * 2;
  if (new_size < ht->size) return false;
  ht_e** new_items = calloc(new_size, sizeof(ht_e*));
  if (new_items == NULL) return false;
  ht->n = 0;

  for (size_t i = 0; i < ht->size; i++) {
    ht_e* item = ht->items[i];
    if (item != NULL) {
      ht_set(new_items, new_size, &ht->n, item->key, item->value);
    }
  }
  free(ht->items);
  ht->items = new_items;
  ht->size = new_size;
  return true;
}

ht_e* ht_insert(hashtab* ht, const char* key, void* value) {
  if (ht->n >= ht->size / 2) {
    if (!ht_expand(ht)) return NULL;
  }
  return ht_set(ht->items, ht->size, &ht->n, key, value);
}

ht_e* ht_lookup(hashtab* ht, const char* key) {
  size_t ind = key_index(key, ht->size);
  while(ht->items[ind] != NULL) {
    if (strcmp(key, ht->items[ind]->key) == 0) {
      return ht->items[ind];
    }
    ind++;
    if (ind >= ht->size) ind = 0;
  }
  return NULL;
}

void* ht_get(hashtab* ht, const char* key) {
  ht_e* e = ht_lookup(ht, key);
  return (e) ? e->value : NULL;
}

bool ht_delete(hashtab* ht, const char* key) {
  size_t ind = key_index(key, ht->size);
  while(ht->items[ind] != NULL) {
    if (strcmp(key, ht->items[ind]->key) == 0) {
      free_item(ht->items[ind]);
      ht->items[ind] = NULL;
      ht->n--;
      return true;
    }
    ind = (ind + 1) % ht->size;
  }
  return false;
}

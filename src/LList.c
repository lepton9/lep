#include "../include/LList.h"
#include <stdlib.h>

LList *create_list() {
  LList *new_list = (LList *)malloc(sizeof(LList));
  new_list->size = 0;
  new_list->head = NULL;
  return new_list;
}

int get_size(LList *list) {
  int c = 0;
  node *cur = list->head;
  while (cur != NULL) {
    cur = cur->next;
    c++;
  }
  return c;
}

node *copy_nodes(node *head) {
  if (head == NULL) {
    return NULL;
  } else {
    node *n = (node *)malloc(sizeof(node));
    n->data = head->data;
    n->next = copy_nodes(head->next);
    return n;
  }
}

void copy_list(LList *listTo, LList *listFrom) {
  listTo->head = copy_nodes(listFrom->head);
  listTo->size = get_size(listTo);
}

void add_to_begin(LList *list, void *data) {
  node *n = (node *)malloc(sizeof(node));
  n->data = data;
  n->next = list->head;
  list->head = n;
  list->size++;
}

void add_to_end(LList *list, void *data) {
  if (list->head == NULL) {
    list->head = (node *)malloc(sizeof(node));
    list->head->data = data;
    list->head->next = NULL;
  } else {
    node *cur = list->head;
    while (cur->next != NULL) {
      cur = cur->next;
    }
    cur->next = (node *)malloc(sizeof(node));
    cur->next->data = data;
    cur->next->next = NULL;
  }
  list->size++;
}

void *pop_front(LList *list) {
  if (list->head == NULL) {
    return NULL;
  }
  void *ret = NULL;
  node *new_head;
  new_head = list->head->next;
  ret = list->head->data;
  free(list->head);
  list->head = new_head;
  list->size--;
  return ret;
}

void *pop_last(LList *list) {
  if (list->head == NULL) return NULL;

  if (list->head->next == NULL) {
    void *ret = list->head->data;
    free(list->head);
    list->head = NULL;
    list->size--;
    return ret;
  }
  node *cur = list->head;
  while (cur->next->next != NULL) {
    cur = cur->next;
  }
  void *ret = cur->next->data;
  free(cur->next);
  cur->next = NULL;
  list->size--;
  return ret;
}

int is_empty(LList *list) { return (list->size == 0); }

int list_size(LList *list) { return list->size; }

void list_clear(LList *list) {
  while (!is_empty(list)) {
    pop_front(list);
  }
}

#ifndef __BAFS_NVME_LIST_H__
#define __BAFS_NVME_LIST_H__

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/compiler.h>


struct list;

struct list_node {
  struct list*      list;
  struct list_node* next;
  struct list_node* prev;
};

struct list {
  struct list_node head;
  spinlock_t       lock;
};


static void __always_inline list_node_init(struct list_node* e) {
  e->list = NULL;
  e->next = NULL;
  e->prev = NULL;
}


static struct list_node* __always_inline list_next(struct list_node* e) {
  if ((e->next) != (&e->list->head))
    return e->next;
  else
    return NULL;
}


void list_init(struct list* l);

void list_insert(struct list* l, struct list_node* e);

void list_remove(struct list_node* e);


#endif

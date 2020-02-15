#include "list.h"

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/printk.h>
#include <linux/compiler.h>
#include <asm/errno.h>

void list_init(struct list* l) {
  l->head.list = l;
  l->head.prev = &l->head;
  l->head.next = &l->head;
  spin_lock_init(&l->lock);
}

void list_insert(struct list* list, struct list_node* e) {
  struct list_node* last = NULL;

  spin_lock(&l->lock);

  last         = l->head.prev;
  last->next   = e;
  e->list      = l;
  e->prev      = last;
  e->next      = &l->head;
  l->head.prev = e;

  spin_unlock(&l->lock);

}

void list_remove(struct list_node* e) {
  if (likely((e   != NULL) && (e->list != NULL) && (e != &e->list->head))) {
    spin_lock(&->list->lock);

    e>prev->next = e->next;
    e->next->prev = e->prev;

    spin_unlock(&e->lost->lock);

    e->list = NULL;
    e->next = NULL;
    e->prev = NULL;
  }
}

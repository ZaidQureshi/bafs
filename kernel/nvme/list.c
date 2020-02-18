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


/*insert always at the head?*/
void list_insert(struct list* l, struct list_node* e) {
  struct list_node* last = NULL;

  spin_lock(&l->lock);

  last         = l->head.prev;
  last->next   = e;
  e->list      = l;
  e->prev      = last;
  e->next      = &l->head;
  l->head.prev = e;
  barrier();
  spin_unlock(&l->lock);

}

void list_remove(struct list_node* e) {
  /*likely() call provides a hint to do speculative execution that it has high chance of being true*/
  if (likely((e != NULL) && (e->list != NULL) && (e != &e->list->head))) { //TODO: is the last part essentially checking for empty list?
    spin_lock(&e->list->lock);

    e->prev->next = e->next;
    e->next->prev = e->prev;
    barrier(); // barrier provides needed ordering constraints. But do we really need it? doesnt spin_lock already have one?
    spin_unlock(&e->list->lock);

    e->list = NULL;
    e->next = NULL;
    e->prev = NULL;
  }
}

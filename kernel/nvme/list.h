#ifndef __BAFS_NVME_LIST_H__
#define __BAFS_NVME_LIST_H__

#include <linux/types.h>
#include <linux/spinlock.h>
#include <linux/compiler.h>

/*making compiler happy - forward declaration*/
struct list;

/*double ptr list*/
struct list_node {
  struct list*      list;  // ref to the list
  struct list_node* next;  // ptr to next element in the list
  struct list_node *prev;  // ptr to previous element in the list
};


/*Double linked list and expect the head is always empty*/
struct list {
  struct list_node head;    // start of list
  spinlock_t       lock;    // exclusive access purpose
};

/*the list_node is init to NULL*/
void list_node_init(struct list_node* e);

/*get next element if list is not empty*/
struct list_node* list_next(const struct list_node* e);

/*Func calls for init, insert and removal from the list*/

void list_init(struct list* l);

void list_insert(struct list* l, struct list_node* e);

void list_remove(struct list_node* e);

#endif /*__BAFS_NVME_LIST_H__*/

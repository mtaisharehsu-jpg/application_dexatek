//============================================================================
// File: list.h
// Author: Charles Tsai
//
// This is proprietary information of Dexatek Technology Ltd.
// All Rights Reserved. Reproduction of this documentation or the
// accompanying programs in any manner whatsoever without the written
// permission of Dexatek Technology Ltd. is strictly forbidden.
//============================================================================

#ifndef __KERNEL_LIST_H__
#define __KERNEL_LIST_H__

#ifndef __LIST_H

#if defined ( __CC_ARM   )
#ifndef inline
#define inline			__inline
#endif
#endif


/* this file is from Linux Kernel (include/linux/list.h)
 * and modified by simply removing hardware prefetching of list items.
 * Here by copyright, credits attributed to wherever they belong.
 * Kulesh Shanmugasundaram (kulesh [squiggly] isis.poly.edu)
 */

/*
 * simple doubly linked list implementation.
 *
 * some of the internal functions ("__xxx") are useful when
 * manipulating whole lists rather than single entries, as
 * sometimes we already know the next/prev entries and we can
 * generate better code by using them directly rather than
 * using the generic single-entry routines.
 */


/**
 * @brief The list_head struct
 */
struct list_head {
    struct list_head* next;
    struct list_head* prev;
};


#define LIST_HEAD_INIT(name) { &(name), &(name) }

#define LIST_HEAD(name)                                         \
    struct list_head name = LIST_HEAD_INIT(name)

#define INIT_LIST_HEAD(ptr)                                     \
    do {                                                        \
	    (ptr)->next = (ptr); (ptr)->prev = (ptr);               \
    } while (0)


/*
 * insert a new entry between two known consecutive entries.
 *
 * this is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_add (
    struct list_head* list,
    struct list_head* prev,
    struct list_head* next
) {
    next->prev = list;
    list->next = next;
    list->prev = prev;
    prev->next = list;
}


/**
 * list_add - add a new entry
 *
 * @param new new entry to be added
 * @param head list head to add it after
 *
 * insert a new entry after the specified head.
 * this is good for implementing stacks.
 */
static inline void list_add (
    struct list_head* list,
    struct list_head* head
) {
    __list_add (list, head, head->next);
}


/**
 * list_add_tail - add a new entry
 * @param new new entry to be added
 * @param head list head to add it before
 *
 * insert a new entry before the specified head.
 * this is useful for implementing queues.
 */
static inline void list_add_tail (
    struct list_head* list,
    struct list_head* head
) {
    __list_add (list, head->prev, head);
}


/*
 * delete a list entry by making the prev/next entries
 * point to each other.
 *
 * this is only for internal list manipulation where we know
 * the prev/next entries already!
 */
static inline void __list_del (
    struct list_head* prev,
    struct list_head* next
) {
    next->prev = prev;
    prev->next = next;
}


/**
 * list_del - deletes list from head.
 *
 * @param entry the element to delete from the list.
 * note: list_empty on entry does not return true after this, the entry is in an undefined state.
 */
static inline void list_del (
    struct list_head* list
) {
    __list_del (list->prev, list->next);
}


/**
 * list_del_init - deletes entry from list and reinitialize it.
 *
 * @param entry the element to delete from the list.
 */
static inline void list_del_initi (struct list_head* entry) {
    __list_del (entry->prev, entry->next);
    INIT_LIST_HEAD (entry);
}


/**
 * list_move - delete from one list and add as another's head
 *
 * @param list the entry to move
 * @param head the head that will precede our entry
 */
static inline void list_move (
    struct list_head* list,
    struct list_head* head
) {
    __list_del (list->prev, list->next);
    list_add (list, head);
}


/**
 * list_move_tail - delete from one list and add as another's tail
 *
 * @param list the entry to move
 * @param head the head that will follow our entry
 */
static inline void list_move_tail (
    struct list_head* list,
    struct list_head* head
) {
    __list_del (list->prev, list->next);
    list_add_tail (list, head);
}


/**
 * list_empty - tests whether a list is empty
 * @param head the list to test.
 */
static inline int list_empty (
    struct list_head* head
) {
    return head->next == head;
}


/**
 *
 */
static inline void __list_splice (
    struct list_head* list,
    struct list_head* head
) {
    struct list_head* first = list->next;
    struct list_head* last = list->prev;
    struct list_head* at = head->next;

    first->prev = head;
    head->next = first;

    last->next = at;
    at->prev = last;
}


/**
 * list_splice - join two lists
 *
 * @param list the new list to add.
 * @param head the place to add it in the first list.
 */
static inline void list_splice (
    struct list_head* list,
    struct list_head* head
) {
    if (!list_empty(list)) {
        __list_splice(list, head);
    }
}


/**
 * list_splice_init - join two lists and reinitialise the emptied list.
 *
 * @param list the new list to add.
 * @param head the place to add it in the first list.
 *
 * The list at list is reinitialised
 */
static inline void list_splice_init (
    struct list_head* list,
    struct list_head* head
) {
    if (!list_empty(list)) {
        __list_splice(list, head);
        INIT_LIST_HEAD(list);
    }
}


/**
 * list_entry - get the struct for this entry
 *
 * @param ptr the &struct list_head pointer.
 * @param type the type of the struct this is embedded in.
 * @param member the name of the list_struct within the struct.
 */
#define list_entry(ptr, type, member)                           \
	((type *)((char *)(ptr)-(unsigned long)(&((type *)0)->member)))


/**
 * list_first_entry - get the first element from a list
 *
 * @param ptr the list head to take the element from.
 * @param type the type of the struct this is embedded in.
 * @param member the name of the list_head within the struct.
 *
 * note, that list is expected to be not empty.
 */
#define list_first_entry(ptr, type, member)                     \
        list_entry((ptr)->next, type, member)


/**
 * list_for_each	-	iterate over a list
 *
 * @param pos the &struct list_head to use as a loop counter.
 * @param head the head for your list.
 */
#define list_for_each(pos, head)                                \
	for (pos = (head)->next; pos != (head);                     \
        	pos = pos->next)


/**
 * list_for_each_prev	-	iterate over a list backwards
 *
 * @param pos the &struct list_head to use as a loop counter.
 * @param head the head for your list.
 */
#define list_for_each_prev(pos, head)                           \
	for (pos = (head)->prev; pos != (head);                     \
        	pos = pos->prev)


/**
 * list_for_each_safe	-	iterate over a list safe against removal of list entry
 *
 * @param pos the &struct list_head to use as a loop counter.
 * @param n another &struct list_head to use as temporary storage
 * @param head the head for your list.
 */
#define list_for_each_safe(pos, n, head)                        \
	for (pos = (head)->next, n = pos->next; pos != (head);      \
		pos = n, n = pos->next)


/**
 * list_for_each_entry	-	iterate over list of given type
 *
 * @param pos the type * to use as a loop counter.
 * @param head the head for your list.
 * @param member the name of the list_struct within the struct.
 */
#define list_for_each_entry(pos, head, member, type)            \
	for (pos = list_entry((head)->next, type, member);	        \
	     &pos->member != (head); 					            \
	     pos = list_entry(pos->member.next, type, member))


/**
 * list_for_each_entry_safe - iterate over list of given type safe against removal of list entry
 *
 * @param pos the type * to use as a loop counter.
 * @param n another type * to use as temporary storage
 * @param head the head for your list.
 * @param member the name of the list_struct within the struct.
 */
#define list_for_each_entry_safe(pos, n, head, member, type)    \
	for (pos = list_entry((head)->next, type, member),	        \
		n = list_entry(pos->member.next, type, member); 	    \
	     &pos->member != (head); 					            \
	     pos = n, n = list_entry(n->member.next, type, member))


/**
 *
 */
#define get_first_item(attached, type, member) \
    ((type *)((char *)((attached)->next)-(unsigned long)(&((type *)0)->member)))


#endif

#endif

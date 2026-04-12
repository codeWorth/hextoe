#ifndef __BOT_UTIL__
#define __BOT_UTIL__

#include <assert.h>
#include <stddef.h>
#include "arc.h"

#ifdef DEBUG
#define DEBUG_ASSERT(cond)	assert(cond)
#else
#define DEBUG_ASSERT(cond)
#endif

/*
 * FNV hash magic constants
 */

#define FNV_INIT 	((uint64_t) 0xcbf29ce484222325ULL)
#define FNV_PRIME	((uint64_t) 0x100000001b3ULL)

#define HASH_FN(key)	splitmix64(key)

#define MASK_30		0x3FFFFFFFULL
#define MASK_61		0x1FFFFFFFFFFFFFFF

#define	DLL_INIT(h, type, next, prev) {			\
	(h)->next = (type *)(h);			\
	(h)->prev = (type *)(h);			\
}

#define	DLL_ADD2HEAD(h, e, type, next, prev) {		\
	(e)->next = (h)->next;				\
	(e)->prev = (type *) (h);			\
	(h)->next->prev = (e);				\
	(h)->next = (e);				\
}

#define DLL_PREPEND(new_e, e, type, next, prev) {	\
	(new_e)->prev = (e)->prev;			\
	(new_e)->prev->next = (new_e);			\
	(new_e)->next = (e);				\
	(e)->prev = (new_e);				\
}

#define	DLL_DELETE(e, next, prev) {			\
	(e)->prev->next = (e)->next;			\
	(e)->next->prev = (e)->prev;			\
	(e)->prev = (e)->next = NULL;			\
}

#define IS_FLAG_SET(cont, flag)		(((cont) & (flag)) != 0)
#define SET_FLAG(cont, flag)		((cont) |= (flag))
#define RESET_FLAG(cont, flag)		((cont) &= ~(flag))
#define PUT_FLAG(cont, flag, v) 		\
	((v) ? ((cont) |= flag) : ((cont) &= ~flag))

#define MAP_NEW_ENTRY(map, buckets_f, stack_f, sz_f, hash, index_func,	\
		      entry_type, next, prev) ({			\
	uint32_t	index;						\
	entry_type	*head, *entry;					\
									\
	index = index_func((hash));					\
	entry = &(map)->stack_f[(map)->sz_f];				\
	(map)->sz_f++;							\
									\
	head = &(map)->buckets_f[index];				\
	DLL_ADD2HEAD(head, entry, entry_type, next, prev);		\
	entry;								\
})

#define MAP_INSERT_ENTRY(map, entry, stack_f, sz_f, entry_type, next,	\
			 prev) ({					\
	entry_type	*new_entry;					\
									\
	new_entry = &(map)->stack_f[(map)->sz_f];			\
	(map)->sz_f++;							\
									\
	DLL_PREPEND(new_entry, entry, entry_type, next, prev);		\
	new_entry;							\
})

#define MAP_GET(map, buckets_f, key, index_func, entry_type, next,	\
		cond) ({						\
	uint32_t	index;						\
	entry_type	*head, *node, *res = NULL;			\
									\
	index = index_func((key));					\
	head = &(map)->buckets_f[index];				\
	node = head->next;						\
	while(node != head) {						\
		if(cond) {						\
			res = node;					\
			break;						\
		}							\
		node = node->next;					\
	}								\
	res;								\
})

extern uint64_t splitmix64(uint64_t);
extern uint64_t fnv_hash(uint64_t);

extern uint64_t encode_arc(arc_t *);
extern void decode_arc(uint64_t, arc_t *);

#endif
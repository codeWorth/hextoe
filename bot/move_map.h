#ifndef __MOVE_MAP__
#define __MOVE_MAP__

#include "bot_util.h"

#define MM_BUCKETS	512
#define MM_MASK		(MM_BUCKETS - 1)
#define MM_STACK_SIZE	2048
#define MME_INDEX(key)	(HASH_FN(key) & MM_MASK)

typedef uint64_t	mm_key;

typedef struct mm_entry {
	struct mm_entry	*mme_next;
	struct mm_entry	*mme_prev;
	mm_key		mme_key;
	int		mme_value;
} mm_entry_t;

typedef struct move_map {
	mm_entry_t	mm_buckets[MM_BUCKETS];
	mm_entry_t	mm_stack[MM_STACK_SIZE];
	uint32_t	mm_stack_size;
} move_map_t;

extern mm_entry_t * mm_entries_qselect(mm_entry_t *, int, int);

extern mm_key mm_arc2key(arc_t *);
extern void mm_decode_key(mm_key, arc_t *);

extern void mm_init(move_map_t *);
extern mm_entry_t * mm_insert(move_map_t *, mm_key, int);
extern mm_entry_t * mm_get(move_map_t *, mm_key);
extern void mm_remove(move_map_t *, mm_key);

#endif
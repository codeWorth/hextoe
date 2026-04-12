#ifndef __TILE_MAP__
#define __TILE_MAP__

#include "bot_util.h"

#define TM_BUCKETS	512
#define TM_MASK		(TM_BUCKETS - 1)
#define TM_STACK_SIZE	2048
#define TME_INDEX(hash)	(hash & TM_MASK)

#define TME_P1_VAL	0x0001
#define	TME_A_VAL	0x0002

#define TME_GET_A(tme)					\
	IS_FLAG_SET((tme)->tme_flags, TME_A_VAL)

#define TME_IS_P1(tme)					\
	IS_FLAG_SET((tme)->tme_flags, TME_P1_VAL)

#define TME_A_MATCH(e1, e2)				\
	(((e1)->tme_flags & TME_A_VAL) == ((e2)->tme_flags & TME_A_VAL))

#define TME_LOCATION_MATCH(e1, e2)					\
	((e1)->tme_hash == (e2)->tme_hash && TME_A_MATCH(e1, e2) &&	\
	 (e1)->tme_r == (e2)->tme_r && (e1)->tme_c == (e2)->tme_c)

typedef uint32_t	tm_hash;

typedef struct tm_entry {
	struct tm_entry	*tme_next;
	struct tm_entry	*tme_prev;
	tm_hash		tme_hash;
	int32_t		tme_r;
	int32_t		tme_c;
	uint16_t	tme_flags;
} tm_entry_t;

typedef struct tile_map {
	tm_entry_t	tm_buckets[TM_BUCKETS];
	tm_entry_t	tm_stack[TM_STACK_SIZE];
	uint32_t	tm_stack_size;
} tile_map_t;

extern tm_hash tm_hash_arc(arc_t *);

extern void tm_init(tile_map_t *);
extern tm_entry_t * tm_insert(tile_map_t *, arc_t *, bool);
extern tm_entry_t * tm_get(tile_map_t *, arc_t *);
extern tm_entry_t * tm_get_hash(tile_map_t *, tm_hash, arc_t *);
extern void tm_remove_entry(tile_map_t *, tm_entry_t *);
extern void tm_remove(tile_map_t *, arc_t *);
extern void tm_remove_hash(tile_map_t *, tm_hash, arc_t *);

#endif
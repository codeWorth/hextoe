#ifndef __MOVE_MAP__
#define __MOVE_MAP__

#include "bot_util.h"

#define MM_BUCKETS	512
#define MM_MASK		(MM_BUCKETS - 1)
#define MM_STACK_SIZE	4096
#define MME_INDEX(hash)	(hash & MM_MASK)

#define MME_SKIPPED	0x0001
#define	MME_A_VAL	0x0002

#define MME_IS_SKIPPED(mme)				\
	IS_FLAG_SET((mme)->mme_flags, MME_SKIPPED)

#define MME_GET_A(mme)					\
	IS_FLAG_SET((mme)->mme_flags, MME_A_VAL)

#define MME_A_MATCH(e1, e2)				\
	(((e1)->mme_flags & MME_A_VAL) == ((e2)->mme_flags & MME_A_VAL))

#define MME_LOCATION_MATCH(e1, e2)					\
	((e1)->mme_hash == (e2)->mme_hash && MME_A_MATCH(e1, e2) &&	\
	 (e1)->mme_r == (e2)->mme_r && (e1)->mme_c == (e2)->mme_c)

#define MME_IMPACT(mme) ({				\
	uint32_t	impact = 0;			\
							\
	impact += (mme)->mme_score.s0_p1;		\
	impact += (mme)->mme_score.s60_p1;		\
	impact += (mme)->mme_score.s120_p1;		\
	impact += (mme)->mme_score.s0_p2;		\
	impact += (mme)->mme_score.s60_p2;		\
	impact += (mme)->mme_score.s120_p2;		\
	impact;						\
})

#define MME_POPULATE_ARC(arc, mme) ({	\
					\
	(arc).a = MME_GET_A((mme));	\
	(arc).r = (mme)->mme_r;		\
	(arc).c = (mme)->mme_c;		\
})

typedef uint32_t	mm_hash;

typedef struct mm_score {
	uint32_t	s0_p1;
	uint32_t	s60_p1;
	uint32_t	s120_p1;
	uint32_t	s0_p2;
	uint32_t	s60_p2;
	uint32_t	s120_p2;
} mm_score_t;

typedef struct mm_entry {
	struct mm_entry	*mme_next;
	struct mm_entry	*mme_prev;
	mm_hash		mme_hash;
	int32_t		mme_r;
	int32_t		mme_c;
	mm_score_t	mme_score;
	uint16_t	mme_flags;
} mm_entry_t;

typedef struct move_map {
	mm_entry_t	mm_buckets[MM_BUCKETS];
	mm_entry_t	mm_stack[MM_STACK_SIZE];
	uint32_t	mm_stack_size;
} move_map_t;


extern mm_hash mm_hash_arc(arc_t *);
extern void mm_init(move_map_t *);

extern mm_entry_t * mm_insert(move_map_t *, arc_t *, mm_score_t *);
extern mm_entry_t * mm_overwrite(move_map_t *, mm_entry_t *, mm_score_t *);

extern mm_entry_t * mm_get(move_map_t *, arc_t *);
extern mm_entry_t * mm_get_hash(move_map_t *, mm_hash, arc_t *);

extern void mm_remove_entry(move_map_t *, mm_entry_t *);
extern void mm_remove(move_map_t *, arc_t *);
extern void mm_remove_hash(move_map_t *, mm_hash, arc_t *);
extern void mm_remove_to_length(move_map_t *, uint32_t);

#endif
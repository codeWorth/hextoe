#ifndef __MOVE_HEAP__
#define __MOVE_HEAP__

#include "move_map.h"

#define PUSH_STACK(ml, move, score) ({				\
	DEBUG_ASSERT((ml)->ml_len < (ml)->ml_size);		\
	(ml)->ml_moves[((ml)->ml_len)].mle_mm_entry = (move);	\
	(ml)->ml_moves[((ml)->ml_len)].mle_score = (score);	\
	(ml)->ml_len++;						\
})

#define INIT_STACK(ml, arr, size) ({	\
	(ml)->ml_moves = (arr);		\
	(ml)->ml_size = (size);		\
	(ml)->ml_len = 0;		\
})

typedef struct move_entry {
	mm_entry_t	*mle_mm_entry;
	uint32_t	mle_score;
} move_entry_t;

typedef struct move_list {
	move_entry_t	*ml_moves;
	int		ml_len;
	int		ml_size;
} move_list_t;

extern void mh_push(move_list_t *, mm_entry_t *, uint32_t);
extern mm_entry_t * mh_pop(move_list_t *);

#endif
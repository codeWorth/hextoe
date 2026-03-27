#ifndef __LINE_MAP__
#define __LINE_MAP__

#include "bot_util.h"
#include "move_map.h"

typedef uint64_t	lm_key;

#define LM_BUCKETS	512
#define LM_MASK		(LM_BUCKETS - 1)
#define LM_STACK_SIZE	2048
#define LME_INDEX(key)	(HASH_FN(key) & LM_MASK)

#define LM_SKIPPED		0b00010000

#define LME_IS_SKIPPED(lme)			\
	(IS_FLAG_SET((lme)->lme_data, LM_SKIPPED))

#define LME_GET_LENGTH(lme)			\
	((lme)->lme_data & 0xF)

#define PUSH_LINE_MOD(line_mods, sz, index, skipped_below) ({	\
	(line_mods)[sz].me_stack_index = (index);		\
	(line_mods)[sz].me_skipped_below = (skipped_below);	\
	(sz)++;							\
})

typedef struct lm_entry {
	struct lm_entry	*lme_next;
	struct lm_entry	*lme_prev;
	lm_key		lme_key;
	int16_t		lme_score;
	uint8_t		lme_data;	// length(low 4), flags(high 4)
} lm_entry_t;

typedef struct line_map {
	lm_entry_t	lm_buckets[LM_BUCKETS];
	lm_entry_t	lm_stack[LM_STACK_SIZE];
	uint32_t	lm_stack_size;
} line_map_t;

typedef struct mod_entry {
	uint32_t	me_stack_index;
	uint8_t		me_skipped_below;
} mod_entry_t;

extern lm_key lm_mmkey2lmkey(mm_key, uint8_t);
extern lm_key lm_arc2key(arc_t *, uint8_t);
extern uint8_t lm_key2dir(lm_key);
extern void lm_decode_key(lm_key, arc_t *);

extern void lm_init(line_map_t *);
extern lm_entry_t * lm_insert(line_map_t *, lm_key, int16_t, uint8_t);
extern lm_entry_t * lm_get(line_map_t *, lm_key);
extern void lm_remove(line_map_t *, mod_entry_t *);
extern void lm_mark_skipped(lm_entry_t *);

#endif
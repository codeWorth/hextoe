#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

/*
 * FNV hash magic constants
 */

#define FNV_INIT 	((uint64_t) 0xcbf29ce484222325ULL)
#define FNV_PRIME	((uint64_t) 0x100000001b3ULL)

#define MM_BUCKETS	512
#define MM_MASK		(MM_BUCKETS - 1)
#define MM_STACK_SIZE	2048
#define HASH_FN(key)	splitmix64(key)
#define MME_INDEX(key)	(HASH_FN(key) & MM_MASK)

#define MASK_30		0x3FFFFFFFULL
#define MASK_61		0x1FFFFFFFFFFFFFFF
#define P1_WON		262144
#define P2_WON		-262144

#ifndef EVAL_CUSTOM
#define MAX_EVAL_DEPTH	9
#define MAX_EVAL_WIDTH	15
#define MIN_EVAL_WIDTH	4
#define EVAL_WIDTH_STEP	2
#endif

#define L1_SCORE	1
#define L2_SCORE	8
#define L3_SCORE	30
#define L4_SCORE	120
#define L5_SCORE	130

#define CLOSE_TERM	1
#define FAR_TERM	2
#define NO_TERM		3

#define STEP_0		step_r
#define STEP_R0		step_l
#define STEP_1		step_dr
#define STEP_R1		step_ul
#define STEP_2		step_dl
#define STEP_R2		step_ur

#define SCORE_FOR_LEN(len) ({		\
	int	score = 0;		\
					\
	switch(len) {			\
	case 1:				\
		score = L1_SCORE;	\
		break;			\
	case 2:				\
		score = L2_SCORE;	\
		break;			\
	case 3:				\
		score = L3_SCORE;	\
		break;			\
	case 4:				\
		score = L4_SCORE;	\
		break;			\
	case 5:				\
		score = L5_SCORE;	\
		break;			\
	default:			\
		assert(0);		\
	}				\
	score;				\
})

#define PUSH_STACK(ml, move, score) ({				\
	assert((ml)->ml_len < (ml)->ml_size);			\
	(ml)->ml_moves[((ml)->ml_len)].mle_move = (move);	\
	(ml)->ml_moves[((ml)->ml_len)].mle_score = (score);	\
	(ml)->ml_len++;						\
})

#define INIT_STACK(ml, arr, size) ({	\
	(ml)->ml_moves = (arr);		\
	(ml)->ml_size = (size);		\
	(ml)->ml_len = 0;		\
})

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

#define	DLL_DELETE(e, next, prev) {			\
	(e)->prev->next = (e)->next;			\
	(e)->next->prev = (e)->prev;			\
	(e)->prev = (e)->next = NULL;			\
}

#define LM_SET(lm_entry, line)			\
	(lm_entry)->mme_value = *((int *) line)

#define LINE_TO_INT(line)			\
	*((int *) line)

#define LM_GET(lm_entry)			\
	((line_t *) (&(lm_entry)->mme_value))

#define LINE_EXTRACT_INFO(info, term_close, term_far, da, len) ({	\
	int16_t	l_info = (info);					\
									\
	*(term_close) = l_info & 1;					\
	l_info >>= 1;							\
	*(term_far) = l_info & 1;					\
	l_info >>= 1;							\
	*(da) = l_info & 1;						\
	l_info >>= 1;							\
	*(len) = l_info;						\
})

#define LINE_GET_LENGTH(info)			\
	((info) >> 3)

#define LINE_GET_TERM_CLOSE(info)		\
	((info) & 1)

#define LINE_PACK_INFO(term_close, term_far, da, len) ({	\
	int16_t	info = 0;					\
								\
	info = (len);						\
	info <<= 1;						\
	info |= (da) & 1;					\
	info <<= 1;						\
	info |= (term_far) & 1;					\
	info <<= 1;						\
	info |= (term_close) & 1;				\
})

typedef struct mm_entry {
	struct mm_entry	*mme_next;
	struct mm_entry	*mme_prev;
	uint64_t	mme_key;
	int		mme_value;
	int16_t		mme_score;	// used only for line map
	bool		mme_skipped;
} mm_entry_t;

typedef struct move_map {
	mm_entry_t	mm_buckets[MM_BUCKETS];
	mm_entry_t	mm_stack[MM_STACK_SIZE];
	uint32_t	mm_stack_size;
} move_map_t;

typedef struct arc {
	bool	a;
	int	r;
	int	c;
} arc_t;

typedef struct move_entry {
	uint64_t	mle_move;
	int		mle_score;
} move_entry_t;

typedef struct move_list {
	move_entry_t	*ml_moves;
	int		ml_len;
	int		ml_size;
} move_list_t;

// l_info contains packed bytes for the info about this line.
// bottom 3 bits are close side term, far side term, then far loc a value
// remaining 13 bits are signed score.
// The "close side" is defined by wherever "step_rev" determines in score_line
// term_far indicates the far side is terminated with opposite player.
// l_dr is the change in r to get to the far side.
// l_dc is the change in c to get to the far side.
typedef struct line {
	int16_t	l_info;
	int8_t	l_dr;
	int8_t	l_dc;
} line_t;

uint64_t
splitmix64(uint64_t key) {
    key += 0x9e3779b97f4a7c15;
    key = (key ^ (key >> 30)) * 0xbf58476d1ce4e5b9;
    key = (key ^ (key >> 27)) * 0x94d049bb133111eb;
    return key ^ (key >> 31);
}

// Turns out this is way slower than splitmix64
uint64_t
fnv_hash(uint64_t key)
{
	int		i = 0;
	uint64_t	hval = FNV_INIT;

	while(i < 8) {
		hval ^= key & 0xFF;
		hval *= FNV_PRIME;
		key >>= 8;
		i++;
	}
	return hval;
}

// We need to remember to swap pointers so that the map stays valid.
void
swap_entries(mm_entry_t *entries, int i, int j)
{
	mm_entry_t	tmp_entry;

	tmp_entry = entries[i];

	if(entries[i].mme_next == &entries[j]) {
		entries[i] = entries[j];
		entries[j] = tmp_entry;
		entries[i].mme_prev->mme_next = &entries[i];
		entries[j].mme_next->mme_prev = &entries[j];
		entries[i].mme_next = &entries[j];
		entries[j].mme_next = &entries[i];
	} else if(entries[j].mme_next == &entries[i]) {
		entries[i] = entries[j];
		entries[j] = tmp_entry;
		entries[j].mme_prev->mme_next = &entries[j];
		entries[i].mme_next->mme_prev = &entries[i];
		entries[j].mme_next = &entries[i];
		entries[i].mme_next = &entries[j];
	}else {
		entries[i].mme_next->mme_prev = &entries[j];
		entries[i].mme_prev->mme_next = &entries[j];
		entries[i] = entries[j];

		entries[j].mme_next->mme_prev = &entries[i];
		entries[j].mme_prev->mme_next = &entries[i];
		entries[j] = tmp_entry;
	}
}

// Courtesy of rosetta code. QuickSelect algorithm. Modifies the given array.
// This reorders the stack, so popping from the stack will be a bad idea now.
// However, it updates the node pointers, so no worries about continuing to use
// the hash map.
mm_entry_t *
mm_entries_qselect(mm_entry_t *entries, int len, int k)
{
	int i, st;

	for (st = i = 0; i < len - 1; i++) {
		if (entries[i].mme_value < entries[len-1].mme_value) continue;
		swap_entries(entries, i, st);
		st++;
	}
	swap_entries(entries, len-1, st);
	if(k == st) {
		return &entries[st];
	} else if(st > k) {
		return mm_entries_qselect(entries, st, k);
	} else {
		return mm_entries_qselect(entries + st, len - st, k - st);
	}
}


// Also courtesy of rosetta code. In place list insertion sort, descending order.
void
ml_sort(move_list_t *ml) {
	int		i, j, len = ml->ml_len;
	move_entry_t	e;

	for(i = 1; i < len; ++i) {
		e = ml->ml_moves[i];
		j = i;
		while((j > 0) && (e.mle_score > ml->ml_moves[j - 1].mle_score)) {
			ml->ml_moves[j] = ml->ml_moves[j - 1];
			--j;
		}
		ml->ml_moves[j] = e;
	}
}

// Also also courtesy of rosetta code. Min heap push and pop
void
mh_push(move_list_t *mh, uint64_t move, int score)
{
	int	i, j;

	assert(mh->ml_len < mh->ml_size);
	i = mh->ml_len + 1;
	j = i / 2;
	while(i > 1 && mh->ml_moves[j].mle_score > score) {
		mh->ml_moves[i] = mh->ml_moves[j];
		i = j;
		j = j / 2;
	}
	mh->ml_moves[i].mle_score = move;
	mh->ml_moves[i].mle_score = score;
	mh->ml_len++;
}

uint64_t
mh_pop(move_list_t *mh) {
	int		i, j, k;
	uint64_t	move;

	assert(mh->ml_len > 0);
	move = mh->ml_moves[1].mle_move;
	mh->ml_moves[1] = mh->ml_moves[mh->ml_len];
	mh->ml_len--;
	i = 1;
	while(i != mh->ml_len + 1) {
		k = mh->ml_len + 1;
		j = 2 * i;
		if (j <= mh->ml_len && mh->ml_moves[j].mle_score < mh->ml_moves[k].mle_score) {
			k = j;
		}
		if (j + 1 <= mh->ml_len && mh->ml_moves[j + 1].mle_score < mh->ml_moves[k].mle_score) {
			k = j + 1;
		}
		mh->ml_moves[i] = mh->ml_moves[k];
		i = k;
	}
	return move;
}

uint64_t
mm_make_key(arc_t *arc)
{
	uint64_t	key = 0;

	key |= arc->a & 1;
	key <<= 30;
	key |= arc->r & MASK_30;
	key <<= 30;
	key |= arc->c & MASK_30;
	return key;
}

uint64_t
mm_append_dir_key(uint64_t key, uint8_t dir)
{
	uint64_t	llu_dir = dir;

	// We're putting this into the top 3 bits. Technically it will only
	// take up 2 bits worth of space, but you get the point.
	llu_dir <<= 61;
	return (key & MASK_61) | llu_dir;
}

void
pos_from_mm_key(uint64_t key, arc_t *arc)
{
	// Sign extend the first 30 bits
	arc->c = (((int) (key & MASK_30)) << 2) >> 2;
	key >>= 30;
	arc->r = (((int) (key & MASK_30)) << 2) >> 2;
	key >>= 30;
	arc->a = key & 1;
}

uint8_t
dir_from_mm_key(uint64_t key)
{
	return key >> 61;
}

void
mm_init(move_map_t *mm)
{
	int	i;

	for(i = 0; i < MM_BUCKETS; i++) {
		DLL_INIT(&mm->mm_buckets[i], mm_entry_t, mme_next, mme_prev);
	}
	mm->mm_stack_size = 0;
}

mm_entry_t *
mm_insert(move_map_t *mm, uint64_t key, int value, int16_t score)
{
	uint32_t	index;
	mm_entry_t	*head, *entry;

	// Get bucket index
	index = MME_INDEX(key);

	// Get new entry from stack, increase stack size
	entry = &mm->mm_stack[mm->mm_stack_size];
	mm->mm_stack_size++;

	// Populate entry
	entry->mme_key = key;
	entry->mme_value = value;
	entry->mme_score = score;
	entry->mme_skipped = false;

	// Insert entry at start of bucket list, past head node
	head = &mm->mm_buckets[index];
	DLL_ADD2HEAD(head, entry, mm_entry_t, mme_next, mme_prev);
	return entry;
}

mm_entry_t *
mm_get(move_map_t *mm, uint64_t key)
{
	uint32_t	index;
	mm_entry_t	*head, *node;

	// Make hash/value pair, get bucket index
	index = MME_INDEX(key);

	// Iterate over list in bucket
	head = &mm->mm_buckets[index];
	node = head->mme_next;
	while(node != head) {
		if(key == node->mme_key && !node->mme_skipped) {
			return node;
		}
		node = node->mme_next;
	}
	return NULL;
}

void
mm_remove(move_map_t *mm, uint64_t key)
{
	uint32_t	index;
	mm_entry_t	*node, *head;

	// Get bucket index
	index = MME_INDEX(key);

	// Iterate over list in bucket
	head = &mm->mm_buckets[index];
	node = head->mme_next;
	while(node != head) {
		if(key == node->mme_key) {
			// Remove this element. It needs to be the top item
			// of the stack. In my use case, I always insert and
			// remove in this order (LIFO).
			assert(node == &mm->mm_stack[mm->mm_stack_size - 1]);
			mm->mm_stack_size--;
			DLL_DELETE(node, mme_next, mme_prev);
			return;
		}
		node = node->mme_next;
	}
	// We always expect to find the node we intend to remove.
	assert(false);
}

bool
is_p1_for_turn(int num_moves)
{

	if(num_moves == 0) {
		return true;
	}
	if(((num_moves - 1) / 2) % 2 == 0) {
		return false;
	}
	return true;
}

void
step_l(arc_t *arc, arc_t *darc)
{
	darc->a = arc->a;
	darc->r = arc->r;
	darc->c = arc->c - 1;
}

void
step_r(arc_t *arc, arc_t *darc)
{
	darc->a = arc->a;
	darc->r = arc->r;
	darc->c = arc->c + 1;
}

void
step_dl(arc_t *arc, arc_t *darc)
{
	darc->a = !arc->a;
	darc->r = arc->r + (arc->a & 1);
	darc->c = arc->c - (darc->a & 1);
}

void
step_dr(arc_t *arc, arc_t *darc)
{
	darc->a = !arc->a;
	darc->r = arc->r + (arc->a & 1);
	darc->c = arc->c + (arc->a & 1);
}

void
step_ul(arc_t *arc, arc_t *darc)
{
	darc->a = !arc->a;
	darc->r = arc->r - (darc->a & 1);
	darc->c = arc->c - (darc->a & 1);
}

void
step_ur(arc_t *arc, arc_t *darc)
{
	darc->a = !arc->a;
	darc->r = arc->r - (darc->a & 1);
	darc->c = arc->c + (arc->a & 1);
}

int
score_line(move_map_t *mm, mm_entry_t *entry, arc_t *arc, int *len, int *score,
	   arc_t *far_arc, void (*step)(arc_t *, arc_t *),
	   void (*step_rev)(arc_t *, arc_t *))
{
	int		i, negate_score;
	bool		is_p1, terminated;
	arc_t		cur_arc, prev_arc, next_arc;
	uint64_t	prev_key, next_key;
	mm_entry_t	*prev_entry, *next_entry;

	is_p1 = entry->mme_value;
	step_rev(arc, &prev_arc);
	prev_key = mm_make_key(&prev_arc);
	prev_entry = mm_get(mm, prev_key);
	// If the previous entry is the same as the current one, we just return 0.
	// We only evaluate the end of a line, because we don't want to double count
	// scores.
	if(prev_entry != NULL && prev_entry->mme_value == is_p1) {
		*len = 0;
		*score = 0;
		return 0;
	}
	// If this line is terminated on the prev side, we will single count the score.
	// If it's unterminated on the prev side, we'll double count it, because we have two
	// opportunities to use this line.
	terminated = prev_entry != NULL;
	// If we are p1, the score must be positive. If we are p1, the score must be
	// negative.
	negate_score = is_p1 ? 1 : -1;
	cur_arc.a = arc->a;
	cur_arc.r = arc->r;
	cur_arc.c = arc->c;
	for(i = 0; i < 5; i++) {
		step(&cur_arc, &next_arc);
		next_key = mm_make_key(&next_arc);
		next_entry = mm_get(mm, next_key);
		// If we've reached the end of the line, and there's nothing blocking it,
		// then we can return the score (doubled if not terminated on the prev side)
		if(next_entry == NULL) {
			// A good candidate move is at the end of this line
			far_arc->a = next_arc.a;
			far_arc->r = next_arc.r;
			far_arc->c = next_arc.c;
			// We want to make high scores much more desirable. Otherwise, dense points,
			// a.k.a. locations with many nearby tiles, get overrepresented.
			*len = (i+1) * negate_score;
			if(terminated) {
				*score = SCORE_FOR_LEN(i+1) * negate_score;
				return CLOSE_TERM;
			} else {
				// Double the score if we're free on both sides
				*score = SCORE_FOR_LEN(i+1) * negate_score * 2;
				return NO_TERM;
			}
		}
		// If we find an opponent tile, we've ended here. We may have 0 score,
		// or normal score if the other side is not terminated.
		if(next_entry->mme_value != is_p1) {
			if(terminated) {
				*len = 0;
				*score = 0;
				return 0;
			} else {
				*len = (i+1) * negate_score;
				*score = SCORE_FOR_LEN(i+1) * negate_score;
				return FAR_TERM;
			}
		}
		cur_arc.a = next_arc.a;
		cur_arc.r = next_arc.r;
		cur_arc.c = next_arc.c;
	}
	// We never hit an opponent or a blank spot. So that means we have 6 in a row,
	// which is winning outright. Return P1_WON or P2_WON to represent that.
	*score = is_p1 ? P1_WON : P2_WON;
	return 0;
}

void
populate_move_map(move_map_t *mm, int* as, int* rs, int* cs, int* is_p1s,
		  int num_moves)
{
	int		i;
	arc_t		arc;
	uint64_t	key;

	for(i = 0; i < num_moves; i++) {
		arc.a = as[i];
		arc.r = rs[i];
		arc.c = cs[i];
		key = mm_make_key(&arc);
		mm_insert(mm, key, is_p1s[i], 0);
	}
}

// Populate the given candidate move map with moves. For over every line in
// the line map. For each line, retrieve which ends are terminated. For each
// end which is not terminated, insert or update the cmm with the move and its
// score (absolute value to highlight important tiles for either player).
void
populate_cmm(move_map_t *cmm, move_map_t *lm)
{
	int		i, dir, score, da, len;
	bool		term_close, term_far;
	arc_t		arc, darc;
	line_t		*line;
	uint64_t	move_close, move_far;
	mm_entry_t	*entry, *cm_entry;

	// Clear the cmm
	mm_init(cmm);
	for(i = 0; i < lm->mm_stack_size; i++) {
		entry = &lm->mm_stack[i];
		line = LM_GET(entry);
		dir = dir_from_mm_key(entry->mme_key);
		LINE_EXTRACT_INFO(line->l_info, &term_close, &term_far, &da,
				  &len);
		score = entry->mme_score;
		score = score > 0 ? score : -score;
		pos_from_mm_key(entry->mme_key, &arc);
		if(dir == 0) {
			STEP_R0(&arc, &darc);
		} else if(dir == 1) {
			STEP_R1(&arc, &darc);
		} else {
			STEP_R2(&arc, &darc);
		}

		if(!term_close) {
			move_close = mm_make_key(&darc);
			cm_entry = mm_get(cmm, move_close);
			if(cm_entry == NULL) {
				mm_insert(cmm, move_close, score, 0);
			} else {
				cm_entry->mme_value += score;
			}
		}
		if(!term_far) {
			arc.a = da;
			arc.r += line->l_dr;
			arc.c += line->l_dc;
			move_far = mm_make_key(&arc);
			cm_entry = mm_get(cmm, move_far);
			if(cm_entry == NULL) {
				mm_insert(cmm, move_far, score, 0);
			} else {
				cm_entry->mme_value += score;
			}
		}
	}
}

// Iterate over every move in the current board state. For each move, check all the
// directions to see if it's part of a line (we check right, down right, down left,
// since we don't need to check the opposite directions).
// Each move will give us a score and some candidate moves, or undefined.
// If we get undefined, it means that move is connected to a winning row.
// If we get score = 0, then it's a no-op.
// Otherwise, we put the move into the candidates table, adding the score if the
// move is already in the table. We also add up the total score for every move.
// This should be only be called at the top level of iteration. On deeper levels,
// we should reuse line map results, and just update based on the one new move.
int
evaluate_board(move_map_t *mm, move_map_t *cmm, move_map_t *lm)
{
	int 		i, j, len, term_res, score, total_score = 0;
	bool		a;
	arc_t		arc, far_arc;
	line_t		line;
	uint64_t	key;
	mm_entry_t	*entry;

	mm_init(lm);
	for(i = 0; i < mm->mm_stack_size; i++) {
		entry = &mm->mm_stack[i];
		pos_from_mm_key(entry->mme_key, &arc);
		for(j = 0; j < 3; j++) {
			if(j == 0) {
				term_res = score_line(mm, entry, &arc, &len,
						      &score, &far_arc,
						      STEP_0, STEP_R0);
			} else if(j == 1) {
				term_res = score_line(mm, entry, &arc, &len,
						      &score, &far_arc,
						      STEP_1, STEP_R1);
			} else {
				term_res = score_line(mm, entry, &arc, &len,
						      &score, &far_arc,
						      STEP_2, STEP_R2);
			}
			// If we got a winner, just return that.
			if(score == P1_WON || score == P2_WON) {
				return score;
			}
			// If this move got scored 0, we don't care about it at all.
			if(score == 0) {
				continue;
			}
			total_score += score;
			// Insert line map entry for the line we just scored.
			if(term_res != FAR_TERM) {
				// We are not terminated on the far side, so
				// we need to populate the location of that
				// for later use.
				a = far_arc.a;
				line.l_dr = far_arc.r - arc.r;
				line.l_dc = far_arc.c - arc.c;
			}
			line.l_info = LINE_PACK_INFO(term_res == CLOSE_TERM,
						     term_res == FAR_TERM,
						     a, len);
			key = mm_append_dir_key(entry->mme_key, j);
			mm_insert(lm, key, LINE_TO_INT(&line), score);
		}
	}
	populate_cmm(cmm, lm);
	return total_score;
}

// Walk in the given direction until we hit nothing, or we hit a different player
// than ourselves. Return false if we found nothing past the initial one.
bool
walk_dir(move_map_t *mm, arc_t *start, bool is_p1,
	 void (*step)(arc_t *, arc_t *), arc_t *end)
{
	bool		found_line = false;
	arc_t		tmp;
	uint64_t	key;
	mm_entry_t	*entry;

	step(start, end);
	key = mm_make_key(end);
	while((entry = mm_get(mm, key)) != NULL) {
		if(entry->mme_value != is_p1) {
			break;
		}
		found_line = true;
		tmp.a = end->a;
		tmp.r = end->r;
		tmp.c = end->c;
		step(&tmp, end);
		key = mm_make_key(end);
	}
	return found_line;
}

// Just like evaluate_board, but we don't need to go over the entire board and
// get every single line again. All we need to do is check the region around the
// most recent move. Also, when this function returns, we need to undo any
// changes made to the line map.
int
evaluate_board_cached(move_map_t *mm, move_map_t *cmm, move_map_t *lm,
		      arc_t *move, bool is_p1)
{
	int 		i, j, score, len, len2, total_score = 0;
	int		dr, dc;
	bool		tc, tf, a, friend_left, friend_right;
	arc_t		arc1, arc2;
	line_t		*line, new_line;
	line_t		sv_lines[3];
	uint8_t		cases[3];
	uint64_t	key, left_key, home_key;
	mm_entry_t	*entry;
	mm_entry_t	*n_entries[3], *r_entries[3];
	mm_entry_t	*n_entry, *r_entry;

	home_key = mm_make_key(move);
	// Update based on the new move, looking the region around it for new
	// lines.
	// In the comments below I say "right" and "left" because it's easier to
	// think that way, but really it means normal and reverse.
	for(j = 0; j < 3; j++) {
		// First, look right by 1.
		if(j == 0) {
			STEP_0(move, &arc1);
		} else if(j == 1) {
			STEP_1(move, &arc1);
		} else {
			STEP_2(move, &arc1);
		}
		key = mm_make_key(&arc1);
		key = mm_append_dir_key(key, 0);
		n_entry = mm_get(lm, key);
		// Then look left until the end of that line.
		if(j == 0) {
			walk_dir(mm, move, is_p1, STEP_R0, &arc2);
		} else if(j == 1) {
			walk_dir(mm, move, is_p1, STEP_R1, &arc2);
		} else {
			walk_dir(mm, move, is_p1, STEP_R2, &arc2);
		}
		key = mm_make_key(&arc2);
		left_key = mm_append_dir_key(key, 0);
		r_entry = mm_get(lm, left_key);
		// Now we've got the lines that might connect to this new move, if they
		// exist. That's because lines always start at the leftmost location, so
		// we did a walk dir to the left to find the start, and just went right by
		// one to find that line.
		// Even lines of length 1 are in the map. Because of that, we do
		// not need to do additional checks in the move map. If we have
		// a single tile to our left, or a line going down right,
		// both would still appear in the move map as a line of length one going right.
		// Check if it is friend or foe to the left and right.
		friend_left = n_entry && (n_entry->mme_score > 0) == is_p1;
		friend_right = r_entry && (r_entry->mme_score > 0) == is_p1;
		// Copy over the entry pointers so we can restore things later
		n_entries[j] = n_entry;
		r_entries[j] = r_entry;
		if(n_entry != NULL && friend_left && r_entry != NULL && friend_right) {
			// We've got friendly lines on both sides, so we're filling in
			// the middle of a line. Mark the one on the norm side skipped,
			// and update the score of the one on the rev side. Also save
			// the original info of the rev side so we can restore it later.
			line = LM_GET(n_entry);
			LINE_EXTRACT_INFO(line->l_info, &tc, &tf, &a, &len);
			dr = line->l_dr;
			dc = line->l_dc;
			// n0 marked skipped now
			n_entry->mme_skipped = true;

			// Copy the r entry line info so we don't forget.
			sv_lines[j] = *LM_GET(r_entry);
			// Get a reference to the r entry's line
			line = LM_GET(r_entry);
			// Get the original length
			len2 = LINE_GET_LENGTH(line->l_info);
			// Other than the new length and left side termination info,
			// we want to copy everything from the right side
			tc = LINE_GET_TERM_CLOSE(line->l_info);
			line->l_info = LINE_PACK_INFO(tc, tf, a, len2 + 1 + len);
			line->l_dr = dr;
			line->l_dc = dc;
			cases[j] = 0;
			continue;
		}
		if(n_entry != NULL && friend_right) {
			// We only have a friendly line on our right. Mark it skipped.
			line = LM_GET(n_entry);
			LINE_EXTRACT_INFO(line->l_info, &tc, &tf, &a, &len);
			// n0 marked skipped now
			n_entry->mme_skipped = true;

			// Copy everything from the entry to our right, other than
			// increasing the length by one for our new move, and the tc.
			// Term close depends on if we found someone to the left.
			tc = r_entry != NULL;
			new_line.l_info = LINE_PACK_INFO(tc, tf, a, len+1);
			new_line.l_dr = line->l_dr;
			new_line.l_dc = line->l_dc;
			mm_insert(lm, mm_append_dir_key(home_key, j),
				  LINE_TO_INT(&new_line), score);
			cases[j] = 1;
			continue;
		}
		if(r_entry != NULL && friend_left) {
			// We only have a friendly line to our left. Append this move to it.
			// Also save the current line info to restore it later.
			line = LM_GET(r_entry);
			LINE_EXTRACT_INFO(line->l_info, &tc, &tf, &a, &len);
			sv_lines[j] = *line;
			// Now we've got the info for this line. We have a couple of tasks:
			// 1. Update the total length.
			// 2. Update the right side termination info
			// 3. Update the delta to the far side stored in the line
			len += 1;
			tf = n_entry != NULL;
			a = arc1.a;
			line->l_info = LINE_PACK_INFO(tc, tf, a, len);
			// arc1 is where we tried to find someone to our right.
			// arc2 is where the r0 line begins
			line->l_dr = arc1.r - arc2.r;
			line->l_dc = arc1.c - arc2.c;
			cases[j] = 2;
			continue;
		}
		// We've got no one around, or maybe just enemies. Therefore,
		// this is a 1 long line. Create and insert it.
		// Whether we are terminated on the left and right depends on
		// whether we found any line there.
		tc = r_entry != NULL;
		tf = n_entry != NULL;
		// The tail location is arc1, because that's where we looked to
		// the right, similar to the case above. However, unlike the case
		// above, the starting location is just the current move.
		a = arc1.a;
		new_line.l_dr = arc1.r - move->r;
		new_line.l_dc = arc1.c - move->c;
		new_line.l_info = LINE_PACK_INFO(tc, tf, a, 1);
		mm_insert(lm, mm_append_dir_key(home_key, j),
			  LINE_TO_INT(&new_line), score);
		cases[j] = 3;
	}

	// Now that we've populated the line map with the new lines, find the
	// total score
	for(i = 0; i < lm->mm_stack_size; i++) {
		entry = &lm->mm_stack[i];
		score = entry->mme_score;

		// If we got a winner, just return that.
		if(score == P1_WON || score == P2_WON) {
			total_score = score;
			goto undo_move;
		}
		total_score += score;
	}

undo_move:
	// We're cleaning up now. We need to undo the move that was made,
	// specifically its effect on the line map needs to be undone.
	// We go in reverse order because we may delete entries from the lm,
	// and we want to pop them off the stack correctly.
	for(j = 2; j >= 0; j--) {
		switch(cases[j]) {
		case 0:
			// We had friends on both sides. We need to unmark the
			// right side as skipped, and restore the left side.
			n_entries[j]->mme_skipped = false;
			LM_SET(r_entries[j], &sv_lines[j]);
			break;
		case 1:
			// We had only a friend on the right. We need to unmark
			// it as skipped, and delete the new record we inserted
			// for the current location.
			n_entries[j]->mme_skipped = false;
			mm_remove(lm, mm_append_dir_key(home_key, j));
			break;
		case 2:
			// We had only a friend on the left. We need to restore
			// it's value.
			LM_SET(r_entries[j], &sv_lines[j]);
			break;
		case 3:
			// We had no friends around us. We just need to delete
			// the 1 long line we inserted for this case.
			mm_remove(lm, mm_append_dir_key(home_key, j));
			break;
		}
	}
	return total_score;
}

int
look_moves_at_depth(int depth)
{
	int 	moves;

	moves = MAX_EVAL_WIDTH - depth * EVAL_WIDTH_STEP;
	if(moves < MIN_EVAL_WIDTH) {
		return MIN_EVAL_WIDTH;
	} else {
		return moves;
	}
}

// This function chooses the best move for the current player, based on the moves
// passed in.
// mm is shared between calls to this function, it must
// be returned to its original state when this function exits.
// candidate_moves is shared memory but not needed to be saved between calls.
// lm is the lines map, it is shared between calls and must be returned to its
// original state before returning.
int
do_evaluate_ahead(move_map_t *mm, move_map_t *candidate_moves, move_map_t *lm,
		  int depth, int alpha, int beta, uint64_t *best_move_out)
{
	bool		is_p1;
	int		i, sub_eval, look_moves, current_eval, best_score;
	uint64_t	current_move, best_move;
	move_list_t	impact_moves;
	move_entry_t	impact_entries[MAX_EVAL_WIDTH];
	mm_entry_t	*kth_largest, *entry;

	INIT_STACK(&impact_moves, impact_entries, MAX_EVAL_WIDTH);
	is_p1 = is_p1_for_turn(mm->mm_stack_size);
	current_eval = evaluate_board(mm, candidate_moves, lm);
	if(current_eval == P1_WON || current_eval == P2_WON) {
		// If we need to return the best move, but the game is over,
		// so return -1 to indicate as such. -1 is a valid score, but
		// the top level caller expects the return value to be an error
		// code, not a score.
		if(depth == 0) {
			return -1;
		} else {
			return current_eval;
		}
	}
	// If this is our max depth, just return the current eval without finding anything more accurate.
	if(depth >= MAX_EVAL_DEPTH) {
		return current_eval;
	}
	// Now we need to go through the moves and evaluate them in order. The moves
	// are labeled with a score of how impactful they are. We simply go through them
	// in order of impact, and see which gives us the best evaluation.
	look_moves = look_moves_at_depth(depth);
	if(look_moves > candidate_moves->mm_stack_size) {
		look_moves = candidate_moves->mm_stack_size;
		// If we are taking every element, skip all of the below code.
		// First copy every move over though.
		for(i = 0; i < look_moves; i++) {
			entry = &candidate_moves->mm_stack[i];
			PUSH_STACK(&impact_moves, entry->mme_key,
				   entry->mme_value);
		}
		goto eval_impact_moves;
	}
	// First find the kth largest element:
	kth_largest = mm_entries_qselect(candidate_moves->mm_stack,
					 candidate_moves->mm_stack_size,
					 look_moves - 1);
	// Now copy <look_moves> moves from candidate_moves if they have a large
	// enough impact. We first copy those with larger impact than kth element,
	// then those with impact == kth, because we don't want to miss any elements
	// with high impact.
	for(i = 0; i < candidate_moves->mm_stack_size; i++) {
		entry = &candidate_moves->mm_stack[i];
		if(entry->mme_value > kth_largest->mme_value) {
			PUSH_STACK(&impact_moves, entry->mme_key,
				   entry->mme_value);
		}
	}
	assert(impact_moves.ml_len < look_moves);
	// Now copy with impact == kth until we fill enough slots.
	for(i = 0; i < candidate_moves->mm_stack_size; i++) {
		entry = &candidate_moves->mm_stack[i];
		if(entry->mme_value == kth_largest->mme_value) {
			PUSH_STACK(&impact_moves, entry->mme_key,
				   entry->mme_value);
			if(impact_moves.ml_len >= look_moves) {
				break;
			}
		}
	}

eval_impact_moves:
	// Now iterate over every impact move.
	ml_sort(&impact_moves);
	for(i = 0; i < impact_moves.ml_len; i++) {
		current_move = impact_moves.ml_moves[i].mle_move;
		mm_insert(mm, current_move, is_p1, 0);
		sub_eval = do_evaluate_ahead(mm, candidate_moves, lm, depth+1,
					     alpha, beta, NULL);
		mm_remove(mm, current_move);
		// If this is the first try, or this move is better for us than the previous
		// best, update the best move.
		if(i == 0 || (is_p1 && sub_eval > best_score) ||
		   (!is_p1 && sub_eval < best_score)) {
			best_score = sub_eval;
			best_move = current_move;
		}
		// Update alpha/beta bounds and prune if the opponent can already do better.
		if(is_p1) {
			if(best_score > alpha) {
				alpha = best_score;
			}
			if(alpha >= beta) {
				break;
			}
		} else {
			if(best_score < beta) {
				beta = best_score;
			}
			if(beta <= alpha) {
				break;
			}
		}
	}
	if(depth == 0) {
		*best_move_out = best_move;
		return 0;
	} else {
		return best_score;
	}
}

bool
evaluate_ahead(int* as, int* rs, int* cs, int* is_p1s, int num_moves,
	       int* best_a, int* best_r, int* best_c)
{
	int		err;
	arc_t		best_move_pos;
	uint64_t	best_move;
	move_map_t	mm;
	move_map_t	candidate_moves;
	move_map_t	lm;

	mm_init(&mm);
	populate_move_map(&mm, as, rs, cs, is_p1s, num_moves);
	err = do_evaluate_ahead(&mm, &candidate_moves, &lm, 0, P2_WON, P1_WON,
				&best_move);
	if(err == 0) {
		pos_from_mm_key(best_move, &best_move_pos);
		*best_a = best_move_pos.a;
		*best_r = best_move_pos.r;
		*best_c = best_move_pos.c;
		return true;
	} else {
		return false;
	}
}

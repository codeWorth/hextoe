#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <assert.h>

#ifdef DEBUG
#include <stdio.h>
#include <time.h>
#endif

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

#define MASK_31		0x7FFFFFFFULL
#define P1_WON		262144
#define P2_WON		-262144
#define MAX_EVAL_DEPTH	6
#define MAX_EVAL_WIDTH	11
#define MIN_EVAL_WIDTH	4
#define EVAL_WIDTH_STEP	2

#define PUSH_MOVE_STACK(stack, sz, mme) ({	\
	(stack)[(sz)] = (mme)->mme_key;		\
	(sz)++;					\
})

typedef struct mm_entry {
	struct mm_entry	*mme_next;
	uint64_t	mme_key;
	int		mme_value;
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

// If we're calling this, we don't care about the linked list anymore, so no
// need to swap the next pointers.
void
swap_entries(mm_entry_t *entries, int i, int j)
{
	int		tmp_mme_value;
	uint64_t	tmp_mme_key;

	tmp_mme_key = entries[i].mme_key;
	tmp_mme_value = entries[i].mme_value;
	entries[i].mme_key = entries[j].mme_key;
	entries[i].mme_value = entries[j].mme_value;
	entries[j].mme_key = tmp_mme_key;
	entries[j].mme_value = tmp_mme_value;
}

// Courtesy of rosetta code
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

uint64_t
mm_make_key(arc_t *arc)
{
	uint64_t	key = 0;

	key |= arc->a & 1;
	key <<= 31;
	key |= arc->r & MASK_31;
	key <<= 31;
	key |= arc->c & MASK_31;
	return key;
}

void
pos_from_mm_key(uint64_t key, arc_t *arc)
{
	// Sign extend the first 31 bits
	arc->c = (((int) (key & MASK_31)) << 1) >> 1;
	key >>= 31;
	arc->r = (((int) (key & MASK_31)) << 1) >> 1;
	key >>= 31;
	arc->a = key & 1;
}

void
mm_init(move_map_t *mm)
{
	int	i;

	for(i = 0; i < MM_BUCKETS; i++) {
		mm->mm_buckets[i].mme_next = NULL;
	}
	mm->mm_stack_size = 0;
}

void
mm_insert(move_map_t *mm, uint64_t key, int value)
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

	// Insert entry at start of bucket list, past head node
	head = &mm->mm_buckets[index];
	entry->mme_next = head->mme_next;
	head->mme_next = entry;
}

mm_entry_t *
mm_get(move_map_t *mm, uint64_t key)
{
	uint32_t	index;
	mm_entry_t	*node;

	// Make hash/value pair, get bucket index
	index = MME_INDEX(key);

	// Iterate over list in bucket
	node = mm->mm_buckets[index].mme_next;
	while(node != NULL) {
		if(key == node->mme_key) {
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
	mm_entry_t	*node, *next, *head;

	// Get bucket index
	index = MME_INDEX(key);

	// Iterate over list in bucket
	head = &mm->mm_buckets[index];
	node = head->mme_next;
	while(node != NULL) {
		if(key == node->mme_key) {
			// Remove this element. It needs to be the top item
			// of the stack. In my use case, I always insert and
			// remove in this order (LIFO).
			assert(node == &mm->mm_stack[mm->mm_stack_size - 1]);
			mm->mm_stack_size--;
			head->mme_next = node->mme_next;
			node->mme_next = NULL;
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
score_line(move_map_t *mm, mm_entry_t *entry, arc_t *arc, int *score,
	   uint64_t *move1, uint64_t *move2, void (*step)(arc_t *, arc_t *),
	   void (*step_rev)(arc_t *, arc_t *))
{
	int		i, negate_score, l_score;
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
		l_score = i+1;
		step(&cur_arc, &next_arc);
		next_key = mm_make_key(&next_arc);
		next_entry = mm_get(mm, next_key);
		// If we've reached the end of the line, and there's nothing blocking it,
		// then we can return the score (doubled if not terminated on the prev side)
		if(next_entry == NULL) {
			// We want to make high scores much more desirable. Otherwise, points of interest,
			// a.k.a. locations with many nearby tiles, get overrepresented.
			l_score = l_score * l_score * l_score;
			if(terminated) {
				// A good candidate move is at the end of this line
				*score = l_score * negate_score;
				*move1 = next_key;
				return 1;
			} else {
				// At the end of both lines is good if possible
				*score = l_score * negate_score * 2;
				*move1 = prev_key;
				*move2 = next_key;
				return 2;
			}
		}
		// If we find an opponent tile, we've ended here. We may have 0 score,
		// or normal score if the other side is not terminated.
		if(next_entry->mme_value != is_p1) {
			l_score = l_score * l_score * l_score;
			if(terminated) {
				*score = 0;
				return 0;
			} else {
				*score = l_score * negate_score;
				*move1 = prev_key;
				return 1;
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
		mm_insert(mm, key, is_p1s[i]);
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
// Do not rely on the move index in the movesTbl value; it may not be set.
int
evaluate_board(move_map_t *mm, move_map_t *candidate_moves)
{
	int 		r, c, i, j, score, mcount, total_score = 0;
	bool		a;
	arc_t		arc;
	uint64_t	move1, move2;
	mm_entry_t	*entry, *cm_entry;

	// Clear this map
	mm_init(candidate_moves);

	for(i = 0; i < mm->mm_stack_size; i++) {
		entry = &mm->mm_stack[i];
		pos_from_mm_key(entry->mme_key, &arc);
		for(j = 0; j < 3; j++) {
			if(j == 0) {
				mcount = score_line(mm, entry, &arc, &score,
						    &move1, &move2,
						    step_r, step_l);
			} else if(j == 1) {
				mcount = score_line(mm, entry, &arc, &score,
						    &move1, &move2,
						    step_dr, step_ul);
			} else {
				mcount = score_line(mm, entry, &arc, &score,
						    &move1, &move2,
						    step_dl, step_ur);
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
			// Make score always positive
			if(score < 0) {
				score = -score;
			}
			// Insert/update at move1
			if(mcount >= 1) {
				cm_entry = mm_get(candidate_moves, move1);
				if(cm_entry == NULL) {
					mm_insert(candidate_moves, move1, score);
				} else {
					cm_entry->mme_value += score;
				}
			}
			// Insert/update at move2
			if(mcount >= 2) {
				cm_entry = mm_get(candidate_moves, move2);
				if(cm_entry == NULL) {
					mm_insert(candidate_moves, move2, score);
				} else {
					cm_entry->mme_value += score;
				}
			}
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
// passed in. Do not rely on the move index in the movesTbl value; it may not be set.
// movesTbl is shared between calls to this function, it must
// be returned to its original state when this function exits.
int
do_evaluate_ahead(move_map_t *mm, move_map_t* candidate_moves, int depth,
		  int alpha, int beta, uint64_t *best_move)
{
	bool		is_p1;
	int		i, sub_eval, look_moves, current_eval, best_score;
	int		im_count;
	uint64_t	current_move, sub_best_move;
	uint64_t	impact_moves[MAX_EVAL_WIDTH];
	mm_entry_t	*kth_largest, *entry;

	is_p1 = is_p1_for_turn(mm->mm_stack_size);
	current_eval = evaluate_board(mm, candidate_moves);
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
	im_count = 0;
	look_moves = look_moves_at_depth(depth);
	if(look_moves > candidate_moves->mm_stack_size) {
		look_moves = candidate_moves->mm_stack_size;
		// If we are taking every element, skip all of the below code.
		// First copy every move over though.
		for(i = 0; i < look_moves; i++) {
			entry = &candidate_moves->mm_stack[i];
			PUSH_MOVE_STACK(impact_moves, im_count, entry);
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
			PUSH_MOVE_STACK(impact_moves, im_count, entry);
		}
	}
	assert(im_count < look_moves);
	// Now copy with impact == kth until we fill enough slots.
	for(i = 0; i < candidate_moves->mm_stack_size; i++) {
		entry = &candidate_moves->mm_stack[i];
		if(entry->mme_value == kth_largest->mme_value) {
			PUSH_MOVE_STACK(impact_moves, im_count, entry);
			if(im_count >= look_moves) {
				break;
			}
		}
	}

eval_impact_moves:
	// Now iterate over every impact move.
	for(i = 0; i < im_count; i++) {
		current_move = impact_moves[i];
		mm_insert(mm, current_move, is_p1);
		sub_eval = do_evaluate_ahead(mm, candidate_moves, depth+1,
					     alpha, beta, &sub_best_move);
		mm_remove(mm, current_move);
		// If this is the first try, or this move is better for us than the previous
		// best, update the best move.
		if(i == 0 || (is_p1 && sub_eval > best_score) ||
		   (!is_p1 && sub_eval < best_score)) {
			best_score = sub_eval;
			*best_move = current_move;
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
	return depth == 0 ? 0 : best_score;
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

	mm_init(&mm);
	populate_move_map(&mm, as, rs, cs, is_p1s, num_moves);
	err = do_evaluate_ahead(&mm, &candidate_moves, 0, P2_WON, P1_WON,
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

#ifdef DEBUG
int
main(int argc, char const *argv[])
{
	bool 	found_move;
	int	best_a, best_r, best_c;
	int 	as[33] = {1, 1, 0, 0, 1, 0, 0, 0, 1, 0, 1, 1, 0, 0, 0, 0, 0, 1, 1,
			  1, 0, 1, 1, 1, 1, 0, 1, 1, 1, 1, 1, 1, 0};
	int 	rs[33] = {-1, -1, 0, 0, 0, -1, -1, 1, 1, 2, -2, 0, -2, -1, -1, -1,
			  -1, -2, -2, -2, -2, -2, -1, -2, -3, -3, -1, 0, 0, 0, 0, 1, 2};
	int 	cs[33] = {0, 1, 2, 1, 1, 0, 1, 2, 2, 3, 0, 2, 0, -1, -2, -3, 2, -1,
			  -2, 1, 1, -3, -2, -4, 0, 0, 2, 0, -1, 3, -2, 1, 1};
	int 	is_p1s[33] = {1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0,
			      0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1};

	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	found_move = evaluate_ahead(as, rs, cs, is_p1s, 33, &best_a, &best_r,
				    &best_c);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	double elapsed = (t1.tv_sec - t0.tv_sec) * 1e3 +
			 (t1.tv_nsec - t0.tv_nsec) / 1e6;
	printf("evaluate_ahead: %.3f ms\n", elapsed);
	if(found_move) {
		printf("%d %d %d\n", best_a, best_r, best_c);
	} else {
		printf("Failed to find move.");
	}
	return 0;
}
#endif

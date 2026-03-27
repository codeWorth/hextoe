#include <stdbool.h>
#include <stdio.h>
#include "move_map.h"
#include "line_map.h"
#include "bot_util.h"
#include "bot.h"
#include "bot_params.h"
#include "old_bot.h"

#define P1_WON		262144
#define P2_WON		-262144

#define NO_TERM		1
#define BOTH_TERM	2

#define STEP_0		step_r
#define STEP_R0		step_l
#define STEP_1		step_dr
#define STEP_R1		step_ul
#define STEP_2		step_dl
#define STEP_R2		step_ur

#define L1_SCORE	1
#define L2_SCORE	8
#define L3_SCORE	30
#define L4_SCORE	120
#define L5_SCORE	130

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
	   void (*step)(arc_t *, arc_t *),
	   void (*step_rev)(arc_t *, arc_t *))
{
	int		i, negate_score;
	bool		is_p1, terminated;
	arc_t		cur_arc, prev_arc, next_arc;
	uint64_t	prev_key, next_key;
	mm_entry_t	*prev_entry, *next_entry;

	is_p1 = entry->mme_value;
	step_rev(arc, &prev_arc);
	prev_key = mm_arc2key(&prev_arc);
	prev_entry = mm_get(mm, prev_key);
	// If the previous entry is the same as the current one, we return -1;
	// We only evaluate the start of a line, because we don't want to double count
	// scores.
	if(prev_entry != NULL && prev_entry->mme_value == is_p1) {
		*len = 0;
		*score = 0;
		return -1;
	}
	// If this line is terminated on the prev side, we will single count the score.
	// If it's unterminated on the prev side, we'll double count it, because we have two
	// opportunities to use this line.
	terminated = prev_entry != NULL;
	// If we are p1, the score must be positive. If we are p1, the score must be
	// negative.
	negate_score = is_p1 ? 1 : -1;
	cur_arc = *arc;
	for(i = 0; i < 5; i++) {
		step(&cur_arc, &next_arc);
		next_key = mm_arc2key(&next_arc);
		next_entry = mm_get(mm, next_key);
		// If we've reached the end of the line, and there's nothing blocking it,
		// then we can return the score (doubled if not terminated on the prev side)
		if(next_entry == NULL) {
			// We want to make high scores much more desirable. Otherwise, dense points,
			// a.k.a. locations with many nearby tiles, get overrepresented.
			*len = (i+1);
			*score = SCORE_FOR_LEN(i+1) * negate_score;
			if(terminated) {
				return 0;
			} else {
				return NO_TERM;
			}
		}
		// If we find an opponent tile, we've ended here. We may have 0 score,
		// or normal score if the other side is not terminated.
		if(next_entry->mme_value != is_p1) {
			*len = (i+1);
			*score = SCORE_FOR_LEN(i+1) * negate_score;
			if(terminated) {
				return BOTH_TERM;
			} else {
				return 0;
			}
		}
		cur_arc = next_arc;
	}
	// We never hit an opponent or a blank spot. So that means we have 6 in a row,
	// which is winning outright. Return P1_WON or P2_WON to represent that.
	*score = is_p1 ? P1_WON : P2_WON;
	return 0;
}

// Populate the move map with the given list of moves.
// The value of each entry is just 1 for p1 and 0 for p2.
// The key is an encoded version of the position.
void
populate_move_map(move_map_t *mm, int* as, int* rs, int* cs, int* is_p1s,
		  int num_moves)
{
	int		i;
	arc_t		arc;
	mm_key		key;

	for(i = 0; i < num_moves; i++) {
		arc.a = as[i];
		arc.r = rs[i];
		arc.c = cs[i];
		key = mm_arc2key(&arc);
		mm_insert(mm, key, is_p1s[i]);
	}
}

// Walk in the given direction until the line ends. Return false if no line
// was found.
bool
walk_dir(move_map_t *mm, arc_t *start,
	 void (*step)(arc_t *, arc_t *), arc_t *end)
{
	bool		is_p1, found_line = false;
	arc_t		tmp;
	uint64_t	key;
	mm_entry_t	*entry;

	step(start, &tmp);
	key = mm_arc2key(&tmp);
	while((entry = mm_get(mm, key)) != NULL) {
		if(!found_line) {
			// This is our first find, so we're gonna follow this
			// player to the end of their line.
			is_p1 = entry->mme_value;
			found_line = true;
		} else if(entry->mme_value != is_p1) {
			// The player changed, aka the line ended. Return the
			// last known spot.
			break;
		}
		end->a = tmp.a;
		end->r = tmp.r;
		end->c = tmp.c;
		step(end, &tmp);
		key = mm_arc2key(&tmp);
	}
	return found_line;
}

void
walk_count(arc_t start, arc_t *end, void (*step)(arc_t *, arc_t *), int count)
{

	DEBUG_ASSERT(count > 0);
	for(; count > 0; count--) {
		step(&start, end);
		start = *end;
	}
}

// Populate the given candidate move map with moves. Loop over every line in
// the line map. For each line, retrieve which ends are terminated. For each
// end which is not terminated, insert or update the cmm with the move and its
// score (absolute value to highlight important tiles for either player).
// Returns the total score.
int
populate_cmm(move_map_t *mm, move_map_t *cmm, line_map_t *lm)
{
	int		i, score, dir, total_score = 0;
	bool		term_close, term_far;
	arc_t		arc, l_arc, r_arc;
	mm_key		move_close, move_far;
	mm_entry_t	*cm_entry;
	lm_entry_t	*lm_entry;

	// Clear the cmm
	mm_init(cmm);
	for(i = 0; i < lm->lm_stack_size; i++) {
		lm_entry = &lm->lm_stack[i];
		if(LME_IS_SKIPPED(lm_entry)) continue;

		lm_decode_key(lm_entry->lme_key, &arc);
		dir = lm_key2dir(lm_entry->lme_key);
		if(dir == 0) {
			STEP_R0(&arc, &l_arc);
			walk_count(arc, &r_arc, STEP_0, LME_GET_LENGTH(lm_entry));
		} else if(dir == 1) {
			STEP_R1(&arc, &l_arc);
			walk_count(arc, &r_arc, STEP_1, LME_GET_LENGTH(lm_entry));
		} else {
			STEP_R2(&arc, &l_arc);
			walk_count(arc, &r_arc, STEP_2, LME_GET_LENGTH(lm_entry));
		}
		move_close = mm_arc2key(&l_arc);
		move_far = mm_arc2key(&r_arc);
		term_close = mm_get(mm, move_close) != NULL;
		term_far = mm_get(mm, move_far) != NULL;
		// If we have no space on either side, there's nothing to do
		// here.
		if(term_close && term_far) {
			continue;
		}

		score = lm_entry->lme_score;
		if(!term_close && !term_far) {
			score *= 2;
		}
		total_score += score;
		score = score > 0 ? score : -score;

		if(!term_close) {
			cm_entry = mm_get(cmm, move_close);
			if(cm_entry == NULL) {
				mm_insert(cmm, move_close, score);
			} else {
				cm_entry->mme_value += score;
			}
		}
		if(!term_far) {
			cm_entry = mm_get(cmm, move_far);
			if(cm_entry == NULL) {
				mm_insert(cmm, move_far, score);
			} else {
				cm_entry->mme_value += score;
			}
		}
	}
	return total_score;
}

// Iterate over every move in the current board state. For each move, check all the
// directions to see if it's part of a line (we check right, down right, down left,
// since we don't need to check the opposite directions).
// Each move will give us a score and some candidate moves.
// If we get P1_WON or P2_WON, it means that move is connected to a winning row.
// If we get score = 0, then it's a no-op.
// Otherwise, we put the move into the candidates table, adding the score if the
// move is already in the table. We also add up the total score for every move.
// This should be only be called at the top level of iteration. On deeper levels,
// we should reuse line map results, and just update based on the one new move.
int
evaluate_board(move_map_t *mm, line_map_t *lm)
{
	int 		i, j, len, term_res, score, total_score = 0;
	arc_t		arc;
	lm_key		key;
	mm_entry_t	*entry;

	lm_init(lm);
	for(i = 0; i < mm->mm_stack_size; i++) {
		entry = &mm->mm_stack[i];
		mm_decode_key(entry->mme_key, &arc);
		for(j = 0; j < 3; j++) {
			if(j == 0) {
				term_res = score_line(mm, entry, &arc, &len,
						      &score, STEP_0, STEP_R0);
			} else if(j == 1) {
				term_res = score_line(mm, entry, &arc, &len,
						      &score, STEP_1, STEP_R1);
			} else {
				term_res = score_line(mm, entry, &arc, &len,
						      &score, STEP_2, STEP_R2);
			}
			// If we got a winner, just return that.
			if(score == P1_WON || score == P2_WON) {
				return score;
			}
			// If this isn't the start of the line, skip it.
			if(term_res < 0) {
				continue;
			}

			// Insert line map entry for the line we just scored.
			key = lm_mmkey2lmkey(entry->mme_key, j);
			DEBUG_ASSERT(lm_get(lm, key) == NULL);
			lm_insert(lm, key, score, len);

			// Add up total_score
			if(term_res == BOTH_TERM) {
				continue;
			}
			if(term_res == NO_TERM) {
				score *= 2;
			}
			total_score += score;
		}
	}
	return total_score;
}

// We're generating the line map based on the given move map, but we just update
// in the vicinity of home_key. Populates the modified_lines paramwter with a list
// of line map entries which had new lines. All the caller must do to undo
// everything that this function did, is go through the list and call mm_remove
// on each key. That's because we have inserted lines at each of those locations,
// and there are duplicate entries. Each bucket is in insertion order, so you
// just have to delete to undo.
// It's very important to delete from the modified_lines list in reverse order,
// because that lets us unwind the underlying stack in the line map.
// Returns a winning score if someone won, otherwise returns 0
int
evaluate_board_cached(move_map_t *mm, line_map_t *lm, mm_key home_key,
		      bool is_p1, mod_entry_t *line_mods, int *lmods_count)
{
	int 		j, len, score;
	int		negate, l_mod = 0;
	bool		friend_left, friend_right, found;
	arc_t		arc_r, arc_l, move;
	lm_key		key;
	lm_entry_t	*r_entry, *l_entry, *entry;

	negate = is_p1 ? 1 : -1;
	lm_decode_key(home_key, &move);
	// Update based on the new move, looking the region around it for new
	// lines.
	// In the comments below I say "right" and "left" because it's easier to
	// think that way, but really it means forward and reverse as defined
	// by the STEP_0, STEP_R0, etc macros above.
	for(j = 0; j < 3; j++) {
		// First, look right by 1.
		if(j == 0) {
			STEP_0(&move, &arc_r);
		} else if(j == 1) {
			STEP_1(&move, &arc_r);
		} else {
			STEP_2(&move, &arc_r);
		}
		key = lm_arc2key(&arc_r, j);
		r_entry = lm_get(lm, key);
		// Then look left until the end of that line.
		if(j == 0) {
			found = walk_dir(mm, &move, STEP_R0, &arc_l);
		} else if(j == 1) {
			found = walk_dir(mm, &move, STEP_R1, &arc_l);
		} else {
			found = walk_dir(mm, &move, STEP_R2, &arc_l);
		}
		if(found) {
			key = lm_arc2key(&arc_l, j);
			l_entry = lm_get(lm, key);
		} else {
			l_entry = NULL;
		}
		// Now we've got the lines that might connect to this new move, if they
		// exist. That's because lines always start at the leftmost location, so
		// we did a walk dir to the left to find the start, and just went right by
		// one to find that line.
		// Even lines of length 1 are in the map. Because of that, we do
		// not need to do additional checks in the move map. If we have
		// a single tile to our left, or a line going down right (for example),
		// both would still appear in the line map as a line of length one.

		// Check if it is friend or foe to the left and right.
		friend_left = l_entry && (l_entry->lme_score > 0) == is_p1;
		friend_right = r_entry && (r_entry->lme_score > 0) == is_p1;

		// Do we have friends on both sides? If so, we need to join the line
		// on our left through the whole length.
		if(r_entry != NULL && friend_left && l_entry != NULL && friend_right) {
			// The new line will have the length of both lines summed + 1.
			// It will have left side termination based on the left side,
			// and right side termination based on the right side.
			// Finally, it will have dest_a, delta r, and delta c based
			// on the right side.
			// The key for the joined line will be the left entry's.
			len = LME_GET_LENGTH(l_entry) + LME_GET_LENGTH(r_entry) + 1;
			if(len >= 6) {
				*lmods_count = l_mod;
				return is_p1 ? P1_WON : P2_WON;
			}
			score = SCORE_FOR_LEN(len) * negate;
			entry = lm_insert(lm, l_entry->lme_key, score, len);
			lm_mark_skipped(l_entry);
			PUSH_LINE_MOD(line_mods, l_mod, (entry - lm->lm_stack),
				      true);

			// Now we need to hide the right entry. Mark it skipped,
			// and push a line mod to undo that later.
			lm_mark_skipped(r_entry);
			PUSH_LINE_MOD(line_mods, l_mod, (r_entry - lm->lm_stack), 0);
			continue;
		}
		// We only have a friend on the right side. We may or may not
		// have an enemy on the left, so goto the handling for that when done.
		if(r_entry != NULL && friend_right) {
			// We need to make a new line starting at the current
			// move's position. It will be the same as the line to our
			// right, but one longer, and left side termination will
			// be based on l_entry's existence.
			// Also, we need to calculate the new delta.
			// Finally, we mask the existing entry.
			len = LME_GET_LENGTH(r_entry) + 1;
			if(len >= 6) {
				*lmods_count = l_mod;
				return is_p1 ? P1_WON : P2_WON;
			}
			score = SCORE_FOR_LEN(len) * negate;

			// Insert the new line
			key = lm_mmkey2lmkey(home_key, j);
			DEBUG_ASSERT(lm_get(lm, key) == NULL);
			entry = lm_insert(lm, key, score, len);
			// This one isn't covering up an existing entry at this location.
			PUSH_LINE_MOD(line_mods, l_mod, (entry - lm->lm_stack), false);

			// Mask the old one
			lm_mark_skipped(r_entry);
			PUSH_LINE_MOD(line_mods, l_mod, (r_entry - lm->lm_stack), 0);
			continue;
		}
		// We only have a friend on the left side. We may or may not
		// have an enemy on the right, so goto the handling for that when done.
		if(l_entry != NULL && friend_left) {
			// We need to overwrite l_entry's information, by inserting
			// a new record at that location.
			// The length will increase by 1.
			// The termination info will match on the left side, but
			// be based on the existence of r_entry on the right.
			// The delta needs to extend by one.
			// Unlike the above cases, there's nothing to mask.
			len = LME_GET_LENGTH(l_entry) + 1;
			if(len >= 6) {
				*lmods_count = l_mod;
				return is_p1 ? P1_WON : P2_WON;
			}
			score = SCORE_FOR_LEN(len) * negate;

			// Insert the new line
			entry = lm_insert(lm, l_entry->lme_key, score, len);
			lm_mark_skipped(l_entry);
			PUSH_LINE_MOD(line_mods, l_mod, (entry - lm->lm_stack), true);
			continue;
		}
		// We've got no one around, or maybe just enemies. Therefore,
		// this is a 1 long line. Create and insert it.
		// Whether we are terminated on the left and right depends on
		// whether we found any line there.
		// The tail location is arc_r, because that's where we looked to
		// the right, similar to the case above. However, unlike the case
		// above, the starting location is just the current move.
		len = 1;
		score = SCORE_FOR_LEN(len) * negate;
		// Insert the new line
		key = lm_mmkey2lmkey(home_key, j);
		entry = lm_insert(lm, key, score, len);
		PUSH_LINE_MOD(line_mods, l_mod, (entry - lm->lm_stack), false);
	}
	*lmods_count = l_mod;
	return 0;
}

void
line_mods_undo(line_map_t *lm, mod_entry_t *line_mods, int n_modified)
{
	int	i;

	for(i = n_modified-1; i >= 0; i--) {
		lm_remove(lm, &line_mods[i]);
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
do_evaluate_ahead(move_map_t *mm, move_map_t *candidate_moves, line_map_t *lm,
		  int depth, int alpha, int beta, uint64_t *best_move_out,
		  uint64_t prev_move)
{
	bool		is_p1, was_p1;
	int		i, sub_eval, look_moves, current_eval, best_score, m_c;
	uint64_t	current_move, best_move;
	mm_entry_t	*kth_largest, *entry;
	move_list_t	impact_moves;
	mod_entry_t	line_mods[12];
	move_entry_t	impact_entries[MAX_EVAL_WIDTH];
#ifdef DEBUG
	int		eval2, j;
	arc_t		arc;
	lm_key		lmkey;
	line_map_t	lm_copy, lm_copy2;
	mm_entry_t	*my_cmm, *old_cmm;
	lm_entry_t	*lm_entry1, *lm_entry2;
	move_map_t	cmm2;

	if(depth > 0) {
		lm_init(&lm_copy);
		for(i = 0; i < lm->lm_stack_size; i++) {
			lm_copy.lm_stack[i] = lm->lm_stack[i];
		}
	}
#endif

	INIT_STACK(&impact_moves, impact_entries, MAX_EVAL_WIDTH);
	was_p1 = is_p1_for_turn(mm->mm_stack_size - 1);
	is_p1 = is_p1_for_turn(mm->mm_stack_size);
	if(depth == 0) {
		current_eval = evaluate_board(mm, lm);
		m_c = 0;
	} else {
		current_eval = evaluate_board_cached(mm, lm, prev_move, was_p1,
						     line_mods, &m_c);
		DEBUG_ASSERT(m_c <= 12);
#ifdef DEBUG
		eval2 = evaluate_board(mm, &lm_copy2);
		for(i = 0; i < mm->mm_stack_size; i++) {
			if(current_eval == P1_WON || current_eval == P2_WON) {
				break;
			}
			entry = &mm->mm_stack[i];
			for(j = 0; j < 3; j++) {
				lmkey = lm_mmkey2lmkey(entry->mme_key, j);
				lm_entry1 = lm_get(lm, lmkey);
				lm_entry2 = lm_get(&lm_copy2, lmkey);
				assert((lm_entry1 == NULL) == (lm_entry2 == NULL));
				if(lm_entry1 == NULL) continue;
				assert(lm_entry1->lme_key == lm_entry2->lme_key);
				assert(lm_entry1->lme_data == lm_entry2->lme_data);
				assert(lm_entry1->lme_score == lm_entry2->lme_score);
			}
		}
#endif
	}
	if(current_eval == P1_WON || current_eval == P2_WON) {
		// If we need to return the best move, but the game is over,
		// so return -1 to indicate as such. -1 is a valid score, but
		// the top level caller expects the return value to be an error
		// code, not a score.
		if(depth == 0) {
			return -1;
		} else {
			line_mods_undo(lm, line_mods, m_c);
			return current_eval;
		}
	}
	// If this is our max depth, just return the current eval without finding anything more accurate.
	if(depth >= MAX_EVAL_DEPTH) {
		line_mods_undo(lm, line_mods, m_c);
#ifdef DEBUG
		for(i = 0; i < lm->lm_stack_size; i++) {
			DEBUG_ASSERT(lm->lm_stack[i].lme_prev == lm_copy.lm_stack[i].lme_prev);
			DEBUG_ASSERT(lm->lm_stack[i].lme_next == lm_copy.lm_stack[i].lme_next);
			DEBUG_ASSERT(lm->lm_stack[i].lme_key == lm_copy.lm_stack[i].lme_key);
			DEBUG_ASSERT(lm->lm_stack[i].lme_score == lm_copy.lm_stack[i].lme_score);
			DEBUG_ASSERT(lm->lm_stack[i].lme_data == lm_copy.lm_stack[i].lme_data);
		}
#endif
		return populate_cmm(mm, candidate_moves, lm);
	}
	current_eval = populate_cmm(mm, candidate_moves, lm);
#ifdef DEBUG
	eval2 = evaluate_board_o(mm, &cmm2);
	for(i = 0; i < candidate_moves->mm_stack_size; i++) {
		my_cmm = &candidate_moves->mm_stack[i];
		old_cmm = mm_get(&cmm2, my_cmm->mme_key);
		DEBUG_ASSERT(old_cmm != NULL);
		DEBUG_ASSERT(my_cmm->mme_key == old_cmm->mme_key);
		DEBUG_ASSERT(my_cmm->mme_value == old_cmm->mme_value);
	}
	for(i = 0; i < cmm2.mm_stack_size; i++) {
		old_cmm = &cmm2.mm_stack[i];
		my_cmm = mm_get(candidate_moves, old_cmm->mme_key);
		DEBUG_ASSERT(my_cmm != NULL);
		DEBUG_ASSERT(my_cmm->mme_key == old_cmm->mme_key);
		DEBUG_ASSERT(my_cmm->mme_value == old_cmm->mme_value);
	}
	if(depth > 0) {
		assert(current_eval == eval2);
	}
#endif
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
	DEBUG_ASSERT(impact_moves.ml_len < look_moves);
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
		mm_insert(mm, current_move, is_p1);
		sub_eval = do_evaluate_ahead(mm, candidate_moves, lm, depth+1,
					     alpha, beta, NULL, current_move);
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
		line_mods_undo(lm, line_mods, m_c);
#ifdef DEBUG
		for(i = 0; i < lm->lm_stack_size; i++) {
			DEBUG_ASSERT(lm->lm_stack[i].lme_prev == lm_copy.lm_stack[i].lme_prev);
			DEBUG_ASSERT(lm->lm_stack[i].lme_next == lm_copy.lm_stack[i].lme_next);
			DEBUG_ASSERT(lm->lm_stack[i].lme_key == lm_copy.lm_stack[i].lme_key);
			DEBUG_ASSERT(lm->lm_stack[i].lme_score == lm_copy.lm_stack[i].lme_score);
			DEBUG_ASSERT(lm->lm_stack[i].lme_data == lm_copy.lm_stack[i].lme_data);
		}
#endif
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
	line_map_t	lm;

	mm_init(&mm);
	populate_move_map(&mm, as, rs, cs, is_p1s, num_moves);
	err = do_evaluate_ahead(&mm, &candidate_moves, &lm, 0, P2_WON, P1_WON,
				&best_move, 0);
	if(err == 0) {
		mm_decode_key(best_move, &best_move_pos);
		*best_a = best_move_pos.a;
		*best_r = best_move_pos.r;
		*best_c = best_move_pos.c;
		return true;
	} else {
		return false;
	}
}

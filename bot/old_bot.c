#include <stdbool.h>
#include <stdio.h>
#include "move_map.h"
#include "line_map.h"
#include "bot_util.h"
#include "old_bot.h"
#include "bot_params.h"

#define P1_WON		262144
#define P2_WON		-262144

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

bool
is_p1_for_turn_o(int num_moves)
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
step_l_o(arc_t *arc, arc_t *darc)
{
	darc->a = arc->a;
	darc->r = arc->r;
	darc->c = arc->c - 1;
}

void
step_r_o(arc_t *arc, arc_t *darc)
{
	darc->a = arc->a;
	darc->r = arc->r;
	darc->c = arc->c + 1;
}

void
step_dl_o(arc_t *arc, arc_t *darc)
{
	darc->a = !arc->a;
	darc->r = arc->r + (arc->a & 1);
	darc->c = arc->c - (darc->a & 1);
}

void
step_dr_o(arc_t *arc, arc_t *darc)
{
	darc->a = !arc->a;
	darc->r = arc->r + (arc->a & 1);
	darc->c = arc->c + (arc->a & 1);
}

void
step_ul_o(arc_t *arc, arc_t *darc)
{
	darc->a = !arc->a;
	darc->r = arc->r - (darc->a & 1);
	darc->c = arc->c - (darc->a & 1);
}

void
step_ur_o(arc_t *arc, arc_t *darc)
{
	darc->a = !arc->a;
	darc->r = arc->r - (darc->a & 1);
	darc->c = arc->c + (arc->a & 1);
}

int
score_line_o(move_map_t *mm, mm_entry_t *entry, arc_t *arc, int *score,
	   uint64_t *move1, uint64_t *move2, void (*step)(arc_t *, arc_t *),
	   void (*step_r_oev)(arc_t *, arc_t *))
{
	int		i, negate_score, l_score;
	bool		is_p1, terminated;
	arc_t		cur_arc, prev_arc, next_arc;
	uint64_t	prev_key, next_key;
	mm_entry_t	*prev_entry, *next_entry;

	is_p1 = entry->mme_value;
	step_r_oev(arc, &prev_arc);
	prev_key = mm_arc2key(&prev_arc);
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
		step(&cur_arc, &next_arc);
		next_key = mm_arc2key(&next_arc);
		next_entry = mm_get(mm, next_key);
		// If we've reached the end of the line, and there's nothing blocking it,
		// then we can return the score (doubled if not terminated on the prev side)
		if(next_entry == NULL) {
			// We want to make high scores much more desirable. Otherwise, points of interest,
			// a.k.a. locations with many nearby tiles, get overrepresented.
			l_score = SCORE_FOR_LEN(i+1);
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
			l_score = SCORE_FOR_LEN(i+1);
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
populate_move_map_o(move_map_t *mm, int* as, int* rs, int* cs, int* is_p1s,
		  int num_moves)
{
	int		i;
	arc_t		arc;
	uint64_t	key;

	for(i = 0; i < num_moves; i++) {
		arc.a = as[i];
		arc.r = rs[i];
		arc.c = cs[i];
		key = mm_arc2key(&arc);
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
evaluate_board_o(move_map_t *mm, move_map_t *candidate_moves)
{
	int 		i, j, score, mcount, total_score = 0;
	arc_t		arc;
	uint64_t	move1, move2;
	mm_entry_t	*entry, *cm_entry;

	// Clear this map
	mm_init(candidate_moves);

	for(i = 0; i < mm->mm_stack_size; i++) {
		entry = &mm->mm_stack[i];
		mm_decode_key(entry->mme_key, &arc);
		for(j = 0; j < 3; j++) {
			if(j == 0) {
				mcount = score_line_o(mm, entry, &arc, &score,
						    &move1, &move2,
						    step_r_o, step_l_o);
			} else if(j == 1) {
				mcount = score_line_o(mm, entry, &arc, &score,
						    &move1, &move2,
						    step_dr_o, step_ul_o);
			} else {
				mcount = score_line_o(mm, entry, &arc, &score,
						    &move1, &move2,
						    step_dl_o, step_ur_o);
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
look_moves_at_depth_o(int depth)
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
do_evaluate_ahead_o(move_map_t *mm, move_map_t* candidate_moves, int depth,
		  int alpha, int beta, uint64_t *best_move_out)
{
	bool		is_p1;
	int		i, sub_eval, look_moves, current_eval, best_score;
	uint64_t	current_move, best_move;
	move_list_t	impact_moves;
	move_entry_t	impact_entries[MAX_EVAL_WIDTH];
	mm_entry_t	*kth_largest, *entry;

	INIT_STACK(&impact_moves, impact_entries, MAX_EVAL_WIDTH);
	is_p1 = is_p1_for_turn_o(mm->mm_stack_size);
	current_eval = evaluate_board_o(mm, candidate_moves);
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
	look_moves = look_moves_at_depth_o(depth);
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
		sub_eval = do_evaluate_ahead_o(mm, candidate_moves, depth+1,
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
old_evaluate_ahead(int* as, int* rs, int* cs, int* is_p1s, int num_moves,
		   int* best_a, int* best_r, int* best_c)
{
	int		err;
	arc_t		best_move_pos;
	uint64_t	best_move;
	move_map_t	mm;
	move_map_t	candidate_moves;

	mm_init(&mm);
	populate_move_map_o(&mm, as, rs, cs, is_p1s, num_moves);
	err = do_evaluate_ahead_o(&mm, &candidate_moves, 0, P2_WON, P1_WON,
				&best_move);
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

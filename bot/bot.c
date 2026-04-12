#include <stdio.h>
#include "move_map.h"
#include "tile_map.h"
#include "move_heap.h"
#include "bot_util.h"
#include "bot.h"
#include "bot_params.h"

#define P_WON		262144
#define P1_WON		P_WON
#define P2_WON		-P_WON

#define NO_TERM		1
#define BOTH_TERM	2

// It's probably not as simple as changing this only.
#define WIN_LENGTH	6

arc_t	arc_sr = {0, 1, 0};
arc_t	arc_sl = {0, -1, 0};
arc_t	arc_sdr = {0, 0, 1};
arc_t	arc_sul = {-1, -1, 1};
arc_t	arc_sdl = {0, -1, 1};
arc_t	arc_sur = {-1, 0, 1};

#define STEP_0		step_r
#define STEP_R0		step_l
#define STEP_1		step_dr
#define STEP_R1		step_ul
#define STEP_2		step_dl
#define STEP_R2		step_ur

#define ARC_S0		(&arc_sr)
#define ARC_SR0		(&arc_sl)
#define ARC_S1		(&arc_sdr)
#define ARC_SR1		(&arc_sul)
#define ARC_S2		(&arc_sdl)
#define ARC_SR2		(&arc_sur)

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

#define UPDATE_AXIS(score, old_score, total, s_p1, s_p2, p1_f, p2_f) ({	\
	(score).p1_f = (s_p1);						\
	(score).p2_f = (s_p2);						\
	(total) -= (old_score).p1_f;					\
	(total) += (old_score).p2_f;					\
	(total) += (s_p1);						\
	(total) -= (s_p2);						\
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

/*
 * The first turn is always p1's. Then, the players alternate, getting two turns
 * in a row each.
 */

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

// Populate the move map with the given list of moves.
// The value of each entry is just 1 for p1 and -1 for p2.
void
populate_tile_map(tile_map_t *tm, int* as, int* rs, int* cs, int* is_p1s,
		  int num_moves)
{
	int		i;
	arc_t		arc;

	for(i = 0; i < num_moves; i++) {
		arc.a = as[i];
		arc.r = rs[i];
		arc.c = cs[i];
		tm_insert(tm, &arc, is_p1s[i]);
	}
}

/*
 * Walk in the given direction until the line ends. Returns the line length.
 * Populates "enemy_end" with true if the line ends with an enemy tile. If the
 * walk is started on an empty or enemy tile, the length will be 0.
 */

int
walk_dir(tile_map_t *tm, arc_t *start, bool is_p1,
	 void (*step)(arc_t *, arc_t *), bool *enemy_end)
{
	int		length = 0;
	arc_t		cur_pos, next_pos;
	tm_entry_t	*entry;

	cur_pos = *start;
	while((entry = tm_get(tm, &cur_pos)) != NULL) {
		if(TME_IS_P1(entry) != is_p1) {
			break;
		}
		length++;
		step(&cur_pos, &next_pos);
		cur_pos = next_pos;
	}
	*enemy_end = entry != NULL;
	return length;
}

/*
 * Walk in the given direction until an enemy tile is reached, or until max.
 * Returns the number of friendly tiles found along the way. This count includes
 * the starting position (or ends at the starting position, if it is an enemy).
 * Populates "length" with the number of tiles walked.
 */

int
count_dir(tile_map_t *tm, arc_t *start, bool is_p1, int max,
	  void (*step)(arc_t *, arc_t *), int* length)
{
	int		count = 0;
	arc_t		cur_pos, next_pos;
	tm_entry_t	*entry;

	cur_pos = *start;
	*length = 0;
	while(*length < max) {
		entry = tm_get(tm, &cur_pos);
		if(entry != NULL && TME_IS_P1(entry) != is_p1) {
			break;
		}
		count++;
		step(&cur_pos, &next_pos);
		cur_pos = next_pos;
		(*length)++;
	}
	return count;
}

/*
 * Score a given move along the given direction. The score is essentially the
 * marginal value provided by this move along this axis.
 * First, we determine score A, which is the number of friendly tiles within
 * 6 tiles in both directions, or until an enemy tile is reached.
 * Second, we determine score B, based on how long the line in a row will be
 * with this move. Score B is found from this length via the bot params. This is
 * doubled if neither side of the line is terminated by an enemy tile.
 * Score C = (Score A + Score B). Score C will be set to 0 if this player
 * does not have enough space to make 6 in a row in this line.
 *
 * step and step_rev should be function pointers to step forward and step in
 * reverse, respectively.
 */

int
score_move(tile_map_t *tm, arc_t *move, bool is_p1,
	   void (*step)(arc_t *, arc_t *), void (*step_rev)(arc_t *, arc_t *))
{
	int	score_a, score_b, len_r, len_l, len, count;
	bool	en_end_r, en_end_l;
	arc_t	start;

	DEBUG_ASSERT(tm_get(tm, move) == NULL);
	count = count_dir(tm, move, is_p1, WIN_LENGTH, step, &len_r);
	count += count_dir(tm, move, is_p1, WIN_LENGTH, step_rev, &len_l);
	// Check if there isn't enough space to make a winning N-in-a-row along
	// this axis. Return 0 if so.
	if(len_l + len_r - 1 < WIN_LENGTH) {
		return 0;
	}
	score_a = count * NEARBY_SCORE;

	step(move, &start);
	len_r = walk_dir(tm, &start, is_p1, step, &en_end_r);
	step_rev(move, &start);
	len_l = walk_dir(tm, &start, is_p1, step_rev, &en_end_l);
	// We do not expect to be terminated on both sides, because that should
	// be caught above by the not enough space check. If we are, it must be
	// because this is a winning move.
	DEBUG_ASSERT(!(en_end_r && en_end_l) ||
		     len_r + len_l + 1 >= WIN_LENGTH);
	len = len_l + len_r + 1;
	if(len >= WIN_LENGTH) {
		return P_WON;
	}
	score_b = SCORE_FOR_LEN(len);
	if(!en_end_r && !en_end_l) {
		score_b *= 2;
	}

	return score_a + score_b;
}

/*
 * Score a position across all 3 axes for both players, insert it into the cmm,
 * and update the running total. Returns the updated total, or P1_WON/P2_WON.
 */

int
score_and_insert(tile_map_t *tm, move_map_t *cmm, arc_t *pos, int total)
{
	mm_score_t	score;

	DEBUG_ASSERT(tm_get(tm, pos) == NULL);
	DEBUG_ASSERT(mm_get(cmm, pos) == NULL);

	score.s0_p1 = score_move(tm, pos, true, step_r, step_l);
	score.s60_p1 = score_move(tm, pos, true, step_dr, step_ul);
	score.s120_p1 = score_move(tm, pos, true, step_dl, step_ur);
	if(score.s0_p1 == P_WON || score.s60_p1 == P_WON ||
	   score.s120_p1 == P_WON) {
		return P1_WON;
	}
	score.s0_p2 = score_move(tm, pos, false, step_r, step_l);
	score.s60_p2 = score_move(tm, pos, false, step_dr, step_ul);
	score.s120_p2 = score_move(tm, pos, false, step_dl, step_ur);
	if(score.s0_p2 == P_WON || score.s60_p2 == P_WON ||
	   score.s120_p2 == P_WON) {
		return P2_WON;
	}
	mm_insert(cmm, pos, &score);
	total += score.s0_p1 + score.s60_p1 + score.s120_p1;
	total -= score.s0_p2 + score.s60_p2 + score.s120_p2;
	return total;
}

/*
 * Populates the candidate move map. We go through every move in the tile map,
 * and look around it. For any empty tile, score it in all directions from the
 * perspective of both players. If a move has already been scored, skip it.
 * Returns the total score of the current list of moves. This is simply the sum
 * of score of all candidate moves, with p2 score being negative.
 */

int
populate_cmm(tile_map_t *tm, move_map_t *cmm)
{
	int 		i, dir, total = 0;
	arc_t		pos, cur;
	tm_entry_t	*entry;
	void		(*steps[6])(arc_t *, arc_t *) = {step_r, step_dr,
							 step_dl, step_l,
							 step_ul, step_ur};

	DEBUG_ASSERT(cmm->mm_stack_size == 0);
	for(i = 0; i < tm->tm_stack_size; i++) {
		entry = &tm->tm_stack[i];
		pos.a = TME_GET_A(entry);
		pos.r = entry->tme_r;
		pos.c = entry->tme_c;
		for(dir = 0; dir < 6; dir++) {
			steps[dir](&pos, &cur);

			// Do not try to make a move on an occupied tile
			if(tm_get(tm, &cur) != NULL) {
				continue;
			}
			// Do not re-score a move we already scored
			if(mm_get(cmm, &cur) != NULL) {
				continue;
			}
			total = score_and_insert(tm, cmm, &cur, total);
			if(total == P1_WON || total == P2_WON) {
				return total;
			}
		}
	}
	return total;
}

/*
 * Walk up to WIN_LENGTH steps from pos in the given direction. For each
 * candidate found in the cmm, re-score it along the given axis and update
 * the running total. Returns the updated total, or P1_WON/P2_WON.
 */

int
rescore_axis(tile_map_t *tm, move_map_t *cmm, arc_t *pos, int total, int dir,
	     void (*walk)(arc_t *, arc_t *),
	     void (*step)(arc_t *, arc_t *),
	     void (*step_rev)(arc_t *, arc_t *))
{
	int		j, s_p1, s_p2;
	arc_t		cur, next;
	mm_entry_t	*entry;
	mm_score_t	score, old_score;

	cur = *pos;
	for(j = 0; j < WIN_LENGTH; j++) {
		walk(&cur, &next);
		cur = next;
		entry = mm_get(cmm, &cur);
		if(entry == NULL) {
			continue;
		}
		DEBUG_ASSERT(tm_get(tm, &cur) == NULL);
		s_p1 = score_move(tm, &cur, true, step, step_rev);
		s_p2 = score_move(tm, &cur, false, step, step_rev);
		if(s_p1 == P_WON) {
			return P1_WON;
		}
		if(s_p2 == P_WON) {
			return P2_WON;
		}
		old_score = entry->mme_score;
		score = old_score;
		switch(dir) {
		case 0:
			UPDATE_AXIS(score, old_score, total,
				    s_p1, s_p2, s0_p1, s0_p2);
			break;
		case 1:
			UPDATE_AXIS(score, old_score, total,
				    s_p1, s_p2, s60_p1, s60_p2);
			break;
		case 2:
			UPDATE_AXIS(score, old_score, total,
				    s_p1, s_p2, s120_p1, s120_p2);
			break;
		}
		mm_overwrite(cmm, entry, &score);
	}
	return total;
}

/*
 * Update the candidate move map based on the given move that was made. We know
 * when a move is made that only moves along the 3 axes can be affected by the
 * change. In addition, only moves with 6 tiles along those 3 axes can be
 * affected. So all we need to do is walk along those axes, find any candidate
 * moves, and overwrite them with the new score. Also, we need to in the
 * neighborhood of the most recent move, and add any new entries to the move
 * map for valid moves around it. There may be an existing candidate move in
 * the new move's neighborhood, which will be overwritten, or no move yet,
 * meaning a new one is inserted.
 */

int
update_cmm(tile_map_t *tm, move_map_t *cmm, mm_entry_t *move, int prev_total)
{
	int		total = prev_total;
	int		dir;
	arc_t		pos, cur;
	void		(*step)(arc_t *, arc_t *);
	void		(*steps[3])(arc_t *, arc_t *) = {step_r, step_dr,
							 step_dl};
	void		(*steps_rev[3])(arc_t *, arc_t *) = {step_l, step_ul,
							     step_ur};

	DEBUG_ASSERT(MME_IS_SKIPPED(move));
	total -= move->mme_score.s0_p1 + move->mme_score.s60_p1 +
		 move->mme_score.s120_p1;
	total += move->mme_score.s0_p2 + move->mme_score.s60_p2 +
		 move->mme_score.s120_p2;
	MME_POPULATE_ARC(pos, move);

	/*
	 * For each of the 3 axes, walk up to WIN_LENGTH in both directions
	 * from the move. Re-score any existing candidate along that axis.
	 */

	for(dir = 0; dir < 3; dir++) {
		total = rescore_axis(tm, cmm, &pos, total, dir,
				     steps[dir], steps[dir], steps_rev[dir]);
		if(total == P1_WON || total == P2_WON) {
			return total;
		}
		total = rescore_axis(tm, cmm, &pos, total, dir,
				     steps_rev[dir], steps[dir],
				     steps_rev[dir]);
		if(total == P1_WON || total == P2_WON) {
			return total;
		}
	}

	/*
	 * Add new candidates from the move's immediate neighborhood.
	 * For each of the 6 neighbors, if the position is empty and not
	 * already in the cmm, score it fully and insert.
	 */

	for(dir = 0; dir < 6; dir++) {
		step = dir < 3 ? steps[dir] : steps_rev[dir - 3];

		step(&pos, &cur);
		if(tm_get(tm, &cur) != NULL) {
			continue;
		}
		if(mm_get(cmm, &cur) != NULL) {
			continue;
		}
		total = score_and_insert(tm, cmm, &cur, total);
		if(total == P1_WON || total == P2_WON) {
			return total;
		}
	}

	return total;
}

/*
 * Given a list of candidate moves in the candidate move map, find the N highest
 * impact moves, and sort them in descending order. Populate the input list,
 * sorted_moves, with these N moves. N is determined from the provided depth.
 * Returns the number of moves in sorted_moves.
 */

int
populate_sorted_moves(move_map_t *cmm, move_list_t *ml,
		      mm_entry_t **sorted_moves, int depth)
{
	int		i, look_moves;
	uint32_t	move_impact;
	mm_entry_t	*entry;

	// ml is reused between calls. Simply setting the length to 0 should
	// allow it to be safely reused.
	ml->ml_len = 0;
	// We need to sort the current candidate moves to find the top N most
	// impactful. Then we can evaluate in order of impact. First we use a
	// min heap to find this subset.
	for(i = 0; i < cmm->mm_stack_size; i++) {
		entry = &cmm->mm_stack[i];
		if(MME_IS_SKIPPED(entry)) {
			continue;
		}
		move_impact = MME_IMPACT(entry);
		if(ml->ml_len >= ml->ml_size) {
			if(move_impact <= ml->ml_moves[1].mle_score) {
				continue;
			}
			mh_pop(ml);
		}
		mh_push(ml, entry, move_impact);
	}
	// Before we mess with the min heap, lets get the number of moves we're
	// going to iterate over. In rare cases, there are fewer available moves
	// than desired at this depth, so we have to account for that.
	look_moves = look_moves_at_depth(depth);
	if(look_moves > ml->ml_len) {
		look_moves = ml->ml_len;
	}
	// Our min heap has the N highest impact moves. We need to pop each
	// element out, because it will be popped out in reverse order with the
	// least impactful first. We insert them in reverse order into the list.
	while(ml->ml_len > 0) {
		i = ml->ml_len - 1;
		sorted_moves[i] = mh_pop(ml);
	}
	return look_moves;
}

/*
 * This function chooses the best move for the current player, based on the
 * moves passed in.
 * mm and cmm are shared between calls to this function, and must be returned
 * to their original state before returning.
 */

int
do_evaluate_ahead(tile_map_t *tm, move_map_t *cmm, move_list_t *ml, int depth,
		  int alpha, int beta, arc_t *best_move_out,
		  mm_entry_t *prev_move, int prev_eval)
{
	bool		is_p1;
	int		i, best_score, current_eval, sub_eval, look_moves;
	arc_t		move, best_move;
	mm_entry_t	*sorted_moves[MAX_EVAL_WIDTH];
	uint32_t	cmm_stack_size;
	mm_entry_t	*entry;
	tm_entry_t	*tile;

	is_p1 = is_p1_for_turn(tm->tm_stack_size);
	cmm_stack_size = cmm->mm_stack_size;
	if(depth == 0) {
		current_eval = populate_cmm(tm, cmm);
	} else {
		current_eval = update_cmm(tm, cmm, prev_move, prev_eval);
	}
	if(current_eval == P1_WON || current_eval == P2_WON) {
		// If we need to return the best move, but the game is over,
		// so return -1 to indicate as such. -1 is a valid score, but
		// the top level caller expects the return value to be an error
		// code, not a score.
		if(depth == 0) {
			return -1;
		} else {
			best_score = current_eval;
			goto undo_cmm_and_exit;
		}
	}
	// If this is our max depth, just return the current eval without
	// finding anything more accurate.
	if(depth >= MAX_EVAL_DEPTH) {
		best_score = update_cmm(tm, cmm, prev_move, prev_eval);
		goto undo_cmm_and_exit;
	}
	// Get the top N moves, in sorted order (descending).
	look_moves = populate_sorted_moves(cmm, ml, sorted_moves, depth);

	// Now we need to go through the moves and evaluate them in order. The
	// moves are labeled with a score of how impactful they are. We simply
	// go through them in order of impact, and see which gives us the best
	// evaluation.
	for(i = 0; i < look_moves; i++) {
		entry = sorted_moves[i];
		DEBUG_ASSERT(!MME_IS_SKIPPED(entry));
		SET_FLAG(entry->mme_flags, MME_SKIPPED);
		MME_POPULATE_ARC(move, entry);
		tile = tm_insert(tm, &move, is_p1);
		sub_eval = do_evaluate_ahead(tm, cmm, ml, depth+1, alpha, beta,
					     NULL, entry, current_eval);
		tm_remove_entry(tm, tile);
		DEBUG_ASSERT(MME_IS_SKIPPED(entry));
		RESET_FLAG(entry->mme_flags, MME_SKIPPED);
		// If this is the first try, or this move is better for us than
		// the previous best, update the best move.
		if(i == 0 || (is_p1 && sub_eval > best_score) ||
		   (!is_p1 && sub_eval < best_score)) {
			best_score = sub_eval;
			best_move = move;
		}
		// Update alpha/beta bounds and prune if the opponent can
		// already do better.
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
	}

undo_cmm_and_exit:
	mm_remove_to_length(cmm, cmm_stack_size);
	return best_score;
}

bool
evaluate_ahead(int* as, int* rs, int* cs, int* is_p1s, int num_moves,
	       int* best_a, int* best_r, int* best_c)
{
	int		err;
	arc_t		best_move;
	tile_map_t	tm;
	move_map_t	cmm;
	move_list_t	moves_list;
	move_entry_t	moves_heap[MAX_EVAL_WIDTH + 1];

	// Even though the moves_heap has MAX_EVAL_WIDTH + 1 slots, our max size
	// is actually just MAX_EVAL_WIDTH, because our min heap is 1 indexed.
	// Index 0 is left empty, which simplifies the indexing math.
	INIT_STACK(&moves_list, moves_heap, MAX_EVAL_WIDTH);

	tm_init(&tm);
	mm_init(&cmm);
	populate_tile_map(&tm, as, rs, cs, is_p1s, num_moves);
	err = do_evaluate_ahead(&tm, &cmm, &moves_list, 0, P2_WON, P1_WON,
				&best_move, NULL, 0);
	if(err == 0) {
		*best_a = best_move.a;
		*best_r = best_move.r;
		*best_c = best_move.c;
		return true;
	} else {
		return false;
	}
}

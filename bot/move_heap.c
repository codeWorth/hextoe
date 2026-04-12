#include "move_heap.h"

// Courtesy of rosetta code. Min heap push and pop
void
mh_push(move_list_t *mh, mm_entry_t *entry, uint32_t score)
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
	mh->ml_moves[i].mle_mm_entry = entry;
	mh->ml_moves[i].mle_score = score;
	mh->ml_len++;
}

mm_entry_t *
mh_pop(move_list_t *mh) {
	int		i, j, k;
	mm_entry_t	*entry;

	assert(mh->ml_len > 0);
	entry = mh->ml_moves[1].mle_mm_entry;
	mh->ml_moves[1] = mh->ml_moves[mh->ml_len];
	mh->ml_len--;
	i = 1;
	while(i != mh->ml_len + 1) {
		k = mh->ml_len + 1;
		j = 2 * i;
		if(j <= mh->ml_len &&
		   mh->ml_moves[j].mle_score < mh->ml_moves[k].mle_score) {
			k = j;
		}
		if(j + 1 <= mh->ml_len &&
		   mh->ml_moves[j + 1].mle_score < mh->ml_moves[k].mle_score) {
			k = j + 1;
		}
		mh->ml_moves[i] = mh->ml_moves[k];
		i = k;
	}
	return entry;
}

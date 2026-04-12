#include "bot_util.h"

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

// Courtesy of rosetta code. Min heap push and pop
void
mh_push(move_list_t *mh, arc_t *move, uint32_t score)
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
	mh->ml_moves[i].mle_move = *move;
	mh->ml_moves[i].mle_score = score;
	mh->ml_len++;
}

arc_t
mh_pop(move_list_t *mh) {
	int	i, j, k;
	arc_t	move;

	assert(mh->ml_len > 0);
	move = mh->ml_moves[1].mle_move;
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
	return move;
}

uint64_t
encode_arc(arc_t *arc)
{
	uint64_t	move = 0;

	move |= arc->a & 1;
	move <<= 30;
	move |= arc->r & MASK_30;
	move <<= 30;
	move |= arc->c & MASK_30;
	return move;
}

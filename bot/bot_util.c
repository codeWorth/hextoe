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

void
decode_arc(uint64_t key, arc_t *arc)
{
	arc->c = (int32_t)((key & MASK_30) << 2) >> 2;
	key >>= 30;
	arc->r = (int32_t)((key & MASK_30) << 2) >> 2;
	key >>= 30;
	arc->a = key & 1;
}

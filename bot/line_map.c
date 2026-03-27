#include "line_map.h"
#include "bot_util.h"

lm_key
lm_mmkey2lmkey(mm_key key, uint8_t dir)
{
	uint64_t	llu_dir = dir;

	// We're putting this into the top 3 bits. Technically it will only
	// take up 2 bits worth of space, but you get the point.
	llu_dir <<= 61;
	return (key & MASK_61) | llu_dir;
}

lm_key
lm_arc2key(arc_t *arc, uint8_t dir)
{

	return lm_mmkey2lmkey(mm_arc2key(arc), dir);
}

uint8_t
lm_key2dir(lm_key key)
{

	return key >> 61;
}

void
lm_decode_key(lm_key key, arc_t *arc)
{

	deocde_move(key & MASK_61, arc);
}

int
lm_key_get_a(lm_key key)
{

	return ((key >> 60) & 1);
}

void
lm_init(line_map_t *lm)
{
	int	i;

	for(i = 0; i < LM_BUCKETS; i++) {
		DLL_INIT(&lm->lm_buckets[i], lm_entry_t, lme_next, lme_prev);
	}
	lm->lm_stack_size = 0;
}

lm_entry_t *
lm_insert(line_map_t *lme, lm_key key, int16_t score, uint8_t length)
{
	lm_entry_t	*new_entry;

	// MAP_NEW_ENTRY inserts at the head, and MAP_GET searches starting from
	// the head. For this reason, we can insert duplicate entries, and the
	// most recently inserted will always be returned.
	new_entry = MAP_NEW_ENTRY(lme, lm_buckets, lm_stack, lm_stack_size, key,
				  LME_INDEX, lm_entry_t, lme_next, lme_prev);
	new_entry->lme_key = key;
	new_entry->lme_score = score;
	new_entry->lme_data = length & 0xF;
	return new_entry;
}

lm_entry_t *
lm_get(line_map_t *lm, lm_key key)
{
	lm_entry_t	*entry;

	entry = MAP_GET(lm, lm_buckets, key, LME_INDEX, lm_entry_t, lme_next,
		        (key == node->lme_key));
	if(entry != NULL && !LME_IS_SKIPPED(entry)) {
		return entry;
	} else {
		return NULL;
	}
}

void
lm_remove(line_map_t *lm, mod_entry_t *mod)
{
	lm_entry_t	*node, *next;

	node = &lm->lm_stack[mod->me_stack_index];
	if(LME_IS_SKIPPED(node)) {
		// If this element was skipped, and we were asked to remove it,
		// it really means we were asking to remove the "mask" on it.
		RESET_FLAG(node->lme_data, LM_SKIPPED);
	} else {
		// We may need to look for an element under this one to unskip.
		if(mod->me_skipped_below) {
			next = node->lme_next;
			while(next != node) {
				if(next->lme_key == node->lme_key) {
					RESET_FLAG(next->lme_data, LM_SKIPPED);
					break;
				}
				next = next->lme_next;
			}
			DEBUG_ASSERT(next != node);
		}
		// Remove this element. It needs to be the top item of the stack.
		//  Therefore, callers are required to remove in LIFO order.
		DEBUG_ASSERT(mod->me_stack_index == lm->lm_stack_size - 1);
		lm->lm_stack_size--;
		DLL_DELETE(node, lme_next, lme_prev);
	}
}

void
lm_mark_skipped(lm_entry_t *entry)
{

	SET_FLAG(entry->lme_data, LM_SKIPPED);
}

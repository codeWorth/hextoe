#include "tile_map.h"
#include "bot_util.h"

tm_hash
tm_hash_arc(arc_t *arc)
{

	return HASH_FN(encode_arc(arc));
}

void
tm_init(tile_map_t *tm)
{
	int	i;

	for(i = 0; i < TM_BUCKETS; i++) {
		DLL_INIT(&tm->tm_buckets[i], tm_entry_t, tme_next, tme_prev);
	}
	tm->tm_stack_size = 0;
}

tm_entry_t *
tm_insert(tile_map_t *tm, arc_t *arc, bool is_p1)
{
	tm_hash		hash = tm_hash_arc(arc);
	tm_entry_t	*new_entry;

	DEBUG_ASSERT(tm_get_hash(tm, hash, arc) == NULL);
	new_entry = MAP_NEW_ENTRY(tm, tm_buckets, tm_stack, tm_stack_size, hash,
				  TME_INDEX, tm_entry_t, tme_next, tme_prev);
	new_entry->tme_r = arc->r;
	new_entry->tme_c = arc->c;
	new_entry->tme_flags = 0;
	PUT_FLAG(new_entry->tme_flags, TME_A_VAL, arc->a);
	PUT_FLAG(new_entry->tme_flags, TME_P1_VAL, is_p1);
	return new_entry;
}

tm_entry_t *
tm_get(tile_map_t *tm, arc_t *arc)
{
	tm_hash	hash;

	hash = tm_hash_arc(arc);
	return tm_get_hash(tm, hash, arc);
}

tm_entry_t *
tm_get_hash(tile_map_t *tm, tm_hash hash, arc_t *arc)
{

	return MAP_GET(tm, tm_buckets, hash, TME_INDEX, tm_entry_t, tme_next,
		       (TME_GET_A(node) == arc->a && node->tme_r == arc->r &&
		        node->tme_c == arc->c));
}

void
tm_remove_entry(tile_map_t *tm, tm_entry_t *entry)
{

	// Remove this element. It needs to be the top item of the stack.
	// Therefore, callers are required to remove in LIFO order.
	DEBUG_ASSERT(entry == &tm->tm_stack[tm->tm_stack_size - 1]);
	tm->tm_stack_size--;
	DLL_DELETE(entry, tme_next, tme_prev);
}

void
tm_remove(tile_map_t *tm, arc_t *arc)
{
	tm_entry_t	*entry;

	entry = tm_get(tm, arc);
	DEBUG_ASSERT(entry != NULL);
	tm_remove_entry(tm, entry);
}

void
tm_remove_hash(tile_map_t *tm, tm_hash hash, arc_t *arc)
{
	tm_entry_t	*entry;

	entry = tm_get_hash(tm, hash, arc);
	DEBUG_ASSERT(entry != NULL);
	tm_remove_entry(tm, entry);
}

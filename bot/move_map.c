#include "move_map.h"
#include "bot_util.h"

// We need to remember to swap pointers so that the map stays valid.
// TODO: I think that isn't needed for my case.
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

mm_key
mm_arc2key(arc_t *arc)
{

	return encode_arc(arc);
}

void
mm_decode_key(mm_key key, arc_t *arc)
{

	deocde_move(key, arc);
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
mm_insert(move_map_t *mm, mm_key key, int value)
{
	mm_entry_t	*new_entry;

	new_entry = MAP_NEW_ENTRY(mm, mm_buckets, mm_stack, mm_stack_size, key,
							  MME_INDEX, mm_entry_t, mme_next, mme_prev);
	new_entry->mme_key = key;
	new_entry->mme_value = value;
	return new_entry;
}

mm_entry_t *
mm_get(move_map_t *mm, mm_key key)
{

	return MAP_GET(mm, mm_buckets, key, MME_INDEX, mm_entry_t, mme_next,
				   (key == node->mme_key));
}

void
mm_remove(move_map_t *mm, mm_key key)
{
	mm_entry_t	*node;

	node = mm_get(mm, key);
	assert(node != NULL);
	// Remove this element. It needs to be the top item of the stack. Therefore,
	// callers are required to remove in LIFO order.
	assert(node == &mm->mm_stack[mm->mm_stack_size - 1]);
	mm->mm_stack_size--;
	DLL_DELETE(node, mme_next, mme_prev);
}

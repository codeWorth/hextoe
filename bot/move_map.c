#include "move_map.h"
#include "bot_util.h"

// We need to remember to swap pointers so that the map stays valid.
void
swap_entries(mm_entry_t *entries, int i, int j)
{
	mm_entry_t	*pi, *ni, *pj, *nj;
	mm_entry_t	tmp_entry;

	if(i == j)
		return;

	pi = entries[i].mme_prev;
	ni = entries[i].mme_next;
	pj = entries[j].mme_prev;
	nj = entries[j].mme_next;

	tmp_entry = entries[i];
	entries[i] = entries[j];
	entries[j] = tmp_entry;

	if(ni == &entries[j]) {
		// i was directly before j in the DLL
		entries[j].mme_prev = pi;
		entries[j].mme_next = &entries[i];
		entries[i].mme_prev = &entries[j];
		entries[i].mme_next = nj;
		pi->mme_next = &entries[j];
		nj->mme_prev = &entries[i];
	} else if(nj == &entries[i]) {
		// j was directly before i in the DLL
		entries[i].mme_prev = pj;
		entries[i].mme_next = &entries[j];
		entries[j].mme_prev = &entries[i];
		entries[j].mme_next = ni;
		pj->mme_next = &entries[i];
		ni->mme_prev = &entries[j];
	} else {
		// Non-adjacent: fix all four external neighbors
		pi->mme_next = &entries[j];
		ni->mme_prev = &entries[j];
		entries[j].mme_next = ni;
		entries[j].mme_prev = pi;

		pj->mme_next = &entries[i];
		nj->mme_prev = &entries[i];
		entries[i].mme_next = nj;
		entries[i].mme_prev = pj;
	}
}

mm_hash
mm_hash_arc(arc_t *arc)
{

	return HASH_FN(encode_arc(arc));
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
mm_insert(move_map_t *mm, arc_t *arc, mm_score_t *score)
{
	mm_hash		hash = mm_hash_arc(arc);
	mm_entry_t	*new_entry;

	DEBUG_ASSERT(mm_get_hash(mm, hash, arc) == NULL);
	new_entry = MAP_NEW_ENTRY(mm, mm_buckets, mm_stack, mm_stack_size, hash,
				  MME_INDEX, mm_entry_t, mme_next, mme_prev);
	new_entry->mme_hash = hash;
	new_entry->mme_r = arc->r;
	new_entry->mme_c = arc->c;
	new_entry->mme_score = *score;
	new_entry->mme_flags = 0;
	PUT_FLAG(new_entry->mme_flags, MME_A_VAL, arc->a);
	return new_entry;
}

/*
 * Entry is inserted at the head of the DLL. The provided entry is marked as
 * skipped.
 */

mm_entry_t *
mm_overwrite(move_map_t *mm, mm_entry_t *entry, mm_score_t *score)
{
	mm_entry_t	*new_entry;

	DEBUG_ASSERT(!MME_IS_SKIPPED(entry));
	SET_FLAG(entry->mme_flags, MME_SKIPPED);
	new_entry = MAP_INSERT_ENTRY(mm, entry, mm_stack, mm_stack_size,
				     mm_entry_t, mme_next, mme_prev);
	new_entry->mme_hash = entry->mme_hash;
	new_entry->mme_r = entry->mme_r;
	new_entry->mme_c = entry->mme_c;
	new_entry->mme_score = *score;
	new_entry->mme_flags = entry->mme_flags & MME_A_VAL;
	return new_entry;
}

mm_entry_t *
mm_get(move_map_t *mm, arc_t *arc)
{
	mm_hash	hash;

	hash = mm_hash_arc(arc);
	return mm_get_hash(mm, hash, arc);
}

mm_entry_t *
mm_get_hash(move_map_t *mm, mm_hash hash, arc_t *arc)
{

	return MAP_GET(mm, mm_buckets, hash, MME_INDEX, mm_entry_t, mme_next,
		       (!MME_IS_SKIPPED(node) &&
		        MME_GET_A(node) == arc->a && node->mme_r == arc->r &&
		        node->mme_c == arc->c));
}

mm_entry_t *
mm_get_entry(move_map_t *mm, mm_entry_t *entry)
{
	arc_t	arc;

	arc.a = MME_GET_A(entry);
	arc.r = entry->mme_r;
	arc.c = entry->mme_c;
	return mm_get_hash(mm, entry->mme_hash, &arc);
}

void
mm_remove_entry(move_map_t *mm, mm_entry_t *entry)
{

	// Remove this element. It needs to be the top item of the stack.
	// Therefore, callers are required to remove in LIFO order.
	DEBUG_ASSERT(entry == &mm->mm_stack[mm->mm_stack_size - 1]);
	mm->mm_stack_size--;
	DLL_DELETE(entry, mme_next, mme_prev);
}

void
mm_remove(move_map_t *mm, arc_t *arc)
{
	mm_entry_t	*entry;

	entry = mm_get(mm, arc);
	DEBUG_ASSERT(entry != NULL);
	mm_remove_entry(mm, entry);
}

void
mm_remove_hash(move_map_t *mm, mm_hash hash, arc_t *arc)
{
	mm_entry_t	*entry;

	entry = mm_get_hash(mm, hash, arc);
	DEBUG_ASSERT(entry != NULL);
	mm_remove_entry(mm, entry);
}

/*
 * This function should be used to clear any modifications made on the move map
 * in a given recursive pass. The intention is that each call to this function
 * removes a "layer," so to speak. Essentially it removes the modifications from
 * that pass, but leaves the other modifications intact. It is up to the caller
 * to provide the correct length to accomplish that. As long as the caller
 * provides the length from before modifications began, this should work
 * correctly.
 */

void
mm_remove_to_length(move_map_t *mm, uint32_t length)
{
	mm_entry_t	*entry, *next_entry, *head;

	DEBUG_ASSERT(length <= mm->mm_stack_size);
	while(mm->mm_stack_size > length) {
		entry = &mm->mm_stack[mm->mm_stack_size - 1];
		next_entry = entry->mme_next;
		head = &mm->mm_buckets[MME_INDEX(entry->mme_hash)];
		mm_remove_entry(mm, entry);
		if(next_entry != head &&
		   MME_LOCATION_MATCH(entry, next_entry)) {
			DEBUG_ASSERT(MME_IS_SKIPPED(next_entry));
			RESET_FLAG(next_entry->mme_flags, MME_SKIPPED);
		} else {
			DEBUG_ASSERT(mm_get_entry(mm, entry) == NULL);
		}
	}
}
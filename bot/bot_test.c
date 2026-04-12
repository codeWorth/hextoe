#include <stdio.h>
#include <string.h>
#include <assert.h>
#include "bot.c"

#define SCORE(a, b, c, d, e, f)	\
	((mm_score_t){ .s0_p1 = a, .s60_p1 = b, .s120_p1 = c, \
		       .s0_p2 = d, .s60_p2 = e, .s120_p2 = f })

#define ARC(av, rv, cv) ((arc_t){ .a = av, .r = rv, .c = cv })

#define PASS(name) printf("  PASS: %s\n", name)

/* ================================================================== */
/* move_map tests                                                     */
/* ================================================================== */

static move_map_t mm;

/* ------------------------------------------------------------------ */
/* mm_init                                                            */
/* ------------------------------------------------------------------ */

static void
test_init_buckets_empty(void)
{
	int	i;

	mm_init(&mm);
	for(i = 0; i < MM_BUCKETS; i++) {
		assert(mm.mm_buckets[i].mme_next ==
		       (mm_entry_t *)&mm.mm_buckets[i]);
		assert(mm.mm_buckets[i].mme_prev ==
		       (mm_entry_t *)&mm.mm_buckets[i]);
	}
	PASS("init_buckets_empty");
}

static void
test_init_stack_size_zero(void)
{

	mm_init(&mm);
	assert(mm.mm_stack_size == 0);
	PASS("init_stack_size_zero");
}

/* ------------------------------------------------------------------ */
/* mm_insert                                                          */
/* ------------------------------------------------------------------ */

static void
test_insert_single(void)
{
	arc_t		arc = ARC(false, 1, 2);
	mm_score_t	score = SCORE(10, 20, 30, 40, 50, 60);
	mm_entry_t	*e;

	mm_init(&mm);
	e = mm_insert(&mm, &arc, &score);

	assert(e != NULL);
	assert(e->mme_r == 1);
	assert(e->mme_c == 2);
	assert(!MME_GET_A(e));
	assert(e->mme_score.s0_p1 == 10);
	assert(e->mme_score.s60_p1 == 20);
	assert(e->mme_score.s120_p1 == 30);
	assert(e->mme_score.s0_p2 == 40);
	assert(e->mme_score.s60_p2 == 50);
	assert(e->mme_score.s120_p2 == 60);
	assert(!MME_IS_SKIPPED(e));
	assert(mm.mm_stack_size == 1);
	PASS("insert_single");
}

static void
test_insert_different_buckets(void)
{
	arc_t		a1 = ARC(false, 1, 2);
	arc_t		a2 = ARC(true, 100, 200);
	mm_score_t	s1 = SCORE(1, 0, 0, 0, 0, 0);
	mm_score_t	s2 = SCORE(2, 0, 0, 0, 0, 0);
	mm_entry_t	*e1, *e2;

	mm_init(&mm);
	e1 = mm_insert(&mm, &a1, &s1);
	e2 = mm_insert(&mm, &a2, &s2);

	assert(e1 != e2);
	assert(mm_get(&mm, &a1) == e1);
	assert(mm_get(&mm, &a2) == e2);
	assert(mm.mm_stack_size == 2);
	PASS("insert_different_buckets");
}

static void
test_insert_same_bucket(void)
{
	/*
	 * Two arcs that collide on the same bucket index. Since the bucket
	 * index is hash & 0x1FF (9 bits), we just need two distinct arcs
	 * whose hashes share the low 9 bits. We brute-force find a pair.
	 */
	arc_t		a1 = ARC(false, 0, 0);
	mm_hash		h1 = mm_hash_arc(&a1);
	uint32_t	bucket = MME_INDEX(h1);
	arc_t		a2;
	mm_hash		h2;
	int		r;
	mm_score_t	s1 = SCORE(1, 0, 0, 0, 0, 0);
	mm_score_t	s2 = SCORE(2, 0, 0, 0, 0, 0);
	mm_entry_t	*e1, *e2;

	/* Find a second arc that lands in the same bucket */
	for(r = 1; r < 10000; r++) {
		a2 = ARC(false, r, r);
		h2 = mm_hash_arc(&a2);
		if(MME_INDEX(h2) == bucket)
			break;
	}
	assert(MME_INDEX(h2) == bucket);

	mm_init(&mm);
	e1 = mm_insert(&mm, &a1, &s1);
	e2 = mm_insert(&mm, &a2, &s2);

	assert(mm_get(&mm, &a1) == e1);
	assert(mm_get(&mm, &a2) == e2);
	assert(e1->mme_score.s0_p1 == 1);
	assert(e2->mme_score.s0_p1 == 2);
	PASS("insert_same_bucket");
}

static void
test_insert_stack_size(void)
{
	mm_score_t	s = SCORE(0, 0, 0, 0, 0, 0);
	arc_t		a;
	int		i;

	mm_init(&mm);
	for(i = 0; i < 10; i++) {
		a = ARC(false, i, i);
		mm_insert(&mm, &a, &s);
		assert(mm.mm_stack_size == (uint32_t)(i + 1));
	}
	PASS("insert_stack_size");
}

/* ------------------------------------------------------------------ */
/* mm_get / mm_get_hash                                               */
/* ------------------------------------------------------------------ */

static void
test_get_exists(void)
{
	arc_t		arc = ARC(true, 5, 10);
	mm_score_t	s = SCORE(7, 0, 0, 0, 0, 0);
	mm_entry_t	*e;

	mm_init(&mm);
	mm_insert(&mm, &arc, &s);
	e = mm_get(&mm, &arc);
	assert(e != NULL);
	assert(e->mme_r == 5);
	assert(e->mme_c == 10);
	assert(MME_GET_A(e));
	PASS("get_exists");
}

static void
test_get_missing(void)
{
	arc_t		a1 = ARC(false, 1, 2);
	arc_t		a2 = ARC(false, 99, 99);
	mm_score_t	s = SCORE(0, 0, 0, 0, 0, 0);

	mm_init(&mm);
	mm_insert(&mm, &a1, &s);
	assert(mm_get(&mm, &a2) == NULL);
	PASS("get_missing");
}

static void
test_get_distinguishes_a(void)
{
	arc_t		a_false = ARC(false, 3, 4);
	arc_t		a_true  = ARC(true, 3, 4);
	mm_score_t	s1 = SCORE(1, 0, 0, 0, 0, 0);
	mm_score_t	s2 = SCORE(2, 0, 0, 0, 0, 0);
	mm_entry_t	*e1, *e2;

	mm_init(&mm);
	e1 = mm_insert(&mm, &a_false, &s1);
	e2 = mm_insert(&mm, &a_true, &s2);

	assert(mm_get(&mm, &a_false) == e1);
	assert(mm_get(&mm, &a_true) == e2);
	assert(mm_get(&mm, &a_false)->mme_score.s0_p1 == 1);
	assert(mm_get(&mm, &a_true)->mme_score.s0_p1 == 2);
	PASS("get_distinguishes_a");
}

static void
test_get_hash_direct(void)
{
	arc_t		arc = ARC(false, 7, 8);
	mm_score_t	s = SCORE(0, 0, 0, 0, 0, 0);
	mm_hash		hash;
	mm_entry_t	*e;

	mm_init(&mm);
	mm_insert(&mm, &arc, &s);
	hash = mm_hash_arc(&arc);
	e = mm_get_hash(&mm, hash, &arc);
	assert(e != NULL);
	assert(e->mme_r == 7);
	assert(e->mme_c == 8);
	PASS("get_hash_direct");
}

/* ------------------------------------------------------------------ */
/* mm_overwrite                                                       */
/* ------------------------------------------------------------------ */

static void
test_overwrite_updates_score(void)
{
	arc_t		arc = ARC(false, 1, 1);
	mm_score_t	s1 = SCORE(10, 0, 0, 0, 0, 0);
	mm_score_t	s2 = SCORE(99, 0, 0, 0, 0, 0);
	mm_entry_t	*orig, *ow;

	mm_init(&mm);
	orig = mm_insert(&mm, &arc, &s1);
	ow = mm_overwrite(&mm, orig, &s2);

	assert(ow != orig);
	assert(ow->mme_score.s0_p1 == 99);
	PASS("overwrite_updates_score");
}

static void
test_overwrite_marks_skipped(void)
{
	arc_t		arc = ARC(true, 2, 3);
	mm_score_t	s1 = SCORE(1, 0, 0, 0, 0, 0);
	mm_score_t	s2 = SCORE(2, 0, 0, 0, 0, 0);
	mm_entry_t	*orig;

	mm_init(&mm);
	orig = mm_insert(&mm, &arc, &s1);
	mm_overwrite(&mm, orig, &s2);

	assert(MME_IS_SKIPPED(orig));
	PASS("overwrite_marks_skipped");
}

static void
test_overwrite_get_returns_new(void)
{
	arc_t		arc = ARC(false, 5, 5);
	mm_score_t	s1 = SCORE(1, 0, 0, 0, 0, 0);
	mm_score_t	s2 = SCORE(2, 0, 0, 0, 0, 0);
	mm_entry_t	*orig, *ow, *found;

	mm_init(&mm);
	orig = mm_insert(&mm, &arc, &s1);
	ow = mm_overwrite(&mm, orig, &s2);
	found = mm_get(&mm, &arc);

	/* mm_get walks the DLL and finds the first match; the new entry was
	 * prepended before the old one, so it should be found first. */
	assert(found == ow);
	assert(!MME_IS_SKIPPED(found));
	assert(found->mme_score.s0_p1 == 2);
	PASS("overwrite_get_returns_new");
}

static void
test_overwrite_preserves_a(void)
{
	arc_t		arc = ARC(true, 4, 4);
	mm_score_t	s1 = SCORE(0, 0, 0, 0, 0, 0);
	mm_score_t	s2 = SCORE(0, 0, 0, 0, 0, 0);
	mm_entry_t	*orig, *ow;

	mm_init(&mm);
	orig = mm_insert(&mm, &arc, &s1);
	ow = mm_overwrite(&mm, orig, &s2);

	assert(MME_GET_A(ow));
	assert(ow->mme_r == orig->mme_r);
	assert(ow->mme_c == orig->mme_c);
	PASS("overwrite_preserves_a");
}

/* ------------------------------------------------------------------ */
/* mm_remove_entry                                                    */
/* ------------------------------------------------------------------ */

static void
test_remove_entry_decrements_stack(void)
{
	arc_t		arc = ARC(false, 0, 0);
	mm_score_t	s = SCORE(0, 0, 0, 0, 0, 0);
	mm_entry_t	*e;

	mm_init(&mm);
	e = mm_insert(&mm, &arc, &s);
	assert(mm.mm_stack_size == 1);
	mm_remove_entry(&mm, e);
	assert(mm.mm_stack_size == 0);
	PASS("remove_entry_decrements_stack");
}

static void
test_remove_entry_unlinks(void)
{
	arc_t		arc = ARC(false, 0, 0);
	mm_score_t	s = SCORE(0, 0, 0, 0, 0, 0);
	mm_entry_t	*e;

	mm_init(&mm);
	e = mm_insert(&mm, &arc, &s);
	mm_remove_entry(&mm, e);
	assert(mm_get(&mm, &arc) == NULL);
	PASS("remove_entry_unlinks");
}

/* ------------------------------------------------------------------ */
/* mm_remove / mm_remove_hash                                         */
/* ------------------------------------------------------------------ */

static void
test_remove_by_arc(void)
{
	arc_t		arc = ARC(false, 10, 20);
	mm_score_t	s = SCORE(0, 0, 0, 0, 0, 0);

	mm_init(&mm);
	mm_insert(&mm, &arc, &s);
	assert(mm_get(&mm, &arc) != NULL);
	mm_remove(&mm, &arc);
	assert(mm_get(&mm, &arc) == NULL);
	assert(mm.mm_stack_size == 0);
	PASS("remove_by_arc");
}

static void
test_remove_by_hash(void)
{
	arc_t		arc = ARC(true, 11, 22);
	mm_score_t	s = SCORE(0, 0, 0, 0, 0, 0);
	mm_hash		hash;

	mm_init(&mm);
	mm_insert(&mm, &arc, &s);
	hash = mm_hash_arc(&arc);
	mm_remove_hash(&mm, hash, &arc);
	assert(mm_get(&mm, &arc) == NULL);
	assert(mm.mm_stack_size == 0);
	PASS("remove_by_hash");
}

/* ------------------------------------------------------------------ */
/* mm_remove_to_length                                                */
/* ------------------------------------------------------------------ */

static void
test_remove_to_length_basic(void)
{
	mm_score_t	s = SCORE(0, 0, 0, 0, 0, 0);
	arc_t		arcs[5];
	int		i;

	mm_init(&mm);
	for(i = 0; i < 5; i++) {
		arcs[i] = ARC(false, i, i + 10);
		mm_insert(&mm, &arcs[i], &s);
	}
	assert(mm.mm_stack_size == 5);

	mm_remove_to_length(&mm, 2);
	assert(mm.mm_stack_size == 2);

	/* First two remain */
	assert(mm_get(&mm, &arcs[0]) != NULL);
	assert(mm_get(&mm, &arcs[1]) != NULL);
	/* Last three are gone */
	assert(mm_get(&mm, &arcs[2]) == NULL);
	assert(mm_get(&mm, &arcs[3]) == NULL);
	assert(mm_get(&mm, &arcs[4]) == NULL);
	PASS("remove_to_length_basic");
}

static void
test_remove_to_length_single_overwrite(void)
{
	arc_t		arc = ARC(false, 1, 1);
	mm_score_t	s1 = SCORE(10, 0, 0, 0, 0, 0);
	mm_score_t	s2 = SCORE(20, 0, 0, 0, 0, 0);
	mm_entry_t	*orig, *ow, *found;
	uint32_t	len_after_insert;

	mm_init(&mm);
	orig = mm_insert(&mm, &arc, &s1);
	len_after_insert = mm.mm_stack_size;

	ow = mm_overwrite(&mm, orig, &s2);
	found = mm_get(&mm, &arc);
	assert(found == ow);
	assert(found->mme_score.s0_p1 == 20);

	/* Peel back the overwrite */
	mm_remove_to_length(&mm, len_after_insert);

	found = mm_get(&mm, &arc);
	assert(found == orig);
	assert(!MME_IS_SKIPPED(found));
	assert(found->mme_score.s0_p1 == 10);
	PASS("remove_to_length_single_overwrite");
}

static void
test_remove_to_length_double_overwrite(void)
{
	arc_t		arc = ARC(true, 7, 7);
	mm_score_t	s1 = SCORE(100, 0, 0, 0, 0, 0);
	mm_score_t	s2 = SCORE(200, 0, 0, 0, 0, 0);
	mm_score_t	s3 = SCORE(300, 0, 0, 0, 0, 0);
	mm_entry_t	*e1, *e2, *e3, *found;
	uint32_t	len0, len1;

	mm_init(&mm);

	/* Original insert */
	e1 = mm_insert(&mm, &arc, &s1);
	len0 = mm.mm_stack_size;		/* 1 */

	/* First overwrite */
	e2 = mm_overwrite(&mm, e1, &s2);
	len1 = mm.mm_stack_size;		/* 2 */

	/* Second overwrite */
	e3 = mm_overwrite(&mm, e2, &s3);

	/* Currently visible: 3rd value (2nd overwrite) */
	found = mm_get(&mm, &arc);
	assert(found == e3);
	assert(found->mme_score.s0_p1 == 300);
	assert(!MME_IS_SKIPPED(found));
	assert(MME_IS_SKIPPED(e2));
	assert(MME_IS_SKIPPED(e1));

	/* Peel back to len1: 2nd value (1st overwrite) should be visible */
	mm_remove_to_length(&mm, len1);
	found = mm_get(&mm, &arc);
	assert(found == e2);
	assert(found->mme_score.s0_p1 == 200);
	assert(!MME_IS_SKIPPED(found));
	assert(MME_IS_SKIPPED(e1));

	/* Peel back to len0: original value should be visible */
	mm_remove_to_length(&mm, len0);
	found = mm_get(&mm, &arc);
	assert(found == e1);
	assert(found->mme_score.s0_p1 == 100);
	assert(!MME_IS_SKIPPED(found));
	PASS("remove_to_length_double_overwrite");
}

static void
test_remove_to_length_zero(void)
{
	mm_score_t	s = SCORE(0, 0, 0, 0, 0, 0);
	arc_t		a1 = ARC(false, 0, 0);
	arc_t		a2 = ARC(true, 1, 1);

	mm_init(&mm);
	mm_insert(&mm, &a1, &s);
	mm_insert(&mm, &a2, &s);
	assert(mm.mm_stack_size == 2);

	mm_remove_to_length(&mm, 0);
	assert(mm.mm_stack_size == 0);
	assert(mm_get(&mm, &a1) == NULL);
	assert(mm_get(&mm, &a2) == NULL);
	PASS("remove_to_length_zero");
}

/* ------------------------------------------------------------------ */
/* move_map macros                                                    */
/* ------------------------------------------------------------------ */

static void
test_mme_impact(void)
{
	mm_entry_t	e;

	memset(&e, 0, sizeof(e));
	e.mme_score = SCORE(1, 2, 3, 4, 5, 6);
	assert(MME_IMPACT(&e) == 21);

	e.mme_score = SCORE(0, 0, 0, 0, 0, 0);
	assert(MME_IMPACT(&e) == 0);
	PASS("mme_impact");
}

static void
test_mme_location_match(void)
{
	mm_entry_t	e1, e2;

	memset(&e1, 0, sizeof(e1));
	memset(&e2, 0, sizeof(e2));

	/* Identical entries match */
	e1.mme_hash = 42;
	e1.mme_r = 5;
	e1.mme_c = 10;
	e1.mme_flags = 0;
	e2 = e1;
	assert(MME_LOCATION_MATCH(&e1, &e2));

	/* Different hash */
	e2 = e1;
	e2.mme_hash = 99;
	assert(!MME_LOCATION_MATCH(&e1, &e2));

	/* Different r */
	e2 = e1;
	e2.mme_r = 6;
	assert(!MME_LOCATION_MATCH(&e1, &e2));

	/* Different c */
	e2 = e1;
	e2.mme_c = 11;
	assert(!MME_LOCATION_MATCH(&e1, &e2));

	/* Different a flag */
	e2 = e1;
	SET_FLAG(e2.mme_flags, MME_A_VAL);
	assert(!MME_LOCATION_MATCH(&e1, &e2));
	PASS("mme_location_match");
}

/* ================================================================== */
/* tile_map tests                                                     */
/* ================================================================== */

static tile_map_t tm;

/* ------------------------------------------------------------------ */
/* tm_init                                                            */
/* ------------------------------------------------------------------ */

static void
test_tm_init_buckets_empty(void)
{
	int	i;

	tm_init(&tm);
	for(i = 0; i < TM_BUCKETS; i++) {
		assert(tm.tm_buckets[i].tme_next ==
		       (tm_entry_t *)&tm.tm_buckets[i]);
		assert(tm.tm_buckets[i].tme_prev ==
		       (tm_entry_t *)&tm.tm_buckets[i]);
	}
	PASS("tm_init_buckets_empty");
}

static void
test_tm_init_stack_size_zero(void)
{

	tm_init(&tm);
	assert(tm.tm_stack_size == 0);
	PASS("tm_init_stack_size_zero");
}

/* ------------------------------------------------------------------ */
/* tm_insert                                                          */
/* ------------------------------------------------------------------ */

static void
test_tm_insert_single(void)
{
	arc_t		arc = ARC(false, 1, 2);
	tm_entry_t	*e;

	tm_init(&tm);
	e = tm_insert(&tm, &arc, true);

	assert(e != NULL);
	assert(e->tme_r == 1);
	assert(e->tme_c == 2);
	assert(!TME_GET_A(e));
	assert(TME_IS_P1(e));
	assert(tm.tm_stack_size == 1);
	PASS("tm_insert_single");
}

static void
test_tm_insert_p1_p2(void)
{
	arc_t		a1 = ARC(false, 1, 1);
	arc_t		a2 = ARC(false, 2, 2);
	tm_entry_t	*e1, *e2;

	tm_init(&tm);
	e1 = tm_insert(&tm, &a1, true);
	e2 = tm_insert(&tm, &a2, false);

	assert(TME_IS_P1(e1));
	assert(!TME_IS_P1(e2));
	PASS("tm_insert_p1_p2");
}

static void
test_tm_insert_different_buckets(void)
{
	arc_t		a1 = ARC(false, 1, 2);
	arc_t		a2 = ARC(true, 100, 200);
	tm_entry_t	*e1, *e2;

	tm_init(&tm);
	e1 = tm_insert(&tm, &a1, true);
	e2 = tm_insert(&tm, &a2, false);

	assert(e1 != e2);
	assert(tm_get(&tm, &a1) == e1);
	assert(tm_get(&tm, &a2) == e2);
	assert(tm.tm_stack_size == 2);
	PASS("tm_insert_different_buckets");
}

static void
test_tm_insert_same_bucket(void)
{
	arc_t		a1 = ARC(false, 0, 0);
	tm_hash		h1 = tm_hash_arc(&a1);
	uint32_t	bucket = TME_INDEX(h1);
	arc_t		a2;
	tm_hash		h2;
	int		r;
	tm_entry_t	*e1, *e2;

	for(r = 1; r < 10000; r++) {
		a2 = ARC(false, r, r);
		h2 = tm_hash_arc(&a2);
		if(TME_INDEX(h2) == bucket)
			break;
	}
	assert(TME_INDEX(h2) == bucket);

	tm_init(&tm);
	e1 = tm_insert(&tm, &a1, true);
	e2 = tm_insert(&tm, &a2, true);

	assert(tm_get(&tm, &a1) == e1);
	assert(tm_get(&tm, &a2) == e2);
	PASS("tm_insert_same_bucket");
}

static void
test_tm_insert_stack_size(void)
{
	arc_t	a;
	int	i;

	tm_init(&tm);
	for(i = 0; i < 10; i++) {
		a = ARC(false, i, i);
		tm_insert(&tm, &a, true);
		assert(tm.tm_stack_size == (uint32_t)(i + 1));
	}
	PASS("tm_insert_stack_size");
}

/* ------------------------------------------------------------------ */
/* tm_get / tm_get_hash                                               */
/* ------------------------------------------------------------------ */

static void
test_tm_get_exists(void)
{
	arc_t		arc = ARC(true, 5, 10);
	tm_entry_t	*e;

	tm_init(&tm);
	tm_insert(&tm, &arc, true);
	e = tm_get(&tm, &arc);
	assert(e != NULL);
	assert(e->tme_r == 5);
	assert(e->tme_c == 10);
	assert(TME_GET_A(e));
	PASS("tm_get_exists");
}

static void
test_tm_get_missing(void)
{
	arc_t	a1 = ARC(false, 1, 2);
	arc_t	a2 = ARC(false, 99, 99);

	tm_init(&tm);
	tm_insert(&tm, &a1, true);
	assert(tm_get(&tm, &a2) == NULL);
	PASS("tm_get_missing");
}

static void
test_tm_get_distinguishes_a(void)
{
	arc_t		a_false = ARC(false, 3, 4);
	arc_t		a_true  = ARC(true, 3, 4);
	tm_entry_t	*e1, *e2;

	tm_init(&tm);
	e1 = tm_insert(&tm, &a_false, true);
	e2 = tm_insert(&tm, &a_true, false);

	assert(tm_get(&tm, &a_false) == e1);
	assert(tm_get(&tm, &a_true) == e2);
	assert(TME_IS_P1(tm_get(&tm, &a_false)));
	assert(!TME_IS_P1(tm_get(&tm, &a_true)));
	PASS("tm_get_distinguishes_a");
}

static void
test_tm_get_hash_direct(void)
{
	arc_t		arc = ARC(false, 7, 8);
	tm_hash		hash;
	tm_entry_t	*e;

	tm_init(&tm);
	tm_insert(&tm, &arc, true);
	hash = tm_hash_arc(&arc);
	e = tm_get_hash(&tm, hash, &arc);
	assert(e != NULL);
	assert(e->tme_r == 7);
	assert(e->tme_c == 8);
	PASS("tm_get_hash_direct");
}

/* ------------------------------------------------------------------ */
/* tm_remove_entry                                                    */
/* ------------------------------------------------------------------ */

static void
test_tm_remove_entry_decrements_stack(void)
{
	arc_t		arc = ARC(false, 0, 0);
	tm_entry_t	*e;

	tm_init(&tm);
	e = tm_insert(&tm, &arc, true);
	assert(tm.tm_stack_size == 1);
	tm_remove_entry(&tm, e);
	assert(tm.tm_stack_size == 0);
	PASS("tm_remove_entry_decrements_stack");
}

static void
test_tm_remove_entry_unlinks(void)
{
	arc_t		arc = ARC(false, 0, 0);
	tm_entry_t	*e;

	tm_init(&tm);
	e = tm_insert(&tm, &arc, true);
	tm_remove_entry(&tm, e);
	assert(tm_get(&tm, &arc) == NULL);
	PASS("tm_remove_entry_unlinks");
}

/* ------------------------------------------------------------------ */
/* tm_remove / tm_remove_hash                                         */
/* ------------------------------------------------------------------ */

static void
test_tm_remove_by_arc(void)
{
	arc_t	arc = ARC(false, 10, 20);

	tm_init(&tm);
	tm_insert(&tm, &arc, true);
	assert(tm_get(&tm, &arc) != NULL);
	tm_remove(&tm, &arc);
	assert(tm_get(&tm, &arc) == NULL);
	assert(tm.tm_stack_size == 0);
	PASS("tm_remove_by_arc");
}

static void
test_tm_remove_by_hash(void)
{
	arc_t		arc = ARC(true, 11, 22);
	tm_hash		hash;

	tm_init(&tm);
	tm_insert(&tm, &arc, false);
	hash = tm_hash_arc(&arc);
	tm_remove_hash(&tm, hash, &arc);
	assert(tm_get(&tm, &arc) == NULL);
	assert(tm.tm_stack_size == 0);
	PASS("tm_remove_by_hash");
}

/* ------------------------------------------------------------------ */
/* tile_map macros                                                    */
/* ------------------------------------------------------------------ */

static void
test_tme_location_match(void)
{
	tm_entry_t	e1, e2;

	memset(&e1, 0, sizeof(e1));
	memset(&e2, 0, sizeof(e2));

	/* Identical entries match */
	e1.tme_hash = 42;
	e1.tme_r = 5;
	e1.tme_c = 10;
	e1.tme_flags = 0;
	e2 = e1;
	assert(TME_LOCATION_MATCH(&e1, &e2));

	/* Different hash */
	e2 = e1;
	e2.tme_hash = 99;
	assert(!TME_LOCATION_MATCH(&e1, &e2));

	/* Different r */
	e2 = e1;
	e2.tme_r = 6;
	assert(!TME_LOCATION_MATCH(&e1, &e2));

	/* Different c */
	e2 = e1;
	e2.tme_c = 11;
	assert(!TME_LOCATION_MATCH(&e1, &e2));

	/* Different a flag */
	e2 = e1;
	SET_FLAG(e2.tme_flags, TME_A_VAL);
	assert(!TME_LOCATION_MATCH(&e1, &e2));
	PASS("tme_location_match");
}

/* ================================================================== */
/* min heap tests                                                     */
/* ================================================================== */

#define MH_SIZE 64

static move_entry_t	mh_buf[MH_SIZE + 1]; /* 1-indexed */
static move_list_t	mh;
static move_map_t	mh_cmm; /* backing cmm to provide mm_entry_t pointers */

static void
mh_reset(void)
{

	memset(mh_buf, 0, sizeof(mh_buf));
	mh.ml_moves = mh_buf;
	mh.ml_size = MH_SIZE;
	mh.ml_len = 0;
	mm_init(&mh_cmm);
}

/* Helper: insert into mh_cmm and return the mm_entry_t pointer */
static mm_entry_t *
mh_make_entry(int a, int r, int c)
{
	arc_t		arc = ARC(a, r, c);
	mm_score_t	s = SCORE(0, 0, 0, 0, 0, 0);

	return mm_insert(&mh_cmm, &arc, &s);
}

/* ------------------------------------------------------------------ */
/* mh_push                                                            */
/* ------------------------------------------------------------------ */

static void
test_push_single(void)
{
	mm_entry_t	*e;

	mh_reset();
	e = mh_make_entry(false, 1, 2);
	mh_push(&mh, e, 42);
	assert(mh.ml_len == 1);
	assert(mh_buf[1].mle_score == 42);
	assert(mh_buf[1].mle_mm_entry->mme_r == 1);
	assert(mh_buf[1].mle_mm_entry->mme_c == 2);
	PASS("push_single");
}

static void
test_push_descending_min_at_top(void)
{
	int	i;

	mh_reset();
	/* Push 10, 9, 8, ..., 1 — each new element is smaller */
	for(i = 10; i >= 1; i--) {
		mm_entry_t *e = mh_make_entry(false, i, i);
		mh_push(&mh, e, (uint32_t)i);
	}
	assert(mh.ml_len == 10);
	assert(mh_buf[1].mle_score == 1);
	PASS("push_descending_min_at_top");
}

/* ------------------------------------------------------------------ */
/* mh_pop                                                             */
/* ------------------------------------------------------------------ */

static void
test_pop_single(void)
{
	mm_entry_t	*e, *out;

	mh_reset();
	e = mh_make_entry(true, 5, 10);
	mh_push(&mh, e, 7);
	out = mh_pop(&mh);
	assert(mh.ml_len == 0);
	assert(out->mme_r == 5);
	assert(out->mme_c == 10);
	assert(MME_GET_A(out));
	PASS("pop_single");
}

static void
test_pop_returns_min(void)
{
	mm_entry_t	*out;

	mh_reset();
	mh_push(&mh, mh_make_entry(false, 0, 0), 50);
	mh_push(&mh, mh_make_entry(false, 1, 1), 10);
	mh_push(&mh, mh_make_entry(false, 2, 2), 30);

	out = mh_pop(&mh);
	assert(out->mme_r == 1 && out->mme_c == 1); /* score 10 */
	assert(mh.ml_len == 2);
	PASS("pop_returns_min");
}

static void
test_pop_all_ascending(void)
{
	uint32_t	scores[] = {50, 10, 80, 5, 30, 90, 1, 60, 25, 15};
	int		n = sizeof(scores) / sizeof(scores[0]);
	uint32_t	prev;
	int		i;

	mh_reset();
	for(i = 0; i < n; i++) {
		mm_entry_t *e = mh_make_entry(false, i, i);
		mh_push(&mh, e, scores[i]);
	}
	assert(mh.ml_len == n);

	prev = 0;
	for(i = 0; i < n; i++) {
		mm_entry_t *out = mh_pop(&mh);
		/* We can identify which score this was by r == original index */
		uint32_t score = scores[out->mme_r];
		assert(score >= prev);
		prev = score;
	}
	assert(mh.ml_len == 0);
	PASS("pop_all_ascending");
}

static void
test_pop_equal_scores(void)
{
	int	i;

	mh_reset();
	for(i = 0; i < 5; i++) {
		mh_push(&mh, mh_make_entry(false, i, 0), 42);
	}
	assert(mh.ml_len == 5);

	for(i = 0; i < 5; i++) {
		mh_pop(&mh);
	}
	assert(mh.ml_len == 0);
	PASS("pop_equal_scores");
}

/* ------------------------------------------------------------------ */
/* interleaved push/pop                                               */
/* ------------------------------------------------------------------ */

static void
test_interleaved(void)
{
	mm_entry_t	*out;

	mh_reset();

	mh_push(&mh, mh_make_entry(false, 0, 0), 30);
	mh_push(&mh, mh_make_entry(false, 1, 0), 10);
	mh_push(&mh, mh_make_entry(false, 2, 0), 20);

	/* Pop min (10) */
	out = mh_pop(&mh);
	assert(out->mme_r == 1);

	/* Push something smaller than remaining */
	mh_push(&mh, mh_make_entry(false, 3, 0), 5);

	/* Pop min (5) */
	out = mh_pop(&mh);
	assert(out->mme_r == 3);

	/* Pop min (20) */
	out = mh_pop(&mh);
	assert(out->mme_r == 2);

	/* Pop min (30) */
	out = mh_pop(&mh);
	assert(out->mme_r == 0);

	assert(mh.ml_len == 0);
	PASS("interleaved");
}

/* ------------------------------------------------------------------ */
/* INIT_STACK / PUSH_STACK macros                                     */
/* ------------------------------------------------------------------ */

static void
test_init_stack(void)
{
	move_list_t	ml;
	move_entry_t	buf[16];

	INIT_STACK(&ml, buf, 16);
	assert(ml.ml_moves == buf);
	assert(ml.ml_size == 16);
	assert(ml.ml_len == 0);
	PASS("init_stack");
}

static void
test_push_stack(void)
{
	move_list_t	ml;
	move_entry_t	buf[16];
	mm_entry_t	*e;

	INIT_STACK(&ml, buf, 16);
	mm_init(&mh_cmm);

	e = mh_make_entry(false, 1, 2);
	PUSH_STACK(&ml, e, 100);
	assert(ml.ml_len == 1);
	assert(buf[0].mle_mm_entry->mme_r == 1);
	assert(buf[0].mle_mm_entry->mme_c == 2);
	assert(buf[0].mle_score == 100);

	e = mh_make_entry(true, 3, 4);
	PUSH_STACK(&ml, e, 200);
	assert(ml.ml_len == 2);
	assert(buf[1].mle_mm_entry->mme_r == 3);
	assert(buf[1].mle_score == 200);
	PASS("push_stack");
}

static void
test_heap_reuse_after_len_zero(void)
{
	mm_entry_t	*out;

	mh_reset();
	mh_push(&mh, mh_make_entry(false, 0, 0), 50);
	mh_push(&mh, mh_make_entry(false, 1, 0), 10);
	mh_push(&mh, mh_make_entry(false, 2, 0), 30);

	/* Reset by just setting length to 0 (no memset) */
	mh.ml_len = 0;

	/* Reuse: push new values (reuse mh_cmm — new entries get appended) */
	mh_push(&mh, mh_make_entry(false, 3, 0), 100);
	mh_push(&mh, mh_make_entry(false, 4, 0), 5);
	mh_push(&mh, mh_make_entry(false, 5, 0), 40);

	assert(mh.ml_len == 3);

	/* Should pop in min order: 5, 40, 100 */
	out = mh_pop(&mh);
	assert(out->mme_r == 4);
	out = mh_pop(&mh);
	assert(out->mme_r == 5);
	out = mh_pop(&mh);
	assert(out->mme_r == 3);
	assert(mh.ml_len == 0);
	PASS("heap_reuse_after_len_zero");
}

/* ================================================================== */
/* is_p1_for_turn                                                     */
/* ================================================================== */

static void
test_is_p1_for_turn_pattern(void)
{
	/* Pattern: p1, p2, p2, p1, p1, p2, p2, p1, p1 */
	bool	expected[] = {true, false, false, true, true,
			      false, false, true, true};
	int	i;

	for(i = 0; i < 9; i++) {
		assert(is_p1_for_turn(i) == expected[i]);
	}
	PASS("is_p1_for_turn_pattern");
}

/* ================================================================== */
/* step functions                                                     */
/* ================================================================== */

/* ------------------------------------------------------------------ */
/* step_r / step_l (horizontal)                                       */
/* ------------------------------------------------------------------ */

static void
test_step_r_from_a0(void)
{
	arc_t	src = ARC(false, 3, 5);
	arc_t	dst;

	step_r(&src, &dst);
	assert(dst.a == false);
	assert(dst.r == 3);
	assert(dst.c == 6);
	PASS("step_r_from_a0");
}

static void
test_step_r_from_a1(void)
{
	arc_t	src = ARC(true, 3, 5);
	arc_t	dst;

	step_r(&src, &dst);
	assert(dst.a == true);
	assert(dst.r == 3);
	assert(dst.c == 6);
	PASS("step_r_from_a1");
}

static void
test_step_l_from_a0(void)
{
	arc_t	src = ARC(false, 3, 5);
	arc_t	dst;

	step_l(&src, &dst);
	assert(dst.a == false);
	assert(dst.r == 3);
	assert(dst.c == 4);
	PASS("step_l_from_a0");
}

static void
test_step_l_from_a1(void)
{
	arc_t	src = ARC(true, 3, 5);
	arc_t	dst;

	step_l(&src, &dst);
	assert(dst.a == true);
	assert(dst.r == 3);
	assert(dst.c == 4);
	PASS("step_l_from_a1");
}

/* ------------------------------------------------------------------ */
/* step_dr / step_ul (60-degree axis)                                 */
/* ------------------------------------------------------------------ */

static void
test_step_dr_from_a0(void)
{
	arc_t	src = ARC(false, 3, 5);
	arc_t	dst;

	step_dr(&src, &dst);
	assert(dst.a == true);
	assert(dst.r == 3);	/* a was 0, so r unchanged */
	assert(dst.c == 5);	/* a was 0, so c unchanged */
	PASS("step_dr_from_a0");
}

static void
test_step_dr_from_a1(void)
{
	arc_t	src = ARC(true, 3, 5);
	arc_t	dst;

	step_dr(&src, &dst);
	assert(dst.a == false);
	assert(dst.r == 4);	/* a was 1, so r + 1 */
	assert(dst.c == 6);	/* a was 1, so c + 1 */
	PASS("step_dr_from_a1");
}

static void
test_step_ul_from_a0(void)
{
	arc_t	src = ARC(false, 3, 5);
	arc_t	dst;

	step_ul(&src, &dst);
	assert(dst.a == true);
	assert(dst.r == 2);	/* darc->a is 1, so r - 1 */
	assert(dst.c == 4);	/* darc->a is 1, so c - 1 */
	PASS("step_ul_from_a0");
}

static void
test_step_ul_from_a1(void)
{
	arc_t	src = ARC(true, 3, 5);
	arc_t	dst;

	step_ul(&src, &dst);
	assert(dst.a == false);
	assert(dst.r == 3);	/* darc->a is 0, so r unchanged */
	assert(dst.c == 5);	/* darc->a is 0, so c unchanged */
	PASS("step_ul_from_a1");
}

/* ------------------------------------------------------------------ */
/* step_dl / step_ur (120-degree axis)                                */
/* ------------------------------------------------------------------ */

static void
test_step_dl_from_a0(void)
{
	arc_t	src = ARC(false, 3, 5);
	arc_t	dst;

	step_dl(&src, &dst);
	assert(dst.a == true);
	assert(dst.r == 3);	/* a was 0, so r unchanged */
	assert(dst.c == 4);	/* darc->a is 1, so c - 1 */
	PASS("step_dl_from_a0");
}

static void
test_step_dl_from_a1(void)
{
	arc_t	src = ARC(true, 3, 5);
	arc_t	dst;

	step_dl(&src, &dst);
	assert(dst.a == false);
	assert(dst.r == 4);	/* a was 1, so r + 1 */
	assert(dst.c == 5);	/* darc->a is 0, so c unchanged */
	PASS("step_dl_from_a1");
}

static void
test_step_ur_from_a0(void)
{
	arc_t	src = ARC(false, 3, 5);
	arc_t	dst;

	step_ur(&src, &dst);
	assert(dst.a == true);
	assert(dst.r == 2);	/* darc->a is 1, so r - 1 */
	assert(dst.c == 5);	/* arc->a is 0, so c unchanged */
	PASS("step_ur_from_a0");
}

static void
test_step_ur_from_a1(void)
{
	arc_t	src = ARC(true, 3, 5);
	arc_t	dst;

	step_ur(&src, &dst);
	assert(dst.a == false);
	assert(dst.r == 3);	/* darc->a is 0, so r unchanged */
	assert(dst.c == 6);	/* arc->a is 1, so c + 1 */
	PASS("step_ur_from_a1");
}

/* ------------------------------------------------------------------ */
/* step round-trip consistency                                        */
/* ------------------------------------------------------------------ */

static void
test_step_roundtrip_r_l(void)
{
	arc_t	orig = ARC(false, 7, -3);
	arc_t	mid, back;

	step_r(&orig, &mid);
	step_l(&mid, &back);
	assert(back.a == orig.a && back.r == orig.r && back.c == orig.c);

	orig = ARC(true, 7, -3);
	step_r(&orig, &mid);
	step_l(&mid, &back);
	assert(back.a == orig.a && back.r == orig.r && back.c == orig.c);
	PASS("step_roundtrip_r_l");
}

static void
test_step_roundtrip_dr_ul(void)
{
	arc_t	orig = ARC(false, 4, 2);
	arc_t	mid, back;

	step_dr(&orig, &mid);
	step_ul(&mid, &back);
	assert(back.a == orig.a && back.r == orig.r && back.c == orig.c);

	orig = ARC(true, 4, 2);
	step_dr(&orig, &mid);
	step_ul(&mid, &back);
	assert(back.a == orig.a && back.r == orig.r && back.c == orig.c);
	PASS("step_roundtrip_dr_ul");
}

static void
test_step_roundtrip_dl_ur(void)
{
	arc_t	orig = ARC(false, -1, 5);
	arc_t	mid, back;

	step_dl(&orig, &mid);
	step_ur(&mid, &back);
	assert(back.a == orig.a && back.r == orig.r && back.c == orig.c);

	orig = ARC(true, -1, 5);
	step_dl(&orig, &mid);
	step_ur(&mid, &back);
	assert(back.a == orig.a && back.r == orig.r && back.c == orig.c);
	PASS("step_roundtrip_dl_ur");
}

/* ================================================================== */
/* populate_tile_map                                                  */
/* ================================================================== */

static void
test_populate_tile_map(void)
{
	tile_map_t	ltm;
	tm_entry_t	*e;
	int		as[]     = {0, 1, 0};
	int		rs[]     = {0, 1, 2};
	int		cs[]     = {0, 1, 2};
	int		is_p1s[] = {1, 0, 1};
	arc_t		a;

	tm_init(&ltm);
	populate_tile_map(&ltm, as, rs, cs, is_p1s, 3);

	assert(ltm.tm_stack_size == 3);

	a = ARC(false, 0, 0);
	e = tm_get(&ltm, &a);
	assert(e != NULL);
	assert(TME_IS_P1(e));

	a = ARC(true, 1, 1);
	e = tm_get(&ltm, &a);
	assert(e != NULL);
	assert(!TME_IS_P1(e));

	a = ARC(false, 2, 2);
	e = tm_get(&ltm, &a);
	assert(e != NULL);
	assert(TME_IS_P1(e));

	PASS("populate_tile_map");
}

/* ================================================================== */
/* walk_dir                                                           */
/* ================================================================== */

/*
 * All walk_dir tests use step_r along a=false, r=0 row for simplicity.
 * Tiles are placed at (a=0, r=0, c=0), (a=0, r=0, c=1), etc.
 */

static void
test_walk_dir_empty_start(void)
{
	tile_map_t	ltm;
	arc_t		start = ARC(false, 0, 0);
	bool		enemy_end;
	int		len;

	tm_init(&ltm);
	len = walk_dir(&ltm, &start, true, step_r, &enemy_end);
	assert(len == 0);
	assert(!enemy_end);
	PASS("walk_dir_empty_start");
}

static void
test_walk_dir_enemy_start(void)
{
	tile_map_t	ltm;
	arc_t		start = ARC(false, 0, 0);
	bool		enemy_end;
	int		len;

	tm_init(&ltm);
	tm_insert(&ltm, &start, false); /* p2 tile */
	len = walk_dir(&ltm, &start, true, step_r, &enemy_end);
	assert(len == 0);
	assert(enemy_end);
	PASS("walk_dir_enemy_start");
}

static void
test_walk_dir_single_friendly_then_empty(void)
{
	tile_map_t	ltm;
	arc_t		start = ARC(false, 0, 0);
	bool		enemy_end;
	int		len;

	tm_init(&ltm);
	tm_insert(&ltm, &start, true);
	len = walk_dir(&ltm, &start, true, step_r, &enemy_end);
	assert(len == 1);
	assert(!enemy_end);
	PASS("walk_dir_single_friendly_then_empty");
}

static void
test_walk_dir_single_friendly_then_enemy(void)
{
	tile_map_t	ltm;
	arc_t		a0 = ARC(false, 0, 0);
	arc_t		a1 = ARC(false, 0, 1);
	bool		enemy_end;
	int		len;

	tm_init(&ltm);
	tm_insert(&ltm, &a0, true);
	tm_insert(&ltm, &a1, false); /* enemy */
	len = walk_dir(&ltm, &a0, true, step_r, &enemy_end);
	assert(len == 1);
	assert(enemy_end);
	PASS("walk_dir_single_friendly_then_enemy");
}

static void
test_walk_dir_multi_friendly_then_empty(void)
{
	tile_map_t	ltm;
	arc_t		a0 = ARC(false, 0, 0);
	arc_t		a1 = ARC(false, 0, 1);
	arc_t		a2 = ARC(false, 0, 2);
	bool		enemy_end;
	int		len;

	tm_init(&ltm);
	tm_insert(&ltm, &a0, true);
	tm_insert(&ltm, &a1, true);
	tm_insert(&ltm, &a2, true);
	len = walk_dir(&ltm, &a0, true, step_r, &enemy_end);
	assert(len == 3);
	assert(!enemy_end);
	PASS("walk_dir_multi_friendly_then_empty");
}

static void
test_walk_dir_multi_friendly_then_enemy(void)
{
	tile_map_t	ltm;
	arc_t		a0 = ARC(false, 0, 0);
	arc_t		a1 = ARC(false, 0, 1);
	arc_t		a2 = ARC(false, 0, 2);
	arc_t		a3 = ARC(false, 0, 3);
	bool		enemy_end;
	int		len;

	tm_init(&ltm);
	tm_insert(&ltm, &a0, true);
	tm_insert(&ltm, &a1, true);
	tm_insert(&ltm, &a2, true);
	tm_insert(&ltm, &a3, false); /* enemy */
	len = walk_dir(&ltm, &a0, true, step_r, &enemy_end);
	assert(len == 3);
	assert(enemy_end);
	PASS("walk_dir_multi_friendly_then_enemy");
}

/* ================================================================== */
/* count_dir                                                          */
/* ================================================================== */

/*
 * count_dir counts positions walked (friendly + empty) up to max or enemy.
 * Returns the count including empty gaps; length = total steps taken.
 */

static void
test_count_dir_all_empty(void)
{
	tile_map_t	ltm;
	arc_t		start = ARC(false, 0, 0);
	int		length, count;

	tm_init(&ltm);
	count = count_dir(&ltm, &start, true, 6, step_r, &length);
	assert(count == 6);
	assert(length == 6);
	PASS("count_dir_all_empty");
}

static void
test_count_dir_enemy_at_start(void)
{
	tile_map_t	ltm;
	arc_t		start = ARC(false, 0, 0);
	int		length, count;

	tm_init(&ltm);
	tm_insert(&ltm, &start, false); /* enemy */
	count = count_dir(&ltm, &start, true, 6, step_r, &length);
	assert(count == 0);
	assert(length == 0);
	PASS("count_dir_enemy_at_start");
}

static void
test_count_dir_friendly_with_gaps(void)
{
	tile_map_t	ltm;
	arc_t		a0 = ARC(false, 0, 0);
	arc_t		a2 = ARC(false, 0, 2);
	int		length, count;

	tm_init(&ltm);
	tm_insert(&ltm, &a0, true);
	/* c=1 is empty */
	tm_insert(&ltm, &a2, true);
	count = count_dir(&ltm, &a0, true, 3, step_r, &length);
	assert(count == 3); /* friendly, empty, friendly all counted */
	assert(length == 3);
	PASS("count_dir_friendly_with_gaps");
}

static void
test_count_dir_friendly_then_enemy(void)
{
	tile_map_t	ltm;
	arc_t		a0 = ARC(false, 0, 0);
	arc_t		a1 = ARC(false, 0, 1);
	arc_t		a2 = ARC(false, 0, 2);
	int		length, count;

	tm_init(&ltm);
	tm_insert(&ltm, &a0, true);
	tm_insert(&ltm, &a1, true);
	tm_insert(&ltm, &a2, false); /* enemy */
	count = count_dir(&ltm, &a0, true, 6, step_r, &length);
	assert(count == 2);
	assert(length == 2);
	PASS("count_dir_friendly_then_enemy");
}

static void
test_count_dir_max_reached(void)
{
	tile_map_t	ltm;
	arc_t		a;
	int		i, length, count;
	arc_t		start = ARC(false, 0, 0);

	tm_init(&ltm);
	for(i = 0; i < 5; i++) {
		a = ARC(false, 0, i);
		tm_insert(&ltm, &a, true);
	}
	count = count_dir(&ltm, &start, true, 3, step_r, &length);
	assert(count == 3);
	assert(length == 3);
	PASS("count_dir_max_reached");
}

static void
test_count_dir_empty_then_enemy(void)
{
	tile_map_t	ltm;
	arc_t		start = ARC(false, 0, 0);
	arc_t		a1 = ARC(false, 0, 1);
	int		length, count;

	tm_init(&ltm);
	/* c=0 is empty */
	tm_insert(&ltm, &a1, false); /* enemy at c=1 */
	count = count_dir(&ltm, &start, true, 6, step_r, &length);
	assert(count == 1);
	assert(length == 1);
	PASS("count_dir_empty_then_enemy");
}

/* ================================================================== */
/* score_move                                                         */
/* ================================================================== */

/*
 * All score_move tests use the horizontal axis (step_r / step_l).
 * The move position must be empty. Tiles are along a=false, r=0.
 * WIN_LENGTH = 6.
 *
 * Score A = count * NEARBY_SCORE, where count is total non-enemy positions
 *           reachable in both directions (up to 6 each), including the move.
 * Score B = SCORE_FOR_LEN(line_length), doubled if neither end is enemy.
 * Returns 0 if not enough space for a 6-in-a-row.
 * Returns P_WON if line_length >= 6.
 */

static void
test_score_move_not_enough_space(void)
{
	tile_map_t	ltm;
	arc_t		move, a;
	int		score;

	/*
	 * Enemy at c=-2 and c=3. Move at c=0.
	 * count_dir right from c=0: c=0,c=1,c=2 then enemy at c=3 -> len_r=3
	 * count_dir left from c=0: c=0,c=-1 then enemy at c=-2 -> len_l=2
	 * len_l + len_r - 1 = 2 + 3 - 1 = 4 < 6 -> returns 0
	 */
	tm_init(&ltm);
	a = ARC(false, 0, -2);
	tm_insert(&ltm, &a, false);
	a = ARC(false, 0, 3);
	tm_insert(&ltm, &a, false);

	move = ARC(false, 0, 0);
	score = score_move(&ltm, &move, true, step_r, step_l);
	assert(score == 0);
	PASS("score_move_not_enough_space");
}

static void
test_score_move_isolated_open(void)
{
	tile_map_t	ltm;
	arc_t		move = ARC(false, 0, 0);
	int		score, expected;

	/*
	 * Empty board. Move at c=0.
	 * count_dir right: 6 empty positions (c=0..c=5), len_r=6
	 * count_dir left: 6 empty positions (c=0..c=-5), len_l=6
	 * total count = 6 + 6 = 12 (move counted twice by both count_dirs)
	 * len_l + len_r - 1 = 11 >= 6, so not pruned.
	 * Score A = 12 * NEARBY_SCORE = 12 * 4 = 48
	 *
	 * walk_dir right from c=1: no tiles -> len_r=0
	 * walk_dir left from c=-1: no tiles -> len_l=0
	 * line length = 0 + 0 + 1 = 1
	 * Score B = L1_SCORE * 2 = 1 * 2 = 2 (neither end enemy)
	 *
	 * Total = 48 + 2 = 50
	 */
	tm_init(&ltm);
	score = score_move(&ltm, &move, true, step_r, step_l);
	expected = 12 * NEARBY_SCORE + L1_SCORE * 2;
	assert(score == expected);
	PASS("score_move_isolated_open");
}

static void
test_score_move_extends_line_of_2(void)
{
	tile_map_t	ltm;
	arc_t		move = ARC(false, 0, 0);
	arc_t		a;
	int		score, expected;

	/*
	 * Friendly at c=1 and c=2. Move at c=0.
	 * count_dir right from c=0: all empty/friendly up to 6 -> len_r=6
	 * count_dir left from c=0: all empty up to 6 -> len_l=6
	 * count = (6 friendly+empty right) + (6 empty left) = 14 with 2 friendly
	 * Actually count counts friendly AND empty equally (any non-enemy).
	 * Right: c=0(empty),c=1(f),c=2(f),c=3(e),c=4(e),c=5(e) -> count=6
	 * Left: c=0(empty),c=-1(e),c=-2(e),c=-3(e),c=-4(e),c=-5(e) -> count=6
	 * total count = 12
	 * Score A = 12 * 4 = 48
	 *
	 * walk_dir right from c=1: c=1(f),c=2(f),c=3(empty)->stop. len_r=2
	 * walk_dir left from c=-1: empty -> len_l=0
	 * line = 2 + 0 + 1 = 3, neither end enemy
	 * Score B = L3_SCORE * 2 = 30 * 2 = 60
	 *
	 * Total = 48 + 60 = 108
	 */
	tm_init(&ltm);
	a = ARC(false, 0, 1);
	tm_insert(&ltm, &a, true);
	a = ARC(false, 0, 2);
	tm_insert(&ltm, &a, true);

	score = score_move(&ltm, &move, true, step_r, step_l);
	expected = 12 * NEARBY_SCORE + L3_SCORE * 2;
	assert(score == expected);
	PASS("score_move_extends_line_of_2");
}

static void
test_score_move_bridges_two_segments(void)
{
	tile_map_t	ltm;
	arc_t		move = ARC(false, 0, 0);
	arc_t		a;
	int		score, expected;

	/*
	 * Friendly at c=-1 and c=1. Move at c=0 bridges them.
	 * count_dir right: c=0..c=5 all non-enemy -> 6, count=6
	 * count_dir left: c=0..c=-5 all non-enemy -> 6, count=6
	 * total count = 12
	 * Score A = 12 * 4 = 48
	 *
	 * walk_dir right from c=1: c=1(f), c=2(empty)->stop. len_r=1
	 * walk_dir left from c=-1: c=-1(f), c=-2(empty)->stop. len_l=1
	 * line = 1 + 1 + 1 = 3, neither end enemy
	 * Score B = L3_SCORE * 2 = 60
	 *
	 * Total = 48 + 60 = 108
	 */
	tm_init(&ltm);
	a = ARC(false, 0, -1);
	tm_insert(&ltm, &a, true);
	a = ARC(false, 0, 1);
	tm_insert(&ltm, &a, true);

	score = score_move(&ltm, &move, true, step_r, step_l);
	expected = 12 * NEARBY_SCORE + L3_SCORE * 2;
	assert(score == expected);
	PASS("score_move_bridges_two_segments");
}

static void
test_score_move_one_end_enemy(void)
{
	tile_map_t	ltm;
	arc_t		move = ARC(false, 0, 0);
	arc_t		a;
	int		score, score_a, score_b;
	int		count_r, count_l;

	/*
	 * Friendly at c=1, enemy at c=2. Move at c=0.
	 * count_dir right: c=0(e),c=1(f) then c=2 is enemy -> len_r=2, count=2
	 * count_dir left: c=0..c=-5 all empty -> len_l=6, count=6
	 * len_l + len_r - 1 = 6 + 2 - 1 = 7 >= 6, not pruned
	 * total count = 8
	 * Score A = 8 * 4 = 32
	 *
	 * walk_dir right from c=1: c=1(f), c=2(enemy)->stop. len_r=1, en_end_r=true
	 * walk_dir left from c=-1: empty -> len_l=0, en_end_l=false
	 * line = 1 + 0 + 1 = 2
	 * Score B = L2_SCORE = 8 (NOT doubled, one end is enemy)
	 *
	 * Total = 32 + 8 = 40
	 */
	tm_init(&ltm);
	a = ARC(false, 0, 1);
	tm_insert(&ltm, &a, true);
	a = ARC(false, 0, 2);
	tm_insert(&ltm, &a, false);

	score = score_move(&ltm, &move, true, step_r, step_l);
	count_r = 2;
	count_l = 6;
	score_a = (count_r + count_l) * NEARBY_SCORE;
	score_b = L2_SCORE; /* not doubled */
	assert(score == score_a + score_b);
	PASS("score_move_one_end_enemy");
}

static void
test_score_move_winning(void)
{
	tile_map_t	ltm;
	arc_t		move = ARC(false, 0, 0);
	arc_t		a;
	int		i, score;

	/*
	 * Friendly at c=1..c=5 (5 tiles). Move at c=0 completes 6 in a row.
	 * walk_dir right from c=1: 5 friendly -> len_r=5
	 * walk_dir left from c=-1: empty -> len_l=0
	 * line = 5 + 0 + 1 = 6 >= WIN_LENGTH -> returns P_WON
	 */
	tm_init(&ltm);
	for(i = 1; i <= 5; i++) {
		a = ARC(false, 0, i);
		tm_insert(&ltm, &a, true);
	}

	score = score_move(&ltm, &move, true, step_r, step_l);
	assert(score == P_WON);
	PASS("score_move_winning");
}

/* ================================================================== */
/* populate_cmm                                                       */
/* ================================================================== */

static void
test_populate_cmm_single_tile(void)
{
	tile_map_t	ltm;
	move_map_t	lcmm;
	arc_t		tile = ARC(false, 0, 0);
	arc_t		nbr;
	int		total;

	/*
	 * One tile placed. It has 6 neighbors, all empty.
	 * Each should become a candidate in the cmm.
	 */
	tm_init(&ltm);
	mm_init(&lcmm);
	tm_insert(&ltm, &tile, true);
	total = populate_cmm(&ltm, &lcmm);

	assert(lcmm.mm_stack_size == 6);

	/* Verify each neighbor is present */
	step_r(&tile, &nbr);
	assert(mm_get(&lcmm, &nbr) != NULL);
	step_l(&tile, &nbr);
	assert(mm_get(&lcmm, &nbr) != NULL);
	step_dr(&tile, &nbr);
	assert(mm_get(&lcmm, &nbr) != NULL);
	step_ul(&tile, &nbr);
	assert(mm_get(&lcmm, &nbr) != NULL);
	step_dl(&tile, &nbr);
	assert(mm_get(&lcmm, &nbr) != NULL);
	step_ur(&tile, &nbr);
	assert(mm_get(&lcmm, &nbr) != NULL);
	PASS("populate_cmm_single_tile");
}

static void
test_populate_cmm_adjacent_dedup(void)
{
	tile_map_t	ltm;
	move_map_t	lcmm;
	arc_t		t0 = ARC(false, 0, 0);
	arc_t		t1 = ARC(false, 0, 1);
	int		total;

	/*
	 * Two horizontally adjacent tiles. They share some neighbors.
	 * The shared neighbors should only appear once in the cmm.
	 * Also, t0 and t1 themselves are occupied so not candidates.
	 */
	tm_init(&ltm);
	mm_init(&lcmm);
	tm_insert(&ltm, &t0, true);
	tm_insert(&ltm, &t1, true);
	total = populate_cmm(&ltm, &lcmm);

	/* t0 has 6 neighbors, t1 has 6 neighbors.
	 * t1 is a neighbor of t0 (right) -> occupied, skipped.
	 * t0 is a neighbor of t1 (left) -> occupied, skipped.
	 * Shared empty neighbors are deduped.
	 * Count unique empty neighbors by checking the cmm. */
	assert(lcmm.mm_stack_size > 0);

	/* Neither occupied tile should be a candidate */
	assert(mm_get(&lcmm, &t0) == NULL);
	assert(mm_get(&lcmm, &t1) == NULL);
	PASS("populate_cmm_adjacent_dedup");
}

static void
test_populate_cmm_occupied_skipped(void)
{
	tile_map_t	ltm;
	move_map_t	lcmm;
	arc_t		t0 = ARC(false, 0, 0);
	arc_t		t1 = ARC(false, 0, 1);
	int		total;

	/*
	 * Two adjacent tiles. Verify neither occupied position is in the cmm.
	 */
	tm_init(&ltm);
	mm_init(&lcmm);
	tm_insert(&ltm, &t0, true);
	tm_insert(&ltm, &t1, false);
	total = populate_cmm(&ltm, &lcmm);

	assert(mm_get(&lcmm, &t0) == NULL);
	assert(mm_get(&lcmm, &t1) == NULL);
	PASS("populate_cmm_occupied_skipped");
}

static void
test_populate_cmm_p1_wins(void)
{
	tile_map_t	ltm;
	move_map_t	lcmm;
	arc_t		a;
	int		i, total;

	/*
	 * 5 p1 tiles in a horizontal row at c=0..c=4.
	 * The neighbor at c=5 (or c=-1) completes 6 in a row for p1.
	 * populate_cmm should detect this and return P1_WON.
	 */
	tm_init(&ltm);
	mm_init(&lcmm);
	for(i = 0; i < 5; i++) {
		a = ARC(false, 0, i);
		tm_insert(&ltm, &a, true);
	}
	total = populate_cmm(&ltm, &lcmm);
	assert(total == P1_WON);
	PASS("populate_cmm_p1_wins");
}

static void
test_populate_cmm_p2_wins(void)
{
	tile_map_t	ltm;
	move_map_t	lcmm;
	arc_t		a;
	int		i, total;

	/*
	 * 5 p2 tiles in a horizontal row at c=0..c=4.
	 * populate_cmm should detect this and return P2_WON.
	 */
	tm_init(&ltm);
	mm_init(&lcmm);
	for(i = 0; i < 5; i++) {
		a = ARC(false, 0, i);
		tm_insert(&ltm, &a, false);
	}
	total = populate_cmm(&ltm, &lcmm);
	assert(total == P2_WON);
	PASS("populate_cmm_p2_wins");
}

static void
test_populate_cmm_total_sign(void)
{
	tile_map_t	ltm;
	move_map_t	lcmm;
	arc_t		a;
	int		i, total;

	/*
	 * 3 p1 tiles in a row -> p1 has strong lines, p2 has nothing.
	 * Total should be positive (p1 score minus p2 score > 0).
	 */
	tm_init(&ltm);
	mm_init(&lcmm);
	for(i = 0; i < 3; i++) {
		a = ARC(false, 0, i);
		tm_insert(&ltm, &a, true);
	}
	total = populate_cmm(&ltm, &lcmm);
	assert(total > 0);

	/*
	 * 3 p2 tiles in a row -> p2 has strong lines, p1 has nothing.
	 * Total should be negative.
	 */
	tm_init(&ltm);
	mm_init(&lcmm);
	for(i = 0; i < 3; i++) {
		a = ARC(false, 0, i);
		tm_insert(&ltm, &a, false);
	}
	total = populate_cmm(&ltm, &lcmm);
	assert(total < 0);
	PASS("populate_cmm_total_sign");
}

/* ================================================================== */
/* populate_sorted_moves                                              */
/* ================================================================== */

/*
 * Helper: insert an entry into the cmm with a given impact.
 * We put the entire impact into s0_p1 and zero the rest, so
 * MME_IMPACT == impact.
 */

static void
cmm_insert_with_impact(move_map_t *cmm, int a, int r, int c, uint32_t impact)
{
	arc_t		arc = ARC(a, r, c);
	mm_score_t	s = SCORE(impact, 0, 0, 0, 0, 0);

	mm_insert(cmm, &arc, &s);
}

static void
test_psm_basic_ordering(void)
{
	move_map_t	lcmm;
	move_list_t	ml;
	move_entry_t	heap_buf[MAX_EVAL_WIDTH + 1];
	mm_entry_t	*sorted[MAX_EVAL_WIDTH];
	int		n;

	mm_init(&lcmm);
	INIT_STACK(&ml, heap_buf, MAX_EVAL_WIDTH);

	cmm_insert_with_impact(&lcmm, false, 0, 0, 10);
	cmm_insert_with_impact(&lcmm, false, 0, 1, 50);
	cmm_insert_with_impact(&lcmm, false, 0, 2, 30);
	cmm_insert_with_impact(&lcmm, false, 0, 3, 70);
	cmm_insert_with_impact(&lcmm, false, 0, 4, 20);

	n = populate_sorted_moves(&lcmm, &ml, sorted, 0);
	assert(n == 5);

	/* Descending order of impact */
	assert(MME_IMPACT(sorted[0]) == 70);
	assert(MME_IMPACT(sorted[1]) == 50);
	assert(MME_IMPACT(sorted[2]) == 30);
	assert(MME_IMPACT(sorted[3]) == 20);
	assert(MME_IMPACT(sorted[4]) == 10);
	PASS("psm_basic_ordering");
}

static void
test_psm_fewer_than_look_moves(void)
{
	move_map_t	lcmm;
	move_list_t	ml;
	move_entry_t	heap_buf[MAX_EVAL_WIDTH + 1];
	mm_entry_t	*sorted[MAX_EVAL_WIDTH];
	int		n, i;

	/* depth 0 -> look_moves = 15. Insert only 4. */
	mm_init(&lcmm);
	INIT_STACK(&ml, heap_buf, MAX_EVAL_WIDTH);

	cmm_insert_with_impact(&lcmm, false, 0, 0, 40);
	cmm_insert_with_impact(&lcmm, false, 0, 1, 10);
	cmm_insert_with_impact(&lcmm, false, 0, 2, 30);
	cmm_insert_with_impact(&lcmm, false, 0, 3, 20);

	n = populate_sorted_moves(&lcmm, &ml, sorted, 0);
	assert(n == 4);

	/* Verify descending order */
	for(i = 0; i < n - 1; i++) {
		uint32_t a = MME_IMPACT(sorted[i]);
		uint32_t b = MME_IMPACT(sorted[i + 1]);
		assert(a >= b);
	}
	PASS("psm_fewer_than_look_moves");
}

static void
test_psm_more_than_look_moves(void)
{
	move_map_t	lcmm;
	move_list_t	ml;
	move_entry_t	heap_buf[MAX_EVAL_WIDTH + 1];
	mm_entry_t	*sorted[MAX_EVAL_WIDTH];
	int		n, i;
	uint32_t	min_impact;

	/* depth 0 -> look_moves = 15. Insert 20 entries. */
	mm_init(&lcmm);
	INIT_STACK(&ml, heap_buf, MAX_EVAL_WIDTH);

	for(i = 0; i < 20; i++) {
		cmm_insert_with_impact(&lcmm, false, 0, i, (uint32_t)(i + 1));
	}

	n = populate_sorted_moves(&lcmm, &ml, sorted, 0);
	assert(n == 15);

	/* All returned entries should have impact >= 6 (top 15 of 1..20) */
	min_impact = MME_IMPACT(sorted[n - 1]);
	assert(min_impact >= 6);

	/* Verify descending order */
	for(i = 0; i < n - 1; i++) {
		uint32_t a = MME_IMPACT(sorted[i]);
		uint32_t b = MME_IMPACT(sorted[i + 1]);
		assert(a >= b);
	}
	PASS("psm_more_than_look_moves");
}

static void
test_psm_skipped_entries_ignored(void)
{
	move_map_t	lcmm;
	move_list_t	ml;
	move_entry_t	heap_buf[MAX_EVAL_WIDTH + 1];
	mm_entry_t	*sorted[MAX_EVAL_WIDTH];
	mm_entry_t	*entry;
	int		n, i;

	mm_init(&lcmm);
	INIT_STACK(&ml, heap_buf, MAX_EVAL_WIDTH);

	/* Insert 3 entries, then overwrite the highest-impact one.
	 * The original (skipped) should not appear; the overwritten
	 * value should. */
	cmm_insert_with_impact(&lcmm, false, 0, 0, 10);
	cmm_insert_with_impact(&lcmm, false, 0, 1, 50);
	cmm_insert_with_impact(&lcmm, false, 0, 2, 30);

	/* Overwrite (0,0,1) impact=50 -> new impact=5 */
	arc_t ow_arc = ARC(false, 0, 1);
	entry = mm_get(&lcmm, &ow_arc);
	mm_score_t new_s = SCORE(5, 0, 0, 0, 0, 0);
	mm_overwrite(&lcmm, entry, &new_s);

	n = populate_sorted_moves(&lcmm, &ml, sorted, 0);
	assert(n == 3);

	/* Descending: 30, 10, 5 (the skipped impact=50 entry is gone) */
	assert(MME_IMPACT(sorted[0]) == 30);
	assert(MME_IMPACT(sorted[1]) == 10);
	assert(MME_IMPACT(sorted[2]) == 5);
	PASS("psm_skipped_entries_ignored");
}

static void
test_psm_tied_scores(void)
{
	move_map_t	lcmm;
	move_list_t	ml;
	move_entry_t	heap_buf[MAX_EVAL_WIDTH + 1];
	mm_entry_t	*sorted[MAX_EVAL_WIDTH];
	int		n, i;

	mm_init(&lcmm);
	INIT_STACK(&ml, heap_buf, MAX_EVAL_WIDTH);

	/* 8 entries, all with impact=25 */
	for(i = 0; i < 8; i++) {
		cmm_insert_with_impact(&lcmm, false, 0, i, 25);
	}

	n = populate_sorted_moves(&lcmm, &ml, sorted, 0);
	assert(n == 8);

	/* All impacts should be 25 */
	for(i = 0; i < n; i++) {
		assert(MME_IMPACT(sorted[i]) == 25);
	}
	PASS("psm_tied_scores");
}

static void
test_psm_reuse_across_calls(void)
{
	move_map_t	lcmm1, lcmm2;
	move_list_t	ml;
	move_entry_t	heap_buf[MAX_EVAL_WIDTH + 1];
	mm_entry_t	*sorted[MAX_EVAL_WIDTH];
	int		n;

	INIT_STACK(&ml, heap_buf, MAX_EVAL_WIDTH);

	/* First call */
	mm_init(&lcmm1);
	cmm_insert_with_impact(&lcmm1, false, 0, 0, 99);
	cmm_insert_with_impact(&lcmm1, false, 0, 1, 50);
	cmm_insert_with_impact(&lcmm1, false, 0, 2, 75);

	n = populate_sorted_moves(&lcmm1, &ml, sorted, 0);
	assert(n == 3);
	assert(MME_IMPACT(sorted[0]) == 99);

	/* Second call with different cmm, same ml */
	mm_init(&lcmm2);
	cmm_insert_with_impact(&lcmm2, false, 1, 0, 10);
	cmm_insert_with_impact(&lcmm2, false, 1, 1, 40);

	n = populate_sorted_moves(&lcmm2, &ml, sorted, 0);
	assert(n == 2);
	assert(MME_IMPACT(sorted[0]) == 40);
	assert(MME_IMPACT(sorted[1]) == 10);
	PASS("psm_reuse_across_calls");
}

static void
test_psm_depth_affects_look_moves(void)
{
	move_map_t	lcmm;
	move_list_t	ml;
	move_entry_t	heap_buf[MAX_EVAL_WIDTH + 1];
	mm_entry_t	*sorted[MAX_EVAL_WIDTH];
	int		n, i;

	mm_init(&lcmm);
	INIT_STACK(&ml, heap_buf, MAX_EVAL_WIDTH);

	/* Insert 12 entries with distinct impacts */
	for(i = 0; i < 12; i++) {
		cmm_insert_with_impact(&lcmm, false, 0, i, (uint32_t)(i + 1));
	}

	/* depth 0: look_moves = 15, but only 12 entries -> returns 12 */
	n = populate_sorted_moves(&lcmm, &ml, sorted, 0);
	assert(n == 12);

	/* depth 3: look_moves = max(15-6,10) = 10, 12 entries -> returns 10 */
	n = populate_sorted_moves(&lcmm, &ml, sorted, 3);
	assert(n == 10);

	/* Verify the top 10 are the highest impact (12,11,...,3) in order */
	for(i = 0; i < 10; i++) {
		assert(MME_IMPACT(sorted[i]) ==
		       (uint32_t)(12 - i));
	}
	PASS("psm_depth_affects_look_moves");
}

/* ================================================================== */
/* main                                                               */
/* ================================================================== */

int
main(int argc, char const *argv[])
{
	printf("== mm_init ==\n");
	test_init_buckets_empty();
	test_init_stack_size_zero();

	printf("== mm_insert ==\n");
	test_insert_single();
	test_insert_different_buckets();
	test_insert_same_bucket();
	test_insert_stack_size();

	printf("== mm_get / mm_get_hash ==\n");
	test_get_exists();
	test_get_missing();
	test_get_distinguishes_a();
	test_get_hash_direct();

	printf("== mm_overwrite ==\n");
	test_overwrite_updates_score();
	test_overwrite_marks_skipped();
	test_overwrite_get_returns_new();
	test_overwrite_preserves_a();

	printf("== mm_remove_entry ==\n");
	test_remove_entry_decrements_stack();
	test_remove_entry_unlinks();

	printf("== mm_remove / mm_remove_hash ==\n");
	test_remove_by_arc();
	test_remove_by_hash();

	printf("== mm_remove_to_length ==\n");
	test_remove_to_length_basic();
	test_remove_to_length_single_overwrite();
	test_remove_to_length_double_overwrite();
	test_remove_to_length_zero();

	printf("== move_map macros ==\n");
	test_mme_impact();
	test_mme_location_match();

	printf("== tm_init ==\n");
	test_tm_init_buckets_empty();
	test_tm_init_stack_size_zero();

	printf("== tm_insert ==\n");
	test_tm_insert_single();
	test_tm_insert_p1_p2();
	test_tm_insert_different_buckets();
	test_tm_insert_same_bucket();
	test_tm_insert_stack_size();

	printf("== tm_get / tm_get_hash ==\n");
	test_tm_get_exists();
	test_tm_get_missing();
	test_tm_get_distinguishes_a();
	test_tm_get_hash_direct();

	printf("== tm_remove_entry ==\n");
	test_tm_remove_entry_decrements_stack();
	test_tm_remove_entry_unlinks();

	printf("== tm_remove / tm_remove_hash ==\n");
	test_tm_remove_by_arc();
	test_tm_remove_by_hash();

	printf("== tile_map macros ==\n");
	test_tme_location_match();

	printf("== mh_push ==\n");
	test_push_single();
	test_push_descending_min_at_top();

	printf("== mh_pop ==\n");
	test_pop_single();
	test_pop_returns_min();
	test_pop_all_ascending();
	test_pop_equal_scores();

	printf("== interleaved push/pop ==\n");
	test_interleaved();

	printf("== INIT_STACK / PUSH_STACK ==\n");
	test_init_stack();
	test_push_stack();

	printf("== heap reuse ==\n");
	test_heap_reuse_after_len_zero();

	printf("== is_p1_for_turn ==\n");
	test_is_p1_for_turn_pattern();

	printf("== step_r / step_l ==\n");
	test_step_r_from_a0();
	test_step_r_from_a1();
	test_step_l_from_a0();
	test_step_l_from_a1();

	printf("== step_dr / step_ul ==\n");
	test_step_dr_from_a0();
	test_step_dr_from_a1();
	test_step_ul_from_a0();
	test_step_ul_from_a1();

	printf("== step_dl / step_ur ==\n");
	test_step_dl_from_a0();
	test_step_dl_from_a1();
	test_step_ur_from_a0();
	test_step_ur_from_a1();

	printf("== populate_tile_map ==\n");
	test_populate_tile_map();

	printf("== walk_dir ==\n");
	test_walk_dir_empty_start();
	test_walk_dir_enemy_start();
	test_walk_dir_single_friendly_then_empty();
	test_walk_dir_single_friendly_then_enemy();
	test_walk_dir_multi_friendly_then_empty();
	test_walk_dir_multi_friendly_then_enemy();

	printf("== count_dir ==\n");
	test_count_dir_all_empty();
	test_count_dir_enemy_at_start();
	test_count_dir_friendly_with_gaps();
	test_count_dir_friendly_then_enemy();
	test_count_dir_max_reached();
	test_count_dir_empty_then_enemy();

	printf("== score_move ==\n");
	test_score_move_not_enough_space();
	test_score_move_isolated_open();
	test_score_move_extends_line_of_2();
	test_score_move_bridges_two_segments();
	test_score_move_one_end_enemy();
	test_score_move_winning();

	printf("== populate_cmm ==\n");
	test_populate_cmm_single_tile();
	test_populate_cmm_adjacent_dedup();
	test_populate_cmm_occupied_skipped();
	test_populate_cmm_p1_wins();
	test_populate_cmm_p2_wins();
	test_populate_cmm_total_sign();

	printf("== populate_sorted_moves ==\n");
	test_psm_basic_ordering();
	test_psm_fewer_than_look_moves();
	test_psm_more_than_look_moves();
	test_psm_skipped_entries_ignored();
	test_psm_tied_scores();
	test_psm_reuse_across_calls();
	test_psm_depth_affects_look_moves();

	printf("== step round-trips ==\n");
	test_step_roundtrip_r_l();
	test_step_roundtrip_dr_ul();
	test_step_roundtrip_dl_ur();

	printf("\nAll tests passed.\n");
	return 0;
}

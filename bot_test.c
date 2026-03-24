#include "bot.c"
#include <stdio.h>
#include <string.h>

static int tests_run = 0;
static int tests_passed = 0;
static int test_failed_flag = 0;

#define TEST(name) \
	static void name(void); \
	static void name(void)

#define RUN(name) do { \
	tests_run++; \
	test_failed_flag = 0; \
	printf("  %s... ", #name); \
	name(); \
	if (!test_failed_flag) { \
		tests_passed++; \
		printf("ok\n"); \
	} \
} while(0)

#define ASSERT_EQ(a, b) do { \
	long long _a = (long long)(a), _b = (long long)(b); \
	if (_a != _b) { \
		printf("FAIL\n    %s:%d: %s == %lld, expected %lld\n", \
		       __FILE__, __LINE__, #a, _a, _b); \
		test_failed_flag = 1; \
		return; \
	} \
} while(0)

#define ASSERT_TRUE(a) do { \
	if (!(a)) { \
		printf("FAIL\n    %s:%d: %s was false\n", \
		       __FILE__, __LINE__, #a); \
		test_failed_flag = 1; \
		return; \
	} \
} while(0)

#define ASSERT_NULL(a) do { \
	if ((a) != NULL) { \
		printf("FAIL\n    %s:%d: %s was not NULL\n", \
		       __FILE__, __LINE__, #a); \
		test_failed_flag = 1; \
		return; \
	} \
} while(0)

/*
 * Step function tests
 *
 * Reference (from JS):
 *   stepR(a, r, c)  = { a,   r,          c+1       }
 *   stepL(a, r, c)  = { a,   r,          c-1       }
 *   stepDR(a, r, c) = { 1-a, r+(a&1),    c+(a&1)   }
 *   stepDL(a, r, c) = { 1-a, r+(a&1),    c-(na&1)  }  na=1-a
 *   stepUR(a, r, c) = { 1-a, r-(na&1),   c+(a&1)   }  na=1-a
 *   stepUL(a, r, c) = { 1-a, r-(na&1),   c-(na&1)  }  na=1-a
 */

TEST(test_step_r) {
	arc_t in = {0, 3, 5}, out;
	step_r(&in, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, 3);
	ASSERT_EQ(out.c, 6);

	arc_t in2 = {1, -2, -1}, out2;
	step_r(&in2, &out2);
	ASSERT_EQ(out2.a, 1);
	ASSERT_EQ(out2.r, -2);
	ASSERT_EQ(out2.c, 0);
}

TEST(test_step_l) {
	arc_t in = {0, 3, 5}, out;
	step_l(&in, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, 3);
	ASSERT_EQ(out.c, 4);

	arc_t in2 = {1, 0, 0}, out2;
	step_l(&in2, &out2);
	ASSERT_EQ(out2.a, 1);
	ASSERT_EQ(out2.r, 0);
	ASSERT_EQ(out2.c, -1);
}

TEST(test_step_dr_a0) {
	// a=0: na=1, r+0, c+0
	arc_t in = {0, 3, 5}, out;
	step_dr(&in, &out);
	ASSERT_EQ(out.a, 1);
	ASSERT_EQ(out.r, 3);
	ASSERT_EQ(out.c, 5);
}

TEST(test_step_dr_a1) {
	// a=1: na=0, r+1, c+1
	arc_t in = {1, 3, 5}, out;
	step_dr(&in, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, 4);
	ASSERT_EQ(out.c, 6);
}

TEST(test_step_dl_a0) {
	// a=0: na=1, r+0, c-(1&1)=c-1
	arc_t in = {0, 3, 5}, out;
	step_dl(&in, &out);
	ASSERT_EQ(out.a, 1);
	ASSERT_EQ(out.r, 3);
	ASSERT_EQ(out.c, 4);
}

TEST(test_step_dl_a1) {
	// a=1: na=0, r+1, c-(0&1)=c
	arc_t in = {1, 3, 5}, out;
	step_dl(&in, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, 4);
	ASSERT_EQ(out.c, 5);
}

TEST(test_step_ur_a0) {
	// a=0: na=1, r-(1&1)=r-1, c+(0&1)=c
	arc_t in = {0, 3, 5}, out;
	step_ur(&in, &out);
	ASSERT_EQ(out.a, 1);
	ASSERT_EQ(out.r, 2);
	ASSERT_EQ(out.c, 5);
}

TEST(test_step_ur_a1) {
	// a=1: na=0, r-(0&1)=r, c+(1&1)=c+1
	arc_t in = {1, 3, 5}, out;
	step_ur(&in, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, 3);
	ASSERT_EQ(out.c, 6);
}

TEST(test_step_ul_a0) {
	// a=0: na=1, r-(1&1)=r-1, c-(1&1)=c-1
	arc_t in = {0, 3, 5}, out;
	step_ul(&in, &out);
	ASSERT_EQ(out.a, 1);
	ASSERT_EQ(out.r, 2);
	ASSERT_EQ(out.c, 4);
}

TEST(test_step_ul_a1) {
	// a=1: na=0, r-(0&1)=r, c-(0&1)=c
	arc_t in = {1, 3, 5}, out;
	step_ul(&in, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, 3);
	ASSERT_EQ(out.c, 5);
}

// step + reverse step should be identity
TEST(test_step_r_l_inverse) {
	arc_t in = {0, -3, 7}, mid, out;
	step_r(&in, &mid);
	step_l(&mid, &out);
	ASSERT_EQ(out.a, in.a);
	ASSERT_EQ(out.r, in.r);
	ASSERT_EQ(out.c, in.c);
}

TEST(test_step_dr_ul_inverse) {
	arc_t in0 = {0, -3, 7}, mid0, out0;
	step_dr(&in0, &mid0);
	step_ul(&mid0, &out0);
	ASSERT_EQ(out0.a, in0.a);
	ASSERT_EQ(out0.r, in0.r);
	ASSERT_EQ(out0.c, in0.c);

	arc_t in1 = {1, 2, -4}, mid1, out1;
	step_dr(&in1, &mid1);
	step_ul(&mid1, &out1);
	ASSERT_EQ(out1.a, in1.a);
	ASSERT_EQ(out1.r, in1.r);
	ASSERT_EQ(out1.c, in1.c);
}

TEST(test_step_dl_ur_inverse) {
	arc_t in0 = {0, -3, 7}, mid0, out0;
	step_dl(&in0, &mid0);
	step_ur(&mid0, &out0);
	ASSERT_EQ(out0.a, in0.a);
	ASSERT_EQ(out0.r, in0.r);
	ASSERT_EQ(out0.c, in0.c);

	arc_t in1 = {1, 2, -4}, mid1, out1;
	step_dl(&in1, &mid1);
	step_ur(&mid1, &out1);
	ASSERT_EQ(out1.a, in1.a);
	ASSERT_EQ(out1.r, in1.r);
	ASSERT_EQ(out1.c, in1.c);
}

/*
 * mm_make_key / pos_from_mm_key tests
 */

TEST(test_key_roundtrip_positive) {
	arc_t in = {1, 5, 10}, out;
	uint64_t key = mm_make_key(&in);
	pos_from_mm_key(key, &out);
	ASSERT_EQ(out.a, 1);
	ASSERT_EQ(out.r, 5);
	ASSERT_EQ(out.c, 10);
}

TEST(test_key_roundtrip_negative) {
	arc_t in = {0, -7, -3}, out;
	uint64_t key = mm_make_key(&in);
	pos_from_mm_key(key, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, -7);
	ASSERT_EQ(out.c, -3);
}

TEST(test_key_roundtrip_zero) {
	arc_t in = {0, 0, 0}, out;
	uint64_t key = mm_make_key(&in);
	ASSERT_EQ(key, 0ULL);
	pos_from_mm_key(key, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, 0);
	ASSERT_EQ(out.c, 0);
}

TEST(test_key_roundtrip_mixed) {
	arc_t in = {1, -100, 200}, out;
	uint64_t key = mm_make_key(&in);
	pos_from_mm_key(key, &out);
	ASSERT_EQ(out.a, 1);
	ASSERT_EQ(out.r, -100);
	ASSERT_EQ(out.c, 200);
}

TEST(test_key_roundtrip_minus_one) {
	arc_t in = {0, -1, -1}, out;
	uint64_t key = mm_make_key(&in);
	pos_from_mm_key(key, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, -1);
	ASSERT_EQ(out.c, -1);
}

TEST(test_key_distinct) {
	arc_t a1 = {0, 1, 2};
	arc_t a2 = {0, 1, 3};
	arc_t a3 = {1, 1, 2};
	ASSERT_TRUE(mm_make_key(&a1) != mm_make_key(&a2));
	ASSERT_TRUE(mm_make_key(&a1) != mm_make_key(&a3));
	ASSERT_TRUE(mm_make_key(&a2) != mm_make_key(&a3));
}

/*
 * mm_append_dir_key / dir_from_mm_key tests
 */

TEST(test_dir_key_roundtrip_all_dirs) {
	arc_t pos = {1, 5, -3};
	uint64_t base = mm_make_key(&pos);
	uint8_t dir;

	for (dir = 0; dir <= 3; dir++) {
		uint64_t dk = mm_append_dir_key(base, dir);
		ASSERT_EQ(dir_from_mm_key(dk), dir);
	}
}

TEST(test_dir_key_does_not_corrupt_pos) {
	arc_t pos = {0, -7, 12}, out;
	uint64_t base = mm_make_key(&pos);
	uint8_t dir;

	for (dir = 0; dir <= 3; dir++) {
		uint64_t dk = mm_append_dir_key(base, dir);
		pos_from_mm_key(dk, &out);
		ASSERT_EQ(out.a, 0);
		ASSERT_EQ(out.r, -7);
		ASSERT_EQ(out.c, 12);
	}
}

TEST(test_dir_key_zero_base) {
	arc_t pos = {0, 0, 0}, out;
	uint64_t base = mm_make_key(&pos);

	uint64_t dk = mm_append_dir_key(base, 2);
	ASSERT_EQ(dir_from_mm_key(dk), 2);
	pos_from_mm_key(dk, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, 0);
	ASSERT_EQ(out.c, 0);
}

TEST(test_dir_key_distinct_dirs) {
	arc_t pos = {1, 3, 3};
	uint64_t base = mm_make_key(&pos);

	uint64_t d0 = mm_append_dir_key(base, 0);
	uint64_t d1 = mm_append_dir_key(base, 1);
	uint64_t d2 = mm_append_dir_key(base, 2);
	uint64_t d3 = mm_append_dir_key(base, 3);

	ASSERT_TRUE(d0 != d1);
	ASSERT_TRUE(d1 != d2);
	ASSERT_TRUE(d2 != d3);
	ASSERT_TRUE(d0 != d3);
}

TEST(test_dir_key_negative_coords) {
	arc_t pos = {1, -100, -200}, out;
	uint64_t base = mm_make_key(&pos);
	uint64_t dk = mm_append_dir_key(base, 3);

	ASSERT_EQ(dir_from_mm_key(dk), 3);
	pos_from_mm_key(dk, &out);
	ASSERT_EQ(out.a, 1);
	ASSERT_EQ(out.r, -100);
	ASSERT_EQ(out.c, -200);
}

/*
 * mm_insert / mm_get / mm_remove tests
 */

TEST(test_mm_insert_get) {
	move_map_t mm;
	mm_init(&mm);

	arc_t pos = {0, 1, 2};
	uint64_t key = mm_make_key(&pos);
	mm_insert(&mm, key, 1);

	mm_entry_t *entry = mm_get(&mm, key);
	ASSERT_TRUE(entry != NULL);
	ASSERT_EQ(entry->mme_key, key);
	ASSERT_EQ(entry->mme_value, 1);
	ASSERT_EQ(mm.mm_stack_size, 1);
}

TEST(test_mm_get_missing) {
	move_map_t mm;
	mm_init(&mm);

	arc_t pos = {0, 5, 5};
	uint64_t key = mm_make_key(&pos);
	ASSERT_NULL(mm_get(&mm, key));
}

TEST(test_mm_insert_multiple) {
	move_map_t mm;
	mm_init(&mm);

	arc_t p1 = {0, 0, 0}, p2 = {1, 1, 1}, p3 = {0, -1, -1};
	uint64_t k1 = mm_make_key(&p1);
	uint64_t k2 = mm_make_key(&p2);
	uint64_t k3 = mm_make_key(&p3);

	mm_insert(&mm, k1, 1);
	mm_insert(&mm, k2, 0);
	mm_insert(&mm, k3, 1);

	ASSERT_EQ(mm.mm_stack_size, 3);
	ASSERT_EQ(mm_get(&mm, k1)->mme_value, 1);
	ASSERT_EQ(mm_get(&mm, k2)->mme_value, 0);
	ASSERT_EQ(mm_get(&mm, k3)->mme_value, 1);
}

TEST(test_mm_remove_lifo) {
	move_map_t mm;
	mm_init(&mm);

	arc_t p1 = {0, 0, 0}, p2 = {1, 1, 1};
	uint64_t k1 = mm_make_key(&p1);
	uint64_t k2 = mm_make_key(&p2);

	mm_insert(&mm, k1, 1);
	mm_insert(&mm, k2, 0);
	ASSERT_EQ(mm.mm_stack_size, 2);

	// Remove k2 (top of stack)
	mm_remove(&mm, k2);
	ASSERT_EQ(mm.mm_stack_size, 1);
	ASSERT_NULL(mm_get(&mm, k2));
	ASSERT_TRUE(mm_get(&mm, k1) != NULL);

	// Remove k1 (now top of stack)
	mm_remove(&mm, k1);
	ASSERT_EQ(mm.mm_stack_size, 0);
	ASSERT_NULL(mm_get(&mm, k1));
}

TEST(test_mm_insert_remove_reinsert) {
	move_map_t mm;
	mm_init(&mm);

	arc_t p1 = {0, 2, 3};
	uint64_t k1 = mm_make_key(&p1);

	mm_insert(&mm, k1, 1);
	ASSERT_EQ(mm.mm_stack_size, 1);

	mm_remove(&mm, k1);
	ASSERT_EQ(mm.mm_stack_size, 0);
	ASSERT_NULL(mm_get(&mm, k1));

	// Reinsert with different value
	mm_insert(&mm, k1, 0);
	ASSERT_EQ(mm.mm_stack_size, 1);
	ASSERT_EQ(mm_get(&mm, k1)->mme_value, 0);
}

/*
 * mm_entries_qselect tests
 */

// Helper: build a move_map with entries of given values, for qselect tests.
// Keys are arbitrary distinct arcs; values are what we care about.
static void
qs_setup(move_map_t *mm, int *values, int n)
{
	int i;
	mm_init(mm);
	for (i = 0; i < n; i++) {
		arc_t p = {0, i, i * 7};
		uint64_t key = mm_make_key(&p);
		mm_insert(mm, key, values[i]);
	}
}

TEST(test_qselect_k0) {
	move_map_t mm;
	int vals[5] = {3, 7, 1, 9, 5};
	qs_setup(&mm, vals, 5);
	mm_entry_t *result = mm_entries_qselect(mm.mm_stack, 5, 0);
	ASSERT_EQ(result->mme_value, 9);
}

TEST(test_qselect_k2) {
	move_map_t mm;
	int vals[5] = {3, 7, 1, 9, 5};
	qs_setup(&mm, vals, 5);
	mm_entry_t *result = mm_entries_qselect(mm.mm_stack, 5, 2);
	ASSERT_EQ(result->mme_value, 5);
}

TEST(test_qselect_last) {
	move_map_t mm;
	int vals[4] = {50, 10, 30, 20};
	qs_setup(&mm, vals, 4);
	mm_entry_t *result = mm_entries_qselect(mm.mm_stack, 4, 3);
	ASSERT_EQ(result->mme_value, 10);
}

TEST(test_qselect_single) {
	move_map_t mm;
	int vals[1] = {42};
	qs_setup(&mm, vals, 1);
	mm_entry_t *result = mm_entries_qselect(mm.mm_stack, 1, 0);
	ASSERT_EQ(result->mme_value, 42);
}

TEST(test_qselect_equal_values) {
	move_map_t mm;
	int vals[4] = {5, 5, 5, 5};
	qs_setup(&mm, vals, 4);
	mm_entry_t *result = mm_entries_qselect(mm.mm_stack, 4, 0);
	ASSERT_EQ(result->mme_value, 5);
	result = mm_entries_qselect(mm.mm_stack, 4, 3);
	ASSERT_EQ(result->mme_value, 5);
}

/*
 * score_line tests
 *
 * Helper to build a board and call score_line on a specific tile.
 */

static move_map_t sl_mm;

static void
sl_reset(void)
{
	mm_init(&sl_mm);
}

static uint64_t
sl_add(int a, int r, int c, int is_p1)
{
	arc_t arc = {a, r, c};
	uint64_t key = mm_make_key(&arc);
	mm_insert(&sl_mm, key, is_p1);
	return key;
}

static mm_entry_t *
sl_get(int a, int r, int c)
{
	arc_t arc = {a, r, c};
	uint64_t key = mm_make_key(&arc);
	return mm_get(&sl_mm, key);
}

// Single tile, open on both sides in R direction.
// prev (left) is empty -> not terminated on either side
// length 1 p1 -> len = 1
TEST(test_score_line_single_tile_open) {
	sl_reset();
	sl_add(0, 0, 0, 1);

	arc_t pos = {0, 0, 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len;
	int term = score_line(&sl_mm, entry, &pos, &len,
			      &far, step_r, step_l);

	ASSERT_EQ(term, NO_TERM);
	ASSERT_EQ(len, 1);
}

// Single tile with opponent on the left.
// Terminated on close (prev) side, open on far side.
TEST(test_score_line_single_tile_terminated) {
	sl_reset();
	sl_add(0, 0, -1, 0);  // opponent to the left
	sl_add(0, 0, 0, 1);

	arc_t pos = {0, 0, 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len;
	int term = score_line(&sl_mm, entry, &pos, &len,
			      &far, step_r, step_l);

	ASSERT_EQ(term, CLOSE_TERM);
	ASSERT_EQ(len, 1);
}

// Two p1 tiles in a row. Scoring from the left end (start of line).
// prev of (0,0,0) is empty -> not terminated
// length = 2, p1 -> len = 2
TEST(test_score_line_two_in_row) {
	sl_reset();
	sl_add(0, 0, 0, 1);
	sl_add(0, 0, 1, 1);

	arc_t pos = {0, 0, 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len;
	int term = score_line(&sl_mm, entry, &pos, &len,
			      &far, step_r, step_l);

	ASSERT_EQ(term, NO_TERM);
	ASSERT_EQ(len, 2);
}

// Scoring from the second tile (not the line start) should return 0
// because prev is same player.
TEST(test_score_line_not_line_start) {
	sl_reset();
	sl_add(0, 0, 0, 1);
	sl_add(0, 0, 1, 1);

	arc_t pos = {0, 0, 1}, far;
	mm_entry_t *entry = sl_get(0, 0, 1);
	int len;
	int term = score_line(&sl_mm, entry, &pos, &len,
			      &far, step_r, step_l);

	ASSERT_EQ(len, 0);
	ASSERT_EQ(term, 0);
}

// P2 single tile, open both sides.
// len = -1 (negative for p2)
TEST(test_score_line_p2) {
	sl_reset();
	sl_add(0, 0, 0, 0);

	arc_t pos = {0, 0, 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len;
	int term = score_line(&sl_mm, entry, &pos, &len,
			      &far, step_r, step_l);

	ASSERT_EQ(term, NO_TERM);
	ASSERT_EQ(len, -1);
}

// Blocked on both sides: opponent left, opponent right.
// Both sides blocked -> len = 0
TEST(test_score_line_fully_blocked) {
	sl_reset();
	sl_add(0, 0, -1, 0);  // opponent left
	sl_add(0, 0, 0, 1);
	sl_add(0, 0, 1, 0);   // opponent right

	arc_t pos = {0, 0, 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len;
	int term = score_line(&sl_mm, entry, &pos, &len,
			      &far, step_r, step_l);

	ASSERT_EQ(len, 0);
	ASSERT_EQ(term, 0);
}

// Open left, opponent right after 1 tile.
// Far side terminated, len = 1
TEST(test_score_line_open_left_blocked_right) {
	sl_reset();
	sl_add(0, 0, 0, 1);
	sl_add(0, 0, 1, 0);   // opponent right

	arc_t pos = {0, 0, 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len;
	int term = score_line(&sl_mm, entry, &pos, &len,
			      &far, step_r, step_l);

	ASSERT_EQ(term, FAR_TERM);
	ASSERT_EQ(len, 1);
}

// 6 in a row = P1_WON
TEST(test_score_line_six_in_row_p1) {
	sl_reset();
	int i;
	for (i = 0; i < 6; i++) {
		sl_add(0, 0, i, 1);
	}

	arc_t pos = {0, 0, 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len;
	score_line(&sl_mm, entry, &pos, &len,
		   &far, step_r, step_l);

	ASSERT_EQ(len, P1_WON);
}

// 6 in a row for p2 = P2_WON
TEST(test_score_line_six_in_row_p2) {
	sl_reset();
	int i;
	for (i = 0; i < 6; i++) {
		sl_add(0, 0, i, 0);
	}

	arc_t pos = {0, 0, 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len;
	score_line(&sl_mm, entry, &pos, &len,
		   &far, step_r, step_l);

	ASSERT_EQ(len, P2_WON);
}

// Three in a row, open both sides, using DR/UL direction.
TEST(test_score_line_diagonal) {
	sl_reset();
	sl_add(0, 0, 0, 1);
	sl_add(1, 0, 0, 1);
	sl_add(0, 1, 1, 1);

	arc_t pos = {0, 0, 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len;
	int term = score_line(&sl_mm, entry, &pos, &len,
			      &far, step_dr, step_ul);

	ASSERT_EQ(term, NO_TERM);
	ASSERT_EQ(len, 3);
}

// Verify far_arc position — single p1 tile, step right.
// far_arc should be one step past the end: (0,0,1)
TEST(test_score_line_far_arc_position) {
	sl_reset();
	sl_add(0, 0, 0, 1);

	arc_t pos = {0, 0, 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len;
	int term = score_line(&sl_mm, entry, &pos, &len,
			      &far, step_r, step_l);

	ASSERT_EQ(term, NO_TERM);
	ASSERT_EQ(far.a, 0);
	ASSERT_EQ(far.r, 0);
	ASSERT_EQ(far.c, 1);
}

/*
 * swap_entries tests
 *
 * After swapping, the hash map must remain fully functional:
 * mm_get finds all entries, mm_insert adds new ones, mm_remove
 * works on the (now-reordered) stack top.
 */

TEST(test_swap_entries_get_still_works) {
	move_map_t mm;
	mm_init(&mm);

	arc_t p0 = {0, 0, 0}, p1 = {1, 1, 1}, p2 = {0, 2, 2};
	uint64_t k0 = mm_make_key(&p0);
	uint64_t k1 = mm_make_key(&p1);
	uint64_t k2 = mm_make_key(&p2);

	mm_insert(&mm, k0, 1);
	mm_insert(&mm, k1, 0);
	mm_insert(&mm, k2, 1);

	// Swap stack slots 0 and 2
	swap_entries(mm.mm_stack, 0, 2);

	// All three entries must still be found with correct values
	mm_entry_t *e0 = mm_get(&mm, k0);
	mm_entry_t *e1 = mm_get(&mm, k1);
	mm_entry_t *e2 = mm_get(&mm, k2);
	ASSERT_TRUE(e0 != NULL);
	ASSERT_TRUE(e1 != NULL);
	ASSERT_TRUE(e2 != NULL);
	ASSERT_EQ(e0->mme_value, 1);
	ASSERT_EQ(e1->mme_value, 0);
	ASSERT_EQ(e2->mme_value, 1);
}

TEST(test_swap_entries_slots_changed) {
	move_map_t mm;
	mm_init(&mm);

	arc_t p0 = {0, 0, 0}, p1 = {1, 1, 1}, p2 = {0, 2, 2};
	uint64_t k0 = mm_make_key(&p0);
	uint64_t k1 = mm_make_key(&p1);
	uint64_t k2 = mm_make_key(&p2);

	mm_insert(&mm, k0, 1);
	mm_insert(&mm, k1, 0);
	mm_insert(&mm, k2, 1);

	// Before swap: stack[0] has k0, stack[2] has k2
	ASSERT_EQ(mm.mm_stack[0].mme_key, k0);
	ASSERT_EQ(mm.mm_stack[2].mme_key, k2);

	swap_entries(mm.mm_stack, 0, 2);

	// After swap: stack slots are exchanged
	ASSERT_EQ(mm.mm_stack[0].mme_key, k2);
	ASSERT_EQ(mm.mm_stack[2].mme_key, k0);
	// Middle entry is untouched
	ASSERT_EQ(mm.mm_stack[1].mme_key, k1);
}

TEST(test_swap_entries_insert_after) {
	move_map_t mm;
	mm_init(&mm);

	arc_t p0 = {0, 0, 0}, p1 = {1, 1, 1};
	uint64_t k0 = mm_make_key(&p0);
	uint64_t k1 = mm_make_key(&p1);

	mm_insert(&mm, k0, 1);
	mm_insert(&mm, k1, 0);

	swap_entries(mm.mm_stack, 0, 1);

	// Insert a new entry — should work fine
	arc_t p2 = {0, 3, 3};
	uint64_t k2 = mm_make_key(&p2);
	mm_insert(&mm, k2, 1);

	ASSERT_EQ(mm.mm_stack_size, 3);
	ASSERT_TRUE(mm_get(&mm, k0) != NULL);
	ASSERT_TRUE(mm_get(&mm, k1) != NULL);
	ASSERT_TRUE(mm_get(&mm, k2) != NULL);
	ASSERT_EQ(mm_get(&mm, k2)->mme_value, 1);
}

TEST(test_swap_entries_remove_after) {
	move_map_t mm;
	mm_init(&mm);

	arc_t p0 = {0, 0, 0}, p1 = {1, 1, 1}, p2 = {0, 2, 2};
	uint64_t k0 = mm_make_key(&p0);
	uint64_t k1 = mm_make_key(&p1);
	uint64_t k2 = mm_make_key(&p2);

	mm_insert(&mm, k0, 1);
	mm_insert(&mm, k1, 0);
	mm_insert(&mm, k2, 1);

	// Swap 0 and 2: now stack top (index 2) holds k0
	swap_entries(mm.mm_stack, 0, 2);

	// mm_remove expects LIFO, so top of stack (k0) is removable
	mm_remove(&mm, k0);
	ASSERT_EQ(mm.mm_stack_size, 2);
	ASSERT_NULL(mm_get(&mm, k0));
	ASSERT_TRUE(mm_get(&mm, k1) != NULL);
	ASSERT_TRUE(mm_get(&mm, k2) != NULL);
}

TEST(test_swap_entries_adjacent) {
	move_map_t mm;
	mm_init(&mm);

	arc_t p0 = {0, 0, 0}, p1 = {1, 1, 1};
	uint64_t k0 = mm_make_key(&p0);
	uint64_t k1 = mm_make_key(&p1);

	mm_insert(&mm, k0, 1);
	mm_insert(&mm, k1, 0);

	swap_entries(mm.mm_stack, 0, 1);

	ASSERT_EQ(mm.mm_stack[0].mme_key, k1);
	ASSERT_EQ(mm.mm_stack[1].mme_key, k0);
	ASSERT_TRUE(mm_get(&mm, k0) != NULL);
	ASSERT_TRUE(mm_get(&mm, k1) != NULL);
	ASSERT_EQ(mm_get(&mm, k0)->mme_value, 1);
	ASSERT_EQ(mm_get(&mm, k1)->mme_value, 0);
}

TEST(test_swap_entries_same_bucket) {
	// Two keys that hash to the same bucket — swap must keep
	// the bucket chain intact.
	move_map_t mm;
	mm_init(&mm);

	// Find two keys that collide (same bucket index)
	arc_t pa = {0, 0, 0};
	uint64_t ka = mm_make_key(&pa);
	uint32_t target_bucket = MME_INDEX(ka);

	// Brute-force a second key in the same bucket
	arc_t pb;
	uint64_t kb;
	int r;
	for (r = 1; r < 1000; r++) {
		pb = (arc_t){0, r, 0};
		kb = mm_make_key(&pb);
		if (MME_INDEX(kb) == target_bucket)
			break;
	}
	ASSERT_EQ(MME_INDEX(kb), target_bucket);

	mm_insert(&mm, ka, 1);
	mm_insert(&mm, kb, 0);

	swap_entries(mm.mm_stack, 0, 1);

	// Both must still be reachable through the same bucket chain
	ASSERT_TRUE(mm_get(&mm, ka) != NULL);
	ASSERT_TRUE(mm_get(&mm, kb) != NULL);
	ASSERT_EQ(mm_get(&mm, ka)->mme_value, 1);
	ASSERT_EQ(mm_get(&mm, kb)->mme_value, 0);
}

TEST(test_swap_entries_same_bucket_rev) {
	// Two keys that hash to the same bucket — swap must keep
	// the bucket chain intact.
	// Tests passing the entries in the reverse order.
	move_map_t mm;
	mm_init(&mm);

	// Find two keys that collide (same bucket index)
	arc_t pa = {0, 0, 0};
	uint64_t ka = mm_make_key(&pa);
	uint32_t target_bucket = MME_INDEX(ka);

	// Brute-force a second key in the same bucket
	arc_t pb;
	uint64_t kb;
	int r;
	for (r = 1; r < 1000; r++) {
		pb = (arc_t){0, r, 0};
		kb = mm_make_key(&pb);
		if (MME_INDEX(kb) == target_bucket)
			break;
	}
	ASSERT_EQ(MME_INDEX(kb), target_bucket);

	mm_insert(&mm, ka, 1);
	mm_insert(&mm, kb, 0);

	swap_entries(mm.mm_stack, 1, 0);

	// Both must still be reachable through the same bucket chain
	ASSERT_TRUE(mm_get(&mm, ka) != NULL);
	ASSERT_TRUE(mm_get(&mm, kb) != NULL);
	ASSERT_EQ(mm_get(&mm, ka)->mme_value, 1);
	ASSERT_EQ(mm_get(&mm, kb)->mme_value, 0);
}

TEST(test_swap_entries_many) {
	// Insert many entries, swap several pairs, verify all lookups
	move_map_t mm;
	mm_init(&mm);

	int n = 20;
	uint64_t keys[20];
	int i;
	for (i = 0; i < n; i++) {
		arc_t p = {i & 1, i * 3, i * 7};
		keys[i] = mm_make_key(&p);
		mm_insert(&mm, keys[i], i);
	}

	// Swap several pairs
	swap_entries(mm.mm_stack, 0, 19);
	swap_entries(mm.mm_stack, 5, 10);
	swap_entries(mm.mm_stack, 3, 17);

	// Every entry must still be found with its original value
	for (i = 0; i < n; i++) {
		mm_entry_t *e = mm_get(&mm, keys[i]);
		ASSERT_TRUE(e != NULL);
		ASSERT_EQ(e->mme_value, i);
	}
}

/*
 * ml_sort tests
 *
 * ml_sort sorts a move_list_t by mle_score in descending order.
 */

TEST(test_ml_sort_empty)
{
	move_entry_t arr[1] = {{0, 42}};
	move_list_t ml;
	INIT_STACK(&ml, arr, 1);
	ml.ml_len = 0;
	ml_sort(&ml);
	ASSERT_EQ(arr[0].mle_score, 42); /* untouched */
}

TEST(test_ml_sort_single)
{
	move_entry_t arr[] = {{0, 7}};
	move_list_t ml;
	INIT_STACK(&ml, arr, 1);
	ml.ml_len = 1;
	ml_sort(&ml);
	ASSERT_EQ(arr[0].mle_score, 7);
}

TEST(test_ml_sort_already_sorted)
{
	move_entry_t arr[] = {{0,50},{0,40},{0,30},{0,20},{0,10}};
	move_list_t ml;
	INIT_STACK(&ml, arr, 5);
	ml.ml_len = 5;
	ml_sort(&ml);
	ASSERT_EQ(arr[0].mle_score, 50);
	ASSERT_EQ(arr[1].mle_score, 40);
	ASSERT_EQ(arr[2].mle_score, 30);
	ASSERT_EQ(arr[3].mle_score, 20);
	ASSERT_EQ(arr[4].mle_score, 10);
}

TEST(test_ml_sort_reverse_sorted)
{
	move_entry_t arr[] = {{0,1},{0,2},{0,3},{0,4},{0,5}};
	move_list_t ml;
	INIT_STACK(&ml, arr, 5);
	ml.ml_len = 5;
	ml_sort(&ml);
	ASSERT_EQ(arr[0].mle_score, 5);
	ASSERT_EQ(arr[1].mle_score, 4);
	ASSERT_EQ(arr[2].mle_score, 3);
	ASSERT_EQ(arr[3].mle_score, 2);
	ASSERT_EQ(arr[4].mle_score, 1);
}

TEST(test_ml_sort_random_order)
{
	move_entry_t arr[] = {{0,30},{0,10},{0,50},{0,20},{0,40}};
	move_list_t ml;
	INIT_STACK(&ml, arr, 5);
	ml.ml_len = 5;
	ml_sort(&ml);
	ASSERT_EQ(arr[0].mle_score, 50);
	ASSERT_EQ(arr[1].mle_score, 40);
	ASSERT_EQ(arr[2].mle_score, 30);
	ASSERT_EQ(arr[3].mle_score, 20);
	ASSERT_EQ(arr[4].mle_score, 10);
}

TEST(test_ml_sort_duplicates)
{
	move_entry_t arr[] = {{0,5},{0,3},{0,5},{0,1},{0,3}};
	move_list_t ml;
	INIT_STACK(&ml, arr, 5);
	ml.ml_len = 5;
	ml_sort(&ml);
	ASSERT_EQ(arr[0].mle_score, 5);
	ASSERT_EQ(arr[1].mle_score, 5);
	ASSERT_EQ(arr[2].mle_score, 3);
	ASSERT_EQ(arr[3].mle_score, 3);
	ASSERT_EQ(arr[4].mle_score, 1);
}

TEST(test_ml_sort_all_equal)
{
	move_entry_t arr[] = {{0,99},{0,99},{0,99}};
	move_list_t ml;
	INIT_STACK(&ml, arr, 3);
	ml.ml_len = 3;
	ml_sort(&ml);
	ASSERT_EQ(arr[0].mle_score, 99);
	ASSERT_EQ(arr[1].mle_score, 99);
	ASSERT_EQ(arr[2].mle_score, 99);
}

/*
 * evaluate_board_cached tests
 *
 * The key invariant: after calling evaluate_board_cached, the line map (lm)
 * must be identical to what it was before — same stack_size, same keys,
 * same values, same skipped flags.
 */

// Snapshot of one lm entry for comparison
typedef struct lm_snapshot_entry {
	uint64_t	key;
	int		value;
	bool		skipped;
} lm_snapshot_entry_t;

typedef struct lm_snapshot {
	uint32_t		size;
	lm_snapshot_entry_t	entries[MM_STACK_SIZE];
} lm_snapshot_t;

static void
lm_take_snapshot(move_map_t *lm, lm_snapshot_t *snap)
{
	uint32_t i;

	snap->size = lm->mm_stack_size;
	for (i = 0; i < lm->mm_stack_size; i++) {
		snap->entries[i].key = lm->mm_stack[i].mme_key;
		snap->entries[i].value = lm->mm_stack[i].mme_value;
		snap->entries[i].skipped = lm->mm_stack[i].mme_skipped;
	}
}

// Returns 1 if snapshots match, 0 otherwise. Sets *mismatch_idx on failure.
static int
lm_check_snapshot(move_map_t *lm, lm_snapshot_t *snap, int *mismatch_idx)
{
	uint32_t i;

	if (lm->mm_stack_size != snap->size) {
		*mismatch_idx = -1;
		return 0;
	}
	for (i = 0; i < snap->size; i++) {
		if (lm->mm_stack[i].mme_key != snap->entries[i].key ||
		    lm->mm_stack[i].mme_value != snap->entries[i].value ||
		    lm->mm_stack[i].mme_skipped != snap->entries[i].skipped) {
			*mismatch_idx = i;
			return 0;
		}
	}
	return 1;
}

// These are large, so static.
static move_map_t ebc_mm, ebc_cmm, ebc_lm;
static lm_snapshot_t ebc_snap;

// Helper: build a board, call evaluate_board to populate lm, then insert
// a move, call evaluate_board_cached, and check lm is restored.
// Returns 1 on success, 0 on failure.
static void
ebc_setup(int *as, int *rs, int *cs, int *is_p1s, int num_moves)
{
	mm_init(&ebc_mm);
	populate_move_map(&ebc_mm, as, rs, cs, is_p1s, num_moves);
	evaluate_board(&ebc_mm, &ebc_cmm, &ebc_lm);
}

// Case 3: new tile with no friendly neighbors — creates a new 1-long line
// that should be removed on undo.
TEST(test_ebc_isolated_tile) {
	// Start with one tile at (0,0,0)
	int as[] = {0};
	int rs[] = {0};
	int cs[] = {0};
	int ps[] = {1};

	ebc_setup(as, rs, cs, ps, 1);
	lm_take_snapshot(&ebc_lm, &ebc_snap);

	// Insert a far-away tile that won't connect to anything
	arc_t move = {0, 10, 10};
	mm_insert(&ebc_mm, mm_make_key(&move), 0);
	evaluate_board_cached(&ebc_mm, &ebc_cmm, &ebc_lm, &move, 0);
	mm_remove(&ebc_mm, mm_make_key(&move));

	int idx;
	ASSERT_TRUE(lm_check_snapshot(&ebc_lm, &ebc_snap, &idx));
}

// Case 2: new tile extends a line on its left (reverse) side only.
TEST(test_ebc_extend_left) {
	// Two p1 tiles in a horizontal row: (0,0,0) and (0,0,1)
	int as[] = {0, 0};
	int rs[] = {0, 0};
	int cs[] = {0, 1};
	int ps[] = {1, 0};

	ebc_setup(as, rs, cs, ps, 2);
	lm_take_snapshot(&ebc_lm, &ebc_snap);

	// Add p1 tile at (0,0,2) — extends the line to the right
	// From (0,0,2)'s perspective, the existing line is to its left (step_rev)
	arc_t move = {0, 0, 2};
	mm_insert(&ebc_mm, mm_make_key(&move), 1);
	evaluate_board_cached(&ebc_mm, &ebc_cmm, &ebc_lm, &move, 1);
	mm_remove(&ebc_mm, mm_make_key(&move));

	int idx;
	ASSERT_TRUE(lm_check_snapshot(&ebc_lm, &ebc_snap, &idx));
}

// Case 1: new tile has a friendly line to its right (step direction) only.
TEST(test_ebc_extend_right) {
	// p1 tile at (0,0,1)
	int as[] = {0};
	int rs[] = {0};
	int cs[] = {1};
	int ps[] = {1};

	ebc_setup(as, rs, cs, ps, 1);
	lm_take_snapshot(&ebc_lm, &ebc_snap);

	// Add p1 tile at (0,0,0) — the existing line at (0,0,1) is to its right
	arc_t move = {0, 0, 0};
	mm_insert(&ebc_mm, mm_make_key(&move), 1);
	evaluate_board_cached(&ebc_mm, &ebc_cmm, &ebc_lm, &move, 1);
	mm_remove(&ebc_mm, mm_make_key(&move));

	int idx;
	ASSERT_TRUE(lm_check_snapshot(&ebc_lm, &ebc_snap, &idx));
}

// Case 0: new tile connects two friendly lines.
TEST(test_ebc_connect_two_lines) {
	// p1 at (0,0,0) and p1 at (0,0,2) — gap at (0,0,1)
	int as[] = {0, 0};
	int rs[] = {0, 0};
	int cs[] = {0, 2};
	int ps[] = {1, 1};

	ebc_setup(as, rs, cs, ps, 2);
	lm_take_snapshot(&ebc_lm, &ebc_snap);

	// Fill the gap — connects both lines
	arc_t move = {0, 0, 1};
	mm_insert(&ebc_mm, mm_make_key(&move), 1);
	evaluate_board_cached(&ebc_mm, &ebc_cmm, &ebc_lm, &move, 1);
	mm_remove(&ebc_mm, mm_make_key(&move));

	int idx;
	ASSERT_TRUE(lm_check_snapshot(&ebc_lm, &ebc_snap, &idx));
}

// Multiple existing lines in various directions
TEST(test_ebc_multi_direction) {
	// Build an L-shape: horizontal p1 at (0,0,0)-(0,0,1),
	// and diagonal p1 at (1,0,0)-(0,1,1)
	int as[] = {0, 0, 1, 0};
	int rs[] = {0, 0, 0, 1};
	int cs[] = {0, 1, 0, 1};
	int ps[] = {1, 1, 1, 1};

	ebc_setup(as, rs, cs, ps, 4);
	lm_take_snapshot(&ebc_lm, &ebc_snap);

	// Add a tile that might interact with multiple directions
	arc_t move = {0, 0, 2};
	mm_insert(&ebc_mm, mm_make_key(&move), 0);
	evaluate_board_cached(&ebc_mm, &ebc_cmm, &ebc_lm, &move, 0);
	mm_remove(&ebc_mm, mm_make_key(&move));

	int idx;
	ASSERT_TRUE(lm_check_snapshot(&ebc_lm, &ebc_snap, &idx));
}

// Enemy tile next to a line — should create a terminated 1-long line
TEST(test_ebc_enemy_neighbor) {
	// p1 at (0,0,0), p2 at (0,0,1)
	int as[] = {0, 0};
	int rs[] = {0, 0};
	int cs[] = {0, 1};
	int ps[] = {1, 0};

	ebc_setup(as, rs, cs, ps, 2);
	lm_take_snapshot(&ebc_lm, &ebc_snap);

	// Add p1 at (0,0,2) — has enemy p2 neighbor to left at (0,0,1)
	arc_t move = {0, 0, 2};
	mm_insert(&ebc_mm, mm_make_key(&move), 1);
	evaluate_board_cached(&ebc_mm, &ebc_cmm, &ebc_lm, &move, 1);
	mm_remove(&ebc_mm, mm_make_key(&move));

	int idx;
	ASSERT_TRUE(lm_check_snapshot(&ebc_lm, &ebc_snap, &idx));
}

// Larger board — use the game state from the debug main function
TEST(test_ebc_large_board) {
	int as[] = {0, 0, 1, 1, 0, 1, 1, 1, 0, 1, 0, 0, 1, 1, 0, 0, 0, 1,
		    0, 1, 1, 1, 0, 0, 0, 0, 1, 0, 1, 0, 1, 0, 1};
	int rs[] = {0, 0, -1, -1, -1, -2, 0, -1, -1, -1, -1, 0, -2, 0, -2,
		    -2, 0, -3, 0, -2, -2, -2, -2, -1, -1, -1, -1, -2, -3,
		    -3, -3, -3, -4};
	int cs[] = {0, 1, -1, 0, 1, 1, -1, 1, 0, 2, 2, 2, 0, 2, 0, 1, 3,
		    1, 4, -1, -2, -3, -2, -3, -2, -4, -3, -1, -1, 0, -2,
		    -1, -2};
	int ps[] = {1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0,
		    0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1};

	ebc_setup(as, rs, cs, ps, 33);
	lm_take_snapshot(&ebc_lm, &ebc_snap);

	// Add a move and check lm is restored
	arc_t move = {0, -4, -2};
	mm_insert(&ebc_mm, mm_make_key(&move), 0);
	evaluate_board_cached(&ebc_mm, &ebc_cmm, &ebc_lm, &move, 0);
	mm_remove(&ebc_mm, mm_make_key(&move));

	int idx;
	ASSERT_TRUE(lm_check_snapshot(&ebc_lm, &ebc_snap, &idx));
}

int
main(int argc, char const *argv[])
{
	printf("Step functions:\n");
	RUN(test_step_r);
	RUN(test_step_l);
	RUN(test_step_dr_a0);
	RUN(test_step_dr_a1);
	RUN(test_step_dl_a0);
	RUN(test_step_dl_a1);
	RUN(test_step_ur_a0);
	RUN(test_step_ur_a1);
	RUN(test_step_ul_a0);
	RUN(test_step_ul_a1);
	RUN(test_step_r_l_inverse);
	RUN(test_step_dr_ul_inverse);
	RUN(test_step_dl_ur_inverse);

	printf("\nKey encoding:\n");
	RUN(test_key_roundtrip_positive);
	RUN(test_key_roundtrip_negative);
	RUN(test_key_roundtrip_zero);
	RUN(test_key_roundtrip_mixed);
	RUN(test_key_roundtrip_minus_one);
	RUN(test_key_distinct);

	printf("\nDir key encoding:\n");
	RUN(test_dir_key_roundtrip_all_dirs);
	RUN(test_dir_key_does_not_corrupt_pos);
	RUN(test_dir_key_zero_base);
	RUN(test_dir_key_distinct_dirs);
	RUN(test_dir_key_negative_coords);

	printf("\nMove map:\n");
	RUN(test_mm_insert_get);
	RUN(test_mm_get_missing);
	RUN(test_mm_insert_multiple);
	RUN(test_mm_remove_lifo);
	RUN(test_mm_insert_remove_reinsert);

	printf("\nqselect:\n");
	RUN(test_qselect_k0);
	RUN(test_qselect_k2);
	RUN(test_qselect_last);
	RUN(test_qselect_single);
	RUN(test_qselect_equal_values);

	printf("\nswap_entries:\n");
	RUN(test_swap_entries_get_still_works);
	RUN(test_swap_entries_slots_changed);
	RUN(test_swap_entries_insert_after);
	RUN(test_swap_entries_remove_after);
	RUN(test_swap_entries_adjacent);
	RUN(test_swap_entries_same_bucket);
	RUN(test_swap_entries_same_bucket_rev);
	RUN(test_swap_entries_many);

	printf("\nml_sort:\n");
	RUN(test_ml_sort_empty);
	RUN(test_ml_sort_single);
	RUN(test_ml_sort_already_sorted);
	RUN(test_ml_sort_reverse_sorted);
	RUN(test_ml_sort_random_order);
	RUN(test_ml_sort_duplicates);
	RUN(test_ml_sort_all_equal);

	printf("\nscore_line:\n");
	RUN(test_score_line_single_tile_open);
	RUN(test_score_line_single_tile_terminated);
	RUN(test_score_line_two_in_row);
	RUN(test_score_line_not_line_start);
	RUN(test_score_line_p2);
	RUN(test_score_line_fully_blocked);
	RUN(test_score_line_open_left_blocked_right);
	RUN(test_score_line_six_in_row_p1);
	RUN(test_score_line_six_in_row_p2);
	RUN(test_score_line_diagonal);
	RUN(test_score_line_far_arc_position);

	printf("\nevaluate_board_cached:\n");
	RUN(test_ebc_isolated_tile);
	RUN(test_ebc_extend_left);
	RUN(test_ebc_extend_right);
	RUN(test_ebc_connect_two_lines);
	RUN(test_ebc_multi_direction);
	RUN(test_ebc_enemy_neighbor);
	RUN(test_ebc_large_board);

	printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
	return tests_passed == tests_run ? 0 : 1;
}

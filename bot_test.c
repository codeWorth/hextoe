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

TEST(test_qselect_k0) {
	// Find the largest element (k=0 means 0th position in descending order)
	mm_entry_t entries[5] = {
		{NULL, 10, 3},
		{NULL, 20, 7},
		{NULL, 30, 1},
		{NULL, 40, 9},
		{NULL, 50, 5},
	};
	mm_entry_t *result = mm_entries_qselect(entries, 5, 0);
	ASSERT_EQ(result->mme_value, 9);
}

TEST(test_qselect_k2) {
	mm_entry_t entries[5] = {
		{NULL, 10, 3},
		{NULL, 20, 7},
		{NULL, 30, 1},
		{NULL, 40, 9},
		{NULL, 50, 5},
	};
	// k=2: third largest (9, 7, 5, ...)
	mm_entry_t *result = mm_entries_qselect(entries, 5, 2);
	ASSERT_EQ(result->mme_value, 5);
}

TEST(test_qselect_last) {
	mm_entry_t entries[4] = {
		{NULL, 10, 50},
		{NULL, 20, 10},
		{NULL, 30, 30},
		{NULL, 40, 20},
	};
	// k=3: smallest
	mm_entry_t *result = mm_entries_qselect(entries, 4, 3);
	ASSERT_EQ(result->mme_value, 10);
}

TEST(test_qselect_single) {
	mm_entry_t entries[1] = {
		{NULL, 99, 42},
	};
	mm_entry_t *result = mm_entries_qselect(entries, 1, 0);
	ASSERT_EQ(result->mme_value, 42);
}

TEST(test_qselect_equal_values) {
	mm_entry_t entries[4] = {
		{NULL, 1, 5},
		{NULL, 2, 5},
		{NULL, 3, 5},
		{NULL, 4, 5},
	};
	mm_entry_t *result = mm_entries_qselect(entries, 4, 0);
	ASSERT_EQ(result->mme_value, 5);
	result = mm_entries_qselect(entries, 4, 3);
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
// score_line from the tile stepping right:
//   prev (left) is empty -> not terminated
//   next (right) is empty -> end of line, length 1
//   score = 1^3 * 2 * 1 = 2 (p1, unterminated, cubed)
//   returns 2 candidate moves (prev and next)
TEST(test_score_line_single_tile_open) {
	sl_reset();
	sl_add(0, 0, 0, 1);

	arc_t pos = {0, 0, 0};
	mm_entry_t *entry = sl_get(0, 0, 0);
	int score;
	uint64_t move1, move2;
	int mcount = score_line(&sl_mm, entry, &pos, &score,
				&move1, &move2, step_r, step_l);

	ASSERT_EQ(mcount, 2);
	ASSERT_EQ(score, 2);  // 1^3 * 1 * 2 = 2
}

// Single tile with opponent on the left.
// Terminated on prev side, open on right.
// score = 1^3 * 1 = 1
// returns 1 candidate move (next)
TEST(test_score_line_single_tile_terminated) {
	sl_reset();
	sl_add(0, 0, -1, 0);  // opponent to the left
	sl_add(0, 0, 0, 1);

	arc_t pos = {0, 0, 0};
	mm_entry_t *entry = sl_get(0, 0, 0);
	int score;
	uint64_t move1, move2;
	int mcount = score_line(&sl_mm, entry, &pos, &score,
				&move1, &move2, step_r, step_l);

	ASSERT_EQ(mcount, 1);
	ASSERT_EQ(score, 1);  // 1^3 * 1 = 1
}

// Two p1 tiles in a row. Scoring from the left end (start of line).
// (0,0,0) -> (0,0,1) -> empty
// prev of (0,0,0) is (0,0,-1) = empty -> not terminated
// stepping right: (0,0,1) is same player, (0,0,2) is empty
// length = 2, score = 2^3 * 1 * 2 = 16 (unterminated)
TEST(test_score_line_two_in_row) {
	sl_reset();
	sl_add(0, 0, 0, 1);
	sl_add(0, 0, 1, 1);

	arc_t pos = {0, 0, 0};
	mm_entry_t *entry = sl_get(0, 0, 0);
	int score;
	uint64_t move1, move2;
	int mcount = score_line(&sl_mm, entry, &pos, &score,
				&move1, &move2, step_r, step_l);

	ASSERT_EQ(mcount, 2);
	ASSERT_EQ(score, 16);  // 2^3 * 2 = 16
}

// Scoring from the second tile (not the line start) should return 0
// because prev is same player.
TEST(test_score_line_not_line_start) {
	sl_reset();
	sl_add(0, 0, 0, 1);
	sl_add(0, 0, 1, 1);

	arc_t pos = {0, 0, 1};
	mm_entry_t *entry = sl_get(0, 0, 1);
	int score;
	uint64_t move1, move2;
	int mcount = score_line(&sl_mm, entry, &pos, &score,
				&move1, &move2, step_r, step_l);

	ASSERT_EQ(score, 0);
	ASSERT_EQ(mcount, 0);
}

// P2 single tile, open both sides.
// score = 1^3 * (-1) * 2 = -2
TEST(test_score_line_p2) {
	sl_reset();
	sl_add(0, 0, 0, 0);

	arc_t pos = {0, 0, 0};
	mm_entry_t *entry = sl_get(0, 0, 0);
	int score;
	uint64_t move1, move2;
	int mcount = score_line(&sl_mm, entry, &pos, &score,
				&move1, &move2, step_r, step_l);

	ASSERT_EQ(mcount, 2);
	ASSERT_EQ(score, -2);  // 1^3 * -1 * 2 = -2
}

// Blocked on both sides: opponent left, opponent right.
// terminated on prev, opponent on next at i=0.
// Both sides blocked -> score = 0
TEST(test_score_line_fully_blocked) {
	sl_reset();
	sl_add(0, 0, -1, 0);  // opponent left
	sl_add(0, 0, 0, 1);
	sl_add(0, 0, 1, 0);   // opponent right

	arc_t pos = {0, 0, 0};
	mm_entry_t *entry = sl_get(0, 0, 0);
	int score;
	uint64_t move1, move2;
	int mcount = score_line(&sl_mm, entry, &pos, &score,
				&move1, &move2, step_r, step_l);

	ASSERT_EQ(score, 0);
	ASSERT_EQ(mcount, 0);
}

// Open left, opponent right after 1 tile.
// Not terminated on left, opponent at i=0 on right.
// score = 1^3 * 1 = 1, 1 move (prev)
TEST(test_score_line_open_left_blocked_right) {
	sl_reset();
	sl_add(0, 0, 0, 1);
	sl_add(0, 0, 1, 0);   // opponent right

	arc_t pos = {0, 0, 0};
	mm_entry_t *entry = sl_get(0, 0, 0);
	int score;
	uint64_t move1, move2;
	int mcount = score_line(&sl_mm, entry, &pos, &score,
				&move1, &move2, step_r, step_l);

	ASSERT_EQ(mcount, 1);
	ASSERT_EQ(score, 1);  // 1^3 * 1 = 1
}

// 6 in a row = P1_WON
TEST(test_score_line_six_in_row_p1) {
	sl_reset();
	int i;
	for (i = 0; i < 6; i++) {
		sl_add(0, 0, i, 1);
	}

	arc_t pos = {0, 0, 0};
	mm_entry_t *entry = sl_get(0, 0, 0);
	int score;
	uint64_t move1, move2;
	score_line(&sl_mm, entry, &pos, &score,
		   &move1, &move2, step_r, step_l);

	ASSERT_EQ(score, P1_WON);
}

// 6 in a row for p2 = P2_WON
TEST(test_score_line_six_in_row_p2) {
	sl_reset();
	int i;
	for (i = 0; i < 6; i++) {
		sl_add(0, 0, i, 0);
	}

	arc_t pos = {0, 0, 0};
	mm_entry_t *entry = sl_get(0, 0, 0);
	int score;
	uint64_t move1, move2;
	score_line(&sl_mm, entry, &pos, &score,
		   &move1, &move2, step_r, step_l);

	ASSERT_EQ(score, P2_WON);
}

// Three in a row, open both sides, using DR/UL direction.
// Start at (0,0,0). DR steps: (0,0,0)->(1,0,0)->(0,1,1)->(1,1,1)
// Let's place tiles and score from the UL end.
TEST(test_score_line_diagonal) {
	sl_reset();
	// Place 3 tiles going down-right from (0,0,0)
	// (0,0,0) -> step_dr -> (1,0,0) -> step_dr -> (0,1,1)
	sl_add(0, 0, 0, 1);
	sl_add(1, 0, 0, 1);
	sl_add(0, 1, 1, 1);

	arc_t pos = {0, 0, 0};
	mm_entry_t *entry = sl_get(0, 0, 0);
	int score;
	uint64_t move1, move2;
	int mcount = score_line(&sl_mm, entry, &pos, &score,
				&move1, &move2, step_dr, step_ul);

	// 3 tiles, open both sides: score = 3^3 * 1 * 2 = 54
	ASSERT_EQ(mcount, 2);
	ASSERT_EQ(score, 54);
}

// Verify candidate move keys decode to sensible positions
TEST(test_score_line_candidate_positions) {
	sl_reset();
	sl_add(0, 0, 0, 1);

	arc_t pos = {0, 0, 0};
	mm_entry_t *entry = sl_get(0, 0, 0);
	int score;
	uint64_t move1, move2;
	int mcount = score_line(&sl_mm, entry, &pos, &score,
				&move1, &move2, step_r, step_l);

	ASSERT_EQ(mcount, 2);

	// move1 should be left of (0,0,0) -> (0,0,-1)
	arc_t m1;
	pos_from_mm_key(move1, &m1);
	ASSERT_EQ(m1.a, 0);
	ASSERT_EQ(m1.r, 0);
	ASSERT_EQ(m1.c, -1);

	// move2 should be right of (0,0,0) -> (0,0,1)
	arc_t m2;
	pos_from_mm_key(move2, &m2);
	ASSERT_EQ(m2.a, 0);
	ASSERT_EQ(m2.r, 0);
	ASSERT_EQ(m2.c, 1);
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
	RUN(test_score_line_candidate_positions);

	printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
	return tests_passed == tests_run ? 0 : 1;
}

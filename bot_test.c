#include "bot.c"
#include <stdio.h>
#include <string.h>

/* Forward declarations for non-public functions under test */
extern void swap_entries(mm_entry_t *, int, int);

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

/* ================================================================
 * Step function tests
 *
 * Reference (from JS):
 *   stepR(a, r, c)  = { a,   r,          c+1       }
 *   stepL(a, r, c)  = { a,   r,          c-1       }
 *   stepDR(a, r, c) = { 1-a, r+(a&1),    c+(a&1)   }
 *   stepDL(a, r, c) = { 1-a, r+(a&1),    c-(na&1)  }  na=1-a
 *   stepUR(a, r, c) = { 1-a, r-(na&1),   c+(a&1)   }  na=1-a
 *   stepUL(a, r, c) = { 1-a, r-(na&1),   c-(na&1)  }  na=1-a
 * ================================================================ */

TEST(test_step_r) {
	arc_t in = {.a = 0, .r = 3, .c = 5}, out;
	step_r(&in, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, 3);
	ASSERT_EQ(out.c, 6);

	arc_t in2 = {.a = 1, .r = -2, .c = -1}, out2;
	step_r(&in2, &out2);
	ASSERT_EQ(out2.a, 1);
	ASSERT_EQ(out2.r, -2);
	ASSERT_EQ(out2.c, 0);
}

TEST(test_step_l) {
	arc_t in = {.a = 0, .r = 3, .c = 5}, out;
	step_l(&in, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, 3);
	ASSERT_EQ(out.c, 4);

	arc_t in2 = {.a = 1, .r = 0, .c = 0}, out2;
	step_l(&in2, &out2);
	ASSERT_EQ(out2.a, 1);
	ASSERT_EQ(out2.r, 0);
	ASSERT_EQ(out2.c, -1);
}

TEST(test_step_dr_a0) {
	arc_t in = {.a = 0, .r = 3, .c = 5}, out;
	step_dr(&in, &out);
	ASSERT_EQ(out.a, 1);
	ASSERT_EQ(out.r, 3);
	ASSERT_EQ(out.c, 5);
}

TEST(test_step_dr_a1) {
	arc_t in = {.a = 1, .r = 3, .c = 5}, out;
	step_dr(&in, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, 4);
	ASSERT_EQ(out.c, 6);
}

TEST(test_step_dl_a0) {
	arc_t in = {.a = 0, .r = 3, .c = 5}, out;
	step_dl(&in, &out);
	ASSERT_EQ(out.a, 1);
	ASSERT_EQ(out.r, 3);
	ASSERT_EQ(out.c, 4);
}

TEST(test_step_dl_a1) {
	arc_t in = {.a = 1, .r = 3, .c = 5}, out;
	step_dl(&in, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, 4);
	ASSERT_EQ(out.c, 5);
}

TEST(test_step_ur_a0) {
	arc_t in = {.a = 0, .r = 3, .c = 5}, out;
	step_ur(&in, &out);
	ASSERT_EQ(out.a, 1);
	ASSERT_EQ(out.r, 2);
	ASSERT_EQ(out.c, 5);
}

TEST(test_step_ur_a1) {
	arc_t in = {.a = 1, .r = 3, .c = 5}, out;
	step_ur(&in, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, 3);
	ASSERT_EQ(out.c, 6);
}

TEST(test_step_ul_a0) {
	arc_t in = {.a = 0, .r = 3, .c = 5}, out;
	step_ul(&in, &out);
	ASSERT_EQ(out.a, 1);
	ASSERT_EQ(out.r, 2);
	ASSERT_EQ(out.c, 4);
}

TEST(test_step_ul_a1) {
	arc_t in = {.a = 1, .r = 3, .c = 5}, out;
	step_ul(&in, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, 3);
	ASSERT_EQ(out.c, 5);
}

TEST(test_step_r_l_inverse) {
	arc_t in = {.a = 0, .r = -3, .c = 7}, mid, out;
	step_r(&in, &mid);
	step_l(&mid, &out);
	ASSERT_EQ(out.a, in.a);
	ASSERT_EQ(out.r, in.r);
	ASSERT_EQ(out.c, in.c);
}

TEST(test_step_dr_ul_inverse) {
	arc_t in0 = {.a = 0, .r = -3, .c = 7}, mid0, out0;
	step_dr(&in0, &mid0);
	step_ul(&mid0, &out0);
	ASSERT_EQ(out0.a, in0.a);
	ASSERT_EQ(out0.r, in0.r);
	ASSERT_EQ(out0.c, in0.c);

	arc_t in1 = {.a = 1, .r = 2, .c = -4}, mid1, out1;
	step_dr(&in1, &mid1);
	step_ul(&mid1, &out1);
	ASSERT_EQ(out1.a, in1.a);
	ASSERT_EQ(out1.r, in1.r);
	ASSERT_EQ(out1.c, in1.c);
}

TEST(test_step_dl_ur_inverse) {
	arc_t in0 = {.a = 0, .r = -3, .c = 7}, mid0, out0;
	step_dl(&in0, &mid0);
	step_ur(&mid0, &out0);
	ASSERT_EQ(out0.a, in0.a);
	ASSERT_EQ(out0.r, in0.r);
	ASSERT_EQ(out0.c, in0.c);

	arc_t in1 = {.a = 1, .r = 2, .c = -4}, mid1, out1;
	step_dl(&in1, &mid1);
	step_ur(&mid1, &out1);
	ASSERT_EQ(out1.a, in1.a);
	ASSERT_EQ(out1.r, in1.r);
	ASSERT_EQ(out1.c, in1.c);
}

/* ================================================================
 * bot_util.c: encode_arc / deocde_move tests
 * ================================================================ */

TEST(test_encode_roundtrip_positive) {
	arc_t in = {.a = 1, .r = 5, .c = 10}, out;
	uint64_t key = encode_arc(&in);
	deocde_move(key, &out);
	ASSERT_EQ(out.a, 1);
	ASSERT_EQ(out.r, 5);
	ASSERT_EQ(out.c, 10);
}

TEST(test_encode_roundtrip_negative) {
	arc_t in = {.a = 0, .r = -7, .c = -3}, out;
	uint64_t key = encode_arc(&in);
	deocde_move(key, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, -7);
	ASSERT_EQ(out.c, -3);
}

TEST(test_encode_roundtrip_zero) {
	arc_t in = {.a = 0, .r = 0, .c = 0}, out;
	uint64_t key = encode_arc(&in);
	ASSERT_EQ(key, 0ULL);
	deocde_move(key, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, 0);
	ASSERT_EQ(out.c, 0);
}

TEST(test_encode_roundtrip_mixed) {
	arc_t in = {.a = 1, .r = -100, .c = 200}, out;
	uint64_t key = encode_arc(&in);
	deocde_move(key, &out);
	ASSERT_EQ(out.a, 1);
	ASSERT_EQ(out.r, -100);
	ASSERT_EQ(out.c, 200);
}

TEST(test_encode_roundtrip_minus_one) {
	arc_t in = {.a = 0, .r = -1, .c = -1}, out;
	uint64_t key = encode_arc(&in);
	deocde_move(key, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, -1);
	ASSERT_EQ(out.c, -1);
}

TEST(test_encode_distinct) {
	arc_t a1 = {.a = 0, .r = 1, .c = 2};
	arc_t a2 = {.a = 0, .r = 1, .c = 3};
	arc_t a3 = {.a = 1, .r = 1, .c = 2};
	ASSERT_TRUE(encode_arc(&a1) != encode_arc(&a2));
	ASSERT_TRUE(encode_arc(&a1) != encode_arc(&a3));
	ASSERT_TRUE(encode_arc(&a2) != encode_arc(&a3));
}

/* ================================================================
 * bot_util.c: splitmix64 / fnv_hash tests
 * ================================================================ */

TEST(test_splitmix64_deterministic) {
	ASSERT_EQ(splitmix64(0), splitmix64(0));
	ASSERT_EQ(splitmix64(42), splitmix64(42));
}

TEST(test_splitmix64_different_inputs) {
	ASSERT_TRUE(splitmix64(0) != splitmix64(1));
	ASSERT_TRUE(splitmix64(100) != splitmix64(101));
}

TEST(test_fnv_hash_deterministic) {
	ASSERT_EQ(fnv_hash(0), fnv_hash(0));
	ASSERT_EQ(fnv_hash(42), fnv_hash(42));
}

TEST(test_fnv_hash_different_inputs) {
	ASSERT_TRUE(fnv_hash(0) != fnv_hash(1));
	ASSERT_TRUE(fnv_hash(100) != fnv_hash(101));
}

/* ================================================================
 * bot_util.c: ml_sort tests
 * ================================================================ */

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

TEST(test_ml_sort_preserves_moves)
{
	move_entry_t arr[] = {{111,30},{222,10},{333,50}};
	move_list_t ml;
	INIT_STACK(&ml, arr, 3);
	ml.ml_len = 3;
	ml_sort(&ml);
	/* sorted descending by score: 50, 30, 10 */
	ASSERT_EQ(arr[0].mle_move, 333);
	ASSERT_EQ(arr[1].mle_move, 111);
	ASSERT_EQ(arr[2].mle_move, 222);
}

/* ================================================================
 * move_map.c: mm_arc2key / mm_decode_key tests
 * ================================================================ */

TEST(test_mm_key_roundtrip_positive) {
	arc_t in = {.a = 1, .r = 5, .c = 10}, out;
	mm_key key = mm_arc2key(&in);
	mm_decode_key(key, &out);
	ASSERT_EQ(out.a, 1);
	ASSERT_EQ(out.r, 5);
	ASSERT_EQ(out.c, 10);
}

TEST(test_mm_key_roundtrip_negative) {
	arc_t in = {.a = 0, .r = -7, .c = -3}, out;
	mm_key key = mm_arc2key(&in);
	mm_decode_key(key, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, -7);
	ASSERT_EQ(out.c, -3);
}

TEST(test_mm_key_roundtrip_zero) {
	arc_t in = {.a = 0, .r = 0, .c = 0}, out;
	mm_key key = mm_arc2key(&in);
	mm_decode_key(key, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, 0);
	ASSERT_EQ(out.c, 0);
}

/* ================================================================
 * move_map.c: mm_init / mm_insert / mm_get / mm_remove tests
 * ================================================================ */

TEST(test_mm_insert_get) {
	move_map_t mm;
	mm_init(&mm);

	arc_t pos = {.a = 0, .r = 1, .c = 2};
	mm_key key = mm_arc2key(&pos);
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

	arc_t pos = {.a = 0, .r = 5, .c = 5};
	mm_key key = mm_arc2key(&pos);
	ASSERT_NULL(mm_get(&mm, key));
}

TEST(test_mm_insert_multiple) {
	move_map_t mm;
	mm_init(&mm);

	arc_t p1 = {.a = 0, .r = 0, .c = 0}, p2 = {.a = 1, .r = 1, .c = 1}, p3 = {.a = 0, .r = -1, .c = -1};
	mm_key k1 = mm_arc2key(&p1);
	mm_key k2 = mm_arc2key(&p2);
	mm_key k3 = mm_arc2key(&p3);

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

	arc_t p1 = {.a = 0, .r = 0, .c = 0}, p2 = {.a = 1, .r = 1, .c = 1};
	mm_key k1 = mm_arc2key(&p1);
	mm_key k2 = mm_arc2key(&p2);

	mm_insert(&mm, k1, 1);
	mm_insert(&mm, k2, 0);
	ASSERT_EQ(mm.mm_stack_size, 2);

	mm_remove(&mm, k2);
	ASSERT_EQ(mm.mm_stack_size, 1);
	ASSERT_NULL(mm_get(&mm, k2));
	ASSERT_TRUE(mm_get(&mm, k1) != NULL);

	mm_remove(&mm, k1);
	ASSERT_EQ(mm.mm_stack_size, 0);
	ASSERT_NULL(mm_get(&mm, k1));
}

TEST(test_mm_insert_remove_reinsert) {
	move_map_t mm;
	mm_init(&mm);

	arc_t p1 = {.a = 0, .r = 2, .c = 3};
	mm_key k1 = mm_arc2key(&p1);

	mm_insert(&mm, k1, 1);
	ASSERT_EQ(mm.mm_stack_size, 1);

	mm_remove(&mm, k1);
	ASSERT_EQ(mm.mm_stack_size, 0);
	ASSERT_NULL(mm_get(&mm, k1));

	mm_insert(&mm, k1, 0);
	ASSERT_EQ(mm.mm_stack_size, 1);
	ASSERT_EQ(mm_get(&mm, k1)->mme_value, 0);
}

TEST(test_mm_insert_returns_entry) {
	move_map_t mm;
	mm_init(&mm);

	arc_t pos = {.a = 1, .r = -3, .c = 4};
	mm_key key = mm_arc2key(&pos);
	mm_entry_t *entry = mm_insert(&mm, key, 42);

	ASSERT_TRUE(entry != NULL);
	ASSERT_EQ(entry->mme_key, key);
	ASSERT_EQ(entry->mme_value, 42);
}

TEST(test_mm_init_empty) {
	move_map_t mm;
	mm_init(&mm);
	ASSERT_EQ(mm.mm_stack_size, 0);

	/* Any lookup should return NULL */
	arc_t pos = {.a = 0, .r = 0, .c = 0};
	ASSERT_NULL(mm_get(&mm, mm_arc2key(&pos)));
}

/* ================================================================
 * move_map.c: duplicate key tests
 *
 * Inserting the same key twice pushes a second entry onto the stack.
 * mm_get returns the most recent. mm_remove deletes the most recent.
 * ================================================================ */

TEST(test_mm_duplicate_key_get_returns_latest) {
	move_map_t mm;
	mm_init(&mm);

	arc_t pos = {.a = 0, .r = 3, .c = 3};
	mm_key key = mm_arc2key(&pos);

	mm_insert(&mm, key, 10);
	mm_insert(&mm, key, 99);

	ASSERT_EQ(mm.mm_stack_size, 2);
	mm_entry_t *entry = mm_get(&mm, key);
	ASSERT_TRUE(entry != NULL);
	ASSERT_EQ(entry->mme_value, 99);
}

TEST(test_mm_duplicate_key_remove_reveals_old) {
	move_map_t mm;
	mm_init(&mm);

	arc_t pos = {.a = 0, .r = 3, .c = 3};
	mm_key key = mm_arc2key(&pos);

	mm_insert(&mm, key, 10);
	mm_insert(&mm, key, 99);

	/* Remove the top (most recent) */
	mm_remove(&mm, key);
	ASSERT_EQ(mm.mm_stack_size, 1);

	mm_entry_t *entry = mm_get(&mm, key);
	ASSERT_TRUE(entry != NULL);
	ASSERT_EQ(entry->mme_value, 10);

	/* Remove the original */
	mm_remove(&mm, key);
	ASSERT_EQ(mm.mm_stack_size, 0);
	ASSERT_NULL(mm_get(&mm, key));
}

TEST(test_mm_duplicate_key_three_deep) {
	move_map_t mm;
	mm_init(&mm);

	arc_t pos = {.a = 1, .r = -2, .c = 5};
	mm_key key = mm_arc2key(&pos);

	mm_insert(&mm, key, 1);
	mm_insert(&mm, key, 2);
	mm_insert(&mm, key, 3);

	ASSERT_EQ(mm.mm_stack_size, 3);
	ASSERT_EQ(mm_get(&mm, key)->mme_value, 3);

	mm_remove(&mm, key);
	ASSERT_EQ(mm_get(&mm, key)->mme_value, 2);

	mm_remove(&mm, key);
	ASSERT_EQ(mm_get(&mm, key)->mme_value, 1);

	mm_remove(&mm, key);
	ASSERT_NULL(mm_get(&mm, key));
}

TEST(test_mm_duplicate_key_interleaved) {
	move_map_t mm;
	mm_init(&mm);

	arc_t pa = {.a = 0, .r = 0, .c = 0}, pb = {.a = 1, .r = 1, .c = 1};
	mm_key ka = mm_arc2key(&pa);
	mm_key kb = mm_arc2key(&pb);

	mm_insert(&mm, ka, 10);
	mm_insert(&mm, kb, 20);
	mm_insert(&mm, kb, 30);  /* duplicate kb */

	ASSERT_EQ(mm.mm_stack_size, 3);
	ASSERT_EQ(mm_get(&mm, kb)->mme_value, 30);
	ASSERT_EQ(mm_get(&mm, ka)->mme_value, 10);

	/* Remove in LIFO order */
	mm_remove(&mm, kb);
	ASSERT_EQ(mm_get(&mm, kb)->mme_value, 20);

	mm_remove(&mm, kb);
	ASSERT_NULL(mm_get(&mm, kb));
	ASSERT_EQ(mm_get(&mm, ka)->mme_value, 10);
}

/* ================================================================
 * move_map.c: swap_entries tests
 * ================================================================ */

TEST(test_swap_entries_get_still_works) {
	move_map_t mm;
	mm_init(&mm);

	arc_t p0 = {.a = 0, .r = 0, .c = 0}, p1 = {.a = 1, .r = 1, .c = 1}, p2 = {.a = 0, .r = 2, .c = 2};
	mm_key k0 = mm_arc2key(&p0);
	mm_key k1 = mm_arc2key(&p1);
	mm_key k2 = mm_arc2key(&p2);

	mm_insert(&mm, k0, 1);
	mm_insert(&mm, k1, 0);
	mm_insert(&mm, k2, 1);

	swap_entries(mm.mm_stack, 0, 2);

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

	arc_t p0 = {.a = 0, .r = 0, .c = 0}, p1 = {.a = 1, .r = 1, .c = 1}, p2 = {.a = 0, .r = 2, .c = 2};
	mm_key k0 = mm_arc2key(&p0);
	mm_key k1 = mm_arc2key(&p1);
	mm_key k2 = mm_arc2key(&p2);

	mm_insert(&mm, k0, 1);
	mm_insert(&mm, k1, 0);
	mm_insert(&mm, k2, 1);

	ASSERT_EQ(mm.mm_stack[0].mme_key, k0);
	ASSERT_EQ(mm.mm_stack[2].mme_key, k2);

	swap_entries(mm.mm_stack, 0, 2);

	ASSERT_EQ(mm.mm_stack[0].mme_key, k2);
	ASSERT_EQ(mm.mm_stack[2].mme_key, k0);
	ASSERT_EQ(mm.mm_stack[1].mme_key, k1);
}

TEST(test_swap_entries_adjacent) {
	move_map_t mm;
	mm_init(&mm);

	arc_t p0 = {.a = 0, .r = 0, .c = 0}, p1 = {.a = 1, .r = 1, .c = 1};
	mm_key k0 = mm_arc2key(&p0);
	mm_key k1 = mm_arc2key(&p1);

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

TEST(test_swap_entries_insert_after) {
	move_map_t mm;
	mm_init(&mm);

	arc_t p0 = {.a = 0, .r = 0, .c = 0}, p1 = {.a = 1, .r = 1, .c = 1};
	mm_key k0 = mm_arc2key(&p0);
	mm_key k1 = mm_arc2key(&p1);

	mm_insert(&mm, k0, 1);
	mm_insert(&mm, k1, 0);

	swap_entries(mm.mm_stack, 0, 1);

	arc_t p2 = {.a = 0, .r = 3, .c = 3};
	mm_key k2 = mm_arc2key(&p2);
	mm_insert(&mm, k2, 1);

	ASSERT_EQ(mm.mm_stack_size, 3);
	ASSERT_TRUE(mm_get(&mm, k0) != NULL);
	ASSERT_TRUE(mm_get(&mm, k1) != NULL);
	ASSERT_TRUE(mm_get(&mm, k2) != NULL);
	ASSERT_EQ(mm_get(&mm, k2)->mme_value, 1);
}

TEST(test_swap_entries_same_bucket) {
	move_map_t mm;
	mm_init(&mm);

	arc_t pa = {.a = 0, .r = 0, .c = 0};
	mm_key ka = mm_arc2key(&pa);
	uint32_t target_bucket = MME_INDEX(ka);

	arc_t pb;
	mm_key kb;
	int r;
	for (r = 1; r < 1000; r++) {
		pb = (arc_t){.a = 0, .r = r, .c = 0};
		kb = mm_arc2key(&pb);
		if (MME_INDEX(kb) == target_bucket)
			break;
	}
	ASSERT_EQ(MME_INDEX(kb), target_bucket);

	mm_insert(&mm, ka, 1);
	mm_insert(&mm, kb, 0);

	swap_entries(mm.mm_stack, 0, 1);

	ASSERT_TRUE(mm_get(&mm, ka) != NULL);
	ASSERT_TRUE(mm_get(&mm, kb) != NULL);
	ASSERT_EQ(mm_get(&mm, ka)->mme_value, 1);
	ASSERT_EQ(mm_get(&mm, kb)->mme_value, 0);
}

TEST(test_swap_entries_many) {
	move_map_t mm;
	mm_init(&mm);

	int n = 20;
	mm_key keys[20];
	int i;
	for (i = 0; i < n; i++) {
		arc_t p = {.a = i & 1, .r = i * 3, .c = i * 7};
		keys[i] = mm_arc2key(&p);
		mm_insert(&mm, keys[i], i);
	}

	swap_entries(mm.mm_stack, 0, 19);
	swap_entries(mm.mm_stack, 5, 10);
	swap_entries(mm.mm_stack, 3, 17);

	for (i = 0; i < n; i++) {
		mm_entry_t *e = mm_get(&mm, keys[i]);
		ASSERT_TRUE(e != NULL);
		ASSERT_EQ(e->mme_value, i);
	}
}

/* ================================================================
 * move_map.c: mm_entries_qselect tests
 * ================================================================ */

static void
qs_setup(move_map_t *mm, int *values, int n)
{
	int i;
	mm_init(mm);
	for (i = 0; i < n; i++) {
		arc_t p = {.a = 0, .r = i, .c = i * 7};
		mm_key key = mm_arc2key(&p);
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

/* ================================================================
 * line_map.c: lm_mmkey2lmkey / lm_arc2key / lm_key2dir / lm_decode_key
 * ================================================================ */

TEST(test_lm_dir_roundtrip_all_dirs) {
	arc_t pos = {.a = 1, .r = 5, .c = -3};
	mm_key base = mm_arc2key(&pos);
	uint8_t dir;

	for (dir = 0; dir < 3; dir++) {
		lm_key dk = lm_mmkey2lmkey(base, dir);
		ASSERT_EQ(lm_key2dir(dk), dir);
	}
}

TEST(test_lm_dir_does_not_corrupt_pos) {
	arc_t pos = {.a = 0, .r = -7, .c = 12}, out;
	mm_key base = mm_arc2key(&pos);
	uint8_t dir;

	for (dir = 0; dir < 3; dir++) {
		lm_key dk = lm_mmkey2lmkey(base, dir);
		lm_decode_key(dk, &out);
		ASSERT_EQ(out.a, 0);
		ASSERT_EQ(out.r, -7);
		ASSERT_EQ(out.c, 12);
	}
}

TEST(test_lm_arc2key_roundtrip) {
	arc_t pos = {.a = 1, .r = -10, .c = 20}, out;

	lm_key dk = lm_arc2key(&pos, 2);
	ASSERT_EQ(lm_key2dir(dk), 2);
	lm_decode_key(dk, &out);
	ASSERT_EQ(out.a, 1);
	ASSERT_EQ(out.r, -10);
	ASSERT_EQ(out.c, 20);
}

TEST(test_lm_dir_key_zero_base) {
	arc_t pos = {.a = 0, .r = 0, .c = 0}, out;

	lm_key dk = lm_arc2key(&pos, 2);
	ASSERT_EQ(lm_key2dir(dk), 2);
	lm_decode_key(dk, &out);
	ASSERT_EQ(out.a, 0);
	ASSERT_EQ(out.r, 0);
	ASSERT_EQ(out.c, 0);
}

TEST(test_lm_dir_key_distinct_dirs) {
	arc_t pos = {.a = 1, .r = 3, .c = 3};
	lm_key d0 = lm_arc2key(&pos, 0);
	lm_key d1 = lm_arc2key(&pos, 1);
	lm_key d2 = lm_arc2key(&pos, 2);

	ASSERT_TRUE(d0 != d1);
	ASSERT_TRUE(d1 != d2);
	ASSERT_TRUE(d0 != d2);
}

TEST(test_lm_mmkey2lmkey_matches_arc2key) {
	arc_t pos = {.a = 0, .r = -5, .c = 8};
	mm_key base = mm_arc2key(&pos);
	uint8_t dir;

	for (dir = 0; dir < 3; dir++) {
		lm_key from_mm = lm_mmkey2lmkey(base, dir);
		lm_key from_arc = lm_arc2key(&pos, dir);
		ASSERT_EQ(from_mm, from_arc);
	}
}

/* ================================================================
 * line_map.c: lm_init / lm_insert / lm_get / lm_remove tests
 * ================================================================ */

TEST(test_lm_insert_get) {
	line_map_t lm;
	lm_init(&lm);

	arc_t pos = {.a = 0, .r = 1, .c = 2};
	lm_key key = lm_arc2key(&pos, 0);
	lm_insert(&lm, key, 10, 2, 0, 2, 0);

	lm_entry_t *entry = lm_get(&lm, key);
	ASSERT_TRUE(entry != NULL);
	ASSERT_EQ(entry->lme_key, key);
	ASSERT_EQ(entry->lme_score, 10);
	ASSERT_EQ(entry->lme_length, 2);
	ASSERT_EQ(entry->lme_dl_r, 0);
	ASSERT_EQ(entry->lme_dl_c, 2);
	ASSERT_EQ(lm.lm_stack_size, 1);
}

TEST(test_lm_get_missing) {
	line_map_t lm;
	lm_init(&lm);

	arc_t pos = {.a = 0, .r = 5, .c = 5};
	lm_key key = lm_arc2key(&pos, 1);
	ASSERT_NULL(lm_get(&lm, key));
}

TEST(test_lm_insert_multiple) {
	line_map_t lm;
	lm_init(&lm);

	arc_t p1 = {.a = 0, .r = 0, .c = 0}, p2 = {.a = 1, .r = 1, .c = 1};
	lm_key k1 = lm_arc2key(&p1, 0);
	lm_key k2 = lm_arc2key(&p2, 1);

	lm_insert(&lm, k1, 5, 1, 0, 1, 0);
	lm_insert(&lm, k2, -8, 2, 1, 0, LM_TERM_CLOSE);

	ASSERT_EQ(lm.lm_stack_size, 2);
	ASSERT_EQ(lm_get(&lm, k1)->lme_score, 5);
	ASSERT_EQ(lm_get(&lm, k2)->lme_score, -8);
	ASSERT_TRUE(LME_IS_TERM_CLOSE(lm_get(&lm, k2)));
}

TEST(test_lm_remove_lifo) {
	line_map_t lm;
	lm_init(&lm);

	arc_t p1 = {.a = 0, .r = 0, .c = 0}, p2 = {.a = 1, .r = 1, .c = 1};
	lm_key k1 = lm_arc2key(&p1, 0);
	lm_key k2 = lm_arc2key(&p2, 1);

	lm_insert(&lm, k1, 5, 1, 0, 1, 0);
	lm_insert(&lm, k2, 8, 2, 1, 0, 0);
	ASSERT_EQ(lm.lm_stack_size, 2);

	lm_remove(&lm, k2);
	ASSERT_EQ(lm.lm_stack_size, 1);
	ASSERT_NULL(lm_get(&lm, k2));
	ASSERT_TRUE(lm_get(&lm, k1) != NULL);

	lm_remove(&lm, k1);
	ASSERT_EQ(lm.lm_stack_size, 0);
	ASSERT_NULL(lm_get(&lm, k1));
}

TEST(test_lm_insert_remove_reinsert) {
	line_map_t lm;
	lm_init(&lm);

	arc_t pos = {.a = 0, .r = 2, .c = 3};
	lm_key key = lm_arc2key(&pos, 0);

	lm_insert(&lm, key, 10, 1, 0, 1, 0);
	ASSERT_EQ(lm.lm_stack_size, 1);

	lm_remove(&lm, key);
	ASSERT_EQ(lm.lm_stack_size, 0);
	ASSERT_NULL(lm_get(&lm, key));

	lm_insert(&lm, key, -20, 3, 2, 1, LM_TERM_FAR);
	ASSERT_EQ(lm.lm_stack_size, 1);
	lm_entry_t *entry = lm_get(&lm, key);
	ASSERT_TRUE(entry != NULL);
	ASSERT_EQ(entry->lme_score, -20);
	ASSERT_EQ(entry->lme_length, 3);
	ASSERT_TRUE(LME_IS_TERM_FAR(entry));
}

/* ================================================================
 * line_map.c: lm_insert_mask tests
 *
 * Masking inserts a score=0 entry that shadows earlier entries.
 * lm_get returns NULL for masked keys.
 * ================================================================ */

TEST(test_lm_insert_mask_hides_entry) {
	line_map_t lm;
	lm_init(&lm);

	arc_t pos = {.a = 0, .r = 0, .c = 0};
	lm_key key = lm_arc2key(&pos, 0);

	lm_insert(&lm, key, 10, 1, 0, 1, 0);
	ASSERT_TRUE(lm_get(&lm, key) != NULL);

	lm_insert_mask(&lm, key);
	ASSERT_NULL(lm_get(&lm, key));
	ASSERT_EQ(lm.lm_stack_size, 2);
}

TEST(test_lm_insert_mask_remove_reveals) {
	line_map_t lm;
	lm_init(&lm);

	arc_t pos = {.a = 0, .r = 0, .c = 0};
	lm_key key = lm_arc2key(&pos, 0);

	lm_insert(&lm, key, 10, 1, 0, 1, 0);
	lm_insert_mask(&lm, key);
	ASSERT_NULL(lm_get(&lm, key));

	/* Remove mask (LIFO), should reveal the original entry */
	lm_remove(&lm, key);
	ASSERT_EQ(lm.lm_stack_size, 1);
	lm_entry_t *entry = lm_get(&lm, key);
	ASSERT_TRUE(entry != NULL);
	ASSERT_EQ(entry->lme_score, 10);
}

TEST(test_lm_insert_mask_on_empty) {
	line_map_t lm;
	lm_init(&lm);

	arc_t pos = {.a = 1, .r = 3, .c = 3};
	lm_key key = lm_arc2key(&pos, 2);

	/* Mask with nothing underneath */
	lm_insert_mask(&lm, key);
	ASSERT_NULL(lm_get(&lm, key));
	ASSERT_EQ(lm.lm_stack_size, 1);

	lm_remove(&lm, key);
	ASSERT_EQ(lm.lm_stack_size, 0);
}

/* ================================================================
 * line_map.c: lm_insert duplicate key (overlay) behavior
 *
 * Inserting the same key twice should shadow the first; lm_get
 * returns the most recent. Removing in LIFO order restores the old.
 * ================================================================ */

TEST(test_lm_insert_duplicate_key_overlay) {
	line_map_t lm;
	lm_init(&lm);

	arc_t pos = {.a = 0, .r = 0, .c = 0};
	lm_key key = lm_arc2key(&pos, 0);

	lm_insert(&lm, key, 5, 1, 0, 1, 0);
	lm_insert(&lm, key, 30, 3, 2, 3, LM_TERM_CLOSE | LM_TERM_FAR);

	ASSERT_EQ(lm.lm_stack_size, 2);
	lm_entry_t *entry = lm_get(&lm, key);
	ASSERT_TRUE(entry != NULL);
	ASSERT_EQ(entry->lme_score, 30);
	ASSERT_EQ(entry->lme_length, 3);

	/* Remove top, see original */
	lm_remove(&lm, key);
	entry = lm_get(&lm, key);
	ASSERT_TRUE(entry != NULL);
	ASSERT_EQ(entry->lme_score, 5);
	ASSERT_EQ(entry->lme_length, 1);
}

/* ================================================================
 * line_map.c: flag macro tests
 * ================================================================ */

TEST(test_lm_flags_close_term) {
	line_map_t lm;
	lm_init(&lm);

	arc_t pos = {.a = 0, .r = 0, .c = 0};
	lm_key key = lm_arc2key(&pos, 0);

	lm_insert(&lm, key, 10, 1, 0, 1, LM_TERM_CLOSE);
	lm_entry_t *entry = lm_get(&lm, key);
	ASSERT_TRUE(LME_IS_TERM_CLOSE(entry));
	ASSERT_TRUE(!LME_IS_TERM_FAR(entry));
	ASSERT_TRUE(!LME_GET_DEST_A(entry));
}

TEST(test_lm_flags_far_term) {
	line_map_t lm;
	lm_init(&lm);

	arc_t pos = {.a = 0, .r = 0, .c = 0};
	lm_key key = lm_arc2key(&pos, 0);

	lm_insert(&lm, key, 10, 1, 0, 1, LM_TERM_FAR);
	lm_entry_t *entry = lm_get(&lm, key);
	ASSERT_TRUE(!LME_IS_TERM_CLOSE(entry));
	ASSERT_TRUE(LME_IS_TERM_FAR(entry));
}

TEST(test_lm_flags_dest_a) {
	line_map_t lm;
	lm_init(&lm);

	arc_t pos = {.a = 0, .r = 0, .c = 0};
	lm_key key = lm_arc2key(&pos, 0);

	lm_insert(&lm, key, 10, 1, 0, 1, LM_DEST_A);
	lm_entry_t *entry = lm_get(&lm, key);
	ASSERT_TRUE(LME_GET_DEST_A(entry));
	ASSERT_TRUE(!LME_IS_TERM_CLOSE(entry));
	ASSERT_TRUE(!LME_IS_TERM_FAR(entry));
}

TEST(test_lm_flags_all_set) {
	line_map_t lm;
	lm_init(&lm);

	arc_t pos = {.a = 0, .r = 0, .c = 0};
	lm_key key = lm_arc2key(&pos, 0);

	uint8_t flags = LM_TERM_CLOSE | LM_TERM_FAR | LM_DEST_A;
	lm_insert(&lm, key, 10, 1, 0, 1, flags);
	lm_entry_t *entry = lm_get(&lm, key);
	ASSERT_TRUE(LME_IS_TERM_CLOSE(entry));
	ASSERT_TRUE(LME_IS_TERM_FAR(entry));
	ASSERT_TRUE(LME_GET_DEST_A(entry));
}

TEST(test_lm_flags_none_set) {
	line_map_t lm;
	lm_init(&lm);

	arc_t pos = {.a = 0, .r = 0, .c = 0};
	lm_key key = lm_arc2key(&pos, 0);

	lm_insert(&lm, key, 10, 1, 0, 1, 0);
	lm_entry_t *entry = lm_get(&lm, key);
	ASSERT_TRUE(!LME_IS_TERM_CLOSE(entry));
	ASSERT_TRUE(!LME_IS_TERM_FAR(entry));
	ASSERT_TRUE(!LME_GET_DEST_A(entry));
}

/* ================================================================
 * line_map.c: lm_init clears state
 * ================================================================ */

TEST(test_lm_init_empty) {
	line_map_t lm;
	lm_init(&lm);
	ASSERT_EQ(lm.lm_stack_size, 0);

	arc_t pos = {.a = 0, .r = 0, .c = 0};
	ASSERT_NULL(lm_get(&lm, lm_arc2key(&pos, 0)));
}

/* ================================================================
 * score_line tests (bot.c, uses move_map)
 * ================================================================ */

static move_map_t sl_mm;

static void
sl_reset(void)
{
	mm_init(&sl_mm);
}

static uint64_t
sl_add(int a, int r, int c, int is_p1)
{
	arc_t arc = {.a = a, .r = r, .c = c};
	uint64_t key = mm_arc2key(&arc);
	mm_insert(&sl_mm, key, is_p1);
	return key;
}

static mm_entry_t *
sl_get(int a, int r, int c)
{
	arc_t arc = {.a = a, .r = r, .c = c};
	uint64_t key = mm_arc2key(&arc);
	return mm_get(&sl_mm, key);
}

TEST(test_score_line_single_tile_open) {
	sl_reset();
	sl_add(0, 0, 0, 1);

	arc_t pos = {.a = 0, .r = 0, .c = 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len, score;
	int term = score_line(&sl_mm, entry, &pos, &len, &score,
			      &far, step_r, step_l);

	ASSERT_EQ(term, NO_TERM);
	ASSERT_EQ(len, 1);
}

TEST(test_score_line_single_tile_terminated) {
	sl_reset();
	sl_add(0, 0, -1, 0);
	sl_add(0, 0, 0, 1);

	arc_t pos = {.a = 0, .r = 0, .c = 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len, score;
	int term = score_line(&sl_mm, entry, &pos, &len, &score,
			      &far, step_r, step_l);

	ASSERT_EQ(term, CLOSE_TERM);
	ASSERT_EQ(len, 1);
}

TEST(test_score_line_two_in_row) {
	sl_reset();
	sl_add(0, 0, 0, 1);
	sl_add(0, 0, 1, 1);

	arc_t pos = {.a = 0, .r = 0, .c = 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len, score;
	int term = score_line(&sl_mm, entry, &pos, &len, &score,
			      &far, step_r, step_l);

	ASSERT_EQ(term, NO_TERM);
	ASSERT_EQ(len, 2);
}

TEST(test_score_line_not_line_start) {
	sl_reset();
	sl_add(0, 0, 0, 1);
	sl_add(0, 0, 1, 1);

	arc_t pos = {.a = 0, .r = 0, .c = 1}, far;
	mm_entry_t *entry = sl_get(0, 0, 1);
	int len, score;
	int term = score_line(&sl_mm, entry, &pos, &len, &score,
			      &far, step_r, step_l);

	ASSERT_EQ(len, 0);
	ASSERT_EQ(term, 0);
}

TEST(test_score_line_p2) {
	sl_reset();
	sl_add(0, 0, 0, 0);

	arc_t pos = {.a = 0, .r = 0, .c = 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len, score;
	int term = score_line(&sl_mm, entry, &pos, &len, &score,
			      &far, step_r, step_l);

	ASSERT_EQ(term, NO_TERM);
	ASSERT_EQ(len, 1);
	ASSERT_EQ(score, -2);
}

TEST(test_score_line_fully_blocked) {
	sl_reset();
	sl_add(0, 0, -1, 0);
	sl_add(0, 0, 0, 1);
	sl_add(0, 0, 1, 0);

	arc_t pos = {.a = 0, .r = 0, .c = 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len, score;
	int term = score_line(&sl_mm, entry, &pos, &len, &score,
			      &far, step_r, step_l);

	ASSERT_EQ(len, 0);
	ASSERT_EQ(term, 0);
}

TEST(test_score_line_open_left_blocked_right) {
	sl_reset();
	sl_add(0, 0, 0, 1);
	sl_add(0, 0, 1, 0);

	arc_t pos = {.a = 0, .r = 0, .c = 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len, score;
	int term = score_line(&sl_mm, entry, &pos, &len, &score,
			      &far, step_r, step_l);

	ASSERT_EQ(term, FAR_TERM);
	ASSERT_EQ(len, 1);
}

TEST(test_score_line_six_in_row_p1) {
	sl_reset();
	int i;
	for (i = 0; i < 6; i++)
		sl_add(0, 0, i, 1);

	arc_t pos = {.a = 0, .r = 0, .c = 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len, score;
	score_line(&sl_mm, entry, &pos, &len, &score,
		   &far, step_r, step_l);

	ASSERT_EQ(score, P1_WON);
}

TEST(test_score_line_six_in_row_p2) {
	sl_reset();
	int i;
	for (i = 0; i < 6; i++)
		sl_add(0, 0, i, 0);

	arc_t pos = {.a = 0, .r = 0, .c = 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len, score;
	score_line(&sl_mm, entry, &pos, &len, &score,
		   &far, step_r, step_l);

	ASSERT_EQ(score, P2_WON);
}

TEST(test_score_line_diagonal) {
	sl_reset();
	sl_add(0, 0, 0, 1);
	sl_add(1, 0, 0, 1);
	sl_add(0, 1, 1, 1);

	arc_t pos = {.a = 0, .r = 0, .c = 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len, score;
	int term = score_line(&sl_mm, entry, &pos, &len, &score,
			      &far, step_dr, step_ul);

	ASSERT_EQ(term, NO_TERM);
	ASSERT_EQ(len, 3);
}

TEST(test_score_line_far_arc_position) {
	sl_reset();
	sl_add(0, 0, 0, 1);

	arc_t pos = {.a = 0, .r = 0, .c = 0}, far;
	mm_entry_t *entry = sl_get(0, 0, 0);
	int len, score;
	int term = score_line(&sl_mm, entry, &pos, &len, &score,
			      &far, step_r, step_l);

	ASSERT_EQ(term, NO_TERM);
	ASSERT_EQ(far.a, 0);
	ASSERT_EQ(far.r, 0);
	ASSERT_EQ(far.c, 1);
}

/* ================================================================
 * main
 * ================================================================ */

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

	printf("\nencode_arc / deocde_move:\n");
	RUN(test_encode_roundtrip_positive);
	RUN(test_encode_roundtrip_negative);
	RUN(test_encode_roundtrip_zero);
	RUN(test_encode_roundtrip_mixed);
	RUN(test_encode_roundtrip_minus_one);
	RUN(test_encode_distinct);

	printf("\nsplitmix64 / fnv_hash:\n");
	RUN(test_splitmix64_deterministic);
	RUN(test_splitmix64_different_inputs);
	RUN(test_fnv_hash_deterministic);
	RUN(test_fnv_hash_different_inputs);

	printf("\nml_sort:\n");
	RUN(test_ml_sort_empty);
	RUN(test_ml_sort_single);
	RUN(test_ml_sort_already_sorted);
	RUN(test_ml_sort_reverse_sorted);
	RUN(test_ml_sort_random_order);
	RUN(test_ml_sort_duplicates);
	RUN(test_ml_sort_all_equal);
	RUN(test_ml_sort_preserves_moves);

	printf("\nmm_arc2key / mm_decode_key:\n");
	RUN(test_mm_key_roundtrip_positive);
	RUN(test_mm_key_roundtrip_negative);
	RUN(test_mm_key_roundtrip_zero);

	printf("\nMove map:\n");
	RUN(test_mm_insert_get);
	RUN(test_mm_get_missing);
	RUN(test_mm_insert_multiple);
	RUN(test_mm_remove_lifo);
	RUN(test_mm_insert_remove_reinsert);
	RUN(test_mm_insert_returns_entry);
	RUN(test_mm_init_empty);

	printf("\nmm duplicate keys:\n");
	RUN(test_mm_duplicate_key_get_returns_latest);
	RUN(test_mm_duplicate_key_remove_reveals_old);
	RUN(test_mm_duplicate_key_three_deep);
	RUN(test_mm_duplicate_key_interleaved);

	printf("\nswap_entries:\n");
	RUN(test_swap_entries_get_still_works);
	RUN(test_swap_entries_slots_changed);
	RUN(test_swap_entries_adjacent);
	RUN(test_swap_entries_insert_after);
	RUN(test_swap_entries_same_bucket);
	RUN(test_swap_entries_many);

	printf("\nqselect:\n");
	RUN(test_qselect_k0);
	RUN(test_qselect_k2);
	RUN(test_qselect_last);
	RUN(test_qselect_single);
	RUN(test_qselect_equal_values);

	printf("\nlm key encoding:\n");
	RUN(test_lm_dir_roundtrip_all_dirs);
	RUN(test_lm_dir_does_not_corrupt_pos);
	RUN(test_lm_arc2key_roundtrip);
	RUN(test_lm_dir_key_zero_base);
	RUN(test_lm_dir_key_distinct_dirs);
	RUN(test_lm_mmkey2lmkey_matches_arc2key);

	printf("\nLine map:\n");
	RUN(test_lm_init_empty);
	RUN(test_lm_insert_get);
	RUN(test_lm_get_missing);
	RUN(test_lm_insert_multiple);
	RUN(test_lm_remove_lifo);
	RUN(test_lm_insert_remove_reinsert);

	printf("\nlm_insert_mask:\n");
	RUN(test_lm_insert_mask_hides_entry);
	RUN(test_lm_insert_mask_remove_reveals);
	RUN(test_lm_insert_mask_on_empty);

	printf("\nlm duplicate key overlay:\n");
	RUN(test_lm_insert_duplicate_key_overlay);

	printf("\nlm flags:\n");
	RUN(test_lm_flags_close_term);
	RUN(test_lm_flags_far_term);
	RUN(test_lm_flags_dest_a);
	RUN(test_lm_flags_all_set);
	RUN(test_lm_flags_none_set);

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

	printf("\n%d/%d tests passed.\n", tests_passed, tests_run);
	return tests_passed == tests_run ? 0 : 1;
}

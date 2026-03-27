#include <stdio.h>
#include <time.h>
#include "bot.h"
#include "old_bot.h"

#define N_MOVES	28

void
compare_evals(int *as, int *rs, int *cs, int *is_p1s, int i)
{
	int		eval1, eval2;
	move_map_t	mm;
	move_map_t	cmm;
	line_map_t	lm;

	mm_init(&mm);
	populate_move_map_o(&mm, as, rs, cs, is_p1s, i);

	eval1 = evaluate_board_o(&mm, &cmm);
	eval2 = evaluate_board(&mm, &lm);
	printf("Old eval = %d, new eval = %d\n", eval1, eval2);
}

int
main(int argc, char const *argv[])
{
	int	i = 3;
	bool 	found_move;
	int	best_a, best_r, best_c;
	int as[N_MOVES] =  {0, 0, 1, 0, 1, 1, 1, 0, 1, 1, 0, 1, 0, 1, 0, 1, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 1, 0};
	int rs[N_MOVES] =  {0, 0, -1, -1, -1, 0, 0, 0, -2, 0, -2, 0, 1, -1, 1, 1, -1, -2, -1, 1, 1, 2, 1, 1, 2, -1, -1, 0};
	int cs[N_MOVES] =  {0, 1, 0, 0, -1, 0, 1, -1, 0, -2, 1, -1, 0, -2, 2, 2, -1, -2, -2, 0, 1, 1, -1, -1, 2, 1, -3, -3,};
	int is_p1s[N_MOVES] =  {1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1};

again:
	if(i > N_MOVES) {
		return 0;
	}
	printf("i = %d\n", i);
	struct timespec t0, t1;
	clock_gettime(CLOCK_MONOTONIC, &t0);
	found_move = evaluate_ahead(as, rs, cs, is_p1s, i, &best_a, &best_r,
				    &best_c);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	double elapsed = (t1.tv_sec - t0.tv_sec) * 1e3 +
			 (t1.tv_nsec - t0.tv_nsec) / 1e6;
	printf("evaluate_ahead: %.3f ms\n", elapsed);
	if(found_move) {
		printf("%d %d %d\n", best_a, best_r, best_c);
	} else {
		printf("Failed to find move.");
	}

	clock_gettime(CLOCK_MONOTONIC, &t0);
	found_move = old_evaluate_ahead(as, rs, cs, is_p1s, i, &best_a, &best_r,
				    &best_c);
	clock_gettime(CLOCK_MONOTONIC, &t1);
	elapsed = (t1.tv_sec - t0.tv_sec) * 1e3 + (t1.tv_nsec - t0.tv_nsec) / 1e6;
	printf("evaluate_ahead: %.3f ms\n", elapsed);
	if(found_move) {
		printf("%d %d %d\n", best_a, best_r, best_c);
	} else {
		printf("Failed to find move.");
	}
	printf("\n\n");
	i += 3;
	goto again;
}
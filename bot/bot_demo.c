#include <stdio.h>
#include <time.h>
#include "bot.h"
#include "old_bot.h"

int
main(int argc, char const *argv[])
{
	int	i = 31;
	bool 	found_move;
	int	best_a, best_r, best_c;
	int as[31] =  {0, 1, 0, 1, 1, 0, 0, 0, 1, 1, 1, 0, 1, 0, 0, 0, 0, 1, 0, 1, 1, 0, 1, 1, 0, 0, 1, 1, 0, 1, 1};
	int rs[31] =  {0, 0, 0, -1, 0, 1, 1, -1, -1, -1, -2, 0, 0, 1, 1, 1, 1, 1, -1, 1, 0, 0, 0, 1, 0, 2, -1, 0, 0, 0, 0};
	int cs[31] =  {0, 1, 1, 0, -1, -1, 2, 1, -1, -2, 1, -1, -2, 1, 0, -2, 3, -3, 0, -1, -3, 2, 2, -2, -3, -1, -4, -4, -4, -5, 0};
	int is_p1s[31] =  {1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0, 1, 1, 0, 0};

again:
	if(i > 31) {
		return 0;
	}
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
	return 0;

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
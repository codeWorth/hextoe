#ifndef __OLD_BOT__
#define __OLD_BOT__

#include <stdbool.h>

extern bool old_evaluate_ahead(int*, int*, int*, int*, int, int*, int*, int*);
extern int evaluate_board_o(move_map_t *, move_map_t *);
extern void populate_move_map_o(move_map_t *, int *, int *, int *, int *, int);

#endif
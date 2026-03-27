#ifndef __BOT__
#define __BOT__

#include <stdbool.h>
#include "line_map.h"

extern bool evaluate_ahead(int*, int*, int*, int*, int, int*, int*, int*);
extern int evaluate_board(move_map_t *, line_map_t *);

#endif
#ifndef __ARC__
#define __ARC__

#include <stdint.h>
#include <stdbool.h>

typedef struct arc {
	int32_t	r;
	int32_t	c;
	bool	a;
} arc_t;

extern void arc_increment(arc_t *, arc_t *);
extern void arc_mulinc(arc_t *, arc_t *, bool);

#endif
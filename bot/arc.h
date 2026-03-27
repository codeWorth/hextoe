#ifndef __ARC__
#define __ARC__

#include <stdint.h>

typedef struct arc {
	int32_t	r;
	int32_t	c;
	uint8_t	a;
} arc_t;

extern void arc_increment(arc_t *, arc_t *);
extern void arc_mulinc(arc_t *, arc_t *, uint8_t);

#endif
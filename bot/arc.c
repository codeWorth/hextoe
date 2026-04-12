#include "arc.h"

void
arc_increment(arc_t *dst, arc_t *inc)
{

	dst->r = dst->r + inc->r + (dst->a & inc->a);
	dst->c = dst->c + inc->c + (dst->a & inc->a);
	dst->a = (dst->a ^ inc->a) & 1;
}

void
arc_mulinc(arc_t *dst, arc_t *inc, bool n)
{
	bool		a2;
	int32_t		r2;
	int32_t		c2;

	// This multiplies inc by n
	a2 = inc->a & (n & 1);
	r2 = n * inc->r + (inc->a ? (n >> 1) : 0);
	c2 = n * inc->c + (inc->a ? (n >> 1) : 0);
	// Then we add inc to dst
	dst->r = dst->r + r2 + (dst->a & a2);
	dst->c = dst->c + c2 + (dst->a & a2);
	dst->a = (dst->a ^ a2) & 1;
}

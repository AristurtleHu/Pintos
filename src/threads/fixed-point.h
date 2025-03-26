#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

typedef int32_t fixed_t;
#define fixed_t_size 32
#define fixed_p 17
#define fixed_q 14
#define F (1 << 14)

#define INT_TO_FIXED(n) ((n) * F)
//in: int , out: fix_t, return type: fix_t
#define FIXED_TO_INT(x) ((x) / F)
//in: fix_t , out: int, return type: int
#define ROUND_FIXED(x) ((x) >= 0 ? ((x) + F/2) / F : ((x) - F / 2) / F)
//in: fix_t , out: int, return type: int (rounding to nearest)
#define ADD_FIXED(x, y) ((x) + (y))
//in: fix_t , fix_t, out: fix_t, return type: fix_t
#define SUB_FIXED(x, y) ((x) - (y))
//in: fix_t , fix_t, out: fix_t, return type: fix_t
#define ADD_INT(x, n) ((x) + (n) * F)
//in: fix_t , int, out: fix_t, return type: fix_t
#define SUB_INT(x, n) ((x) - INT_TO_FIXED(n))
//in: fix_t , int, out: fix_t, return type: fix_t
#define MUL_FIXED(x, y) ((fixed_t)(((int64_t)(x)) * (y) / F))
//in: fix_t , fix_t, out: fix_t, return type: fix_t
#define MUL_INT(x, n) ((x) * (n))
//in: fix_t , int, out: fix_t, return type: fix_t
#define DIV_FIXED(x, y) ((fixed_t)(((int64_t)(x) * F) / (y)))
//in: fix_t , fix_t, out: fix_t, return type: fix_t
#define DIV_INT(x, n) ((x) / (n))
//in: fix_t , int, out: fix_t, return type: fix_t


#endif /* threads/fixed-point.h */
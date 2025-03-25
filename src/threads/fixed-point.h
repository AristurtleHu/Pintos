#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <stdint.h>

typedef int32_t fixed_t;
#define fixed_t_size 32
#define fixed_p 17
#define fixed_q 14
#define F (1 << 14)

#define INT_TO_FIXED(n) ((n) * F)
#define FIXED_TO_INT(x) ((x) / F)
#define ROUND_FIXED(x) ((x) >= 0 ? ((x) + F/2) / F : ((x) - F / 2) / F)
#define ADD_FIXED(x, y) ((x) + (y))
#define SUB_FIXED(x, y) ((x) - (y))
#define ADD_INT(x, n) ((x) + (n) * F)
#define SUB_INT(x, n) (x - n * F)
#define MUL_FIXED(x, y) ((fixed_t)(((int64_t)(x) * (y)) / F))
#define MUL_INT(x, n) ((x) * (n))
#define DIV_FIXED(x, y) ((fixed_t)(((int64_t)(x) * F) / (y)))
#define DIV_INT(x, n) ((x) / (n))


#endif /* threads/fixed-point.h */
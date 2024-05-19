

// Here's a fixed point implementation that you can use.
// Copy paste the code below to threads/fixed-point.h

// PLEASE DO NOT DISTRIBUTE THIS CODE 
// PLEASE DO NOT DISTRIBUTE THIS CODE
// PLEASE DO NOT DISTRIBUTE THIS CODE 


#ifndef THREADS_FIXED_POINT_H
#define THREADS_FIXED_POINT_H

#include <debug.h>

/* Parameters. */
#define f_BITS 32             /* Total bits per fixed-point number. */
#define f_P 16                /* Number of integer bits. */
#define f_Q 16                /* Number of fractional bits. */
#define f_F (1 << f_Q)      /* pow(2, f_Q). */

#define f_MIN_INT (-f_MAX_INT)      /* Smallest representable integer. */
#define f_MAX_INT ((1 << f_P) - 1)  /* Largest representable integer. */

/* A fixed-point number. */
typedef struct
  {
    int f;
  }
fp_t;

/* Returns a fixed-point number with F as its internal value. */
static inline fp_t
__mk_fix (int f)
{
  fp_t x;
  x.f = f;
  return x;
}

/* Returns fixed-point number corresponding to integer N. */
static inline fp_t
f_int (int n)
{
  ASSERT (n >= f_MIN_INT && n <= f_MAX_INT);
  return __mk_fix (n * f_F);
}

/* Returns fixed-point number corresponding to N divided by D. */
static inline fp_t
f_frac (int n, int d)
{
  ASSERT (d != 0);
  ASSERT (n / d >= f_MIN_INT && n / d <= f_MAX_INT);
  return __mk_fix ((long long) n * f_F / d);
}

/* Returns X rounded to the nearest integer. */
static inline int
f_round (fp_t x)
{
  return (x.f + f_F / 2) / f_F;
}

/* Returns X truncated down to the nearest integer. */
static inline int
f_trunc (fp_t x)
{
  return x.f / f_F;
}

/* Returns X + Y. */
static inline fp_t
f_add (fp_t x, fp_t y)
{
  return __mk_fix (x.f + y.f);
}

/* Returns X - Y. */
static inline fp_t
f_sub (fp_t x, fp_t y)
{
  return __mk_fix (x.f - y.f);
}

/* Returns X * Y. */
static inline fp_t
f_mul (fp_t x, fp_t y)
{
  return __mk_fix ((long long) x.f * y.f / f_F);
}

/* Returns X * N. */
static inline fp_t
f_scale (fp_t x, int n)
{
  ASSERT (n >= 0);
  return __mk_fix (x.f * n);
}

/* Returns X / Y. */
static inline fp_t
f_div (fp_t x, fp_t y)
{
  return __mk_fix ((long long) x.f * f_F / y.f);
}

/* Returns X / N. */
static inline fp_t
f_unscale (fp_t x, int n)
{
  ASSERT (n > 0);
  return __mk_fix (x.f / n);
}

/* Returns 1 / X. */
static inline fp_t
f_inv (fp_t x)
{
  return f_div (f_int (1), x);
}

/* Returns -1 if X < Y, 0 if X == Y, 1 if X > Y. */
static inline int
f_compare (fp_t x, fp_t y)
{
  return x.f < y.f ? -1 : x.f > y.f;
}

#endif /* threads/fixed-point.h */

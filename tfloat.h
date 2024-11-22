/* tfloat.h Floating point type and functions. */
/* (c) 2014,2024 Frank Wille */

#if 1 /*defined(__VBCC__) || (defined(_MSC_VER) && _MSC_VER < 1800) || defined(__CYGWIN__)*/
typedef double tfloat;
#define strtotfloat(n,e) strtod(n,e)
#define roundtfloat(f) round(f)
#define ldexptfloat(m,e) ldexp(m,e)
#else
typedef long double tfloat;
#define strtotfloat(n,e) strtold(n,e)
#define roundtfloat(f) roundl(f)
#define ldexptfloat(m,e) ldexpl(m,e)
#endif

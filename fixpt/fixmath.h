#ifndef __fixmath_h__
#define __fixmath_h__

/*-
 * Copyright (c) 2013 Gabor Pali
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#include <sys/types.h>

#define FIXEDPT_BITS	64
#define FIXEDPT_WBITS	48
#define FIXEDPT_NO_SSE

#include "fixedptc.h"

#define FIXEDPT_WMASK		((((fixedpt)1 << FIXEDPT_WBITS) - 1) << FIXEDPT_FBITS)
#define fixedpt_intpart(A)	((fixedpt)(A) & FIXEDPT_WMASK)

typedef fixedpt fixpt;

static const fixpt fixpt_two_pi   = FIXEDPT_TWO_PI;
static const fixpt fixpt_pi       = FIXEDPT_PI;
static const fixpt fixpt_half_pi  = FIXEDPT_HALF_PI;
static const fixpt fixpt_e        = FIXEDPT_E;
static const fixpt fixpt_one_half = FIXEDPT_ONE_HALF;
static const fixpt fixpt_one      = FIXEDPT_ONE;
static const fixpt fixptwo        = FIXEDPT_TWO;

extern fixpt fixpt_from_int(fixpt x);
extern fixpt fixpt_to_int(fixpt x);

#if !defined(_KERNEL)
extern fixpt fixpt_from_dlb(double x);
extern double fixpt_to_dbl(fixpt x);
#endif

extern fixpt fixpt_add(fixpt x, fixpt y);
extern fixpt fixpt_sub(fixpt x, fixpt y);
extern fixpt fixpt_mul(fixpt x, fixpt y);
extern fixpt fixpt_div(fixpt x, fixpt y);
extern fixpt fixpt_abs(fixpt x);
extern fixpt fixpt_floor(fixpt x);
extern fixpt fixpt_ceil(fixpt x);
extern fixpt fixpt_sin(fixpt x);
extern fixpt fixpt_cos(fixpt x);
extern fixpt fixpt_tan(fixpt x);
extern fixpt fixpt_asin(fixpt x);
extern fixpt fixpt_acos(fixpt x);
extern fixpt fixpt_atan2(fixpt x, fixpt y);
extern fixpt fixpt_atan(fixpt x);
extern fixpt fixpt_sinh(fixpt x);
extern fixpt fixpt_cosh(fixpt x);
extern fixpt fixpt_tanh(fixpt x);
extern fixpt fixpt_sqrt(fixpt x);
extern fixpt fixpt_pow(fixpt x, fixpt y);
extern fixpt fixpt_exp(fixpt x);
extern fixpt fixpt_log(fixpt x);
extern fixpt fixpt_log10(fixpt x);
extern fixpt fixpt_copysign(fixpt x, fixpt y);
extern fixpt fixpt_mod(fixpt x, fixpt y);
extern fixpt fixpt_hypot(fixpt x, fixpt y);
extern fixpt fixpt_ldexp(fixpt x, int exp);
extern fixpt fixpt_expm1(fixpt x);
extern fixpt fixpt_log1p(fixpt x);
extern fixpt fixpt_frexp(fixpt x, int *exp);
extern fixpt fixpt_modf(fixpt x, fixpt *p);

extern void  fixpt_to_str(fixpt x, char *buf, int decimals);

extern fixpt fixpt_from_str(const char *buf);
extern fixpt fixpt_strtod(const char *buf, const char ** endptr);

#endif

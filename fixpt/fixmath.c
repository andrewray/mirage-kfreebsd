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

#if defined(_KERNEL)
#include <sys/ctype.h>
#include <sys/limits.h>
#include <sys/_null.h>
#else
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#endif

#include "fixmath.h"

static fixpt half_pi         = fixedpt_rconst(3.14159265358979323846 / 2);
static fixpt quarter_pi      = fixedpt_rconst(3.14159265358979323846 / 4);
static fixpt thirdquarter_pi = fixedpt_rconst(3 * 3.14159265358979323846 / 4);

static fixpt foo1 = fixedpt_rconst(0.19629);
static fixpt foo2 = fixedpt_rconst(0.98169);

static fixpt fixpt_sq(fixpt x);

fixpt fixpt_sq(fixpt x)
{
	return fixpt_mul(x, x);
}

fixpt fixpt_from_int(fixpt x)
{
	return fixedpt_fromint(x);
}

fixpt fixpt_to_int(fixpt x)
{
	return fixedpt_toint(x);
}

#if !defined(_KERNEL)
fixpt fixpt_from_dbl(double x)
{
	return fixedpt_rconst(x);
}

double fixpt_to_dbl(fixpt x)
{
	return (x / (double) fixpt_one);
}
#endif

fixpt fixpt_add(fixpt x, fixpt y)
{
	return fixedpt_add(x, y);
}

fixpt fixpt_sub(fixpt x, fixpt y)
{
	return fixedpt_sub(x, y);
}

fixpt fixpt_mul(fixpt x, fixpt y)
{
	return fixedpt_mul(x, y);
}

fixpt fixpt_div(fixpt x, fixpt y)
{
	return fixedpt_div(x, y);
}

fixpt fixpt_abs(fixpt x)
{
	return fixedpt_abs(x);
}

fixpt fixpt_floor(fixpt x)
{
	return fixedpt_intpart(x);
}

fixpt fixpt_ceil(fixpt x)
{
	return (x ? fixedpt_intpart(x) + fixpt_one : fixedpt_intpart(x));
}

fixpt fixpt_sin(fixpt x)
{
	return fixedpt_sin(x);
}

fixpt fixpt_cos(fixpt x)
{
	return fixedpt_cos(x);
}

fixpt fixpt_tan(fixpt x)
{
	return fixedpt_tan(x);
}

fixpt fixpt_asin(fixpt x)
{
	fixpt result;

	if ((x > fixpt_one) || (x < -fixpt_one))
		return 0;

	result = fixpt_one - fixpt_mul(x, x);
	result = fixpt_div(x, fixpt_sqrt(result));
	result = fixpt_atan(result);
	return result;
}

fixpt fixpt_acos(fixpt x)
{
	return (half_pi - fixpt_asin(x));
}

fixpt fixpt_atan2(fixpt y, fixpt x)
{
	fixpt angle, abs_y, r, r_3;

	if (x == 0 && y == 0)
		return 0;

	abs_y = fixpt_abs(y);

	if (x >= 0) {
		r   = fixpt_div(x - abs_y, x + abs_y);
		r_3 = fixpt_mul(fixpt_mul(r, r), r);
		angle = fixpt_mul(foo1, r_3) - fixpt_mul(foo2, r) + quarter_pi;
	}
	else {
		r   = fixpt_div(x + abs_y, abs_y - x);
		r_3 = fixpt_mul(fixpt_mul(r, r), r);
		angle = fixpt_mul(foo1, r_3) - fixpt_mul(foo2, r) + thirdquarter_pi;
	}

	if (y < 0)
		angle = -angle;

	return angle;
}

fixpt fixpt_atan(fixpt x)
{
	return fixpt_atan2(x, fixpt_one);
}

fixpt fixpt_sinh(fixpt x)
{
	fixpt a = fixpt_sub(fixpt_one, fixpt_exp(fixpt_mul(-2 * fixpt_one, x)));
	fixpt b = fixpt_mul(2 * fixpt_one, fixpt_exp(-x));
	return fixpt_div(a, b);
}

fixpt fixpt_cosh(fixpt x)
{
	fixpt a = fixpt_add(fixpt_one, fixpt_exp(fixpt_mul(-2 * fixpt_one,x)));
	fixpt b = fixpt_mul(2 * fixpt_one, fixpt_exp(-x));
	return fixpt_div(a, b);
}

fixpt fixpt_tanh(fixpt x)
{
	return fixpt_div(fixpt_sinh(x), fixpt_cosh(x));
}

fixpt fixpt_sqrt(fixpt x)
{
	return fixedpt_sqrt(x);
}

fixpt fixpt_pow(fixpt x, fixpt y)
{
	if (y == 0) return fixpt_one;
	if (x == 0) return 0;
	if (y == 2 * fixpt_one) return fixpt_sq(x);
	return fixedpt_pow(x, y);
}

fixpt fixpt_exp(fixpt x)
{
	return fixedpt_exp(x);
}

fixpt fixpt_log(fixpt x)
{
	return fixedpt_ln(x);
}

fixpt fixpt_log10(fixpt x)
{
	return (fixedpt_log(x, 10 * fixpt_one));
}

fixpt fixpt_copysign(fixpt x, fixpt y)
{
	fixpt result;

	result = x;

	if ((x < 0 && y > 0) || (x > 0 && y < 0))
		result = -result;

	return result;
}

fixpt fixpt_mod(fixpt x, fixpt y)
{
	return (x % y);
}

fixpt fixpt_hypot(fixpt x, fixpt y)
{
	return fixpt_sqrt(fixpt_add(fixpt_sq(x), fixpt_sq(y)));
}

fixpt fixpt_ldexp(fixpt x, int exp)
{
	return fixpt_mul(x, fixpt_one << exp);
}

fixpt fixpt_expm1(fixpt x)
{
	return fixpt_sub(fixpt_exp(x), fixpt_one);
}

fixpt fixpt_log1p(fixpt x)
{
	return fixpt_log(fixpt_add(fixpt_one, x));
}

fixpt fixpt_frexp(fixpt x, int *exp)
{
	fixpt half;
	fixpt two;
	fixpt cache;

	*exp = 0;

	if (x == 0)
		return 0;

	half = fixpt_one >> 1;
	two  = fixpt_one << 1;

	if (x > half)
		for (; (cache = fixpt_div(x, two)) >= half; x = cache, (*exp)++ );
	else
	if (x < half)
		for (; x < half; x = fixpt_mul(x, two), (*exp)-- );

	return x;
}

fixpt fixpt_modf(fixpt x, fixpt *p)
{
	*p = fixedpt_intpart(x);
	return (fixedpt_fracpart(x));
}

void fixpt_to_str(fixpt x, char *buf, int decimals)
{
	fixedpt_str(x, buf, decimals);
}

fixpt fixpt_from_str(const char *buf)
{
	return fixpt_strtod(buf, NULL);
}

fixpt fixpt_strtod(const char *buf, const char ** endptr)
{
	int negative, count;
	fixedptu intpart, value, fracpart, scale;

	while (isspace(*buf))
		buf++;

	negative = (*buf == '-');

	if (*buf == '+' || *buf == '-')
		buf++;

	intpart = 0;
	count = 0;

	while (isdigit(*buf)) {
		intpart *= 10;
		intpart += *buf++ - '0';
		count++;
	}

	value = intpart << FIXEDPT_FBITS;

	if (*buf == '.' || *buf == ',') {
		buf++;

		fracpart = 0;
		scale = 1;

		while (isdigit(*buf) && scale < 10000000000) {
			scale *= 10;
			fracpart *= 10;
			fracpart += *buf++ - '0';
		}

		value += fixpt_div(fracpart, scale);
	}

	if (endptr)
		*endptr = buf;

	return negative ? -value : value;
}

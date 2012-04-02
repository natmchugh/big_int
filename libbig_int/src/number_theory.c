/***********************************************************************
    Copyright 2004, 2005 Alexander Valyalkin

    These sources is free software. You can redistribute it and/or
    modify it freely. You can use it with any free or commercial
    software.

    These sources is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY. Without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

    You may contact the author by:
       e-mail:  valyala@gmail.com
*************************************************************************/
#include <assert.h> /* for assert() */
#include "big_int.h"
#include "basic_funcs.h" /* for basic big_int functions */
#include "low_level_funcs.h" /* for low level add, sub, etc. funcs */
#include "bitset_funcs.h" /* for big_int_scan1_bit() */
#include "number_theory.h"

static unsigned int *get_primes_up_to(unsigned int base, unsigned int *primes_cnt);
static int primality_test(const big_int *a, unsigned int *dividers, unsigned int dividers_cnt,
                          int level, int *is_prime);

/**
    Private function.
    Returns array of all primes up to [base]^2.
    Also, [primes_cnt] is set to number of primes up to [n].
    This array must be freed by bi_free()
    On error returns NULL.

    The function is using the Sieve of Eratosthenes to find primes.
*/
static unsigned int *get_primes_up_to(unsigned int base, unsigned int *primes_cnt)
{
    unsigned int *primes;
    unsigned int i, j, n;

    /* integer overflow check */
    assert((~(unsigned int) 0) / base >= base);

    n = base * base;
    primes = (unsigned int *) bi_malloc(n * sizeof(*primes));
    if (primes == NULL) {
        return NULL;
    }

    for (i = 0; i < n; i++) {
        primes[i] = i;
    }
    primes[0] = primes[1] = 0;

    for (i = 2; i < base; i++) {
        while (!primes[i]) {
            i++;
        }
        j = i;
        for (j += i; j < n; j += i) {
            primes[j] = 0;
        }
    }

    j = 0;
    for (i = 0; i < n; i++) {
        if (primes[i]) {
            primes[j++] = primes[i];
        }
    }

    *primes_cnt = j;

    return primes;
}

/**
    Private function.
    Checks if [a] is probably prime.

    [level] is level of primality test:
        0 - test using trial division only for primes in the array [dividers].

        1 - Checks the same as level = 0, and additionally checks Miller's
            test for 7 bases: [2, 3, 5, 7, 11, 13, 17].
            It can prove 100% primality for numbers up to 341,550,071,728,321.
            See http://www.utm.edu/research/primes/prove/prove2_3.html for details.

        2 - Checks the same as level = 1, and additionally checks Miller's
            test for [dividers] as bases.

    Sets [is_prime]:
        0 - if [a] is composite
        1 - if [a] is probably prime
        2 - if [a] if 100% prime

    Note: array [dividers] must be generated by private function get_primes_up_to().
    Else result of this function is undefined!

    Returns error number:
        0 - no errors
        other - internal error
*/
static int primality_test(const big_int *a, unsigned int *dividers, unsigned int dividers_cnt,
                          int level, int *is_prime)
{
    static const unsigned int bases_cnt = 7;
    static const char *prime_limits[] = {
        "2047", "1373653", "25326001", "3215031751", "2152302898747",
        "3474749660383", "341550071728321",
    };
    static const size_t prime_limits_len[] = {4, 7, 8, 10, 13, 13, 15};
    big_int *tmp = NULL;
    big_int_str *str = NULL;
    unsigned int i;
    int cmp_flag;
    int result = 0;

    assert(a != NULL);
    assert(is_prime != NULL);
    assert(level >= 0 && level < 3);

    /* check trivial cases */
    if (a->len == 1) {
        switch (a->num[0]) {
            case 1: /* if [a] = 1, then return 0 */
                *is_prime = 0;
                goto end;
            case 2: /* 2 is prime. so, return 2 */
                *is_prime = 2;
                goto end;
        }
    }

    tmp = big_int_create(1);
    if (tmp == NULL) {
        result = 3;
        goto end;
    }
    str = big_int_str_create(100);
    if (str == NULL) {
        result = 4;
        goto end;
    }

    /* try to find small dividers of [a] using trial division */
    for (i = 0; i < dividers_cnt; i++) {
        if (big_int_from_int(dividers[i], tmp)) {
            result = 5;
            goto end;
        }
        if (big_int_mod(a, tmp, tmp)) {
            result = 6;
            goto end;
        }
        if (tmp->len == 1 && tmp->num[0] == 0) {
            /* found divider dividers[i]. So, [a] is composite */
            *is_prime = 0;
            goto end;
        }
        /* if [a]  < (dividers[i] + 1)^2, then [a] is 100% prime */
        if (big_int_from_int(dividers[i], tmp)) {
            result = 7;
            goto end;
        }
        if (big_int_inc(tmp, tmp)) {
            result = 8;
            goto end;
        }
        if (big_int_sqr(tmp, tmp)) {
            result = 9;
            goto end;
        }
        big_int_cmp_abs(a, tmp, &cmp_flag);
        if (cmp_flag < 0) {
            /* [a] is 100% prime */
            *is_prime = 2;
            goto end;
        }
    }
    if (level == 0) {
        /* [a] is probably prime */
        *is_prime = 1;
        goto end;
    }

    /*
        Miller-Rabin's probable-primality test for first
        [bases_cnt] bases from [dividers] array
    */
    for (i = 0; i < bases_cnt; i++) {
        if (big_int_from_int(dividers[i], tmp)) {
            result = 10;
            goto end;
        }
        if (big_int_miller_test(a, tmp, is_prime)) {
            result = 11;
            goto end;
        }
        if (*is_prime == 0) {
            /* [a] is composite */
            goto end;
        }
        /*
            if [a] < prime_limits[i], then [a] is 100% prime,
            else [a] is probably prime
            More info is avaible by the url:
            http://www.utm.edu/research/primes/prove/prove2_3.html
        */
        if (big_int_str_copy_s(prime_limits[i], prime_limits_len[i], str)) {
            result = 12;
            goto end;
        }
        if (big_int_from_str(str, 10, tmp)) {
            result = 13;
            goto end;
        }
        big_int_cmp_abs(a, tmp, &cmp_flag);
        if (cmp_flag < 0) {
            /* [a] is 100% prime */
            *is_prime = 2;
            goto end;
        }
    }

    if (level == 1) {
        /* [a] is probably prime */
        *is_prime = 1;
        goto end;
    }

    /*
        Miller-Rabin's probable-primality test for [dividers] as bases
    */
    for (i = bases_cnt; i < dividers_cnt; i++) {
        if (big_int_from_int(dividers[i], tmp)) {
            result = 14;
            goto end;
        }
        if (big_int_miller_test(a, tmp, is_prime)) {
            result = 15;
            goto end;
        }
        if (*is_prime == 0) {
            /* [a] is composite */
            goto end;
        }
    }

    /* [a] is still probable prime */
    *is_prime = 1;

end:
    /* free allocated memory */
    big_int_destroy(tmp);
    big_int_str_destroy(str);

    return result;
}

/**
    Founds Greatest Common Divider (GCD) of [a] and [b].
    Also this function can find two numbers [x] and [y], so
        abs(a)*x + abs(b)*y = gcd
    Note: gcd > 0

    [x] or [y] can be NULL, if we needn't to calculate them.

    Returns error number:
        0 - no errors
        1 - division by zero. [a] and [b] cannot be zero.
        other - internal error
*/
int big_int_gcd_extended(const big_int *a, const big_int *b, big_int *gcd, big_int *x, big_int *y)
{
    big_int *r1 = NULL, *r2 = NULL, *q = NULL, *tmp;
    int result = 0;
    int need_xy = 0;
    big_int *x1 = NULL, *x2 = NULL, *x3 = NULL;

    assert(a != NULL);
    assert(b != NULL);
    assert(gcd != NULL);
    assert(x != y || x == NULL);

    r1 = big_int_dup(a);
    if (r1 == NULL) {
        result = 2;
        goto end;
    }
    r1->sign = PLUS;
    r2 = big_int_dup(b);
    if (r2 == NULL) {
        result = 3;
        goto end;
    }
    r2->sign = PLUS;
    q = big_int_create(1);
    if (q == NULL) {
        result = 4;
        goto end;
    }

    if (x != NULL || y != NULL) {
        /* create temporary numbers [x1], [x2], [x3] to calculate [x] and [y] */
        need_xy = 1;
        x1 = big_int_create(1);
        if (x1 == NULL) {
            result = 5;
            goto end;
        }
        if (big_int_from_int(1, x1)) {
            result = 6;
            goto end;
        }
        x2 = big_int_create(1);
        if (x2 == NULL) {
            result = 7;
            goto end;
        }
        if (big_int_from_int(0, x2)) {
            result = 8;
            goto end;
        }
        x3 = big_int_create(1);
        if (x3 == NULL) {
            result = 9;
            goto end;
        }
    }

    do {
        result = big_int_div_extended(r1, r2, q, r1);
        if (result) {
            result = (result == 1) ? 1 : 10;
            goto end;
        }
        /* exchange [r1] and [r2] */
        tmp = r1;
        r1 = r2;
        r2 = tmp;

        if (need_xy) {
            /* calculate x3 = x1 - q * x2 */
            if (big_int_mul(x2, q, x3)) {
                result = 11;
                goto end;
            }
            if (big_int_sub(x1, x3, x3)) {
                result = 12;
                goto end;
            }
            tmp = x1;
            x1 = x2;
            x2 = x3;
            x3 = tmp;
        }
    } while (r2->len > 1 || r2->num[0] != 0);

    if (y != NULL) {
        /* calculate y = (r1 - a * x1) / b */
        if (big_int_mul(x1, a, x3)) {
            result = 13;
            goto end;
        }
        if (big_int_sub(r1, x3, x3)) {
            result = 14;
            goto end;
        }
        if (big_int_div(x3, b, y)) {
            result = 15;
            goto end;
        }
    }
    if (x != NULL) {
        if (big_int_copy(x1, x)) {
            result = 16;
            goto end;
        }
    }
    if (big_int_copy(r1, gcd)) {
        result = 17;
        goto end;
    }

end:
    /* free allocated memory */
    big_int_destroy(x3);
    big_int_destroy(x2);
    big_int_destroy(x1);
    big_int_destroy(q);
    big_int_destroy(r2);
    big_int_destroy(r1);

    return result;
}

/**
    Founds greatest common divider of [a] and [b]:
        answer = GCD(a, b)

    Returns error number:
        0 - no errors
        other - internal error
*/
int big_int_gcd(const big_int *a, const big_int *b, big_int *answer)
{
    return big_int_gcd_extended(a, b, answer, NULL, NULL);
}

/**
    Founds square root of [a]:
        answer = floor(sqrt(a))

    Returns error number:
        0 - no errors
        1 - cannot calculate square root form negative number [a]
        other - internal error
*/
int big_int_sqrt(const big_int *a, big_int *answer)
{
    big_int *y = NULL, *y_new = NULL, *tmp = NULL;
    int result = 0;
    int cmp_flag;

    assert(a != NULL);
    assert(answer != NULL);

    if (a->sign == MINUS) {
        /* cannot calculate square root from negative number */
        result = 1;
        goto end;
    }

    y = big_int_dup(a);
    if (y == NULL) {
        result = 2;
        goto end;
    }
    y_new = big_int_dup(a);
    if (y_new == NULL) {
        result = 4;
        goto end;
    }

    /* calculate y_new = [( a + 1 ) / 2] */
    if (big_int_inc(y_new, y_new)) {
        result = 5;
        goto end;
    }
    if (big_int_rshift(y_new, 1, y_new)) {
        result = 6;
        goto end;
    }

    while (1) {
        big_int_cmp_abs(y, y_new, &cmp_flag);
        if (cmp_flag <= 0) {
            /* square root of [a] found. It is in the [y] */
            break;
        }
        /* calculates y[i+1] = [(y[i]*y[i] + a) / (2*y[i])] */
        if (big_int_sqr(y_new, y)) {
            result = 7;
            goto end;
        }
        if (big_int_add(y, a, y)) {
            result = 8;
            goto end;
        }
        if (big_int_lshift(y_new, 1, y_new)) {
            result = 9;
            goto end;
        }
        if (big_int_div(y, y_new, y)) {
            result = 10;
            goto end;
        }
        if (big_int_rshift(y_new, 1, y_new)) {
            result = 11;
            goto end;
        }
        /* exchange [y] and [y_new] */
        tmp = y;
        y = y_new;
        y_new = tmp;
    }

    if (big_int_copy(y, answer)) {
        result = 4;
        goto end;
    }

end:
    /* free allocated memory */
    big_int_destroy(y_new);
    big_int_destroy(y);

    return result;
}

/**
    Calculates remainder of squre root:
        answer = a - floor(sqrt(a))

    Returns error number:
        0 - no errors
        1 - cannot calculate square root from negative [a]
        other - internal error
*/
int big_int_sqrt_rem(const big_int *a, big_int *answer)
{
    big_int *answer_copy = NULL;
    int result = 0;

    assert(a != NULL);
    assert(answer != NULL);

    if (a->sign == MINUS) {
        result = 1;
        goto end;
    }

    if (a == answer) {
        answer_copy = big_int_create(1);
        if (answer_copy == NULL) {
            result = 3;
            goto end;
        }
    } else {
        answer_copy = answer;
    }

    if (big_int_sqrt(a, answer_copy)) {
        result = 4;
        goto end;
    }

    if (big_int_sqr(answer_copy, answer_copy)) {
        result = 5;
        goto end;
    }

    if (big_int_sub(a, answer_copy, answer_copy)) {
        result = 6;
        goto end;
    }

    if (big_int_copy(answer_copy, answer)) {
        result = 7;
        goto end;
    }

end:
    /* free allocated memory */
    if (answer_copy != answer) {
        big_int_destroy(answer_copy);
    }

    return result;
}

/**
    Checks the Miller primality test for number [a] with base [base].
    If [a] is composite, then [is_prime] sets to 0.
    If [a] is a "strong PROBABLY prime" (NOT 100% prime, only 75%
    by Rabin's theoreme), then [is_prime] sets to 1.

    Returns error number:
        0 - no errors
        1 - [base] is too small. It must be 1 < base < (a - 1)
        2 - [base] is too high. It must be 1 < base < (a - 1)
        other - internal error
*/
int big_int_miller_test(const big_int *a, const big_int *base, int *is_prime)
{
    big_int *r = NULL, *r1 = NULL;
    big_int *tmp; /* specially not assigned to NULL */
    size_t zero_bits;
    big_int_word one = 1;
    int cmp_flag;
    int result = 0;

    assert(a != NULL);
    assert(base != NULL);
    assert(is_prime != NULL);

    if (a->len == 1 && a->num[0] < 4) {
        *is_prime = (a->num[0] > 1) ? 1 : 0;
        goto end;
    }

    /* calculate [r] = abs(a) - 1 */
    r = big_int_dup(a);
    if (r == NULL) {
        result = 3;
        goto end;
    }
    r->sign = PLUS;
    r1 = big_int_create(a->len);
    if (r1 == NULL) {
        result = 4;
        goto end;
    }

    low_level_sub(r->num, r->num + r->len, &one, (&one) + 1, r->num);
    /* check [base] correctness */
    if ((base->sign == MINUS || base->len == 1) && (base->num[0] < 2)) {
        /* base must be greater than 1 */
        result = 1;
        goto end;
    }
    big_int_cmp_abs(r, base, &cmp_flag);
    if (cmp_flag != 1) {
        /* base must be less than a - 1 */
        result = 2;
        goto end;
    }

    /* count lower zero bits in the [r] */
    if (big_int_scan1_bit(r, 0, &zero_bits)) {
        /* [r] = 0, so, a = r + 1 = 1 is composite */
        *is_prime = 0;
        goto end;
    }

    /* shift [r] by [zero_bits] to the right */
    if (big_int_rshift(r, (int) zero_bits, r)) {
        result = 5;
        goto end;
    }

    /* calculate r = pow(base, r) (mod a) */
    if (big_int_powmod(base, r, a, r)) {
        result = 6;
        goto end;
    }

    /* if r == 1, then [a] is probably prime */
    if (r->len == 1 && r->num[0] == 1) {
        *is_prime = 1;
        goto end;
    }

    /*
        main loop:
            1. if zero_bits == 0, then [a] is composite
            2. zero_bits--
            3. if r == -1 (mod a), then [a] is probably prime
            5. if zero_bits == 0, then [a] is composite
            6. calculate r = r * r (mod a)
            7. goto 1
    */
    while (zero_bits--) {
        /* compare r to a - 1 */
        if (big_int_inc(r, r)) {
            result = 7;
            goto end;
        }
        big_int_cmp_abs(r, a, &cmp_flag);
        if (cmp_flag == 0) {
            /* [a] is probably prime */
            *is_prime = 1;
            goto end;
        }
        if (big_int_dec(r, r)) {
            result = 8;
            goto end;
        }

        /*
            At last loop we needn't calculate r = r * r (mod a),
            because [a] is already composite.
            This is performance optimization :)
        */
        if (zero_bits == 0) {
            break;
        }

        /* calculate r1 = r * r (mod a) */
        if (big_int_sqrmod(r, a, r1)) {
            result = 9;
            goto end;
        }
        /* exchange [r] <=> [r1] */
        tmp = r;
        r = r1;
        r1 = tmp;
    }
    /* [a] is composite */
    *is_prime = 0;

end:
    /* free allocated memory */
    big_int_destroy(r1);
    big_int_destroy(r);

    return result;
}

/**
    Checks if [a] is probably prime.

    [level] is level of primality test:
        0 - test using trial division only for primes up to [primes_to]^2.
            It can prove 100% primality for numbers up to [primes_to]^4.

        1 - Checks the same as level = 0, and additionally checks Miller's
            test for 7 bases: [2, 3, 5, 7, 11, 13, 17].
            It can prove 100% primality for numbers up to 341,550,071,728,321.
            See http://www.utm.edu/research/primes/prove/prove2_3.html for details.

        2 - Checks the same as level = 1, and additionally checks Miller's
            test for primes up to [primes_to]^2 as bases.

    Sets [is_prime]:
        0 - if [a] is composite
        1 - if [a] is probably prime
        2 - if [a] if 100% prime

    Note: if [primes_to] < 5, then [primes_to] will be set to 5.

    Returns error number:
        0 - no errors
        other - internal error
*/
int big_int_is_prime(const big_int *a, unsigned int primes_to, int level, int *is_prime)
{
    unsigned int *primes = NULL;
    unsigned int primes_cnt;

    assert(a != NULL);
    assert(is_prime != NULL);
    assert(level >= 0 && level < 3);

    /* generate small primes for trial division test */
    primes_to = (primes_to < 5) ? 5 : primes_to;
    primes = get_primes_up_to(primes_to, &primes_cnt);
    if (primes == NULL) {
        return 3;
    }

    if (primality_test(a, primes, primes_cnt, level, is_prime)) {
        bi_free(primes);
        return 4;
    }
    bi_free(primes);

    return 0;
}

/**
    Finds minimal pseudoprime, greater than number [a].

    Warning: [answer] is not 100% prime. It is probably-prime!

    Returns error number:
        0 - no errors
        other - internal error
*/
int big_int_next_prime(const big_int *a, big_int *answer)
{
    int is_prime = 0;
    unsigned int *primes = NULL;
    unsigned int primes_cnt;
    int result = 0;

    assert(a != NULL);
    assert(answer != NULL);

    /* check trivial case [a] = 2 or [a] = -2 */
    if (a->len == 1 && a->num[0] == 2) {
        if (a->sign == PLUS) {
            if (big_int_from_int(3, answer)) {
                result = 1;
                goto end;
            }
        } else {
            if (big_int_from_int(2, answer)) {
                result = 2;
                goto end;
            }
        }
        goto end;
    }

    /* precompute array or primes */
    primes = get_primes_up_to(100, &primes_cnt);
    if (primes == NULL) {
        result = 3;
        goto end;
    }

    if (big_int_copy(a, answer)) {
        result = 4;
        goto end;
    }

    if (!(answer->num[0] & 1)) {
        if (big_int_dec(answer, answer)) {
            result = 5;
            goto end;
        }
    }

    do {
        if (big_int_inc(answer, answer)) {
            result = 6;
            goto end;
        }
        /* check if [answer] == 2, then it is prime :) */
        if (answer->len == 1 && answer->num[0] == 2) {
            goto end;
        }
        if (big_int_inc(answer, answer)) {
            result = 7;
            goto end;
        }
        if (primality_test(answer, primes, primes_cnt, 1, &is_prime)) {
            result = 8;
            goto end;
        }
    } while (is_prime == 0);

end:
    /* free allocated memory */
    bi_free(primes);

    return result;
}

/**
    Powers [a] by [power]:
        answer = pow(a, power);

    Returns error number:
        0 - no errors
        1 - cannot allocate memory for result
        other - internal errors
*/
int big_int_pow(const big_int *a, int power, big_int *answer)
{
    size_t answer_len;
    size_t n_bits;
    big_int *answer_copy = NULL;
    int result = 0;

    assert(a != NULL);
    assert(answer != NULL);

    if (power < 0) {
        /* if [power] is negative, then pow(a, power) = 0 */
        if (big_int_from_int(0, answer)) {
            result = 2;
            goto end;
        }
        goto end;
    }

    /* check simple cases ([a] = 0 or [a] = 1) */
    if (a->len == 1 && (a->num[0] == 0 || a->num[0] == 1)) {
        if (big_int_copy(a, answer)) {
            result = 3;
            goto end;
        }
        goto end;
    }

    /*
        if [a] points to the same addres as [answer],
        then do copy of [answer]
    */
    if (a == answer) {
        answer_copy = big_int_create(1);
        if (answer_copy == NULL) {
            result = 4;
            goto end;
        }
    } else {
        answer_copy = answer;
    }

    /* allocate memory for result */
    if ((~(size_t)0) / (a->len * BIG_INT_WORD_BYTES_CNT) < (size_t) power) {
        /* cannot allocate memory for result */
        result = 1;
        goto end;
    }
    answer_len = a->len * power;
    if (big_int_realloc(answer_copy, answer_len)) {
        result = 1;
        goto end;
    }

    /*
        calculate answer = pow(a, power) by the following algorithm:
            answer = 1;
            while (power) {
                answer *= answer;
                if higher bit of power is 1, then
                    answer *= a;
                power <<= 1;
            }
    */
    if (big_int_from_int(1, answer_copy)) {
        result = 5;
        goto end;
    }
    n_bits = 8 * sizeof(power);
    while (n_bits && !(power >> (8 * sizeof(power) - 1))) {
        power <<= 1;
        n_bits--;
    }
    while (n_bits--) {
        if (big_int_sqr(answer_copy, answer_copy)) {
            result = 6;
            goto end;
        }
        if ((power >> (8 * sizeof(power) - 1))) {
            if (big_int_mul(answer_copy, a, answer_copy)) {
                result = 7;
                goto end;
            }
        }
        power <<= 1;
    }

    if (big_int_copy(answer_copy, answer)) {
        result = 8;
        goto end;
    }

end:
    /* free allocated memory */
    if (answer != answer_copy) {
        big_int_destroy(answer_copy);
    }

    return result;
}

/**
    Calculates factorial of [n]:
        answer = n!

    Returns error number:
        0 - no errors
        1 - [n] cannot be negative
        ohter - internal error
*/
int big_int_fact(int n, big_int *answer)
{
    big_int *a = NULL;
    int result = 0;

    assert(answer != NULL);

    if (n < 0) {
        /* [n] cannot be negative */
        result = 1;
        goto end;
    }

    a = big_int_create(1);
    if (a == NULL) {
        result = 2;
        goto end;
    }
    if (big_int_from_int(n, a)) {
        result = 3;
        goto end;
    }

    if (big_int_from_int(1, answer)) {
        result = 4;
        goto end;
    }
    while (a->len > 1 || a->num[0] > 1) {
        if (big_int_mul(answer, a, answer)) {
            result = 5;
            goto end;
        }
        if (big_int_dec(a, a)) {
            result = 6;
            goto end;
        }
    }

end:
    /* free allocated memory */
    big_int_destroy(a);

    return result;
}

/**
    Calculates Jacobi symbol according to algorithm on page
    http://primes.utm.edu/glossary/page.php?sort=JacobiSymbol

    jacobi = (a|b), if GCD(a, b) == 1
    jacobi = 0, if GCD(a, b) != 1

    Note: Legendre symbol is equal to Jacoby symbol, if [b] is prime.
    Info about Legendre symbol is avaible by the url:
    http://primes.utm.edu/glossary/page.php/LegendreSymbol.html

    Returns error number:
        0 - no errors
        1 - [b] must be odd (not even)
        other - internal errors
*/
int big_int_jacobi(const big_int *a, const big_int *b, int *jacobi)
{
    big_int *a_copy = NULL, *b_copy = NULL;
    big_int *tmp;
    size_t zeros_cnt;
    int j;
    int result = 0;

    assert(a != NULL);
    assert(b != NULL);
    assert(jacobi != NULL);

    if (!(b->num[0] & 1)) {
        /* [b] cannot be odd */
        result = 1;
        goto end;
    }

    a_copy = big_int_dup(a);
    if (a_copy == NULL) {
        result = 3;
        goto end;
    }
    b_copy = big_int_dup(b);
    if (b_copy == NULL) {
        result = 4;
        goto end;
    }

    /* normalize [a_copy] - calculate a_copy (mod b_copy) */
    if (big_int_absmod(a_copy, b_copy, a_copy)) {
        result = 5;
        goto end;
    }

    /*
        Main loop:

        Jacobi(a,b) {
          j := 1
          while (a not 0) do {
            while (a even) do {
              a := a/2
              if (b = 3 (mod 8) or b = 5 (mod 8)) then j := -j
            }
            interchange(a,b)
            if (a = 3 (mod 4) and b = 3 (mod 4)) then j := -j
            a := a mod b
          }
          if (b = 1) then return (j) else return(0)
        }
    */
    j = 1;
    while (a_copy->len > 1 || a_copy->num[0] > 0) {
        if (big_int_scan1_bit(a_copy, 0, &zeros_cnt)) {
            result = 6;
            goto end;
        }
        if (big_int_rshift(a_copy, (int) zeros_cnt, a_copy)) {
            result = 7;
            goto end;
        }
        if ((zeros_cnt & 1) && ((b_copy->num[0] & 7) == 3 || (b_copy->num[0] & 7) == 5)) {
            j = -j;
        }
        tmp = a_copy;
        a_copy = b_copy;
        b_copy = tmp;
        if ((a_copy->num[0] & 3) == 3 && (b_copy->num[0] & 3) == 3) {
            j = -j;
        }
        if (big_int_absmod(a_copy, b_copy, a_copy)) {
            result = 8;
            goto end;
        }
    }
    if (b_copy->len > 1 || b_copy->num[0] != 1) {
        j = 0;
    }
    *jacobi = j;

end:
    /* free allocated memory */
    big_int_destroy(b_copy);
    big_int_destroy(a_copy);

    return result;
}

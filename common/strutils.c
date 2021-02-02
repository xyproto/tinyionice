#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#include "bitops.h"
#include "c.h"
#include "nls.h"
#include "pathnames.h"
#include "strutils.h"

static int STRTOXX_EXIT_CODE = EXIT_FAILURE;

static int do_scale_by_power(uintmax_t* x, int base, int power)
{
    while (power--) {
        if (UINTMAX_MAX / base < *x)
            return -ERANGE;
        *x *= base;
    }
    return 0;
}

#ifndef HAVE_MEMPCPY
void* mempcpy(void* restrict dest, const void* restrict src, size_t n)
{
    return ((char*)memcpy(dest, src, n)) + n;
}
#endif

#ifndef HAVE_STRNLEN
size_t strnlen(const char* s, size_t maxlen)
{
    size_t i;

    for (i = 0; i < maxlen; i++) {
        if (s[i] == '\0')
            return i;
    }
    return maxlen;
}
#endif

#ifndef HAVE_STRNDUP
char* strndup(const char* s, size_t n)
{
    size_t len = strnlen(s, n);
    char* new = malloc((len + 1) * sizeof(char));
    if (!new)
        return NULL;
    new[len] = '\0';
    return (char*)memcpy(new, s, len);
}
#endif

static uint32_t _strtou32_or_err(const char* str, const char* errmesg, int base);

int16_t strtos16_or_err(const char* str, const char* errmesg)
{
    int32_t num = strtos32_or_err(str, errmesg);

    if (num < INT16_MIN || num > INT16_MAX) {
        errno = ERANGE;
        err(STRTOXX_EXIT_CODE, "%s: '%s'", errmesg, str);
    }
    return num;
}

int32_t strtos32_or_err(const char* str, const char* errmesg)
{
    int64_t num = strtos64_or_err(str, errmesg);

    if (num < INT32_MIN || num > INT32_MAX) {
        errno = ERANGE;
        err(STRTOXX_EXIT_CODE, "%s: '%s'", errmesg, str);
    }
    return num;
}

int64_t strtos64_or_err(const char* str, const char* errmesg)
{
    int64_t num;
    char* end = NULL;

    errno = 0;
    if (str == NULL || *str == '\0')
        goto err;
    num = strtoimax(str, &end, 10);

    if (errno || str == end || (end && *end))
        goto err;

    return num;
err:
    if (errno == ERANGE)
        err(STRTOXX_EXIT_CODE, "%s: '%s'", errmesg, str);

    errx(STRTOXX_EXIT_CODE, "%s: '%s'", errmesg, str);
}

/*
 * returns exponent (2^x=n) in range KiB..EiB (2^10..2^60)
 */
static int get_exp(uint64_t n)
{
    int shft;

    for (shft = 10; shft <= 60; shft += 10) {
        if (n < (1ULL << shft))
            break;
    }
    return shft - 10;
}

char* size_to_human_string(int options, uint64_t bytes)
{
    char buf[32];
    int dec, exp;
    uint64_t frac;
    const char* letters = "BKMGTPE";
    char suffix[sizeof(" KiB")], *psuf = suffix;
    char c;

    if (options & SIZE_SUFFIX_SPACE)
        *psuf++ = ' ';

    exp = get_exp(bytes);
    c = *(letters + (exp ? exp / 10 : 0));
    dec = exp ? bytes / (1ULL << exp) : bytes;
    frac = exp ? bytes % (1ULL << exp) : 0;

    *psuf++ = c;

    if ((options & SIZE_SUFFIX_3LETTER) && (c != 'B')) {
        *psuf++ = 'i';
        *psuf++ = 'B';
    }

    *psuf = '\0';

    /* fprintf(stderr, "exp: %d, unit: %c, dec: %d, frac: %jd\n",
     *                 exp, suffix[0], dec, frac);
     */

    /* round */
    if (frac) {
        /* get 3 digits after decimal point */
        if (frac >= UINT64_MAX / 1000)
            frac = ((frac / 1024) * 1000) / (1ULL << (exp - 10));
        else
            frac = (frac * 1000) / (1ULL << (exp));

        if (options & SIZE_DECIMAL_2DIGITS) {
            /* round 4/5 and keep 2 digits after decimal point */
            frac = (frac + 5) / 10;
        } else {
            /* round 4/5 and keep 1 digit after decimal point */
            frac = ((frac + 50) / 100) * 10;
        }

        /* rounding could have overflowed */
        if (frac == 100) {
            dec++;
            frac = 0;
        }
    }

    if (frac) {
        struct lconv const* l = localeconv();
        char* dp = l ? l->decimal_point : NULL;
        int len;

        if (!dp || !*dp)
            dp = ".";

        len = snprintf(buf, sizeof(buf), "%d%s%02" PRIu64, dec, dp, frac);
        if (len > 0 && (size_t)len < sizeof(buf)) {
            /* remove potential extraneous zero */
            if (buf[len - 1] == '0')
                buf[len--] = '\0';
            /* append suffix */
            xstrncpy(buf + len, suffix, sizeof(buf) - len);
        } else
            *buf = '\0'; /* snprintf error */
    } else
        snprintf(buf, sizeof(buf), "%d%s", dec, suffix);

    return strdup(buf);
}

static size_t strcspn_escaped(const char* s, const char* reject)
{
    int escaped = 0;
    int n;

    for (n = 0; s[n]; n++) {
        if (escaped)
            escaped = 0;
        else if (s[n] == '\\')
            escaped = 1;
        else if (strchr(reject, s[n]))
            break;
    }

    /* if s ends in \, return index of previous char */
    return n - escaped;
}

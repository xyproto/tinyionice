#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <limits.h>
#include <stdarg.h>
#include <stddef.h>
#include <inttypes.h>

#define HAVE_PROGRAM_INVOCATION_SHORT_NAME "ion"

/*
 * Fundamental C definitions.
 */

#ifdef HAVE_ERR_H
#include <err.h>
#endif

#ifdef HAVE_SYS_SYSMACROS_H
#include <sys/sysmacros.h> /* for major, minor */
#endif

#ifndef LOGIN_NAME_MAX
#define LOGIN_NAME_MAX 256
#endif

#ifndef NAME_MAX
#define NAME_MAX PATH_MAX
#endif

/*
 * __GNUC_PREREQ is deprecated in favour of __has_attribute() and
 * __has_feature(). The __has macros are supported by clang and gcc>=5.
 */
#ifndef __GNUC_PREREQ
#if defined __GNUC__ && defined __GNUC_MINOR__
#define __GNUC_PREREQ(maj, min) ((__GNUC__ << 16) + __GNUC_MINOR__ >= ((maj) << 16) + (min))
#else
#define __GNUC_PREREQ(maj, min) 0
#endif
#endif

#ifdef __GNUC__

/* &a[0] degrades to a pointer: a different type from an array */
#define __must_be_array(a) \
    UL_BUILD_BUG_ON_ZERO(__builtin_types_compatible_p(__typeof__(a), __typeof__(&a[0])))

#define ignore_result(x)                                         \
    __extension__({                                              \
        __typeof__(x) __dummy __attribute__((__unused__)) = (x); \
        (void)__dummy;                                           \
    })

#else /* !__GNUC__ */
#define __must_be_array(a) 0
#define __attribute__(_arg_)
#define ignore_result(x) ((void)(x))
#endif /* !__GNUC__ */

/*
 * It evaluates to 1 if the attribute/feature is supported by the current
 * compilation target. Fallback for old compilers.
 */
#ifndef __has_attribute
#define __has_attribute(x) 0
#endif

#ifndef __has_feature
#define __has_feature(x) 0
#endif

/*
 * Function attributes
 */
#ifndef __ul_alloc_size
#if (__has_attribute(alloc_size) && __has_attribute(warn_unused_result)) || __GNUC_PREREQ(4, 3)
#define __ul_alloc_size(s) __attribute__((alloc_size(s), warn_unused_result))
#else
#define __ul_alloc_size(s)
#endif
#endif

#ifndef __ul_calloc_size
#if (__has_attribute(alloc_size) && __has_attribute(warn_unused_result)) || __GNUC_PREREQ(4, 3)
#define __ul_calloc_size(n, s) __attribute__((alloc_size(n, s), warn_unused_result))
#else
#define __ul_calloc_size(n, s)
#endif
#endif

#if __has_attribute(returns_nonnull) || __GNUC_PREREQ(4, 9)
#define __ul_returns_nonnull __attribute__((returns_nonnull))
#else
#define __ul_returns_nonnull
#endif

/*
 * Force a compilation error if condition is true, but also produce a
 * result (of value 0 and type size_t), so the expression can be used
 * e.g. in a structure initializer (or wherever else comma expressions
 * aren't permitted).
 */
#define UL_BUILD_BUG_ON_ZERO(e) __extension__(sizeof(struct { int : -!!(e); }))
#define BUILD_BUG_ON_NULL(e) ((void*)sizeof(struct { int : -!!(e); }))

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]) + __must_be_array(arr))
#endif

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef TRUE
#define TRUE 1
#endif

#ifndef FALSE
#define FALSE 0
#endif

#ifndef min
#define min(x, y)                      \
    __extension__({                    \
        __typeof__(x) _min1 = (x);     \
        __typeof__(y) _min2 = (y);     \
        (void)(&_min1 == &_min2);      \
        _min1 < _min2 ? _min1 : _min2; \
    })
#endif

#ifndef max
#define max(x, y)                      \
    __extension__({                    \
        __typeof__(x) _max1 = (x);     \
        __typeof__(y) _max2 = (y);     \
        (void)(&_max1 == &_max2);      \
        _max1 > _max2 ? _max1 : _max2; \
    })
#endif

#ifndef cmp_numbers
#define cmp_numbers(x, y)            \
    __extension__({                  \
        __typeof__(x) _a = (x);      \
        __typeof__(y) _b = (y);      \
        (void)(&_a == &_b);          \
        _a == _b ? 0 : _a > _b ? 1   \
                               : -1; \
    })
#endif

#ifndef offsetof
#define offsetof(TYPE, MEMBER) ((size_t) & ((TYPE*)0)->MEMBER)
#endif

/*
 * container_of - cast a member of a structure out to the containing structure
 * @ptr:	the pointer to the member.
 * @type:	the type of the container struct this is embedded in.
 * @member:	the name of the member within the struct.
 */
#ifndef container_of
#define container_of(ptr, type, member)                       \
    __extension__({                                           \
        const __typeof__(((type*)0)->member)* __mptr = (ptr); \
        (type*)((char*)__mptr - offsetof(type, member));      \
    })
#endif

#ifndef HAVE_ERR_H
static inline void errmsg(char doexit, int excode, char adderr, const char* fmt, ...)
{
    fprintf(stderr, "%s: ", program_invocation_short_name);
    if (fmt != NULL) {
        va_list argp;
        va_start(argp, fmt);
        vfprintf(stderr, fmt, argp);
        va_end(argp);
        if (adderr)
            fprintf(stderr, ": ");
    }
    if (adderr)
        fprintf(stderr, "%m");
    fprintf(stderr, "\n");
    if (doexit)
        exit(excode);
}

#ifndef HAVE_ERR
#define err(E, FMT...) errmsg(1, E, 1, FMT)
#endif

#ifndef HAVE_ERRX
#define errx(E, FMT...) errmsg(1, E, 0, FMT)
#endif

#ifndef HAVE_WARN
#define warn(FMT...) errmsg(0, 0, 1, FMT)
#endif

#ifndef HAVE_WARNX
#define warnx(FMT...) errmsg(0, 0, 0, FMT)
#endif
#endif /* !HAVE_ERR_H */

/* Don't use inline function to avoid '#include "nls.h"' in c.h
 */
#define errtryhelp(eval)                                                                          \
    __extension__({                                                                               \
        fprintf(                                                                                  \
            stderr, _("Try '%s --help' for more information.\n"), program_invocation_short_name); \
        exit(eval);                                                                               \
    })

/* After failed execvp() */
#define EX_EXEC_FAILED 126 /* Program located, but not usable. */
#define EX_EXEC_ENOENT 127 /* Could not find program to exec.  */
#define errexec(name) \
    err(errno == ENOENT ? EX_EXEC_ENOENT : EX_EXEC_FAILED, _("failed to execute %s"), name)

static inline __attribute__((const)) int is_power_of_2(unsigned long num)
{
    return (num != 0 && ((num & (num - 1)) == 0));
}

#ifndef HAVE_LOFF_T
typedef int64_t loff_t;
#endif

#if !defined(HAVE_DIRFD) && (!defined(HAVE_DECL_DIRFD) || HAVE_DECL_DIRFD == 0) \
    && defined(HAVE_DIR_DD_FD)
#include <dirent.h>
#include <sys/types.h>
static inline int dirfd(DIR* d)
{
    return d->dd_fd;
}
#endif

/*
 * Fallback defines for old versions of glibc
 */
#include <fcntl.h>

#ifdef O_CLOEXEC
#define UL_CLOEXECSTR "e"
#else
#define UL_CLOEXECSTR ""
#endif

#ifndef O_CLOEXEC
#define O_CLOEXEC 0
#endif

#ifdef __FreeBSD_kernel__
#ifndef F_DUPFD_CLOEXEC
#define F_DUPFD_CLOEXEC 17 /* Like F_DUPFD, but FD_CLOEXEC is set */
#endif
#endif

#ifndef AI_ADDRCONFIG
#define AI_ADDRCONFIG 0x0020
#endif

#ifndef IUTF8
#define IUTF8 0040000
#endif

/*
 * MAXHOSTNAMELEN replacement
 */
static inline size_t get_hostname_max(void)
{
    long len = sysconf(_SC_HOST_NAME_MAX);

    if (0 < len)
        return len;

#ifdef MAXHOSTNAMELEN
    return MAXHOSTNAMELEN;
#elif HOST_NAME_MAX
    return HOST_NAME_MAX;
#endif
    return 64;
}

#define HAVE_NANOSLEEP

/*
 * The usleep function was marked obsolete in POSIX.1-2001 and was removed
 * in POSIX.1-2008.  It was replaced with nanosleep() that provides more
 * advantages (like no interaction with signals and other timer functions).
 */
#include <time.h>

static inline int xusleep(useconds_t usec)
{
#ifdef HAVE_NANOSLEEP
    struct timespec waittime = { .tv_sec = usec / 1000000L, .tv_nsec = (usec % 1000000L) * 1000 };
    return nanosleep(&waittime, NULL);
#elif defined(HAVE_USLEEP)
    return usleep(usec);
#else
#error "System with usleep() or nanosleep() required!"
#endif
}

/*
 * Constant strings for usage() functions. For more info see
 * Documentation/{howto-usage-function.txt,boilerplate.c}
 */
#define USAGE_HEADER _("\nUsage:\n")
#define USAGE_OPTIONS _("\nOptions:\n")
#define USAGE_FUNCTIONS _("\nFunctions:\n")
#define USAGE_COMMANDS _("\nCommands:\n")
#define USAGE_ARGUMENTS _("\nArguments:\n")
#define USAGE_COLUMNS _("\nAvailable output columns:\n")
#define USAGE_SEPARATOR "\n"

#define USAGE_OPTSTR_HELP _("display this help")
#define USAGE_OPTSTR_VERSION _("display version")

#define USAGE_HELP_OPTIONS(marg_dsc) \
    "%-" #marg_dsc "s%s\n"           \
    "%-" #marg_dsc "s%s\n",          \
        " -h, --help", USAGE_OPTSTR_HELP, " -V, --version", USAGE_OPTSTR_VERSION

#define USAGE_ARG_SEPARATOR "\n"
#define USAGE_ARG_SIZE(_name)                                            \
    _(" %s arguments may be followed by the suffixes for\n"              \
      "   GiB, TiB, PiB, EiB, ZiB, and YiB (the \"iB\" is optional)\n"), \
        _name

#define ION_VERSION _("%s 1.0.0\n"), program_invocation_short_name

#define print_version(eval)  \
    __extension__({          \
        printf(ION_VERSION); \
        exit(eval);          \
    })

/*
 * seek stuff
 */
#ifndef SEEK_DATA
#define SEEK_DATA 3
#endif
#ifndef SEEK_HOLE
#define SEEK_HOLE 4
#endif

/*
 * Macros to convert #define'itions to strings, for example
 * #define XYXXY 42
 * printf ("%s=%s\n", stringify(XYXXY), stringify_value(XYXXY));
 */
#define stringify_value(s) stringify(s)
#define stringify(s) #s

/*
 * UL_ASAN_BLACKLIST is a macro to tell AddressSanitizer (a compile-time
 * instrumentation shipped with Clang and GCC) to not instrument the
 * annotated function.  Furthermore, it will prevent the compiler from
 * inlining the function because inlining currently breaks the blacklisting
 * mechanism of AddressSanitizer.
 */
#if __has_feature(address_sanitizer) && __has_attribute(no_sanitize_memory) \
    && __has_attribute(no_sanitize_address)
#define UL_ASAN_BLACKLIST                                         \
    __attribute__((noinline)) __attribute__((no_sanitize_memory)) \
        __attribute__((no_sanitize_address))
#else
#define UL_ASAN_BLACKLIST /* nothing */
#endif

/*
 * Note that sysconf(_SC_GETPW_R_SIZE_MAX) returns *initial* suggested size for
 * pwd buffer and in some cases it is not large enough. See POSIX and
 * getpwnam_r man page for more details.
 */
#define UL_GETPW_BUFSIZ (16 * 1024)

/*
 * Darwin or other BSDs may only have MAP_ANON. To get it on Darwin we must
 * define _DARWIN_C_SOURCE before including sys/mman.h. We do this in config.h.
 */
#if !defined MAP_ANONYMOUS && defined MAP_ANON
#define MAP_ANONYMOUS (MAP_ANON)
#endif

#ifndef LOCALEDIR
#define LOCALEDIR "/usr/share/locale"
#endif

#ifdef HAVE_LOCALE_H
#include <locale.h>
#else
#undef setlocale
#define setlocale(Category, Locale) /* empty */
struct lconv {
    char* decimal_point;
};
#undef localeconv
#define localeconv() NULL
#endif

#ifdef ENABLE_NLS
#include <libintl.h>
/*
 * For NLS support in the public shared libraries we have to specify text
 * domain name to be independent on the main program. For this purpose define
 * UL_TEXTDOMAIN_EXPLICIT before you include nls.h to your shared library code.
 */
#ifdef UL_TEXTDOMAIN_EXPLICIT
#define _(Text) dgettext(UL_TEXTDOMAIN_EXPLICIT, Text)
#else
#define _(Text) gettext(Text)
#endif
#ifdef gettext_noop
#define N_(String) gettext_noop(String)
#else
#define N_(String) (String)
#endif
#define P_(Singular, Plural, n) ngettext(Singular, Plural, n)
#else
#undef bindtextdomain
#define bindtextdomain(Domain, Directory) /* empty */
#undef textdomain
#define textdomain(Domain) /* empty */
#define _(Text) (Text)
#define N_(Text) (Text)
#define P_(Singular, Plural, n) ((n) == 1 ? (Singular) : (Plural))
#endif /* ENABLE_NLS */

#ifdef HAVE_LANGINFO_H
#include <langinfo.h>
#else

typedef int nl_item;
extern char* langinfo_fallback(nl_item item);

#define nl_langinfo langinfo_fallback

enum {
    CODESET = 1,
    RADIXCHAR,
    THOUSEP,
    D_T_FMT,
    D_FMT,
    T_FMT,
    T_FMT_AMPM,
    AM_STR,
    PM_STR,

    DAY_1,
    DAY_2,
    DAY_3,
    DAY_4,
    DAY_5,
    DAY_6,
    DAY_7,

    ABDAY_1,
    ABDAY_2,
    ABDAY_3,
    ABDAY_4,
    ABDAY_5,
    ABDAY_6,
    ABDAY_7,

    MON_1,
    MON_2,
    MON_3,
    MON_4,
    MON_5,
    MON_6,
    MON_7,
    MON_8,
    MON_9,
    MON_10,
    MON_11,
    MON_12,

    ABMON_1,
    ABMON_2,
    ABMON_3,
    ABMON_4,
    ABMON_5,
    ABMON_6,
    ABMON_7,
    ABMON_8,
    ABMON_9,
    ABMON_10,
    ABMON_11,
    ABMON_12,

    ERA_D_FMT,
    ERA_D_T_FMT,
    ERA_T_FMT,
    ALT_DIGITS,
    CRNCYSTR,
    YESEXPR,
    NOEXPR
};

#endif /* !HAVE_LANGINFO_H */

#ifndef HAVE_LANGINFO_ALTMON
#define ALTMON_1 MON_1
#define ALTMON_2 MON_2
#define ALTMON_3 MON_3
#define ALTMON_4 MON_4
#define ALTMON_5 MON_5
#define ALTMON_6 MON_6
#define ALTMON_7 MON_7
#define ALTMON_8 MON_8
#define ALTMON_9 MON_9
#define ALTMON_10 MON_10
#define ALTMON_11 MON_11
#define ALTMON_12 MON_12
#endif /* !HAVE_LANGINFO_ALTMON */

#ifndef HAVE_LANGINFO_NL_ABALTMON
#define _NL_ABALTMON_1 ABMON_1
#define _NL_ABALTMON_2 ABMON_2
#define _NL_ABALTMON_3 ABMON_3
#define _NL_ABALTMON_4 ABMON_4
#define _NL_ABALTMON_5 ABMON_5
#define _NL_ABALTMON_6 ABMON_6
#define _NL_ABALTMON_7 ABMON_7
#define _NL_ABALTMON_8 ABMON_8
#define _NL_ABALTMON_9 ABMON_9
#define _NL_ABALTMON_10 ABMON_10
#define _NL_ABALTMON_11 ABMON_11
#define _NL_ABALTMON_12 ABMON_12
#endif /* !HAVE_LANGINFO_NL_ABALTMON */

static int CLOSE_EXIT_CODE = EXIT_FAILURE;
static int STRTOXX_EXIT_CODE = EXIT_FAILURE;

static inline int
flush_standard_stream(FILE* stream)
{
    int fd;

    errno = 0;

    if (ferror(stream) != 0 || fflush(stream) != 0)
        goto error;

    /*
     * Calling fflush is not sufficient on some filesystems
     * like e.g. NFS, which may defer the actual flush until
     * close. Calling fsync would help solve this, but would
     * probably result in a performance hit. Thus, we work
     * around this issue by calling close on a dup'd file
     * descriptor from the stream.
     */
    if ((fd = fileno(stream)) < 0 || (fd = dup(fd)) < 0 || close(fd) != 0)
        goto error;

    return 0;
error:
    return (errno == EBADF) ? 0 : EOF;
}

/* Meant to be used atexit(close_stdout); */
static inline void
close_stdout(void)
{
    if (flush_standard_stream(stdout) != 0 && !(errno == EPIPE)) {
        if (errno)
            warn(_("write error"));
        else
            warnx(_("write error"));
        _exit(CLOSE_EXIT_CODE);
    }

    if (flush_standard_stream(stderr) != 0)
        _exit(CLOSE_EXIT_CODE);
}

static inline void
close_stdout_atexit(void)
{
    /*
     * Note that close stdout at exit disables ASAN to report memory leaks
     */
#if !defined(__SANITIZE_ADDRESS__)
    atexit(close_stdout);
#endif
}

/* caller guarantees n > 0 */
static inline void xstrncpy(char* dest, const char* src, size_t n)
{
    strncpy(dest, src, n - 1);
    dest[n - 1] = 0;
}

/* This is like strncpy(), but based on memcpy(), so compilers and static
 * analyzers do not complain when sizeof(destination) is the same as 'n' and
 * result is not terminated by zero.
 *
 * Use this function to copy string to logs with fixed sizes (wtmp/utmp. ...)
 * where string terminator is optional.
 */
static inline void* str2memcpy(void* dest, const char* src, size_t n)
{
    size_t bytes = strlen(src) + 1;

    if (bytes > n)
        bytes = n;

    memcpy(dest, src, bytes);
    return dest;
}

static inline char* mem2strcpy(char* dest, const void* src, size_t n, size_t nmax)
{
    if (n + 1 > nmax)
        n = nmax - 1;

    memcpy(dest, src, n);
    dest[nmax - 1] = '\0';
    return dest;
}

/* Reallocate @str according to @newstr and copy @newstr to @str; returns new @str.
 * The @str is not modified if reallocation failed (like classic realloc()).
 */
static inline char* __attribute__((warn_unused_result))
strrealloc(char* str, const char* newstr)
{
    size_t nsz, osz;

    if (!str)
        return newstr ? strdup(newstr) : NULL;
    if (!newstr)
        return NULL;

    osz = strlen(str);
    nsz = strlen(newstr);

    if (nsz > osz)
        str = realloc(str, nsz + 1);
    if (str)
        memcpy(str, newstr, nsz + 1);
    return str;
}

/* Copy string @str to struct @stru to member addressed by @offset */
static inline int strdup_to_offset(void* stru, size_t offset, const char* str)
{
    char** o;
    char* p = NULL;

    if (!stru)
        return -EINVAL;

    o = (char**)((char*)stru + offset);
    if (str) {
        p = strdup(str);
        if (!p)
            return -ENOMEM;
    }

    free(*o);
    *o = p;
    return 0;
}

/* Copy string __str to struct member _m of the struct _s */
#define strdup_to_struct_member(_s, _m, _str) \
    strdup_to_offset((void*)_s, offsetof(__typeof__(*(_s)), _m), _str)

/* Copy string addressed by @offset between two structs */
static inline int strdup_between_offsets(void* stru_dst, void* stru_src, size_t offset)
{
    char** src;
    char** dst;
    char* p = NULL;

    if (!stru_src || !stru_dst)
        return -EINVAL;

    src = (char**)((char*)stru_src + offset);
    dst = (char**)((char*)stru_dst + offset);

    if (*src) {
        p = strdup(*src);
        if (!p)
            return -ENOMEM;
    }

    free(*dst);
    *dst = p;
    return 0;
}

/* Copy string addressed by struct member between two instances of the same
 * struct type */
#define strdup_between_structs(_dst, _src, _m) \
    strdup_between_offsets((void*)_dst, (void*)_src, offsetof(__typeof__(*(_src)), _m))

/*
 * Match string beginning.
 */
static inline const char* startswith(const char* s, const char* prefix)
{
    size_t sz = prefix ? strlen(prefix) : 0;

    if (s && sz && strncmp(s, prefix, sz) == 0)
        return s + sz;
    return NULL;
}

/*
 * Case insensitive match string beginning.
 */
static inline const char* startswith_no_case(const char* s, const char* prefix)
{
    size_t sz = prefix ? strlen(prefix) : 0;

    if (s && sz && strncasecmp(s, prefix, sz) == 0)
        return s + sz;
    return NULL;
}

/*
 * Match string ending.
 */
static inline const char* endswith(const char* s, const char* postfix)
{
    size_t sl = s ? strlen(s) : 0;
    size_t pl = postfix ? strlen(postfix) : 0;

    if (pl == 0)
        return s + sl;
    if (sl < pl)
        return NULL;
    if (memcmp(s + sl - pl, postfix, pl) != 0)
        return NULL;
    return s + sl - pl;
}

/*
 * Skip leading white space.
 */
static inline const char* skip_space(const char* p)
{
    while (isspace(*p))
        ++p;
    return p;
}

static inline const char* skip_blank(const char* p)
{
    while (isblank(*p))
        ++p;
    return p;
}

/* Removes whitespace from the right-hand side of a string (trailing
 * whitespace).
 *
 * Returns size of the new string (without \0).
 */
static inline size_t rtrim_whitespace(unsigned char* str)
{
    size_t i;

    if (!str)
        return 0;
    i = strlen((char*)str);
    while (i) {
        i--;
        if (!isspace(str[i])) {
            i++;
            break;
        }
    }
    str[i] = '\0';
    return i;
}

/* Removes whitespace from the left-hand side of a string.
 *
 * Returns size of the new string (without \0).
 */
static inline size_t ltrim_whitespace(unsigned char* str)
{
    size_t len;
    unsigned char* p;

    if (!str)
        return 0;
    for (p = str; *p && isspace(*p); p++)
        ;

    len = strlen((char*)p);

    if (p > str)
        memmove(str, p, len + 1);

    return len;
}

static inline void strrep(char* s, int find, int replace)
{
    while (s && *s && (s = strchr(s, find)) != NULL)
        *s++ = replace;
}

static inline void strrem(char* s, int rem)
{
    char* p;

    if (!s)
        return;
    for (p = s; *s; s++) {
        if (*s != rem)
            *p++ = *s;
    }
    *p = '\0';
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

int32_t strtos32_or_err(const char* str, const char* errmesg)
{
    int64_t num = strtos64_or_err(str, errmesg);
    if (num < INT32_MIN || num > INT32_MAX) {
        errno = ERANGE;
        err(STRTOXX_EXIT_CODE, "%s: '%s'", errmesg, str);
    }
    return num;
}

static int tolerant;

static inline int ioprio_set(int which, int who, int ioprio)
{
    return syscall(SYS_ioprio_set, which, who, ioprio);
}

static inline int ioprio_get(int which, int who)
{
    return syscall(SYS_ioprio_get, which, who);
}

enum {
    IOPRIO_CLASS_NONE,
    IOPRIO_CLASS_RT,
    IOPRIO_CLASS_BE,
    IOPRIO_CLASS_IDLE,
};

enum {
    IOPRIO_WHO_PROCESS = 1,
    IOPRIO_WHO_PGRP,
    IOPRIO_WHO_USER,
};

#define IOPRIO_CLASS_SHIFT (13)
#define IOPRIO_PRIO_MASK ((1UL << IOPRIO_CLASS_SHIFT) - 1)

#define IOPRIO_PRIO_CLASS(mask) ((mask) >> IOPRIO_CLASS_SHIFT)
#define IOPRIO_PRIO_DATA(mask) ((mask)&IOPRIO_PRIO_MASK)
#define IOPRIO_PRIO_VALUE(class, data) (((class) << IOPRIO_CLASS_SHIFT) | data)

static const char* to_prio[] = {
    [IOPRIO_CLASS_NONE] = "none",
    [IOPRIO_CLASS_RT] = "realtime",
    [IOPRIO_CLASS_BE] = "best-effort",
    [IOPRIO_CLASS_IDLE] = "idle"
};

static int parse_ioclass(const char* str)
{
    size_t i;

    for (i = 0; i < ARRAY_SIZE(to_prio); i++)
        if (!strcasecmp(str, to_prio[i]))
            return i;
    return -1;
}

static void ioprio_print(int pid, int who)
{
    int ioprio = ioprio_get(who, pid);

    if (ioprio == -1)
        err(EXIT_FAILURE, _("ioprio_get failed"));
    else {
        int ioclass = IOPRIO_PRIO_CLASS(ioprio);
        const char* name = _("unknown");

        if (ioclass >= 0 && (size_t)ioclass < ARRAY_SIZE(to_prio))
            name = to_prio[ioclass];

        if (ioclass != IOPRIO_CLASS_IDLE)
            printf(_("%s: prio %lu\n"), name,
                IOPRIO_PRIO_DATA(ioprio));
        else
            printf("%s\n", name);
    }
}

static void ioprio_setid(int which, int ioclass, int data, int who)
{
    int rc = ioprio_set(who, which,
        IOPRIO_PRIO_VALUE(ioclass, data));

    if (rc == -1 && !tolerant)
        err(EXIT_FAILURE, _("ioprio_set failed"));
}

static void __attribute__((__noreturn__)) usage(void)
{
    FILE* out = stdout;
    fputs(USAGE_HEADER, out);
    fprintf(out, _(" %1$s [options] -p <pid>...\n"
                   " %1$s [options] -P <pgid>...\n"
                   " %1$s [options] -u <uid>...\n"
                   " %1$s [options] <command>\n"),
        program_invocation_short_name);

    fputs(USAGE_SEPARATOR, out);
    fputs(_("Show or change the I/O-scheduling class and priority of a process.\n"), out);

    fputs(USAGE_OPTIONS, out);
    fputs(_(" -c, --class <class>    name or number of scheduling class,\n"
            "                          0: none, 1: realtime, 2: best-effort, 3: idle\n"),
        out);
    fputs(_(" -n, --classdata <num>  priority (0..7) in the specified scheduling class,\n"
            "                          only for the realtime and best-effort classes\n"),
        out);
    fputs(_(" -p, --pid <pid>...     act on these already running processes\n"), out);
    fputs(_(" -P, --pgid <pgrp>...   act on already running processes in these groups\n"), out);
    fputs(_(" -t, --ignore           ignore failures\n"), out);
    fputs(_(" -u, --uid <uid>...     act on already running processes owned by these users\n"), out);

    fputs(USAGE_SEPARATOR, out);
    printf(USAGE_HELP_OPTIONS(24));

    exit(EXIT_SUCCESS);
}

int main(int argc, char** argv)
{
    int data = 4, set = 0, ioclass = IOPRIO_CLASS_BE, c;
    int which = 0, who = 0;
    const char* invalid_msg = NULL;

    static const struct option longopts[] = {
        { "classdata", required_argument, NULL, 'n' },
        { "class", required_argument, NULL, 'c' },
        { "help", no_argument, NULL, 'h' },
        { "ignore", no_argument, NULL, 't' },
        { "pid", required_argument, NULL, 'p' },
        { "pgid", required_argument, NULL, 'P' },
        { "uid", required_argument, NULL, 'u' },
        { "version", no_argument, NULL, 'V' },
        { NULL, 0, NULL, 0 }
    };

    setlocale(LC_ALL, "");
    bindtextdomain(PACKAGE, LOCALEDIR);
    textdomain(PACKAGE);
    close_stdout_atexit();

    while ((c = getopt_long(argc, argv, "+n:c:p:P:u:tVh", longopts, NULL)) != EOF)
        switch (c) {
        case 'n':
            data = strtos32_or_err(optarg, _("invalid class data argument"));
            set |= 1;
            break;
        case 'c':
            if (isdigit(*optarg))
                ioclass = strtos32_or_err(optarg,
                    _("invalid class argument"));
            else {
                ioclass = parse_ioclass(optarg);
                if (ioclass < 0)
                    errx(EXIT_FAILURE,
                        _("unknown scheduling class: '%s'"),
                        optarg);
            }
            set |= 2;
            break;
        case 'p':
            if (who)
                errx(EXIT_FAILURE,
                    _("can handle only one of pid, pgid or uid at once"));
            invalid_msg = _("invalid PID argument");
            which = strtos32_or_err(optarg, invalid_msg);
            who = IOPRIO_WHO_PROCESS;
            break;
        case 'P':
            if (who)
                errx(EXIT_FAILURE,
                    _("can handle only one of pid, pgid or uid at once"));
            invalid_msg = _("invalid PGID argument");
            which = strtos32_or_err(optarg, invalid_msg);
            who = IOPRIO_WHO_PGRP;
            break;
        case 'u':
            if (who)
                errx(EXIT_FAILURE,
                    _("can handle only one of pid, pgid or uid at once"));
            invalid_msg = _("invalid UID argument");
            which = strtos32_or_err(optarg, invalid_msg);
            who = IOPRIO_WHO_USER;
            break;
        case 't':
            tolerant = 1;
            break;

        case 'V':
            print_version(EXIT_SUCCESS);
        case 'h':
            usage();
        default:
            errtryhelp(EXIT_FAILURE);
        }

    switch (ioclass) {
    case IOPRIO_CLASS_NONE:
        if ((set & 1) && !tolerant)
            warnx(_("ignoring given class data for none class"));
        data = 0;
        break;
    case IOPRIO_CLASS_RT:
    case IOPRIO_CLASS_BE:
        break;
    case IOPRIO_CLASS_IDLE:
        if ((set & 1) && !tolerant)
            warnx(_("ignoring given class data for idle class"));
        data = 7;
        break;
    default:
        if (!tolerant)
            warnx(_("unknown prio class %d"), ioclass);
        break;
    }

    if (!set && !which && optind == argc)
        /*
         * ion without options, print the current ioprio
         */
        ioprio_print(0, IOPRIO_WHO_PROCESS);
    else if (!set && who) {
        /*
         * ion -p|-P|-u ID [ID ...]
         */
        ioprio_print(which, who);

        for (; argv[optind]; ++optind) {
            which = strtos32_or_err(argv[optind], invalid_msg);
            ioprio_print(which, who);
        }
    } else if (set && who) {
        /*
         * ion -c CLASS -p|-P|-u ID [ID ...]
         */
        ioprio_setid(which, ioclass, data, who);

        for (; argv[optind]; ++optind) {
            which = strtos32_or_err(argv[optind], invalid_msg);
            ioprio_setid(which, ioclass, data, who);
        }
    } else if (argv[optind]) {
        /*
         * ion [-c CLASS] COMMAND
         */
        ioprio_setid(0, ioclass, data, IOPRIO_WHO_PROCESS);
        execvp(argv[optind], &argv[optind]);
        errexec(argv[optind]);
    } else {
        warnx(_("bad usage"));
        errtryhelp(EXIT_FAILURE);
    }

    return EXIT_SUCCESS;
}

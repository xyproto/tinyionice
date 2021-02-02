#include <ctype.h>
#include <errno.h>
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/syscall.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>

#define HAVE_FSYNC
#define HAVE_PROGRAM_INVOCATION_SHORT_NAME "ion"

#include "c.h"
#include "closestream.h"
#include "nls.h"

/* initialize a custom exit code for all *_or_err functions */
extern void strutils_set_exitcode(int exit_code);

extern int parse_size(const char* str, uintmax_t* res, int* power);
extern int strtosize(const char* str, uintmax_t* res);
extern uintmax_t strtosize_or_err(const char* str, const char* errmesg);

extern int32_t strtos32_or_err(const char* str, const char* errmesg);

extern int64_t strtos64_or_err(const char* str, const char* errmesg);

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

static int STRTOXX_EXIT_CODE = EXIT_FAILURE;

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

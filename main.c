#include <assert.h>
#include <ctype.h>
#include <dirent.h>
#include <err.h>
#include <errno.h>
#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <locale.h>
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <unistd.h>

/*
 * Force a compilation error if condition is true, but also produce a
 * result (of value 0 and type size_t), so the expression can be used
 * e.g. in a structure initializer (or wherever else comma expressions
 * aren't permitted).
 */
#define UL_BUILD_BUG_ON_ZERO(e) __extension__(sizeof(struct { int : -!!(e); }))

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

/*
 * Constant strings for usage() functions. For more info see
 * Documentation/{howto-usage-function.txt,boilerplate.c}
 */
#define USAGE_HEADER _("\nUsage:\n")
#define USAGE_OPTIONS _("\nOptions:\n")
#define USAGE_SEPARATOR "\n"
#define USAGE_OPTSTR_HELP _("display this help")
#define USAGE_OPTSTR_VERSION _("display version")

#define USAGE_HELP_OPTIONS(marg_dsc) \
    "%-" #marg_dsc "s%s\n"           \
    "%-" #marg_dsc "s%s\n",          \
        " -h, --help", USAGE_OPTSTR_HELP, " -V, --version", USAGE_OPTSTR_VERSION

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

#include <langinfo.h>

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
    atexit(close_stdout);
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
    return (int32_t)num;
}

static int tolerant;

static inline int ioprio_set(int which, int who, int ioprio)
{
    return (int)syscall(SYS_ioprio_set, which, who, ioprio);
}

static inline int ioprio_get(int which, int who)
{
    return (int)syscall(SYS_ioprio_get, which, who);
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
    for (int i = 0; i < 4; i++) {
        if (!strcasecmp(str, to_prio[i])) {
            return i;
        }
    }
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

        if (ioclass >= 0 && (size_t)ioclass < 4)
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
    fprintf(out, _(" ion [options] -p <pid>...\n"
                   " ion [options] -P <pgid>...\n"
                   " ion [options] -u <uid>...\n"
                   " ion [options] <command>\n"));

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
            printf("%s %s\n", "ion", "1.0.0");
            exit(EXIT_SUCCESS);
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

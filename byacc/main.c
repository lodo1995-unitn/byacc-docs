#include <signal.h>
#include "defs.h"

char dflag;
char lflag;
char rflag;
char tflag;
char vflag;

char *symbol_prefix;
char *file_prefix = "y";
char *myname = "yacc";
char *temp_form = "yacc.XXXXXXX";

int lineno;
int outline;

char *action_file_name;
char *code_file_name;
char *defines_file_name;
char *input_file_name = "";
char *output_file_name;
char *text_file_name;
char *union_file_name;
char *verbose_file_name;

FILE *action_file;      /*  a temp file, used to save actions associated    */
                        /*  with rules until the parser is written          */
FILE *code_file;        /*  y.code.c (used when the -r option is specified) */
FILE *defines_file;     /*  y.tab.h                                         */
FILE *input_file;       /*  the input file                                  */
FILE *output_file;      /*  y.tab.c                                         */
FILE *text_file;        /*  a temp file, used to save text until all        */
                        /*  symbols have been defined                       */
FILE *union_file;       /*  a temp file, used to save the union             */
                        /*  definition until all symbol have been           */
                        /*  defined                                         */
FILE *verbose_file;     /*  y.output                                        */

int nitems;

/**
*   @brief The number of rules in the grammar
*/
int nrules;

/**
*   @brief The number of symbols (terminals + non-terminals) in the grammar
*
*   All symbols can be uniquely represented using integers in the range [0, nsyms - 1], which is obtained
*   by joining the range of terminals [0, ntokens - 1] with the range of non-terminals [ntokens, nsyms - 1].
*
*   It holds that nsyms = ntokens + nvars.   
*/
int nsyms;

/**
*   @brief The number of tokens (terminals) in the grammar
*
*   All tokens can be uniquely represented using integers in the range [0, ntokens - 1].
*
*   It holds that nsyms = ntokens + nvars.
*/
int ntokens;

/**
*   @brief The number of variables (non-terminals) in the grammar
*
*   All variables can be uniquely represented using integers in the range [ntokens, nsyms - 1], which is equivalent
*   to the range [ntokens, ntokens + nvars - 1].
*
*   It holds that nsyms = ntokens + nvars.
*/
int nvars;

/**
*   @brief Index of the starting symbol of the grammar
*
*   It holds that start_symbol = ntokens. In fact, the starting symbol is always placed at the beginning of the
*   non-terminal range [ntokens, nsyms - 1].
*/
int   start_symbol;

/**
*   @brief Array of symbol names.
*
*   Array of strings representing the names of all symbols. All names are allocated in a single contiguous block of
*   memory to improve locality.
*/
char  **symbol_name;
short *symbol_value;
short *symbol_prec;
char  *symbol_assoc;

/**
*   @brief Representation of all productions (and items)
*
*   Typical shape: [1, 12, 21, -1, 2, 3, -2, 1, 4, -3, ...]
*
*   All productions are represented in this array as the list of their right-hand side symbols followed by the negation
*   of their index. So the symbols on the right-hand side of the first rule are followed by -1, then there are the
*   right-hand side symbols of the second rule and then -2, and so on.
*
*   Indices (using short integers) inside this array can represent rules (in that case they point to the first element
*   after a negative one) or items, in which case they can point to any position; the element in that position is taken
*   to be the first element after the "point" of the item. If the index points to a negative number, then it represents
*   a reduction item for the rule whose number is the absolute value of the pointed element.
*
*   The array called rrhs contains indices inside this list and is used to associate to each rule the beginning of its
*   production (that is, the closure item for that rule).
*/
short *ritem;

/**
*   @brief List of left-hand sides of all rules
*
*   This array associates to each production its left-hand side symbol.
*/
short *rlhs;

/**
*   @brief List of right-hand sides of all rules
*
*   Array of indices inside ritem. It is used record the position inside ritem where each rule begins.
*/
short *rrhs;
short *rprec;
char  *rassoc;

/**
*   @brief List of rules that derive each non-terminal
*
*   Array of pointers that associates to each symbol the list of productions that have it as the left-hand side. Each
*   item in those lists is the identifying number of a rule. The first ntokens entries (the ones for terminals) are not
*   set and should not be accessed. They are only present to make the indexes consistent with other arrays (i.e. this
*   array can be indexed using the symbol number).
*
*   Allocated and filled in set_first_derives().
*/
short **derives;
char *nullable;

extern char *mktemp();
extern char *getenv();


/**
*   @ingroup eh
*   @brief Shutdown function
*
*   This function deletes all temporary files still open and exits the program
*   with the status code passed to it.
*
*   @param[in] k The status code the program should exit with
*/
done(k)
int k;
{
    if (action_file) { fclose(action_file); unlink(action_file_name); }
    if (text_file) { fclose(text_file); unlink(text_file_name); }
    if (union_file) { fclose(union_file); unlink(union_file_name); }
    exit(k);
}

/**
*   @ingroup eh
*   @brief Signal handler callback
*
*   This function is executed whenever the program receives a shutdown signal
*   (either SIGINT, SIGTERM, or SIGHUP). It just calls done(int) with code 1.
*/
onintr()
{
    done(1);
}


/**
*   @ingroup eh
*   @brief Sets the appropriate signal handlers up
*
*   This function is executed on startup and sets onintr() as callback for any
*   shutdown signal (SIGINT, SIGTERM, SIGHUP).
*/
set_signals()
{
#ifdef SIGINT
    if (signal(SIGINT, SIG_IGN) != SIG_IGN)
        signal(SIGINT, onintr);
#endif
#ifdef SIGTERM
    if (signal(SIGTERM, SIG_IGN) != SIG_IGN)
        signal(SIGTERM, onintr);
#endif
#ifdef SIGHUP
    if (signal(SIGHUP, SIG_IGN) != SIG_IGN)
        signal(SIGHUP, onintr);
#endif
}


usage()
{
    fprintf(stderr, "usage: %s [-dlrtv] [-b file_prefix] [-p symbol_prefix] filename\n", myname);
    exit(1);
}


getargs(argc, argv)
int argc;
char *argv[];
{
    register int i;
    register char *s;

    if (argc > 0) myname = argv[0];
    for (i = 1; i < argc; ++i)
    {
        s = argv[i];
        if (*s != '-') break;
        switch (*++s)
        {
        case '\0':
            input_file = stdin;
            if (i + 1 < argc) usage();
            return;

        case '-':
            ++i;
            goto no_more_options;

        case 'b':
            if (*++s)
                file_prefix = s;
            else if (++i < argc)
                file_prefix = argv[i];
            else
                usage();
            continue;

        case 'd':
            dflag = 1;
            break;

        case 'l':
            lflag = 1;
            break;

        case 'p':
            if (*++s)
                symbol_prefix = s;
            else if (++i < argc)
                symbol_prefix = argv[i];
            else
                usage();
            continue;

        case 'r':
            rflag = 1;
            break;

        case 't':
            tflag = 1;
            break;

        case 'v':
            vflag = 1;
            break;

        default:
            usage();
        }

        for (;;)
        {
            switch (*++s)
            {
            case '\0':
                goto end_of_option;

            case 'd':
                dflag = 1;
                break;

            case 'l':
                lflag = 1;
                break;

            case 'r':
                rflag = 1;
                break;

            case 't':
                tflag = 1;
                break;

            case 'v':
                vflag = 1;
                break;

            default:
                usage();
            }
        }
end_of_option:;
    }

no_more_options:;
    if (i + 1 != argc) usage();
    input_file_name = argv[i];
}


char *
allocate(n)
unsigned n;
{
    register char *p;

    p = NULL;
    if (n)
    {
        p = CALLOC(1, n);
        if (!p) no_space();
    }
    return (p);
}


create_file_names()
{
    int i, len;
    char *tmpdir;

    tmpdir = getenv("TMPDIR");
    if (tmpdir == 0) tmpdir = "/tmp";

    len = strlen(tmpdir);
    i = len + 13;
    if (len && tmpdir[len-1] != '/')
        ++i;

    action_file_name = MALLOC(i);
    if (action_file_name == 0) no_space();
    text_file_name = MALLOC(i);
    if (text_file_name == 0) no_space();
    union_file_name = MALLOC(i);
    if (union_file_name == 0) no_space();

    strcpy(action_file_name, tmpdir);
    strcpy(text_file_name, tmpdir);
    strcpy(union_file_name, tmpdir);

    if (len && tmpdir[len - 1] != '/')
    {
        action_file_name[len] = '/';
        text_file_name[len] = '/';
        union_file_name[len] = '/';
        ++len;
    }

    strcpy(action_file_name + len, temp_form);
    strcpy(text_file_name + len, temp_form);
    strcpy(union_file_name + len, temp_form);

    action_file_name[len + 5] = 'a';
    text_file_name[len + 5] = 't';
    union_file_name[len + 5] = 'u';

    mktemp(action_file_name);
    mktemp(text_file_name);
    mktemp(union_file_name);

    len = strlen(file_prefix);

    output_file_name = MALLOC(len + 7);
    if (output_file_name == 0)
        no_space();
    strcpy(output_file_name, file_prefix);
    strcpy(output_file_name + len, OUTPUT_SUFFIX);

    if (rflag)
    {
        code_file_name = MALLOC(len + 8);
        if (code_file_name == 0)
            no_space();
        strcpy(code_file_name, file_prefix);
        strcpy(code_file_name + len, CODE_SUFFIX);
    }
    else
        code_file_name = output_file_name;

    if (dflag)
    {
        defines_file_name = MALLOC(len + 7);
        if (defines_file_name == 0)
            no_space();
        strcpy(defines_file_name, file_prefix);
        strcpy(defines_file_name + len, DEFINES_SUFFIX);
    }

    if (vflag)
    {
        verbose_file_name = MALLOC(len + 8);
        if (verbose_file_name == 0)
            no_space();
        strcpy(verbose_file_name, file_prefix);
        strcpy(verbose_file_name + len, VERBOSE_SUFFIX);
    }
}


open_files()
{
    create_file_names();

    if (input_file == 0)
    {
        input_file = fopen(input_file_name, "r");
        if (input_file == 0)
            open_error(input_file_name);
    }

    action_file = fopen(action_file_name, "w");
    if (action_file == 0)
        open_error(action_file_name);

    text_file = fopen(text_file_name, "w");
    if (text_file == 0)
        open_error(text_file_name);

    if (vflag)
    {
        verbose_file = fopen(verbose_file_name, "w");
        if (verbose_file == 0)
            open_error(verbose_file_name);
    }

    if (dflag)
    {
        defines_file = fopen(defines_file_name, "w");
        if (defines_file == 0)
            open_error(defines_file_name);
        union_file = fopen(union_file_name, "w");
        if (union_file ==  0)
            open_error(union_file_name);
    }

    output_file = fopen(output_file_name, "w");
    if (output_file == 0)
        open_error(output_file_name);

    if (rflag)
    {
        code_file = fopen(code_file_name, "w");
        if (code_file == 0)
            open_error(code_file_name);
    }
    else
        code_file = output_file;
}


int
main(argc, argv)
int argc;
char *argv[];
{
    set_signals();
    getargs(argc, argv);
    open_files();
    reader();
    lr0();
    lalr();
    make_parser();
    verbose();
    output();
    done(0);
    /*NOTREACHED*/
}

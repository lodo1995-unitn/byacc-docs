#include "defs.h"

short *itemset;
short *itemsetend;
unsigned *ruleset;

/**
*   @brief Matrix of closure productions
*   
*   Let \f$ V \f$ be the set of all variables (non-terminals) and \f$ \mathcal{P} \f$ the set of all productions.
*
*   This is a matrix of \f$ \left| V \right| \times \left| \mathcal{P} \right| \f$ bits, where entry
*   \f$ \mathrm{first\_derives}_{i,j} \f$ is set if and only if the closure of an item of kind
*   \f$ A \rightarrow \alpha . V_i \beta \f$ should include the production \f$ \mathcal{P}_j \f$ (with the dot on
*   the far left).
*
*   It can be seen as a relation \f$ V \rightarrow 2^{\mathcal{P}} \f$.
*/
static unsigned *first_derives;

/**
*   @brief Matrix of Epsilon-Free Firsts
*   
*   Let \f$ V \f$ be the set of all variables (non-terminals) and \f$ \mathcal{P} \f$ the set of all productions.
*
*   This is a matrix of \f$ \left| V \right| \times \left| V \right| \f$ bits, where entry \f$ \mathrm{EFF}_{i,j} \f$
*   is set if and only if the closure of an item of kind \f$ A \rightarrow \alpha . V_i \beta \f$ should include the
*   productions of \f$ V_j \f$ (i.e. the closure items of the form \f$ V_j \rightarrow . \alpha \f$).
*
*   It can be seen as a relation \f$ V \rightarrow 2^V \f$.
*/
static unsigned *EFF;


/**
*   @brief Populates EFF, the matrix of Epsilon-Free Firsts
*   
*   Let \f$ V \f$ be the set of all variables (non-terminals) and \f$ \mathcal{P} \f$ the set of all productions.
*
*   For each non-terminal \f$ V_i \f$, scans all of its productions. If they start with a non-terminal \f$ V_j \f$,
*   sets \f$ \mathrm{EFF}_{i,j} \f$, thus computing the direct epsilon-free firsts of \f$ V_i \f$. It then calls a
*   function that computes the reflexive and transitive closure of such relation.
*/
set_EFF()
{
    register unsigned *row;
    register int symbol;
    register short *sp;
    register int rowsize;
    register int i;
    register int rule;

    rowsize = WORDSIZE(nvars);
    EFF = NEW2(nvars * rowsize, unsigned);

    row = EFF;
    for (i = start_symbol; i < nsyms; i++)
    {
        sp = derives[i];
        for (rule = *sp; rule > 0; rule = *++sp)
        {
            symbol = ritem[rrhs[rule]];
            if (ISVAR(symbol))
            {
                symbol -= start_symbol;
                SETBIT(row, symbol);
            }
        }
        row += rowsize;
    }

    reflexive_transitive_closure(EFF, nvars);

#ifdef DEBUG
    print_EFF();
#endif
}


/**
*   @brief Populates first_derives, the matrix of closure productions
*   
*   Let \f$ V \f$ be the set of all variables (non-terminals) and \f$ \mathcal{P} \f$ the set of all productions.
*
*   Using the information saved in EFF, computes the closure productions of each non-terminal. That is, it sets
*   \f$ \mathrm{first\_derives}_{i,j} \f$ if and only if the left-hand side of \f$ \mathcal{P}_j \f$ is a \f$ V_k \f$
*   such that \f$ \mathrm{EFF}_{i,k} \f$ is set.
*
*   In other words, it calculates the same information as set_EFF(), expanding the list of non-terminals to the list of
*   their productions.
*/
set_first_derives()
{
    register unsigned *rrow;
    register unsigned *vrow;
    register int j;
    register unsigned k;
    register unsigned cword;
    register short *rp;

    int rule;
    int i;
    int rulesetsize;
    int varsetsize;

    rulesetsize = WORDSIZE(nrules);
    varsetsize = WORDSIZE(nvars);
    first_derives = NEW2(nvars * rulesetsize, unsigned) - ntokens * rulesetsize;

    set_EFF();

    rrow = first_derives + ntokens * rulesetsize;
    for (i = start_symbol; i < nsyms; i++)
    {
        vrow = EFF + ((i - ntokens) * varsetsize);
        k = BITS_PER_WORD;
        for (j = start_symbol; j < nsyms; k++, j++)
        {
            if (k >= BITS_PER_WORD)
            {
                cword = *vrow++;
                k = 0;
            }

            if (cword & (1 << k))
            {
                rp = derives[j];
                while ((rule = *rp++) >= 0)
                {
                    SETBIT(rrow, rule);
                }
            }
        }

        vrow += varsetsize;
        rrow += rulesetsize;
    }

#ifdef DEBUG
    print_first_derives();
#endif

    FREE(EFF);
}


closure(nucleus, n)
short *nucleus;
int n;
{
    register int ruleno;
    register unsigned word;
    register unsigned i;
    register short *csp;
    register unsigned *dsp;
    register unsigned *rsp;
    register int rulesetsize;

    short *csend;
    unsigned *rsend;
    int symbol;
    int itemno;

    rulesetsize = WORDSIZE(nrules);
    rsp = ruleset;
    rsend = ruleset + rulesetsize;
    for (rsp = ruleset; rsp < rsend; rsp++)
        *rsp = 0;

    csend = nucleus + n;
    for (csp = nucleus; csp < csend; ++csp)
    {
        symbol = ritem[*csp];
        if (ISVAR(symbol))
        {
            dsp = first_derives + symbol * rulesetsize;
            rsp = ruleset;
            while (rsp < rsend)
                *rsp++ |= *dsp++;
        }
    }

    ruleno = 0;
    itemsetend = itemset;
    csp = nucleus;
    for (rsp = ruleset; rsp < rsend; ++rsp)
    {
        word = *rsp;
        if (word)
        {
            for (i = 0; i < BITS_PER_WORD; ++i)
            {
                if (word & (1 << i))
                {
                    itemno = rrhs[ruleno+i];
                    while (csp < csend && *csp < itemno)
                        *itemsetend++ = *csp++;
                    *itemsetend++ = itemno;
                    while (csp < csend && *csp == itemno)
                        ++csp;
                }
            }
        }
        ruleno += BITS_PER_WORD;
    }

    while (csp < csend)
        *itemsetend++ = *csp++;

#ifdef DEBUG
    print_closure(n);
#endif
}



finalize_closure()
{
    FREE(itemset);
    FREE(ruleset);
    FREE(first_derives + ntokens * WORDSIZE(nrules));
}


#ifdef DEBUG

print_closure(n)
int n;
{
    register short *isp;

    printf("\n\nn = %d\n\n", n);
    for (isp = itemset; isp < itemsetend; isp++)
        printf("   %d\n", *isp);
}


print_EFF()
{
    register int i, j;
    register unsigned *rowp;
    register unsigned word;
    register unsigned k;

    printf("\n\nEpsilon Free Firsts\n");

    for (i = start_symbol; i < nsyms; i++)
    {
        printf("\n%s", symbol_name[i]);
        rowp = EFF + ((i - start_symbol) * WORDSIZE(nvars));
        word = *rowp++;

        k = BITS_PER_WORD;
        for (j = 0; j < nvars; k++, j++)
        {
            if (k >= BITS_PER_WORD)
            {
                word = *rowp++;
                k = 0;
            }

            if (word & (1 << k))
                printf("  %s", symbol_name[start_symbol + j]);
        }
    }
}


print_first_derives()
{
    register int i;
    register int j;
    register unsigned *rp;
    register unsigned cword;
    register unsigned k;

    printf("\n\n\nFirst Derives\n");

    for (i = start_symbol; i < nsyms; i++)
    {
        printf("\n%s derives\n", symbol_name[i]);
        rp = first_derives + i * WORDSIZE(nrules);
        k = BITS_PER_WORD;
        for (j = 0; j <= nrules; k++, j++)
        {
            if (k >= BITS_PER_WORD)
            {
                cword = *rp++;
                k = 0;
            }

            if (cword & (1 << k))
                printf("   %d\n", j);
        }
    }

  fflush(stdout);
}

#endif

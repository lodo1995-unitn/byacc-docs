#include "defs.h"

/**
*   Computes the transitive closure of a relation in-place.
*
*   Let \f$ \mathrm{R} \f$ be an \f$ \mathrm{n} \times \mathrm{n} \f$ matrix of bits expressing a relation between n
*   elements, such that \f$ \mathrm{R}_{i,j} \f$ is set if and only if the i-th elements is in relation with the j-th one.
*
*   Then this function computes the transitive closure of such relation and stores it in-place, using Warshall's
*   algorithm. Its complexity is \f$ O\left( n^3 \right) \f$.
*/

transitive_closure(R, n)
unsigned *R;
int n;
{
    register int rowsize;
    register unsigned i;
    register unsigned *rowj;
    register unsigned *rp;
    register unsigned *rend;
    register unsigned *ccol;
    register unsigned *relend;
    register unsigned *cword;
    register unsigned *rowi;

    rowsize = WORDSIZE(n);
    relend = R + n*rowsize;

    cword = R;
    i = 0;
    rowi = R;
    while (rowi < relend)
    {
        ccol = cword;
        rowj = R;

        while (rowj < relend)
        {
            if (*ccol & (1 << i))
            {
                rp = rowi;
                rend = rowj + rowsize;
                while (rowj < rend)
                    *rowj++ |= *rp++;
            }
            else
            {
                rowj += rowsize;
            }

            ccol += rowsize;
        }

        if (++i >= BITS_PER_WORD)
        {
            i = 0;
            cword++;
        }

        rowi += rowsize;
    }
}

/**
*   Computes the transitive closure of a relation in-place.
*
*   Let \f$ \mathrm{R} \f$ be an \f$ \mathrm{n} \times \mathrm{n} \f$ matrix of bits expressing a relation between n
*   elements, such that \f$ \mathrm{R}_{i,j} \f$ is set if and only if the i-th elements is in relation with the j-th one.
*
*   Then this function computes the reflexive and transitive closure of such relation and stores it in-place, using
*   Warshall's algorithm. Its complexity is \f$ O\left( n^3 \right) \f$, dominated by the complexity of
*   transitive_closure().
*
*   Internally, this function just calls transitive_closure() and then sets all elements on the matrix diagonal to make
*   the relation reflexive.
*/

reflexive_transitive_closure(R, n)
unsigned *R;
int n;
{
    register int rowsize;
    register unsigned i;
    register unsigned *rp;
    register unsigned *relend;

    transitive_closure(R, n);

    rowsize = WORDSIZE(n);
    relend = R + n*rowsize;

    i = 0;
    rp = R;
    while (rp < relend)
    {
        *rp |= (1 << i);
        if (++i >= BITS_PER_WORD)
        {
            i = 0;
            rp++;
        }

        rp += rowsize;
    }
}

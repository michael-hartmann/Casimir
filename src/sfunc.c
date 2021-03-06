#include <assert.h>
#include <stdio.h>
#include <math.h>

#include "edouble.h"
#include "sfunc.h"

edouble inline logadd_s(const edouble a, const int sign_a, const edouble b, const int sign_b, int *sign)
{
    if(isinfq(a) && a < 0)
    {
        *sign = sign_b;
        return b;
    }
    else if(isinfq(b) && b < 0)
    {
        *sign = sign_a;
        return a;
    }

    if(a > b)
    {
        *sign = sign_a;
        return a + log1pq(sign_a*sign_b*expq(b-a));
    }
    else
    {
        *sign = sign_b;
        return b + log1pq(sign_a*sign_b*expq(a-b));
    }
}


edouble inline logadd_ms(const edouble list[], const int signs[], const size_t len, int *sign)
{
    size_t i;
    edouble sum;
    edouble max = list[0];

    for(i = 1; i < len; i++)
        if(list[i] > max)
            max = list[i];

    sum = signs[0]*expq(list[0]-max);
    for(i = 1; i < len; i++)
        sum += signs[i]*expq(list[i]-max);

    *sign = copysignq(1, sum);
    return max + logq(fabsq(sum));
}


edouble inline lbinom(int n, int k)
{
    return lngamma(1+n)-lngamma(1+k)-lngamma(1+n-k);
}


void bessel_lnInuKnu(int nu, const edouble x, edouble *lnInu_p, edouble *lnKnu_p)
{
    int l;
    edouble lnKnu = 1, lnKnup = 1+1./x;
    edouble logx = logq(x);

    // calculate Knu, Knup
    {
        edouble prefactor = -x+0.5*(LOGPI-LOG2-logx);

        if(nu == 0)
        {
            lnKnu  = prefactor+logq(lnKnu);
            lnKnup = prefactor+logq(lnKnup);
        }
        else
        {
            for(l = 2; l <= nu+1; l++)
            {
                edouble Kn = (2*l-1)*lnKnup/x + lnKnu;
                lnKnu  = lnKnup;
                lnKnup = Kn;
            }

            lnKnup = prefactor+logq(lnKnup);
            lnKnu  = prefactor+logq(lnKnu);
        }

        if(isnanq(lnKnup) || isinfq(lnKnup))
        {
            /* so, we couldn't calculate lnKnup and lnKnu. Maybe we can at
             * least use the asymptotic behaviour for small values.
             */
            if(x < sqrt(nu)*1e3)
            {
                /* small arguments */
                lnKnu  = lngamma(nu+0.5)-LOG2+(nu+0.5)*(LOG2-logx);
                lnKnup = lngamma(nu+1.5)-LOG2+(nu+1.5)*(LOG2-logx);
            }
            else
                lnKnu = lnKnup = 0;
        }

        if(lnKnu_p != NULL)
            *lnKnu_p = lnKnu;
    }

    if(lnInu_p != NULL)
    {
        #define an(n,nu,x) (2*(nu+0.5+n)/x)

        edouble nom   = an(2,nu,x)+1/an(1,nu,x);
        edouble denom = an(2,nu,x);
        edouble ratio = (an(1,nu,x)*nom)/denom;
        edouble ratio_last = 0;

        l = 3;
        while(1)
        {
            nom   = an(l,nu,x)+1/nom;
            denom = an(l,nu,x)+1/denom;
            ratio *= nom/denom;

            if(ratio_last != 0 && fabs(1-ratio/ratio_last) < 1e-15)
                break;

            ratio_last = ratio;
            l++;
        }

        *lnInu_p = -logx-lnKnu-logq(expq(lnKnup-lnKnu)+1/ratio);
    }
}


edouble bessel_lnKnu(const int nu, const edouble x)
{
    edouble Knu;
    bessel_lnInuKnu(nu, x, NULL, &Knu);
    return Knu;
}


edouble bessel_lnInu(const int nu, const edouble x)
{
    edouble Inu;
    bessel_lnInuKnu(nu, x, &Inu, NULL);
    return Inu;
}


double linspace(double start, double stop, int N, int i)
{
    if(start == stop)
        return start;

    return start+(stop-start)*i/(N-1);
}


double logspace(double start, double stop, int N, int i)
{
    if(start == stop)
        return start;

    return start*pow(pow(stop/start, 1./(N-1)), i);
}

edouble ln_doublefact(int n)
{
    if(n < 0)
        return NAN;

    if(n == 0 || n == 1) /* 0!! = 1!! = 0 */
        return 0;

    if(n % 2 == 0) /* even */
    {
        int k = n/2;
        return k*LOG2 + lnfac(k);
    }
    else /* odd */
    {
        int k = (n+1)/2;
        return lnfac(2*k) - k*LOG2 - lnfac(k);
    }
}


/* This module implements associated legendre functions and its derivatives
 * for m >= 0 and x >= 1.
 * 
 * Associated Legendre polynomials are defined as follows:
 *     Plm(x) = (-1)^m (1-x²)^(m/2) * d^m/dx^m Pl(x)
 * where Pl(x) denotes a Legendre polynomial.
 *
 * As Pl(x) are ordinary polynomials, the only problem is the term (1-x²) when
 * extending the domain to values of x > 1.
 *
 * (*) Note:
 * Products of associated legendre polynomials with common m are unambiguous, because
 *     (i)² = (-i)² = -1.
 */

/* calculate Plm for l=m...l=lmax */
static inline void _lnplm_array(int lmax, int m, edouble x, edouble lnplm[], int sign[])
{
    int l;
    edouble logx = logq(x);

    if(m == 0)
    {
        sign[0] = +1;
        lnplm[0] = 0; // log(1)
    }
    else
    {
        sign[0]  = MPOW((int)(m/2) + m%2);
        lnplm[0] = ln_doublefact(2*m-1) + m*0.5*logq(pow_2(x)-1); // l=m,m=m
    }

    if(lmax == m)
        return;

    sign[1]  = sign[0];
    lnplm[1] = lnplm[0]+logx+logq(2*m+1); // l=m+1, m=m

    for(l = m+2; l <= lmax; l++)
    {
        lnplm[l-m] = logadd_s(logq(2*l-1)+logx+lnplm[l-m-1], sign[l-m-1], logq(l+m-1)+lnplm[l-m-2], -sign[l-m-2], &sign[l-m]);
        lnplm[l-m]-= logq(l-m);
    }
}

/* calculate Plm(x) */
edouble plm_lnPlm(int l, int m, edouble x, int *sign)
{
    edouble plm[l-m+1];
    int  signs[l-m+1];

    _lnplm_array(l, m, x, plm, signs);
    *sign = signs[l-m];

    return plm[l-m];
}

edouble plm_Plm(int l, int m, edouble x)
{
    int sign;
    edouble value = plm_lnPlm(l, m, x, &sign);
    return sign*expq(value);
}

/* calculate dPlm(x) */
edouble plm_lndPlm(int l, int m, edouble x, int *sign)
{
    const int lmax = l+1;
    edouble plm[lmax-m+1];
    int signs[lmax-m+1];

    _lnplm_array(lmax, m, x, plm, signs);

    return logadd_s(logq(l-m+1)+plm[l+1-m], signs[l+1-m], logq(l+1)+logq(x)+plm[l-m], -signs[l+1-m], sign) - logq(pow_2(x)-1);
}


edouble plm_dPlm(int l, int m, edouble x)
{
    int sign;
    edouble value = plm_lndPlm(l, m, x, &sign);
    return sign*expq(value);
}

void plm_PlmPlm(int l1, int l2, int m, edouble x, plm_combination_t *res)
{
    const int lmax = MAX(l1,l2)+1;
    edouble lnPlm[lmax-m+1];
    int signs[lmax-m+1];
    edouble logx = logq(x);
    edouble logx2m1 = logq(pow_2(x)-1);
    edouble lnPl1m, lnPl2m, lndPl1m, lndPl2m;
    int sign_Pl1m, sign_Pl2m, sign_dPl1m, sign_dPl2m;
    int common_sign = MPOW(m%2);

    _lnplm_array(lmax, m, x, lnPlm, signs);

    lnPl1m    = lnPlm[l1-m];
    sign_Pl1m = signs[l1-m];
    lnPl2m    = lnPlm[l2-m];
    sign_Pl2m = signs[l2-m];

    lndPl1m = logadd_s(logq(l1-m+1)+lnPlm[l1+1-m], signs[l1+1-m], logq(l1+1)+logx+lnPlm[l1-m], -signs[l1+1-m], &sign_dPl1m) - logx2m1;
    lndPl2m = logadd_s(logq(l2-m+1)+lnPlm[l2+1-m], signs[l2+1-m], logq(l2+1)+logx+lnPlm[l2-m], -signs[l2+1-m], &sign_dPl2m) - logx2m1;

    /* Pl1m*Pl2m */
    res->lnPl1mPl2m    = lnPl1m + lnPl2m;
    res->sign_Pl1mPl2m = common_sign * sign_Pl1m * sign_Pl2m;

    /* Pl1m*dPl2m */
    res->lnPl1mdPl2m    = lnPl1m + lndPl2m;
    res->sign_Pl1mdPl2m = common_sign * sign_Pl1m * sign_dPl2m;

    /* dPl1m*dPl2m */
    res->lndPl1mPl2m    = lndPl1m + lnPl2m;
    res->sign_dPl1mPl2m = common_sign * sign_dPl1m * sign_Pl2m;

    /* dPl1m*dPl2m */
    res->lndPl1mdPl2m    = lndPl1m + lndPl2m;
    res->sign_dPl1mdPl2m = common_sign * sign_dPl1m * sign_dPl2m;
}

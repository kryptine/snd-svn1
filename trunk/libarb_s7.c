#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <gmp.h>
#include <mpfr.h>
#include <mpc.h>

#include <arb.h>
#include <acb.h>
#include <acb_hypgeom.h>
#include <acb_elliptic.h>

#define WITH_GMP 1
#include "s7.h"

static void s7_number_to_acb(s7_scheme *sc, acb_t z, s7_pointer x, slong prec)
{
  if (s7_is_big_real(x))
    {
      arf_set_mpfr((arf_struct *)acb_realref(z), *s7_big_real(x));
      arb_zero(acb_imagref(z));
    }
  else
    {
      if (s7_is_big_integer(x))
	{
	  arf_set_mpz((arf_struct *)acb_realref(z), *s7_big_integer(x));
	  arb_zero(acb_imagref(z));
	}
      else
	{
	  if (s7_is_integer(x))
	    acb_set_si(z, s7_integer(x));
	  else
	    {
	      if (s7_is_big_ratio(x))
		{
		  mpfr_t mq;
		  mpfr_init2(mq, prec);
		  mpfr_set_q(mq, *s7_big_ratio(x), MPFR_RNDN);
		  arf_set_mpfr((arf_struct *)acb_realref(z), mq);
		  arb_zero(acb_imagref(z));
		  mpfr_clear(mq);
		}
	      else
		{
		  if (s7_is_real(x))
		    acb_set_d(z, s7_real(x));
		  else 
		    {
		      if (s7_is_big_complex(x))
			{
			  arf_set_mpfr((arf_struct *)acb_realref(z), mpc_realref(*s7_big_complex(x)));
			  arf_set_mpfr((arf_struct *)acb_imagref(z), mpc_imagref(*s7_big_complex(x)));
			}
		      else
			{
			  if (s7_is_complex(x))
			    acb_set_d_d(z, s7_real_part(x), s7_imag_part(x));
			}}}}}}
}

static s7_pointer acb_to_s7(s7_scheme *sc, acb_t w, slong prec)
{
  s7_pointer result;
  if (arb_is_zero(acb_imagref(w)))
    {
      mpfr_t mp;
      mpfr_init2(mp, prec);
      arf_get_mpfr(mp, (const arf_struct *)acb_realref(w), MPFR_RNDN);
      result = s7_make_big_real(sc, &mp);
      mpfr_clear(mp);
    }
  else
    {
      mpc_t mp;
      mpc_init2(mp, prec);
      arf_get_mpfr(mpc_realref(mp), (const arf_struct *)acb_realref(w), MPFR_RNDN);
      arf_get_mpfr(mpc_imagref(mp), (const arf_struct *)acb_imagref(w), MPFR_RNDN);
      result = s7_make_big_complex(sc, &mp);
      mpc_clear(mp);
    }
  return(result);
}


static acb_t nu, w, z;
/* these should be local to s7_scheme someday via a void* list or something */


/* -------------------------------- aci -------------------------------- */
static s7_pointer aci(s7_scheme *sc, s7_pointer args, const char *b_name, void acb_func(acb_t res, const acb_t z, slong prec))
{
  s7_pointer n, x;
  slong prec;

  prec = s7_integer(s7_let_field_ref(sc, s7_make_symbol(sc, "bignum-precision")));

  n = s7_car(args);
  if (!s7_is_number(n))
    return(s7_wrong_type_arg_error(sc, b_name, 1, n, "number"));
  s7_number_to_acb(sc, z, n, prec);

  acb_func(w, z, prec);
  return(acb_to_s7(sc, w, prec));
}

static s7_pointer a_erf(s7_scheme *sc, s7_pointer args)  {return(aci(sc, args, "acb_erf", acb_hypgeom_erf));}
static s7_pointer a_erfc(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_erfc", acb_hypgeom_erfc));}
static s7_pointer a_erfi(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_erfi", acb_hypgeom_erfi));}

static s7_pointer a_ei(s7_scheme *sc, s7_pointer args)        {return(aci(sc, args, "acb_ei", acb_hypgeom_ei));}
static s7_pointer a_si(s7_scheme *sc, s7_pointer args)        {return(aci(sc, args, "acb_si", acb_hypgeom_si));}
static s7_pointer a_ci(s7_scheme *sc, s7_pointer args)        {return(aci(sc, args, "acb_ci", acb_hypgeom_ci));}
static s7_pointer a_shi(s7_scheme *sc, s7_pointer args)       {return(aci(sc, args, "acb_shi", acb_hypgeom_shi));}
static s7_pointer a_chi(s7_scheme *sc, s7_pointer args)       {return(aci(sc, args, "acb_chi", acb_hypgeom_chi));}
static s7_pointer a_erf_1f1a(s7_scheme *sc, s7_pointer args)  {return(aci(sc, args, "acb_erf_1f1a", acb_hypgeom_erf_1f1a));}
static s7_pointer a_erf_1f1b(s7_scheme *sc, s7_pointer args)  {return(aci(sc, args, "acb_erf_1f1b", acb_hypgeom_erf_1f1b));}
static s7_pointer a_ei_asymp(s7_scheme *sc, s7_pointer args)  {return(aci(sc, args, "acb_ei_asymp", acb_hypgeom_ei_asymp));}
static s7_pointer a_ei_2f2(s7_scheme *sc, s7_pointer args)    {return(aci(sc, args, "acb_ei_2f2", acb_hypgeom_ei_2f2));}
static s7_pointer a_si_asymp(s7_scheme *sc, s7_pointer args)  {return(aci(sc, args, "acb_si_asymp", acb_hypgeom_si_asymp));}
static s7_pointer a_si_1f2(s7_scheme *sc, s7_pointer args)    {return(aci(sc, args, "acb_si_1f2", acb_hypgeom_si_1f2));}
static s7_pointer a_ci_asymp(s7_scheme *sc, s7_pointer args)  {return(aci(sc, args, "acb_ci_asymp", acb_hypgeom_ci_asymp));}
static s7_pointer a_ci_2f3(s7_scheme *sc, s7_pointer args)    {return(aci(sc, args, "acb_ci_2f3", acb_hypgeom_ci_2f3));}
static s7_pointer a_chi_asymp(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_chi_asymp", acb_hypgeom_chi_asymp));}
static s7_pointer a_chi_2f3(s7_scheme *sc, s7_pointer args)   {return(aci(sc, args, "acb_chi_2f3", acb_hypgeom_chi_2f3));}
static s7_pointer a_dilog_bernoulli(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_dilog_bernoulli", acb_hypgeom_dilog_bernoulli));}
static s7_pointer a_dilog_zero_taylor(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_dilog_zero_taylor", acb_hypgeom_dilog_zero_taylor));}
static s7_pointer a_dilog_zero(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_dilog_zero", acb_hypgeom_dilog_zero));}
static s7_pointer a_dilog(s7_scheme *sc, s7_pointer args)     {return(aci(sc, args, "acb_dilog", acb_hypgeom_dilog));}

static s7_pointer a_elliptic_k(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_elliptic_k", acb_elliptic_k));}
static s7_pointer a_elliptic_e(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_elliptic_e", acb_elliptic_e));}
static s7_pointer a_elliptic_rc1(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_elliptic_rc1", acb_elliptic_rc1));}
static s7_pointer a_gamma(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_gamma", acb_gamma));}
static s7_pointer a_rgamma(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_rgamma", acb_rgamma));}
static s7_pointer a_lgamma(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_lgamma", acb_lgamma));}
static s7_pointer a_log_sin_pi(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_log_sin_pi", acb_log_sin_pi));}
static s7_pointer a_digamma(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_digamma", acb_digamma));}
static s7_pointer a_zeta(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_zeta", acb_zeta));}
static s7_pointer a_log_barnes_g(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_log_barnes_g", acb_log_barnes_g));}
static s7_pointer a_barnes_g(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_barnes_g", acb_barnes_g));}
static s7_pointer a_sinc(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_sinc", acb_sinc));}
static s7_pointer a_sinc_pi(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_sinc_pi", acb_sinc_pi));}
static s7_pointer a_agm1(s7_scheme *sc, s7_pointer args) {return(aci(sc, args, "acb_agm1", acb_agm1));}


/* -------------------------------- acci -------------------------------- */
static s7_pointer acci(s7_scheme *sc, s7_pointer args, const char *b_name, void acb_func(acb_t res, const acb_t nu, const acb_t z, slong prec))
{
  s7_pointer n, x;
  slong prec;

  prec = s7_integer(s7_let_field_ref(sc, s7_make_symbol(sc, "bignum-precision")));

  n = s7_car(args);
  if (!s7_is_number(n))
    return(s7_wrong_type_arg_error(sc, b_name, 1, n, "number"));
  s7_number_to_acb(sc, nu, n, prec);

  x = s7_cadr(args);
  if (!s7_is_number(x))
    return(s7_wrong_type_arg_error(sc, b_name, 2, x, "number"));
  s7_number_to_acb(sc, z, x, prec);

  acb_func(w, nu, z, prec);
  return(acb_to_s7(sc, w, prec));
}
			
static s7_pointer a_bessel_j(s7_scheme *sc, s7_pointer args) {return(acci(sc, args, "acb_bessel_j", acb_hypgeom_bessel_j));}
static s7_pointer a_bessel_y(s7_scheme *sc, s7_pointer args) {return(acci(sc, args, "acb_bessel_y", acb_hypgeom_bessel_y));}
static s7_pointer a_bessel_i(s7_scheme *sc, s7_pointer args) {return(acci(sc, args, "acb_bessel_i", acb_hypgeom_bessel_i));}
static s7_pointer a_bessel_k(s7_scheme *sc, s7_pointer args) {return(acci(sc, args, "acb_bessel_k", acb_hypgeom_bessel_k));}

static s7_pointer a_hermite_h(s7_scheme *sc, s7_pointer args)       {return(acci(sc, args, "acb_hermite_h",       acb_hypgeom_hermite_h));}
static s7_pointer a_chebyshev_t(s7_scheme *sc, s7_pointer args)     {return(acci(sc, args, "acb_chebyshev_t",     acb_hypgeom_chebyshev_t));}
static s7_pointer a_chebyshev_u(s7_scheme *sc, s7_pointer args)     {return(acci(sc, args, "acb_chebyshev_u",     acb_hypgeom_chebyshev_u));}
static s7_pointer a_bessel_j_0f1(s7_scheme *sc, s7_pointer args)    {return(acci(sc, args, "acb_bessel_j_0f1",    acb_hypgeom_bessel_j_0f1));}
static s7_pointer a_bessel_j_asymp(s7_scheme *sc, s7_pointer args)  {return(acci(sc, args, "acb_bessel_j_asymp",  acb_hypgeom_bessel_j_asymp));}
static s7_pointer a_bessel_i_scaled(s7_scheme *sc, s7_pointer args) {return(acci(sc, args, "acb_bessel_i_scaled", acb_hypgeom_bessel_i_scaled));}
static s7_pointer a_bessel_k_scaled(s7_scheme *sc, s7_pointer args) {return(acci(sc, args, "acb_bessel_k_scaled", acb_hypgeom_bessel_k_scaled));}
static s7_pointer a_expint(s7_scheme *sc, s7_pointer args)          {return(acci(sc, args, "acb_expint",          acb_hypgeom_expint));}
static s7_pointer a_elliptic_pi(s7_scheme *sc, s7_pointer args)     {return(acci(sc, args, "acb_elliptic_pi",     acb_elliptic_pi));}
static s7_pointer a_elliptic_p(s7_scheme *sc, s7_pointer args)      {return(acci(sc, args, "acb_elliptic_p",      acb_elliptic_p));}
static s7_pointer a_elliptic_zeta(s7_scheme *sc, s7_pointer args)   {return(acci(sc, args, "acb_elliptic_zeta",   acb_elliptic_zeta));}
static s7_pointer a_elliptic_sigma(s7_scheme *sc, s7_pointer args)  {return(acci(sc, args, "acb_elliptic_sigma",  acb_elliptic_sigma));}
static s7_pointer a_elliptic_inv_p(s7_scheme *sc, s7_pointer args)  {return(acci(sc, args, "acb_elliptic_inv_p",  acb_elliptic_inv_p));}
static s7_pointer a_hurwitz_zeta(s7_scheme *sc, s7_pointer args)    {return(acci(sc, args, "acb_hurwitz_zeta",    acb_hurwitz_zeta));}
static s7_pointer a_polygamma(s7_scheme *sc, s7_pointer args)       {return(acci(sc, args, "acb_polygamma",       acb_polygamma));}
static s7_pointer a_polylog(s7_scheme *sc, s7_pointer args)         {return(acci(sc, args, "acb_polylog",         acb_polylog));}
static s7_pointer a_agm(s7_scheme *sc, s7_pointer args)             {return(acci(sc, args, "acb_agm",             acb_agm));}
static s7_pointer a_hypgeom_dilog_continuation(s7_scheme *sc, s7_pointer args) {return(acci(sc, args, "acb_hypgeom_dilog_continuation", acb_hypgeom_dilog_continuation));}



/* -------------------------------- libarb_s7_init -------------------------------- */
/* TODO: signatures */

void libarb_s7_init(s7_scheme *sc);
void libarb_s7_init(s7_scheme *sc)
{
  s7_pointer old_shadow, arb, old_curlet;

  acb_init(nu);
  acb_init(w);
  acb_init(z);

  s7_define_constant(sc, "*arb*", arb = s7_inlet(sc, s7_nil(sc)));
  old_curlet = s7_set_curlet(sc, arb);
  old_shadow = s7_set_shadow_rootlet(sc, arb);

  s7_define_function(sc, "acb_erf",             a_erf,             1, 0, false, "(acb_erf x)");
  s7_define_function(sc, "acb_erfc",            a_erfc,            1, 0, false, "(acb_erfc x)");
  s7_define_function(sc, "acb_erfi",            a_erfi,            1, 0, false, "(acb_erfi x)");
  s7_define_function(sc, "acb_ei",              a_ei,              1, 0, false, "(acb_ei x)");
  s7_define_function(sc, "acb_si",              a_si,              1, 0, false, "(acb_si x)");
  s7_define_function(sc, "acb_ci",              a_ci,              1, 0, false, "(acb_ci x)");
  s7_define_function(sc, "acb_shi",             a_shi,             1, 0, false, "(acb_shi x)");
  s7_define_function(sc, "acb_chi",             a_chi,             1, 0, false, "(acb_chi x)");
  s7_define_function(sc, "acb_erf_1f1a",        a_erf_1f1a,        1, 0, false, "(acb_erf_1f1a x)");
  s7_define_function(sc, "acb_erf_1f1b",        a_erf_1f1b,        1, 0, false, "(acb_erf_1f1b x)");
  s7_define_function(sc, "acb_ei_asymp",        a_ei_asymp,        1, 0, false, "(acb_ei_asymp x)");
  s7_define_function(sc, "acb_ei_2f2",          a_ei_2f2,          1, 0, false, "(acb_ei_2f2 x)");
  s7_define_function(sc, "acb_si_asymp",        a_si_asymp,        1, 0, false, "(acb_si_asymp x)");
  s7_define_function(sc, "acb_si_1f2",          a_si_1f2,          1, 0, false, "(acb_si_1f2 x)");
  s7_define_function(sc, "acb_ci_asymp",        a_ci_asymp,        1, 0, false, "(acb_ci_asymp x)");
  s7_define_function(sc, "acb_ci_2f3",          a_ci_2f3,          1, 0, false, "(acb_ci_2f3 x)");
  s7_define_function(sc, "acb_chi_asymp",       a_chi_asymp,       1, 0, false, "(acb_chi_asymp x)");
  s7_define_function(sc, "acb_chi_2f3",         a_chi_2f3,         1, 0, false, "(acb_chi_2f3 x)");
  s7_define_function(sc, "acb_dilog_bernoulli", a_dilog_bernoulli, 1, 0, false, "(acb_dilog_bernoulli x)");
  s7_define_function(sc, "acb_dilog_zero_taylor", a_dilog_zero_taylor, 1, 0, false, "(acb_dilog_zero_taylor x)");
  s7_define_function(sc, "acb_dilog_zero",      a_dilog_zero,      1, 0, false, "(acb_dilog_zero x)");
  s7_define_function(sc, "acb_dilog",           a_dilog,           1, 0, false, "(acb_dilog x)");
  s7_define_function(sc, "acb_elliptic_k",      a_elliptic_k,      1, 0, false, "(acb_elliptic_k x)");
  s7_define_function(sc, "acb_elliptic_e",      a_elliptic_e,      1, 0, false, "(acb_elliptic_e x)");
  s7_define_function(sc, "acb_elliptic_rc1",    a_elliptic_rc1,    1, 0, false, "(acb_elliptic_rc1 x)");
  s7_define_function(sc, "acb_gamma",           a_gamma,           1, 0, false, "(acb_gamma x)");
  s7_define_function(sc, "acb_rgamma",          a_rgamma,          1, 0, false, "(acb_rgamma x)");
  s7_define_function(sc, "acb_lgamma",          a_lgamma,          1, 0, false, "(acb_lgamma x)");
  s7_define_function(sc, "acb_log_sin_pi",      a_log_sin_pi,      1, 0, false, "(acb_log_sin_pi x)");
  s7_define_function(sc, "acb_digamma",         a_digamma,         1, 0, false, "(acb_digamma x)");
  s7_define_function(sc, "acb_zeta",            a_zeta,            1, 0, false, "(acb_zeta x)");
  s7_define_function(sc, "acb_log_barnes_g",    a_log_barnes_g,    1, 0, false, "(acb_log_barnes_g x)");
  s7_define_function(sc, "acb_barnes_g",        a_barnes_g,        1, 0, false, "(acb_barnes_g x)");
  s7_define_function(sc, "acb_sinc",            a_sinc,            1, 0, false, "(acb_sinc x)");
  s7_define_function(sc, "acb_sinc_pi",         a_sinc_pi,         1, 0, false, "(acb_sinc_pi x)");
  s7_define_function(sc, "acb_agm1",            a_agm1,            1, 0, false, "(acb_agm1 x)");


  s7_define_function(sc, "acb_bessel_j",        a_bessel_j,        2, 0, false, "(acb_bessel_j n x) returns Jn(x)");
  s7_define_function(sc, "acb_bessel_y",        a_bessel_y,        2, 0, false, "(acb_bessel_y n x) returns Yn(x)");
  s7_define_function(sc, "acb_bessel_i",        a_bessel_i,        2, 0, false, "(acb_bessel_i n x) returns In(x)");
  s7_define_function(sc, "acb_bessel_k",        a_bessel_k,        2, 0, false, "(acb_bessel_k n x) returns Kn(x)");
  s7_define_function(sc, "acb_hermite_h",       a_hermite_h,       2, 0, false, "(acb_hermite_h n x)");
  s7_define_function(sc, "acb_chebyshev_t",     a_chebyshev_t,     2, 0, false, "(acb_chebyshev_t n x)");
  s7_define_function(sc, "acb_chebyshev_u",     a_chebyshev_u,     2, 0, false, "(acb_chebyshev_u n x)");
  s7_define_function(sc, "acb_bessel_j_0f1",    a_bessel_j_0f1,    2, 0, false, "(acb_bessel_j_0f1 n x)");
  s7_define_function(sc, "acb_bessel_j_asymp",  a_bessel_j_asymp,  2, 0, false, "(acb_bessel_j_asymp n x)");
  s7_define_function(sc, "acb_bessel_i_scaled", a_bessel_i_scaled, 2, 0, false, "(acb_bessel_i_scaled n x)");
  s7_define_function(sc, "acb_bessel_k_scaled", a_bessel_k_scaled, 2, 0, false, "(acb_bessel_k_scaled n x)");
  s7_define_function(sc, "acb_expint",          a_expint,          2, 0, false, "(acb_expint n x)");
  s7_define_function(sc, "acb_elliptic_pi",     a_elliptic_pi,     2, 0, false, "(acb_elliptic_pi)");
  s7_define_function(sc, "acb_elliptic_p",      a_elliptic_p,      2, 0, false, "(acb_elliptic_p)");
  s7_define_function(sc, "acb_elliptic_zeta",   a_elliptic_zeta,   2, 0, false, "(acb_elliptic_zeta)");
  s7_define_function(sc, "acb_elliptic_sigma",  a_elliptic_sigma,  2, 0, false, "(acb_elliptic_sigma)");
  s7_define_function(sc, "acb_elliptic_inv_p",  a_elliptic_inv_p,  2, 0, false, "(acb_elliptic_inv_p)");
  s7_define_function(sc, "acb_hurwitz_zeta",    a_hurwitz_zeta,    2, 0, false, "(acb_hurwitz_zeta)");
  s7_define_function(sc, "acb_polygamma",       a_polygamma,       2, 0, false, "(acb_polygamma)");
  s7_define_function(sc, "acb_polylog",         a_polylog,         2, 0, false, "(acb_polylog)");
  s7_define_function(sc, "acb_agm",             a_agm,             2, 0, false, "(acb_agm)");
  s7_define_function(sc, "acb_hypgeom_dilog_continuation", a_hypgeom_dilog_continuation, 2, 0, false, "(acb_hypgeom_dilog_continuation)");



  s7_set_curlet(sc, old_curlet);
  s7_set_shadow_rootlet(sc, old_shadow);
}

/* gcc -fPIC -c libarb_s7.c
 * gcc libarb_s7.o -shared -o libarb_s7.so -lflint -larb
 * repl
 *   > (load "libarb_s7.so" (inlet 'init_func 'libarb_s7_init))
 *   > (arb_bessel_j 0 1.0)
 *   7.651976865579665514497175261026632209096E-1
 */


#if 0
aci:

acci:

acii:
void acb_elliptic_k_jet(acb_ptr w, const acb_t m, slong len, slong prec);
void acb_hypgeom_li(acb_t res, const acb_t z, int offset, slong prec);
void acb_hypgeom_dilog_transform(acb_t res, const acb_t z, int algorithm, slong prec);

poly_t:
void acb_hypgeom_erf_series(acb_poly_t g, const acb_poly_t h, slong len, slong prec);
void acb_hypgeom_erfc_series(acb_poly_t g, const acb_poly_t h, slong len, slong prec);
void acb_hypgeom_erfi_series(acb_poly_t g, const acb_poly_t h, slong len, slong prec);
void acb_hypgeom_ei_series(acb_poly_t g, const acb_poly_t h, slong len, slong prec);
void acb_hypgeom_si_series(acb_poly_t g, const acb_poly_t h, slong len, slong prec);
void acb_hypgeom_ci_series(acb_poly_t g, const acb_poly_t h, slong len, slong prec);
void acb_hypgeom_shi_series(acb_poly_t g, const acb_poly_t h, slong len, slong prec);
void acb_hypgeom_chi_series(acb_poly_t g, const acb_poly_t h, slong len, slong prec);
void acb_elliptic_k_series(acb_poly_t res, const acb_poly_t m, slong len, slong prec);


accii:
void acb_hypgeom_bessel_i_0f1(acb_t res, const acb_t nu, const acb_t z, int scaled, slong prec);
void acb_hypgeom_bessel_i_asymp(acb_t res, const acb_t nu, const acb_t z, int scaled, slong prec);
void acb_hypgeom_bessel_k_0f1(acb_t res, const acb_t nu, const acb_t z, int scaled, slong prec);
void acb_hypgeom_bessel_k_asymp(acb_t res, const acb_t nu, const acb_t z, int scaled, slong prec);
void acb_elliptic_f(acb_t res, const acb_t phi, const acb_t m, int times_pi, slong prec);
void acb_hypgeom_0f1_asymp(acb_t res, const acb_t a, const acb_t z, int regularized, slong prec);
void acb_hypgeom_0f1_direct(acb_t res, const acb_t a, const acb_t z, int regularized, slong prec);
void acb_hypgeom_0f1(acb_t res, const acb_t a, const acb_t z, int regularized, slong prec);
void acb_hypgeom_gamma_lower(acb_t res, const acb_t s, const acb_t z, int modified, slong prec);
void acb_hypgeom_gamma_upper_asymp(acb_t res, const acb_t s, const acb_t z, int modified, slong prec);
void acb_hypgeom_gamma_upper_1f1a(acb_t res, const acb_t s, const acb_t z, int modified, slong prec);
void acb_hypgeom_gamma_upper_1f1b(acb_t res, const acb_t s, const acb_t z, int modified, slong prec);
void acb_hypgeom_gamma_upper(acb_t res, const acb_t s, const acb_t z, int modified, slong prec);
void acb_elliptic_e_inc(acb_t res, const acb_t phi, const acb_t m, int times_pi, slong prec);
void acb_elliptic_p_jet(acb_ptr r, const acb_t z, const acb_t tau, slong len, slong prec);

poly_t:
void acb_elliptic_p_series(acb_poly_t res, const acb_poly_t z, const acb_t tau, slong len, slong prec);

accci:
void acb_hypgeom_u_1f1(acb_t res, const acb_t a, const acb_t b, const acb_t z, slong prec);
void acb_hypgeom_u(acb_t res, const acb_t a, const acb_t b, const acb_t z, slong prec);
void acb_hypgeom_gegenbauer_c(acb_t res, const acb_t n, const acb_t m, const acb_t z, slong prec);
void acb_hypgeom_laguerre_l(acb_t res, const acb_t n, const acb_t m, const acb_t z, slong prec);

acccii:
void acb_hypgeom_m_asymp(acb_t res, const acb_t a, const acb_t b, const acb_t z, int regularized, slong prec);
void acb_hypgeom_m_1f1(acb_t res, const acb_t a, const acb_t b, const acb_t z, int regularized, slong prec);
void acb_hypgeom_m(acb_t res, const acb_t a, const acb_t b, const acb_t z, int regularized, slong prec);
void acb_hypgeom_1f1(acb_t res, const acb_t a, const acb_t b, const acb_t z, int regularized, slong prec);
void acb_hypgeom_u_asymp(acb_t res, const acb_t a, const acb_t b, const acb_t z, slong n, slong prec);
void acb_hypgeom_u_1f1_series(acb_poly_t res, const acb_poly_t a, const acb_poly_t b, const acb_poly_t z, slong len, slong prec);
void acb_hypgeom_beta_lower(acb_t res, const acb_t a, const acb_t b, const acb_t z, int regularized, slong prec);
void acb_hypgeom_legendre_p(acb_t res, const acb_t n, const acb_t m, const acb_t z, int type, slong prec);
void acb_hypgeom_legendre_q(acb_t res, const acb_t n, const acb_t m, const acb_t z, int type, slong prec);
void acb_elliptic_rf(acb_t res, const acb_t x, const acb_t y, const acb_t z, int flags, slong prec);
void acb_elliptic_rg(acb_t res, const acb_t x, const acb_t y, const acb_t z, int flags, slong prec);
void acb_elliptic_pi_inc(acb_t res, const acb_t n, const acb_t phi, const acb_t m, int times_pi, slong prec);


void acb_hypgeom_pfq_bound_factor(mag_t C, acb_srcptr a, slong p, acb_srcptr b, slong q, const acb_t z, ulong n);
slong acb_hypgeom_pfq_choose_n(acb_srcptr a, slong p, acb_srcptr b, slong q, const acb_t z, slong prec);
void acb_hypgeom_pfq_sum_forward(acb_t s, acb_t t, acb_srcptr a, slong p, acb_srcptr b, slong q, const acb_t z, slong n, slong prec);
void acb_hypgeom_pfq_sum_rs(acb_t s, acb_t t, acb_srcptr a, slong p, acb_srcptr b, slong q, const acb_t z, slong n, slong prec);
void acb_hypgeom_pfq_sum_bs(acb_t s, acb_t t, acb_srcptr a, slong p, acb_srcptr b, slong q, const acb_t z, slong n, slong prec);
void acb_hypgeom_pfq_sum_fme(acb_t s, acb_t t, acb_srcptr a, slong p, acb_srcptr b, slong q, const acb_t z, slong n, slong prec);
void acb_hypgeom_pfq_sum(acb_t s, acb_t t, acb_srcptr a, slong p, acb_srcptr b, slong q, const acb_t z, slong n, slong prec);
void acb_hypgeom_pfq_sum_bs_invz(acb_t s, acb_t t, acb_srcptr a, slong p, acb_srcptr b, slong q, const acb_t z, slong n, slong prec);
void acb_hypgeom_pfq_sum_invz(acb_t s, acb_t t, acb_srcptr a, slong p, acb_srcptr b, slong q, const acb_t z, const acb_t zinv, slong n, slong prec);
void acb_hypgeom_pfq_direct(acb_t res, acb_srcptr a, slong p, acb_srcptr b, slong q, const acb_t z, slong n, slong prec);
slong acb_hypgeom_pfq_series_choose_n(const acb_poly_struct * a, slong p, const acb_poly_struct * b, slong q, const acb_poly_t z, slong len, slong prec);

void acb_hypgeom_pfq_series_sum_forward(acb_poly_t s, acb_poly_t t, const acb_poly_struct * a, slong p, const acb_poly_struct * b, slong q,
    const acb_poly_t z, int regularized, slong n, slong len, slong prec);

void acb_hypgeom_pfq_series_sum_bs(acb_poly_t s, acb_poly_t t, const acb_poly_struct * a, slong p, const acb_poly_struct * b, slong q,
    const acb_poly_t z, int regularized, slong n, slong len, slong prec);

void acb_hypgeom_pfq_series_sum_rs(acb_poly_t s, acb_poly_t t, const acb_poly_struct * a, slong p, const acb_poly_struct * b, slong q,
    const acb_poly_t z, int regularized, slong n, slong len, slong prec);

void acb_hypgeom_pfq_series_sum(acb_poly_t s, acb_poly_t t, const acb_poly_struct * a, slong p, const acb_poly_struct * b, slong q,
    const acb_poly_t z, int regularized, slong n, slong len, slong prec);

void acb_hypgeom_pfq_series_direct(acb_poly_t res, const acb_poly_struct * a, slong p, const acb_poly_struct * b, slong q,
    const acb_poly_t z, int regularized, slong n, slong len, slong prec);

void acb_hypgeom_pfq(acb_t res, acb_srcptr a, slong p, acb_srcptr b, slong q, const acb_t z, int regularized, slong prec);
int acb_hypgeom_u_use_asymp(const acb_t z, slong prec);

void acb_hypgeom_bessel_k_0f1_series(acb_poly_t res, const acb_poly_t n, const acb_poly_t z, int scaled, slong len, slong prec);
void acb_hypgeom_bessel_jy(acb_t res1, acb_t res2, const acb_t nu, const acb_t z, slong prec);

void acb_hypgeom_airy_bound(mag_t ai, mag_t aip, mag_t bi, mag_t bip, const acb_t z);
void acb_hypgeom_airy_asymp(acb_t ai, acb_t aip, acb_t bi, acb_t bip, const acb_t z, slong n, slong prec);
void acb_hypgeom_airy_direct(acb_t ai, acb_t aip, acb_t bi, acb_t bip, const acb_t z, slong n, slong prec);
void acb_hypgeom_airy(acb_t ai, acb_t aip, acb_t bi, acb_t bip, const acb_t z, slong prec);
void acb_hypgeom_airy_jet(acb_ptr ai, acb_ptr bi, const acb_t z, slong len, slong prec);
void acb_hypgeom_airy_series(acb_poly_t ai, acb_poly_t ai_prime, acb_poly_t bi, acb_poly_t bi_prime, const acb_poly_t z, slong len, slong prec);

void acb_hypgeom_coulomb(acb_t F, acb_t G, acb_t Hpos, acb_t Hneg, const acb_t l, const acb_t eta, const acb_t z, slong prec);
void acb_hypgeom_coulomb_jet(acb_ptr F, acb_ptr G, acb_ptr Hpos, acb_ptr Hneg, const acb_t l, const acb_t eta, const acb_t z, slong len, slong prec);
void acb_hypgeom_coulomb_series(acb_poly_t F, acb_poly_t G, acb_poly_t Hpos, acb_poly_t Hneg, const acb_t l, const acb_t eta, const acb_poly_t z, slong len, slong prec);

void acb_hypgeom_gamma_upper_singular(acb_t res, slong s, const acb_t z, int modified, slong prec);
void acb_hypgeom_gamma_upper_series(acb_poly_t g, const acb_t s, const acb_poly_t h, int regularized, slong n, slong prec);
void acb_hypgeom_gamma_lower_series(acb_poly_t g, const acb_t s, const acb_poly_t h, int regularized, slong n, slong prec);
void acb_hypgeom_beta_lower_series(acb_poly_t res, const acb_t a, const acb_t b, const acb_poly_t z, int regularized, slong len, slong prec);

void acb_hypgeom_erf_propagated_error(mag_t re, mag_t im, const acb_t z);
void acb_hypgeom_erf_asymp(acb_t res, const acb_t z, int complementary, slong prec, slong prec2);

void acb_hypgeom_fresnel(acb_t res1, acb_t res2, const acb_t z, int normalized, slong prec);
void acb_hypgeom_fresnel_series(acb_poly_t s, acb_poly_t c, const acb_poly_t h, int normalized, slong len, slong prec);

void acb_hypgeom_li_series(acb_poly_t g, const acb_poly_t h, int offset, slong len, slong prec);
void acb_hypgeom_dilog_bitburst(acb_t res, acb_t z0, const acb_t z, slong prec);

void acb_hypgeom_2f1_continuation(acb_t res0, acb_t res1, const acb_t a, const acb_t b, const acb_t c, const acb_t z0, const acb_t z1, const acb_t f0, const acb_t f1, slong prec);
void acb_hypgeom_2f1_series_direct(acb_poly_t res, const acb_poly_t a, const acb_poly_t b, const acb_poly_t c, const acb_poly_t z, int regularized, slong len, slong prec);
void acb_hypgeom_2f1_direct(acb_t res, const acb_t a, const acb_t b, const acb_t c, const acb_t z, int regularized, slong prec);

void acb_hypgeom_2f1_transform(acb_t res, const acb_t a, const acb_t b, const acb_t c, const acb_t z, int regularized, int which, slong prec);
void acb_hypgeom_2f1_transform_limit(acb_t res, const acb_t a, const acb_t b, const acb_t c, const acb_t z, int regularized, int which, slong prec);
void acb_hypgeom_2f1_corner(acb_t res, const acb_t a, const acb_t b, const acb_t c, const acb_t z, int regularized, slong prec);
int acb_hypgeom_2f1_choose(const acb_t z);
void acb_hypgeom_2f1(acb_t res, const acb_t a, const acb_t b, const acb_t c, const acb_t z, int regularized, slong prec);

#define ACB_HYPGEOM_2F1_REGULARIZED 1
#define ACB_HYPGEOM_2F1_AB 2   /* a-b integer */
#define ACB_HYPGEOM_2F1_AC 4   /* a-c integer */
#define ACB_HYPGEOM_2F1_BC 8   /* b-c integer */
#define ACB_HYPGEOM_2F1_ABC 16  /* a+b-c integer */

void acb_hypgeom_legendre_p_uiui_rec(acb_t res, ulong n, ulong m, const acb_t z, slong prec);
void acb_hypgeom_jacobi_p(acb_t res, const acb_t n, const acb_t a, const acb_t b, const acb_t z, slong prec);

void acb_hypgeom_spherical_y(acb_t res, slong n, slong m, const acb_t theta, const acb_t phi, slong prec);

elliptic
void _acb_elliptic_k_series(acb_ptr res, acb_srcptr m, slong zlen, slong len, slong prec);
void acb_elliptic_rj(acb_t res, const acb_t x, const acb_t y, const acb_t z, const acb_t p, int flags, slong prec);
void acb_elliptic_rj_carlson(acb_t res, const acb_t x, const acb_t y, const acb_t z, const acb_t p, int flags, slong prec);
void acb_elliptic_rj_integration(acb_t res, const acb_t x, const acb_t y, const acb_t z, const acb_t p, int flags, slong prec);
void _acb_elliptic_p_series(acb_ptr res, acb_srcptr z, slong zlen, const acb_t tau, slong len, slong prec);
void acb_elliptic_roots(acb_t e1, acb_t e2, acb_t e3, const acb_t tau, slong prec);
void acb_elliptic_invariants(acb_t g2, acb_t g3, const acb_t tau, slong prec);

acb.h

void acb_root_ui(acb_t y, const acb_t x, ulong k, slong prec);
void acb_quadratic_roots_fmpz(acb_t r1, acb_t r2, const fmpz_t a, const fmpz_t b, const fmpz_t c, slong prec);
void acb_chebyshev_t_ui(acb_t a, ulong n, const acb_t x, slong prec);
void acb_chebyshev_t2_ui(acb_t a, acb_t b, ulong n, const acb_t x, slong prec);
void acb_chebyshev_u_ui(acb_t a, ulong n, const acb_t x, slong prec);
void acb_chebyshev_u2_ui(acb_t a, acb_t b, ulong n, const acb_t x, slong prec);

void acb_rising_ui_bs(acb_t y, const acb_t x, ulong n, slong prec);
void acb_rising(acb_t z, const acb_t x, const acb_t n, slong prec);
void acb_rising_ui_rs(acb_t y, const acb_t x, ulong n, ulong m, slong prec);
void acb_rising_ui_rec(acb_t y, const acb_t x, ulong n, slong prec);
void acb_rising_ui(acb_t z, const acb_t x, ulong n, slong prec);
void acb_rising2_ui_bs(acb_t u, acb_t v, const acb_t x, ulong n, slong prec);
void acb_rising2_ui_rs(acb_t u, acb_t v, const acb_t x, ulong n, ulong m, slong prec);
void acb_rising2_ui(acb_t u, acb_t v, const acb_t x, ulong n, slong prec);
void acb_rising_ui_get_mag(mag_t bound, const acb_t s, ulong n);

void acb_bernoulli_poly_ui(acb_t res, ulong n, const acb_t x, slong prec);
void acb_polylog_si(acb_t w, slong s, const acb_t z, slong prec);
void acb_agm1_cpx(acb_ptr m, const acb_t z, slong len, slong prec);

#define ACB_LAMBERTW_LEFT 2
#define ACB_LAMBERTW_MIDDLE 4

void acb_lambertw_asymp(acb_t res, const acb_t z, const fmpz_t k, slong L, slong M, slong prec);
int acb_lambertw_check_branch(const acb_t w, const fmpz_t k, slong prec);
void acb_lambertw_bound_deriv(mag_t res, const acb_t z, const acb_t ez1, const fmpz_t k);
void acb_lambertw(acb_t res, const acb_t z, const fmpz_t k, int flags, slong prec);

/* 
<acb_dft.h>
acb_mat.h
acb_dirichlet.h  
acb_modular.h
<acb_poly.h>
[acb.h]
*/

#endif


/* 3-D SSR migration/modeling using extended split-step */
/*
  Copyright (C) 2004 University of Texas at Austin
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#include <math.h>

#include <rsf.h>
/*^*/

#include "zomig.h"
#include "fft2.h"
#include "taper.h"
#include "slowref.h"
#include "ssr.h"

#include "slice.h"
/*^*/

#define LOOP(a) for(imy=0;imy<amy.n;imy++){ for(imx=0;imx<amx.n;imx++){ {a} }}
#define SOOP(a) for(ily=0;ily<aly.n;ily++){ for(ilx=0;ilx<alx.n;ilx++){ {a} }}

static axa az,aw,alx,aly,amx,amy,ae;
static bool verb;
static float eps;

static float         **qq; /* image */
static float complex **wx; /* wavefield x */
static float         **sm; /* reference slowness squared */
static float         **ss; /* slowness */
static float         **so; /* slowness */
static int            *nr; /* number of references */
static fslice        slow; /* slowness slice */

/*------------------------------------------------------------*/

void zomig_init(bool verb_,
		float eps_,
		float  dt,
		axa az_          /* depth */,
		axa aw_          /* frequency */,
		axa ae_          /* experiment */,
		axa amx_         /* i-line (data/image) */,
		axa amy_         /* x-line (data/image) */,
		axa alx_         /* i-line (slowness) */,
		axa aly_         /* x-line (slowness) */,
		int tmx, int tmy /* taper size */,
		int pmx, int pmy /* padding in the k domain */,
		int nrmax        /* maximum number of references */,
		fslice slow_)
/*< initialize >*/
{
    int   ilx, ily, iz, jj;
    float ds;

    verb=verb_;
    eps = eps_;

    az = az_;
    aw = aw_;
    ae = ae_;
    amx= amx_;
    amy= amy_;
    alx= alx_;
    aly= aly_;

    /* from hertz to radian */
    aw.d *= 2.*SF_PI; 
    aw.o *= 2.*SF_PI;

    ds  = dt/az.d;

    /* SSR */
    ssr_init(az_ ,
	     amx_,amy_,
	     alx_,aly_,
	     pmx ,pmy,
	     tmx ,tmy,
	     ds);

    /* precompute taper */
    taper2_init(amy.n,
		amx.n,
		SF_MIN(tmy,amy.n-1),
		SF_MIN(tmx,amx.n-1), 
		true,
		true);
    
    /* compute reference slowness */
    ss = sf_floatalloc2(alx.n,aly.n); /* slowness */
    so = sf_floatalloc2(alx.n,aly.n); /* slowness */
    sm = sf_floatalloc2 (nrmax,az.n); /* ref slowness squared*/
    nr = sf_intalloc          (az.n); /* nr of ref slownesses */
    slow = slow_;
    for (iz=0; iz<az.n; iz++) {
	fslice_get(slow,iz,ss[0]);
	SOOP( ss[ily][ilx] *=2; ); /* 2-way time */

	nr[iz] = slowref(nrmax,ds,alx.n*aly.n,ss[0],sm[iz]);
	if (verb) sf_warning("nr[%d]=%d",iz,nr[iz]);
    }
    for (iz=0; iz<az.n-1; iz++) {
	for (jj=0; jj<nr[iz]; jj++) {
	    sm[iz][jj] = 0.5*(sm[iz][jj]+sm[iz+1][jj]);
	}
    }

    /* allocate wavefield storage */
    wx = sf_complexalloc2(amx.n,amy.n);
}

/*------------------------------------------------------------*/

void zomig_close(void)
/*< free allocated storage >*/
{
    ssr_close();

    free( *wx); free( wx);
    free( *ss); free( ss);
    free( *so); free( so);
    free( *sm); free( sm);
    ;           free( nr);
}

/*------------------------------------------------------------*/

void zomig_aloc()
/*< allocate migration storage >*/
{
    qq = sf_floatalloc2(amx.n,amy.n);
}

void zomig_free()
/*< free migration storage >*/
{
    free( *qq); free( qq);
}

/*------------------------------------------------------------*/

void zomig(bool inv  /* forward/adjoint flag */, 
	  fslice data /* data  [nw][nmy][nmx] */,
	  fslice imag /* image [nz][nmy][nmx] */)
/*< Apply migration/modeling >*/
{
    int iz,iw,imy,imx,ilx,ily;
    float complex w;

    if (!inv) { /* prepare image for migration */
	LOOP( qq[imy][imx] = 0.0; );
	for (iz=0; iz<az.n; iz++) {
	    fslice_put(imag,iz,qq[0]);
	}
    }
    
    /* loop over frequencies w */
    for (iw=0; iw<aw.n; iw++) {
	if (verb) sf_warning ("iw=%3d of %3d",iw+1,aw.n);

	if (inv) { /* MODELING */
	    w = eps*aw.d + I*(aw.o+iw*aw.d); /* +1 for upward continuation */
	    LOOP( wx[imy][imx] = 0; );  

	    fslice_get(slow,az.n-1,so[0]);
	    SOOP( so[ily][ilx] *=2; ); /* 2-way time */
	    for (iz=az.n-1; iz>0; iz--) {
		fslice_get(imag,iz,qq[0]);
		LOOP( wx[imy][imx] +=
		      qq[imy][imx]; );
		
		/* upward continuation */
		fslice_get(slow,iz-1,ss[0]);
		SOOP( ss[ily][ilx] *=2; ); /* 2-way time */
		ssr_ssf(w,wx,so,ss,nr[iz],sm[iz]);
		SOOP( so[ily][ilx] = ss[ily][ilx]; );
	    }

	    fslice_get(imag,0,qq[0]);      /*     imaging @ iz=0 */
	    LOOP( wx[imy][imx]  += 
		  qq[imy][imx]; );		
	    
	    taper2(wx);
	    fslice_put(data,iw,wx[0]);    /* output data @ iz=0 */
	    
	} else { /* MIGRATION */
	    w = eps*aw.d - I*(aw.o+iw*aw.d); /* -1 for downward continuation */

	    fslice_get(data,iw,wx[0]);    /*  input data @ iz=0 */
	    taper2(wx);

	    fslice_get(imag,0,qq[0]);      /*     imaging @ iz=0 */
	    LOOP(;      qq[imy][imx] += 
		 crealf(wx[imy][imx] ); );
	    fslice_put(imag,0,qq[0]);

	    fslice_get(slow,0,so[0]);	    
	    SOOP( so[ily][ilx] *=2; ); /* 2-way time */
	    for (iz=0; iz<az.n-1; iz++) {

		/* downward continuation */
		fslice_get(slow,iz+1,ss[0]);
		SOOP( ss[ily][ilx] *=2; ); /* 2-way time */
		ssr_ssf(w,wx,so,ss,nr[iz],sm[iz]);
		SOOP( so[ily][ilx] = ss[ily][ilx]; );

		fslice_get(imag,iz+1,qq[0]); /* imaging */
		LOOP(;      qq[imy][imx] += 
		     crealf(wx[imy][imx] ); );
		fslice_put(imag,iz+1,qq[0]);
	    }

	} /* else */
    } /* iw */
}

/*------------------------------------------------------------*/

void zodtm(bool inv    /* forward/adjoint flag */, 
	   fslice data /* data [nw][nmy][nmx] */,
	   fslice wfld /* wfld [nw][nmy][nmx] */)
/*< Apply upward/downward datuming >*/
{
    int iz,iw,ie, ilx,ily;
    float complex w;

    for(ie=0; ie<ae.n; ie++) {
	
	/* loop over frequencies w */
	for (iw=0; iw<aw.n; iw++) {
	    if (verb) sf_warning ("iw=%3d of %3d:   ie=%3d of %3d",iw+1,aw.n,ie+1,ae.n);
	    
	    if (inv) { /* UPWARD DATUMING */
		w = eps*aw.d + I*(aw.o+iw*aw.d);
		
		fslice_get(wfld,iw+ie*aw.n,wx[0]);
		taper2(wx);
		
		fslice_get(slow,az.n-1,so[0]);
		SOOP( so[ily][ilx] *=2; ); /* 2-way time */
		for (iz=az.n-1; iz>0; iz--) {
		    fslice_get(slow,iz-1,ss[0]);
		    SOOP( ss[ily][ilx] *=2; ); /* 2-way time */
		    ssr_ssf(w,wx,so,ss,nr[iz],sm[iz]);
		    SOOP( so[ily][ilx] = ss[ily][ilx]; );
		}
		
		taper2(wx);
		fslice_put(data,iw+ie*aw.n,wx[0]);
	    } else { /* DOWNWARD DATUMING */
		w = eps*aw.d - I*(aw.o+iw*aw.d);
		
		fslice_get(data,iw+ie*aw.n,wx[0]);
		taper2(wx);
		
		fslice_get(slow,0,so[0]);
		SOOP( so[ily][ilx] *=2; ); /* 2-way time */
		for (iz=0; iz<az.n-1; iz++) {
		    fslice_get(slow,iz+1,ss[0]);
		    SOOP( ss[ily][ilx] *=2; ); /* 2-way time */
		    ssr_ssf(w,wx,so,ss,nr[iz],sm[iz]);
		    SOOP( so[ily][ilx] = ss[ily][ilx]; );
		}
		
		taper2(wx);
		fslice_put(wfld,iw+ie*aw.n,wx[0]);
	    } /* else */
	} /* iw */
    } /* ie */
}

/*------------------------------------------------------------*/

void zowfl(fslice data /*      data [nw][nmy][nmx] */,
	   fslice wfld /* wavefield [nw][nmy][nmx] */)
/*< Save wavefield from downward continuation >*/
{
    int iz,iw, ilx,ily;
    float complex w;

    /* loop over frequencies w */
    for (iw=0; iw<aw.n; iw++) {
	if (verb) sf_warning ("iw=%3d of %3d",iw+1,aw.n);
	w = eps*aw.d + I*(aw.o+iw*aw.d);

	fslice_get(data,iw,wx[0]);
	taper2(wx);
	
	taper2(wx);
	fslice_put(wfld,iw*az.n,wx[0]);

	fslice_get(slow,0,so[0]);
	SOOP( so[ily][ilx] *=2; ); /* 2-way time */	
	for (iz=0; iz<az.n-1; iz++) {	    
	    fslice_get(slow,iz+1,ss[0]);
	    SOOP( ss[ily][ilx] *=2; ); /* 2-way time */
	    ssr_ssf(w,wx,so,ss,nr[iz],sm[iz]);
	    SOOP( so[ily][ilx] = ss[ily][ilx]; );

	    taper2(wx);
	    fslice_put(wfld,iw*az.n+iz+1,wx[0]);
	}
    } /* iw */
}

/*------------------------------------------------------------*/


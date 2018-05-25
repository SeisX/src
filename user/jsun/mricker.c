/* Ricker filter */
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

#include "mricker.h"

static kiss_fft_cpx *shape;

void ricker_init(int nfft   /* time samples */, 
		 float freq /* frequency percentage w.r.t 1/dt */,
                 float dt   /* temporal sampling rate */)
/*< initialize >*/
{
    int iw, nw;
    float dw, w, w0;

    /* determine frequency sampling (for real to complex FFT) */
    nw = nfft/2+1;
    /* dw = 1./(nfft*freq); */
    dw = 1./(nfft*dt);
    w0 = freq/dt;
    w0 *= w0;
 
    shape = (kiss_fft_cpx*) sf_complexalloc(nw);

    for (iw=0; iw < nw; iw++) {
	w = iw*dw;
	/* w *= w;
        shape[iw].r = w*expf(1-w)/nfft;
        shape[iw].i = 0.; */

        shape[iw].r = 0.;
        shape[iw].i = -w/w0*expf(1-w*w/w0)/nfft/2;
        /* shape[iw].i = -w*w/w0*expf(1-w*w/w0)/nfft; */
    }

    sf_freqfilt_init(nfft,nw);
    sf_freqfilt_cset(shape);
}

void ricker_close(void) 
/*< free allocated storage >*/
{
    free(shape);
    sf_freqfilt_close();
}

/* 	$Id: ricker.c 5023 2009-11-23 01:19:26Z sfomel $	 */

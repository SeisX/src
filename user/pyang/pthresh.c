/* Generalized p-norm thresholding operator for complex/float numbers
 */
/*
  Copyright (C) 2013  Xi'an Jiaotong University, UT Austin (Pengliang Yang)

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
#include <rsf.h>

#include "pthresh.h"

void sf_cpthresh(sf_complex *x, int n, float thr, float p, char* mode)
/*< p-norm thresholding operator for complex numbers >*/
{
    float a,b;
    int i;

#ifdef _OPENMP
#pragma omp parallel for default(none) private(i,a,b) shared(x,n,thr,p,mode)
#endif
    for(i=0;i<n;i++){
	a=cabsf(x[i]);/* complex numbers */
	b=(a==0.)?1.0:0.0;
	if (strcmp(mode,"hard") == 0) { /* hard thresholding*/
#ifdef SF_HAS_COMPLEX_H
	    x[i]=(x[i])*(a>thr?1.:0.);
#else
	    x[i]=sf_crmul(x[i],(a>thr?1.:0.));
#endif
	} else{
	    if (strcmp(mode,"soft") == 0) a=1.0-thr/(a+b);/* soft thresholding */
	    if (strcmp(mode,"pthresh") == 0) a=1.0-powf((a+b)/thr, p-2.0);
	    /* generalized quasi p-norm thresholding*/
	    if (strcmp(mode,"exp") == 0) a=expf(-powf((a+b)/thr, p-2.0));
	    /* exponential shrinkage */
#ifdef SF_HAS_COMPLEX_H
	    x[i]=(x[i])*(a>0.0?a:0.0);
#else
	    x[i]=sf_crmul(x[i],(a>0.0?a:0.0));
#endif
	}
    }
}

void sf_pthresh(float *x, int n, float thr, float p, char* mode)
/*< p-norm thresholding operator for real numbers >*/
{
    float a,b;
    int i;

#ifdef _OPENMP
#pragma omp parallel for default(none) private(i,a,b) shared(x,n,thr,p,mode)
#endif
    for(i=0;i<n;i++){
	a=fabsf(x[i]);/* float numbers */
	b=(a==0.)?1.0:0.0;
	if (strcmp(mode,"hard") == 0) x[i]=(x[i])*(a>thr?1.:0.);/* hard thresholding*/
	else{
	    if (strcmp(mode,"soft") == 0) a=1.0-thr/(a+b);/* soft thresholding */
	    if (strcmp(mode,"pthresh") == 0) a=1.0-powf((a+b)/thr, p-2.0);
	    /* generalized quasi p-norm thresholding*/
	    if (strcmp(mode,"exp") == 0) a=expf(-powf((a+b)/thr, p-2.0));
	    /* exponential shrinkage */

	    x[i]=(x[i])*(a>0.0?a:0.0);
	}
    }
}

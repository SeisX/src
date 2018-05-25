/* 2-D Two-step POCS interpolation using a general Lp-norm optimization
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

  Reference: On analysis-based two-step interpolation methods for randomly 
  sampled seismic data, P Yang, J Gao, W Chen, Computers & Geosciences
*/
#include <rsf.h>
#include <math.h>

#ifdef _OPENMP
#include <omp.h>
#endif

#include "pthresh.h"

#ifdef SF_HAS_FFTW
#include <fftw3.h>

int main(int argc, char* argv[])
{
    bool verb;
    int niter; 
    float p, tol,pclip;
    char *mode;
    sf_file Fin, Fout, Fmask;/* mask and I/O files*/ 

    /* define temporary variables */
    int n1,n2;
    float *din, *dout, *mask;

    sf_init(argc,argv);	/* Madagascar initialization */
#ifdef _OPENMP
    omp_init(); 	/* initialize OpenMP support */
#endif

    /* setup I/O files */
    Fin=sf_input("in");	/* read the data to be interpolated */
    Fmask=sf_input("mask");  	/* read the 2-D mask for 3-D data */
    Fout=sf_output("out"); 	/* output the reconstructed data */
 
    if(!sf_getbool("verb",&verb))    	verb=false;
    /* verbosity */
    if (!sf_getint("niter",&niter)) 	niter=100;
    /* total number iterations */
    if (!sf_getfloat("tol",&tol)) 	tol=1.0e-6;
    /* iteration tolerance */
    if (!sf_getfloat("pclip",&pclip)) 	pclip=99.;
    /* starting data clip percentile (default is 99)*/
    if (pclip <=0. || pclip > 100.)
	sf_error("pclip=%g should be > 0 and <= 100",pclip);
    if ( !(mode=sf_getstring("mode")) ) mode = "exp";
    /* thresholding mode: 'hard', 'soft','pthresh','exp';
       'hard', hard thresholding;	'soft', soft thresholding; 
       'pthresh', generalized quasi-p; 'exp', exponential shrinkage */
    if (!sf_getfloat("p",&p)) 		p=0.35;
    /* norm=p, where 0<p<=1 */;
    if (strcmp(mode,"soft") == 0) 	p=1;
    else if (strcmp(mode,"hard") == 0) 	p=0;
	

    /* Read the data size */
    if (!sf_histint(Fin,"n1",&n1)) sf_error("No n1= in input");
    if (!sf_histint(Fin,"n2",&n2)) sf_error("No n2= in input");

    /* allocate data and mask arrays */
    din=sf_floatalloc(n1*n2); sf_floatread(din,n1*n2,Fin);
    dout=sf_floatalloc(n1*n2);
    if (NULL != sf_getstring("mask")){
	mask=sf_floatalloc(n2);
	sf_floatread(mask,n2,Fmask);
    } else {
	mask=NULL;
    }
 
    /********************* 2-D POCS interpolation *********************/
    fftwf_plan p1,p2;/* execute plan for FFT and IFFT */
    int i1,i2, iter;
    float t0=1.0,t1,beta,thr;		
    sf_complex *dprev,*dcurr,*dtmp;

    dprev=(fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*n1*n2);
    dcurr=(fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*n1*n2);
    dtmp=(fftwf_complex*)fftwf_malloc(sizeof(fftwf_complex)*n1*n2);
    p1=fftwf_plan_dft_2d(n1,n2,dtmp,dtmp,FFTW_FORWARD,FFTW_MEASURE);	
    p2=fftwf_plan_dft_2d(n1,n2,dtmp,dtmp,FFTW_BACKWARD,FFTW_MEASURE);

    for(i1=0; i1<n1*n2; i1++) {
	dprev[i1]=dcurr[i1]=dtmp[i1]=din[i1];
    }

    /* FPOCS iterations */
    for(iter=0; iter<niter; iter++)
    {
	t1=(1.0+sqrtf(1.0+4.0*t0*t0))/2.0;
	beta=(t0-1.0)/t1;
	t0=t1;

#ifdef _OPENMP
#pragma omp parallel for
#endif
	for(i1=0;i1<n1*n2;i1++) {
	    dtmp[i1]=dcurr[i1]+beta*(dcurr[i1]-dprev[i1]);
	    dprev[i1]=dcurr[i1];
	}	

	fftwf_execute(p1);/* FFT */

	/* perform p-norm thresholding */
#ifdef _OPENMP
#pragma omp parallel for
#endif
	for(i1=0; i1<n1*n2; i1++)    dout[i1]=cabsf(dtmp[i1]);

   	int nthr = 0.5+n1*n2*(0.01*pclip);  /*round off*/
    	if (nthr < 0) nthr=0;
    	if (nthr >= n1*n2) nthr=n1*n2-1;
	thr=sf_quantile(nthr,n1*n2,dout);
	sf_cpthresh(dtmp,n1*n2, thr, p,mode);

	fftwf_execute(p2);/* unnormalized IFFT */

	/* adjointness needs scaling with factor 1.0/(n1*n2) */	
#ifdef _OPENMP
#pragma omp parallel for 
#endif
	for(i1=0; i1<n1*n2;i1++) dcurr[i1]=dtmp[i1]/(n1*n2);
	
	/* update d_rec: d_rec = d_obs+(1-M)*A T{ At(d_rec) } */
	for(i2=0;i2<n2;i2++){
	    if (mask[i2]){			
		for(i1=0; i1<n1; i1++) dcurr[i1+n1*i2]=din[i1+n1*i2];
	    }
	}

	if (verb)    sf_warning("iteration %d;",iter+1);
    }

    /* take the real part */
    for(i1=0;i1<n1*n2; i1++) dout[i1]=crealf(dcurr[i1]);
	
    fftwf_destroy_plan(p1);
    fftwf_destroy_plan(p2);
    fftwf_free(dprev);
    fftwf_free(dcurr);
    fftwf_free(dtmp);

    sf_floatwrite(dout,n1*n2,Fout); /* output reconstructed seismograms */

    exit(0);
}
#endif

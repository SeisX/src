/* Recursive stacking by plane-wave construction. */
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

#include <rsf.h>
#include <rsfpwd.h>

#include "pwinmo.h"
#include "bandpass.h"

int main (int argc, char *argv[])
{
   // bool verb;
    //int n1,n2,n3, i1,i2,i3,order,mode;
    //float eps, **u, **p, *trace;
    //sf_file inp, out, dip;

    bool half, slow, verb, nmo;
    int ih,ix,it,nt,nx,nd,nh, CDPtype, jump, niter, restart, nplo, nphi, ni, axis, lim, j, mode, order;
    off_t n;
    float dt, t0, h0, dh, eps1, eps, dy, tol, flo, fhi, velocity;
    float *trace, *trace2, *off, **gather, **dense, **dense2, **p;
    char key1[7];
    sf_file cmp, stack, offset, dip;

    sf_init (argc,argv);
    cmp = sf_input("in");
    //velocity = sf_input("velocity");
    stack = sf_output("out");
    dip = sf_input("dip");

    axis = 2; 
    lim = axis-1;
  
    n = 1;
    for (j=0; j < lim; j++) {
      sprintf(key1,"n%d",j+1);
	    if (!sf_histint(cmp,key1,&ni)) break;
	    n *= ni;
    }
  
    (void) sf_unshiftdim(cmp,stack,axis);
 
    if (SF_FLOAT != sf_gettype(cmp)) sf_error("Need float input");
    if (!sf_histint(cmp,"n1",&nt)) sf_error("No n1= in input");
    if (!sf_histfloat(cmp,"d1",&dt)) sf_error("No d1= in input");
    if (!sf_histfloat(cmp,"o1",&t0)) sf_error("No o1= in input");

    if (!sf_histint(cmp,"n2",&nh)) sf_error("No n2= in input");

    off = sf_floatalloc(nh);

    if (!sf_getbool("half",&half)) half=true;
    /* if y, the second axis is half-offset instead of full offset */

    CDPtype=1;
    if (NULL != sf_getstring("offset")) {
	offset = sf_input("offset");
	sf_floatread (off,nh,offset);
	sf_fileclose(offset);
    } else {
	if (!sf_histfloat(cmp,"d2",&dh)) sf_error("No d2= in input");
	if (!sf_histfloat(cmp,"o2",&h0)) sf_error("No o2= in input");
	
	if (sf_histfloat(cmp,"d3",&dy)) {
	    CDPtype=half? 0.5+dh/dy : 0.5+0.5*dh/dy;
	    if (CDPtype < 1) {
		CDPtype=1;
	    } else if (1 != CDPtype) {
		sf_histint(cmp,"CDPtype",&CDPtype);
	    	sf_warning("CDPtype=%d",CDPtype);
	    }
	} 

	for (ih = 0; ih < nh; ih++) {
	    off[ih] = h0 + ih*dh; 
	}
    }

    if (!sf_getbool("slowness",&slow)) slow=false;
    /* if y, use slowness instead of velocity */

    nx = sf_leftsize(cmp,2);

    if (!sf_getbool("verb",&verb)) verb=false;
    if (!sf_getfloat("eps",&eps)) eps=0.01;
    /* regularization */
     
    if (!sf_getfloat("velocity",&velocity)) velocity=0.0f;
    /* constant velocity */ 

    if (!sf_getbool("nmo",&nmo)) nmo=false;
    /* if y, apply constant velocity NMO */

    if (!sf_getint("order",&order)) order=1;
    /* accuracy order */

    if (!sf_getint("mode",&mode)) mode=1;
    /* 1: predict backward, 2: predict forward then backward */

    if (!sf_getfloat ("h0",&h0)) h0=0.;
    /* reference offset */
    if (half) h0 *= 2.;
    if (!sf_getfloat("eps1",&eps1)) eps1=0.01;
    /* stretch regularization */

    if (!sf_getint("jump",&jump)) jump=1;
    /* subsampling */

    if (!sf_getint("niter",&niter)) niter=10;
    /* number of iterations */

    if (!sf_getint("restart",&restart)) restart=niter;
    /* GMRES memory */

    if (!sf_getfloat("tol",&tol)) tol=1e-5;
    /* GMRES tolerance */

    if (!sf_getfloat("flo",&flo)) {
	/* Low frequency in band, default is 0 */
	flo=0.;
    } else if (0. > flo) {
	sf_error("Negative flo=%g",flo);
    } else {
	flo *= (dt/jump);
    }

    if (!sf_getfloat("fhi",&fhi)) {
	/* High frequency in band, default is Nyquist */	
	fhi=0.5;
    } else {
	fhi *= (dt/jump);	
	if (flo > fhi) 
	    sf_error("Need flo < fhi, "
		     "got flo=%g, fhi=%g",flo/(dt/jump),fhi/(dt/jump));
	if (0.5 < fhi)
	    sf_error("Need fhi < Nyquist, "
		     "got fhi=%g, Nyquist=%g",fhi/(dt/jump),0.5/(dt/jump));
    }

    if (!sf_getint("nplo",&nplo)) nplo = 6;
    /* number of poles for low cutoff */
    if (nplo < 1)  nplo = 1;
    if (nplo > 1)  nplo /= 2; 

    if (!sf_getint("nphi",&nphi)) nphi = 6;
    /* number of poles for high cutoff */
    if (nphi < 1)  nphi = 1;
    if (nphi > 1)  nphi /= 2;
  
    nd = (nt-1)*jump+1;
  
    bandpass_init(nd,flo,fhi,nplo,nphi);
    
    sf_putint(stack,"n1",nd);
    sf_putfloat(stack,"d1",dt/jump);

    trace = sf_floatalloc(nd);
    trace2 = sf_floatalloc(nd);
    gather = sf_floatalloc2(nt,nh);
    dense = sf_floatalloc2(nd,nh);
    dense2 = sf_floatalloc2(nd,nh);
    p = sf_floatalloc2(nd,nh);

    //vel = sf_floatalloc(nd);

    //predict_init (nd, nh, eps*eps, order, 1, false);

    for (ix=0; ix < nx; ix++) {
	if (verb) sf_warning("cmp %d of %d;",ix+1,nx);

	//sf_floatread (vel,nd,velocity);	
	sf_floatread(p[0],nd*nh,dip);

	inmo_init(velocity, off, nh, 
		  h0, dh, CDPtype, ix,
		  nt, slow,
		  t0, dt, half, eps1, eps, order,
		  jump, mode, nmo);

        seislet_set(p);
	
        sf_floatread (gather[0],nt*nh,cmp);
	if (nmo == false) {
		/* apply backward operator */
		interpolate(gather, dense);
		pwstack(dense,trace,mode);
		/* apply shaping */
		bandpass(trace);
	}
	else {
		/* apply backward operator */
		interpolate(gather, dense);
		cnmo(dense,dense2);
		pwstack(dense2,trace,mode);
		/* apply shaping */
		bandpass(trace);
	}
	
	sf_gmres_init(nd,restart);

	for (it=0; it < nd; it++) {
	    trace2[it] = 0.0f;
	}

	/* run GMRES */
	sf_gmres(trace,trace2,pwstack_oper,NULL,niter,tol,true);

	sf_floatwrite (trace2,nd,stack);

	sf_gmres_close();

	inmo_close();
    }
    if (verb) sf_warning(".");

    exit (0);
}

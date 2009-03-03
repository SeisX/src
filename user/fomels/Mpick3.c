/* Automatic picking  from 3-D semblance-like panels. */
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

#include "dynprog3.h"
#include "divn.h"

int main(int argc, char* argv[])
{
    int dim, n[SF_MAX_DIM], rect;
    int it0, it1, niter, n1, n2, i2, n3, i3, i1, gate1, gate2, i0;
    float ***scan, ***weight, **pick, *ampl, **pick2, o2, d2, o3, d3, an1, an2, asum, a, ct0, ct1, vel0;
    bool smooth;
    sf_file scn, pik;

    sf_init(argc,argv);
    scn = sf_input("in");
    pik = sf_output("out");

    if (SF_FLOAT != sf_gettype(scn)) sf_error("Need float input");
    dim = sf_filedims (scn,n);
    if (dim != 3) sf_error("Need three dimensions");

    n1 = n[0];
    n2 = n[1];
    n3 = n[2];

    if (!sf_histfloat(scn,"o2",&o2)) o2=0.;
    if (!sf_histfloat(scn,"d2",&d2)) d2=1.;

    if (!sf_histfloat(scn,"o3",&o3)) o3=0.;
    if (!sf_histfloat(scn,"d3",&d3)) d3=1.;
 
    if (!sf_getfloat("vel0",&vel0)) vel0=o2;
    /* surface velocity */
    i0 = 0.5 + (vel0-o2)/d2;
    if (i0 < 0) i0=0;
    if (i0 >= n2) i0=n2-1;

    sf_putint(pik,"n2",1);
    sf_putint(pik,"n3",1);
    sf_putint(pik,"n4",2);
    
    if (!sf_getint("rect1",&rect)) rect=1;
    /* smoothing radius */

    if (!sf_getint("niter",&niter)) niter=100;
    /* number of iterations */

    if (!sf_getfloat("an1",&an1)) an1=1.;
    if (!sf_getfloat("an2",&an2)) an2=1.;
    /* axes anisotropy */
    if (!sf_getint("gate1",&gate1)) gate1=3;
    if (!sf_getint("gate2",&gate2)) gate2=3;
    /* picking gate */
    if (!sf_getbool("smooth",&smooth)) smooth=true;
    /* if apply smoothing */

    scan = sf_floatalloc3(n1,n2,n3);
    weight = sf_floatalloc3(n2,n3,n1);

    dynprog3_init(n1,n2,n3,gate1,gate2,an1,an2);

    if (smooth) {
	pick = sf_floatalloc2(n1,2);
	pick2 = sf_floatalloc2(n1,2);	
	ampl = sf_floatalloc(n1);

	divn_init(1,n1,&n1,&rect,niter);
    } else {
	pick = NULL;
	pick2 = sf_floatalloc2(n1,2);
	ampl = NULL;
    }

    sf_floatread(scan[0][0],n1*n2*n3,scn);

    /* transpose and reverse */
    for (i3=0; i3 < n3; i3++) {
	for (i2=0; i2 < n2; i2++) {
	    for (i1=0; i1 < n1; i1++) {
		weight[i1][i3][i2] = expf(-scan[i3][i2][i1]);
	    }
	}
    }

    dynprog3(i0, weight);
    dynprog3_traj(pick2);

    if (smooth) {
	for (i1=0; i1 < n1; i1++) {
	    ct0 = pick2[0][i1];
	    it0 = floorf(ct0);
	    ct0 -= it0;

	    if (it0 >= n2-1) {
		it0 = n2-2;
		ct0 = 0.;
	    } else if (it0 < 0) {
		it0 = 0;
		ct0 = 0.;
	    }

	    ct1 = pick2[1][i1];
	    it1 = floorf(ct1);
	    ct1 -= it1;

	    if (it1 >= n3-1) {
		it1 = n3-2;
		ct1 = 0.;
	    } else if (it1 < 0) {
		it1 = 0;
		ct1 = 0.;
	    }

	    ampl[i1]=
		scan[it1  ][it0  ][i1]*(1.-ct0)*(1.-ct1) +
		scan[it1+1][it0  ][i1]*(1.-ct0)*ct1 +
		scan[it1  ][it0+1][i1]*ct0*(1.-ct1) +
		scan[it1+1][it0+1][i1]*ct0*ct1;
	}
    } else {
	for (i1=0; i1 < n1; i1++) {
	    pick2[0][i1] = o2+pick2[0][i1]*d2;
	    pick2[1][i1] = o3+pick2[1][i1]*d3;
	}
    }
    
    if (smooth) {
	/* normalize amplitudes */
	asum = 0.;
	for (i1 = 0; i1 < n1; i1++) {
	    a = ampl[i1];
	    asum += a*a;
	}
	asum = sqrtf (asum/n1);
	for(i1=0; i1 < n1; i1++) {
	    ampl[i1] /= asum;
	    pick[0][i1] = (o2+pick2[0][i1]*d2-vel0)*ampl[i1];
	    pick[1][i1] = (o3+pick2[1][i1]*d3)*ampl[i1];
	}
    
	divn(pick[0],ampl,pick2[0]);
	divn(pick[1],ampl,pick2[1]);

	for(i1=0; i1 < n1; i1++) {
	    pick2[0][i1] += vel0;
	}
    }
    
    sf_floatwrite(pick2[0],n1*2,pik);
 
    exit(0);
}

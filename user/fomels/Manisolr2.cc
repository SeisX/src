// Lowrank decomposition for 2-D anisotropic wave propagation (Real number).
// with options of exact velocity, Zone's approximation (Sripanich and Fomel 2014) and acoustic approximation (Alkhalifah 1998,2000)  
//   Copyright (C) 2010 University of Texas at Austin
//  
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2 of the License, or
//   (at your option) any later version.
//  
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//  
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
#include <time.h>
#include <assert.h>
#include <math.h>

#include <rsf.hh>

#include "vecmatop.hh"
#include "serialize.hh"

using namespace std;

static std::valarray<float>  vx, vz, q, t, vs, xtap, ktap;
static std::valarray<double> kx, kz, c11, c33, c13, c55;
static int approx, relat;
static double dt;
bool ktaper, xtaper;


static int sample(vector<int>& rs, vector<int>& cs, DblNumMat& res)
{
    int nr = rs.size();
    int nc = cs.size();
    res.resize(nr,nc);  
    setvalue(res,0.0);
    for(int a=0; a<nr; a++) {
	int i=rs[a];
	double wx = vx[i]*vx[i];
	double wz = vz[i]*vz[i];
	double qq = q[i];
	double tt = t[i];
	double c = cos(tt);
	double s = sin(tt);
	
	for(int b=0; b<nc; b++) {
	    int j = cs[b];
	    double x0 = kx[j];
	    double z0 = kz[j];
	    // rotation of coordinates
	    double x = x0*c+z0*s;
	    double z = z0*c-x0*s;
	    double r;

	    switch (approx) {
		case 0: // Exact
		{ double second = pow((c11[i]-c55[i])*x*x - (c33[i]-c55[i])*z*z,2) + 4*pow(c13[i]+c55[i],2)*x*x*z*z;
		    second = 0.5*sqrt(second);
		    r = sqrt(0.5*((c11[i]+c55[i])*x*x + (c33[i]+c55[i])*z*z) + second);
		    break; }
		case 1: // Zone's approximation
		{ int lrst = 0;
		    double qv = (pow((c13[i]+c55[i]),2) + c55[i]*(c33[i]-c55[i]))/(c11[i]*(c33[i]-c55[i]));
		    double qh = (pow((c13[i]+c55[i]),2) + c55[i]*(c11[i]-c55[i]))/(c33[i]*(c11[i]-c55[i]));
		    double qm = 0.0, sm = 0.0, sv = 0.0, sh = 0.0 ;
		    
		    switch(relat) {
			case 0: lrst = 0;
			    break;
			case 1: lrst = 1;
			    break;
			case 2: lrst = 2;
			    break;
			default: { 
			    double err[] = {fabs(qh/qv-0.83734),fabs(qh/qv-0.95581),fabs(qh/qv-0.97497)};
			    int l1; // sorting the rr 
			    for (l1=0;l1<2;l1++) {
				if (err[lrst] > err[l1+1]) lrst = l1+1;
			    }
			    break;
			}
		    }
		    /* q horizontal vs q vertical*/
		    double rela[] = {0.83734,0.95581,0.97497};
		    double rela2[] = {0.15810,0.04414,0.02484};
		    
		    /* reduce from four to three */
		    qh = rela[lrst]*qv + rela2[lrst];
		    
		    if ( fabs(qv-1.0) > 1e-4 && fabs(qh-1.0) > 1e-4)  { /*Avoid isotropic or elliptical anisotropy*/
				//sv = (c11[i]*(c33[i]-c11[i])*(qh-1)*(qv-1)*(qv-1))/(2*(c33[i]*(1-qh)+c11[i]*(qv-1))*(c33[i]*(qv-qh)+c11[i]*(qh+qv-qv*qv-1)));
			sv = ((c11[i] - c33[i])*(-1 + qh)*(-1 + qv)*(-1 + qv))/(2*((-c33[i])*(-1 + 2*qv + qh*(1 + qh + (-4 + qv)*qv)) + c11[i]*(-1 + qh*qh - 2*qh*qv + qv*(3 + (-2 + qv)*qv))));
			  	//sh = (c33[i]*(c11[i]-c33[i])*(qh-1)*(qh-1)*(qv-1))/(2*(c33[i]*(qh-1)+c11[i]*(1-qv))*(c11[i]*(qh-qv)+c33[i]*(qh+qv-qh*qh-1)));
			sh = ((c11[i] - c33[i])*(-1 + qh)*(-1 + qh)*(-1 + qv))/(2*(c33[i] - c33[i]*(qh*(3 + (-2 + qh)*qh) - 2*qh*qv + qv*qv) + c11[i]*(-1 + 2*qh + qv + (-4 + qh)*qh*qv + qv*qv)));
		    } else  {
			qv = 1.0;
			qh = 1.0;
			sv = 0.5;
			sh = 0.5;			
		    }
			
		    if(x==0 && z==0) qm=qv;
		    else qm = qh*wx*(x/hypotf(vx[i]*x,vz[i]*z))*(x/hypotf(vx[i]*x,vz[i]*z)) + qv*wz*(z/hypotf(vx[i]*x,vz[i]*z))*(z/hypotf(vx[i]*x,vz[i]*z));
		
		    if(x==0 && z==0) sm=0.5;
		    else sm=sh*wx*(x/hypotf(vx[i]*x,vz[i]*z))*(x/hypotf(vx[i]*x,vz[i]*z)) + sv*wz*(z/hypotf(vx[i]*x,vz[i]*z))*(z/hypotf(vx[i]*x,vz[i]*z));
		
		    x = wx*x*x;
		    z = wz*z*z;
		    r = x+z;			
		    r = sqrt(r*(1-sm) + sm*sqrt(r*r + 2*(qm-1)*x*z/sm));
		    break; }
		case 2: // Acoustic approximation
		{z = wz*z*z;
		    x = wx*x*x;
		    r = x+z;
		    r = r+sqrt(r*r-qq*x*z);
		    r = sqrt(0.5*r);
		    break; }
		default:
		{sf_error("unknown case %d", approx);
		    r = 0.0;
		    break;}
	    }

	    r = 2*(cos(r*dt)-1);
	    if (xtaper) r *= xtap[i];
	    if (ktaper) r *= ktap[j];

	    res(a,b) = r;
	}
    }
    return 0;
}

int main(int argc, char** argv)
{   
    sf_init(argc,argv); // Initialize RSF

    iRSF par(0);
    int seed;

    par.get("seed",seed,time(NULL)); // seed for random number generator
    srand48(seed);

    float eps;
    par.get("eps",eps,1.e-4); // tolerance

    int npk;
    par.get("npk",npk,20); // maximum rank

    par.get("dt",dt); // time step

    bool fftexp0;
    par.get("fftexp0",fftexp0,0); // model/mig with sffftexp0

    iRSF velz, velx("velx"), eta("eta"), theta("theta");

    int nz,nx;
    velz.get("n1",nz);
    velz.get("n2",nx);
    int m = nx*nz;

    vx.resize(m);
    vz.resize(m);
    vs.resize(m);
    q.resize(m);
    t.resize(m);

    c11.resize(m);
    c33.resize(m);
    c13.resize(m);
    c55.resize(m);

    if (fftexp0 == 0) {
      velx >> vx;
      velz >> vz;
    } else {
      velz >> vx;
      velx >> vz;
    }

    eta >> q;
    theta >> t;
    
    par.get("approx",approx,2); // Type of approximation (0=exact 1=zone 2=acoustic)
    par.get("relation",relat,3); // Type of q relationship (0=shale, 1=sand, 2=carbonate, default being smallest error)
    
    /* Get vs*/
    if (approx == 0 || approx==1) {
	iRSF vels("vels");
	vs.resize(m);
	vels >> vs;
    }

    /* Invert for cij*/
    if (approx == 0 || approx == 1) {
    	int count;
    	for (count=0; count<m; count++) {
    		c11[count] = vx[count]*vx[count];
    		c33[count] = vz[count]*vz[count];
    		c55[count] = vs[count]*vs[count];
    		c13[count] = sqrt(c33[count]*(c33[count]-c55[count])*(c11[count]/(c33[count]*(1+2*q[count]))-1)+pow(c33[count]-c55[count],2))-c55[count];
    	}
    }
    
    switch (approx) {
    	case 0:
    	{	
		sf_warning("==================================");
    		sf_warning("Exact velocity");
		sf_warning("==================================");
    		break;
    	}
    	case 1:
    	{
		sf_warning("==================================");
    		sf_warning("Zone's approximation");
		sf_warning("==================================");
    		break;
    	}
    	case 2:
    	{
		sf_warning("==================================");
    		sf_warning("Acoustic approximation");
		sf_warning("==================================");
    		/* from eta to q */
    		for (int im=0; im < m; im++) {
			q[im] = 8*q[im]/(1.0+2*q[im]);
    		}
    		break;
    	}
    }

    /* fram degrees to radians */
    for (int im=0; im < m; im++) {
	t[im] *= SF_PI/180.;
    }
    
    iRSF fft("fft");

    int nkz,nkx;
    fft.get("n1",nkz);
    fft.get("n2",nkx);

    float dkz,dkx;
    fft.get("d1",dkz);
    fft.get("d2",dkx);
    
    float kz0,kx0;
    fft.get("o1",kz0);
    fft.get("o2",kx0);

    int n = nkx*nkz;
    kx.resize(n);
    kz.resize(n);
    int i = 0;
    for (int ix=0; ix < nkx; ix++) {
	for (int iz=0; iz < nkz; iz++) {
	    kx[i] = 2*SF_PI*(kx0+ix*dkx);
	    kz[i] = 2*SF_PI*(kz0+iz*dkz);
	    i++;
	}
    }

    par.get("xtaper",xtaper,false); // if taper in x
    par.get("ktaper",ktaper,false); // if taper in k

    if (xtaper) {
	iRSF fxtap("xtap");
	xtap.resize(m);
	fxtap >> xtap;
    }

    if (ktaper) {
	iRSF fktap("ktap");
	ktap.resize(n);
	fktap >> ktap;
    }

    vector<int> lidx, ridx;
    DblNumMat mid;

    iC( ddlowrank(m,n,sample,eps,npk,lidx,ridx,mid) );

    int m2=mid.m();
    int n2=mid.n();

    vector<int> midx(m), nidx(n);
    for (int k=0; k < m; k++) 
	midx[k] = k;
    for (int k=0; k < n; k++) 
	nidx[k] = k;    

    DblNumMat lmat(m,m2);
    iC ( sample(midx,lidx,lmat) );

    DblNumMat lmat2(m,n2);
    iC( ddgemm(1.0, lmat, mid, 0.0, lmat2) );

    double *ldat = lmat2.data();
    std::valarray<float> ldata(m*n2);
    for (int k=0; k < m*n2; k++) 
	ldata[k] = ldat[k];
    oRSF left("left");
    left.put("n1",m);
    left.put("n2",n2);
    left << ldata;

    DblNumMat rmat(n2,n);
    iC ( sample(ridx,nidx,rmat) );

    double *rdat = rmat.data();
    std::valarray<float> rdata(n2*n);    
    for (int k=0; k < n2*n; k++) 
	rdata[k] = rdat[k];
    oRSF right;
    right.put("n1",n2);
    right.put("n2",n);
    right << rdata;

    exit(0);
}

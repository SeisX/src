/* Seislet transform */
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

#include "seislet.h"
#include "predict.h"

static int nt, n;
static bool inv, unit;
static float **t, **d, *t1, *t2, *w;
static void (*transform)(bool);

static void predict_forw(bool adj, float *tt, int i, int j)
/* Predict forward */
{
    int i2;

    for (i2=i; i2 < i+j; i2++) {
	predict_step(adj,true,tt,d[i2]);
    }
}

static void predict_back(bool adj, float *tt, int i, int j)    
/* Predict backward */
{
    int i2;

    for (i2=i+j-1; i2 >= i; i2--) {
	predict_step(adj,false,tt,d[i2]);
    }
}

static void haar(bool adj) 
/* Lifting Haar transform in place */
{
    int i, j, i1;

    if (adj) {
	for (j=1; j <= nt/2; j *= 2) {
	    for (i=0; i < nt-j; i += 2*j) {
		if (inv) {
		    for (i1=0; i1 < n; i1++) {
			t1[i1] = t[i][i1];
		    }
		    predict_forw(false,t1,i,j); /* *z0 */
		    for (i1=0; i1 < n; i1++) {
			t[i+j][i1] -= t1[i1];     
			/* d = o - P[e] */
		    }
		    for (i1=0; i1 < n; i1++) {
			t2[i1] = t[i+j][i1];
		    }
		    predict_back(false,t2,i,j); /* 1/z0 */
		    for (i1=0; i1 < n; i1++) {
			t[i][i1] += t2[i1]/2; 
                        /* s = e + U[d] */
		    }
		} else {
		    for (i1=0; i1 < n; i1++) {
			t1[i1] = t[i+j][i1];  
		    }
		    predict_back(false,t1,i,j); 
		    for (i1=0; i1 < n; i1++) {
			t[i][i1] += t1[i1];
			/* s = e + P'[d] */
		    }
		    for (i1=0; i1 < n; i1++) {
			t2[i1] = t[i][i1]; 
		    }
		    predict_forw(false,t2,i,j); 
		    for (i1=0; i1 < n; i1++) {
			t[i+j][i1] -= t2[i1]/2;
			/* d = o - U'[s] */
		    }
		}
	    }
	}
    } else {
	for (j=nt/2; j >= 1; j /= 2) {
	    for (i=0; i < nt-j; i += 2*j) {
		for (i1=0; i1 < n; i1++) {
		    t2[i1] = t[i+j][i1];
		}
		predict_back(false,t2,i,j); 
		for (i1=0; i1 < n; i1++) {
		    t[i][i1] -= t2[i1]/2; 
		    /* e = s - U[d] */
		}
		for (i1=0; i1 < n; i1++) {
		    t1[i1] = t[i][i1];
		}
		predict_forw(false,t1,i,j); 
		for (i1=0; i1 < n; i1++) {
		    t[i+j][i1] += t1[i1];     
		    /* o = d + P[e] */
		}
	    }
	}
    } 
}

static void linear(bool adj)
{
    int i, j, i1;
    
    if (adj) {
	for (j=1; j <= nt/2; j *= 2) {
	    if (inv) {
		for (i=0; i < nt-2*j; i += 2*j) {
		    for (i1=0; i1 < n; i1++) {
			t1[i1] = t[i][i1];
			t2[i1] = t[i+2*j][i1];
		    }
		    predict_forw(false,t1,i,j);
		    predict_back(false,t2,i+j,j);
		    for (i1=0; i1 < n; i1++) {
			t[i+j][i1] -= (t1[i1]+t2[i1])/2; 
                        /* d = o - P[e] */
		    }
		}		
		if (i+j < nt) {
		    for (i1=0; i1 < n; i1++) {
			t1[i1] = t[i][i1];
		    }
		    predict_forw(false,t1,i,j);
		    for (i1=0; i1 < n; i1++) {
			t[i+j][i1] -= t1[i1];
			/* d = o - P[e] */
		    }		    
		}
		for (i1=0; i1 < n; i1++) {
		    t1[i1] = t[j][i1];
		}
		predict_back(false,t1,0,j);
		for (i1=0; i1 < n; i1++) {
		    t[0][i1] += t1[i1]/2;
		}
		for (i=2*j; i < nt-j; i += 2*j) {
		    for (i1=0; i1 < n; i1++) {
			t1[i1] = t[i+j][i1];
			t2[i1] = t[i-j][i1];
		    }
		    predict_back(false,t1,i,j);
		    predict_forw(false,t2,i-j,j);
		    for (i1=0; i1 < n; i1++) {
			t[i][i1] += (t1[i1]+t2[i1])/4;
			/* s = e + U d */
		    }
		}
	    } else {
		for (i=0; i < nt-2*j; i += 2*j) {
		    for (i1=0; i1 < n; i1++) {
			t1[i1] = t[i+j][i1];
			t2[i1] = t[i+j][i1];
		    }
		    predict_back(false,t1,i,j);
		    predict_forw(false,t2,i+j,j);
		    for (i1=0; i1 < n; i1++) {
			t[i][i1]     += t1[i1]/2;
			t[i+2*j][i1] += t2[i1]/2;
		    }
		}		
		if (i+j < nt) {
		    for (i1=0; i1 < n; i1++) {
			t1[i1] = t[i+j][i1];
		    }
		    predict_back(false,t1,i,j);
		    for (i1=0; i1 < n; i1++) {
			t[i][i1] += t1[i1];
		    }		    
		}
		for (i1=0; i1 < n; i1++) {
		    t1[i1] = t[0][i1];
		}
		predict_forw(false,t1,0,j);
		for (i1=0; i1 < n; i1++) {
		    t[j][i1] -= t1[i1]/2;
		}
		for (i=2*j; i < nt-j; i += 2*j) {
		    for (i1=0; i1 < n; i1++) {
			t1[i1] = t[i][i1];
			t2[i1] = t[i][i1];
		    }
		    predict_forw(false,t1,i,j);
		    predict_back(false,t2,i-j,j);
		    for (i1=0; i1 < n; i1++) {
			t[i+j][i1] -= t1[i1]/4;
			t[i-j][i1] -= t2[i1]/4;
		    }
		}
	    }
	}
    } else {
	for (j=nt/2; j >= 1; j /= 2) {
	    for (i=2*j; i < nt-j; i += 2*j) {
		for (i1=0; i1 < n; i1++) {
		    t1[i1] = t[i+j][i1];
		    t2[i1] = t[i-j][i1];
		}
		predict_back(false,t1,i,j);
		predict_forw(false,t2,i-j,j);
		for (i1=0; i1 < n; i1++) {
		    t[i][i1] -= (t1[i1]+t2[i1])/4;
		    /* e = s - U d */
		}
	    }
	    for (i1=0; i1 < n; i1++) {
		t1[i1] = t[j][i1];
	    }
	    predict_back(false,t1,0,j);
	    for (i1=0; i1 < n; i1++) {
		t[0][i1] -= t1[i1]/2;
	    }
	    for (i=0; i < nt-2*j; i += 2*j) {
		for (i1=0; i1 < n; i1++) {
		    t1[i1] = t[i][i1];
		    t2[i1] = t[i+2*j][i1];
		}
		predict_forw(false,t1,i,j);
		predict_back(false,t2,i+j,j);
		for (i1=0; i1 < n; i1++) {
		    t[i+j][i1] += (t1[i1]+t2[i1])/2; 
		    /* o = d + P[e] */
		}
	    }	 
	    if (i+j < nt) {
		for (i1=0; i1 < n; i1++) {
		    t1[i1] = t[i][i1];
		}
		predict_forw(false,t1,i,j);
		for (i1=0; i1 < n; i1++) {
		    t[i+j][i1] += t1[i1];
		    /* o = d + P[e] */
		}		    
	    }
	}
    }
}

static void biorthogonal(bool adj)
{
    int i, j, i1;
    float a;    
    if (adj) {
	for (j=1; j <= nt/2; j *= 2) {
	    if (inv) {

		a = -1.586134342f;
	    	for (i=0; i < nt-2*j; i += 2*j) {
		    for (i1=0; i1 < n; i1++) {
                        t1[i1] = t[i][i1];
                        t2[i1] = t[i+2*j][i1];
                    }
		    predict_forw(false,t1,i,j);
		    predict_back(false,t2,i+j,j);
		    for (i1=0; i1 < n; i1++) {
		        t[i+j][i1] += (t1[i1]+t2[i1])*a;
		         /* Predict 1 */
                    }
	        }	 
	        if (i+j < nt) {
		    for (i1=0; i1 < n; i1++) {
			t1[i1] = t[i][i1];
		    }
		    predict_forw(false,t1,i,j);
		    for (i1=0; i1 < n; i1++) {
                        t[i+j][i1] += 2*a*t1[i1];  /*right boundary*/  
		    }		    
		}
                a= -0.05298011854f;
		for (i1=0; i1 < n; i1++) {
		    t1[i1] = t[j][i1];
		}
		predict_back(false,t1,0,j);
		for (i1=0; i1 < n; i1++) {
	            t[0][i1] += 2*a*t1[i1];      /*left boundary*/
                }

	        for (i=2*j; i < nt-j; i += 2*j) {
		    for (i1=0; i1 < n; i1++) {
			t1[i1] = t[i+j][i1];
			t2[i1] = t[i-j][i1];
		    }
		    predict_back(false,t1,i,j);
		    predict_forw(false,t2,i-j,j);
		    for (i1=0; i1 < n; i1++) {
		        t[i][i1] += (t1[i1]+t2[i1])*a;
		        /* Update 1 */
                    }
	        }
                /* Step 1 */
		a = 0.8829110762f;
	    	for (i=0; i < nt-2*j; i += 2*j) {
		    for (i1=0; i1 < n; i1++) {
                        t1[i1] = t[i][i1];
                        t2[i1] = t[i+2*j][i1];
                    }
		    predict_forw(false,t1,i,j);
		    predict_back(false,t2,i+j,j);
		    for (i1=0; i1 < n; i1++) {
		        t[i+j][i1] += (t1[i1]+t2[i1])*a;
		         /* Predict 2 */
                    }
	        }	 
	        if (i+j < nt) {
		    for (i1=0; i1 < n; i1++) {
			t1[i1] = t[i][i1];
		    }
		    predict_forw(false,t1,i,j);
		    for (i1=0; i1 < n; i1++) {
                        t[i+j][i1] += 2*a*t1[i1];  /*right boundary*/  
		    }		    
		}
                a= 0.4435068522f;
		for (i1=0; i1 < n; i1++) {
		    t1[i1] = t[j][i1];
		}
		predict_back(false,t1,0,j);
		for (i1=0; i1 < n; i1++) {
	            t[0][i1] += 2*a*t1[i1];      /*left boundary*/
                }

	        for (i=2*j; i < nt-j; i += 2*j) {
		    for (i1=0; i1 < n; i1++) {
			t1[i1] = t[i+j][i1];
			t2[i1] = t[i-j][i1];
		    }
		    predict_back(false,t1,i,j);
		    predict_forw(false,t2,i-j,j);
		    for (i1=0; i1 < n; i1++) {
		        t[i][i1] += (t1[i1]+t2[i1])*a;
		        /* Update 2 */
                    }
	        }
                /* Step 2 */


                a= 1/(1.230174105f);
	        for (i=0; i < nt-2*j; i += 2*j) {
		    for (i1=0; i1 < n; i1++) {
		        t[i+j][i1] *= a;
                    }
	        }
	        if (i+j < nt) {
		    for (i1=0; i1 < n; i1++) {
                        t[i+j][i1] *= a;  /*right boundary*/  
		    }		    
		}
		for (i1=0; i1 < n; i1++) {
	            t[0][i1] /= a;        /*left boundary*/
                }
	        for (i=2*j; i < nt-j; i += 2*j) {
		    for (i1=0; i1 < n; i1++) {
		        t[i][i1] /= a;
                    }
	        }
		       /* Scale */
	    } else {

		a = 1.586134342f;
	    	for (i=0; i < nt-2*j; i += 2*j) {
		    for (i1=0; i1 < n; i1++) {
                        t1[i1] = t[i+j][i1];
                        t2[i1] = t[i+j][i1];
                    }
		    predict_back(false,t1,i,j);
		    predict_forw(false,t2,i+j,j);
		    for (i1=0; i1 < n; i1++) {
			t[i][i1]     += t1[i1]*a;
			t[i+2*j][i1] += t2[i1]*a;
			/* Adjoint Undo Predict 1 */
                    }
	        }	 
	        if (i+j < nt) {
		    for (i1=0; i1 < n; i1++) {
			t1[i1] = t[i+j][i1];
		    }
		    predict_back(false,t1,i,j);
		    for (i1=0; i1 < n; i1++) {
                        t[i][i1] += 2*a*t1[i1];  /*right boundary*/  
		    }		    
		}
                a= 0.05298011854f;
		for (i1=0; i1 < n; i1++) {
		    t1[i1] = t[0][i1];
		}
		predict_forw(false,t1,0,j);
		for (i1=0; i1 < n; i1++) {
	            t[j][i1] += 2*a*t1[i1];      /*left boundary*/
                }
		
	        for (i=2*j; i < nt-j; i += 2*j) {
		    for (i1=0; i1 < n; i1++) {
			t1[i1] = t[i][i1];
			t2[i1] = t[i][i1];
		    }
		    predict_forw(false,t1,i,j);
		    predict_back(false,t2,i-j,j);
		    for (i1=0; i1 < n; i1++) {
			t[i+j][i1]  += t1[i1]*a;
			t[i-j][i1]  += t2[i1]*a;
			/* Adjoint Undo Update 1 */
		    }
	        }
                /* Adjoint Undo Step 1 */
		a = -0.8829110762f;
	    	for (i=0; i < nt-2*j; i += 2*j) {
		    for (i1=0; i1 < n; i1++) {
                        t1[i1] = t[i+j][i1];
                        t2[i1] = t[i+j][i1];
		    }
		    predict_back(false,t1,i,j);
		    predict_forw(false,t2,i+j,j);
		    for (i1=0; i1 < n; i1++) {
			t[i][i1]     += t1[i1]*a;
			t[i+2*j][i1] += t2[i1]*a;
			/* Adjoint Undo Predict 2 */
                    }
	        }	 
	        if (i+j < nt) {
		    for (i1=0; i1 < n; i1++) {
			t1[i1] = t[i+j][i1];
		    }
		    predict_back(false,t1,i,j);
		    for (i1=0; i1 < n; i1++) {
                        t[i][i1] += 2*a*t1[i1];  /*right boundary*/  
		    }		    
		}
                a= -0.4435068522f;
		for (i1=0; i1 < n; i1++) {
		    t1[i1] = t[0][i1];
		}
		predict_forw(false,t1,0,j);
		for (i1=0; i1 < n; i1++) {
	            t[j][i1] += 2*a*t1[i1];      /*left boundary*/
                }
		
	        for (i=2*j; i < nt-j; i += 2*j) {
		    for (i1=0; i1 < n; i1++) {
			t1[i1] = t[i][i1];
			t2[i1] = t[i][i1];
		    }
		    predict_forw(false,t1,i,j);
		    predict_back(false,t2,i-j,j);
		    for (i1=0; i1 < n; i1++) {
			t[i+j][i1]  += t1[i1]*a;
			t[i-j][i1]  += t2[i1]*a;
			/* Adjoint Undo Update 2 */
                    }
	        }
		/* Adjoint Undo Step 2 */
                a= 1.230174105f;
	        for (i=0; i < nt-2*j; i += 2*j) {
		    for (i1=0; i1 < n; i1++) {
		        t[i+j][i1] *= a;
                    }
	        }
	        if (i+j < nt) {
		    for (i1=0; i1 < n; i1++) {
                        t[i+j][i1] *= a;  /*right boundary*/  
		    }		    
		}
		for (i1=0; i1 < n; i1++) {
	            t[0][i1] /= a;        /*left boundary*/
                }
	        for (i=2*j; i < nt-j; i += 2*j) {
		    for (i1=0; i1 < n; i1++) {
		        t[i][i1] /= a;
                    }
	        }
		/* Adjoint Undo Scale */
	    }
	}
    } else {
	for (j=nt/2; j >= 1; j /= 2) {

            a= 1.230174105f;
	    for (i=2*j; i < nt-j; i += 2*j) {
		for (i1=0; i1 < n; i1++) {
		    t[i][i1] /= a;
                }
	    }
            for (i1=0; i1 < n; i1++) {
	        t[0][i1] /= a;        /*left boundary*/
            }
	    for (i=0; i < nt-2*j; i += 2*j) {
		for (i1=0; i1 < n; i1++) {
		    t[i+j][i1] *= a;
                }
	    }
	    if (i+j < nt) {
		for (i1=0; i1 < n; i1++) {
                    t[i+j][i1] *= a;  /*right boundary*/  
		}		    
            }
		    /* Undo Scale */

            a= -0.4435068522f;
	    for (i=2*j; i < nt-j; i += 2*j) {
		for (i1=0; i1 < n; i1++) {
	            t1[i1] = t[i+j][i1];
		    t2[i1] = t[i-j][i1];
		}
		predict_back(false,t1,i,j);
		predict_forw(false,t2,i-j,j);
		for (i1=0; i1 < n; i1++) {
		    t[i][i1] += (t1[i1]+t2[i1])*a;
		        /* Undo Update 2 */
                }
	    }
            for (i1=0; i1 < n; i1++) {
		t1[i1] = t[j][i1];
	    }
	    predict_back(false,t1,0,j);
	    for (i1=0; i1 < n; i1++) {
	        t[0][i1] += 2*a*t1[i1];      /*left boundary*/
            }

	    a = -0.8829110762f;
	    for (i=0; i < nt-2*j; i += 2*j) {
		for (i1=0; i1 < n; i1++) {
                    t1[i1] = t[i][i1];
                    t2[i1] = t[i+2*j][i1];
                }
		predict_forw(false,t1,i,j);
		predict_back(false,t2,i+j,j);
		for (i1=0; i1 < n; i1++) {
		    t[i+j][i1] += (t1[i1]+t2[i1])*a;
		         /* Undo Predict 2 */
                }
	    }	 
	    if (i+j < nt) {
		for (i1=0; i1 < n; i1++) {
		    t1[i1] = t[i][i1];
		}
		predict_forw(false,t1,i,j);
		for (i1=0; i1 < n; i1++) {
                    t[i+j][i1] += 2*a*t1[i1];  /*right boundary*/  
		}		    
	    }
                   /* Undo Step 2 */

            a= 0.05298011854f;
	    for (i=2*j; i < nt-j; i += 2*j) {
		for (i1=0; i1 < n; i1++) {
	            t1[i1] = t[i+j][i1];
		    t2[i1] = t[i-j][i1];
		}
		predict_back(false,t1,i,j);
		predict_forw(false,t2,i-j,j);
		for (i1=0; i1 < n; i1++) {
		    t[i][i1] += (t1[i1]+t2[i1])*a;
		        /* Undo Update 1 */
                }
	    }
            for (i1=0; i1 < n; i1++) {
		t1[i1] = t[j][i1];
	    }
	    predict_back(false,t1,0,j);
	    for (i1=0; i1 < n; i1++) {
	        t[0][i1] += 2*a*t1[i1];      /*left boundary*/
            }
	    a = 1.586134342f;
	    for (i=0; i < nt-2*j; i += 2*j) {
		for (i1=0; i1 < n; i1++) {
                    t1[i1] = t[i][i1];
                    t2[i1] = t[i+2*j][i1];
                }
		predict_forw(false,t1,i,j);
		predict_back(false,t2,i+j,j);
		for (i1=0; i1 < n; i1++) {
		    t[i+j][i1] += (t1[i1]+t2[i1])*a;
		         /* Undo Predict 1 */
                }
	    }	 
	    if (i+j < nt) {
		for (i1=0; i1 < n; i1++) {
		    t1[i1] = t[i][i1];
		}
		predict_forw(false,t1,i,j);
		for (i1=0; i1 < n; i1++) {
                    t[i+j][i1] += 2*a*t1[i1];  /*right boundary*/  
		}		    
	    }
                   /* Undo Step 1 */
	}
    }
}


void seislet_init(int n1      /* trace length */, 
		  int n2      /* number of traces */, 
		  bool inv1   /* inversion flag */, 
		  bool unit1  /* weighting flag */,
		  float eps   /* regularization parameter */,
		  int order   /* accuracy order */,
		  char type   /* transform type */) 
/*< allocate space >*/
{
    int i,j;
    float wi;

    inv = inv1;
    unit = unit1;

    n = n1;
    for (nt=1; nt < n2; nt *= 2) ;
    if (n2!= nt) sf_error("n2=%d need pow(2,n)",n2);

    t = sf_floatalloc2(n,nt);

    predict_init (n, nt, eps*eps, order, 1, false);

    t1 = sf_floatalloc(n);
    t2 = sf_floatalloc(n);

    switch(type) {
	case 'h': 
	    transform = haar;
	    break;
	case 'l':
	    transform = linear;
	    break;
	case 'b':
	    transform = biorthogonal;
	    break;
	default:
	    sf_error("Unknown wavelet type=%c",type);
	    break;
    }

    if (unit) {
	w = sf_floatalloc(nt);

	w[0] = sqrtf((float) nt);
	wi = 0.5;	
	for (j=1; j <= nt/2; j *= 2, wi *= 2) {
	    for (i=0; i < nt-j; i += 2*j) {
		w[i+j] = sqrtf(wi);
	    }
	}
    }
}

void seislet_set(float **dip /* local slope */)
/*< set local slope >*/
{
    d = dip;
}

void seislet_close(void) 
/*< deallocate space >*/
{
    free (*t);
    free (t);
    free (t1);
    free (t2);
    if (unit) free(w);
    predict_close();
}

void seislet_lop(bool adj, bool add, int nx, int ny, float *x, float *y)
/*< linear operator >*/
{
    int it, i, j, i1;

    sf_adjnull (adj,add,nx,ny,x,y);

    if (adj) {
	for (it=0; it < nx; it++) {
	    t[0][it] = y[it];
	}
	for (it=nx; it < n*nt; it++) {
	    t[0][it] = 0.;
	}
    } else {
	for (i1=0; i1 < n; i1++) {
	    t[0][i1] = x[i1];
	}
	it = n;
	for (j=nt/2; j >= 1; j /= 2) {
	    for (i=0; i < nt-j; i += 2*j) {
		for (i1=0; i1 < n; i1++) {
		    if (it < ny) {
			t[i+j][i1]=x[it];
			it++;
		    } else {
			t[i+j][i1]=0.;
		    }
		}
	    }	    	    
	}

	if (unit) {
	    for (it=0; it < nt; it++) {
		for (i1=0; i1 < n; i1++) {
		    if (inv) {
			t[it][i1] /= w[it];
		    } else {
			t[it][i1] *= w[it];
		    }
		}
	    }
	}
    }

    transform(adj);

    if (adj) {
	if (unit) {
	    for (it=0; it < nt; it++) {
		for (i1=0; i1 < n; i1++) {
		    t[it][i1] *= w[it];
		}
	    }
	}

	for (i1=0; i1 < n; i1++) {
	    x[i1] += t[0][i1];
	}
	it = n;
	for (j=nt/2; j >= 1; j /= 2) {
	    for (i=0; i < nt-j; i += 2*j) {
		for (i1=0; i1 < n; i1++) {
		    x[it] += t[i+j][i1];
		    it++;
		    if (it >= ny) return;
		}
	    }	    	    
	}
    } else {
	for (it=0; it < nx; it++) {
	    y[it] += t[0][it];
	}
    }
}

void seislet_destruct(bool adj, bool add, int nx, int ny, float *x, float *y)
/*< linear operator >*/
{
    int it, i, j, i1;
    inv=(bool)!adj;

    sf_adjnull (adj,add,nx,ny,x,y);

    if (adj) {
	for (i1=0; i1 < n; i1++) {
	    t[0][i1] = y[i1];
	}
	it = n;
	for (j=nt/2; j >= 1; j /= 2) {
	    for (i=0; i < nt-j; i += 2*j) {
		for (i1=0; i1 < n; i1++) {
		    if (it < ny) {
			t[i+j][i1]=y[it];
			it++;
		    } else {
			t[i+j][i1]=0.;
		    }
		}
	    }	    	    
	}

	if (unit) {
	    for (it=0; it < nt; it++) {
		for (i1=0; i1 < n; i1++) {
		    if (inv) {
			t[it][i1] /= w[it];
		    } else {
			t[it][i1] *= w[it];
		    }
		}
	    }
	}

    } else {
	for (it=0; it < nx; it++) {
	    t[0][it] = x[it];
	}
	for (it=nx; it < n*nt; it++) {
	    t[0][it] = 0.;
	}

    }

    transform((bool)!adj);

    if (adj) {
	for (it=0; it < nx; it++) {
	    x[it] += t[0][it];
	}
    } else {
	if (unit) {
	    for (it=0; it < nt; it++) {
		for (i1=0; i1 < n; i1++) {
		    t[it][i1] *= w[it];
		}
	    }
	}

	for (i1=0; i1 < n; i1++) {
	    y[i1] += t[0][i1];
	}
	it = n;
	for (j=nt/2; j >= 1; j /= 2) {
	    for (i=0; i < nt-j; i += 2*j) {
		for (i1=0; i1 < n; i1++) {
		    y[it] += t[i+j][i1];
		    it++;
		    if (it >= ny) return;
		}
	    }	    	    
	}

    }
}

void seislet_construct(bool adj, bool add, int nx, int ny, float *x, float *y)
/*< linear operator >*/
{
    int it, i, j, i1;
    inv=(bool)!adj; 

    sf_adjnull (adj,add,nx,ny,x,y);

    if (adj) {
	for (it=0; it < nx; it++) {
	    t[0][it] = y[it];
	}
	for (it=nx; it < n*nt; it++) {
	    t[0][it] = 0.;
	}
    } else {
	for (i1=0; i1 < n; i1++) {
	    t[0][i1] = x[i1];
	}
	it = n;
	for (j=nt/2; j >= 1; j /= 2) {
	    for (i=0; i < nt-j; i += 2*j) {
		for (i1=0; i1 < n; i1++) {
		    if (it < ny) {
			t[i+j][i1]=x[it];
			it++;
		    } else {
			t[i+j][i1]=0.;
		    }
		}
	    }	    	    
	}

	if (unit) {
	    for (it=0; it < nt; it++) {
		for (i1=0; i1 < n; i1++) {
		    if (inv) {
			t[it][i1] /= w[it];
		    } else {
			t[it][i1] *= w[it];
		    }
		}
	    }
	}
    }

    transform(adj);

    if (adj) {
	if (unit) {
	    for (it=0; it < nt; it++) {
		for (i1=0; i1 < n; i1++) {
		    t[it][i1] *= w[it];
		}
	    }
	}

	for (i1=0; i1 < n; i1++) {
	    x[i1] += t[0][i1];
	}
	it = n;
	for (j=nt/2; j >= 1; j /= 2) {
	    for (i=0; i < nt-j; i += 2*j) {
		for (i1=0; i1 < n; i1++) {
		    x[it] += t[i+j][i1];
		    it++;
		    if (it >= ny) return;
		}
	    }	    	    
	}
    } else {
	for (it=0; it < nx; it++) {
	    y[it] += t[0][it];
	}
    }

}

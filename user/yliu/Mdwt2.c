/* 1-D digital wavelet transform (another version)
Forward transform (adj=y inv=y)   m=T[d]
Adjoint transform (adj=y inv=n)   m=T^(-1)'[d]
Inverse transform (adj=n inv=y/n) d=T^(-1)[m]
*/
/*
  Copyright (C) 2018 Jilin University

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
#include "wavelet2.h"

int main(int argc, char *argv[])
{
    int n1, i2, n2;
    bool inv, adj, unit;
    char *type;
    float *pp, *qq;
    sf_file in=NULL, out=NULL;

    sf_init(argc,argv);

    in = sf_input("in");
    out = sf_output("out");

    if (!sf_histint(in,"n1",&n1)) sf_error("No n1= in input");
    n2 = sf_leftsize(in,1);

    pp = sf_floatalloc(n1);
    qq = sf_floatalloc(n1);

    if (!sf_getbool("inv",&inv)) inv=false;
    /* if y, do inverse transform */

    if (!sf_getbool("adj",&adj)) adj=false;
    /* if y, do adjoint transform */

    if (!sf_getbool("unit",&unit)) unit=false;
    /* if y, use unitary scaling */

    if (NULL == (type=sf_getstring("type"))) type="linear";
    /* [haar,linear,biorthogonal] wavelet type, the default is linear  */

    sf_wavelet_init2(n1,inv,unit,type[0]);

    for (i2=0; i2 < n2; i2++) {
	sf_floatread(pp,n1,in);
	if (adj) {
	    sf_wavelet_lop2(adj,false,n1,n1,qq,pp);
	} else {
	    sf_wavelet_lop2(adj,false,n1,n1,pp,qq);
	}
	sf_floatwrite(qq,n1,out);
    }


    exit(0);
}

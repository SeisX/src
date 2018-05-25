/* Boundary conditions for a non-stationary helix filter */
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

#include "nbound.h"
#include "bound.h"

#include "nhelix.h"
/*^*/

void nbound (int ip     /* patch number */, 
	     int dim    /* number of dimensions */, 
	     int *nd    /* data size [dim] */, 
	     int *na    /* filter size [dim] */, 
	     nfilter aa /* filter */) 
/*< define boundaries >*/
{
    int i, n;
    sf_filter bb;

    n=1;
    for (i=0; i < dim; i++) {
	n *= nd[i];
    }

    aa->mis = sf_boolalloc(n);
    bb = aa->hlx[ip];

    bound (dim, false, nd, nd, na, bb);

    for (i=0; i < n; i++) {
	aa->mis[i] = bb->mis[i];
    }

    free(bb->mis);
    bb->mis = NULL;
}


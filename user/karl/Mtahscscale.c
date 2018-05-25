/* Surface Consistant SCALE - Compute & apply surface consistant scale

tah is the abbreviation of Trace And Header.  Madagascar programs 
that begin with sftah are designed to:
1- read trace and headers from separate rsf files and write them to 
   standard output (ie sftahread)
2- filter programs that read and write standard input/output and 
   process the tah data (eg sftahnmo, sftahstack)
3- read tah data from standard input and write separate rsf files for 
   the trace and headers data (ie sftahwrite)

These programs allow Seismic Unix (su) like processing in Madagascar.  
Some programs have su like names.

Some programs in this suite are sftahread, sftahgethw, ftahhdrmath, 
and sftahwrite.

EXAMPLE:

  sftahscscale \\
    input=../fetch/npr3_field.rsf \\
    sxy=sxy.rsf       gxy=gxy.rsf \\
    sxyamp=sxyamp.rsf gxyamp=gxyamp.rsf \\
  | sftahwrite \\
    verbose=1                           \\
    label2="ep"  o2=14 n2=850  d2=1   \\
    label3="tracf" o3=1 n3=1063 d3=1    \\
    output=scscale.rsf \\
  >/dev/null

sfgrey <scscale.rsf | sfpen

sftahscscale reads data from the file, applies scaling, and writes data
to STDOUT.  DO NOT USE WITH sftahread!

In this example the input data ../fetch/npr_field.rsf is read.  Trace
headers are read from ../fetch/npr_field_hdr.rsf (the dafault for the
headers parameter).  Trace order does not matter.  Shot data is
likely, but the program will process any trace order (eg cdp or
receiver). the source x,y coordinates are written to sxy.rsf and the
group x,y coordinates are written to gxy.rsf. The shot consistant
amplitude and the shot x,y is written to sxyamp.rsf.  The group
consistant amplitude and the group x,y is written to gxyamp.rsf.
Surface consistant scaling is applied to the data and the resulting
trace and header is written to the pipe.  The sftahwrite writes the
trace data to scscale.rsf and the headers are written to the file
scscale_hdr.rsf.  Finally, the output volume is displayed using
sfgrey.
*/
/*
   Program change history:
   date       Who             What
   01/29/2014 Karl Schleicher Original program based in Mtahsort
*/
#include <string.h>
#include <stdlib.h>
#include <math.h>

#include <rsf.h>
#include <rsfsegy.h>

#include "tahsub.h"

/* sparingly make some global variables. */
int verbose;

bool is_header_allzero(float* header, int n1_headers)
/* test if the array header is all zeros */
{
  bool allzero;
  int i1_headers;
  allzero=true;
  for(i1_headers=0; i1_headers<n1_headers; i1_headers++){
    if(0!=header[i1_headers]){
      allzero=false;
      break;
    }
  }
  return allzero;
}

bool binarysearch(double this_x, double this_y, 
		 double* x_array, double* y_array, int num_xy, int* location )
/* binary search.  Return true/false was point found.  Location found 
   will be in the location argument. */
{   
  int first=0, last=num_xy-1, middle=0;
 
   /* handle zero length list as special case */
   if(num_xy<1){
     *location=0;
     return false;
   }
   while( first <= last ){
     middle = (first+last)/2;
     if (  y_array[middle] <  this_y ||
	   (y_array[middle] == this_y && x_array[middle] < this_x)){
       first = middle + 1;    
     } else if (y_array[middle] == this_y && x_array[middle] == this_x ){
       *location=middle;
       return true;
     } else {
       last = middle - 1;
     }
   }
   /*    first > last.  value is not in list.  What is insertion point?
	 middle is a valid index */
   if (  y_array[middle] <  this_y ||
	 (y_array[middle] == this_y && x_array[middle] < this_x)){
     *location=middle+1;
   } else {
     *location=middle;
   }
   return false;   
}

void insert_unique_xy(double **xarray, double **yarray, 
		 int *num_xy, int* size_xy_array, 
		 double this_x, double this_y)
{
  /* binary search for sx,sy.  Insert if not found */
  int insert_indx=0;
  int indx;

  /* when 0==num_sxy, insert at location 0.  Otherwise binarysearch to find
     the insertion point. */
  if(0==*num_xy ||
     !binarysearch(this_x,this_y,*xarray,*yarray,*num_xy,&insert_indx)){
    /* insert into sx,sy arrays */
    /* fprintf(stderr,"before increment *num_xy=%d\n",*num_xy); */
    (*num_xy)++;
    /* fprintf(stderr,"before increment *num_xy=%d\n",*num_xy); */
    if(*num_xy> *size_xy_array){
      *size_xy_array *= 1.2;
      *xarray=realloc(*xarray,*size_xy_array*sizeof(double));
      *yarray=realloc(*yarray,*size_xy_array*sizeof(double));
    }
    /* move array entries from insert_indx to the end one right
       so this_x and this_y can be inserted */
    for(indx=*num_xy-1; indx>insert_indx; indx--){
      (*xarray)[indx]=(*xarray)[indx-1];
      (*yarray)[indx]=(*yarray)[indx-1];
    }	
    (*xarray)[insert_indx]=this_x;
     (*yarray)[insert_indx]=this_y;
  }
}

int main(int argc, char* argv[])
{
  sf_file infile=NULL, out=NULL, inheaders=NULL;
  int n1_traces;
  int n1_headers;
  int n_traces, n_headers; 
  sf_datatype typehead;
  sf_datatype typein;
  float* header=NULL;
  float* intrace=NULL;
  int i_trace;
  char* infile_filename=NULL;
  char* headers_filename=NULL;

  int indx_sx;
  int indx_sy;
  int size_sxy_array;
  int num_sxy;
  double *sx;
  double *sy;
  double this_sx;
  double this_sy;

  int indx_gx;
  int indx_gy;
  int size_gxy_array;
  int num_gxy;
  double *gx;
  double *gy;
  double this_gx;
  double this_gy;

  char*   sxyfile_name=NULL;
  sf_file sxyfile=NULL;
  off_t nin[SF_MAX_DIM];
  int diminput;
  int iaxis;
  char parameter[13];
  int indx;

  char* gxyfile_name=NULL;
  sf_file gxyfile=NULL;

  char*   sxyampfile_name=NULL;
  sf_file sxyampfile=NULL;
  char*   gxyampfile_name=NULL;
  sf_file gxyampfile=NULL;

  int* trace_shotloc;
  int* trace_grouploc;
  float starttime;
  float endtime;
  float o1;
  float d1;
  int istarttime;
  int iendtime;
  float* trace_amp;
  int num_traces;
  int num_samples;
  int i1_traces;
  int inum_trace;
  float* shot_scale;
  float* shot_fold;
  float* group_scale;
  float* group_fold;
  float global_average_trace_amp;
  float num_nonzero_traces;
    

  sf_init (argc,argv);

  /*****************************/
  /* initialize verbose switch */
  /*****************************/
  /* verbose flag controls amount of print */
  /*( verbose=1 0 terse, 1 informative, 2 chatty, 3 debug ) */
  /* fprintf(stderr,"read verbose switch.  getint reads command line.\n"); */
  if(!sf_getint("verbose",&verbose))verbose=1;
  /* \n
     flag to control amount of print
     0 terse, 1 informative, 2 chatty, 3 debug
  */
  if(verbose>0)fprintf(stderr,"verbose=%d\n",verbose);
 
  /*****************************************/
  /* initialize the input and output files */
  /*****************************************/
  if(verbose>0)fprintf(stderr,"read name of input file name\n");
  
  infile_filename=sf_getstring("input");
  /* \n
     Input file for traces amplitudes
  */
  if(infile_filename==NULL) infile = sf_input ("in");
  else infile = sf_input (infile_filename);

  if(verbose>0)
    fprintf(stderr,"set up output file for tah - should be stdout\n");
  out = sf_output ("out");

  if(verbose>0)fprintf(stderr,"read name of input headers file\n");
  headers_filename=sf_getstring("headers");
  /* \n
     Trace header file name.  Default is the input data file
     name, with the final .rsf changed to _hdr.rsf 
  */

  if(headers_filename==NULL){
    /* compute headers_filename from infile_filename by replacing the final
       .rsf with _hdr.rsf */
    if(!(0==strcmp(infile_filename+strlen(infile_filename)-4,".rsf"))){
	fprintf(stderr,"parameter input, the name of the input file, does\n");
	fprintf(stderr,"not end with .rsf, so header filename cannot be\n");
	fprintf(stderr,"computed by replacing the final .rsf with _hdr.rsf.\n");
	sf_error("default for headers parameter cannot be computed.");
    }
    headers_filename=malloc(strlen(infile_filename)+60);
    strcpy(headers_filename,infile_filename);
    strcpy(headers_filename+strlen(infile_filename)-4,"_hdr.rsf\0");
    if(verbose>2)
      fprintf(stderr,"parameter header defaulted.  Computed to be #%s#\n",
			 headers_filename);
  }
  if(verbose>2)
    fprintf(stderr,"parameter header input or default computed  #%s#\n",
		       headers_filename);

  inheaders = sf_input(headers_filename);

  if (!sf_histint(infile,"n1",&n1_traces))
    sf_error("input file does not define n1");
  if (!sf_histint(inheaders,"n1",&n1_headers)) 
    sf_error("input headers file does not define n1");
  
  n_traces=sf_leftsize(infile,1);
  n_headers=sf_leftsize(inheaders,1);
  if(verbose>0)
    fprintf(stderr,"n_traces=%d, n_headers=%d\n",n_traces,n_headers);

  if(n_traces!=n_headers){
    fprintf(stderr,"n_traces=%d, n_headers=%d\n",n_traces,n_headers);
    sf_error("number of traces and headers must be the same");
  }

  typehead = sf_gettype(inheaders);
  if (SF_INT != typehead && SF_FLOAT != typehead ) 
    sf_error("Need float or int input headers.");

  typein = sf_gettype(infile);
  if (SF_FLOAT != typein ) 
    sf_error("input file must contain floats.");
  
  /* must be float or int */
  if(verbose>1)fprintf(stderr,"allocate headers.  n1_headers=%d\n",n1_headers);
  header = sf_floatalloc(n1_headers);
 
  if(verbose>1)fprintf(stderr,"allocate intrace.  n1_traces=%d\n",n1_traces);

  intrace= sf_floatalloc(n1_traces);

  if(verbose>1)
    fprintf(stderr,"need to add 2 words, record type and record length.\n");
  sf_putint(out,"n1",n1_traces+n1_headers+2);
  sf_putint(out,"n1_traces",n1_traces);

  /*************************************************************************/
  /* put info into history so later tah programs can figure out the headers*/
  /*************************************************************************/
  if(verbose>1)fprintf(stderr,"put n1_headers to history\n");
  sf_putint(out,"n1_headers",n1_headers);
  if(verbose>1)fprintf(stderr,"test typehead\n");
  if(SF_INT == typehead) sf_putstring(out,"header_format","native_int");
    else                 sf_putstring(out,"header_format","native_float");
  sf_putstring(out,"headers",headers_filename);
  /* the list of any extra header keys */
  if(verbose>1)fprintf(stderr,"segy_init\n");
  segy_init(n1_headers,inheaders);
  if(verbose>1)fprintf(stderr,"segy2hist\n");
  segy2hist(out,n1_headers);

  /* put the history from the input file to the output */
  if(verbose>1)fprintf(stderr,"fileflush out\n");
  sf_fileflush(out,infile);

  /* get the indicies of sx,sy,gx,gy in the trace header */
  indx_sx=segykey("sx");
  indx_sy=segykey("sy");
  indx_gx=segykey("gx");
  indx_gy=segykey("gy");

  size_sxy_array=100;
  num_sxy=0;
  
  size_gxy_array=100;
  num_gxy=0;

  sx=malloc(size_sxy_array*sizeof(double));
  sy=malloc(size_sxy_array*sizeof(double));
  gx=malloc(size_gxy_array*sizeof(double));
  gy=malloc(size_gxy_array*sizeof(double));

  if(verbose>0)fprintf(stderr,"start read headers loop to build sxy and gxy\n");
  for (i_trace=0; i_trace<n_traces; i_trace++){
    if(verbose>2 ||(verbose>0 && i_trace<5)){
      fprintf(stderr,"i_trace=%d\n",i_trace);
    }
    /**************************/
    /* read trace and headers */
    /**************************/
    sf_floatread(header,n1_headers,inheaders);
    /* if trace header is all zero, skip trace using break from loop */
    if(is_header_allzero(header,n1_headers))
      continue; /* do not add sxy or gxy to list of surface locations */

    if(typehead==SF_FLOAT){
      this_sx=header[indx_sx];
      this_sy=header[indx_sy];
      this_gx=header[indx_gx];
      this_gy=header[indx_gy];
    } else {
      this_sx=((int*)header)[indx_sx];
      this_sy=((int*)header)[indx_sy];
      this_gx=((int*)header)[indx_gx];
      this_gy=((int*)header)[indx_gy];
    }
    /* binary search for sx,sy.  Insert if not found */
    insert_unique_xy(&sx,&sy,
		     &num_sxy,&size_sxy_array,
		     this_sx, this_sy);

    /* binary search for gx,gy.  Insert if not found */
    insert_unique_xy(&gx,&gy,
		     &num_gxy,&size_gxy_array,
		     this_gx, this_gy);

  }
  /* print the sx,sy */
  if(verbose>2){
    for(indx=0; indx<num_sxy; indx++){
      fprintf(stderr,"sx[%d]=%e sy[%d]=%e\n",indx,sx[indx],indx,sy[indx]);
    }
  }
  /* print the gx,gy */
  if(verbose>2){
    for(indx=0; indx<num_gxy; indx++){
      fprintf(stderr,"gx[%d]=%e gy[%d]=%e\n",indx,gx[indx],indx,gy[indx]);
    }
  }

  if(NULL!=(sxyfile_name=sf_getstring("sxy"))){
    if(verbose>0){
	fprintf(stderr,"sxy=%s input.  Write the source xy coords.\n",
		sxyfile_name);
    }
    sxyfile=sf_output(sxyfile_name);
    sf_putint(sxyfile,"n1",num_sxy);
    sf_putfloat(sxyfile,"d1",1);
    sf_putfloat(sxyfile,"o1",0);
    sf_putstring(sxyfile,"label1","none");
    sf_putstring(sxyfile,"unit1","none");
    sf_settype(sxyfile,SF_COMPLEX);
    /* get dimension of the input file.  Override n2=n"dim" on sxyfile */
    diminput = sf_largefiledims(infile,nin);
    for (iaxis=1; iaxis<diminput; iaxis++){
      sprintf(parameter,"n%d",iaxis+1);
      sf_putint(sxyfile,parameter,(off_t)1);
      sf_putint(sxyfile,parameter,(off_t)1);
    }
    sf_fileflush(sxyfile,infile);
    for(indx=0; indx<num_sxy; indx++){
      float myfloat;
      myfloat=(float)sx[indx];
      sf_floatwrite(&myfloat,1,sxyfile);
      myfloat=(float)sy[indx];
      sf_floatwrite(&myfloat,1,sxyfile);
    }
  }

  if(NULL!=(gxyfile_name=sf_getstring("gxy"))){
    if(verbose>0){
	fprintf(stderr,"gxy=%s input.  Write the source xy coords.\n",
		gxyfile_name);
    }
    gxyfile=sf_output(gxyfile_name);
    sf_putint(gxyfile,"n1",num_gxy);
    sf_putfloat(gxyfile,"d1",1);
    sf_putfloat(gxyfile,"o1",0);
    sf_putstring(gxyfile,"label1","none");
    sf_putstring(gxyfile,"unit1","none");
    sf_settype(gxyfile,SF_COMPLEX);
    /* get dimension of the input file.  Override n2=n"dim" on gxyfile */
    diminput = sf_largefiledims(infile,nin);
    for (iaxis=1; iaxis<diminput; iaxis++){
      sprintf(parameter,"n%d",iaxis+1);
      sf_putint(gxyfile,parameter,(off_t)1);
      sf_putint(gxyfile,parameter,(off_t)1);
    }
    sf_fileflush(gxyfile,infile);
    for(indx=0; indx<num_gxy; indx++){
      float myfloat;
      myfloat=(float)gx[indx];
      sf_floatwrite(&myfloat,1,gxyfile);
      myfloat=(float)gy[indx];
      sf_floatwrite(&myfloat,1,gxyfile);
    }
  }
  
  if(verbose>0)
    fprintf(stderr,"read traces, find s/g xy, trace amp, n_traces=%d\n",
	    n_traces);

  num_traces=0;

  trace_shotloc=malloc(n_traces*sizeof(int));
  trace_grouploc=malloc(n_traces*sizeof(int));
  trace_amp=malloc(n_traces*sizeof(float));

  if (!sf_histfloat(infile,"o1",&o1))
    sf_error("input file does not define o1");
  if (!sf_histfloat(infile,"d1",&d1))
    sf_error("input file does not define d1");

  if(!sf_getfloat("starttime",&starttime))starttime=o1;
  /* start time to compute average trace ampltide */
  if(!sf_getfloat("endtime",&endtime))endtime=(n1_traces-1)*d1+o1;
  istarttime=(int)((starttime-o1)/d1+.5);
  iendtime  =(int)((endtime-o1)/d1+.5);
  if(istarttime<0)
    sf_error("user input starttime less that o1 of input file\n");
  if(istarttime>=n1_traces)
    sf_error("user input starttime greater that endtime of input file\n");
  if(iendtime  >=n1_traces)
    sf_error("user input endtime greater that endtime of input file\n");
  if(istarttime>iendtime)
    sf_error("user input starttime > endtime\n");

  /* end time to compute average trace amplitude */
  /* num_traces is a count of the number of traces with non zero headers
     it is zero when entering this loop and incremented each traces read 
     with a non zero header. function is_header_allzero is used to test
     and skip processing and counting the zero header traces.
  */
  for (i_trace=0; i_trace<n_traces; i_trace++){
    if(verbose>2 ||(verbose>0 && i_trace<5)){
      fprintf(stderr,"read trace and header i_trace=%d\n",i_trace);
    }

    /**************************/
    /* read trace and headers */
    /**************************/
    sf_seek(inheaders,  (off_t)i_trace*n1_headers*sizeof(float),  SEEK_SET);
    sf_floatread(header,n1_headers,inheaders);

    if(is_header_allzero(header,n1_headers))
      continue; /* skip processing all zero headers */

    sf_seek(infile,  (off_t)i_trace*n1_traces*sizeof(float),  SEEK_SET);
    sf_floatread(intrace,n1_traces,infile);

    /* build arrays trace_shotloc, trace_grouploc, trace_amp */
    if(typehead==SF_FLOAT){
      this_sx=header[indx_sx];
      this_sy=header[indx_sy];
      this_gx=header[indx_gx];
      this_gy=header[indx_gy];
    } else {
      this_sx=((int*)header)[indx_sx];
      this_sy=((int*)header)[indx_sy];
      this_gx=((int*)header)[indx_gx];
      this_gy=((int*)header)[indx_gy];
    }
    if(!binarysearch(this_sx,this_sy,
		     sx,sy,
		     num_sxy,&(trace_shotloc[num_traces])))
      sf_error("unable to find shot xy for trace %d\n",i_trace); 
    if(!binarysearch(this_gx,this_gy,
		     gx,gy,
		     num_gxy,&(trace_grouploc[num_traces])))
      sf_error("unable to find shot xy for trace %d\n",i_trace); 

    /* compute average amplitude of trace */
    trace_amp[num_traces]=0.0;
    num_samples=0;
    for(i1_traces=istarttime; i1_traces<=iendtime; i1_traces++){
      trace_amp[num_traces]+=fabs(intrace[i1_traces]);
      num_samples++;
    }
    trace_amp[num_traces]/=(float)num_samples;
    /* if(trace_amp[num_traces]>0.0)continue; skip zero traces */

    num_traces++;
  }

  /********************************************/
  /* compute surface consistent scalars using */
  /* trace_shotloc, trace_grouploc, trace_amp */
  /********************************************/
  /* could free sx,sy,gx,gy arrays, but they are needed to compute
     amp[xcmp][ycmp] */
  if(verbose>0)fprintf(stderr,"compute surface consistent scalars\n");

  /* compute the global average trace amplitude */
  global_average_trace_amp=0.0;
  num_nonzero_traces=0;
  for (inum_trace=0; inum_trace<num_traces; inum_trace++){
    if(trace_amp[inum_trace]>0.0){
      global_average_trace_amp+=trace_amp[inum_trace];
      num_nonzero_traces++;
    } 
  }
  global_average_trace_amp/=num_nonzero_traces;

  /* computation of shot_scale and group_scale could be linearized
     by taking log of trace_amp, then use iterative cg type method kls */
  /* compute shot_scale */
  shot_scale  =malloc(num_sxy*sizeof(float));
  shot_fold   =malloc(num_sxy*sizeof(float));
  /* computation of shot_scale might fit nicely into function kls */ 
  for (indx=0; indx<num_sxy; indx++){
    shot_scale[indx]=0.0;
    shot_fold[indx]=0.0;
  }
  for (inum_trace=0; inum_trace<num_traces; inum_trace++){
    if(trace_amp[inum_trace]>0.0){
      shot_scale  [trace_shotloc [inum_trace]]+=trace_amp[inum_trace];
      shot_fold   [trace_shotloc [inum_trace]]++;
    } 
  }
  for (indx=0; indx<num_sxy; indx++){
    if(shot_scale [indx]<1e-30)shot_scale[indx]=0.0;
    else                 shot_scale[indx]=global_average_trace_amp*
			                  shot_fold[indx]/shot_scale[indx];
  }

  /* apply the shot scale to trace_amp before computing group_scale */
  for (inum_trace=0; inum_trace<num_traces; inum_trace++){
    trace_amp[inum_trace]*=shot_scale[trace_shotloc [inum_trace]];
  }
  


  /* compute group_scale */
  group_scale =malloc(num_gxy*sizeof(float));
  group_fold  =malloc(num_gxy*sizeof(float));
  for (indx=0; indx<num_gxy; indx++){
    group_scale[indx]=0.0;
    group_fold[indx]=0.0;
  }   
  for (inum_trace=0; inum_trace<num_traces; inum_trace++){
    if(trace_amp[inum_trace]>0.0){
      group_scale [trace_grouploc[inum_trace]]+=trace_amp[inum_trace];
      group_fold  [trace_grouploc[inum_trace]]++;
    } 
  }
  for (indx=0; indx<num_gxy; indx++){
    if(group_scale[indx]<1e-30)group_scale[indx]=0.0;
    else                 group_scale[indx]=global_average_trace_amp*
			                   group_fold[indx]/group_scale[indx];
  }




  if(verbose>4){
    for (indx=0; indx<num_sxy; indx++){
      fprintf(stderr,"final shot_scale[%d]=%e shot_fold=%e\n",
	      indx,shot_scale[indx],shot_fold[indx]);
    }
    for (indx=0; indx<num_gxy; indx++){
      fprintf(stderr,"final group_scale[%d]=%e group_fold=%e\n",
	      indx,group_scale[indx],group_fold[indx]);
    }
  }

  if(verbose>0)
    fprintf(stderr,"check to see if sxyamp and/or gxyamp are to be output\n");

  if(NULL!=(sxyampfile_name=sf_getstring("sxyamp"))){
    if(verbose>0){
	fprintf(stderr,"sxyamp=%s input.  Write the source x,y,amp.\n",
		sxyampfile_name);
    }
    sxyampfile=sf_output(sxyampfile_name);
    sf_putint(sxyampfile,"n1",3);
    sf_putfloat(sxyampfile,"d1",1);
    sf_putfloat(sxyampfile,"o1",0);
    sf_putstring(sxyampfile,"label1","none");
    sf_putstring(sxyampfile,"unit1","none");

    sf_putint(sxyampfile,"n2",num_sxy);
    sf_putfloat(sxyampfile,"d2",1);
    sf_putfloat(sxyampfile,"o2",0);
    sf_putstring(sxyampfile,"label2","none");
    sf_putstring(sxyampfile,"unit2","none");
    sf_settype(sxyampfile,SF_FLOAT);
    /* get dimension of the input file.  Override n2=n"dim" on sxyfile */
    diminput = sf_largefiledims(infile,nin);
    for (iaxis=2; iaxis<diminput; iaxis++){
      sprintf(parameter,"n%d",iaxis+1);
      sf_putint(sxyampfile,parameter,(off_t)1);
      sf_putint(sxyampfile,parameter,(off_t)1);
    }
    sf_fileflush(sxyampfile,infile);
    for(indx=0; indx<num_sxy; indx++){
      float myfloat;
      myfloat=(float)sx[indx];
      sf_floatwrite(&myfloat,1,sxyampfile);
      myfloat=(float)sy[indx];
      sf_floatwrite(&myfloat,1,sxyampfile);
      myfloat=1.0/shot_scale[indx];
      sf_floatwrite(&myfloat,1,sxyampfile);
    }
  }

  if(NULL!=(gxyampfile_name=sf_getstring("gxyamp"))){
    if(verbose>0){
	fprintf(stderr,"gxyamp=%s input.  Write the group x,y,amp.\n",
		gxyampfile_name);
    }
    gxyampfile=sf_output(gxyampfile_name);
    sf_putint(gxyampfile,"n1",3);
    sf_putfloat(gxyampfile,"d1",1);
    sf_putfloat(gxyampfile,"o1",0);
    sf_putstring(gxyampfile,"label1","none");
    sf_putstring(gxyampfile,"unit1","none");

    sf_putint(gxyampfile,"n2",num_gxy);
    sf_putfloat(gxyampfile,"d2",1);
    sf_putfloat(gxyampfile,"o2",0);
    sf_putstring(gxyampfile,"label2","none");
    sf_putstring(gxyampfile,"unit2","none");
    sf_settype(gxyampfile,SF_FLOAT);
    /* get dimension of the input file.  Override n2=n"dim" on sxyfile */
    diminput = sf_largefiledims(infile,nin);
    for (iaxis=2; iaxis<diminput; iaxis++){
      sprintf(parameter,"n%d",iaxis+1);
      sf_putint(gxyampfile,parameter,(off_t)1);
      sf_putint(gxyampfile,parameter,(off_t)1);
    }
    sf_fileflush(gxyampfile,infile);
    for(indx=0; indx<num_gxy; indx++){
      float myfloat;
      myfloat=(float)gx[indx];
      sf_floatwrite(&myfloat,1,gxyampfile);
      myfloat=(float)gy[indx];
      sf_floatwrite(&myfloat,1,gxyampfile);
      myfloat=1.0/group_scale[indx];
      sf_floatwrite(&myfloat,1,gxyampfile);
    }
  }

  if(verbose>0) 
    fprintf(stderr,"read traces, apply surface consistant scale. n_traces=%d\n",
	    n_traces);

  inum_trace=0;
  for (i_trace=0; i_trace<n_traces; i_trace++){
    if(verbose>2 ||(verbose>0 && i_trace<5)){
      fprintf(stderr,"read trace apply scscale. i_trace=%d\n",i_trace);
    }
    
    /**************************/
    /* read trace and headers */
    /**************************/
    sf_seek(inheaders,  (off_t)i_trace*n1_headers*sizeof(float),  SEEK_SET);
    sf_floatread(header,n1_headers,inheaders);

    if(is_header_allzero(header,n1_headers))
      continue; /* skip processing all zero headers */

    sf_seek(infile,  (off_t)i_trace*n1_traces*sizeof(float),  SEEK_SET);
    sf_floatread(intrace,n1_traces,infile);

    if(1){
      /************************************/
      /* apply surface consistant scaling */
      /************************************/
      /* use inum_trace to understand traces dropped by is_header_allzero */
      for(i1_traces=0; i1_traces<n1_traces; i1_traces++){
	intrace[i1_traces]*=shot_scale  [trace_shotloc [inum_trace]]*
	  group_scale [trace_grouploc[inum_trace]];
      }
    }
    /***************************/
    /* write trace and headers */
    /***************************/
    /* trace and header will be preceeded with words:
       1- type record: 4 charactors 'tah '.  This will support other
          type records like 'htah', hidden trace and header.
       2- the length of the length of the trace and header. */
    put_tah(intrace, header, n1_traces, n1_headers, out);
    inum_trace++;
  }

  exit(0);
}

  

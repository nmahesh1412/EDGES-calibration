#include <stdio.h>
#include <stdlib.h>
#include <fftw3.h>
// Notes - float is faster intel_ipps is even faster
extern float *reamin0,*reamin1,*reamin2,*reamin3,*reamout0,*reamout1,*reamout2,*reamout3;

void fft_init(int n, int m, fftwf_plan *p)
{

         fftwf_complex *in, *out;
//         fftw_plan p;
//         in = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * N);
//         out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * N);

//     in = (fftwf_complex*)reamin;
//     out = (fftwf_complex*)reamout;

//printf("entering fft_init\n");
         in =  (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * n);
         out = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * n);
//printf("entering fft_init after malloc %p\n",in);
     *p = fftwf_plan_dft_1d(n, in, out, FFTW_FORWARD, FFTW_ESTIMATE);
if(m==0) {
     reamin0 = (float *)in;
     reamout0 = (float *)out;
     }
if(m==1) {
     reamin1 = (float *)in;
     reamout1 = (float *)out;
     }
if(m==2) {
     reamin2 = (float *)in;
     reamout2 = (float *)out;
     }
if(m==3) {
     reamin3 = (float *)in;
     reamout3 = (float *)out;
     }
// printf(" after malloc %p %p\n",ina,outa);

//         ...
//         fftwf_execute(p); /* repeat as needed */
//         ...
//         fftwf_destroy_plan(p);
//         fftwf_free(in); fftwf_free(out);
     }

void fft_free(int m, fftwf_plan *p)
{
  fftwf_destroy_plan(*p);
  if(m==0){ fftwf_free((fftwf_complex*)reamin0); fftwf_free((fftwf_complex*)reamout0); }
  if(m==1){ fftwf_free((fftwf_complex*)reamin1); fftwf_free((fftwf_complex*)reamout1); }
  if(m==2){ fftwf_free((fftwf_complex*)reamin2); fftwf_free((fftwf_complex*)reamout2); }
  if(m==3){ fftwf_free((fftwf_complex*)reamin3); fftwf_free((fftwf_complex*)reamout3); }
}


void cfft(fftwf_plan *p)
 {
     fftwf_execute(*p);
 }

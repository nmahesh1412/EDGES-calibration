#include <stdlib.h>
#include <errno.h>
#include <stdio.h>
#include <sys/io.h>
#include <math.h>
#include <time.h>
#include <string.h>
#include <sched.h>
#include <fcntl.h>
#include <complex.h>
#define PI 3.1415926536

void plotfspec(int,int,int,double*,double*,double*);
double sim(double, double, complex double,double,double);
double balun(complex double,double,double,double);
double polyfitr(int, int, double*, double*, double *, double *, double *);
double fitfun(int,int,int,double*);
void MatrixInvert(int);
double rmscalc2(int,double *,double*);

int readdata(double *,double *,double *);

char fname[256]; 
static double inverr;


static long double aarr[10000];    // long double needed for accurate matrix inversion
static double bbrr[10000];
static double brrr[10000];
static long double arrr[10000];
static double mcalc[65536*20];

int main(int argc, char *argv[])
{ int n,jj,i,pfit,plot;
  int nt,npp,mpoly,nfreq,open,fit;
  double delay,spind,freq,phi,tsky,cf,ccf,fstep,cal2,cal3,cal4,a,fre;
  double fstart,fstop,gnd,frst,frstp,frstep,tamb,tcal,tpeak,t150;
  static double freqq[100000],fitfn[200000],data[100000],dataout[100000],wtt[100000],polycfr[100],polycfi[100];
  static double ffreq[100000],ddata[100000],wt[100000];
  static double galdn[100000],tload[100000],cal[100000],fitf[10000];
  static double r4_ant[] = {18,10,6.6,5.7,8.2,15,31,53,67,63.6,52,44,40,36.8,37.3,41,46.5};  // 17.5" ant Balun short Anritsu 23 Oct 10 on radar plate
  static double i4_ant[] = {-123,-88,-61,-40,-19.4,-1.5,15,17,0.7,-10,-13.7,-9.6,-2.9,6.4,17.7,28,43};
  static double rr_ant[100000],ii_ant[100000];
  complex double Zin,tf;
  char buf[256];

 plot=2; npp=0; gnd=0; tamb=300.0; tcal = 1000.0;
 open=0; fit=0; cal3 = 0;
 cal2 = -13; cal4 = -10;
 mpoly = 15; inverr = -1e99; 
 for(i=0;i<argc;i++){
  sscanf(argv[i], "%79s", buf);
  if (strstr(buf, "-open")) { sscanf(argv[i+1], "%d",&open);}
  if (strstr(buf, "-pfit")) { sscanf(argv[i+1], "%d",&fit);}
  if (strstr(buf, "-f")) {sscanf(argv[i+1], "%s", fname); npp = readdata(ffreq,ddata,wt); }
       }
  fstart = 80+10; fstop = 160-10; fre = 150;
  if(npp) {fstart = ffreq[0]; fstop = ffreq[npp-1]; fre = ffreq[npp/2]; }
  fstep = 0.5;
  if(npp) fstep = (fstop-fstart)/npp;
  printf("npp %d fstep %f\n",npp,fstep);
  nt=nfreq=0;
  n=0;
  frst=50;frstp=210; frstep=10;
  for(freq=frst;freq<=frstp;freq+=frstep){ 
    rr_ant[n] = r4_ant[n];
    ii_ant[n] = i4_ant[n];
    wtt[n] = 1;
    printf("n %d freq %3.0f r_ant %f i_ant %f\n",n,freq,rr_ant[n],ii_ant[n]);
    freqq[n] = freq;
    n++;
  }
  nt = n;
  for(i=0;i<nt;i++){
     fitf[i] = 1;
   for(jj=1;jj<mpoly;jj++) {
      if(jj%2==1) fitf[i+jj*nt] = cos(PI*(freqq[i] - fre)*((jj+1)/2)/(frstp-frst));
      else         fitf[i+jj*nt] = sin(PI*(freqq[i] - fre)*((jj+1)/2)/(frstp-frst));
     }
  }
  pfit = mpoly;   // use Fourier series fit to impedance 
  polyfitr(pfit,n,rr_ant,mcalc,wtt,dataout,fitf);
  printf("rms_real %5.1f\n",rmscalc2(n,rr_ant,wtt));
  for(i=0;i<pfit;i++) polycfr[i]=bbrr[i];
  polyfitr(pfit,n,ii_ant,mcalc,wtt,dataout,fitf);
  printf("rms_imag %5.1f\n",rmscalc2(n,ii_ant,wtt));
  for(i=0;i<pfit;i++) polycfi[i]=bbrr[i];
  t150 = 300.0; spind = -2.5;
  for(jj=-2;jj<1+fit;jj++){
  n=0; 
  for(freq=fstart;freq<=fstop;freq+=fstep){
  rr_ant[n] =  polycfr[0];
  ii_ant[n] =  polycfi[0];
  for(i=1;i<pfit;i++) {
                 if(i%2==1) a = cos(PI*(freq - fre)*((i+1)/2)/(frstp-frst));
                 else       a = sin(PI*(freq - fre)*((i+1)/2)/(frstp-frst));
                       rr_ant[n] += polycfr[i]*a;
                       ii_ant[n] += polycfi[i]*a;
   }

  Zin = 0;
  cf = 1.0+cal4*0.01; 
  ccf = 1.0+cal2*0.01;

  tsky=300.0*pow(freq/150.0,spind -0.12*log(freq/150.0)) + 3.0 + gnd;

  if(jj >= 0) {
         t150 = 300.0;
         if(jj==1) tsky = t150*pow(freq/150.0,spind -0.12*log(freq/150.0)) + 3.0 + gnd;   // galaxy reference
         if(jj==2) tsky = t150*pow(freq/150.0,spind-0.01 -0.12*log(freq/150.0)) + 3.0 + gnd;  // derivative with spectral index
         Zin = rr_ant[n] + I*ii_ant[n];
         tsky = balun(Zin,tsky,freq,tamb);
         tf = (Zin - 50.0)/(Zin + 50.0);

  if(open){
           tsky = tamb;
           delay = 15.1e-9;   // end of cable is critical
           tf = pow(10.0,-0.05*((0.242080*sqrt(freq)+0.000330*freq)*delay*0.983e7*0.84)); // measured 0.94 at 150 MHz
//           printf("freq %f tf %f\n",freq,creal(tf));
           phi = -2.0*PI*freq*1e06*delay;
           tf = tf*(cos(phi)+I*sin(phi));   // LMR-240  3db/100ft 150 MHz
           if(open==2) tf=1.0;   // open at connector
           if(open==3) tf=-1.0;  // short at connector
           if(open==4) {tsky = t150 = 1200; tf=0;}   // for test noise fed into EDGES
            }

         delay = 1.1e-9+1e-9*cal3;   // cable delay inside LNA box
         tf = tf*pow(10.0,-0.05*((0.242080*sqrt(freq)+0.000330*freq)*delay*0.983e7*0.84+0.1));  // small added loss for switch and bends
         phi = -2.0*PI*freq*1e06*delay;
         tf = tf*(cos(phi)+I*sin(phi));   // LMR-240  3db/100ft 150 MHz
         Zin = 50.0*(tf+1.0)/(1.0-tf);
            }
  if(jj==-1) {  Zin = 50.0;  tsky = tamb; }  // 3-pos sw
  if(jj==-2) {  Zin = 50.0;  tsky = tamb + tcal;}  // 3-pos 
  data[n] = sim(freq,tsky/tamb,Zin,cf,ccf);    // simulate LNA
  if(jj==-2) cal[n]=data[n];
  if(jj==-1) tload[n]=data[n];
  if(jj==0) galdn[n]=tcal*((data[n]-tload[n])/(cal[n]-tload[n]))+tamb;  // can be used as test data
  if(jj==1) fitfn[n] = tcal*((data[n]-tload[n])/(cal[n]-tload[n]))+tamb;
  if(jj==2) fitfn[n+(jj-1)*nfreq] = fitfn[n]-(tcal*((data[n]-tload[n])/(cal[n]-tload[n]))+tamb); // include spectral index
  if(jj >= 3)  fitfn[n+(jj-1)*nfreq] = pow(freq-fre,jj-1); // first term curvature 
  wtt[n] = 1;
  freqq[n] = freq;
  n++;
  }
  nfreq = n;
  }
 for(i=0;i<n;i++) data[i] = ddata[i];
 if(fit){
 polyfitr(fit,n,data,mcalc,wt,dataout,fitfn);
 tpeak = bbrr[0]*t150;
 t150=tpeak;
 spind = spind+bbrr[1]*0.01;
 printf("fit t150(K) %4.0f specindex %8.3f rms %8.3f\n",tpeak,spind,rmscalc2(n,data,wt));
 for(i=0;i<n;i++) data[i] = dataout[i];
 }
 else  for(i=0;i<n;i++) data[i] = galdn[i];
 plotfspec(n,npp,plot,freqq,data,ddata);
  return 0;
}

double balun(complex double Zin, double tsky, double freq, double tamb)
{ complex double Z1,Z2,Z3,Z0;
  double len,rt;
// cicuit in memo 43
// approximation of balun loss and temperature contribution
// note that the antenna impedance is measured via the balun
// and so the impedance seen by the LNA is not changed
         len = 7;   // cable length in balun in cm
         Z1=1.0/(1.0/900.0 + I*2.0*PI*freq*1e06*1e-10*len/(3.0*50.0*0.7*2.0)); // 1/2 cap of len cm 50 ohm cable
         Z2 = 0.0 + I*2.0*PI*freq*1e06*1e-10*len*50.0/(3.0*0.7);  // inductane of len cm 50 ohm cable 0.7 velocity factor
         Z3 = 1.0/(I*2.0*PI*freq*1e06*1e-10*len/(3.0*50.0*0.7*2.0)); // 1/2 cap of len cm 50 ohm cable
         Z0 = (Z1*Z2*Z3 - Zin*Z1*Z2 - Zin*Z1*Z3)/(Zin*(Z1+Z2+Z3)-Z1*Z3-Z2*Z3);
         if(creal(Z0)< 1.0) Z0 = 1.0 + I*cimag(Z0);
         rt = sqrt(creal(Z0)/creal(Zin))*cabs(Z3 * (Z1 / (Z1 + Z2 + Z3)) * (1.0 / (Z0 + 1.0/(1.0/Z1 + 1.0/(Z2+Z3)))));
         tsky = tsky *rt*rt;  // attenuation of sky signal
         rt = sqrt(creal(Z1)/creal(Zin))*cabs(Z3 * (Z0 / (Z0 + Z2 + Z3)) * (1.0 / (Z1 + 1.0/(1.0/Z0 + 1.0/(Z2+Z3)))));
         tsky += tamb * rt * rt; // added noise from balun
         return tsky;
}

double sim(double freq, double tsky, complex double ZZin,double cf, double ccf)
{ int i;
  double vgain,Nin,Ng,Nf,Ns,No,Nout;
  double pout[10],pwr,w,tau,c,L,cg,cout,Lin,rin,Cin,rinc;
  complex double g,Vout,zin,Zin,Zout,V,Rg,Rf,Ro,Rs,ii,y,Zin0,Z1,Z2,Z3;
  complex double a00,a01,a02,a10,a11,a12,a20,a21,a22;
  complex double aa00,aa01,aa02,aa10,aa11,aa12,aa20,aa21,aa22;
  complex double b0,b1,b2,d;
// freq in MHz,tsky in ratio to ambient,cf scale factor on nominal tranconductance,cff scale factor on gate drain cap
  vgain =0;
  zin = 0;
  w = freq*1e6*2.0*PI;
  c = 0.65e-12*ccf;  // gate drain capacitance
  tau = 12.0e-12;    // HEMT delay
  L = 0.4e-9;        // HEMT source inductance
  Rs = 0.5+I*w*L;    
  cout = 1.2e-12;
  Ro = 1.0/(1.0/150.0 + I*w*cout);
  cg = 1.12e-12;
  y= 1.0/(0.0 + I*w*cg);
  Zout = 30.0 + 50.0;
  Rg = 2.5;
  g = 0.41*cf -I*sin(w*tau); // HEMT tranconductance
  Rf = 1.0 /(1.0/1200.0 + I*w*c); // feedback to get good match
  Lin = 270e-9;  // 270 nH on input may be increased in future 
  Cin = 100e-12; // 100 pf coupling may be increased in future
  rin = 1.0; // resistance of 270nH   1 ohm ac240
  rinc = 1.0; // resistance of 100pf   1 ohm ac240
// notes: keep zs low, r = 1k for 50ohm match,keep cg low,
// raise rout and r for good noise and input match
// see memo 62 for circuit model 
  for(i=0;i<10;i++){  // gain,Nin,Ng,Nf,Ns,No,Nout,rs,rp
  Zin = ZZin;
    // resistors at ambient
  Zin0 = Zin;
  Z1 = rin + I*w*Lin;
  Z3 = Z1;
  Z1 = 1.0/(1.0/Z1 + I*w*1.8e-12);  // 2 x 0.9 pf hp5082-2810
  Z2 = rinc + 1.0/(I*w*Cin);  // see input circuitry of EDGES LNA
  Zin = 1.0/(1.0/Zin + 1.0/Z1);
  Zin = Zin + Z2;
  Zin = 1.0/(1.0/Zin + 1.0/Z3);
  Nin=Ng=Nf=Ns=No=Nout=0;
  if(i==0) Nin = 1; 
  if(i==1){
          Nin = 2.0*sqrt(tsky)*sqrt(creal(Zin0));  // tsky is ratio to tambient
          Nin = Nin * cabs(Z3 * (Z1 / (Z1 + Z2 + Z3)) * (1.0 / (Zin0 + 1.0/(1.0/Z1 + 1.0/(Z2+Z3)))));
   }
  if(i==2) Ng = 2.0*sqrt(creal(Rg));
  if(i==3) Nf = 2.0*sqrt(creal(Rf));
  if(i==4) Ns = 2.0*sqrt(creal(Rs));
  if(i==5) No = 2.0*sqrt(creal(Ro));
  if(i==6) Nout = 2.0*sqrt(creal(Zout));
  if(i==7) { Nin = 2.0*sqrt(creal(Z1));
             Nin = Nin * cabs(Z3 * (Zin0 / (Zin0 + Z2 + Z3)) * (1.0 / (Z1 + 1.0/(1.0/Zin0 + 1.0/(Z2+Z3)))));
           }
  if(i==8) { Nin = 2.0*sqrt(creal(Z2));
             Nin = Nin * cabs(Z3 * (1.0 / (Z2 + Z3 + 1.0/(1.0/Zin0 + 1.0/Z1))));
           }
  if(i==9) { Nin = 2.0*sqrt(creal(Z3));
             Nin = Nin * cabs((Z2 + 1.0/(1.0/Zin0+1.0/Z1)) * (1.0 / (Z2 + Z3 + 1.0/(1.0/Zin0 + 1.0/Z1))));
           }

  a00 = 1.0/Ro;                     a01 =  -1.0 -(Rs/Ro);   a02 = g+(1.0/y);
  a10 = -1.0/Zout - 1.0/(Rf+Zin);   a11 = -1.0;             a12 = -(Rf/y)/(Rf+Zin);
  a20 = 1.0 - Rf/(Rf+Zin);          a21 = -Rs;              a22 = -1.0 - (Rf/y)*(Rf/(Rf+Zin)) + (Rf/y) - Rg/y;

  b0 = (No + Ns)/Ro; b1 = -Nout/Zout -(Nf + Nin)/(Rf+Zin); b2 = Nf + Ng + Ns - (Nf + Nin)*Rf/(Rf+Zin);

    d = a00*a11*a22 + a10*a21*a02 + a20*a01*a12 - a20*a11*a02 - a10*a01*a22 - a21*a12*a00;
    
// remember to transpose
    aa00 =  (a11*a22-a21*a12)/d;
    aa01 = -(a01*a22-a21*a02)/d;
    aa02 =  (a01*a12-a11*a02)/d;
    aa10 = -(a10*a22-a20*a12)/d;
    aa11 =  (a00*a22-a20*a02)/d;
    aa12 = -(a00*a12-a10*a02)/d;
    aa20 =  (a10*a21-a20*a11)/d;
    aa21 = -(a00*a21-a20*a01)/d;
    aa22 =  (a00*a11-a10*a01)/d;

    Vout = aa00*b0 + aa01*b1 + aa02*b2;
    ii   = aa10*b0 + aa11*b1 + aa12*b2;
    V   = aa20*b0 + aa21*b1 + aa22*b2;

  if(i==0) { 
        vgain = 10.0*log10(cabs(Vout)*cabs(Vout))+6.0;
        zin = (V+(V/y)*Rg+ii*Rs)/(ii+Vout/Zout);
        zin = 1.0/(1.0/zin + 1.0/(rin+I*w*Lin)) + rinc + 1.0/(I*w*Cin);
        zin = 1.0/(1.0/zin + 1.0/(rin+I*w*Lin));
//        printf("freq %3.0f zin %3.0f + %3.0fj vgain %3.1f dB\n",freq,creal(zin),cimag(zin),vgain);
  }
  pout[i] = cabs(Vout)*cabs(Vout);
//  printf("i %d pout %f\n",i,pout[i]);
  }
  pwr = pout[1]+pout[2]+pout[3]+pout[4]+pout[5]+pout[6]+pout[7]+pout[8]+pout[9];
// power returned has arbitrary scale as scale cancels in 3-pos switch
  return pwr;
}

void plotfspec(int np, int npp, int nter, double freqq[], double data[],double data2[])
  // plot the spectrum 
{
    char txt[256];
    int k, kk,iter;
    double x, y, xp, yp, dmax, dmin, f, totpp, scale, step, h, s, b;
    double xoffset,yoffset,fstart,fstop;
    time_t now;
    FILE *file;

    if ((file = fopen("spe.pos", "w")) == NULL) {
        printf("cannot open spe.pos:\n");
        return;
    }
    fprintf(file, "%%!PS-Adobe-\n%c%cBoundingBox:  0 0 612 700\n%c%cEndProlog\n", '%', '%', '%', '%');
    fprintf(file, "1 setlinewidth\n");
//    fprintf(file, "2 setlinewidth\n");
    fprintf(file, "/Times-Roman findfont\n 12 scalefont\n setfont\n");

    xoffset = 80.0;
    yoffset = 100.0;
    fstart = freqq[0];
    fstop = freqq[np-1];
    dmax = 400.0;
    dmin = 0;
    if(nter < 3){
    dmax = -1e99; dmin= 1e99;
    for(k=0;k<np;k++) if(data[k] > dmax) dmax = data[k];
    for(k=0;k<np;k++) if(data[k] < dmin) dmin = data[k];
//    for(k=0;k<npp;k++) if(data2[k] > dmax) dmax = data2[k];
//    for(k=0;k<npp;k++) if(data2[k] < dmin) dmin = data2[k];
    }
    dmax = dmax*1.5;
    if(dmax < 1000) dmax = 1000;
    dmin = 0;
    scale = dmax-dmin;
    printf("dmax %f dmin %f\n",dmax,dmin);
    for (y = 0; y < 2; y++) 
    fprintf(file, "newpath\n %6.2f %6.2f moveto\n %6.2f %6.2f lineto\n 0 0 0 sethsbcolor stroke\n",
             xoffset, y * 480 + yoffset, xoffset + 400.0, y * 480 + yoffset);
    fprintf(file, "newpath\n %6.2f %6.2f moveto\n %6.2f %6.2f lineto\n 0 0 0 sethsbcolor stroke\n",
      xoffset, yoffset, xoffset, 480.0 + yoffset);
    fprintf(file, "newpath\n %6.2f %6.2f moveto\n %6.2f %6.2f lineto\n 0 0 0 sethsbcolor stroke\n",
      xoffset + 400.0, yoffset, xoffset + 400.0, 480.0+yoffset);
    fprintf(file, "newpath\n %6.2f %6.2f moveto\n %6.2f %6.2f lineto\n 0 0 0 sethsbcolor stroke\n",
     xoffset + 0.0, yoffset, xoffset + 0.0, 480.0+yoffset);
    for(iter=0;iter<nter;iter++){
    if(iter==1 && npp) np = npp;
    fprintf(file, "%d setlinewidth\n",iter+1*0);
    yp = 0;
    xp = 0;
    totpp = 0;
    kk = 0;
    for (k = 0; k < np; k++) {
        h=s=b=0;
        x = k * 400.0 / (double) np;
        if (scale > 0.0) {
            if(iter==0)totpp = data[k];
            if(iter==1)totpp = data2[k];
        }
        totpp = (totpp - dmin) / scale;
        y = totpp * 480.0;
        kk = 1;
        if (y < 0)
            y = 0;
        if (y > 480)
            y = 480;
        if (k == 0)
            yp = y;
        if (y != yp) {
            if(kk) fprintf(file, "newpath\n %6.2f %6.2f moveto\n %6.2f %6.2f lineto\n %5.2f %5.2f %5.2f sethsbcolor stroke\n",
            xp + xoffset,yp + yoffset,x + xoffset, yp + yoffset,h,s,b);
            xp = x;
            if (y > yp) {
                if(kk) fprintf(file, "newpath\n %6.2f %6.2f moveto\n %6.2f %6.2f lineto\n  %5.2f %5.2f %5.2f sethsbcolor stroke\n",
                 x + xoffset,yp+yoffset,x + xoffset,y+yoffset,h,s,b);
            }
            if (yp > y) {
                if(kk) fprintf(file, "newpath\n %6.2f %6.2f moveto\n %6.2f %6.2f lineto\n %5.2f %5.2f %5.2f sethsbcolor stroke\n",
               x + xoffset,y+yoffset,x + xoffset,yp+yoffset,h,s,b);
            }
        }
        yp = y;
    } if(kk) fprintf(file, "newpath\n %6.2f %6.2f moveto\n %6.2f %6.2f lineto\n 0 0 0 sethsbcolor stroke\n",
        xp + xoffset,yp+yoffset,400.0 + xoffset,yp+yoffset);
    }
    fprintf(file, "1 setlinewidth\n");
    step = 20.0;
//    i = (int) (fstart / fstep + 1.0);
    for (f = fstart; f <= fstop; f += step) {
        x = (f - fstart) * 400.0 / (fstop-fstart);
        y = 0.0;
        fprintf(file, "newpath\n %6.2f %6.2f moveto\n %6.2f %6.2f lineto\n 0 0 0 sethsbcolor stroke\n",
         x + xoffset,y+yoffset,x + xoffset,y - 10.0+yoffset);
        if(step == 10.0) sprintf(txt, "%5.1f", f);
        else sprintf(txt, "%6.2f", f);
        fprintf(file, "%6.2f %6.2f moveto\n (%s) show\n",x + xoffset - 15.0,y - 20.0+yoffset, txt);
    } 
   {
    for (f = dmin; f <dmax; f += (dmax-dmin)*0.1) {
        x = 0;
        y = (f-dmin) * 480.0 / (dmax-dmin);
        fprintf(file, "newpath\n %6.2f %6.2f moveto\n %6.2f %6.2f lineto\n 0 0 0 sethsbcolor stroke\n",
           x + xoffset,y+yoffset,x + xoffset + 5,y+yoffset);
        if(dmax-dmin<10.0)
            sprintf(txt, "%4.0f mK", f*1e03);
        else
            sprintf(txt, "%4.0f K", f);
        fprintf(file, "%6.2f %6.2f moveto\n (%s) show\n",x + xoffset - 50.0,y - 1.0+yoffset, txt);
    } 
    }
    sprintf(txt, "freq(MHZ)");
    fprintf(file, "%6.2f %6.2f moveto\n (%s) show\n",xoffset + 180.0, 65.0, txt);
    sprintf(txt,
     "fstart %3.0f fstop %3.0f ", 
                  fstart, fstop);
    fprintf(file, "%6.2f %6.2f moveto\n (%s) show\n",0.0+xoffset, 50.0, txt);
    now = time(NULL);
    fprintf(file,"%d %d moveto\n (%s) show\n",450,20,ctime(&now));
    fprintf(file, "showpage\n%c%cTrailer\n", '%', '%');
    fclose(file);
} 

int readdata(double ffreq[],double ddata[],double wt[])
{   char buf[256];
    int j;
    FILE *file1;
    if ((file1 = fopen(fname, "r")) == NULL) {
        printf("cannot open file:%s\n", fname);
        return 0;
    }
    printf("file %s\n", fname);
    j = 0;
    while (fgets(buf, 256, file1) != 0) {
           sscanf(buf, "%lf %lf %lf", &ffreq[j], &ddata[j], &wt[j]);
           j++;
           }
 fclose(file1);
 return j;
}

double polyfitr(int npoly, int nfreq, double ddata[],double mcalc[], double wtt[], 
                double dataout[], double fitfn[])
{
    int i, j, k, kk, m1, m2;
    double re, max, dd;
    long double rre,rrre;
    for (i = 0; i < nfreq; i++) {
        kk = i * npoly;
        for (j = 0; j < npoly; j++) {
            mcalc[kk] = fitfun(j,i,nfreq,fitfn);
            kk++;
        }
    }
    for (i = 0; i < npoly; i++) {
        re = 0.0;
        m1 = i;
        for (k = 0; k < nfreq; k++) {
            dd=ddata[k];
            if(wtt[k]>0) re += mcalc[m1] * dd * wtt[k];
            m1 += npoly;
        }
        brrr[i]=bbrr[i] = re;
        for (j = 0; j <= i; j++) {
            re = 0.0;
            m1 = i;
            m2 = j;
            for (k = 0; k < nfreq; k++) {
                if(wtt[k] > 0) re += mcalc[m1] * mcalc[m2] * wtt[k];
                m1 += npoly;
                m2 += npoly;
            }
            k = j + (i * (i + 1)) / 2;

//     if(i==j) re += 1e-20;     // matrix inversion problem 
            aarr[k] = arrr[k] = re;
//  printf("k %d %d %d aarr %Le\n",k,j,i,aarr[k]);
        }
    }
    MatrixInvert(npoly);
    max = 0;
    for (i = 0; i < npoly; i++) {
    for (j = 0; j <= i; j++) {
        rre = 0;
    for (m1 = 0; m1 < npoly; m1++) {
        if(m1 <= i) k = m1 + (i * (i + 1)) / 2;
        else k = i + (m1 * (m1 + 1)) / 2;
        if(j < m1) kk = j + (m1 * (m1 + 1)) / 2;
        else  kk = m1 + (j * (j + 1)) / 2;
        rre += aarr[kk]*arrr[k];
    }
//        if(i==j) printf("diag %d %Le\n",i,rre);
//        if(i != j) printf("off %d %Le\n",i,rre);
    if(i==j) max += (rre-1.0L)*(rre-1.0L);
    else max += rre*rre;
    }}
    printf("maxtrix inv err %e\n",sqrt(max));
//    if(sqrt(max) > inverr) inverr = sqrt(max);

    max = rrre = 0;
    for (i = 0; i < npoly; i++) {
        rre = 0;
    for (m1 = 0; m1 < npoly; m1++) {
        if(m1 <= i) k = m1 + (i * (i + 1)) / 2;
        else k = i + (m1 * (m1 + 1)) / 2;
        rre += arrr[k]*bbrr[m1];
    }
    max += (rre-brrr[i])*(rre-brrr[i]);
    rrre += brrr[i]*brrr[i];
    }
    printf("maxtrix soln err %e\n",sqrt(max/rrre));
    if(sqrt(max/rrre) > inverr) inverr = sqrt(max/rrre);

    for (i = 0; i < nfreq; i++) {
        re = 0.0;
        for (j = 0; j < npoly; j++) {
            re += bbrr[j] * fitfun(j,i,nfreq,fitfn);
        }
        dd=re;
        dataout[i] = dd;
        ddata[i] = ddata[i] - dd + 0.0;
        if(ddata[i] > 1e99) ddata[i] = 1e99;
        if(ddata[i] < -1e99) ddata[i] = -1e99;
         }
//        printf("ddata %f %f\n", ddata[i], re);
    return pow(10.0,bbrr[0]);
}

double fitfun(int j, int i, int nfreq, double fitfn[])
{
return fitfn[i + j*nfreq];;
}

void MatrixInvert(int nsiz)
{
    int ic0, id, i, j, ij, ic, n;
    long double re, mag, sumr;
    long double ttrr[1000];
    aarr[0] = 1.0L / aarr[0];
    bbrr[0] = bbrr[0] * aarr[0];

    /* inversion by bordering */
    for (n = 1; n <= nsiz - 1; n++) {
        ic0 = (n * (n + 1)) / 2;
        id = ic0 + n;
        for (i = 0; i < n; i++) {
            sumr = 0.0L;
            ij = (i * (i + 1)) / 2; /* A(I,0) */
            for (j = 0; j < n; j++) {
                ic = ic0 + j;
                sumr += aarr[ic] * aarr[ij];
                if (j < i)
                    ij++;
                if (j >= i)
                    ij += j + 1;
            }
            ttrr[i] = sumr;
        }
        sumr = 0.0L;
        for (i = 0; i < n; i++) {
            ic = ic0 + i;
            sumr += ttrr[i] * aarr[ic];
        }
        re = aarr[id] - sumr;
        mag = re * re;
        if (mag > 0.0L)
            aarr[id] = re / mag;

        else {
            aarr[id] = 0.0L;
printf("ffre reached zero\n");
             }
        sumr = 0.0L;
        for (i = 0; i < n; i++) {
            ic = ic0 + i;
            sumr += aarr[ic] * bbrr[i];
        }
        bbrr[n] = aarr[id] * (bbrr[n] - sumr);
        for (i = 0; i < n; i++)
            bbrr[i] += -ttrr[i] * bbrr[n];
        for (i = 0; i < n; i++) {
            ic = ic0 + i;
            aarr[ic] = -aarr[id] * ttrr[i];
        }
        ij = 0;
        for (i = 0; i < n; i++) {
            for (j = 0; j <= i; j++) {
                ic = ic0 + j;
                aarr[ij] += -ttrr[i] * aarr[ic];
                ij++;
            }
        }
    }
}

double rmscalc2(int np, double data[], double wt[])
{
   double b,c;
   int i;
   b  = c = 0;
   for(i=0;i<np;i++){
   if(wt[i] > 0){
   b += data[i]*data[i]*wt[i];
   c += wt[i];
    }
   }
   if(c > 0) {
     return sqrt(b/c);
   }
   else return 0;
}


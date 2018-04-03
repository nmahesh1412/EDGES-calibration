#include <gtk/gtk.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <time.h>
#include <sched.h>
#include <math.h>
#include <sys/io.h>
#include <fcntl.h>
#include <pthread.h>
#include <errno.h>
#include <signal.h>
#include "d1typ6.h"
#include "d1proto6.h"
#include "stdafx.h"
#include <fftw3.h>

#define NSIZ 65536
#define MY_PX14400_BRD_NUM              1

GdkPixmap *pixmap = NULL;
GdkFont *fixed_font;
GtkWidget *table;
GtkWidget *button_exit;
GtkWidget *drawing_area;
float avspec[NSIZ];
float win0[NSIZ*2],win1[NSIZ*2];
float win2[NSIZ*2],win3[NSIZ*2];
//double reamin0[NSIZ*4],reamin1[NSIZ*4],reamout0[NSIZ*4],reamout1[NSIZ*4];
//float reamin0[NSIZ*4],reamin1[NSIZ*4],reamout0[NSIZ*4],reamout1[NSIZ*4];
//float reamin2[NSIZ*4],reamin3[NSIZ*4],reamout2[NSIZ*4],reamout3[NSIZ*4];
float spec0[NSIZ],spec1[NSIZ];
float spec2[NSIZ],spec3[NSIZ];
int midx, midy;
HPX14 hBrd;
px14_sample_t *dma_bufp;
fftwf_plan p0,p1,p2,p3;
float *reamin0,*reamin1,*reamin2,*reamin3,*reamout0,*reamout1,*reamout2,*reamout3;
d1type d1;


void outfile(double *,int,int);
void *runspec(void *);
void px14run(float*,int);
int pxrun(int,px14_sample_t *);

void parport(int pdata)
{
#define DATA 0x378
 int i;
  if(pdata == -1) {
   if (ioperm(DATA,3,1)) {
        printf("Sorry, you were not able to gain access to the ports\n");
        printf("You must be root to run this program\n");
        exit(1);
        }
    }
  else {
    i = 0;  // ant
    if(pdata == 1) i = 2;  // load
    if(pdata == 2) i = 3;  // load + cal
    outb(i,DATA); /* Sends  to the Data Port */
    usleep(100000); // sleep for 0.1 sec
  }
}

/* Convert to Seconds since New Year 1970 */
double
tosecs (int yr, int day, int hr, int min, int sec)
{
  int i;
  double secs;
  secs = (yr - 1970) * 31536000.0 + (day - 1) * 86400.0
    + hr * 3600.0 + min * 60.0 + sec;
  for (i = 1970; i < yr; i++)
    {
      if ((i % 4 == 0 && i % 100 != 0) || i % 400 == 0)
	secs += 86400.0;
    }
  if (secs < 0.0)
    secs = 0.0;
  return secs;
}


/* Convert Seconds to Yr/Day/Hr/Min/Sec */
void
toyrday (double secs, int *pyear, int *pday, int *phr, int *pmin, int *psec)
{
  double days, day, sec;
  int i;

  day = floor (secs / 86400.0);
  sec = secs - day * 86400.0;
  for (i = 1970; day > 365; i++)
    {
      days = ((i % 4 == 0 && i % 100 != 0) || i % 400 == 0) ? 366.0 : 365.0;
      day -= days;
    }
  *phr = (int)(sec / 3600.0);
  sec -= *phr * 3600.0;
  *pmin = (int)(sec / 60.0);
  *psec = (int)(sec - *pmin * 60);
  *pyear = i;
  day = day + 1;
  *pday = (int)day;
  if (day == 366)		// fix for problem with day 366
    {
      days = ((i % 4 == 0 && i % 100 != 0) || i % 400 == 0) ? 366 : 365;
      if (days == 365)
	{
	  day -= 365;
	  *pday = (int)day;
	  *pyear = i + 1;
	}
    }
}

int
dayofyear (int year, int month, int day)
{
  int i, leap;
  int daytab[2][13] = {
    {0, 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31},
    {0, 31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31}
  };
  leap = (year % 4 == 0 && year % 100 != 0) || year % 400 == 0;
  for (i = 1; i < month; i++)
    day += daytab[leap][i];
  return (day);
}

double
readclock (void)
{
  time_t now;
  double secs;
  struct tm *t;
  now = time (NULL);
  t = gmtime (&now);
// gmtime Jan 1 is day 0
  secs =
    tosecs (t->tm_year + 1900, t->tm_yday + 1, t->tm_hour, t->tm_min,
	    t->tm_sec);
  return (secs);
}

static void handler(int signum) {
   printf("SIG %d received\n",signum);
   printf("program will end at the end of the 3-pos cycle\n");
   printf("to terminate gracefully from kill use kill -s SIGINT\n");
   d1.run = 0;
}

int main(int argc, char **argv)
{
        static double data[3*NSIZ];
        static float spec[NSIZ];
        double max;
        int i,kk,maxi,run,nspec,nrun,nblock,pport;
        double av,freq,aa;
        int swmode,swmnext,test;
        char buf[256];
        struct sigaction sa;

    d1.run = 1;
    sa.sa_handler = handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGINT,&sa, NULL) == -1) 
        printf("handler error\n");
    test=0; nrun=100000000; d1.mfreq=200.0; nblock=160;
//    nblock = 80;
  nblock=1280;
    d1.temp=0; d1.disp = 1; d1.sim=0;  d1.printout = 1; 
    d1.nspec = 32768;
    d1.dwin = 1;
  d1.dwin=0;
    pport = 1;
    for(i=0;i<argc-1;i++){
    sscanf(argv[i], "%79s", buf);
    if (strstr(buf, "-disp")) { sscanf(argv[i+1], "%d",&d1.disp); }
    if (strstr(buf, "-sim")) { sscanf(argv[i+1], "%d",&d1.sim); }
    if (strstr(buf, "-print")) { sscanf(argv[i+1], "%d",&d1.printout); }
    if (strstr(buf, "-test")) { sscanf(argv[i+1], "%d",&test); }
    if (strstr(buf, "-nrun")) { sscanf(argv[i+1], "%d",&nrun); }
    if (strstr(buf, "-nblock")) { sscanf(argv[i+1], "%d",&nblock); }
    if (strstr(buf, "-nspec")) { sscanf(argv[i+1], "%d",&d1.nspec); }
    if (strstr(buf, "-pport")) { sscanf(argv[i+1], "%d",&pport); }
    if (strstr(buf, "-dwin")) { sscanf(argv[i+1], "%d",&d1.dwin); }
    if (strstr(buf, "-mfreq")) { sscanf(argv[i+1], "%lf",&d1.mfreq); }
    }

   if(pport)  parport(-1);
//   setuid(0);
   setgid(getgid());
   setuid(getuid());
//    if(nblock == 1) nrun = 1;
    if(!d1.disp) sleep(3);   // allow time for pci bus 
    if(d1.disp){
      gtk_init(&argc, &argv);
      disp();
    }
    d1.stim = 1e-6/(2.0*d1.mfreq);
        nspec = d1.nspec;
        d1.fstart = 0.0; d1.fstop = d1.mfreq; d1.fstep = d1.mfreq/nspec; 
        d1.fres = 4.0*d1.mfreq*1e03/((double)nspec);  
        d1.foutstatus = 0;
   run = 1; 
   px14run(spec,-1);   // init
   if(pport) parport(0);  // set to antenna 
   while(run<=nrun && d1.run){
    if (run > 1 && (run % 120) == 1) {
//      px14run(spec,-2); // recalibrate every 120 cycles
    }
    for(swmode=0; swmode<3; swmode++) {
        d1.adcmax = -1e99;
        d1.adcmin = 1e99;
        d1.mode = swmode;
        d1.numblk = 0;
        px14run(spec,nblock);
       if(swmode == 2 || test == 3) swmnext = 0;
           else swmnext = swmode + 1;
        if(pport) parport(swmnext); // set switch for next cycle as it needs acquire to set
        for(kk=0;kk<nspec;kk++) data[swmode*nspec+kk] = spec[kk];
       if(d1.disp){
        aa = 100.0/((double)d1.numblk*nspec*2.0);  // for compatibility with acml FFT 
        for(kk=0;kk<nspec;kk++) avspec[kk]=10.0*log10(aa*spec[kk]+1e-99);
        d1.totp = 0.0;
        for(kk=(int)(80.0*nspec/210.0);kk<nspec;kk++) d1.totp += spec[kk];
        d1.secs = readclock();
        Repaint(d1.mfreq);
        while (gtk_events_pending()) gtk_main_iteration();
        clearpaint();
        }
        max = -1e99;
        maxi = 0;
        aa = 1.0/((double)d1.numblk*nspec*2.0);  // for compatibility with acml FFT 
        for(kk=0;kk<nspec;kk++){
        av = aa * data[swmode*nspec+kk];
        av = 10.0*log10(av) - 38.3;   // power in dBm -20 dBm into dp310 = -30 dbm
        if(kk < 10) av = -199.0;
        if(av > max){ max = av; maxi=kk; }
        data[swmode*nspec+kk]=av;
        }
        freq = d1.fstart + maxi*d1.mfreq/nspec;
        if(d1.printout) printf("max %f dBm maxkk %d swmode %d freq %5.1f MHz adcmax %8.5f %d adcmin %8.5f temp %2.0f C\n",
                max,maxi,swmode,freq,d1.adcmax,d1.maxindex,d1.adcmin,d1.temp);
            if(!test) outfile(&data[swmode*nspec],nspec,swmode);
       }
       if(test==2){
         for(kk=0;kk<nspec;kk++) {
               aa = pow(10.0,0.1*(data[1*nspec+kk]+38.3));
               freq = d1.fstart + kk*d1.mfreq/nspec;
               av = aa * (1e03/300.0)*pow(freq/100,-2.5);
               av = 10.0*log10(av) - 38.3; 
               if(kk < 10) av = -199.0;
               data[0*nspec+kk]=av;
               av = aa * (700.0/300.0);
               av = 10.0*log10(av) - 38.3;              
               if(kk < 10) av = -199.0;
               data[2*nspec+kk]=av;
          }
         for(swmode=0; swmode<3; swmode++)  outfile(&data[swmode*nspec],nspec,swmode); 
       }
       run++;
       }
        px14run(spec,-3);  // clean-up pci
	return 0;
}


void
outfile (double data[],int num, int swpos)
{
  FILE *file1;
  int yr, da, hr, mn, sc, i,j,k;
  char txt[256];
  char b64[64];
  for (i = 0; i < 26; i++) {
           b64[i] = 'A' + i;
           b64[i + 26] = 'a' + i;
        }
  for (i = 0; i < 10; i++)
        b64[i + 52] = '0' + i;
        b64[62] = '+';
        b64[63] = '/';
      d1.secs = readclock();
    if(d1.foutstatus==0) {
      toyrday (d1.secs, &yr, &da, &hr, &mn, &sc);
      d1.rday = da;
      sprintf (d1.filname, "/home/aeer/px14400/%4d_%03d_%02d.acq", yr, da, hr);
      }
  if ((file1 = fopen (d1.filname, "a")) == NULL)
    {
      if ((file1 = fopen (d1.filname, "w")) == NULL)
	{
	  d1.foutstatus = -99;
	  printf ("cannot write %s\n", d1.filname);
	  return;
	}
      d1.foutstatus = 1;
    }
  else
    d1.foutstatus = 1;
  if (d1.foutstatus == 1)
    {
      toyrday (d1.secs, &yr, &da, &hr, &mn, &sc);
      if (da != d1.rday && swpos == 0)
	{
	  fclose (file1);
	  d1.foutstatus = 0;
	  toyrday (d1.secs, &yr, &da, &hr, &mn, &sc);
	  sprintf (d1.filname, "/home/aeer/px14400/%4d_%03d_%02d.acq", yr, da, hr);
	  if ((file1 = fopen (d1.filname, "w")) == NULL)
	    {
	      d1.foutstatus = -99;
	      return;
	    }
	  d1.foutstatus = 1;
	}
	        fprintf (file1, "# swpos %d resolution %8.3f adcmax %8.5f adcmin %8.5f temp %2.0f C nblk %d nspec %d\n",
                         swpos,d1.fres,d1.adcmax,d1.adcmin,d1.temp,d1.numblk,d1.nspec);
		sprintf (txt,
			 "%4d:%03d:%02d:%02d:%02d %1d %8.3f %8.6f %8.3f %4.1f spectrum ",
                yr, da, hr, mn, sc, swpos, d1.fstart, d1.fstep, d1.fstop, d1.adcmax);
	        fprintf (file1, "%s", txt);
/*
       for(i=0; i<num; i++) {
                fprintf(file1," %9.5f",data[i]);
                }
	        fprintf (file1,"\n");
*/
      for(i=0; i<num; i++) {
         k = -(int)(data[i] * 1e05);
         if(k > 16700000) k = 16700000;
         if(k < 0) k = 0;
       for(j=0; j<4; j++) txt[j] = b64[k >> (18-j*6) & 0x3f]; 
         txt[4] = 0;
         fprintf(file1,"%s",txt);
       }
      fprintf (file1,"\n");
      fclose (file1);
      if(swpos == 0) d1.rday = da;
    }
}




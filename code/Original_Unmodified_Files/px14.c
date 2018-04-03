#include <stdlib.h>
#include <sys/time.h>
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <pthread.h>
#include "d1typ6.h"
#include "d1glob6.h"
#include "d1proto6.h"
#include "stdafx.h"
#include <px14.h>
#include <fftw3.h>
//#define NBR 2097152
#define NBR 4194304
//#define NBR 8388608 spurs and slow
#define NSIZ 65536
#define PI 3.1415926536
//#define DMA_XFER_SAMPLES		(NBR)
#define DMA_XFER_SAMPLES		(2 * 1048576)
#define DMA_BUFFER_SAMPLES		(1 * DMA_XFER_SAMPLES)
/// PX14400 board number (serial or 1-based index) to use
#define MY_PX14400_BRD_NUM		1




//typedef struct timeval TV ;
//typedef struct timezone TZ;
unsigned short waveFormArray[NBR];
unsigned short LastwaveForm[NBR];
int corestat[5],corerr[5];
int numblk0,numblk1,numblk2,numblk3;
extern HPX14 hBrd;
extern fftwf_plan p0,p1,p2,p3;
extern px14_sample_t *dma_bufp;
extern float *reamin0,*reamin1,*reamin2,*reamin3,*reamout0,*reamout1,*reamout2,*reamout3;

void procspec(int);
void *runspec(void *);
void fft_init(int, int, fftwf_plan *);
void fft_free(int, fftwf_plan *);
void cfft(fftwf_plan *);



int pxrun(int mode)
{
  double dAcqRate;
  int res,i;
  u_int brd_rev,sn;

  if(mode == -1){
  dma_bufp = NULL;

  printf ("PciAcqPX14 v1.0 - Signatec PX14400 PCI Acquisition "
	  "Recording Utility\n\n");

  dAcqRate = 400.0; 
  // -- Connect to and initialize the PX14400 device
  printf ("Connecting to and initializing PX14400 device...\n");
  res = ConnectToDevicePX14(&hBrd, MY_PX14400_BRD_NUM);
  if (res != SIG_SUCCESS)
    {
      DumpLibErrorPX14(res, "Failed to connect to PX14400 device: ",hBrd,0);
      return -1;
    }

  GetBoardRevisionPX14(hBrd, &brd_rev, NULL);
  GetSerialNumberPX14(hBrd, &sn);

  printf ("Connected to PX14400 #%u\n\n", sn);

  // Set all hardware settings into a known state
  res = SetPowerupDefaultsPX14(hBrd);
  if (SIG_SUCCESS != res) {
    DumpLibErrorPX14(res, "Failed to set powerup defaults: ", hBrd,0);
    return -1;
  }

  SetActiveChannelsPX14(hBrd, PX14CHANNEL_ONE);
  res = SetInternalAdcClockRatePX14(hBrd, dAcqRate);
  if (SIG_SUCCESS != res)
    {
      DumpLibErrorPX14(res, "Failed to set acquisition rate: ", hBrd,0);
      return -1;
    }


  res = SetInputVoltRangeCh1PX14(hBrd,0);   // was zero
  if (SIG_SUCCESS != res)
    {
      DumpLibErrorPX14(res, "Failed to set input voltage: ", hBrd,0);
      return -1;
    }



  // Allocate a DMA buffer that will receive PCI acquisition data. By 
  //  allocating a DMA buffer, we can use the "fast" PX14400 library
  //  transfer routines for highest performance
  res = AllocateDmaBufferPX14(hBrd, DMA_BUFFER_SAMPLES, &dma_bufp);
  if (SIG_SUCCESS != res)
    {
      DumpLibErrorPX14(res, "Failed to allocate DMA buffer: ", hBrd,0);
      return -1;
    }
     return 0;
   }
  
  if(mode == 0){
//  printf ("Arming board for acquisition\n");
  res = BeginBufferedPciAcquisitionPX14(hBrd,0);
  if (SIG_SUCCESS != res)
    {
      DumpLibErrorPX14(res, "Failed to arm recording: ", hBrd,0);
      return -1;
    }
// printf("armed boarf\n");
	
  // Main recording loop
    {
      // Determine where new data transfer data will go. We alternate
      //  between two halves of our DMA buffer

      // Start asynchronous DMA transfer of new data; the transfer will be
      //  started and the function will return immediately without waiting
      //  for the transfer to complete. This allows our code to continue on
      //  while the transfer occurs in parallel. We'll then wait for the
      //  transfer to complete after we've done our processing.
      res = GetPciAcquisitionDataFastPX14(hBrd, DMA_XFER_SAMPLES, 
					  dma_bufp, PX14_TRUE);
      if (SIG_SUCCESS != res)
	{
	  static const char* msgp =
	    "\nFailed to obtain PCI acquisition data: ";
	  DumpLibErrorPX14 (res, msgp, hBrd,0);
          return -1;
	}

      // Wait for the asynchronous DMA transfer to complete so we can loop 
      //  back around to start a new one. Calling thread will sleep until
      //  the transfer completes or is cancelled. When we're in buffered
      //  PCI acquisition mode, this function will also check the state of
      //  the RAM FIFO and return an error if the RAM FIFO overflowed at
      //  any point during the transfer.
      res = WaitForTransferCompletePX14(hBrd,0);
 
      if (SIG_SUCCESS != res)
	{
	  if (SIG_CANCELLED == res)
	    printf ("\nAcquisition cancelled");
	  else if (SIG_PX14_FIFO_OVERFLOW == res) {
	    printf ("\nRAM FIFO overflow: ");
	    printf ("Could not keep up with acquisition rate\n");
	  }
	  else
	    {
	      static const char* msgp = 
		"\nAn error occurred waiting for transfer to complete: ";
	      DumpLibErrorPX14(res, msgp, hBrd,0);
	    }
            return -1;
	}

    }
  // End the acquisition. Always do this since in ensures the board is 
  //  cleaned up properly
  EndBufferedPciAcquisitionPX14(hBrd);
//  printf("leaving pxrun(0)\n");
  }
  if(mode == 1){
  for(i=0;i<NBR;i++) waveFormArray[i] = dma_bufp[i];
//  for(i=0;i<NBR;i++)  waveFormArray[i] = 32768.0 + sin(i*10e6*2.0*PI/400e6)*32000.0;
//  printf("waveFormArray\n");
//  for(j=0;j<8;j++){
//  for(i=0;i<16;i++)printf(" %d",waveFormArray[i+(NBR/8)*j]);
//  printf("\n");
//  }
  return 0;
  }
  if(mode == 2){
  // -- Cleanup
  if (dma_bufp)
    FreeDmaBufferPX14(hBrd, dma_bufp);
  DisconnectFromDevicePX14(hBrd);
   }
  return 0;
}

int px14run(float spec[], int numacq)
{

    int i, num;
    int blsiz, blsiz2;

    int ithr[5];
    int ii,statok;
    double  a0, a1, a2, a3;
    pthread_t threads[5];


      if(numacq == -1){
        pxrun(-1);
       blsiz2 = d1.nspec;
       blsiz = blsiz2 * 2;
       fft_init(blsiz,0, &p0);
       fft_init(blsiz,1, &p1);
       fft_init(blsiz,2, &p2);
       fft_init(blsiz,3, &p3);
        return 0;
      }

      if(numacq == -2){
        return 0;
        }

      if(numacq == -3){
        pxrun(2);    // clean-up pci
        fft_free(0,&p0);
        fft_free(1,&p1);
        fft_free(2,&p2);
        fft_free(3,&p3);
        return 0;
        }

    blsiz2 = d1.nspec;
    blsiz = blsiz2 * 2;

       a0=0.35875; a1=0.48829; a2=0.14128; a3=0.01168;
     for(i=0;i<blsiz;i++) win0[i]=win1[i]=win2[i]=win3[i]=0.5*(a0-a1*cos(2.0*PI*i/(blsiz-1))+a2*cos(4.0*PI*i/(blsiz-1))-a3*cos(6.0*PI*i/(blsiz-1)))/32768.0;
     // 0.5 for px14400

//       usleep(100000);  // sleep for 0.1 sec
     if(numacq > 0) {
        for (i = 0; i < blsiz2; i++) spec0[i] = spec1[i] = spec2[i] = spec3[i] = 0.0;
        numblk0 = numblk1 = numblk2= numblk3 = 0;

        for(num=0;num<numacq;num++){

        if(num == 0) {

	pxrun(0); // ### Perform an acquisition ###
	pxrun(1); // ### Readout the waveform ###
        for (i = 0; i < NBR; i++)  LastwaveForm[i] = waveFormArray[i];

        }

        else {

//        ii = 3;
        ii = 5;
        for(i=0;i<ii;i++) corestat[i] = corerr[i] = 0;
          for (i = 0; i < ii; i++) {
            if (corestat[i] >= 0) {
               ithr[i] = i;
               if (pthread_create(&threads[i], NULL, runspec, &ithr[i])) {
                    printf("error creating thread\n");
                      return 0;
                     }
                  }
             }
        for (i = 0; i < ii; i++) {
          if (corestat[i] >= 0) {
              if (pthread_join(threads[i], NULL)) {
                  printf("error joining thread\n");
                     return 0;
                    }
                  }
             }
         statok = 1;
         for (i = 0; i < ii; i++) {
             if (corestat[i] != 1) {
                 statok = 0;
                 corerr[i]++;
                   }
           }
         if (statok == 0) {
             printf("statok  error %d\n", statok);
             }

            for (i = 0; i < NBR; i++)  LastwaveForm[i] = waveFormArray[i];
          }

         }

        procspec(0);
        procspec(1);
        procspec(2);
        procspec(3);
        
        for (i = 0; i < blsiz2; i++) spec[i] = spec0[i] + spec1[i] + spec2[i] + spec3[i];
        d1.numblk += numblk0 + numblk1 + numblk2 + numblk3;
     }
//    fft_free(&p0);
//    fft_free(&p1);
//    fft_free(&p2);
//    fft_free(&p3);
	return 0;
}


void *runspec(void *ii)
{
    int icore; 
    icore = *((int *) ii);
    corestat[icore] = 0;
    if(icore == 0) procspec(0);
    if(icore == 1) procspec(1);
    if(icore == 2) procspec(2);
    if(icore == 3) procspec(3);
    if(icore == 4) {
	pxrun(0); // ### Perform an acquisition ###
	pxrun(1); // ### Readout the waveform ###
    }
  corestat[icore] = 1;
  pthread_exit(NULL);         // stops thread from executing again
}

void procspec(int mode)
  {
    int i, j, kkk, nbr;
    int kk, blsiz, blsiz2, kn, blstep;
//    double aam, rre, aam2, rre2;
      float aam, rre, aam2, rre2;
        blsiz2 = d1.nspec;
        blsiz = blsiz2 * 2;
//        nbr = NBR/2;
        nbr = NBR/4;
        if (d1.dwin) {
            kn = 2 * nbr / blsiz - 2;
            blstep = blsiz2;
        }
        else {
            kn = nbr/blsiz;
            blstep = blsiz;
        }
        for (kk = 0; kk < kn; kk++){
                i = kk % 2;
                kkk = kk * blstep;
        if(mode==0) {
         for (j = 0; j < blsiz; j++) {
                rre = (LastwaveForm[j+kkk]-32768)*win0[j];
//                if(d1.sim) rre = 0.5*cos(j*2.0*PI*d1.sim*1e06/(2.0*d1.mfreq*1e06));
                if (rre > d1.adcmax) {d1.adcmax = rre; d1.maxindex=j+kkk;}
                if (rre < d1.adcmin) d1.adcmin = rre;
                reamin0[2*j+i] = rre;
                     }
          }
        if(mode==1) {
         for (j = 0; j < blsiz; j++) {
                rre = (LastwaveForm[j+kkk+nbr]-32768)*win1[j];
//                if(d1.sim) rre = 0.5*cos(j*2.0*PI*d1.sim*1e06/(2.0*d1.mfreq*1e06));
                reamin1[2*j+i] = rre;
                     }
          }
        if(mode==2) {
         for (j = 0; j < blsiz; j++) {
                rre = (LastwaveForm[j+kkk+2*nbr]-32768)*win2[j];
//                if(d1.sim) rre = 0.5*cos(j*2.0*PI*d1.sim*1e06/(2.0*d1.mfreq*1e06));
                reamin2[2*j+i] = rre;
                     }
          }
        if(mode==3) {
         for (j = 0; j < blsiz; j++) {
                rre = (LastwaveForm[j+kkk+3*nbr]-32768)*win3[j];
//                if(d1.sim) rre = 0.5*cos(j*2.0*PI*d1.sim*1e06/(2.0*d1.mfreq*1e06));
                reamin3[2*j+i] = rre;
                     }
          }
                if (kk % 2) {

                    if(mode==0) cfft(&p0);
                    if(mode==1) cfft(&p1);
                    if(mode==2) cfft(&p2);
                    if(mode==3) cfft(&p3);
                 
                  if(mode==0) {
                    for (i = 0; i < blsiz2; i++) {
                        if (i >= 1) {
                            rre = reamout0[2*i] + reamout0[2*(blsiz - i)];
                            aam = reamout0[2*i+1] - reamout0[2*(blsiz - i)+1];
                            aam2 = -reamout0[2*i] + reamout0[2*(blsiz - i)];
                            rre2 = reamout0[2*i+1] + reamout0[2*(blsiz - i)+1];
                        } else {
                            rre = reamout0[2*i] + reamout0[0];
                            aam = reamout0[2*i+1] - reamout0[1];
                            aam2 = -reamout0[2*i] + reamout0[0];
                            rre2 = reamout0[2*i+1] + reamout0[1];
                        }
                        spec0[i] += rre * rre + aam * aam + rre2 * rre2 + aam2 * aam2;
                    }
                          }
                    if(mode==1) {
                    for (i = 0; i < blsiz2; i++) {
                        if (i >= 1) {
                            rre = reamout1[2*i] + reamout1[2*(blsiz - i)];
                            aam = reamout1[2*i+1] - reamout1[2*(blsiz - i)+1];
                            aam2 = -reamout1[2*i] + reamout1[2*(blsiz - i)];
                            rre2 = reamout1[2*i+1] + reamout1[2*(blsiz - i)+1];
                        } else {
                            rre = reamout1[2*i] + reamout1[0];
                            aam = reamout1[2*i+1] - reamout1[1];
                            aam2 = -reamout1[2*i] + reamout1[0];
                            rre2 = reamout1[2*i+1] + reamout1[1];
                        }
                        spec1[i] += rre * rre + aam * aam + rre2 * rre2 + aam2 * aam2;
                    }
                     }
                    if(mode==2) {
                    for (i = 0; i < blsiz2; i++) {
                        if (i >= 1) {
                            rre = reamout2[2*i] + reamout2[2*(blsiz - i)];
                            aam = reamout2[2*i+1] - reamout2[2*(blsiz - i)+1];
                            aam2 = -reamout2[2*i] + reamout2[2*(blsiz - i)];
                            rre2 = reamout2[2*i+1] + reamout2[2*(blsiz - i)+1];
                        } else {
                            rre = reamout2[2*i] + reamout2[0];
                            aam = reamout2[2*i+1] - reamout2[1];
                            aam2 = -reamout2[2*i] + reamout2[0];
                            rre2 = reamout2[2*i+1] + reamout2[1];
                        }
                        spec2[i] += rre * rre + aam * aam + rre2 * rre2 + aam2 * aam2;
                    }
                     }
                    if(mode==3) {
                    for (i = 0; i < blsiz2; i++) {
                        if (i >= 1) {
                            rre = reamout3[2*i] + reamout3[2*(blsiz - i)];
                            aam = reamout3[2*i+1] - reamout3[2*(blsiz - i)+1];
                            aam2 = -reamout3[2*i] + reamout3[2*(blsiz - i)];
                            rre2 = reamout3[2*i+1] + reamout3[2*(blsiz - i)+1];
                        } else {
                            rre = reamout3[2*i] + reamout3[0];
                            aam = reamout3[2*i+1] - reamout3[1];
                            aam2 = -reamout3[2*i] + reamout3[0];
                            rre2 = reamout3[2*i+1] + reamout3[1];
                        }
                        spec3[i] += rre * rre + aam * aam + rre2 * rre2 + aam2 * aam2;
                    }
                     }
                      if(mode==0) numblk0 += 2;
                      if(mode==1) numblk1 += 2;
                      if(mode==2) numblk2 += 2;
                      if(mode==3) numblk3 += 2;
                 }
               }

  }



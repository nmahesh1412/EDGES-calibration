#include "stdafx.h"

// Sampling frequency
#define ACQUISITION_RATE      200
#define DMA_XFER_SAMPLES	(2*1024*1024)
/// PX14400 board number (serial or 1-based index) to use
#define MY_PX14400_BRD_NUM	1

// A quick rough equivalent to a certain other platform's GetTickCount
static unsigned int SysGetTickCount();

int main(int argc, char* argv[])
{
   u_int dwTickStart, dwTickLastUpdate, dwTickNow, dwElapsed, dwTickLastErrSave;
   px14_sample_t *dma_buf1p, *dma_buf2p, *cur_chunkp, *prev_chunkp;
   unsigned long long samples_recorded;
   unsigned loop_counter, h, m, s;
   double dAcqRate;
   FILE* filp;
   bool bDone;
   HPX14 hBrd;
   int res;

   dma_buf1p = dma_buf2p = NULL;
   filp = NULL;

   printf ("PciAcqPX14 v1.1 - Signatec PX14400 PCI Acquisition Recording "
           "Utility\n\n");

   // Open up output file if we're saving data to disk
   if (argc > 1) {
      printf ("Recording output to file: %s\n", argv[1]);
      filp = fopen(argv[1], "wb");
      if (NULL == filp) {
         DumpLibErrorPX14(SIG_PX14_FILE_IO_ERROR, "Failed to open output file: ", hBrd);
         return -2;
      }
   }
   else {
      printf ("No output file specified on command line. "
              "Data will not be saved to file.\n");
   }

   // Determine our acquisition rate
#if 0
   printf ("Acquisition rate in MHz [200.0, 1500.0]? ");
   if (!scanf("%lf", &dAcqRate)) {
      printf ("Umm, what?");
      return -1;
   }
#endif
   dAcqRate = ACQUISITION_RATE;
   printf ("\n\n");

   // -- Connect to and initialize the PX14400 device
   printf ("Connecting to and initializing PX14400 device...\n");
   if ((res=ConnectToDevicePX14(&hBrd, MY_PX14400_BRD_NUM)) != SIG_SUCCESS) {
      DumpLibErrorPX14(res, "Failed to connect to PX14400 device: ");
      goto AllDone;
   }
   // Set all hardware settings into a known state
   if ((res=SetPowerupDefaultsPX14(hBrd)) != SIG_SUCCESS) {
      DumpLibErrorPX14(res, "Failed to set powerup defaults: ");
      goto AllDone;
   }
   if ((res=SetActiveChannelsPX14(hBrd, PX14CHANNEL_ONE)) != SIG_SUCCESS) {
      DumpLibErrorPX14(res, "Failed to set active channel: ");
      goto AllDone;
   }
   if ((res=SetInternalAdcClockRatePX14(hBrd, dAcqRate)) != SIG_SUCCESS) {
      DumpLibErrorPX14(res, "Failed to set acquisition rate: ", hBrd);
      goto AllDone;
   }

   // Allocate a DMA buffer that will receive PCI acquisition data. By
   //  allocating a DMA buffer, we can use the "fast" PX14400 library
   //  transfer routines for highest performance
   // We allocate two DMA buffers and alternate transfers between them.
   //  We'll use asynchronous data transfers so we can transfer to one
   //  buffer while processing the other.
   res = AllocateDmaBufferPX14(hBrd, DMA_XFER_SAMPLES, &dma_buf1p);
   if (SIG_SUCCESS != res) {
      DumpLibErrorPX14(res, "Failed to allocate DMA buffer #1: ", hBrd);
      goto AllDone;
   }
   res = AllocateDmaBufferPX14(hBrd, DMA_XFER_SAMPLES, &dma_buf2p);
   if (SIG_SUCCESS != res) {
      DumpLibErrorPX14(res, "Failed to allocate DMA buffer #2: ", hBrd);
      goto AllDone;
   }

   printf ("\nReady to arm recording. Press ESC to cancel or other "
           "to continue...\n");
   if (27 == _getch()) goto AllDone;

   // Arm recording - Acquisition will begin when the PX14400 receives
   //  a trigger event.
   printf ("Arming board for acquisition. Press ESC to quit\n\n");
   res = BeginBufferedPciAcquisitionPX14(hBrd);
   if (SIG_SUCCESS != res) {
      DumpLibErrorPX14(res, "Failed to arm recording: ", hBrd);
      goto AllDone;
   }

   dwTickLastUpdate = dwTickLastErrSave = 0;
   samples_recorded = 0;
   loop_counter = 0;
   prev_chunkp = NULL;
   dwTickStart = 0;
   bDone = false;

   // Main recording loop - The main idea here is that we're alternating
   //  between two halves of our DMA buffer. While we're transferring
   //  fresh acquisition data to one half, we process (e.g. write to disk)
   //  the other half.
   while (!bDone) {

      // Determine where new data transfer data will go. We alternate
      //  between our two DMA buffers
      if (loop_counter & 1)
         cur_chunkp = dma_buf2p;
      else
         cur_chunkp = dma_buf1p;

      // Start asynchronous DMA transfer of new data; this function starts
      //  the transfer and returns without waiting for it to finish. This
      //  gives us a chance to process the last batch of data in parallel
      //  with this transfer.
      res = GetPciAcquisitionDataFastPX14(hBrd, DMA_XFER_SAMPLES,
                                         cur_chunkp, PX14_TRUE);
      if (SIG_SUCCESS != res) {
         DumpLibErrorPX14 (res, "\nFailed to obtain PCI acquisition data: ", hBrd);
         break;
      }

      // Process previous chunk data while we're transfering to
      if (prev_chunkp) {
         // TODO : Write acquisition data to a file or something
         //   - prev_chunkp points to DMA_XFER_SAMPLES samples of acquisition
         //     data
         //   - Note that each sample is 1 byte

         if (NULL != filp) {

            if (!fwrite(prev_chunkp,sizeof(px14_sample_t),DMA_XFER_SAMPLES,filp)){
               DumpLibErrorPX14(SIG_PX14_FILE_IO_ERROR, "\nFailed to write acquisition data to file: ",hBrd);
               break;
            }
         }
      }

      // Wait for the asynchronous DMA transfer to complete so we can loop
      //  back around to start a new one. Calling thread will sleep until
      //  the transfer completes
      res = WaitForTransferCompletePX14(hBrd);
      if (SIG_SUCCESS != res) {
         if (SIG_CANCELLED == res)
            printf ("\nAcquisition cancelled");
         else {
            DumpLibErrorPX14(res, "\nAn error occurred waiting for transfer "
                            "to complete: ", hBrd);
         }
         break;
      }

      if (!dwTickStart)
         dwTickStart = dwTickLastUpdate = SysGetTickCount();

      // Update counters
      samples_recorded += DMA_XFER_SAMPLES;
      loop_counter++;
      prev_chunkp = cur_chunkp;

      // Are we being asked to quit?
      if (_kbhit()) {
         switch (toupper(_getch())) {
            case 27:
               bDone = true;
               break;
            default:
               putchar('\a');
               break;
         }
      }

      // Periodically update progress
      dwTickNow = SysGetTickCount();
      if (bDone || (dwTickNow - dwTickLastUpdate > 700)) {
         double dRate;

         dwTickLastUpdate = dwTickNow;
         dwElapsed = dwTickNow - dwTickStart;

         if (dwElapsed) {
            dRate = (samples_recorded / 1000000.0) / (dwElapsed / 1000.0);
            h = m = s = 0;
            if (dwElapsed >= 1000) {
               if ((s = dwElapsed / 1000) >= 60) {	// Seconds
                  if ((m = s / 60) >= 60) {		// Minutes
                     if ((h = m / 60))			// Hours
                        m %= 60;
                  }

                  s %= 60;
               }
            }

            printf ("\rTotal data: %0.2f MS, Rate: %6.2f MS/s, "
                    "Elapsed Time: %u:%02u:%02u  ",
                    samples_recorded / 1000000.0, dRate, h, m, s);
            fflush(stdout);
         }
      }
   }

   // End the acquisition. Always do this since in ensures the board is
   //  cleaned up properly
   EndBufferedPciAcquisitionPX14(hBrd);

AllDone:
   // -- Cleanup
   if (NULL != filp) fclose(filp);
   if (dma_buf1p) FreeDmaBufferPX14(hBrd, dma_buf1p);
   if (dma_buf2p) FreeDmaBufferPX14(hBrd, dma_buf2p);

   DisconnectFromDevicePX14(hBrd);

   return 0;
}

// NOTE: Do not use this value for absolute time management;
//  use it as a relative count only
unsigned int SysGetTickCount()
{
   struct timeval tv;
   if (gettimeofday(&tv, (struct timezone *)0))
      return 0;
   return (unsigned)((tv.tv_sec & 0x003FFFFF) * 1000L + tv.tv_usec / 1000L);
}


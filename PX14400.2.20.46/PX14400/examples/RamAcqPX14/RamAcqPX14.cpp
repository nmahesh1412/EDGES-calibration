/** @file		RamAcqPX14.cpp
  @brief		PX14400 RAM Acquisition Example

  @author		Mike DeKoker
  @date		1/14/2010 (Created)

  This application demonstrates how to do a PX14400 RAM acquisition
  and optionally save the data to disk using PX14400 library functions

  -- Version 1.1
  - Updates
  o Using DumpLibErrorPX14 to dump library errors instead of a (removed)
  static function that did the same thing
  */
#include "stdafx.h"
#include <stdlib.h>

// Sampling frequency
#define ACQUISITION_RATE      200
/// Number of samples we'll acquire and save
#define ACQUISITION_SAMPLES   (32*1024)
#define TIMEOUT_SECONDS		   5

int main(int argc, char* argv[])
{
   int res, app_res, brd;
   HPX14 hBrd;

   puts ("RamAcqPX14 v1.1 - Signatec PX14400 RAM Acquisition "
         "Demo Utility");

   // Update board number (1-based index) from commandline, if available.
   brd = (argc>2) ? atoi(argv[2]):1;

   // -- Connect to and initialize the PX14400 device
   printf ("Connecting to and initializing PX14400 device...\n");
   res = ConnectToDevicePX14(&hBrd, brd);
   if (res != SIG_SUCCESS) {
      DumpLibErrorPX14(res, "Failed to connect to PX14400 device: ");
      return -1;
   }

   app_res = -1;

   do {
      // Set all hardware settings into a known state
      SetPowerupDefaultsPX14(hBrd);
      // Configure card for single channel acquisition
      SetActiveChannelsPX14(hBrd, PX14CHANNEL_ONE);
      // Select the internal clock's acquisition rate
      if ((res=SetInternalAdcClockRatePX14(hBrd, ACQUISITION_RATE)) != SIG_SUCCESS) {
         DumpLibErrorPX14(res, "Failed to set acquisition rate: ", hBrd);
         break;
      }

      // Prompt user to continue with acquisition
      puts ("\nReady to arm recording. "
            "Press ESC to cancel to other to continue...");
      if (27 == _getch()) {
         app_res = 0;
         break;
      }

      // -- Do RAM acquisition
      printf ("Doing %u-sample RAM acquisition... (timeout %d seconds)\n", ACQUISITION_SAMPLES, TIMEOUT_SECONDS);
      res = AcquireToBoardRamPX14 (hBrd, 0, ACQUISITION_SAMPLES, TIMEOUT_SECONDS * 1000, PX14_FALSE);
      if (SIG_SUCCESS != res) {
         DumpLibErrorPX14(res, "Failed to acquire to RAM: ", hBrd);
         break;
      }
      printf ("RAM acquisition complete\n");

      // Save acquisition data to disk if file passed on command line
      if (argc > 1) {
         // See PX14400 Operator's Manual for details on this structure. It can be
         //  setup to have the library save the acquired data in a number of
         //  formats.
         PX14S_FILE_WRITE_PARAMS fwp;

         memset (&fwp, 0, sizeof(PX14S_FILE_WRITE_PARAMS));
         fwp.struct_size = sizeof(PX14S_FILE_WRITE_PARAMS);
         fwp.pathname = argv[1];
         // Generate XML context info (output filename + ".srdc")
         fwp.flags = PX14FILWF_GENERATE_SRDC_FILE;
         res = ReadSampleRamFileBufPX14(hBrd, 0, ACQUISITION_SAMPLES, &fwp);
         if (SIG_SUCCESS != res)
            DumpLibErrorPX14(res, "Failed to transfer data to file: ", hBrd);
         else
            printf ("Data saved to file: %s\n", argv[1]);
      }
      else {
         printf ("Note: Pass a filename on the command line to save\n");
         printf ("      acquisition data to that file.\n");
      }

      app_res = 0;

   } while (0);

   // -- Cleanup
   DisconnectFromDevicePX14(hBrd);

   printf ("\n\n");
   return app_res;
}


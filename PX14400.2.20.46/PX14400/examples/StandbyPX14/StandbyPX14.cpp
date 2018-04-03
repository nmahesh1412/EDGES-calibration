/** @file		StandbyPX14
    @brief		Puts all Signatec PX14400 boards into standby mode

    @author		Mike DeKoker
    @date		2/24/2010 (Created)

    This is just a quick utility to put all PX14400 boards into Standby
    mode, which will cancel any current acquisitions or transfers. This
    program is only used for troubleshooting but may be of use to anyone
    developing PX14400 software.
*/
#include <unistd.h>
#include <stdio.h>
#include <px14.h>

int main(int argc, char* argv[])
{
  int device_count, i, res;
  unsigned int sn;
  HPX14 hBrd;

  printf ("StandbyPX14 v1.0 - All PX14400 boards to Standby mode\n");

  device_count = GetDeviceCountPX14();
  if (0 == device_count) {
    printf ("No PX14400 devices detected in system.\n");
    printf (" (Or device driver is not loaded)\n");
    return 0;
  }

  printf ("%d PX14400 device%s detected\n", 
	  device_count, 1==device_count?"":"s");

  for (i=0; i<device_count; i++) {
    printf ("Connecting to device %d...\n", i+1);
    res = ConnectToDevicePX14(&hBrd, i+1);
    if (SIG_SUCCESS != res)
      DumpLibErrorPX14(res, " -Failed to connect to device: ");
    else {
      GetSerialNumberPX14(hBrd, &sn);
      printf (" -Connected to device SN: %u\n", sn);

      res = SetOperatingModePX14(hBrd, PX14MODE_STANDBY);
      if (SIG_SUCCESS != res)
	DumpLibErrorPX14(res, " -Failed to enter standby mode: ", hBrd);
      else
	printf (" -Board put into standby mode\n");

      DisconnectFromDevicePX14(hBrd);
    }
  }

  printf ("\n\n");
  return 0;
}


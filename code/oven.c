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
#include <errno.h>
#include <signal.h>



int main(int argc, char **argv)
{ int usbdev;
  int status,i,chk,t,j,chkrec,mode,m;
  double set;
  char command[16],resp[12],buf[256];
 
  mode = -1; set=25.0;
  for(m=0;m<argc;m++){
  sscanf(argv[m], "%79s", buf);
   if (strstr(buf, "-mode")) {sscanf(argv[m+1], "%x",&mode);}
   if (strstr(buf, "-temp")) {sscanf(argv[m+1], "%lf",&set);}
  }


  if(mode == -1) {
         printf("01  read input1         0x01 - should see about 2500\n");
         printf("02  read switch voltage 0x02 - should see about 12012\n");
         printf("03  read input2         0x03 - should see about -26887\n");
         printf("04  read power          0x04 - should see default 0\n");
         printf("05  read alarm status   0x05 - should see default 10\n");
         printf("40  read set point      0x40 - should see default 2500\n");
         printf("41  read bandwidth      0x41 - should see default 100\n");
         printf("42  read gain           0x42 - should see default 0\n");
         printf("43  read devivative     0x43 - should see default 0\n");
         printf("44  read input1 offset  0x44 - should see default 0\n");
         printf("45  read input2 offset  0x45 - should see default 0\n");
         printf("46  read voltage        0x46 - should see default 1200\n");
         printf("47  read alarm settings 0x47 - should see default 10000\n");
         printf("48  read cool/heat      0x48 - should see default 2\n");
         printf("49  read alarm enable   0x49 - should see default 0\n");
         printf("4a read maxtemp set    0x4a - should see default 1000\n");
         printf("4b read mintemp set    0x4b - should see default -2000\n");
         printf("4c read heat side mult 0x4c - should see default 1000\n");
         printf("4d read output enabled 0x4d - should see default 1\n");
         printf("4e read cool side mult 0x4e - should see default 1000\n");
         printf("10 write set point tmp 0x10\n");
  return 0;}
printf("HHHHH");
  system("stty -F /dev/ttyUSB0 19200 cs8 -cstopb -parity -icanon min 1 time 1");

  usbdev=0; 
  usbdev = open("/dev/ttyUSB0",O_RDWR);
  printf("usbdev %x\n",usbdev);

  t = set*100;
  sprintf(command,"*00%02x%08x",mode,t);


  chk=0; for(i=0;i<12;i++) chk += command[i+1];
  sprintf(&command[13],"%02x",(chk%256));
  command[15] = 0x0d;
  printf("(stx=%x)",command[0]);
  for (i=1;i<15;i++) printf("%1c",command[i]);
  printf("(etx=%x)\n",command[15]);
  status = 0;
  status = write(usbdev,command,16);
  printf("status %x\n",status);
  printf("start to read\n");
  status=0;
  for(i=0;i<12;i++) resp[i]=0;
//  sprintf(resp,"*000003e8c0");  resp[11]=0x5e;
  sleep(1);
  status = read(usbdev,resp,12);
  printf("status %x resp %s\n",status,resp);
//  for(i=0;i<12;i++) printf("i=%d ch=%2x %d\n",i,resp[i],resp[i]);
  t=0; j=1; for(i=0;i<8;i++) { 
         if(resp[8-i] <= '9') m = '0'; else m = 'a' - 10; 
         t += (resp[8-i]-m)*j; j = j*16;
  }
  printf("revcd from %02x = %d %f\n",mode,t,t/100.0);
  chkrec=0; for(i=0;i<8;i++) chkrec += resp[i+1];
  printf("chkrec %02x\n",(chkrec%256));
  close(usbdev); 
	return 0;
}






#include <stdio.h>
#include <dos.h>

void main(void)
{
 unsigned int far *ptraddr;  /* Pointer to location of Port Addresses */
 unsigned int address;       /* Address of Port */
 int a;

 ptraddr=(unsigned int far *)0x00000408;

 for (a = 0; a < 3; a++)
   {
    address = *ptraddr;
    if (address == 0)
		printf("No port found for LPT%d \n",a+1);
    else
		printf("Address assigned to LPT%d is %Xh\n",a+1,address);
    *ptraddr++;
   }
}


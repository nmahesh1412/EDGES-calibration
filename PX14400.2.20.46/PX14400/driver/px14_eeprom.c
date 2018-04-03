/** @file 	px14_eeprom.c
 */
#include "px14_drv.h"

// Commands are only 2 bits, but since there are some with the same value
// the difference will be the second nibble that is not used
#define CMD_WEN				0x10
#define CMD_WDS				0x20
#define CMD_ERASE			0x03
#define CMD_EAL				0x30
#define CMD_READ			0x02
#define CMD_WRITE			0x01

// PLX EPROM Register offset from base address
#define EPROM_REG_OFF		0x6C

// Number of bits (one less) that correspond to command, address and data
#define COMM_SIZE			1
#define ADDR_SIZE			7
#define DATA_SIZE			15
#define MYEND				-1

static u_short EepromSendCmd (px14_device* devp, u_short command, u_short address, u_short data);
static int EepromWrite (px14_device* devp, u_short addr, u_short val);
static int EepromWaitReady (px14_device* devp);
static u_short Read_DIN (px14_device* devp);

// Helper macros

#define WRITE_CS(p,v)		\
  do{(p)->regCfg.fields.reg0.bits.CHIPSEL =(v); WriteHwRegCfgEeprom(p);}while(0)
#define WRITE_DOUT(p,v)		\
  do{(p)->regCfg.fields.reg0.bits.DOUT=(v);     WriteHwRegCfgEeprom(p);}while(0)
#define WRITE_SCLK(p,v)		\
  do{(p)->regCfg.fields.reg0.bits.SCLK=(v);     WriteHwRegCfgEeprom(p);}while(0)


// Private function implementation ------------------------------------------

u_short Read_DIN (px14_device* devp)
{
  PX14_FLUSH_BUS_WRITES(devp);
  devp->regCfg.fields.reg0.val = ReadHwRegCfgEeprom(devp);

  return (u_short)devp->regCfg.fields.reg0.bits.DIN;
}

u_short EepromSendCmd (px14_device* devp,
		       u_short command, 
		       u_short address,
		       u_short data)
{
  int i;

  // Zero DOUT and CS
  WRITE_DOUT(devp, 0);
  WRITE_CS(devp, 0);
  WRITE_SCLK(devp, 0);
  WRITE_SCLK(devp, 1);

  // Bring CS high
  WRITE_SCLK(devp, 0);
  WRITE_CS(devp, 1);
  WRITE_SCLK(devp, 1);

  // Make DOUT high initiating the command transfer
  WRITE_SCLK(devp, 0);
  WRITE_DOUT(devp, 1);
  WRITE_SCLK(devp, 1);
  
  // Send command bit by bit
  for(i=COMM_SIZE; i>MYEND; i--) {
    WRITE_SCLK(devp, 0);
    WRITE_DOUT(devp, (u_short)((command & (1<<i) )>>i));
    WRITE_SCLK(devp, 1);
  }

  // Set address based on command, leave unchanged if CMD_WRITE, CMD_READ 
  // or CMD_ERASE
  switch(command) {
  case (CMD_WEN): address = 0xC0; break;
  case (CMD_WDS): address = 0x00; break;
  case (CMD_EAL): address = 0x80; break;

  default: ;
    // If this command is not one of the above then
    // use the address passed into the function
  }

  // Send address out
  for(i=ADDR_SIZE; i>MYEND; i--) {
    WRITE_SCLK(devp, 0);
    WRITE_DOUT(devp, (u_short)((address & (1<<i) )>>i));
    WRITE_SCLK(devp, 1);
  }

  switch(command) {
  case (CMD_WRITE):
    // Send data out
    for(i=DATA_SIZE; i>MYEND; i--) {
      WRITE_SCLK(devp, 0);
      WRITE_DOUT(devp, (u_short)((data & (1<<i) )>>i));
      WRITE_SCLK(devp, 1);
    }
    break;
  case (CMD_READ):
    // Bring data in
    data=0;
    for(i=DATA_SIZE; i>MYEND; i--) {
      WRITE_SCLK(devp, 0);
      WRITE_SCLK(devp, 1);
      data |= ((Read_DIN(devp) & 0x1) << i);
    }
    break;
    
  }

  // Deassert CS to finish access
  WRITE_SCLK(devp, 0);
  WRITE_CS(devp, 0);
  WRITE_SCLK(devp, 0);
  
  return (command == CMD_READ) ? data : 0;
}

int EepromWaitReady (px14_device* devp)
{
  unsigned long expires;
  int res;

  // In general this wait is very short but in the event that the EEPROM gets
  //  fried or something we don't want to hang up the system.

  expires = jiffies + HZ;	// Timeout after 1 second
  res = 0;

  WRITE_CS(devp, 1);
  while(Read_DIN(devp)!=1)
    {
      if (time_after(jiffies, expires)) {
	res = -SIG_PX14_TIMED_OUT;
	break;
      }
    }
  WRITE_CS(devp, 0);

  return res;
}

u_int ReadSerialNum_PX14 (px14_device* devp)
{
  uint value = 0;
	
#ifndef PX14PP_NO_EEPROM_ACCESS
  value = EepromSendCmd(devp, CMD_READ, PX14EA_SERIALNUM_UPPER, 0) << 16;
  value |= EepromSendCmd(devp, CMD_READ, PX14EA_SERIALNUM_LOWER, 0);
#endif

  // If serial number is 0xFFFFFFFF then EEPROM is likely not initialized
  if (value == 0xFFFFFFFF) 
    value = 0;

  return value;
}

u_int ReadEepromVal_PX14 (px14_device* devp, u_int eeprom_addr)
{
  u_int ret_val = 0;

  if (eeprom_addr >= PX14_EEPROM_WORDS)
    return 0xFFFFFFFF;

#ifndef PX14PP_NO_EEPROM_ACCESS
  ret_val = EepromSendCmd(devp, CMD_READ, (u_short)eeprom_addr, 0);
#endif
  
  return ret_val;
}

u_int ReadEepromVal32_PX14 (px14_device* devp, u_int addrLow, u_int addrHigh)
{
  u_int val;

  if ((addrLow >= PX14_EEPROM_WORDS) || (addrHigh >= PX14_EEPROM_WORDS))
    return 0xFFFFFFFF;
	
#ifndef PX14PP_NO_EEPROM_ACCESS
  val  = EepromSendCmd(devp, CMD_READ, (u_short)addrHigh, 0) << 16;
  val |= EepromSendCmd(devp, CMD_READ, (u_short)addrLow,  0);
#else
  val = 0;
#endif
  
  return val;
}

int EepromWrite (px14_device* devp, u_short addr, u_short val)
{
  int res;

#ifndef PX14PP_NO_EEPROM_ACCESS
  EepromSendCmd(devp, CMD_WEN, 0, 0);		// Enable writes
  EepromSendCmd(devp, CMD_WRITE, addr, val);	// Write data
  res = EepromWaitReady(devp);			// Wait until ready
  EepromSendCmd(devp, CMD_WDS, 0, 0);		// Disable writes
#else
  res = 0;
#endif

  return res;
}

int px14ioc_eeprom_io (px14_device* devp, u_long arg)
{
#ifdef PX14PP_NO_EEPROM_ACCESS
  return -SIG_PX14_NOT_IMPLEMENTED;
#else

  PX14S_EEPROM_IO ctx;
  int res;

  // Input is a PX14S_EEPROM_IO structure
  res = __copy_from_user(&ctx, (void*)arg, sizeof(PX14S_EEPROM_IO));
  if (res != 0)
    return -EFAULT;

  PX14_LOCK_MUTEX(devp)
  {
    if (ctx.bRead) {
      // Read item from EEPROM
      ctx.eeprom_val = EepromSendCmd(devp, CMD_READ, 
				     (u_short)ctx.eeprom_addr, 0);
    }
    else {
      // Write item to EEPROM
      EepromWrite(devp, (u_short)ctx.eeprom_addr, (u_short)ctx.eeprom_val);
    }
  }
  PX14_UNLOCK_MUTEX(devp);

  // Output is a PX14S_EEPROM_IO structure if we're reading
  if (ctx.bRead) {
    res = __copy_to_user ((void*)arg, &ctx, sizeof(PX14S_EEPROM_IO)) 
      ? -EFAULT : 0;
  }
  else
    res = 0;
  
  return res;

#endif
}

int CheckEepromIoPX14 (px14_device* devp)
{
#ifdef PX14PP_NO_EEPROM_ACCESS

  return -1;

#else

  unsigned short eeprom_data, eeprom_data2;
  int res;
  
  res = 0;
  
  // Do a quick test to ensure that configuration EEPROM IO is working.
  
  // Read a value from somewhere; don't care what the value is since
  //  EEPROM may not have been initialized yet
  eeprom_data = ReadEepromVal_PX14(devp, PX14EA_BOARD_TYPE);
  
  // Write it; if it doesn't timeout EEPROM (and/or firmware) is good
  res = EepromWrite(devp, PX14EA_BOARD_TYPE, eeprom_data);
  if (0 == res) {
    // Re-verify the value
    eeprom_data2 = ReadEepromVal_PX14(devp, PX14EA_BOARD_TYPE);
    if (eeprom_data != eeprom_data2)
      res = -1;
  }
  
  return res;

#endif
}

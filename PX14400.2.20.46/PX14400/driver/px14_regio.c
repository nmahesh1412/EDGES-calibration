/** @file 	px14_regio.c
  @brief 	PX14400 register IO routines
  */
#include "px14_drv.h"


static void InitClockGenerator (px14_device* devp);

static void WriteClkRegHelper (px14_device* devp,
                               int reg_addr,
                               unsigned char reg_mask,
                               unsigned char reg_val,
                               unsigned int log_idx,
                               unsigned int byte_idx,
                               unsigned int pre_delay_ms,
                               unsigned int post_dealy_ms,
                               int bUpdateRegs);

static u_int ReadLogicalClkGenReg_PX14 (px14_device* devp,
                                        PX14S_DEV_REG_READ* ctxp);


//static int ResetBoardRamDcms (px14_device* devp);

static void PowerUpChannel (px14_device* devp, int bCh1, int bPowerUp);

static void InitRegSetDriver (px14_device* devp);
static void InitRegSetDevice (px14_device* devp);
static void WriteRegSetDevice (px14_device* devp);

void InitClockGenerator (px14_device* devp)
{
   PX14U_CLKGEN_REGISTER_SET rs;

   // Do a soft reset on clock generator device
   rs.values[0] = PX14CLKREGDEF_00;
   rs.fields.reg00.bits.SoftReset = 1;
   rs.fields.reg00.bits.SoftResetM = 1;
   // Register update not required for soft reset
   WriteClkRegHelper(devp, 0x000, 0xFF,
                     rs.fields.reg00.bytes[0], 0, 0, 0, 0, PX14_TRUE);
   rs.fields.reg00.bits.SoftReset = 0;
   rs.fields.reg00.bits.SoftResetM = 0;
   WriteClkRegHelper(devp, 0x000, 0xFF,
                     rs.fields.reg00.bytes[0], 0, 0, 0, 0, PX14_TRUE);
   udelay(25);

   // -- Setup a local register set with default values
   rs.values[ 0] = PX14CLKREGDEF_00;
   rs.values[ 1] = PX14CLKREGDEF_01;
   rs.values[ 2] = PX14CLKREGDEF_02;
   rs.values[ 3] = PX14CLKREGDEF_03;
   rs.values[ 4] = PX14CLKREGDEF_04;
   rs.values[ 5] = PX14CLKREGDEF_05;
   rs.values[ 6] = PX14CLKREGDEF_06;
   rs.values[ 7] = PX14CLKREGDEF_07;
   rs.values[ 8] = PX14CLKREGDEF_08;
   rs.values[ 9] = PX14CLKREGDEF_09;
   rs.values[10] = PX14CLKREGDEF_10;
   rs.values[11] = PX14CLKREGDEF_11;
   rs.values[12] = PX14CLKREGDEF_12;
   rs.values[13] = PX14CLKREGDEF_13;
   rs.values[14] = PX14CLKREGDEF_14;
   rs.values[15] = PX14CLKREGDEF_15;
   rs.values[16] = PX14CLKREGDEF_16;
   rs.values[17] = PX14CLKREGDEF_17;
   rs.values[18] = PX14CLKREGDEF_18;
   rs.values[19] = PX14CLKREGDEF_19;

   if (devp->boardRev == PX14BRDREV_PX12500) {
      rs.values[12] = PX14CLKREGDEF_12_PX12500;
      rs.values[19] = PX14CLKREGDEF_19_PX12500;
   }
   if (devp->features & PX14FEATURE_NO_INTERNAL_CLOCK) {
      devp->regClkGen.fields.reg01.bits.PLL_PowerDown = 1;
      devp->regClkGen.fields.reg19.bits.BypassVcoDivider = 1;
   }
   if (devp->fwVerPci > PX14_VER32(1,10,0,0)) {
      devp->regClkGen.fields.reg09.bits.Out2LvpeclDiffVolt = 3;
      devp->regClkGen.fields.reg09.bits.Out3LvpeclDiffVolt = 3;
      devp->regClkGen.fields.reg10.bits.Out4LvpeclDiffVolt = 3;
      devp->regClkGen.fields.reg10.bits.Out5LvpeclDiffVolt = 3;
   }

   // -- Update hardware with these defaults
   // (Note: We're doing deep writes for each register write)

   // - Logical register 0
   // CG reg 000
   WriteClkRegHelper(devp, 0x000, 0xFF,
                     rs.fields.reg00.bytes[0], 0, 0, 0, 0, PX14_TRUE);
   // CG reg 004
   WriteClkRegHelper(devp, 0x004, 0xFF,
                     rs.fields.reg00.bytes[1], 0, 1, 0, 0, PX14_TRUE);

   // - Logical register 1
   // CG reg 010
   WriteClkRegHelper(devp, 0x010, 0xFF,
                     rs.fields.reg01.bytes[0], 1, 0, 0, 0, PX14_TRUE);
   // CG reg 011
   WriteClkRegHelper(devp, 0x011, 0xFF,
                     rs.fields.reg01.bytes[1], 1, 1, 0, 0, PX14_TRUE);
   // CG reg 012
   WriteClkRegHelper(devp, 0x012, 0xFF,
                     rs.fields.reg01.bytes[2], 1, 2, 0, 0, PX14_TRUE);
   // CG reg 013
   WriteClkRegHelper(devp, 0x013, 0xFF,
                     rs.fields.reg01.bytes[3], 1, 3, 0, 0, PX14_TRUE);

   // - Logical register 2
   // CG reg 014
   WriteClkRegHelper(devp, 0x014, 0xFF,
                     rs.fields.reg02.bytes[0], 2, 0, 0, 0, PX14_TRUE);
   // CG reg 015
   WriteClkRegHelper(devp, 0x015, 0xFF,
                     rs.fields.reg02.bytes[1], 2, 1, 0, 0, PX14_TRUE);
   // CG reg 016
   WriteClkRegHelper(devp, 0x016, 0xFF,
                     rs.fields.reg02.bytes[2], 2, 2, 0, 0, PX14_TRUE);
   // CG reg 017
   WriteClkRegHelper(devp, 0x017, 0xFF,
                     rs.fields.reg02.bytes[3], 2, 3, 0, 0, PX14_TRUE);

   // - Logical register 3
   // CG reg 018
   WriteClkRegHelper(devp, 0x018, 0xFF,
                     rs.fields.reg03.bytes[0], 3, 0, 0, 0, PX14_TRUE);
   // CG reg 019
   WriteClkRegHelper(devp, 0x019, 0xFF,
                     rs.fields.reg03.bytes[1], 3, 1, 0, 0, PX14_TRUE);
   // CG reg 01A
   WriteClkRegHelper(devp, 0x01A, 0xFF,
                     rs.fields.reg03.bytes[2], 3, 2, 0, 0, PX14_TRUE);
   // CG reg 01B
   WriteClkRegHelper(devp, 0x01B, 0xFF,
                     rs.fields.reg03.bytes[3], 3, 3, 0, 0, PX14_TRUE);

   // - Logical register 4
   // CG reg 01C
   WriteClkRegHelper(devp, 0x01C, 0xFF,
                     rs.fields.reg04.bytes[0], 4, 0, 0, 0, PX14_TRUE);
   // CG reg 01D
   WriteClkRegHelper(devp, 0x01D, 0xFF,
                     rs.fields.reg04.bytes[1], 4, 1, 0, 0, PX14_TRUE);
   // CG reg 01E
   WriteClkRegHelper(devp, 0x01E, 0xFF,
                     rs.fields.reg04.bytes[2], 4, 2, 0, 0, PX14_TRUE);
   // CG reg 01F
   WriteClkRegHelper(devp, 0x01F, 0xFF,
                     rs.fields.reg04.bytes[3], 4, 3, 0, 0, PX14_TRUE);

   // - Logical register 5
   // CG reg 0A0
   WriteClkRegHelper(devp, 0x0A0, 0xFF,
                     rs.fields.reg05.bytes[0], 5, 0, 0, 0, PX14_TRUE);
   // CG reg 0A1
   WriteClkRegHelper(devp, 0x0A1, 0xFF,
                     rs.fields.reg05.bytes[1], 5, 1, 0, 0, PX14_TRUE);
   // CG reg 0A2
   WriteClkRegHelper(devp, 0x0A2, 0xFF,
                     rs.fields.reg05.bytes[2], 5, 2, 0, 0, PX14_TRUE);

   // - Logical register 6
   // CG reg 0A3
   WriteClkRegHelper(devp, 0x0A3, 0xFF,
                     rs.fields.reg06.bytes[0], 6, 0, 0, 0, PX14_TRUE);
   // CG reg 0A4
   WriteClkRegHelper(devp, 0x0A4, 0xFF,
                     rs.fields.reg06.bytes[1], 6, 1, 0, 0, PX14_TRUE);
   // CG reg 0A5
   WriteClkRegHelper(devp, 0x0A5, 0xFF,
                     rs.fields.reg06.bytes[2], 6, 2, 0, 0, PX14_TRUE);

   // - Logical register 7
   // CG reg 0A6
   WriteClkRegHelper(devp, 0x0A6, 0xFF,
                     rs.fields.reg07.bytes[0], 7, 0, 0, 0, PX14_TRUE);
   // CG reg 0A7
   WriteClkRegHelper(devp, 0x0A7, 0xFF,
                     rs.fields.reg07.bytes[1], 7, 1, 0, 0, PX14_TRUE);
   // CG reg 0A8
   WriteClkRegHelper(devp, 0x0A8, 0xFF,
                     rs.fields.reg07.bytes[2], 7, 2, 0, 0, PX14_TRUE);

   // - Logical register 8
   // CG reg 0A9
   WriteClkRegHelper(devp, 0x0A9, 0xFF,
                     rs.fields.reg08.bytes[0], 8, 0, 0, 0, PX14_TRUE);
   // CG reg 0AA
   WriteClkRegHelper(devp, 0x0AA, 0xFF,
                     rs.fields.reg08.bytes[1], 8, 1, 0, 0, PX14_TRUE);
   // CG reg 0AB
   WriteClkRegHelper(devp, 0x0AB, 0xFF,
                     rs.fields.reg08.bytes[2], 8, 2, 0, 0, PX14_TRUE);

   // - Logical register 9
   // CG reg 0F0
   WriteClkRegHelper(devp, 0x0F0, 0xFF,
                     rs.fields.reg09.bytes[0], 9, 0, 0, 0, PX14_TRUE);
   // CG reg 0F1
   WriteClkRegHelper(devp, 0x0F1, 0xFF,
                     rs.fields.reg09.bytes[1], 9, 1, 0, 0, PX14_TRUE);
   // CG reg 0F2
   WriteClkRegHelper(devp, 0x0F2, 0xFF,
                     rs.fields.reg09.bytes[2], 9, 2, 0, 0, PX14_TRUE);
   // CG reg 0F3
   WriteClkRegHelper(devp, 0x0F3, 0xFF,
                     rs.fields.reg09.bytes[3], 9, 3, 0, 0, PX14_TRUE);

   // - Logical register 10
   // CG reg 0F4
   WriteClkRegHelper(devp, 0x0F4, 0xFF,
                     rs.fields.reg10.bytes[0], 10, 0, 0, 0, PX14_TRUE);
   // CG reg 0F5
   WriteClkRegHelper(devp, 0x0F5, 0xFF,
                     rs.fields.reg10.bytes[1], 10, 1, 0, 0, PX14_TRUE);

   // - Logical register 11
   // CG reg 140
   WriteClkRegHelper(devp, 0x140, 0xFF,
                     rs.fields.reg11.bytes[0], 11, 0, 0, 0, PX14_TRUE);
   // CG reg 141
   WriteClkRegHelper(devp, 0x141, 0xFF,
                     rs.fields.reg11.bytes[1], 11, 1, 0, 0, PX14_TRUE);
   // CG reg 142
   WriteClkRegHelper(devp, 0x142, 0xFF,
                     rs.fields.reg11.bytes[2], 11, 2, 0, 0, PX14_TRUE);
   // CG reg 143
   WriteClkRegHelper(devp, 0x143, 0xFF,
                     rs.fields.reg11.bytes[3], 11, 3, 0, 0, PX14_TRUE);

   // - Logical register 12
   // CG reg 190
   WriteClkRegHelper(devp, 0x190, 0xFF,
                     rs.fields.reg12.bytes[0], 12, 0, 0, 0, PX14_TRUE);
   // CG reg 191
   WriteClkRegHelper(devp, 0x191, 0xFF,
                     rs.fields.reg12.bytes[1], 12, 1, 0, 0, PX14_TRUE);
   // CG reg 192
   WriteClkRegHelper(devp, 0x192, 0xFF,
                     rs.fields.reg12.bytes[2], 12, 2, 0, 0, PX14_TRUE);

   // - Logical register 13
   // CG reg 193
   WriteClkRegHelper(devp, 0x193, 0xFF,
                     rs.fields.reg13.bytes[0], 13, 0, 0, 0, PX14_TRUE);
   // CG reg 194
   WriteClkRegHelper(devp, 0x194, 0xFF,
                     rs.fields.reg13.bytes[1], 13, 1, 0, 0, PX14_TRUE);
   // CG reg 195
   WriteClkRegHelper(devp, 0x195, 0xFF,
                     rs.fields.reg13.bytes[2], 13, 2, 0, 0, PX14_TRUE);

   // - Logical register 14
   // CG reg 196
   WriteClkRegHelper(devp, 0x196, 0xFF,
                     rs.fields.reg14.bytes[0], 14, 0, 0, 0, PX14_TRUE);
   // CG reg 197
   WriteClkRegHelper(devp, 0x197, 0xFF,
                     rs.fields.reg14.bytes[1], 14, 1, 0, 0, PX14_TRUE);
   // CG reg 198
   WriteClkRegHelper(devp, 0x198, 0xFF,
                     rs.fields.reg14.bytes[2], 14, 2, 0, 0, PX14_TRUE);

   // - Logical register 15
   // CG reg 199
   WriteClkRegHelper(devp, 0x199, 0xFF,
                     rs.fields.reg15.bytes[0], 15, 0, 0, 0, PX14_TRUE);

   // - Logical register 16
   // CG reg 19A
   WriteClkRegHelper(devp, 0x19A, 0xFF,
                     rs.fields.reg16.bytes[0], 16, 0, 0, 0, PX14_TRUE);
   // CG reg 19B
   WriteClkRegHelper(devp, 0x19B, 0xFF,
                     rs.fields.reg16.bytes[1], 16, 1, 0, 0, PX14_TRUE);
   // CG reg 19C
   WriteClkRegHelper(devp, 0x19C, 0xFF,
                     rs.fields.reg16.bytes[2], 16, 2, 0, 0, PX14_TRUE);
   // CG reg 19D
   WriteClkRegHelper(devp, 0x19D, 0xFF,
                     rs.fields.reg16.bytes[3], 16, 3, 0, 0, PX14_TRUE);

   // - Logical register 17
   // CG reg 19E
   WriteClkRegHelper(devp, 0x19E, 0xFF,
                     rs.fields.reg17.bytes[0], 17, 0, 0, 0, PX14_TRUE);

   // - Logical register 18
   // CG reg 19F
   WriteClkRegHelper(devp, 0x19F, 0xFF,
                     rs.fields.reg18.bytes[0], 18, 0, 0, 0, PX14_TRUE);
   // CG reg 1A0
   WriteClkRegHelper(devp, 0x1A0, 0xFF,
                     rs.fields.reg18.bytes[1], 18, 1, 0, 0, PX14_TRUE);
   // CG reg 1A1
   WriteClkRegHelper(devp, 0x1A1, 0xFF,
                     rs.fields.reg18.bytes[2], 18, 2, 0, 0, PX14_TRUE);
   // CG reg 1A2
   WriteClkRegHelper(devp, 0x1A2, 0xFF,
                     rs.fields.reg18.bytes[3], 18, 3, 0, 0, PX14_TRUE);

   // - Logical register 19
   // CG reg 1E0
   WriteClkRegHelper(devp, 0x1E0, 0xFF,
                     rs.fields.reg19.bytes[0], 19, 0, 0, 0, PX14_TRUE);
   // CG reg 1E1
   WriteClkRegHelper(devp, 0x1E1, 0xFF,
                     rs.fields.reg19.bytes[1], 19, 1, 0, 0, PX14_TRUE);
}

void WriteClkRegHelper (px14_device* devp,
                        int reg_addr,
                        unsigned char reg_mask,
                        unsigned char reg_val,
                        unsigned int log_idx,
                        unsigned int byte_idx,
                        unsigned int pre_delay_ms,
                        unsigned int post_dealy_ms,
                        int bUpdateRegs)
{
   PX14S_DEV_REG_WRITE reg_write;

   reg_write.reg_set = PX14REGSET_CLKGEN;
   reg_write.reg_idx = PX14REGIDX_CLKGEN;
   reg_write.reg_mask = reg_mask;
   reg_write.reg_val = reg_val;
   reg_write.cg_log_reg = log_idx;
   reg_write.cg_byte_idx = byte_idx;
   reg_write.cg_addr = reg_addr;
   WriteClockGenReg_PX14(devp, &reg_write, bUpdateRegs);
}

int WriteClockGenReg_PX14 (px14_device* devp,
                           PX14S_DEV_REG_WRITE* ctxp,
                           int bUpdateRegs)
{
   // Hardcoded 'Update Registers' request code
   static const unsigned sdreg_update_regs = 0x00023201;

   unsigned regVal, cacheByte;
   PX14U_SDREGIO sdregio;
   unsigned long f;

   cacheByte = (ctxp->cg_log_reg << 2) + ctxp->cg_byte_idx;

   // Update software register cache
   PX14_LOCK_DEVICE(devp,f)
   {
      regVal = devp->regClkGen.bytes[cacheByte] & ~ctxp->reg_mask;
      regVal |= (ctxp->reg_val & ctxp->reg_mask);
      devp->regClkGen.bytes[cacheByte] = (u_char)regVal;
   }
   PX14_UNLOCK_DEVICE(devp,f);

   // Package up serial register write request
   sdregio.val = 0;
   sdregio.bits.CG_ADDR = ctxp->cg_addr;
   sdregio.bits.CG_DATA = regVal;
   sdregio.bits.CG_RW_ = 0;
   WriteDevRegVal(devp, ctxp->reg_idx, sdregio.val);
   // The above write is a serial write which will take 1.5us to
   //  complete so we need a small stall so the write can finish.
   PX14_FLUSH_BUS_WRITES(devp);
   udelay(4);

   if (bUpdateRegs) {
      // The clock generator register content is not updated until the
      //  UpdateRegisters bit in register 5A is asserted.
      WriteDevRegVal(devp, ctxp->reg_idx, sdreg_update_regs);
      PX14_FLUSH_BUS_WRITES(devp);
      udelay(4);
   }

   return 0;
}

int WriteDeviceReg_PX14 (px14_device* devp,
                         u_int reg_idx,
                         u_int val,
                         u_int mask)
{
   unsigned long f;
   u_int regVal;

   // Make sure we're not writing outside of our region
   if (reg_idx > (devp->MemCount[PX14_BARIDX_DEVICE] >> 2))
      return -EINVAL;

   PX14_LOCK_DEVICE(devp,f)
   {
      // Write software register cache
      if (0xFFFFFFFF == mask)
         regVal = devp->regDev.values[reg_idx] = val;
      else {
         regVal  = devp->regDev.values[reg_idx] & ~mask;
         regVal |= (val & mask);
         devp->regDev.values[reg_idx] = regVal;
      }

      // Write hardware
      WriteDevReg(devp, reg_idx);

      // Serial registers require a slight delay to provide time for write to
      //  be transmitted to SAB FPGA. Delay is so if another serial register
      //  write comes after this one we won't clobber the first write
      if (PX14_IS_SERIAL_DEVICE_REG(reg_idx)) {
         PX14_FLUSH_BUS_WRITES(devp);
         udelay(5);
      }
   }
   PX14_UNLOCK_DEVICE(devp,f);

   return 0;
}

u_int ReadDeviceReg_PX14 (px14_device* devp, u_int reg_idx, u_int read_how)
{
   unsigned long f;
   u_int retVal;

   PX14_LOCK_DEVICE(devp, f)
   {
      if ((read_how == PX14REGREAD_HARDWARE) || PX14_IS_STATUS_REG(reg_idx)) {
         // Read from hardware
         retVal = ReadDevReg(devp, reg_idx);

         if (PX14_IS_SAB_REG(reg_idx)) {
            // SAB registers are serial, slower reads. The first read
            //  sends the read request and this second one will grab the
            //  correct data
            udelay(4);
            retVal = ReadDevReg(devp, reg_idx);
         }

         // Update cache
         devp->regDev.values[reg_idx] = retVal;
      }
      else {
         // Just grab from cache
         retVal = devp->regDev.values[reg_idx];
      }
   }
   PX14_UNLOCK_DEVICE (devp, f);

   return retVal;
}

u_int ReadClockGenReg_PX14 (px14_device* devp, PX14S_DEV_REG_READ* ctxp)
{
   u_int retVal, cacheByte;
   PX14U_SDREGIO sdregio;
   unsigned long f;

   if (ctxp->cg_addr == PX14CLKREGREAD_LOGICAL_REG_ONLY)
      return ReadLogicalClkGenReg_PX14 (devp, ctxp);

   cacheByte = (ctxp->cg_log_reg << 2) + ctxp->cg_byte_idx;

#ifndef PX14PP_NO_HARDWARE_ACCESS
   if (ctxp->read_how == PX14REGREAD_HARDWARE) {
      // Setup a read request
      sdregio.val = 0;
      sdregio.bits.CG_ADDR = ctxp->cg_addr;
      sdregio.bits.CG_RW_  = 1;
      WriteDevRegVal(devp, ctxp->reg_idx, sdregio.val);
      udelay(4);

      // Do the read
      sdregio.val = ReadDevReg(devp, ctxp->reg_idx);
      retVal = sdregio.bits.CG_DATA;

      // Update software register cache
      PX14_LOCK_DEVICE(devp, f)
      {
         devp->regClkGen.bytes[cacheByte] = (u_char)sdregio.bits.CG_DATA;
      }
      PX14_UNLOCK_DEVICE (devp, f);
   }
   else
#endif
   {
      // Grab from software cache
      retVal = devp->regClkGen.bytes[cacheByte];
   }

   return retVal;
}

u_int ReadLogicalClkGenReg_PX14 (px14_device* devp, PX14S_DEV_REG_READ* ctxp)
{
   // This table defines the actual Clock Gen register indices for a
   // particular logical Clock Gen register. (0xFFFF means no mapping)

   // Example: Logical Clock Gen register 1 is mapped to actual Clock Gen
   //  registers 0x10, 0x11, 0x12, and 0x13
   static const unsigned long long local_to_actual[] =
   {
      0xFFFFFFFF00040000ULL,		// Logical register 0
      0x0013001200110010ULL,		// Logical register 1
      0x0017001600150014ULL,		// Logical register 2
      0x001B001A00190018ULL,		// Logical register 3
      0x001F001E001D000CULL,		// Logical register 4
      0xFFFF00A200A100A0ULL,		// Logical register 5
      0xFFFF00A500A400A3ULL,		// Logical register 6
      0xFFFF00A800A700A6ULL,		// Logical register 7
      0xFFFF00AB00AA00A9ULL,		// Logical register 8
      0x00F300F200F100F0ULL,		// Logical register 9
      0xFFFFFFFF00F500F4ULL,		// Logical register 10
      0x0143014201410140ULL,		// Logical register 11
      0xFFFF019201910189ULL,		// Logical register 12
      0xFFFF019401940193ULL,		// Logical register 13
      0xFFFF019801970196ULL,		// Logical register 14
      0xFFFFFFFFFFFF0199ULL,		// Logical register 15
      0x019D019C019B019AULL,		// Logical register 16
      0xFFFFFFFFFFFF019EULL,		// Logical register 17
      0x01A201A101A0019FULL,		// Logical register 18
      0xFFFF023001E101E0ULL		// Logical register 19
   };

   if (ctxp->read_how == PX14REGREAD_HARDWARE)
   {
      ctxp->cg_addr =
         (unsigned short)((local_to_actual[ctxp->cg_log_reg] >>  0) & 0xFFFF);
      if (ctxp->cg_addr != 0xFFFF) {
         ctxp->cg_byte_idx = 0;
         ReadClockGenReg_PX14(devp, ctxp);
      }

      ctxp->cg_addr =
         (unsigned short)((local_to_actual[ctxp->cg_log_reg] >> 16) & 0xFFFF);
      if (ctxp->cg_addr != 0xFFFF) {
         ctxp->cg_byte_idx = 1;
         ReadClockGenReg_PX14(devp, ctxp);
      }

      ctxp->cg_addr =
         (unsigned short)((local_to_actual[ctxp->cg_log_reg] >> 32) & 0xFFFF);
      if (ctxp->cg_addr != 0xFFFF) {
         ctxp->cg_byte_idx = 2;
         ReadClockGenReg_PX14(devp, ctxp);
      }

      ctxp->cg_addr =
         (unsigned short)((local_to_actual[ctxp->cg_log_reg] >> 48) & 0xFFFF);
      if (ctxp->cg_addr != 0xFFFF) {
         ctxp->cg_byte_idx = 3;
         ReadClockGenReg_PX14(devp, ctxp);
      }
   }

   // Return cached data
   return devp->regClkGen.values[ctxp->cg_log_reg];
}

void InitializeHardwareSettingsPX14 (px14_device* devp)
{
   // Reset DMA initiator logic
   ResetDma_PX14(devp);

   // Make sure board is in standby mode
   SetOperatingMode_PX14(devp, PX14MODE_STANDBY);

	//Make sure the board have the good default trigger selection
	SetTriggerSelection_PX14(devp, PX14TRIGSEL_A_OR_B);

	//Make sure the board have the good default trigger selection
	SetTriggerHysteresis_PX14(devp, PX14TRIGHYST_DEFAULT_LVL);

   // Initialize driver registers; these are software only registers
   InitRegSetDriver(devp);
   InitRegSetDriver(devp);

   // Initialize primary device registers
   WriteRegSetDevice(devp);

   // Initialize all of the clock generator parts
   InitClockGenerator(devp);

   // Synchronize clock outputs; Toggle the CG_SYNC_
   WriteDeviceReg_PX14(devp, PX14REGIDX_CG_SYNC_,
                       0x00000000, PX14REGMSK_CG_SYNC_);
   WriteDeviceReg_PX14(devp, PX14REGIDX_CG_SYNC_,
                       0xFFFFFFFF, PX14REGMSK_CG_SYNC_);

   // Reset ADC clock DCMs
   DoDcmResetWork(devp);
}


/// Change PX14400 operating mode
void SetOperatingMode_PX14 (px14_device* devp, int mode)
{
   // TODO : No OFF mode implemented yet, don't write it to hardware
   if (mode == PX14MODE_OFF)
      mode = PX14MODE_STANDBY;

   // Write operating mode to SAB register
   devp->regDev.fields.regB.bits.OM = mode;
   WriteDevReg(devp, PX14REGIDX_OPERATING_MODE);
}

/// Get current PX14400 operating mode from driver's register cache
int GetOperatingModeSwr_PX14 (px14_device* devp)
{
   int operating_mode;
   unsigned long f;

   PX14_LOCK_DEVICE(devp,f)
   {
      operating_mode = PX14_OP_MODE(devp);
   }
   PX14_UNLOCK_DEVICE(devp,f);

   return operating_mode;
}

/// Setup the PX14400's active memory region; defines acq/xfer length
void SetActiveMemoryRegion_PX14 (px14_device* devp,
                                 u_int start_sample,
                                 u_int sample_count)
{
   unsigned long f;

   PX14_LOCK_DEVICE(devp,f)
   {
      // Starting sample (device register 2)
      devp->regDev.fields.reg2.bits.SA =
         start_sample >> PX14REGHWSH_START_SAMPLE;
      WriteDevReg(devp, 2);

      // Total sample count (device register 1)
      if (sample_count)
      {
         devp->regDev.fields.reg1.bits.FRUN = 0;
         // 1 burst = 1024 elements = 4096 samples = 2 ^ 12
         // Partial burst unit = 8 elements = 32 samples = 2 ^ 5
         devp->regDev.fields.reg1.bits.BC = sample_count >> 12;
         devp->regDev.fields.reg1.bits.PS = (sample_count & 0xFFF) >> 5;
      }
      else
      {
         // Free run acquisition
         devp->regDev.fields.reg1.bits.FRUN = 1;
         // firmware needs non-zero burst count
         devp->regDev.fields.reg1.bits.BC = 1;
      }
      WriteDevReg(devp, 1);
   }
   PX14_UNLOCK_DEVICE(devp,f);
}

/* int ResetBoardRamDcms (px14_device* devp) */
/* { */
/*   const int maxIdleLoops = 1024; */
/*   const int maxStallLoops = 256; */

/*   int bIdsLocked; */
/*   u_int reg_val; */
/*   int i; */

/*   // -- Lock IDS DCMs -- */

/*   // Pulse the DCM4 reset bit */
/*   WriteDeviceReg_PX14(devp, PX14REGIDX_DCM2_RESET,  */
/* 		     0xFFFFFFFF, PX14REGMSK_DCM2_RESET); */
/*   WriteDeviceReg_PX14(devp, PX14REGIDX_DCM2_RESET,  */
/* 		     0x00000000, PX14REGMSK_DCM2_RESET); */

/* #ifndef PX14PP_NO_HARDWARE_ACCESS */

/*   bIdsLocked = PX14_FALSE; */

/*   // Wait for DCMs to lock */

/*   // First try to spin really quickly... */
/*   for (i=0; i<maxIdleLoops && !bIdsLocked; i++) { */
/*     reg_val = ReadDevReg(devp, PX14REGIDX_DCM2_LOCK); */
/*     if (reg_val & PX14REGMSK_DCM2_LOCK) */
/*       bIdsLocked = PX14_TRUE; */
/*   } */

/*   if (!bIdsLocked) { */

/*     // Dumb polling didn't work, do some stalled waits.  */
/*     for (i=0; i<maxStallLoops && !bIdsLocked; i++) { */
/*       udelay (20); */
/*       reg_val = ReadDevReg(devp, PX14REGIDX_DCM2_LOCK); */
/*       if (reg_val & PX14REGMSK_DCM2_LOCK) */
/* 	bIdsLocked = PX14_TRUE; */
/*     } */
/*   } */

/* #else */
/*   bIdsLocked = PX14_TRUE; */
/* #endif */

/*   return bIdsLocked ? 0 : -SIG_PX14_TIMED_OUT; */
/* } */

int DoDcmResetWork (px14_device* devp)
{
   const int maxIdleLoops = 1024;
   const int maxStallLoops = 256;

   int bIdsLocked, bSabLocked, bNoDcm, bExtClk;
   u_int reg_val, bit_mask;
   unsigned long f;
   int i;

   PX14_LOCK_DEVICE(devp,f)
   {
      bNoDcm  = devp->regDev.fields.reg5.bits.NO_DCM ? 1 : 0;
      bExtClk = devp->regDev.fields.reg5.bits.CLKSRC == 1;	// 1 == PX14CLKSRC_EXTERNAL
   }
   PX14_UNLOCK_DEVICE(devp,f);

   // -- Lock IDS DCMs --

   // Pulse the DCM4 reset bit
   bit_mask = PX14REGMSK_DCM4_RESET;
   WriteDeviceReg_PX14(devp, PX14REGIDX_DCM4_RESET, bit_mask, bit_mask);
   WriteDeviceReg_PX14(devp, PX14REGIDX_DCM4_RESET, 0,        bit_mask);

   if (bNoDcm && bExtClk)
   {
      PX14_FLUSH_BUS_WRITES(devp);
      msleep(25);
   }

#ifndef PX14PP_NO_HARDWARE_ACCESS

   if (bNoDcm)
      bIdsLocked = bSabLocked = 1;
   else {

      // Wait for DCMs to lock

      bSabLocked = bIdsLocked = PX14_FALSE;

      // First try to spin really quickly...
      for (i=0; i<maxIdleLoops && !bIdsLocked; i++) {
         reg_val = ReadDevReg(devp, PX14REGIDX_ACQDCMLCK);
         if (reg_val & PX14REGMSK_ACQDCMLCK)
            bIdsLocked = PX14_TRUE;
      }

      if (!bIdsLocked) {
         // Dumb polling didn't work, do some stalled waits.
         for (i=0; i<maxStallLoops && !bIdsLocked; i++) {
            udelay (20);
            reg_val = ReadDevReg(devp, PX14REGIDX_ACQDCMLCK);
            if (reg_val & PX14REGMSK_ACQDCMLCK)
               bIdsLocked = PX14_TRUE;
         }
      }

      if (bIdsLocked) {
         // -- Now lock SAB DCMs --

         // Pulse DCMRST bit
         WriteDeviceReg_PX14(devp, PX14REGIDX_DCMRST, 0xFFFFFFFF, PX14REGMSK_DCMRST);
         PX14_FLUSH_BUS_WRITES(devp);
         udelay (20);
         WriteDeviceReg_PX14(devp, PX14REGIDX_DCMRST, 0x00000000, PX14REGMSK_DCMRST);

         // Wait for DCMs to lock
         // Currently we don't have a trustworthy status bit so we'll just stall
         msleep(25);
         bSabLocked = PX14_TRUE;
      }
   }

#else
   return 0;
#endif

   if (!bSabLocked || !bIdsLocked)
      return -SIG_PX14_DCM_SYNC_FAILED;

   PX14_LOCK_DEVICE(devp,f)
   {
      devp->dwFlags &= ~DEVF_NEED_DCM_RESET;
      if (!bNoDcm)
         atomic_inc(&devp->stat_dcm_resets);

      // Rewrite register 3; requires a stable clock which we may not
      //  have had prior to this reset
      WriteDevReg(devp, 3);
   }
   PX14_UNLOCK_DEVICE(devp,f);

   return 0;
}

int px14ioc_device_reg_read (px14_device* devp, u_long arg)
{
   PX14S_DEV_REG_READ ctx;
   unsigned reg_val;
   int res, f;

   // Input is a PX14S_DEV_REG_READ structure
   res = __copy_from_user(&ctx, (void*)arg, sizeof (PX14S_DEV_REG_READ));
   if (res != 0)
      return -EFAULT;

   if (ctx.reg_set >= PX14REGSET__COUNT)
      return -SIG_INVALIDARG;
   if (ctx.pre_delay_us > PX14_MAX_DRIVER_DELAY)
      return -SIG_INVALIDARG;
   if (ctx.post_delay_us > PX14_MAX_DRIVER_DELAY)
      return -SIG_INVALIDARG;

   if (ctx.pre_delay_us)
      DoDriverStall_PX14(ctx.pre_delay_us, 0);

   PX14_LOCK_MUTEX(devp)
   {
      reg_val = 0;
      res = 0;

      if (ctx.reg_set == PX14REGSET_DEVICE) {

         if (ctx.reg_idx < PX14_DEVICE_REG_COUNT)
            reg_val = ReadDeviceReg_PX14(devp, ctx.reg_idx, ctx.read_how);
         else
            res = -SIG_INVALIDARG;
      }

      else if (ctx.reg_set == PX14REGSET_CLKGEN) {

         if (ctx.cg_log_reg < PX14_CLKGEN_LOGICAL_REG_CNT)
            reg_val = ReadClockGenReg_PX14(devp, &ctx);
         else
            res = -SIG_INVALIDARG;
      }

      else if (ctx.reg_set == PX14REGSET_DRIVER) {

         if (ctx.reg_idx < PX14_DRIVER_REG_COUNT)
            reg_val = devp->regDriver.values[ctx.reg_idx];
         else
            res = -SIG_INVALIDARG;
      }
      else
         res = -SIG_INVALIDARG;
   }
   PX14_UNLOCK_MUTEX(devp);

   // Output is registe value
   if (!res &&  __copy_to_user ((void*)arg, &reg_val, 4))
      return -EFAULT;

   return res;
}

int px14ioc_device_reg_write (px14_device* devp, u_long arg)
{
   int res, f, bUpdateRegs;
   PX14S_DEV_REG_WRITE ctx;
   unsigned* regp;

   // Input is a PX14S_DEV_REG_WRITE structure
   res = __copy_from_user(&ctx, (void*)arg, sizeof (PX14S_DEV_REG_WRITE));
   if (res != 0)
      return -EFAULT;

   if (ctx.reg_set >= PX14REGSET__COUNT)
      return -SIG_INVALIDARG;
   if (ctx.pre_delay_us > PX14_MAX_DRIVER_DELAY)
      return -SIG_INVALIDARG;
   if (ctx.post_delay_us > PX14_MAX_DRIVER_DELAY)
      return -SIG_INVALIDARG;

   if (ctx.pre_delay_us)
      DoDriverStall_PX14(ctx.pre_delay_us, 0);

   PX14_LOCK_MUTEX(devp)
   {
      res = 0;

      if (ctx.reg_set == PX14REGSET_DEVICE) {

         if (ctx.reg_idx < PX14_DEVICE_REG_COUNT)
            res = WriteDeviceReg_PX14(devp, ctx.reg_idx, ctx.reg_val, ctx.reg_mask);
         else
            res = -SIG_INVALIDARG;
      }

      else if (ctx.reg_set == PX14REGSET_CLKGEN) {

         bUpdateRegs = (0 != (ctx.cg_addr & PX14CLKREGWRITE_UPDATE_REGS));
         if (bUpdateRegs)
            ctx.cg_addr &= ~PX14CLKREGWRITE_UPDATE_REGS;

         if (ctx.cg_addr >= PX14_CLKGEN_MAX_REG_IDX)
            res = -SIG_INVALIDARG;
         else
            WriteClockGenReg_PX14(devp, &ctx, bUpdateRegs);
      }

      else if (ctx.reg_set == PX14REGSET_DRIVER) {

         if (ctx.reg_idx < PX14_DRIVER_REG_COUNT) {
            regp = &devp->regDriver.values[ctx.reg_idx];
            *regp &= ~ctx.reg_mask;
            *regp |= (ctx.reg_val & ctx.reg_mask);
         }
         else
            res = -SIG_INVALIDARG;
      }
      else
         res = -SIG_INVALIDARG;
   }
   PX14_UNLOCK_MUTEX(devp);

   if ((0 == res) && ctx.post_delay_us)
      DoDriverStall_PX14(ctx.post_delay_us, 0);

   return res;
}

/** @brief Performs a PX14400 operating mode change

  This driver function does a PX14400 operating mode change. It also handles
  any driver-related side effects of the mode change. This includes things
  like canceling any acq/xfer waiters when the board is put into standby
  mode, setting up device state when entering an active mode, etc.

  @pre
  It is assumed that the user-mode PX14400 library ensures that all mode
  switches are to or from Standby mode. Both the hardware and driver
  require this.
  */
int px14ioc_mode_set (px14_device* devp, u_long arg)
{
   int res, op_mode_to, op_mode_from, bNeedDcmReset, chan_sel;
   unsigned long f;

   // Input is an int (operating mode)
   if (__copy_from_user(&op_mode_to, (void*)arg, sizeof(int)))
      return -EFAULT;
   if (op_mode_to<0 || op_mode_to>=PX14MODE__COUNT)
      return -SIG_INVALIDARG;

   PX14_LOCK_MUTEX(devp)
   {
      PX14_LOCK_DEVICE(devp,f)
      {
         op_mode_from = PX14_OP_MODE(devp);
         switch (devp->regDev.fields.reg7.bits.CHANSEL)
         {
            default:
            case 0:	chan_sel = PX14CHANNEL_DUAL; break;
            case 2: chan_sel = PX14CHANNEL_ONE; break;
            case 3: chan_sel = PX14CHANNEL_TWO; break;
         }

         // Reset samples complete state
         if (op_mode_from == PX14MODE_STANDBY)
            devp->bSampComp = 0;

         bNeedDcmReset = devp->dwFlags & DEVF_NEED_DCM_RESET;

         // Cancel active state if we're entering Standby mode
         if ((devp->DeviceState!=PX14STATE_IDLE) && PX14_IS_STANDBY(op_mode_to))
            DoPendingOpAbortWork_PX14(devp, 0);
      }
      PX14_UNLOCK_DEVICE(devp,f);

      // Coming from standby mode?
      if (PX14_IS_STANDBY(op_mode_from)) {

         // Reset completion object in the event that we get a waiter after the mode change
#if LINUX_VERSION_CODE > KERNEL_VERSION(3,12,0)
         reinit_completion (&devp->comp_acq_or_xfer);
#else
         init_completion (&devp->comp_acq_or_xfer);
#endif
         // And going into an acquisition (or SAB transfer) mode?
         if (PX14_IS_ACQ_MODE(op_mode_to) ||
             (PX14MODE_RAM_READ_SAB == op_mode_to)) {

            // Items area always on; this might change if we implement an Off
            //  mode.
            //PowerUpChannel(devp, PX14_TRUE,  chan_sel != PX14CHANNEL_TWO);
            //PowerUpChannel(devp, PX14_FALSE, chan_sel != PX14CHANNEL_ONE);

            // We're starting an acquisition
            atomic_inc(&devp->stat_acq_start);

            PX14_LOCK_DEVICE(devp,f)
            {
               // Reset initiator
               ResetDma_PX14(devp);

               // If we're entering a PCI acquisition mode then we'll
               //  keep the state clear since we'll have DMA requests
               //  coming in soon.
               if (op_mode_to != PX14MODE_ACQ_PCI_BUF) {
                  devp->DeviceState = PX14STATE_ACQ;
                  devp->bOpCancelled = 0;
               }
            }
            PX14_UNLOCK_DEVICE(devp,f);
         }

         // DCMs need to be reset prior to acquisition whenever we change
         //  acquisition clock
         if (PX14_IS_ACQ_MODE(op_mode_to) && bNeedDcmReset) {
            res = DoDcmResetWork(devp);
            if (0 != res) {
               PX14_UNLOCK_MUTEX_BREAK(devp);
               return res;
            }
         }
      }

      // Force a DMA initiator reset when going into Standby mode. It's
      //  possible that an asynchronous DMA transfer has been cutoff
      //  uncleanly.
      if (op_mode_to == PX14MODE_STANDBY)
         ResetDma_PX14(devp);

      // Set the operating mode
      SetOperatingMode_PX14(devp, op_mode_to);
   }
   PX14_UNLOCK_MUTEX(devp);

   return 0;
}

void PowerUpChannel (px14_device* devp, int bCh1, int bPowerUp)
{
   static const int r7_power_up_delay_microsecs = 350;

   PX14U_REG_DEV_07 r7;
   unsigned int mask;
   int res;

   // AMPPU1 and AMPPU2 are now ignored by the firmware. These components
   //  need to always be on with the current hardware layout.

   // TODO : It's not known if we should be considering PDOVR bit here
   // TODO : We're always keeping ADCs powered up. This should change when
   //        we're doing OFF mode
   // TODO : Consider XFMR?_SEL; if we're using transformer path we don't
   //        need to power up amplifier

   if (bCh1) {
      r7.bits.ADCPD1  = 0;//bPowerUp ? 0 : 1;
      //r7.bits.AMPPU1  = bPowerUp ? 1 : 0;
      //r7.bits.ADC1OE_ = bPowerUp ? 0 : 1;

      mask = PX14REGMSK_ADCPD1 | PX14REGMSK_AMPPU1;
   }
   else {
      r7.bits.ADCPD2  = 0;//bPowerUp ? 0 : 1;
      //r7.bits.AMPPU2  = bPowerUp ? 1 : 0;
      //r7.bits.ADC2OE_ = bPowerUp ? 0 : 1;

      mask = PX14REGMSK_ADCPD2 | PX14REGMSK_AMPPU2;
   }

   WriteDeviceReg_PX14(devp, 7, r7.val, mask);
   if (bPowerUp)
      udelay(r7_power_up_delay_microsecs);
}

void InitRegSetDriver (px14_device* devp)
{
   devp->regDriver.values[0] = PX14DREGDEFAULT_0;
   devp->regDriver.values[1] = PX14DREGDEFAULT_1;
   devp->regDriver.values[2] = PX14DREGDEFAULT_2;
   devp->regDriver.values[3] = PX14DREGDEFAULT_3;
   devp->regDriver.values[4] = PX14DREGDEFAULT_4;
   devp->regDriver.values[5] = PX14DREGDEFAULT_5;

   if (devp->boardRev == PX14BRDREV_PX12500)
   {
      devp->regDriver.values[1] = PX14DREGDEFAULT_1_PX12500;
      devp->regDriver.values[2] = PX14DREGDEFAULT_2_PX12500;
      devp->regDriver.values[3] = PX14DREGDEFAULT_3_PX12500;
      devp->regDriver.values[4] = PX14DREGDEFAULT_4_PX12500;
   }

   PX14_CT_ASSERT(PX14_DRIVER_REG_COUNT == 6);
}

void InitRegSetDevice (px14_device* devp)
{
   PX14U_DEVICE_REGISTER_SET* pRegs;
   unsigned char chanImp;
   unsigned int r7_def;

   pRegs = &devp->regDev;

   pRegs->values[0x0] = PX14REGDEFAULT_0;
   pRegs->values[0x1] = PX14REGDEFAULT_1;
   pRegs->values[0x2] = PX14REGDEFAULT_2;
   pRegs->values[0x3] = PX14REGDEFAULT_3;
   pRegs->values[0x4] = PX14REGDEFAULT_4;
   pRegs->values[0x5] = PX14REGDEFAULT_5;
   pRegs->values[0x6] = PX14REGDEFAULT_6;

   r7_def = PX14REGDEFAULT_7;
   if (devp->chanImps != 0)
   {
      // Channel 1
      chanImp = (unsigned char)(devp->chanImps & 0xFF);
      if (chanImp & PX14CHANIMPF_NON_AMPLIFIED)
      {
         r7_def &= ~PX14REGMSK_ATT1;
         r7_def |=  PX14REGMSK_XFMR1_SEL;
      }
      // Channel 2
      chanImp = (unsigned char)((devp->chanImps & 0xFF00) >> 8);
      if (chanImp & PX14CHANIMPF_NON_AMPLIFIED)
      {
         r7_def &= ~PX14REGMSK_ATT2;
         r7_def |=  PX14REGMSK_XFMR2_SEL;
      }
   }
   pRegs->values[0x7] = r7_def;

   pRegs->values[0x8] = PX14REGDEFAULT_8;
   pRegs->values[0x9] = PX14REGDEFAULT_9;
   pRegs->values[0xA] = PX14REGDEFAULT_A;
   pRegs->values[0xB] = PX14REGDEFAULT_B;
   pRegs->values[0xC] = PX14REGDEFAULT_C;

   if (devp->boardRev == PX14BRDREV_PX12500)
   {
      // Default rate is 500MHz; DCM_500 should be set when >= 450MHz
      pRegs->fields.reg5.bits.DCM_500 = 1;
   }

   if (devp->features & PX14FEATURE_NO_INTERNAL_CLOCK)
   {
      pRegs->fields.reg5.bits.CLKSRC = 1;	// PX14CLKSRC_EXTERNAL
   }
}

void WriteRegSetDevice (px14_device* devp)
{
   WriteDeviceReg_PX14(devp, 0x0, devp->regDev.values[0], 0xFFFFFFFF);
   WriteDeviceReg_PX14(devp, 0x1, devp->regDev.values[1], 0xFFFFFFFF);
   WriteDeviceReg_PX14(devp, 0x2, devp->regDev.values[2], 0xFFFFFFFF);
   WriteDeviceReg_PX14(devp, 0x3, devp->regDev.values[3], 0xFFFFFFFF);
   WriteDeviceReg_PX14(devp, 0x4, devp->regDev.values[4], 0xFFFFFFFF);
   WriteDeviceReg_PX14(devp, 0x5, devp->regDev.values[5], 0xFFFFFFFF);
   WriteDeviceReg_PX14(devp, 0x6, devp->regDev.values[6], 0xFFFFFFFF);
   WriteDeviceReg_PX14(devp, 0x7, devp->regDev.values[7], 0xFFFFFFFF);
   // 8: unused
   WriteDeviceReg_PX14(devp, 0x9, devp->regDev.values[9], 0xFFFFFFFF);
   // A: Special (FPGA processing parameters)
   // B: operating mode
   // C: Read-only (RAM address)
   // D: Read only (Status)
   // E,F: Special: Timestamp read
}

void SetTriggerSelection_PX14 (px14_device* devp, int selection)
{
	// Write trigger selection to trigger register
	devp->regDev.fields.reg10.bits.trig_sel = selection;
	WriteDevReg (devp, PX14REGIDX_TRIG_SEL);
}

void SetTriggerHysteresis_PX14 (px14_device* devp, int hyst)
{
	// Write trigger selection to trigger register
	devp->regDev.fields.reg10.bits.hyst = hyst;
	WriteDevReg (devp, PX14REGIDX_HYST);
}

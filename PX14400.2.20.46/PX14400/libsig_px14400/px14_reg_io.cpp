/** @file	px14_reg_io.cpp
  @brief	PX14 register IO routines
  */
#include "stdafx.h"
#include "px14.h"
#include "px14_private.h"
#include "px14_remote.h"

// Module-local function prototypes ------------------------------------- //

static int _ReadClkGenLogRegister (CStatePX14& state,
                                   unsigned int log_idx,
                                   unsigned int& regVal,
                                   unsigned int read_how);

// PX14 library exports implementation --------------------------------- //

PX14API RawRegWritePX14 (HPX14 hBrd, int region, int nReg, unsigned int val)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);
   if (statep->IsLocalVirtual())
      return SIG_SUCCESS;

   PX14S_RAW_REG_IO dreq;
   memset (&dreq, 0, sizeof(PX14S_RAW_REG_IO));
   dreq.bRead = 0;
   dreq.nRegion = region;
   dreq.nRegister = nReg;
   dreq.regVal = val;

   return DeviceRequest(hBrd, IOCTL_PX14_RAW_REG_IO,
                        reinterpret_cast<void*>(&dreq),
                        sizeof(PX14S_RAW_REG_IO), sizeof(PX14S_RAW_REG_IO));
}

PX14API RawRegReadPX14 (HPX14 hBrd, int region, int nReg, unsigned int* valp)
{
   CStatePX14* statep;
   int res;

   SIGASSERT_POINTER(valp, unsigned int);
   if (NULL == valp)
      return SIG_PX14_INVALID_ARG_3;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);
   if (statep->IsLocalVirtual())
      return SIG_SUCCESS;

   PX14S_RAW_REG_IO dreq;
   memset (&dreq, 0, sizeof(PX14S_RAW_REG_IO));
   dreq.bRead = 1;
   dreq.nRegion = region;
   dreq.nRegister = nReg;
   dreq.regVal = 0;

   res = DeviceRequest(hBrd, IOCTL_PX14_RAW_REG_IO,
                       reinterpret_cast<void*>(&dreq),
                       sizeof(PX14S_RAW_REG_IO), sizeof(PX14S_RAW_REG_IO));
   PX14_RETURN_ON_FAIL(res);

   *valp = dreq.regVal;
   return SIG_SUCCESS;
}

/// Write a PX14 device register
PX14API WriteDeviceRegPX14 (HPX14 hBrd,
                            unsigned int reg_idx,
                            unsigned int mask,
                            unsigned int val,
                            unsigned int pre_delay_us,
                            unsigned int post_delay_us,
                            unsigned int reg_set)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (statep->IsLocalVirtual())
   {
      // These parameters are actually validated in the driver so
      //  we'll only check them on virtual devices
      if (reg_idx >= PX14_DEVICE_REG_COUNT)
         return SIG_PX14_INVALID_ARG_2;
      if (pre_delay_us > PX14_MAX_DRIVER_DELAY)
         return SIG_PX14_INVALID_ARG_5;
      if (post_delay_us > PX14_MAX_DRIVER_DELAY)
         return SIG_PX14_INVALID_ARG_6;
      if ((reg_set != PX14REGSET_DEVICE) &&
          (reg_set != PX14REGSET_DRIVER))
      {
         return SIG_PX14_INVALID_ARG_7;
      }
   }
   else
   {
      PX14S_DEV_REG_WRITE dreq;
      dreq.struct_size = sizeof(PX14S_DEV_REG_WRITE);
      dreq.reg_set = reg_set;
      dreq.reg_idx = reg_idx;
      dreq.reg_mask = mask;
      dreq.reg_val = val;
      dreq.pre_delay_us = pre_delay_us;
      dreq.post_delay_us = post_delay_us;
      //dreq.cg_* only used for clock gen register IO

      res = DeviceRequest(hBrd, IOCTL_PX14_DEVICE_REG_WRITE,
                          reinterpret_cast<void*>(&dreq), sizeof(PX14S_DEV_REG_WRITE), 0);
      PX14_RETURN_ON_FAIL(res);
   }

   // Update our local register cache with new data
   if (PX14REGSET_DEVICE == reg_set)
   {
      if (reg_idx < PX14_DEVICE_REG_COUNT)
      {
         if (mask == 0xFFFFFFFF)
            statep->m_regDev.values[reg_idx] = val;
         else if (mask != 0)
         {
            statep->m_regDev.values[reg_idx] &= ~mask;
            statep->m_regDev.values[reg_idx] |= (mask & val);
         }
      }
   }
   else if (PX14REGSET_DRIVER == reg_set)
   {
      if (reg_idx < PX14_DRIVER_REG_COUNT)
      {
         if (mask == 0xFFFFFFFF)
            statep->m_regDriver.values[reg_idx] = val;
         else if (mask != 0)
         {
            statep->m_regDriver.values[reg_idx] &= ~mask;
            statep->m_regDriver.values[reg_idx] |= (mask & val);
         }
      }
   }

   return SIG_SUCCESS;
}

/** @brief Read a PX14 device register
*/
PX14API ReadDeviceRegPX14 (HPX14 hBrd,
                           unsigned int reg_idx,
                           unsigned int* reg_valp,
                           unsigned int read_how,
                           unsigned int reg_set)
{
   unsigned int reg_val;
   CStatePX14* statep;
   int res;

   SIGASSERT_NULL_OR_POINTER(reg_valp, unsigned int);
   if (read_how >= PX14REGREAD__COUNT)
      return SIG_PX14_INVALID_ARG_4;
   if ((reg_set != PX14REGSET_DEVICE) &&
       (reg_set != PX14REGSET_DRIVER))
   {
      return SIG_PX14_INVALID_ARG_5;
   }
   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (statep->IsVirtual() || (read_how == PX14REGREAD_USER_CACHE))
   {
      reg_val = 0;

      if (reg_set == PX14REGSET_DEVICE)
      {
         if (reg_idx < PX14_DEVICE_REG_COUNT)
            reg_val = statep->m_regDev.values[reg_idx];
      }
      else if (reg_set == PX14REGSET_DRIVER)
      {
         if (reg_idx < PX14_DRIVER_REG_COUNT)
            reg_val = statep->m_regDriver.values[reg_idx];
      }
   }
   else
   {
      // Ask driver to read device register
      PX14S_DEV_REG_READ dreq;
      dreq.struct_size = sizeof(PX14S_DEV_REG_READ);
      dreq.reg_set = reg_set;
      dreq.reg_idx = reg_idx;
      dreq.pre_delay_us = 0;
      dreq.post_delay_us = 0;
      dreq.read_how = read_how;

      res = DeviceRequest(hBrd, IOCTL_PX14_DEVICE_REG_READ,
                          reinterpret_cast<void*>(&dreq), sizeof(PX14S_DEV_REG_READ), 4);
      PX14_RETURN_ON_FAIL(res);

      // Driver returns register value in first 4 bytes of context struct
      reg_val = dreq.struct_size;

      // Update our local cache of register values
      if (reg_set == PX14REGSET_DEVICE)
      {
         if (reg_idx < PX14_DEVICE_REG_COUNT)
            statep->m_regDev.values[reg_idx] = reg_val;
      }
      else if (reg_set == PX14REGSET_DRIVER)
      {
         if (reg_idx < PX14_DRIVER_REG_COUNT)
            statep->m_regDriver.values[reg_idx] = reg_val;
      }
   }

   if (reg_valp)
      *reg_valp = reg_val;

   return SIG_SUCCESS;
}

/** @brief Write all device registers with currently cached values

  This function appears very similary in function
  RewriteHardwareSettingsPX14. The main difference is that this function
  just blindly rewrites all device registers with our cached values. The
  RewriteHardwareSettingsPX14 function actually rewrites each hardware
  setting with the corresponding (SetXXXPX14) function. This ensure all
  secondary library state is updated as well. *This* function is mainly
  for startup init and covers all register bits.

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API WriteAllDeviceRegistersPX14 (HPX14 hBrd)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   // Write operating mode register first
   res = WriteDeviceRegPX14(hBrd, 0xB, 0xFFFFFFFF, statep->m_regDev.values[0xB]);
   PX14_RETURN_ON_FAIL(res);

   res = WriteDeviceRegPX14(hBrd, 0, 0xFFFFFFFF, statep->m_regDev.values[0]);
   PX14_RETURN_ON_FAIL(res);
   res = WriteDeviceRegPX14(hBrd, 1, 0xFFFFFFFF, statep->m_regDev.values[1]);
   PX14_RETURN_ON_FAIL(res);
   res = WriteDeviceRegPX14(hBrd, 2, 0xFFFFFFFF, statep->m_regDev.values[2]);
   PX14_RETURN_ON_FAIL(res);
   res = WriteDeviceRegPX14(hBrd, 3, 0xFFFFFFFF, statep->m_regDev.values[3]);
   PX14_RETURN_ON_FAIL(res);
   res = WriteDeviceRegPX14(hBrd, 4, 0xFFFFFFFF, statep->m_regDev.values[4]);
   PX14_RETURN_ON_FAIL(res);
   res = WriteDeviceRegPX14(hBrd, 5, 0xFFFFFFFF, statep->m_regDev.values[5]);
   PX14_RETURN_ON_FAIL(res);
   res = WriteDeviceRegPX14(hBrd, 6, 0xFFFFFFFF, statep->m_regDev.values[6]);
   PX14_RETURN_ON_FAIL(res);
   res = WriteDeviceRegPX14(hBrd, 7, 0xFFFFFFFF, statep->m_regDev.values[7]);
   PX14_RETURN_ON_FAIL(res);
   // 8 unused
   res = WriteDeviceRegPX14(hBrd, 9, 0xFFFFFFFF, statep->m_regDev.values[9]);
   PX14_RETURN_ON_FAIL(res);
   // A: Special (FPGA processing parameters)
   // B: operating mode (first)
   // C: Read-only (RAM address)
   // D: Read only (Status)
   // E,F: Special: Timestamp read
   res = WriteDeviceRegPX14(hBrd, 0x10, 0xFFFFFFFF, statep->m_regDev.values[0x10]);
   PX14_RETURN_ON_FAIL(res);

   return SIG_SUCCESS;
}

/** @brief
  Read all device registers from hardware; updates all register caches

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.
  */
PX14API ReadAllDeviceRegistersPX14 (HPX14 hBrd)
{
   int res, i;

   for (i=0; i<PX14_DEVICE_REG_COUNT; i++)
   {
      if (!PX14_IS_SPECIAL_REG(i))
      {
         res = ReadDeviceRegPX14(hBrd, i, NULL, PX14REGREAD_HARDWARE);
         PX14_RETURN_ON_FAIL(res);
      }
   }

   return SIG_SUCCESS;
}

PX14API RefreshDriverHwConfigPX14 (HPX14 hBrd)
{
   return DeviceRequest(hBrd, IOCTL_PX14_HWCFG_REFRESH);
}

/** @brief Refresh local device register cache

  Call this function to update the local PX14 device register cache
  associated with the given PX14 device handle. The register values
  are obtained from the kernel-level register cache maintained by the
  PX14 device driver. Calling this function has no effect on any
  underlying PX14 hardware.

  This function is automatically invoked by the ConnectToDevicePX14
  function as part of the device connection procedure.

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param bFromHardware
  If this parameter is non-zero then register content will be updated
  from the PX14 hardware. If zero, the driver's local register cache
  will be consulted. Since all device writes go through the driver,
  the driver's cache will contain an updated cache of hardware
  register content. (Status registers are the exception to this.)
  */
PX14API RefreshLocalRegisterCachePX14 (HPX14 hBrd, int bFromHardware)
{
   unsigned int read_how;
   CStatePX14* statep;
   int i, res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (statep->IsRemote())
      return rmc_RefreshLocalRegisterCache(hBrd, bFromHardware);

   read_how = bFromHardware ? PX14REGREAD_HARDWARE : PX14REGREAD_CACHE_ONLY;

   if (!statep->IsVirtual())
   {
      // Read device registers
      for (i=0; i<PX14_DEVICE_REG_COUNT; i++)
      {
         if (PX14_IS_STATUS_REG(i))
            continue;

         res = ReadDeviceRegPX14(hBrd, i, NULL, read_how);
         PX14_RETURN_ON_FAIL(res);
      }

      // Read clock generation register cache
      for (i=0; i<PX14_CLKGEN_LOGICAL_REG_CNT; i++)
      {
         res = _ReadClkGenLogRegister(*statep, i,
                                      statep->m_regClkGen.values[i], read_how);
         PX14_RETURN_ON_FAIL(res);
      }

      // Read driver registers; these are software-only registers
      for (i=0; i<PX14_DRIVER_REG_COUNT; i++)
      {
         res = ReadDeviceRegPX14(hBrd, i, NULL,
                                 PX14REGREAD_CACHE_ONLY, PX14REGSET_DRIVER);
         PX14_RETURN_ON_FAIL(res);
      }
   }

   return SIG_SUCCESS;
}

PX14API WriteClockGenRegPX14 (HPX14 hBrd,
                              unsigned int reg_addr,
                              unsigned char mask,
                              unsigned char val,
                              unsigned int log_idx,
                              unsigned int byte_idx,
                              unsigned int pre_delay_us,
                              unsigned int post_delay_us,
                              int bUpdateRegs)
{
   int res, cache_idx;
   CStatePX14* statep;
   //bool bVirtual;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   //bVirtual = statep->IsVirtual();

   if (statep->IsLocalVirtual())
   {
      // These parameters are actually validated in the driver so we'll
      //  only check them on virtual devices
      if (reg_addr > PX14_CLKGEN_MAX_REG_IDX)
         return SIG_PX14_INVALID_ARG_3;
      if (pre_delay_us > PX14_MAX_DRIVER_DELAY)
         return SIG_PX14_INVALID_ARG_6;
      if (post_delay_us > PX14_MAX_DRIVER_DELAY)
         return SIG_PX14_INVALID_ARG_7;
   }
   else
   {
      PX14S_DEV_REG_WRITE dreq;
      dreq.struct_size = sizeof(PX14S_DEV_REG_WRITE);
      dreq.reg_set = PX14REGSET_CLKGEN;
      dreq.reg_idx = PX14REGIDX_CLKGEN;
      dreq.reg_mask = mask;
      dreq.reg_val = val;
      dreq.pre_delay_us = pre_delay_us;
      dreq.post_delay_us = post_delay_us;
      dreq.cg_log_reg = log_idx;
      dreq.cg_byte_idx = byte_idx;
      dreq.cg_addr = reg_addr;
      if (bUpdateRegs)
         dreq.cg_addr |= PX14CLKREGWRITE_UPDATE_REGS;

      res = DeviceRequest(hBrd, IOCTL_PX14_DEVICE_REG_WRITE,
                          reinterpret_cast<void*>(&dreq), sizeof(PX14S_DEV_REG_WRITE), 0);
      PX14_RETURN_ON_FAIL(res);
   }

   // Update our local register cache
   cache_idx = (log_idx << 2) + byte_idx;
   statep->m_regClkGen.bytes[cache_idx] &= ~mask;
   statep->m_regClkGen.bytes[cache_idx] |= (val & mask);

   return SIG_SUCCESS;
}

// Read one of the clock generator (logical) registers
PX14API ReadClockGenRegPX14 (HPX14 hBrd,
                             unsigned int reg_addr,
                             unsigned int log_idx,
                             unsigned int byte_idx,
                             unsigned char* reg_valp,
                             unsigned int read_how)
{
   unsigned char* cachep;
   CStatePX14* statep;
   int res;

   SIGASSERT_NULL_OR_POINTER(reg_valp, unsigned char);
   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (statep->IsVirtual() || (read_how == PX14REGREAD_USER_CACHE))
   {
      // These parameters are actually validated in the driver so we'll
      //  only check them on virtual devices
      if (read_how >= PX14REGREAD__COUNT)
         return SIG_PX14_INVALID_ARG_7;
      if (log_idx >= PX14_CLKGEN_LOGICAL_REG_CNT)
         return SIG_PX14_INVALID_ARG_4;
      if (reg_addr > PX14_CLKGEN_MAX_REG_IDX)
         return SIG_PX14_INVALID_ARG_3;

      if (NULL != reg_valp)
      {
         // Read from our software register cache
         cachep = reinterpret_cast<unsigned char*>(&statep->m_regClkGen);
         *reg_valp = cachep[(log_idx << 2) + byte_idx];
      }
   }
   else
   {
      unsigned int val32;

      PX14S_DEV_REG_READ dreq;
      dreq.struct_size = sizeof(PX14S_DEV_REG_READ);
      dreq.reg_set = PX14REGSET_CLKGEN;
      dreq.reg_idx = PX14REGIDX_CLKGEN;
      dreq.pre_delay_us = 0;
      dreq.post_delay_us = 0;
      dreq.read_how = read_how;
      dreq.cg_log_reg = log_idx;
      dreq.cg_byte_idx = byte_idx;
      dreq.cg_addr = reg_addr;

      res = DeviceRequest(hBrd, IOCTL_PX14_DEVICE_REG_READ,
                          reinterpret_cast<void*>(&dreq), sizeof(PX14S_DEV_REG_READ), 4);
      PX14_RETURN_ON_FAIL(res);

      // Driver returns register value in first 4 bytes of context struct
      val32 = dreq.struct_size;

      // Update our local software register cache
      cachep = statep->m_regClkGen.bytes;
      cachep[(log_idx << 2) + byte_idx] = static_cast<unsigned char>(val32);

      if (NULL != reg_valp)
         *reg_valp = static_cast<unsigned char>(val32);
   }

   return SIG_SUCCESS;
}

// Write DC-offset DAC
PX14API WriteDacRegPX14 (HPX14 hBrd,
                         unsigned int reg_data,
                         unsigned int reg_addr,
                         unsigned int command)
{
   PX14U_DACREGIO sdio;
   sdio.val = 0;
   sdio.bits.reg_addr = reg_addr;
   sdio.bits.reg_data = reg_data;
   sdio.bits.cmd      = command;

   return WriteDeviceRegPX14(hBrd, PX14REGIDX_DC_DAC, 0xFFFFFFFF, sdio.val);
}

// Module private function implementation ------------------------------- //

int _ReadClkGenLogRegister (CStatePX14& state,
                            unsigned int log_idx,
                            unsigned int& regVal,
                            unsigned int read_how)
{
   int res;

   if (state.IsVirtual())
   {
      // These parameters are actually validated in the driver so we'll
      //  only check them on virtual devices
      if (log_idx >= PX14_CLKGEN_LOGICAL_REG_CNT)
         return SIG_PX14_INVALID_ARG_3;

      regVal = state.m_regClkGen.values[log_idx];
   }
   else
   {
      PX14S_DEV_REG_READ dreq;
      dreq.struct_size = sizeof(PX14S_DEV_REG_READ);
      dreq.reg_set = PX14REGSET_CLKGEN;
      dreq.reg_idx = PX14REGIDX_CLKGEN;
      dreq.pre_delay_us = 0;
      dreq.post_delay_us = 0;
      dreq.read_how = read_how;
      dreq.cg_log_reg = log_idx;
      dreq.cg_byte_idx = 0;
      dreq.cg_addr = PX14CLKREGREAD_LOGICAL_REG_ONLY;

      res = DeviceRequest(PX14_B2H(&state), IOCTL_PX14_DEVICE_REG_READ,
                          reinterpret_cast<void*>(&dreq), sizeof(PX14S_DEV_REG_READ), 4);
      PX14_RETURN_ON_FAIL(res);

      // Driver returns register value in first 4 bytes of context struct
      regVal = dreq.struct_size;
   }

   return SIG_SUCCESS;
}


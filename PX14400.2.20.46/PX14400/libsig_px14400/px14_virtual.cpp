/** @file	px14_virtual.cpp
  @brief	Routines for virtual-only devices
  */
#include "stdafx.h"
#include "px14.h"
#include "px14_util.h"
#include "px14_private.h"

// Module-local function prototypes ------------------------------------- //

static int Virtual_AllocateDmaBuffer (HPX14 hBrd, PX14S_DMA_BUFFER_ALLOC* ctxp);
static int Virtual_FreeDmaBuffer (HPX14 hBrd, PX14S_DMA_BUFFER_FREE* ctxp);
static int Virtual_ReadDevReg (HPX14 hBrd, PX14S_DEV_REG_READ* ctxp);
static int Virtual_WriteDevReg (HPX14 hBrd, PX14S_DEV_REG_WRITE* ctxp);
static int Virtual_ReadTimestampData (HPX14 hBrd, PX14S_READ_TIMESTAMPS* ctxp);
static int Virtual_GetDeviceState (HPX14 hBrd, int* ctxp);
static int Virtual_EepromIo (HPX14 hBrd, PX14S_EEPROM_IO* ctxp);
static int Virtual_GetDriverVersion (HPX14 hBrd, PX14S_DRIVER_VER* ctxp);
static int Virtual_GetFwVersions (HPX14 hBrd, PX14S_FW_VERSIONS* ctxp);
static int Virtual_GetDriverStats (HPX14 hBrd, PX14S_DRIVER_STATS* ctxp);
static int Virtual_DmaXfer (HPX14 hBrd, PX14S_DMA_XFER* ctxp);
static int Virtual_NeedDcmReset (HPX14 hBrd, int* ctxp);
static int Virtual_ModeSet (HPX14 hBrd, int* ctxp);
static int Virtual_DcmRst (HPX14 hBrd);
static int Virtual_GetHwConfigEx (HPX14 hBrd, PX14S_HW_CONFIG_EX* ctxp);
static int Virtual_BootBufCtrl (HPX14 hBrd, PX14S_BOOTBUF_CTRL* ctxp);

static int PreAcqInit (HPX14 hBrd);
static void PostAcqCleanup (HPX14 hBrd);

// PX14 library exports implementation --------------------------------- //

int GenerateVirtualDataPX14 (HPX14 hBrd,
                             px14_sample_t* bufp,
                             unsigned int samples,
                             unsigned int sample_start,
                             int channel)
{
   static const double TwoPi = 2 * 3.14159265358979323846;
   static const double dPtsPerCycle = 12.3132;

   double dSamp, dFact;
   unsigned int i, at;

   // TODO : Update this implementation. See P X 1 5 0 0 - 4 (continuous data)

   if (sample_start == UINT_MAX)
   {
      // This is meant to generate continuous data over repeated calls...
      sample_start = 0;
   }

   dFact = TwoPi / dPtsPerCycle;
   at = sample_start;

   double rand_phase = ((rand() % 100) / 100.0) * (TwoPi / (dPtsPerCycle * 4));

   for (i=0; i<samples; i++,bufp++)
   {
      //dSamp = 32768 + (32000 * sin(at++ * dFact));
      dSamp = 32768 + (32000 * sin(at++ * dFact + rand_phase)) + (rand() % 160);

      if (dSamp >= PX14_SAMPLE_MAX_VALUE)
         *bufp = PX14_SAMPLE_MAX_VALUE - 1;
      else if (dSamp < 0)
         *bufp = PX14_SAMPLE_MIN_VALUE + 1;
      else
         *bufp = static_cast<px14_sample_t>(dSamp);
   }

   return SIG_SUCCESS;
}

int VirtualDriverRequest (HPX14 hBrd,
                          io_req_t req,
                          void* inp,
                          size_t in_bytes,
                          size_t out_bytes)
{
   int res;

   switch (req)
   {
      // Some things not virtualized; at least not yet
      case IOCTL_PX14_GET_DEVICE_ID:
      case IOCTL_PX14_RAW_REG_IO:
         res = SIG_PX14_NO_VIRTUAL_IMPLEMENTATION;
         break;

      case IOCTL_PX14_DEVICE_REG_WRITE:
         res = Virtual_WriteDevReg(hBrd,
                                   reinterpret_cast<PX14S_DEV_REG_WRITE*>(inp));
         break;
      case IOCTL_PX14_DEVICE_REG_READ:
         res = Virtual_ReadDevReg(hBrd,
                                  reinterpret_cast<PX14S_DEV_REG_READ*>(inp));
         break;

      case IOCTL_PX14_DMA_BUFFER_ALLOC:
         res = Virtual_AllocateDmaBuffer(hBrd,
                                         reinterpret_cast<PX14S_DMA_BUFFER_ALLOC*>(inp));
         break;
      case IOCTL_PX14_DMA_BUFFER_FREE:
         res = Virtual_FreeDmaBuffer(hBrd,
                                     reinterpret_cast<PX14S_DMA_BUFFER_FREE*>(inp));
         break;

      case IOCTL_PX14_READ_TIMESTAMPS:
         res = Virtual_ReadTimestampData(hBrd,
                                         reinterpret_cast<PX14S_READ_TIMESTAMPS*>(inp));
         break;

      case IOCTL_PX14_GET_DEVICE_STATE:
         res = Virtual_GetDeviceState(hBrd,
                                      reinterpret_cast<int*>(inp));
         break;

      case IOCTL_PX14_EEPROM_IO:
         res = Virtual_EepromIo(hBrd,
                                reinterpret_cast<PX14S_EEPROM_IO*>(inp));
         break;

      case IOCTL_PX14_DRIVER_VERSION:
         res = Virtual_GetDriverVersion(hBrd,
                                        reinterpret_cast<PX14S_DRIVER_VER*>(inp));
         break;

      case IOCTL_PX14_FW_VERSIONS:
         res = Virtual_GetFwVersions(hBrd,
                                     reinterpret_cast<PX14S_FW_VERSIONS*>(inp));
         break;

      case IOCTL_PX14_DRIVER_STATS:
         res = Virtual_GetDriverStats(hBrd,
                                      reinterpret_cast<PX14S_DRIVER_STATS*>(inp));
         break;

      case IOCTL_PX14_DMA_XFER:
         res = Virtual_DmaXfer (hBrd,
                                reinterpret_cast<PX14S_DMA_XFER*>(inp));
         break;

      case IOCTL_PX14_MODE_SET:
         res = Virtual_ModeSet (hBrd, reinterpret_cast<int*>(inp));
         break;

      case IOCTL_PX14_NEED_DCM_RESET:
         res = Virtual_NeedDcmReset(hBrd, reinterpret_cast<int*>(inp));
         break;

      case IOCTL_PX14_RESET_DCMS:
         res = Virtual_DcmRst(hBrd);
         break;

      case IOCTL_PX14_GET_HW_CONFIG_EX:
         res = Virtual_GetHwConfigEx(hBrd, reinterpret_cast<PX14S_HW_CONFIG_EX*>(inp));
         break;

      case IOCTL_PX14_BOOTBUF_CTRL:
         res = Virtual_BootBufCtrl(hBrd, reinterpret_cast<PX14S_BOOTBUF_CTRL*>(inp));
         break;
         // Don't need to virtualize any behavior for these guys
      case IOCTL_PX14_JTAG_IO:
      case IOCTL_PX14_JTAG_STREAM:
      case IOCTL_PX14_DRIVER_BUFFERED_XFER:
      case IOCTL_PX14_WAIT_ACQ_OR_XFER:
      case IOCTL_PX14_US_DELAY:
      case IOCTL_PX14_HWCFG_REFRESH:
      case IOCTL_PX14_DBG_REPORT:
         res = SIG_SUCCESS;
         break;

      default:
         res = SIG_PX14_NOT_IMPLEMENTED;
   }

   return res;
}

int VirtualInitSwrCache (HPX14 hBrd)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   // -- Device registers
   statep->m_regDev.values[0x0] = PX14REGDEFAULT_0;
   statep->m_regDev.values[0x1] = PX14REGDEFAULT_1;
   statep->m_regDev.values[0x2] = PX14REGDEFAULT_2;
   statep->m_regDev.values[0x3] = PX14REGDEFAULT_3;
   statep->m_regDev.values[0x4] = PX14REGDEFAULT_4;
   statep->m_regDev.values[0x5] = PX14REGDEFAULT_5;
   statep->m_regDev.values[0x6] = PX14REGDEFAULT_6;
   statep->m_regDev.values[0x7] = PX14REGDEFAULT_7;
   statep->m_regDev.values[0x8] = PX14REGDEFAULT_8;
   statep->m_regDev.values[0x9] = PX14REGDEFAULT_9;
   statep->m_regDev.values[0xA] = PX14REGDEFAULT_A;
   statep->m_regDev.values[0xB] = PX14REGDEFAULT_B;
   statep->m_regDev.values[0xC] = PX14REGDEFAULT_C;
   statep->m_regDev.values[0xD] = 0;
   statep->m_regDev.values[0xE] = 0;
   statep->m_regDev.values[0xF] = 0;
   statep->m_regDev.values[0x10] = PX14REGDEFAULT_10;
   statep->m_regDev.values[0x11] = PX14REGDEFAULT_11;
   PX14_CT_ASSERT(18 == PX14_DEVICE_REG_COUNT);

   // Clock gen registers
   statep->m_regClkGen.values[ 0] = PX14CLKREGDEF_00;
   statep->m_regClkGen.values[ 1] = PX14CLKREGDEF_01;
   statep->m_regClkGen.values[ 2] = PX14CLKREGDEF_02;
   statep->m_regClkGen.values[ 3] = PX14CLKREGDEF_03;
   statep->m_regClkGen.values[ 4] = PX14CLKREGDEF_04;
   statep->m_regClkGen.values[ 5] = PX14CLKREGDEF_05;
   statep->m_regClkGen.values[ 6] = PX14CLKREGDEF_06;
   statep->m_regClkGen.values[ 7] = PX14CLKREGDEF_07;
   statep->m_regClkGen.values[ 8] = PX14CLKREGDEF_08;
   statep->m_regClkGen.values[ 9] = PX14CLKREGDEF_09;
   statep->m_regClkGen.values[10] = PX14CLKREGDEF_10;
   statep->m_regClkGen.values[11] = PX14CLKREGDEF_11;
   statep->m_regClkGen.values[12] = PX14CLKREGDEF_12;
   statep->m_regClkGen.values[13] = PX14CLKREGDEF_13;
   statep->m_regClkGen.values[14] = PX14CLKREGDEF_14;
   statep->m_regClkGen.values[15] = PX14CLKREGDEF_15;
   statep->m_regClkGen.values[16] = PX14CLKREGDEF_16;
   statep->m_regClkGen.values[17] = PX14CLKREGDEF_17;
   statep->m_regClkGen.values[18] = PX14CLKREGDEF_18;
   statep->m_regClkGen.values[19] = PX14CLKREGDEF_19;
   PX14_CT_ASSERT(20 == PX14_CLOCK_REG_COUNT);

   if (statep->IsPX12500())
   {
      statep->m_regClkGen.values[12] = PX14CLKREGDEF_12_PX12500;
      statep->m_regClkGen.values[19] = PX14CLKREGDEF_19_PX12500;
   }

   // Driver registers
   statep->m_regDriver.values[0] = PX14DREGDEFAULT_0;
   statep->m_regDriver.values[1] = PX14DREGDEFAULT_1;
   statep->m_regDriver.values[2] = PX14DREGDEFAULT_2;
   statep->m_regDriver.values[3] = PX14DREGDEFAULT_3;
   statep->m_regDriver.values[4] = PX14DREGDEFAULT_4;
   statep->m_regDriver.values[5] = PX14DREGDEFAULT_5;
   PX14_CT_ASSERT(6 == PX14_DRIVER_REG_COUNT);

   if (statep->IsPX12500())
   {
      statep->m_regDriver.values[1] = PX14DREGDEFAULT_1_PX12500;
      statep->m_regDriver.values[2] = PX14DREGDEFAULT_2_PX12500;
      statep->m_regDriver.values[3] = PX14DREGDEFAULT_3_PX12500;
      statep->m_regDriver.values[4] = PX14DREGDEFAULT_4_PX12500;
   }

   if (statep->m_features & PX14FEATURE_NO_INTERNAL_CLOCK)
   {
      statep->m_regDev.fields.reg5.bits.CLKSRC = PX14CLKSRC_EXTERNAL;
      statep->m_regClkGen.fields.reg01.bits.PLL_PowerDown = 1;
      statep->m_regClkGen.fields.reg19.bits.BypassVcoDivider = 1;
   }

   return SIG_SUCCESS;
}

int Virtual_AllocateDmaBuffer (HPX14 hBrd, PX14S_DMA_BUFFER_ALLOC* ctxp)
{
   px14_sample_t* vbufp;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_DMA_BUFFER_ALLOC, "Virtual_AllocateDmaBuffer");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_DMA_BUFFER_ALLOC_V1, "Virtual_AllocateDmaBuffer");

   // For a virtual device, we'll just allocate a normal heap-allocated
   // buffer.

#ifdef _WIN32

   // Use VirualAlloc on Windows platforms so resultant buffer
   //  will always be okay for non-buffered IO
   vbufp = reinterpret_cast<px14_sample_t*>(VirtualAlloc(NULL,
                                                         ctxp->req_bytes, MEM_COMMIT, PAGE_EXECUTE_READWRITE));

#else

   // Just do a heap allocated buffer for virtual devices
   vbufp = reinterpret_cast<px14_sample_t*>(my_malloc(ctxp->req_bytes));

#endif

   if (NULL == vbufp)
      return SIG_DMABUFALLOCFAIL;

   ctxp->virt_addr =
      static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(vbufp));

   return SIG_SUCCESS;
}

int Virtual_FreeDmaBuffer (HPX14 hBrd, PX14S_DMA_BUFFER_FREE* ctxp)
{
   px14_sample_t* bufp;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_DMA_BUFFER_FREE, "Virtual_FreeDmaBuffer");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_DMA_BUFFER_FREE_V1, "Virtual_FreeDmaBuffer");

   // We just use a heap-allocated buffer for virtual devices

   bufp = *reinterpret_cast<px14_sample_t**>(&ctxp->virt_addr);

#ifdef _WIN32
   VirtualFree(bufp, 0, MEM_RELEASE);
#else
   my_free(bufp);
#endif

   return SIG_SUCCESS;
}

int Virtual_ReadDevReg (HPX14 hBrd, PX14S_DEV_REG_READ* ctxp)
{
   CStatePX14* statep;
   int res;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_DEV_REG_READ, "Virtual_ReadDevReg");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_DEV_REG_READ_V1, "Virtual_ReadDevReg");

   statep = PX14_H2B(hBrd);
   res = SIG_SUCCESS;

   switch (ctxp->reg_set)
   {
      case PX14REGSET_DEVICE:
         {
            ctxp->struct_size = 0;

            if (ctxp->reg_set == PX14REGSET_DEVICE)
            {
               if (ctxp->reg_idx < PX14_DEVICE_REG_COUNT)
                  ctxp->struct_size  = statep->m_regDev.values[ctxp->reg_idx];
            }
            else if (ctxp->reg_set == PX14REGSET_DRIVER)
            {
               if (ctxp->reg_idx < PX14_DRIVER_REG_COUNT)
               {
                  ctxp->struct_size =
                     statep->m_regDriver.values[ctxp->reg_idx];
               }
            }
         }
         break;

      case PX14REGSET_CLKGEN:
         {
            if (ctxp->read_how >= PX14REGREAD__COUNT)
               return SIG_INVALIDARG;
            if (ctxp->cg_log_reg >= PX14_CLKGEN_LOGICAL_REG_CNT)
               return SIG_INVALIDARG;
            if (ctxp->cg_addr > PX14_CLKGEN_MAX_REG_IDX)
               return SIG_INVALIDARG;

            // Read from our software register cache
            ctxp->struct_size =
               statep->m_regClkGen.bytes[(ctxp->cg_log_reg << 2) +
               ctxp->cg_byte_idx];
         }
         break;

      case PX14REGSET_DRIVER:
         {
            if (ctxp->cg_log_reg >= PX14_CLKGEN_LOGICAL_REG_CNT)
               return SIG_INVALIDARG;

            ctxp->struct_size =
               statep->m_regDriver.values[ctxp->cg_log_reg];
         }
         break;

      default:
         res = SIG_INVALIDARG;
   }

   return res;
}

int Virtual_WriteDevReg(HPX14 hBrd, PX14S_DEV_REG_WRITE* ctxp)
{
   CStatePX14* statep;
   int res;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_DEV_REG_WRITE, "Virtual_WriteDevReg");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_DEV_REG_WRITE_V1, "Virtual_WriteDevReg");

   statep = PX14_H2B(hBrd);
   res = SIG_SUCCESS;

   switch (ctxp->reg_set)
   {
      case PX14REGSET_DEVICE:
      case PX14REGSET_DRIVER:
         {
            if (ctxp->reg_idx >= PX14_DEVICE_REG_COUNT)
               return SIG_INVALIDARG;
            if (ctxp->pre_delay_us > PX14_MAX_DRIVER_DELAY)
               return SIG_INVALIDARG;
            if (ctxp->post_delay_us > PX14_MAX_DRIVER_DELAY)
               return SIG_INVALIDARG;

            if (ctxp->reg_set == PX14REGSET_DEVICE)
            {
               if (ctxp->reg_idx < PX14_DEVICE_REG_COUNT)
               {
                  unsigned& cache_val =
                     statep->m_regDev.values[ctxp->reg_idx];

                  if (ctxp->reg_mask == UINT_MAX)
                     cache_val = ctxp->reg_val;
                  else
                  {
                     cache_val &= ~ctxp->reg_mask;
                     cache_val |= (ctxp->reg_mask & ctxp->reg_val);
                  }

                  // Special case: virtual implementation tracks starting
                  // address so we can generate continuous virtual data
                  // nicely
                  if (ctxp->reg_idx == PX14REGIDX_START_SAMPLE)
                  {
                     statep->m_virtual_statep->m_startAddr =
                        GetStartSamplePX14(hBrd);
                  }
               }
            }
            else
            {
               if (ctxp->reg_idx < PX14_DRIVER_REG_COUNT)
               {
                  unsigned& cache_val =
                     statep->m_regDriver.values[ctxp->reg_idx];

                  if (ctxp->reg_mask == UINT_MAX)
                     cache_val = ctxp->reg_val;
                  else
                  {
                     cache_val &= ~ctxp->reg_mask;
                     cache_val |= (ctxp->reg_mask & ctxp->reg_val);
                  }
               }
            }
         }
         break;

      case PX14REGSET_CLKGEN:
         {
            int cache_idx;

            // We don't care about this bit
            ctxp->cg_addr &= ~PX14CLKREGWRITE_UPDATE_REGS;

            if (ctxp->cg_addr > PX14_CLKGEN_MAX_REG_IDX)
               return SIG_INVALIDARG;
            if (ctxp->pre_delay_us > PX14_MAX_DRIVER_DELAY)
               return SIG_INVALIDARG;
            if (ctxp->post_delay_us > PX14_MAX_DRIVER_DELAY)
               return SIG_INVALIDARG;

            // Update our local register cache
            cache_idx = (ctxp->cg_log_reg << 2) + ctxp->cg_byte_idx;
            statep->m_regClkGen.bytes[cache_idx] &= ~ctxp->reg_mask;
            statep->m_regClkGen.bytes[cache_idx] |=
               (ctxp->reg_val & ctxp->reg_mask);
         }
         break;

      default:
         res = SIG_INVALIDARG;
   }

   return res;
}

int Virtual_ReadTimestampData(HPX14 hBrd, PX14S_READ_TIMESTAMPS* ctxp)
{
   px14_timestamp_t* bufp;
   unsigned int i;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_READ_TIMESTAMPS, "Virtual_ReadTimestampData");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_PX14S_READ_TIMESTAMPS_V1, "Virtual_ReadTimestampData");

   bufp = *reinterpret_cast<px14_timestamp_t**>(&ctxp->user_virt_addr);
   SIGASSERT_POINTER(bufp, px14_timestamp_t);
   if (NULL == bufp)
      return SIG_INVALIDARG;

   // Just write some counting values; just want to test all of given
   //  buffer
   for (i=0; i<ctxp->buffer_items; i++,bufp++)
      *bufp = i << 3;

   ctxp->flags &= ~PX14TSREAD__OUTPUT_FLAG_MASK;

   return SIG_SUCCESS;
}

int Virtual_GetDeviceState(HPX14 hBrd, int* ctxp)
{
   PX14_ENSURE_POINTER(hBrd, ctxp, int, "Virtual_GetDeviceState");

   *ctxp = PX14_H2B(hBrd)->m_virtual_statep->m_devState;
   return SIG_SUCCESS;
}

int Virtual_EepromIo(HPX14 hBrd, PX14S_EEPROM_IO* ctxp)
{
   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_EEPROM_IO, "Virtual_EepromIo");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_EEPROM_IO_V1, "Virtual_EepromIo");

   if (ctxp->bRead)
      ctxp->eeprom_val = 0x0000;

   return SIG_SUCCESS;
}

int Virtual_GetDriverVersion(HPX14 hBrd, PX14S_DRIVER_VER* ctxp)
{
   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_DRIVER_VER, "Virtual_GetDriverVersion");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_PX14_DRIVER_VER_V1, "Virtual_GetDriverVersion");

   ctxp->version = 0;
   return SIG_SUCCESS;
}

int Virtual_GetFwVersions (HPX14 hBrd, PX14S_FW_VERSIONS* ctxp)
{
   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_FW_VERSIONS, "Virtual_GetFwVersions");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_PX14S_FW_VERSIONS_V1, "Virtual_GetFwVersions");

   ctxp->fw_ver_pci = 0;
   ctxp->fw_ver_sab = 0;
   ctxp->fw_ver_pkg = 0;
   ctxp->pre_rel_flags = 0;

   return SIG_SUCCESS;
}

int Virtual_GetDriverStats (HPX14 hBrd, PX14S_DRIVER_STATS* ctxp)
{
   PX14S_DRIVER_STATS* virt_statsp;
   unsigned bytes_to_copy;
   CStatePX14* statep;
   bool bReset;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_DRIVER_STATS, "Virtual_GetDriverStats");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_PX14S_DRIVER_STATS_V1, "Virtual_GetDriverStats");

   statep = PX14_H2B(hBrd);

   SIGASSERT_POINTER(statep->m_virtual_statep, CVirtualCtxPX14);
   if (statep->m_virtual_statep)
   {
      virt_statsp = &statep->m_virtual_statep->m_drvStats;

      bReset = 0 != ctxp->nConnections;

      bytes_to_copy =
         PX14_MIN(sizeof(PX14S_DRIVER_STATS), ctxp->struct_size);
      memcpy (ctxp, virt_statsp, bytes_to_copy);

      if (bReset)
      {
         memset (virt_statsp, 0, sizeof(PX14S_DRIVER_STATS));
         virt_statsp->struct_size = sizeof(PX14S_DRIVER_STATS);
         virt_statsp->nConnections = 1;
      }
   }

   return SIG_SUCCESS;
}

int Virtual_DmaXfer (HPX14 hBrd, PX14S_DMA_XFER* ctxp)
{
   PX14S_DRIVER_STATS* virt_statsp;
   CVirtualCtxPX14* virt_statep;
   unsigned sample_count;
   px14_sample_t* bufp;
   int res;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_DMA_XFER, "Virtual_DmaXfer");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_DMA_XFER_V1, "Virtual_DmaXfer");

   virt_statep = PX14_H2B(hBrd)->m_virtual_statep;
   SIGASSERT_POINTER(virt_statep, CVirtualCtxPX14);

   // Generate virtual data for transfer
   sample_count = ctxp->xfer_bytes / sizeof(px14_sample_t);
   if (ctxp->bRead)
   {
      bufp = *reinterpret_cast<px14_sample_t**>(&ctxp->virt_addr);
      SIGASSERT_POINTER(bufp, px14_sample_t);
      if (NULL == bufp)
         return SIG_INVALIDARG;
      res = GenerateVirtualDataPX14(hBrd, bufp, sample_count, virt_statep->m_startAddr, 0);
      PX14_RETURN_ON_FAIL(res);
   }
   virt_statep->m_startAddr += sample_count;

   // Update virtual driver stats
   virt_statsp = &virt_statep->m_drvStats;
   virt_statsp->dma_started_cnt++;
   virt_statsp->isr_cnt++;
   virt_statsp->dma_bytes += ctxp->xfer_bytes;
   virt_statsp->dma_finished_cnt++;

   return SIG_SUCCESS;
}

int Virtual_NeedDcmReset (HPX14 hBrd, int* ctxp)
{
   PX14_ENSURE_POINTER(hBrd, ctxp, int, "Virtual_NeedDcmReset");

   PX14_H2B(hBrd)->m_virtual_statep->m_bNeedDcmRst = (*ctxp != 0);
   return SIG_SUCCESS;
}

int Virtual_DcmRst (HPX14 hBrd)
{
   PX14_H2B(hBrd)->m_virtual_statep->m_drvStats.dcm_reset_cnt++;
   return SIG_SUCCESS;
}

int Virtual_ModeSet (HPX14 hBrd, int* ctxp)
{
   int res, op_mode_to, op_mode_from;
   CVirtualCtxPX14* virt_statep;

   PX14_ENSURE_POINTER(hBrd, ctxp, int, "Virtual_ModeSet");

   op_mode_from = GetOperatingModePX14(hBrd);
   op_mode_to = *ctxp;

   // Ignore call if not changing operating mode
   if (op_mode_from == op_mode_to)
      return SIG_SUCCESS;
   // Must always be coming from or going to standby mode
   if ((op_mode_from != PX14MODE_STANDBY) &&
       (op_mode_to   != PX14MODE_STANDBY))
   {
      return SIG_PX14_INVALID_MODE_CHANGE;
   }

   virt_statep = PX14_H2B(hBrd)->m_virtual_statep;
   SIGASSERT_POINTER(virt_statep, CVirtualCtxPX14);

   // Handle pre-acquisition init if necessary
   if (PX14_IS_ACQ_MODE(op_mode_to))
   {
      res = PreAcqInit(hBrd);
      PX14_RETURN_ON_FAIL(res);
   }

   // And coming from acquisition mode?
   if (PX14_IS_ACQ_MODE(op_mode_from))
   {
      PostAcqCleanup(hBrd);
   }
   //else if (PX14MODE_OFF == op_mode_to)
   //{
   //}
   //else if (PX14MODE_OFF == op_mode_from)
   //{
   //}

   if (op_mode_to == PX14MODE_STANDBY)
   {
      // Reset starting address like the firmware does
      virt_statep->m_startAddr = GetStartSamplePX14(hBrd);
      // Reset device state
      virt_statep->m_devState = PX14STATE_IDLE;
   }

   return SIG_SUCCESS;
}

int PreAcqInit (HPX14 hBrd)
{
   CVirtualCtxPX14* virt_statep;

   virt_statep = PX14_H2B(hBrd)->m_virtual_statep;
   SIGASSERT_POINTER(virt_statep, CVirtualCtxPX14);

   virt_statep->m_drvStats.acq_started_cnt++;
   virt_statep->m_drvStats.acq_finished_cnt++;
   virt_statep->m_devState = PX14STATE_ACQ;

   if (virt_statep->m_bNeedDcmRst)
   {
      virt_statep->m_drvStats.dcm_reset_cnt++;
      virt_statep->m_bNeedDcmRst = false;
   }

   return SIG_SUCCESS;
}

void PostAcqCleanup (HPX14 hBrd)
{
   // Nothing to do now; might have some power management stuff at
   //  some point; can turn of amplifiers or something
}

int Virtual_BootBufCtrl (HPX14 hBrd, PX14S_BOOTBUF_CTRL* ctxp)
{
   int res;

   PX14_ENSURE_POINTER    (hBrd, ctxp, PX14S_BOOTBUF_CTRL,            "Virtual_BootBufCtrl");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_PX14S_BOOTBUF_CTRL_V1, "Virtual_BootBufCtrl");

   if (ctxp->struct_size < _PX14SO_PX14S_BOOTBUF_CTRL_V1)
      return SIG_INVALIDARG;
   if (ctxp->operation >= PX14BBOP__COUNT)
      return SIG_INVALIDARG;

   // Some operations require a specific buffer index
   if ((ctxp->operation == PX14BBOP_CHECK_OUT) || (ctxp->operation == PX14BBOP_QUERY))
   {
      if (ctxp->buf_idx >= PX14_BOOTBUF_COUNT)
         return SIG_INVALIDARG;
   }

   switch (ctxp->operation)
   {
      default:
      case PX14BBOP_CHECK_OUT:
      case PX14BBOP_CHECK_IN:
      case PX14BBOP_REALLOC:
         res = SIG_PX14_NO_VIRTUAL_IMPLEMENTATION;
         break;

      case PX14BBOP_QUERY:
         ctxp->buf_samples = 0;
         ctxp->virt_addr   = 0;
         res = SIG_SUCCESS;
         break;
   }

   return res;
}

int Virtual_GetHwConfigEx (HPX14 hBrd, PX14S_HW_CONFIG_EX* ctxp)
{
   unsigned dwStructInBytes;

   //if ((dwInputSize  < 4) || (dwOutputSize < _PX14SO_PX14S_HW_CONFIG_EX_V1))
   //	return STATUS_INFO_LENGTH_MISMATCH;

   PX14_ENSURE_POINTER(hBrd, ctxp, PX14S_HW_CONFIG_EX, "Virtual_GetHwConfigEx");
   PX14_ENSURE_STRUCT_SIZE(hBrd, ctxp, _PX14SO_PX14S_HW_CONFIG_EX_V1, "Virtual_GetHwConfigEx");

   dwStructInBytes = ctxp->struct_size;

   // PX14400A
   ctxp->struct_size   = _PX14SO_PX14S_HW_CONFIG_EX_V1;
   ctxp->chan_imps     = 0;	// 0x0202; <- DC coupled
   ctxp->chan_filters  = (PX14CHANFILTER_LOWPASS_200MHZ << 8) | PX14CHANFILTER_LOWPASS_200MHZ;
   ctxp->sys_fpga_type = PX14SYSFPGA_V5_LX50T;
   ctxp->sab_fpga_type = PX14SABFPGA_V5_SX50T;
   ctxp->board_rev     = PX14BRDREV_PX14400;	//PX14BRDREV_PX14400 PX14BRDREV_PX12500;
   ctxp->board_rev_sub = PX14BRDREVSUB_1_SP;

   // Default to a PX14400D2
   //	{
   //#pragma message ("------- VIRTUAL DEVICES ARE: PX14BRDREV_PX14400D2\n")

   if (dwStructInBytes > _PX14SO_PX14S_HW_CONFIG_EX_V1)
   {
      ctxp->struct_size = _PX14SO_PX14S_HW_CONFIG_EX_V2;
      ctxp->custFwPkg   = 0;
      ctxp->custFwPci   = 0;
      ctxp->custFwSab   = 0;
      ctxp->custHw      = 0;
   }

   return SIG_SUCCESS;
}

/** @file	px14_dmabuf.cpp
	@brief	PX14 DMA buffer allocation routines
*/
#include "stdafx.h"
#include "px14.h"
#include "px14_private.h"
#include "px14_util.h"

#define PX14_DMA_CHAIN_MAGIC				0x14DACA19

#define PX14_BUFCHAIN_BUF_COUNT(p)			\
	static_cast<int>(reinterpret_cast<intptr_t>(((p)[-1].bufp)))
#define PX14_BUFCHAIN_TOTAL_SAMPLES(p)		\
	((p)[PX14_BUFCHAIN_BUF_COUNT(p)].buf_samples)

typedef std::list<PX14S_BUFNODE> BufList_t;

// PX14 library exports implementation --------------------------------- //

/** @brief Allocate a DMA buffer for use with DMA transfers

	This function is used to allocate physically contiguous, non-paged space
	in PC memory for DMA transfers. The allocated buffer (or any region
	therein) may then be used by functions that require a DMA buffer. Note
	that the only way to get data from the board directly to the PC is via a
	DMA transfer. A DMA buffer must be freed via the FreeDmaBufferPX14
   function when it is no longer needed.

   The DMA buffer may only be used with the PX14 device that it was
   allocated for. A DMA buffer can be used with another PX14 device handle
   (HPX14) of the same process as long as both device handles refer to the
   same underlying PX14 device.

   All 'Fast' PX14 library functions (e.g. ReadSampleRamFastPX14,
   WriteSampleRamFastPX14) require a DMA buffer as a parameter. These
   functions are faster than their counterparts because the driver can
   perform the DMA transfer directly to the DMA buffer which is mapped in
   the user process. The "slower" functions work by using a smaller
   driver-allocated DMA buffer to perform the data transfers which is less
   efficient.

   The maximum size of a DMA buffer is system-dependent.

   On most platforms, the actual amount of memory allocated will usually be
   rounded up to the system page boundary. (Typically 4096 bytes.)

Windows: The virtual address of any allocated DMA buffer should be on a
64KB boundary. This allows DMA buffers to be used for non-buffered file
IO routines. (See FILE_FLAG_NO_BUFFERING documentation for the Win32
CreateFile function.)

The PX14 driver will ensure that all un-freed DMA buffers for a given
process will be freed when that process' last handle is closed. That is
to say, like normal, heap-allocated memory, the underlying PX14
software will ensure that all DMA buffers are properly cleaned up when
a process exits, gracefully or not.

@param hBrd
A handle to the PX14400 board. This handle is obtained by calling
the ConnectToDevicePX14 function.
@param samples
The number of samples to allocate for the buffer. There is no
explicit upper bound to the size of a DMA buffer, it is entirely
dependent on the available resources and the host operating system.
@param bufpp
A pointer to a DMA buffer pointer that will receive the virtual
address of the DMA buffer. This buffer is fully mapped into the
calling process' address space. That is, it can be treated just like
normal memory.

@retval
Returns SIG_SUCCESS on success of one of the SIG_* error values
(which are all negative) on error.

@sa FreeDmaBufferPX14
*/
PX14API AllocateDmaBufferPX14 (HPX14 hBrd, unsigned int samples,
                               px14_sample_t** bufpp)
{
   PX14U_ADDR_UNION au;
   size_t buffer_bytes;
   int res;

   SIGASSERT_POINTER(bufpp, px14_sample_t*);
   if (!samples)
      return SIG_PX14_INVALID_ARG_2;
   if (!bufpp)
      return SIG_PX14_INVALID_ARG_3;

   buffer_bytes = samples * sizeof(px14_sample_t);

   // Ask the driver to allocate a DMA buffer
   PX14S_DMA_BUFFER_ALLOC dreq;
   memset (&dreq, 0, sizeof(PX14S_DMA_BUFFER_ALLOC));
   dreq.struct_size = sizeof(PX14S_DMA_BUFFER_ALLOC);
   dreq.req_bytes = static_cast<unsigned int>(buffer_bytes);
   dreq.virt_addr = 0;

   res = DeviceRequest(hBrd, IOCTL_PX14_DMA_BUFFER_ALLOC,
                       reinterpret_cast<void*>(&dreq),
                       sizeof(PX14S_DMA_BUFFER_ALLOC), sizeof(PX14S_DMA_BUFFER_ALLOC));
   PX14_RETURN_ON_FAIL(res);

#ifdef __linux__
   CStatePX14* statep = PX14_H2B(hBrd);
   if (!statep->IsVirtual()) {

      void* voidp;

      // On Linux, we're given the kernel's virtual address which has no
      //  meaning here in user-space. We can get an equivalent user-space
      //  address that we can use here in this process via mmap
      voidp = mmap(NULL, dreq.req_bytes, PROT_READ|PROT_WRITE,
                   MAP_SHARED, statep->m_hDev, (off_t)dreq.virt_addr);

      if ((NULL == voidp) || (MAP_FAILED == voidp)) {
         // Failed to map, free the DMA buffer. Since we don't have a
         //  user-space address we need a special case to indicate which
         //  buffer to free. Linux driver will recognize the
         //  ~PX14_FREE_ALL_DMA_BUFFERS
         PX14S_DMA_BUFFER_FREE reqFree;
         reqFree.struct_size = sizeof(PX14S_DMA_BUFFER_FREE);
         reqFree.free_all = ~PX14_FREE_ALL_DMA_BUFFERS;
         reqFree.virt_addr = dreq.virt_addr;
         DeviceRequest(hBrd, IOCTL_PX14_DMA_BUFFER_FREE,
                       reinterpret_cast<void*>(&reqFree),
                       sizeof(PX14S_DMA_BUFFER_FREE),
                       sizeof(PX14S_DMA_BUFFER_FREE));

         return SIG_ERROR;
      }

      *bufpp = reinterpret_cast<px14_sample_t*>(voidp);
   }
   else {
      au.addr_int = dreq.virt_addr;
      *bufpp = au.sampp;
   }
#else

   au.addr_int = dreq.virt_addr;
   *bufpp = au.sampp;

#endif

   return SIG_SUCCESS;
}

/** @brief
  Free a DMA buffer previously allocated by the AllocateDmaBufferPX14
  function

  The PX14 driver will automatically free any DMA buffers that have not
  been freed by a process when it exits.

  Attempting to access DMA buffer memory after is has been freed can
  result in an application crash.

  @param hBrd
  A handle to the PX14 board. This handle is obtained by calling the
  ConnectToDevicePX14 function.
  @param bufp
  The address of the DMA buffer to free.
  */
PX14API FreeDmaBufferPX14 (HPX14 hBrd, px14_sample_t* bufp)
{
   SIGASSERT_NULL_OR_POINTER(bufp, px14_sample_t);
   if (!bufp)
      return SIG_SUCCESS;

   // Ask the driver to free the DMA buffer
   PX14S_DMA_BUFFER_FREE dreq;
   memset (&dreq, 0, sizeof(PX14S_DMA_BUFFER_FREE));
   dreq.struct_size = sizeof(PX14S_DMA_BUFFER_FREE);
   dreq.virt_addr = reinterpret_cast<uintptr_t>(bufp);
   dreq.free_all = 0;

   return DeviceRequest(hBrd, IOCTL_PX14_DMA_BUFFER_FREE,
                        reinterpret_cast<void*>(&dreq), sizeof(PX14S_DMA_BUFFER_FREE));
}


/** @brief
  Ensures that the library managed utility DMA buffer is of the given
  size

  This function is used to ensure that the library managed utility DMA
  buffer is at least as large as the given sample count. In the event
  that the current buffer is too small, it will be freed and a new
  buffer will be allocated. Previous data is not guaranteed.

  Each PX14 handle may contain a utility DMA buffer. The address of this
  DMA buffer is obtained by calling the GetUtilityDmaBufferPX14 function.

  This buffer may be freed by calling the FreeUtilityDmaBufferPX14
  function. This buffer is also automatically freed when the associated
  PX14 handle is closed.

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param sample_count
  The minimum sample count that the utility DMA buffer for the given
  PX14 handle should be

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.

  @see FreeUtilityDmaBufferPX14, GetUtilityDmaBufferPX14

*/
PX14API EnsureUtilityDmaBufferPX14 (HPX14 hBrd, unsigned int sample_count)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   // Do we already have a large enough buffer?
   if (statep->m_utility_bufp &&
       statep->m_utility_buf_samples >= sample_count)
   {
      return SIG_SUCCESS;
   }

   // Need to allocate a larger buffer so free what we've got
   FreeUtilityDmaBufferPX14(hBrd);

   res = AllocateDmaBufferPX14(hBrd, sample_count, &statep->m_utility_bufp);
   PX14_RETURN_ON_FAIL(res);

   statep->m_utility_buf_samples = sample_count;
   return SIG_SUCCESS;
}

/** @brief Frees the utility buffer associated with the given PX14 handle

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.

  @see EnsureUtilityDmaBufferPX14, GetUtilityDmaBufferPX14
  */
PX14API FreeUtilityDmaBufferPX14 (HPX14 hBrd)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   SIGASSERT_NULL_OR_POINTER(statep->m_utility_bufp, px14_sample_t);
   if (statep->m_utility_bufp)
   {
      FreeDmaBufferPX14(hBrd, statep->m_utility_bufp);
      statep->m_utility_buf_samples = 0;
      statep->m_utility_bufp = NULL;
   }

   return SIG_SUCCESS;
}

/** @brief Get the library managed utility DMA buffer
  @todo Document GetUtilityDmaBufferPX14
  */
PX14API GetUtilityDmaBufferPX14 (HPX14 hBrd,
                                 px14_sample_t** bufpp,
                                 unsigned int* buf_samplesp)
{
   CStatePX14* statep;
   int res;

   SIGASSERT_NULL_OR_POINTER(bufpp, px14_sample_t*);
   SIGASSERT_NULL_OR_POINTER(buf_samplesp, unsigned int);
   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (bufpp)
      *bufpp = statep->m_utility_bufp;
   if (buf_samplesp)
      *buf_samplesp = statep->m_utility_buf_samples;

   return SIG_SUCCESS;
}

/** @brief
  Ensures that the library managed utility DMA buffer #2 is of the
  given size

  This function is used to ensure that the library managed utility DMA
  buffer #2 is at least as large as the given sample count. In the event
  that the current buffer is too small, it will be freed and a new
  buffer will be allocated. Previous data is not guaranteed. The address
  of this DMA buffer is obtained by calling the GetUtilityDmaBuffer2PX14
  function.

  This buffer may be freed by calling the FreeUtilityDmaBuffer2PX14
  function. This buffer is also automatically freed when the associated
  PX14400 handle is closed.

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14
  @param sample_count
  The minimum sample count that utility DMA buffer #2 should be

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.

  @see FreeUtilityDmaBuffer2PX14, GetUtilityDmaBuffer2PX14

*/
PX14API EnsureUtilityDmaBuffer2PX14 (HPX14 hBrd, unsigned int sample_count)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   // Do we already have a large enough buffer?
   if (statep->m_utility_buf2p &&
       statep->m_utility_buf2_samples >= sample_count)
   {
      return SIG_SUCCESS;
   }

   // Need to allocate a larger buffer so free what we've got
   FreeUtilityDmaBuffer2PX14(hBrd);

   res = AllocateDmaBufferPX14(hBrd, sample_count, &statep->m_utility_buf2p);
   PX14_RETURN_ON_FAIL(res);

   statep->m_utility_buf2_samples = sample_count;
   return SIG_SUCCESS;
}

/** @brief Frees utility DMA buffer #2

  @param hBrd
  A handle to a PX14400 device obtained by calling ConnectToDevicePX14

  @return
  Returns SIG_SUCCESS on success or one of the SIG_* error values on
  error.

  @see EnsureUtilityDmaBufferPX14, GetUtilityDmaBufferPX14
  */
PX14API FreeUtilityDmaBuffer2PX14 (HPX14 hBrd)
{
   CStatePX14* statep;
   int res;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   SIGASSERT_NULL_OR_POINTER(statep->m_utility_buf2p, px14_sample_t);
   if (statep->m_utility_buf2p)
   {
      FreeDmaBufferPX14(hBrd, statep->m_utility_buf2p);
      statep->m_utility_buf2_samples = 0;
      statep->m_utility_buf2p = NULL;
   }

   return SIG_SUCCESS;
}

/** @brief Get the utility DMA buffer #2
  @todo Document GetUtilityDmaBufferPX14
  */
PX14API GetUtilityDmaBuffer2PX14 (HPX14 hBrd,
                                  px14_sample_t** bufpp,
                                  unsigned int* buf_samplesp)
{
   CStatePX14* statep;
   int res;

   SIGASSERT_NULL_OR_POINTER(bufpp, px14_sample_t*);
   SIGASSERT_NULL_OR_POINTER(buf_samplesp, unsigned int);

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (bufpp)
      *bufpp = statep->m_utility_buf2p;
   if (buf_samplesp)
      *buf_samplesp = statep->m_utility_buf2_samples;

   return SIG_SUCCESS;
}

/** @brief Allocate an aggregated chain of DMA buffers of a total size

  Allocate a DMA buffer chain. A DMA buffer chain is an array of DMA
  buffer descriptors containing the address and size of a DMA buffer. A
  DMA buffer chain can be used by a recording application to further
  buffer acquisition data above buffering done by the PX14400 hardware.

  @param hBrd
  A handle to the PX14400 board. This handle is obtained by calling
  the ConnectToDevicePX14 function.
  @param total_samples
  The total number of samples to allocate over all buffers in the
  buffer chain.
  @param nodepp
  A pointer to a PX14S_BUFNODE* variable that will receive the address
  of the DMA buffer chain. See remarks.
  @param flags
  A set of flags that control function behavior. Currently defined
  flags are:
  - PX14DMACHAINF_LESS_IS_OKAY (0x00000001) : If the function fails
  to allocate the requested number of samples, it will return a
  DMA chain containing the buffers it was able to allocate. If
  this flag is set, it's up to the caller to check the buffer
  chain to see how large a chain was allocated.
  - PX14DMACHAINF_UTILITY_CHAIN (0x00010000) : The function will
  allocate a utility DMA buffer chain. Each PX14400 device handle
  can have one utility DMA buffer chain associated with it.
  Multiple DMA buffer chains may be allocated, but only one can be
  designated as the handle's utility DMA buffer chain. The utility
  DMA buffer chain can be used by certain library operations such
  recording sessions. Unlike normal DMA buffer chains, the utility
  DMA buffer chain is automatically freed by the library when the
  device handle is closed. If a utility DMA buffer chain has
  already been allocated and is not less than the requested size
  then no allocation will take place and the previously allocated
  DMA buffer chain will be returned. If a utility DMA buffer chain
  has already been allocated and is less than the requested size
  then it will be freed and a new buffer chain will be allocated.
  In this case, previous data is not copied.
  @param buf_countp
  A pointer to an int variable that will receive the total number of
  buffers allocated for the DMA buffer chain. This count can also be
  used to determine the size of the DMA buffer chain array. This
  parameter may be NULL. See remarks.

  @retval
  Returns SIG_SUCCESS on success of one of the SIG_* error values
  (which are all negative) on error.

  @remarks

  This function is used to allocate a DMA buffer chain. A DMA buffer chain
  is an array of DMA buffer address/length pairs that can be used by
  PX14400 software when additional buffering of acquisition data is
  required, such as high data throughput recordings.

  When a DMA buffer chain is allocated, the library allocates an extra
  buffer descriptor at the end to indicate end-of-array. This descriptor
  is identified by the bufp value being NULL. For this special case node,
  the buf_samples value is the sum total sample count of all buffers in
  the DMA buffer chain. This end-of-array marker allows software to pass
  around a DMA buffer chain object reference without having to include
  an array length parameter.

  The library uses the following allocation scheme:
  1. Initial DMA buffer allocation size is 2MiS (2 * 1024 * 1024 samples)
  2. The library iteratively allocates DMA buffers until the requested
  amount of memory has been allocated.
  3. If a DMA buffer allocation fails, the DMA buffer allocation size is
  halved and allocation continues.
  4. The minimum single DMA buffer allocation size is 512KiS (512 * 1024
samples).
5. The last buffer in the DMA buffer chain may be smaller than the
current DMA buffer allocation size so that the function does not
allocate more than what was requested.

Very few, if any, PX14400 library transfer functions work directly with
a DMA buffer chain. If using a DMA buffer chain in a custom PX14400
application, the application code will be responsible for walking the
DMA buffer chain and transferring to the component buffer described by
each node.

@code

// For a complete example see the PciAcqBufChainPX14 example application
// that is installed in the Examples directory of the main PX14400
// installation directory.

// Pseudo-example:
PX14S_BUFNODE *buf_chainp, *node_curp;
int res;

// Allocate DMA buffer chain
res = AllocateDmaBufferChainPX14(hBrd, 512 * 1048576, &buf_chainp, 0, NULL);
if (SIG_SUCCESS != res) { ERROR; }

// ...

// Use buffer chain
for (node_curp=buf_chainp; node_curp->bufp; node_curp++)
{
   res = GetPciAcquisitionDataFastPX14(hBrd,
                                       node_curp->buf_samples, node_curp->bufp, PX14_FALSE);
   if (SIG_SUCCESS != res) { ERROR; }
}

// ...

// Free DMA buffer chain when done
FreeDmaBufferChainPX14(hBrd, buf_chainp);

@sa FreeDmaBufferChainPX14
   */
PX14API AllocateDmaBufferChainPX14 (HPX14 hBrd,
                                    unsigned int total_samples,
                                    PX14S_BUFNODE** nodepp,
                                    int flags,
                                    int* buf_countp)
{
   static const unsigned int s_max_buf_samples = (4 * _1mebi) / sizeof(px14_sample_t);
   static const unsigned int s_min_buf_samples = (1 * _1mebi) / sizeof(px14_sample_t);

   unsigned int allocated_samples, buf_idx;
   PX14S_BUFNODE buf_desc;
   PX14S_BUFNODE* nodep;
   int res;

   SIGASSERT_NULL_OR_POINTER(nodepp, PX14S_BUFNODE*);
   SIGASSERT_NULL_OR_POINTER(buf_countp, int);

   if (total_samples < s_min_buf_samples)
      return SIG_PX14_INVALID_ARG_3;

   if (flags & PX14DMACHAINF_UTILITY_CHAIN)
   {
      res = GetUtilityDmaBufferChainPX14(hBrd, &nodep, &allocated_samples);
      PX14_RETURN_ON_FAIL(res);

      if (allocated_samples >= total_samples)
      {
         // Current utility buffer chain is large enough
         if (nodepp) *nodepp = nodep;
         if (buf_countp) *buf_countp = PX14_BUFCHAIN_BUF_COUNT(nodep);

         return SIG_SUCCESS;
      }

      if (allocated_samples)
      {
         // Current utility buffer chain is too small
         FreeDmaBufferChainPX14(hBrd, nodep);
      }
   }
   else
   {
      PX14_ENSURE_POINTER(hBrd, nodepp, PX14S_BUFNODE*, "AllocateDmaBufferChainPX14");
   }

   BufList_t::iterator iBuf;
   BufList_t bufList;

   buf_desc.buf_samples = s_max_buf_samples;

   res = SIG_INVALIDARG;		// error if !total_samples
   while (total_samples)
   {
      if (buf_desc.buf_samples > total_samples)
         buf_desc.buf_samples = total_samples;

      res = AllocateDmaBufferPX14(hBrd, buf_desc.buf_samples, &buf_desc.bufp);
      if (SIG_SUCCESS != res)
      {
         // Buffer allocation error?
         if (SIG_DMABUFALLOCFAIL == res)
         {
            // Retry with a smaller buffer
            if (buf_desc.buf_samples < total_samples)
            {
               buf_desc.buf_samples >>= 1;
               if (buf_desc.buf_samples >= s_min_buf_samples)
                  continue;
            }
         }

         break;
      }

      bufList.push_back(buf_desc);
      buf_desc.bufp = NULL;

      total_samples -= buf_desc.buf_samples;
   }

   if ((res != SIG_SUCCESS) && (0 == (flags & PX14DMACHAINF_LESS_IS_OKAY)))
   {
      // Free anything we've allocated
      for (iBuf=bufList.begin(); iBuf!=bufList.end(); iBuf++)
         FreeDmaBufferPX14(hBrd, iBuf->bufp);
      return res;
   }

   // Allocate buffer for descriptor list - extra nodes at extrema
   nodep = reinterpret_cast<PX14S_BUFNODE*>(
                                            my_malloc((bufList.size() + 2) * sizeof(PX14S_BUFNODE)));
   if (NULL == nodep)
   {
      for (iBuf=bufList.begin(); iBuf!=bufList.end(); iBuf++)
         FreeDmaBufferPX14(hBrd, iBuf->bufp);
      return SIG_OUTOFMEMORY;
   }

   // First node is hidden and is used to identify a valid DMA buffer
   //  chain. node->bufp is buffer count (not including first and last
   //  pseudo nodes) and node->buf_samples is a magic number. We hide the
   //  node by making our second node as the user's first node. We can then
   //  just look back when validating.
   nodep[0].buf_samples = PX14_DMA_CHAIN_MAGIC;
   nodep[0].bufp = reinterpret_cast<px14_sample_t*>(
                                                    static_cast<intptr_t>(bufList.size()));

   allocated_samples = 0;
   buf_idx = 1;
   for (iBuf=bufList.begin(); iBuf!=bufList.end(); iBuf++,buf_idx++)
   {
      nodep[buf_idx].buf_samples = iBuf->buf_samples;
      nodep[buf_idx].bufp        = iBuf->bufp;
      allocated_samples += iBuf->buf_samples;
   }
   // Last node - identified by NULL buffer address. This is like end()
   //  Buffer samples is total allocated samples
   nodep[buf_idx].buf_samples = allocated_samples;
   nodep[buf_idx].bufp        = NULL;

   if (buf_countp)
      *buf_countp = static_cast<unsigned int>(bufList.size());
   if (nodepp)
      *nodepp = &nodep[1];

   if (flags & PX14DMACHAINF_UTILITY_CHAIN)
      PX14_H2B(hBrd)->m_utility_buf_chainp = &nodep[1];

   return SIG_SUCCESS;
}

// Free a DMA buffer chain allocated with AllocateDmaBufferChainPX14
PX14API FreeDmaBufferChainPX14 (HPX14 hBrd, PX14S_BUFNODE* nodep)
{
   PX14S_BUFNODE* curp;
   CStatePX14* statep;
   int res;

   PX14_ENSURE_POINTER(hBrd, nodep, PX14S_BUFNODE, "FreeDmaBufferChainPX14");

   // Check that this is a valid DMA buffer chain
   if (nodep[-1].buf_samples != PX14_DMA_CHAIN_MAGIC)
      return SIG_PX14_INVALID_ARG_2;

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   // Invalidate our reference if this is the utility DMA buffer chain
   if (nodep == statep->m_utility_buf_chainp)
      statep->m_utility_buf_chainp = NULL;

   // Free individual buffers
   for (curp=nodep; curp->bufp; curp++)
      FreeDmaBufferPX14(hBrd, curp->bufp);

   // Free node list buffer
   my_free(&nodep[-1]);

   return SIG_SUCCESS;
}

PX14API GetUtilityDmaBufferChainPX14 (HPX14 hBrd,
                                      PX14S_BUFNODE** nodepp,
                                      unsigned int* total_samplesp)
{
   CStatePX14* statep;
   int res;

   SIGASSERT_NULL_OR_POINTER(nodepp, PX14S_BUFNODE*);
   SIGASSERT_NULL_OR_POINTER(total_samplesp, unsigned int);

   res = ValidateHandle(hBrd, &statep);
   PX14_RETURN_ON_FAIL(res);

   if (nodepp)
      *nodepp = statep->m_utility_buf_chainp;
   if (total_samplesp)
   {
      if (statep->m_utility_buf_chainp)
      {
         *total_samplesp =
            PX14_BUFCHAIN_TOTAL_SAMPLES(statep->m_utility_buf_chainp);
      }
      else
         *total_samplesp = 0;
   }

   return SIG_SUCCESS;
}


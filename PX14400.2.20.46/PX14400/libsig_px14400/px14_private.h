/** @file   px14_private.h
    @brief  Private PX14 library definitions

    This file includes definitions used by both the user-space PX14
    library and kernel-space driver.

    There are some items in here that are duplicated from px14.h. Having the
    driver directly include px14.h would result in some preprocessor
    ugliness so in the interest of keeping the public header as pretty as
    possible (and singular) we just duplicate what we need here. The
    compiler will bark if there's ever a discrepency between the values.

    Kernel-mode code should define PX14PP_NO_CLASS_DEFS to mask out some C++
    classes used by user-space library.
*/
/*
    __DEVICE__ = Device registers
    __CLOCK__  = Clock part registers
    __DRIVER__ = Driver (software-only) registers
    __EEPROM__ = Configuration EEPROM image
*/
#ifndef __px14_private_header_defined
#define __px14_private_header_defined

/// Placeholder for unknown values
#define MD_TODO                             0
/// Temporary placeholder for development
#define SIG_SUCCESS_NOT_IMPLEMENTED         0

// 'PX14' in little-endian memory dump
#define PX14_CTX_MAGIC                      0x34315850

// -- PX14 Handle State (CStatePX14) flags (PX14LIBF_*)
/// Device is virtual (fake; not connected to any real hardware)
#define PX14LIBF_VIRTUAL                    0x00000001
/// PLL parameters have changed and a PLL lock check is required
#define PX14LIBF_NEED_PLL_LOCK_CHECK        0x00000002
/// Ignore frequency changes for now
#define PX14LIBF_IGNORE_ACQ_FREQ_CHANGES    0x00000004
/// EEPROM is unprotected; no restrictions on read/writes
#define PX14LIBF_NO_PROTECT_EEPROM          0x00000008
/// PX14 device is remote and we're connected
#define PX14LIBF_REMOTE_CONNECTED           0x00000010
/// We're establishing a connection to remote PX14
#define PX14LIBF_REMOTE_CONNECTING          0x00000020
/// User manually disabling DCM
#define PX14LIBF_MANUAL_DCM_DISABLE         0x00000100
#define PX14LIBF__INIT                      0
/// Flags copied on remote connection
#define PX14LIBF__REMOTE_COPY               (PX14LIBF_MANUAL_DCM_DISABLE)
/// Flags not copied on handle duplication
#define PX14LIBF__NEVER_COPY               (PX14LIBF_NEED_PLL_LOCK_CHECK)

/// Minimum PCI firmware version required for 64-bit DMA operations
#define PX14FWV_PCI_MIN_64BIT_DMA_SUPPORT   PX14_VER32(1,1,2,0)
/// Minimum PCI firmware version required for ind. DMA/SampComp ISR clear
#define PX14FWV_PCI_MIN_SPLIT_INT_CLEAR     PX14_VER32(1,1,2,0)

/// PX14400 PCI vendor ID: Signatec, Inc.
#define PX14_VENDOR_ID                      0x1B94
/// PX14400 PCI device ID: PX14400
#define PX14_DEVICE_ID                      0xE400
/// PX14400 PCI device ID: PX12500
#define PX14_DEVICE_ID_PX12500              0xE412

/// Number of port regions required by the PX14400
#define PX14_PORTS_CNT                      0
/// Number of memory regions required by the PX14400
#define PX14_MEMS_CNT                       3

/// BAR index of PX14 DMA registers [BAR1]
#define PX14_BARIDX_DMA                     0
/// BAR index of PX14 device registers [BAR2]
#define PX14_BARIDX_DEVICE                  1
/// BAR index of PX14 configuration registers (JTAG/EEPROM) [BAR3]
#define PX14_BARIDX_CONFIG                  2

// -- PX14 device state (PX14STATE_*)
/// No pending operations in progress
#define PX14STATE_IDLE                      0
/// Fast DMA transfer in progress
#define PX14STATE_DMA_XFER_FAST             1
/// Buffered DMA transfer in progress
#define PX14STATE_DMA_XFER_BUFFERED         2
// RAM/SAB acquisition (or SAB transfer) in progress
#define PX14STATE_ACQ                       3
/// RAM write operation in progress
#define PX14STATE_WRAM                      4
#define PX14STATE__COUNT                    5

// -- PX14 device register sets
/// Device registers
#define PX14REGSET_DEVICE                   0
/// Clock generator registers
#define PX14REGSET_CLKGEN                   1
/// Driver (software only) registers
#define PX14REGSET_DRIVER                   2
#define PX14REGSET__COUNT                   3

/// PX14 sample data type
#define PX14_SAMPLE_TYPE                    unsigned short
/// Signed-equivalent of a PX14 sample
#define PX14_SAMPLE_TYPE_SIGNED             signed short

/// Minimum swing of PX14400 VCO (MHz)
#define PX14_VCO_SWING_MIN_MHZ              1850
/// Maximum swing of PX14400 VCO (MHz)
#define PX14_VCO_SWING_MAX_MHZ              2220

/// Assumed maximum local PX14400 device count (mutable)
#define PX14_MAX_LOCAL_DEVICE_COUNT         16

/// Maximum device read length (Windows limit is just under 64MiB)
#define PX14_MAX_DEVICE_READ_BYTES          (60 * 1048576)
/// Maximum time the driver will ever delay in microseconds
#define PX14_MAX_DRIVER_DELAY               (1 * 1000 * 1000)   // 1 second
/// Default PLL lock timeout in milliseconds
#define PX14_DEF_PLL_LOCK_WAIT_MS           7000

/// Minimum (non-zero) trigger delay samples count
#define PX14_MIN_TRIG_DELAY_SAMPLES         4
/// Maximum delay trigger samples count for older firmware
#define PX14_MAX_TRIG_DELAY_SAMPLES_OLD     262140
/// Maximum delay trigger samples count
#define PX14_MAX_TRIG_DELAY_SAMPLES         2097148
/// Minimum delay trigger samples alignment
#define PX14_ALIGN_TRIG_DELAY_SAMPLES       4

/// Minimum (non-zero) pre-trigger samples count
#define PX14_MIN_PRE_TRIGGER_SAMPLES        0 //before it was 16 -Fg
/// Maximum pre-trigger samples count on older (<1.3.3) system firmware
#define PX14_MAX_PRE_TRIGGER_SAMPLES        16384-24
/// Maximum pre-trigger samples count for PX12500 devices
#define PX14_MAX_PRE_TRIGGER_SAMPLES_PX12500 2048
/// Minimum pre-trigger samples count alignment
#define PX14_ALIGN_PRE_TRIGGER_SAMPLES      4

/// Minimum (non-zero) segment size
#define PX14_MIN_SEGMENT_SAMPLES            4
/// Maximum segment size
#define PX14_MAX_SEGMENT_SAMPLES            0x7FFFFFFC
/// Required segment size alignment
#define PX14_ALIGN_SEGMENT_SAMPLES          4

/// Maximum starting sample value
#define PX14_MAX_START_SAMPLE               268435448
/// Required alignment for PX14400 starting sample
#define PX14_ALIGN_START_SAMPLE             4096

/// Maximum concrete sample count
#define PX14_MAX_SAMPLE_COUNT               2147483616
/// Required alignment for PX14400 sample count
#define PX14_ALIGN_SAMPLE_COUNT             16
/// Required alignment for PX14 sample count
#define PX14_ALIGN_SAMPLE_COUNT_BURST       4096

/// Minimum DMA transfer alignment; aligning to a SDRAM burst
#define PX14_ALIGN_DMA_XFER_FAST            PX14_ALIGN_SAMPLE_COUNT_BURST

/// GetXXXPX14: Grab value from local register cache; don't go to driver
#define PX14_GET_FROM_CACHE                 1   // == TRUE
/// GetXXXPX14: Grab value from driver cache/hardware
#define PX14_GET_FROM_DRIVER                0   // == FALSE

// -- JTAG IO flags (PX14JIOF_*)
/// Pulse clock after write before read
#define PX14JIOF_PULSE_TCK                  0x00000001
/// Read after write
#define PX14JIOF_POST_READ                  0x00000002
/// Loop on dwValW clock pulses (No TDI)
#define PX14JIOF_PULSE_TCK_LOOP             0x00000004
/// Request start JTAG session
#define PX14JIOF_START_SESSION              0x80000000
/// Request end JTAG session
#define PX14JIOF_END_SESSION                0x40000000
#define PX14JIOF__DEF                       0
#define PX14JIOF__RESULTANT_DATA            \
    (PX14JIOF_POST_READ | PX14JIOF_START_SESSION | PX14JIOF_END_SESSION)

// -- JTAG stream shift flags (PX14JSS_*)
#define PX14JSS_WRITE_TDI                   0x00000001
#define PX14JSS_READ_TDO                    0x00000002
#define PX14JSS_WRITE_READ                  0x00000003
/// Read/write operation mask
#define PX14JSS__OP_MASK                    0x00000003
#define PX14JSS_EXIT_SHIFT                  0x00000004

// -- XSVF File Playing flags
#define PX14XSVFF_WAIT_IS_TICKS             0x00000001
#define PX14XSVFF_LOW_CLOCK_ON_WAIT         0x00000002
#define PX14XSVFF_VIRTUAL                   0x80000000
#define PX14XSVFF__DEFAULT                  (PX14XSVFF_LOW_CLOCK_ON_WAIT)

// -- PX14400 PCIe FPGA types
/// Virtex 5 LX50t
#define PX14SYSFPGA_V5_LX50T                0
#define PX14SYSFPGA__COUNT                  1

// -- PX14400 SAB FPGA types
/// Virtex 5 SX50T
#define PX14SABFPGA_V5_SX50T                0
/// Virtex 5 SX95T
#define PX14SABFPGA_V5_SX95T                1
#define PX14SABFPGA__COUNT                  2

#define PX14_REV1_NAMEA                      "PX14400A"
#define PX14_REV1_NAMEW                     L"PX14400A"
#define PX14_REV2_NAMEA                      "PX12500A"
#define PX14_REV2_NAMEW                     L"PX12500A"
#define PX14_REV3_NAMEA                      "PX14400D"
#define PX14_REV3_NAMEW                     L"PX14400D"
#define PX14_REV4_NAMEA                      "PX14400D2"
#define PX14_REV4_NAMEW                     L"PX14400D2"
#ifdef _UNICODE
# define PX14_REV1_NAME                     PX14_REV1_NAMEW
# define PX14_REV2_NAME                     PX14_REV2_NAMEW
# define PX14_REV3_NAME                     PX14_REV3_NAMEW
# define PX14_REV4_NAME                     PX14_REV4_NAMEW
#else
# define PX14_REV1_NAME                     PX14_REV1_NAMEA
# define PX14_REV2_NAME                     PX14_REV2_NAMEA
# define PX14_REV3_NAME                     PX14_REV3_NAMEA
# define PX14_REV4_NAME                     PX14_REV4_NAMEA
#endif

// - Size of some public structures; used to maintain forward compatibility
/// sizeof(PX14S_RECORDED_DATA_INFO)
#define _PX14SO_RECORDED_DATA_INFO_V1       80 /* no clue what v1 format is!! unneccessary confusion *fuming* */
#ifdef PX14_LIB_64BIT_IMP
/// sizeof(PX14S_REC_SESSION_PARAMS)
#  define _PX14SO_REC_SESSION_PARAMS_V1     72
/// sizeof(PX14S_FILE_WRITE_PARAMS)
#  define _PX14SO_FILE_WRITE_PARAMS_V1      80
/// sizeof(PX14S_REC_SESSION_PROG)
#  define _PX14SO_REC_SESSION_PROG_V1       56
/// sizeof(PX14S_RECORDED_DATA_INFO) (version 2)
#  define _PX14SO_RECORDED_DATA_INFO_V2     88
#else
/// sizeof(PX14S_REC_SESSION_PARAMS)
#  define _PX14SO_REC_SESSION_PARAMS_V1     56
/// sizeof(PX14S_FILE_WRITE_PARAMS)
#  define _PX14SO_FILE_WRITE_PARAMS_V1      48
/// sizeof(PX14S_REC_SESSION_PROG)
#  define _PX14SO_REC_SESSION_PROG_V1       48
/// sizeof(PX14S_RECORDED_DATA_INFO) (version 2)
#  define _PX14SO_RECORDED_DATA_INFO_V2     84
#endif
/// sizeof(PX14S_FW_VER_INFO)
#define _PX14SO_FW_VER_INFO_V1              20

//########################################################################//
//
// Items duplicated from px14.h
//
//########################################################################//

// -- Library error codes; px14.h has comments

#define SIG_SUCCESS                         0
#define SIG_ERROR                           -1
#define SIG_INVALIDARG                      -2
#define SIG_OUTOFBOUNDS                     -3
#define SIG_NODEV                           -4
#define SIG_OUTOFMEMORY                     -5
#define SIG_DMABUFALLOCFAIL                 -6
#define SIG_NOSUCHBOARD                     -7
#define SIG_NT_ONLY                         -8
#define SIG_INVALID_MODE                    -9
#define SIG_CANCELLED                       -10
#define SIG_PENDING                         1
#define SIG_PX14__FIRST                     -512
#define SIG_PX14_NOT_IMPLEMENTED            -512
#define SIG_PX14_INVALID_HANDLE             -513
#define SIG_PX14_BUFFER_TOO_SMALL           -514
#define SIG_PX14_INVALID_ARG_1              -515
#define SIG_PX14_INVALID_ARG_2              -516
#define SIG_PX14_INVALID_ARG_3              -517
#define SIG_PX14_INVALID_ARG_4              -518
#define SIG_PX14_INVALID_ARG_5              -519
#define SIG_PX14_INVALID_ARG_6              -520
#define SIG_PX14_INVALID_ARG_7              -521
#define SIG_PX14_INVALID_ARG_8              -522
#define SIG_PX14_XFER_SIZE_TOO_SMALL        -523
#define SIG_PX14_XFER_SIZE_TOO_LARGE        -524
#define SIG_PX14_INVALID_DMA_ADDR           -525
#define SIG_PX14_WOULD_OVERRUN_BUFFER       -526
#define SIG_PX14_BUSY                       -527
#define SIG_PX14_INVALID_CHAN_IMP           -528
#define SIG_PX14_XML_MALFORMED              -529
#define SIG_PX14_XML_INVALID                -530
#define SIG_PX14_XML_GENERIC                -531
#define SIG_PX14_RATE_TOO_FAST              -532
#define SIG_PX14_RATE_TOO_SLOW              -533
#define SIG_PX14_RATE_NOT_AVAILABLE         -534
#define SIG_PX14_UNEXPECTED                 -535
#define SIG_PX14_SOCKET_ERROR               -536
#define SIG_PX14_NETWORK_NOT_READY          -537
#define SIG_PX14_SOCKETS_TOO_MANY_TASKS     -538
#define SIG_PX14_SOCKETS_INIT_ERROR         -539
#define SIG_PX14_NOT_REMOTE                 -540
#define SIG_PX14_TIMED_OUT                  -541
#define SIG_PX14_CONNECTION_REFUSED         -542
#define SIG_PX14_INVALID_CLIENT_REQUEST     -543
#define SIG_PX14_INVALID_SERVER_RESPONSE    -544
#define SIG_PX14_REMOTE_CALL_RETURNED_ERROR -545
#define SIG_PX14_UNKNOWN_REMOTE_METHOD      -546
#define SIG_PX14_SERVER_DISCONNECTED        -547
#define SIG_PX14_REMOTE_CALL_NOT_AVAILABLE  -548
#define SIG_PX14_UNKNOWN_FW_FILE            -549
#define SIG_PX14_FIRMWARE_UPLOAD_FAILED     -550
#define SIG_PX14_INVALID_FW_FILE            -551
#define SIG_PX14_DEST_FILE_OPEN_FAILED      -552
#define SIG_PX14_SOURCE_FILE_OPEN_FAILED    -553
#define SIG_PX14_FILE_IO_ERROR              -554
#define SIG_PX14_INCOMPATIBLE_FIRMWARE      -555
#define SIG_PX14_UNKNOWN_STRUCT_SIZE        -556
#define SIG_PX14_INVALID_REGISTER           -557
#define SIG_PX14_FIFO_OVERFLOW              -558
#define SIG_PX14_DCM_SYNC_FAILED            -559
#define SIG_PX14_DISK_FULL                  -560
#define SIG_PX14_INVALID_OBJECT_HANDLE      -561
#define SIG_PX14_THREAD_CREATE_FAILURE      -562
#define SIG_PX14_PLL_LOCK_FAILED            -563
#define SIG_PX14_THREAD_NOT_RESPONDING      -564
#define SIG_PX14_REC_SESSION_ERROR          -565
#define SIG_PX14_REC_SESSION_CANNOT_ARM     -566
#define SIG_PX14_SNAPSHOTS_NOT_ENABLED      -567
#define SIG_PX14_SNAPSHOT_NOT_AVAILABLE     -568
#define SIG_PX14_SRDC_FILE_ERROR            -569
#define SIG_PX14_NAMED_ITEM_NOT_FOUND       -570
#define SIG_PX14_CANNOT_FIND_SRDC_DATA      -571
#define SIG_PX14_NOT_IMPLEMENTED_IN_FIRMWARE -572
#define SIG_PX14_TIMESTAMP_FIFO_OVERFLOW    -573
#define SIG_PX14_CANNOT_DETERMINE_FW_REQ    -574
#define SIG_PX14_REQUIRED_FW_NOT_FOUND      -575
#define SIG_PX14_FIRMWARE_IS_UP_TO_DATE     -576
#define SIG_PX14_NO_VIRTUAL_IMPLEMENTATION  -577
#define SIG_PX14_DEVICE_REMOVED             -578
#define SIG_PX14_JTAG_IO_ERROR              -579
#define SIG_PX14_ACCESS_DENIED              -580
#define SIG_PX14_PLL_LOCK_FAILED_UNSTABLE   -581
#define SIG_PX14_UNREACHABLE                -582
#define SIG_PX14_INVALID_MODE_CHANGE        -583
#define SIG_PX14_DEVICE_NOT_READY           -584
#define SIG_PX14_ALIGNMENT_ERROR            -585
#define SIG_PX14_INVALID_OP_FOR_BRD_CONFIG  -586
#define SIG_PX14_UNKNOWN_JTAG_CHAIN         -587
#define SIG_PX14_SLAVE_FAILED_MASTER_SYNC   -588
#define SIG_PX14_NOT_IMPLEMENTED_IN_DRIVER  -589
#define SIG_PX14_TEXT_CONV_ERROR            -590
#define SIG_PX14_INVALIDARG_NULL_POINTER    -591
#define SIG_PX14_INCORRECT_LOGIC_PACKAGE    -592
#define SIG_PX14_CFG_EEPROM_VALIDATE_ERROR  -593
#define SIG_PX14_RESOURCE_ALLOC_FAILURE     -594
#define SIG_PX14_OPERATION_FAILED           -595
#define SIG_PX14_BUFFER_CHECKED_OUT         -596
#define SIG_PX14_BUFFER_NOT_ALLOCATED       -597
#define SIG_PX14_QUASI_SUCCESSFUL            512

/// Size of a PX14 data sample in bytes
#define PX14_SAMPLE_SIZE_IN_BYTES           2
/// Size of a PX14 data sample in bits
#define PX14_SAMPLE_SIZE_IN_BITS            16
/// Maximum PX14 sample value
#define PX14_SAMPLE_MAX_VALUE               0xFFFC
/// Midscale PX14400 sample value
#define PX14_SAMPLE_MID_VALUE               0x8000
/// Minimum PX14 sample value
#define PX14_SAMPLE_MIN_VALUE               0x0000

/// PX14 RAM size in bytes
#define PX14_RAM_SIZE_IN_BYTES              (512 * 1024 * 1024)
/// PX14 RAM size in samples; portable code should use GetSampleRamSizePX14
#define PX14_RAM_SIZE_IN_SAMPLES                \
    (PX14_RAM_SIZE_IN_BYTES / PX14_SAMPLE_SIZE_IN_BYTES)

/// TLP size used for DMA transfers
#define PX14_DMA_TLP_BYTES                  128
/// PX14 DMA transfer frame size in bytes
#define PX14_DMA_XFER_FRAME_SIZE_IN_BYTES   PX14_DMA_TLP_BYTES
/// PX14 DMA transfer frame size in samples
#define PX14_DMA_XFER_FRAME_SIZE_IN_SAMPLES \
    (PX14_DMA_XFER_FRAME_SIZE_IN_BYTES / PX14_SAMPLE_SIZE_IN_BYTES)
/// Minimum PX14 DMA transfer size in bytes
#define PX14_MIN_DMA_XFER_SIZE_IN_BYTES     \
    PX14_DMA_XFER_FRAME_SIZE_IN_BYTES
/// Minimum PX14 DMA transfer size in samples (1024)
#define PX14_MIN_DMA_XFER_SIZE_IN_SAMPLES   \
    (PX14_MIN_DMA_XFER_SIZE_IN_BYTES / PX14_SAMPLE_SIZE_IN_BYTES)
/// Maximum PX14 DMA transfer size in bytes (8388480 bytes = just under 8MiB)
#define PX14_MAX_DMA_XFER_SIZE_IN_BYTES     (0xFFFF * PX14_DMA_TLP_BYTES)
/// Maximum PX14 DMA transfer size in samples (4194240 samples)
#define PX14_MAX_DMA_XFER_SIZE_IN_SAMPLES   \
    (PX14_MAX_DMA_XFER_SIZE_IN_BYTES / PX14_SAMPLE_SIZE_IN_BYTES)

/// Boolean true
#define PX14_TRUE                           1
/// Boolean false
#define PX14_FALSE                          0

// -- PX14 Operating Mode values (PX14MODE_*)
/// Power down mode
#define PX14MODE_OFF                        0
/// Standby (ready) mode
#define PX14MODE_STANDBY                    1
/// RAM acquisition mode
#define PX14MODE_ACQ_RAM                    2
/// PCI acquisition mode using only PCI FIFO
#define PX14MODE_ACQ_PCI_SMALL_FIFO         3
/// RAM-buffered PCI acquisition mode; data buffered through PX14 RAM
#define PX14MODE_ACQ_PCI_BUF                4
/// Signatec Auxiliary Bus (SAB) acquisition; acquire directly to SAB bus
#define PX14MODE_ACQ_SAB                    5
/// Buffered SAB acquisition; acquire to SAB, buffering through PX14 RAM
#define PX14MODE_ACQ_SAB_BUF                6
/// PX14 RAM read to PCI; transfer data from PX14 RAM to host PC
#define PX14MODE_RAM_READ_PCI               7
/// PX14 RAM read to SAB; transfer data from PX14 RAM to SAB bus
#define PX14MODE_RAM_READ_SAB               8
/// PX14 RAM write from PCI; transfer data from host PC to PX14 RAM
#define PX14MODE_RAM_WRITE_PCI              9
/// Cleanup a pending DMA by sending a force Trigger
#define PX14MODE_CLEANUP_PENDING_DMA        10

#define PX14MODE__COUNT                     11  // Invalid setting

// -- Device register read methods (PX14REGREAD_*)
/// Read from cache unless it's dirty (or status), then go to hardware
#define PX14REGREAD_DEFAULT                 0       // Should be default
/// Read from driver cache only; do *not* read from hardware
#define PX14REGREAD_CACHE_ONLY              1
/// Read from hardware regardless of cache state; updates cache
#define PX14REGREAD_HARDWARE                2
/// Read from user-mode, handle-specific cache; no kernel mode
#define PX14REGREAD_USER_CACHE              3
#define PX14REGREAD__COUNT                  4

// -- Device trigger selection

#define PX14TRIGSEL_A_AND_HYST            0
#define PX14TRIGSEL_A_AND_B               1
#define PX14TRIGSEL_RESERVED              2
#define PX14TRIGSEL_A_OR_B                3
#define PX14TRIGSEL__COUNT                4  // Invalid setting

// -- Trigger hysteresis default level

#define PX14TRIGHYST_DEFAULT_LVL         10

// -- PX14 Timestamp read flags (PX14TSREAD_*)
/// Call to read from a known-full timestamp FIFO
#define PX14TSREAD_READ_FROM_FULL_FIFO      0x00000001
/// Ignore FIFO flag state; always read requested number of timestamps
#define PX14TSREAD_IGNORE_FIFO_FLAGS      0x00000002
/// Mask for input flags
#define PX14TSREAD__INPUT_FLAG_MASK         0x000FFFFF
/// Mask for output flags
#define PX14TSREAD__OUTPUT_FLAG_MASK        0xFFF00000
/// Output flag; More timestamps are available
#define PX14TSREAD_MORE_AVAILABLE           0x01000000
/// Output flag; Timestamp FIFO overflowed
#define PX14TSREAD_FIFO_OVERFLOW            0x02000000

// -- PX14 active channel selection constants (PX14CHANNEL_*)
/// Dual channel mode (Power-up default)
#define PX14CHANNEL_DUAL                    0
/// Single channel mode - Channel 1
#define PX14CHANNEL_ONE                     1
/// Single channel mode - Channel 2
#define PX14CHANNEL_TWO                     2
#define PX14CHANNEL__COUNT                  3   // Invalid setting

// -- PX14400 channel implementation flags (PX14CHANIMPF_*)
/// Channel is not amplified; static input voltage range
#define PX14CHANIMPF_NON_AMPLIFIED          0x01
/// Channel is DC coupled
#define PX14CHANIMPF_DC_COUPLED             0x02

// -- PX14400 board revision identifiers
// (Note: This is not the same as a board's hardware revision value)
/// Original PX14400
#define PX14BRDREV_1                        0
#define PX14BRDREV_PX14400                  0
/// PX12500 - 12-bit 500MHz based on PX14400 hardware
#define PX14BRDREV_PX12500                  1
/// PX14400D - DC-coupled PX14400 device
#define PX14BRDREV_PX14400D                 2
/// PX14400D2 - DC-coupled PX14400 device with PDA14 front end
#define PX14BRDREV_PX14400D2                3
#define PX14BRDREV__COUNT                   4

// -- PX14400 board sub-revision values; each set relative to PX14BRDREV_*
/// PX14400 SP - Signal Processing; has SAB
#define PX14BRDREVSUB_1_SP                  0
/// PX14400 DR - Data recorder; no SAB
#define PX14BRDREVSUB_1_DR                  1
#define PX14BRDREVSUB_1__COUNT              2

// -- PX14400 features present (PX14FEATURE_*)
/// SAB functionality is present
#define PX14FEATURE_SAB                     0x00000001
/// Ethernet functionality is present
#define PX14FEATURE_ETHERNET                0x00000002
/// This is a prototype PX14400 device
#define PX14FEATURE_PROTOTYPE               0x00000004
/// Board may have FPGA processing enabled
#define PX14FEATURE_FPGA_PROC               0x00000008
/// No internal clock is available
#define PX14FEATURE_NO_INTERNAL_CLOCK       0x00000010
/// Board uses master/slave hardware configuration 2.0
#define PX14FEATURE_HWMSCFG_2               0x00000020

//########################################################################//
//
// Device configuration registers - Only the driver is aware of these
//  registers; user-space library is unaware of them.
//
// Location: BAR3
//
//########################################################################//

#define PX14_CONFIG_REG_COUNT               2

typedef struct _PX14S_REG_CFG_00_tag
{
    unsigned int _nu_0000FFFF   : 16;
    unsigned int SCLK           : 1;    // <-- EEPROM control
    unsigned int DOUT           : 1;
    unsigned int DIN            : 1;
    unsigned int _UNUSED_       : 1;
    unsigned int CHIPSEL        : 1;    // EEPROM control -->
    unsigned int _nu_FFE00000   : 11;

} PX14S_REG_CFG_00;

// PX14 PCI FPGA Control Register
typedef union _PX14U_REG_CFG_00_tag
{
    PX14S_REG_CFG_00        bits;
    unsigned int            val;

} PX14U_REG_CFG_00;

/// Default register value for PX14U_REG_CFG_00
#define PX14CFGREGINIT_00           0x00000000

typedef struct _PX14S_REG_CFG_01_tag
{
    unsigned int ctrl           : 1;
    unsigned int tdi            : 1;    // Write TDI
    unsigned int tms            : 1;
    unsigned int tck            : 1;
    unsigned int tdo            : 1;    // Read TDO
    unsigned int _nu_FFFFFFE0   : 27;

} PX14S_REG_CFG_01;

// -- PX14 JTAG register bit masks
#define PX14JIO_CTRL                        0x00000001
/// Write to TDI
#define PX14JIO_TDI                         0x00000002
#define PX14JIO_TMS                         0x00000004
#define PX14JIO_TCK                         0x00000008
/// Read from TDO
#define PX14JIO_TDO                         0x00000010

// PX14 JTAG register
typedef union _PX14U_REG_CFG_01_tag
{
    PX14S_REG_CFG_01    bits;
    unsigned            val;

} PX14U_REG_CFG_01;

/// Default register value for PX14U_REG_CFG_01
#define PX14CFGREGINIT_01                   0x00000000

typedef struct _PX14S_CONFIG_REGISTER_SET_tag
{
    PX14U_REG_CFG_00    reg0;
    PX14U_REG_CFG_01    reg1;

} PX14S_CONFIG_REGISTER_SET;

typedef union _PX14U_CONFIG_REGISTER_SET_tag
{
    unsigned int                values[PX14_CONFIG_REG_COUNT];
    PX14S_CONFIG_REGISTER_SET   fields;

} PX14U_CONFIG_REGISTER_SET;

//########################################################################//
//
// DMA registers - Only the driver is aware of these registers; user-space
//  library is unaware of them.
//
// Location: BAR1
//
//########################################################################//

// -- DMA register indices (PX14DMAREG_*)
/// Device Control Register
#define PX14DMAREG_DCR_OFFSET                   0
/// Device Control and Status Register
#define PX14DMAREG_DCSR_OFFSET                  1
/// Write DMA TLP Address Register
#define PX14DMAREG_WRITE_ADDR_OFFSET            2
/// Write DMA TLP Size Register
#define PX14DMAREG_WRITE_SIZE_OFFSET            3
/// Write DMA TLP Count Register
#define PX14DMAREG_WRITE_COUNT_OFFSET           4
/// Write DMA Data Pattern Register
#define PX14DMAREG_WRITE_PATTERN_OFFSET         5
/// Read  DMA Data Pattern Register
#define PX14DMAREG_READ_PATTERN_OFFSET          6
/// Read  DMA TLP Address Register
#define PX14DMAREG_READ_ADDR_OFFSET             7
/// Read  DMA TLP Size Register
#define PX14DMAREG_READ_SIZE_OFFSET             8
/// Read  DMA TLP Count Register
#define PX14DMAREG_READ_COUNT_OFFSET            9
/// Write DMA TLP Address (Upper 64-bits) Register
#define PX14DMAREG_WRITE_ADDRH_OFFSET           10
/// Read DMA TLP Address (Upper 64-bits) Register
#define PX14DMAREG_READ_ADDRH_OFFSET            11
/// Read  DMA Status Register
#define PX14DMAREG_READ_DMA_STATUS_OFFSET       12
/// Read  DMA Number of Completions Register
#define PX14DMAREG_READ_DMA_COMPLETE_OFFSET     13
/// Read  DMA Size of Completions Register
#define PX14DMAREG_READ_DMA_COMPLETE_SIZE       14
/// PCIe Device Link Width Register
#define PX14DMAREG_DEVICE_LINK_WIDTH_STATUS     15
/// PCIe Device Link Transaction Size Register
#define PX14DMAREG_DEVICE_LINK_TRANS_SIZE       16
// Samples Complete interrupt clear (PCI fw v >= 1.1.2)
#define PX14DMAREG_CLEAR_SAMP_COMP_INTERRUPT    17
// DMA Complete interrupt clear (also clears SampComp if PCI fw v < 1.1.2)
#define PX14DMAREG_CLEAR_INTERRUPT              18

// Bit to set in TLP size register to indicate 64-bit operation
#define PX14DMAMSK_64BIT_WRITE_TLP_ENABLE       0x00080000

//########################################################################//
//
// __DEVICE__
//
// PX14 device register mappings
//
// Location: BAR2
//
// These registers are the main registers that defined the state of the
// PX14. This includes things like all hardware settings, operating mode,
// status bits, and access to the clock generator device (which has it's
// own register set defined later).
//
// Rules:
//
// 1. All structures should be 32-bits in size
// 2. Each significant bit field should have a few attributes defined:
//    PX14REGIDX_*   Index of register that contains the field
//    PX14REGMSK_*   Mask in register that defines the field
//    PX14REGSHR_*   Bits to right shift in order to get field in LSB
// 3. Some fields may have these attributes:
//    PX14REGHWSH_*   Bits to right shift when writing to hardware
//
// Device register overview:
//  R0 : Segment size
//  R1 : Total sample count
//  R2 : Starting address
//  R3 : Delay/Pretrigger samples
//  R4 : Trigger levels
//  R5 : Trigger and clock
//  R6 : Board settings
//  R7 : Analog front end
//  R8 : Offset DAC IO (DC-coupled boards)
//  R9 : SAB1
//  RA : FPGA processing parameter IO
//  RB : Operating mode
//  RC : RAM address (Read-only) - ADC Clock Measure (Read-only)
//  RD : Status/Monitor (Read-only)
//  RE : Timestamp (upper) (Read-only)
//  RF : Timestamp (lower) (Read-only)
//  R10 : Trigger (read-write)
//  R11 : Clear status (Write-Only)
//########################################################################//

#define PX14_DEVICE_REG_COUNT               18

/// Register 0 : Segment Size
typedef struct _PX14S_REG_DEV_00_tag
{
    unsigned int SEGSIZE            : 32;       ///< Segment size

} PX14S_REG_DEV_00;

/// Default register value for register 0
#define PX14REGDEFAULT_0                    0
#define PX14REGIDX_SEG_SIZE                 0
#define PX14REGMSK_SEG_SIZE                 0xFFFFFFFF
#define PX14REGSHR_SEG_SIZE                 0
#define PX14REGHWSH_SEG_SIZE                2

/// Register 1 : Total Samples
typedef struct _PX14S_REG_DEV_01_tag
{
    // 1 burst = 4096 16-bit samples
    // Partial burst unit is 16 32-bit samples

    unsigned int BC                 : 19;   ///< Burst count
    unsigned int _nu_00080000       : 1;
    unsigned int PS                 : 7;    ///< Partial burst size
    unsigned int _nu_78000000       : 4;
    unsigned int FRUN               : 1;    ///< Free run

} PX14S_REG_DEV_01;

/// Default register value for register 1
#define PX14REGDEFAULT_1                    0
#define PX14REGIDX_BURST_COUNT              1
#define PX14REGMSK_BURST_COUNT              0x0007FFFF
#define PX14REGSHR_BURST_COUNT              0
#define PX14REGIDX_PARTIAL_BURST            1
#define PX14REGMSK_PARTIAL_BURST            0x07F00000
#define PX14REGSHR_PARTIAL_BURST            0
#define PX14REGIDX_FREE_RUN                 1
#define PX14REGMSK_FREE_RUN                 0x80000000
#define PX14REGSHR_FREE_RUN                 31

/// Register 2 : Starting Sample
typedef struct _PX14S_REG_DEV_02_tag
{
    unsigned int SA                 : 25;   ///< Starting sample
    unsigned int nu_FE000000        : 7;

} PX14S_REG_DEV_02;

/// Default register value for register 2
#define PX14REGDEFAULT_2                    0
#define PX14REGIDX_START_SAMPLE             2
#define PX14REGMSK_START_SAMPLE             0x01FFFFFF
#define PX14REGSHR_START_SAMPLE             0
#define PX14REGHWSH_START_SAMPLE            3

/// Register 3 : Delay/Pretrigger Samples
typedef struct _PX14S_REG_DEV_03_tag
{
   unsigned int TD                 : 16;  ///< 0000FFFF: Trigger delay
   unsigned int PS                 : 13;  ///< 1FFF0000: Pretrigger samples
   unsigned int TD_HI              : 3;   ///< E0000000: Trigger delay upper

} PX14S_REG_DEV_03;

/// Default register value for register 3
#define PX14REGDEFAULT_3                    0
#define PX14REGIDX_TRIG_DELAY               3
#define PX14REGMSK_TRIG_DELAY               0x0000FFFF
#define PX14REGSHR_TRIG_DELAY               0
#define PX14REGHWSH_TRIG_DELAY              2
#define PX14REGIDX_PRETRIG_SAMPLES          3
#define PX14REGMSK_PRETRIG_SAMPLES          0x1FFF0000
#define PX14REGSHR_PRETRIG_SAMPLES          16
#define PX14REGHWSH_PRETRIG_SAMPLES         2
#define PX14REGIDX_TRIG_DELAY_HI            3
#define PX14REGMSK_TRIG_DELAY_HI            0xE0000000
#define PX14REGSHR_TRIG_DELAY_HI            29

/// Register 4 : Trigger Levels
typedef struct _PX14S_REG_DEV_04_tag
{
    unsigned int TLA                : 16;   ///< Trigger level A
    unsigned int TLB                : 16;   ///< Trigger level B

} PX14S_REG_DEV_04;

/// Default register value for register 4
#define PX14REGDEFAULT_4                    0x80008000
#define PX14REGIDX_TRIGLEVEL_A              4
#define PX14REGMSK_TRIGLEVEL_A              0x0000FFFF
#define PX14REGSHR_TRIGLEVEL_A              0
#define PX14REGHWSH_TRIGLEVEL_A             0
#define PX14REGIDX_TRIGLEVEL_B              4
#define PX14REGMSK_TRIGLEVEL_B              0xFFFF0000
#define PX14REGSHR_TRIGLEVEL_B              16
#define PX14REGHWSH_TRIGLEVEL_B             0

/// Register 5 : Trigger/Clock Settings
typedef struct _PX14S_REG_DEV_05_tag
{
   unsigned int TM                 : 2;    ///< 00000003: Trigger mode
   unsigned int TS                 : 2;    ///< 0000000C: Trigger source
   unsigned int TDIRA              : 1;    ///< 00000010: Trigger direction A
   unsigned int TDIRB              : 1;    ///< 00000020: Trigger direction B
   unsigned int TDIR_E             : 1;    ///< 00000040: Trigger direction Ext
   unsigned int STR                : 1;    ///< 00000080: Software trigger
   unsigned int DTRIG              : 1;    ///< 00000100: Delay trigger enable
   unsigned int PTRIG              : 1;    ///< 00000200: Pre-trigger samples enable
   unsigned int CG_PD_             : 1;    ///< 00000400: CG power down pin
   unsigned int CG_RST_            : 1;    ///< 00000800: CG reset pin
   unsigned int CG_SYNC_           : 1;    ///< 00001000: CG SYNC pin
   unsigned int CG_REF_SEL         : 1;    ///< 00002000: CG REF_SEL pin
   unsigned int NO_DCM             : 1;    ///< 00004000: Don't use DCM (F < 64)
   unsigned int DCM_SEL            : 1;    ///< 00008000: DCM selection
   unsigned int CREF               : 1;    ///< 00010000: Clock reference
   unsigned int CLKSRC             : 2;    ///< 00060000: Clock source
   unsigned int CD                 : 5;    ///< 00F80000: Post ADC clock divider
   unsigned int DCMRES1            : 1;    ///< 01000000: DCM1 reset
   unsigned int DCMRES2            : 1;    ///< 02000000: DCM2 reset
   unsigned int DCMRES3            : 1;    ///< 04000000: DCM3 reset
   unsigned int DCMRES4            : 1;    ///< 08000000: DCM3 reset
   unsigned int MST_CLK_SLOW       : 1;    ///< 10000000: Master: use clk_slow
   unsigned int DCM_500            : 1;    ///< 20000000: DCM select for > 450 MHz
   unsigned int _nu_C0000000       : 2;    //   C0000000

} PX14S_REG_DEV_05;

/// Default register value for register 5
#define PX14REGDEFAULT_5                    0x00001C00
#define PX14REGIDX_TRIGGER_MODE             5
#define PX14REGMSK_TRIGGER_MODE             0x00000003
#define PX14REGSHR_TRIGGER_MODE             0
#define PX14REGIDX_TRIGGER_SOURCE           5
#define PX14REGMSK_TRIGGER_SOURCE           0x0000000C
#define PX14REGSHR_TRIGGER_SOURCE           2
#define PX14REGIDX_TRIGGER_DIRA             5
#define PX14REGMSK_TRIGGER_DIRA             0x00000010
#define PX14REGSHR_TRIGGER_DIRA             4
#define PX14REGIDX_TRIGGER_DIRB             5
#define PX14REGMSK_TRIGGER_DIRB             0x00000020
#define PX14REGSHR_TRIGGER_DIRB             5
#define PX14REGIDX_TRIGGER_DIR_EXT          5
#define PX14REGMSK_TRIGGER_DIR_EXT          0x00000040
#define PX14REGSHR_TRIGGER_DIR_EXT          6
#define PX14REGIDX_STR                      5
#define PX14REGMSK_STR                      0x00000080
#define PX14REGSHR_STR                      7
#define PX14REGIDX_DTRIG_ENABLE             5
#define PX14REGMSK_DTRIG_ENABLE             0x00000100
#define PX14REGSHR_DTRIG_ENABLE             8
#define PX14REGIDX_PTRIG_ENABLE             5
#define PX14REGMSK_PTRIG_ENABLE             0x00000200
#define PX14REGSHR_PTRIG_ENABLE             9
#define PX14REGIDX_CG_PD_                   5
#define PX14REGMSK_CG_PD_                   0x00000400
#define PX14REGSHR_CG_PD_                   10
#define PX14REGIDX_CG_RST_                  5
#define PX14REGMSK_CG_RST_                  0x00000800
#define PX14REGSHR_CG_RST_                  11
#define PX14REGIDX_CG_SYNC_                 5
#define PX14REGMSK_CG_SYNC_                 0x00001000
#define PX14REGSHR_CG_SYNC_                 12
#define PX14REGIDX_CG_REF_SEL               5
#define PX14REGMSK_CG_REF_SEL               0x00002000
#define PX14REGSHR_CG_REF_SEL               13
#define PX14REGIDX_NO_DCM                   5
#define PX14REGMSK_NO_DCM                   0x00004000
#define PX14REGSHR_NO_DCM                   14
#define PX14REGIDX_DCM_SEL                  5
#define PX14REGMSK_DCM_SEL                  0x00008000
#define PX14REGSHR_DCM_SEL                  15
#define PX14REGIDX_ADCCLOCK_REF             5
#define PX14REGMSK_ADCCLOCK_REF             0x00010000
#define PX14REGSHR_ADCCLOCK_REF             16
#define PX14REGIDX_ADCCLOCK_SOURCE          5
#define PX14REGMSK_ADCCLOCK_SOURCE          0x00060000
#define PX14REGSHR_ADCCLOCK_SOURCE          17
#define PX14REGIDX_POSTADC_CLKDIV           5
#define PX14REGMSK_POSTADC_CLKDIV           0x00F80000
#define PX14REGSHR_POSTADC_CLKDIV           19
#define PX14REGIDX_DCM1_RESET               5
#define PX14REGMSK_DCM1_RESET               0x01000000
#define PX14REGSHR_DCM1_RESET               24
#define PX14REGIDX_DCM2_RESET               5
#define PX14REGMSK_DCM2_RESET               0x02000000
#define PX14REGSHR_DCM2_RESET               25
#define PX14REGIDX_DCM3_RESET               5
#define PX14REGMSK_DCM3_RESET               0x04000000
#define PX14REGSHR_DCM3_RESET               26
#define PX14REGIDX_DCM4_RESET               5
#define PX14REGMSK_DCM4_RESET               0x08000000
#define PX14REGSHR_DCM4_RESET               27
#define PX14REGIDX_MST_CLK_SLOW             5
#define PX14REGMSK_MST_CLK_SLOW             0x10000000
#define PX14REGSHR_MST_CLK_SLOW             28
#define PX14REGIDX_DCM_500                  5
#define PX14REGMSK_DCM_500                  0x20000000
#define PX14REGSHR_DCM_500                  29

/// Register 6 : Board Settings
typedef struct _PX14S_REG_DEV_06_tag
{
   unsigned int _nu_00000001     : 1;  //   00000001
   unsigned int DOUT             : 3;  ///< 0000000E: Digital output selection
   unsigned int DOUT_EN          : 1;  ///< 00000010: Digital output enable
   unsigned int _nu_00000060     : 2;  //   00000060
   unsigned int FPGA_PROC        : 1;  ///< 00000080: Custom FPGA processing en
   unsigned int TSRES            : 1;  ///< 00000100: Timestamp reset
   unsigned int TSFIFO_RES       : 1;  ///< 00000200: Timestamp FIFO reset
   unsigned int _nu_00000C00     : 2;  //   00000C00
   unsigned int TS_MODE          : 3;  ///< 00007000: Timestamp mode
   unsigned int TS_CNT_MODE      : 1;  ///< 00008000: Timestamp counter mode
   unsigned int MS_CFG           : 4;  ///< 000F0000: Master slave config
   unsigned int D2VR_CH1         : 3;   ///< 00700000: Voltage range for PX14400D2 (ch1)
   unsigned int D2VR_CH2         : 3;   ///< 03800000: Voltage range for PX14400D2 (ch2)
   unsigned int _nu_3C000000      : 4;  //   3C000000
   unsigned int MTRIGSEL         : 1;  ///< 40000000: Master trigger select
   unsigned int _nu_80000000     : 1;  //   80000000

} PX14S_REG_DEV_06;

/// Default register value for register 6
#define PX14REGDEFAULT_6                    0x00000000
#define PX14REGIDX_DOUT_SEL                 6
#define PX14REGMSK_DOUT_SEL                 0x0000000E
#define PX14REGSHR_DOUT_SEL                 1
#define PX14REGIDX_DOUT_EN                  6
#define PX14REGMSK_DOUT_EN                  0x00000010
#define PX14REGSHR_DOUT_EN                  4
#define PX14REGIDX_FPGA_PROC                6
#define PX14REGMSK_FPGA_PROC                0x00000080
#define PX14REGSHR_FPGA_PROC                7
#define PX14REGIDX_TS_RESET                 6
#define PX14REGMSK_TS_RESET                 0x00000100
#define PX14REGSHR_TS_RESET                 8
#define PX14REGIDX_TSFIFO_RESET             6
#define PX14REGMSK_TSFIFO_RESET             0x00000200
#define PX14REGSHR_TSFIFO_RESET             9
#define PX14REGIDX_TS_MODE                  6
#define PX14REGMSK_TS_MODE                  0x00007000
#define PX14REGSHR_TS_MODE                  12
#define PX14REGIDX_TS_CNT_MODE              6
#define PX14REGMSK_TS_CNT_MODE              0x00008000
#define PX14REGSHR_TS_CNT_MODE              15
#define PX14REGIDX_MS_CFG                   6
#define PX14REGMSK_MS_CFG                   0x000F0000
#define PX14REGSHR_MS_CFG                   16
#define PX14REGIDX_D2VR_CH1                 6
#define PX14REGMSK_D2VR_CH1                 0x00700000
#define PX14REGSHR_D2VR_CH1                 20
#define PX14REGIDX_D2VR_CH2                 6
#define PX14REGMSK_D2VR_CH2                 0x03800000
#define PX14REGSHR_D2VR_CH2                 23
#define PX14REGIDX_MTRIGSEL                 6
#define PX14REGMSK_MTRIGSEL                 0x40000000
#define PX14REGSHR_MTRIGSEL                 30

/// Register 7 : Analog front end
typedef struct _PX14S_REG_DEV_07_tag
{
   unsigned int ATT1             : 5;  ///< 0000001F: AC: Ch. 1 attenuation
   unsigned int XFMR1_SEL        : 1;  ///< 00000020: AC: Prototype only
   unsigned int ADCPD1           : 1;  //   00000040
   unsigned int AMPPU1           : 1;  ///< 00000080: AC
   unsigned int dac_sync_        : 1;  ///< 00000100: DC:
   unsigned int dac_clr_         : 1;  ///< 00000200: DC
   unsigned int higain           : 1;  ///< 00000400: DC: Gain select
   unsigned int pd1_logain_ch1_  : 1;  ///< 00000800: DC
   unsigned int pd2_higain_ch1_  : 1;  ///< 00001000: DC
   unsigned int pd3_logain_ch2_  : 1;  ///< 00002000: DC
   unsigned int pd4_higain_ch2_  : 1;  ///< 00004000: DC
   unsigned int d2_vr_sel_protect: 1;  ///< 00008000: D2: Disable some ch2 vr sel bits
   unsigned int ATT2             : 5;  ///< 001F0000: AC: Ch. 2 attenuation
   unsigned int XFMR2_SEL        : 1;  ///< 00200000: AC: Prototype only
   unsigned int ADCPD2           : 1;  //   00400000
   unsigned int AMPPU2           : 1;  ///< 00800000: AC:
   unsigned int _nu_3F000000     : 6;  //   3F000000
   unsigned int CHANSEL          : 2;  ///< C0000000: Active channels

} PX14S_REG_DEV_07;

/// Default register value for register 7; assuming amplified channels
#define PX14REGDEFAULT_7                  0x00942894
#define PX14REGIDX_ATT1                   7
#define PX14REGMSK_ATT1                   0x0000001F
#define PX14REGSHR_ATT1                   0
#define PX14REGIDX_XFMR1_SEL              7
#define PX14REGMSK_XFMR1_SEL              0x00000020
#define PX14REGSHR_XFMR1_SEL              5
#define PX14REGIDX_ADCPD1                 7
#define PX14REGMSK_ADCPD1                 0x00000040
#define PX14REGSHR_ADCPD1                 6
#define PX14REGIDX_AMPPU1                 7
#define PX14REGMSK_AMPPU1                 0x00000080
#define PX14REGSHR_AMPPU1                 7
#define PX14REGMSK_DAC_SYNC_              0x00000100
#define PX14REGMSK_DAC_CLR_               0x00000200
#define PX14REGMSK_HIGAIN                 0x00000400
#define PX14REGMSG_PD1_LOGAIN_CH1_        0x00000800
#define PX14REGMSG_PD2_HIGAIN_CH1_        0x00001000
#define PX14REGMSG_PD3_LOGAIN_CH2_        0x00002000
#define PX14REGMSG_PD4_HIGAIN_CH2_        0x00004000
#define PX14REGMSK_D2_VR_SEL_PROTECT      0x00008000
#define PX14REGIDX_ATT2                   7
#define PX14REGMSK_ATT2                   0x001F0000
#define PX14REGSHR_ATT2                   16
#define PX14REGIDX_XFMR2_SEL              7
#define PX14REGMSK_XFMR2_SEL              0x00200000
#define PX14REGSHR_XFMR2_SEL              21
#define PX14REGIDX_ADCPD2                 7
#define PX14REGMSK_ADCPD2                 0x00400000
#define PX14REGSHR_ADCPD2                 22
#define PX14REGIDX_AMPPU2                 7
#define PX14REGMSK_AMPPU2                 0x00800000
#define PX14REGSHR_AMPPU2                 23
#define PX14REGIDX_CHANSEL                7
#define PX14REGMSK_CHANSEL                0xC0000000
#define PX14REGSHR_CHANSEL                30

/// Register 8 : DAC register IO
typedef struct _PX14S_REG_DEV_08_tag
{
    unsigned int _nu_FFFFFFFF       : 32;

} PX14S_REG_DEV_08;

/// Register 9 : First SAB Register
typedef struct _PX14S_REG_DEV_09_tag
{
    unsigned int BN                 : 4;    ///< SAB board number
    unsigned int MD                 : 2;    ///< SAB mode
    unsigned int CLK                : 2;    ///< SAB clock
    unsigned int CNF                : 2;    ///< SAB configuration
    unsigned int DCMRST             : 1;    ///< SAB DCM reset
    unsigned int NO_SAB_DCM         : 1;    ///< Set when no DCM (<30MHz)
    unsigned int _nu_FFFFF000       : 20;

} PX14S_REG_DEV_09;

/// Default register value for register 8
#define PX14REGDEFAULT_8                    0x00000000

/// Default register value for register 9
#define PX14REGDEFAULT_9                    0x00000000
#define PX14REGIDX_SAB_BRDNUM               9
#define PX14REGMSK_SAB_BRDNUM               0x0000000F
#define PX14REGSHR_SAB_BRDNUM               0
#define PX14REGIDX_SAB_MODE                 9
#define PX14REGMSK_SAB_MODE                 0x00000030
#define PX14REGSHR_SAB_MODE                 4
#define PX14REGIDX_SAB_CLOCK                9
#define PX14REGMSK_SAB_CLOCK                0x000000C0
#define PX14REGSHR_SAB_CLOCK                6
#define PX14REGIDX_SAB_CNF                  9
#define PX14REGMSK_SAB_CNF                  0x00000300
#define PX14REGSHR_SAB_CNF                  8
#define PX14REGIDX_DCMRST                   9
#define PX14REGMSK_DCMRST                   0x00000400
#define PX14REGSHR_DCMRST                   10
#define PX14REGIDX_NO_SAB_DCM               9
#define PX14REGSHR_NO_SAB_DCM               11
#define PX14REGMSK_NO_SAB_DCM               0x00000800

/// Register A : Board Processing Parameter IO
typedef struct _PX14S_REG_DEV_0A_tag
{
   // Bit 0x4000 is used to specify a read register request
    unsigned int preg_idx           : 16;
    unsigned int preg_val           : 16;

} PX14S_REG_DEV_0A;

/// When this bit is set in preg_idx it identifies a read request
#define PX14_FPGAPROC_REG_READ_BIT         0x4000

/// Default register value for register A
#define PX14REGDEFAULT_A                    0x00000000

/// Register B : Operating mode
typedef struct _PX14S_REG_DEV_0B_tag
{
    unsigned int OM                 : 4;    ///< Operating mode
    unsigned int _nu_FFFFFFF0       : 28;

} PX14S_REG_DEV_0B;

/// Default register value for register C
#define PX14REGDEFAULT_B                    PX14MODE_STANDBY
#define PX14REGIDX_OPERATING_MODE           0xB
#define PX14REGMSK_OPERATING_MODE           0x0000000F
#define PX14REGSHR_OPERATING_MODE           0

/// Register C : RAM address
typedef struct _PX14S_REG_DEV_0C_tag
{
    unsigned int RA                 : 32;   ///< RAM address

} PX14S_REG_DEV_0C;

/// Default register value for register C
#define PX14REGDEFAULT_C                    0

/// Register D : Status/Monitor
typedef struct _PX14S_REG_DEV_0D_tag
{
    unsigned int SCF                : 1;    ///< Samples complete flag
    unsigned int SFF                : 1;    ///< Segment full flag
    unsigned int FFF                : 1;    ///< FIFO full flag
    unsigned int DCM1LCK            : 1;    ///< DCM1 lock status
    unsigned int ACQFIFO            : 1;    ///< AcQ FIFO overflow flag
    unsigned int ACQDCMLCKLATCHED   : 1;    ///< AcQ DCM lock status (Latched)
    unsigned int ACQDCMLCK          : 1;    ///< AcQ DCM lock status
    unsigned int TRIGSTAT           : 1;    ///< Trigger status
    unsigned int TSFIFO_OVFL        : 1;    ///< Timestamp FIFO overflow
    unsigned int CG_STATUS          : 1;    ///< CG status pin
    unsigned int CG_LOCK            : 1;    ///< CG PLL lock status pin
    unsigned int CG_REFMON          : 1;    ///< CG reference monitor pin
    unsigned int SLAVE_SYNC2        : 1;
    unsigned int TSFIFO_AF          : 1;    ///< Timestamp FIFO almost full
    unsigned int TSFIFO_EMPTY       : 1;    ///< Timestamp FIFO empty
    unsigned int SLAVE_SYNC1        : 1;
    unsigned int _nu_00300000       : 6;
    unsigned int FPGA_Temp          : 10;   ///< FPGA Temperature reading

} PX14S_REG_DEV_0D;

#define PX14REGIDX_SAMP_COMP                0xD
#define PX14REGMSK_SAMP_COMP                0x00000001
#define PX14REGSHR_SAMP_COMP                0
#define PX14REGIDX_SEG_FULL                 0xD
#define PX14REGMSK_SEG_FULL                 0x00000002
#define PX14REGSHR_SEG_FULL                 1
#define PX14REGIDX_FIFO_FULL                0xD
#define PX14REGMSK_FIFO_FULL                0x00000004
#define PX14REGSHR_FIFO_FULL                2
#define PX14REGIDX_DCM1_LOCK                0xD
#define PX14REGMSK_DCM1_LOCK                0x00000008
#define PX14REGSHR_DCM1_LOCK                3
#define PX14REGIDX_ACQFIFO                  0xD
#define PX14REGMSK_ACQFIFO                  0x00000010
#define PX14REGSHR_ACQFIFO                  4
#define PX14REGIDX_ACQDCMLCKLATCHED         0xD
#define PX14REGMSK_ACQDCMLCKLATCHED         0x00000020
#define PX14REGSHR_ACQDCMLCKLATCHED         5
#define PX14REGIDX_ACQDCMLCK                0xD
#define PX14REGMSK_ACQDCMLCK                0x00000040
#define PX14REGSHR_ACQDCMLCK                6
#define PX14REGIDX_TRIG_STATUS              0xD
#define PX14REGMSK_TRIG_STATUS              0x00000080
#define PX14REGSHR_TRIG_STATUS              7
#define PX14REGIDX_TS_OVRF                  0xD
#define PX14REGMSK_TS_OVRF                  0x00000100
#define PX14REGSHR_TS_OVRF                  8
#define PX14REGIDX_CGSTAT                   0xD
#define PX14REGMSK_CGSTAT                   0x00000200
#define PX14REGSHR_CGSTAT                   9
#define PX14REGIDX_CG_LOCK                  0xD
#define PX14REGMSK_CG_LOCK                  0x00000400
#define PX14REGSHR_CG_LOCK                  10
#define PX14REGIDX_CG_REF_MON               0xD
#define PX14REGMSK_CG_REF_MON               0x00000800
#define PX14REGSHR_CG_REF_MON               11
#define PX14REGIDX_SLAVE_SYNC2              0xD
#define PX14REGMSK_SLAVE_SYNC2              0x00001000
#define PX14REGSHR_SLAVE_SYNC2              12
#define PX14REGIDX_TSFIFO_EMPTY             0xD
#define PX14REGMSK_TSFIFO_EMPTY             0x00004000
#define PX14REGSHR_TSFIFO_EMPTY             14
#define PX14REGIDX_SLAVE_SYNC1              0xD
#define PX14REGMSK_SLAVE_SYNC1              0x00008000
#define PX14REGSHR_SLAVE_SYNC1              15
#define PX14REGIDX_FPGA_TEMP                0xD
#define PX14REGMSK_FPGA_TEMP                0xFFC00000
#define PX14REGSHR_FPGA_TEMP                16

/// Register E : Timestamp MSB
typedef struct _PX14S_REG_DEV_0E_tag
{
    unsigned int TS_MSB             : 32;   ///< MSB of timestamp

} PX14S_REG_DEV_0E;


/// Register F : Timestamp LSB
typedef struct _PX14S_REG_DEV_0F_tag
{
    unsigned int TS_LSB             : 32;   ///< LSB of timestamp

} PX14S_REG_DEV_0F;

/// Register 10 : Trigger
typedef struct _PX14S_REG_DEV_10_tag
{
   unsigned int TODPT_Mode          : 1;    //   00000001
   unsigned int _nu_00001FFF        : 12;   //   00001FFE
   unsigned int trig_sel            : 3;    //   0000E000
   unsigned int hyst                : 5;    //   001F0000
   unsigned int _nu_FFE00000        : 11;   //   FFE00000

} PX14S_REG_DEV_10;

#define PX14REGIDX_TODPT               0x10
#define PX14REGMSK_10_TODPT            0x00000001
#define PX14REGSHR_TODPT               0
#define PX14REGIDX_TRIG_SEL            0x10
#define PX14REGMSK_10_TRIG_SEL         0x0000E000
#define PX14REGSHR_TRIG_SEL            1
#define PX14REGIDX_HYST                0x10
#define PX14REGMSK_10_HYST             0x001F0000
#define PX14REGSHR_HYST                2

#define PX14REGDEFAULT_10              0x000A6000

/// Register 11 : Clear status
typedef struct _PX14S_REG_DEV_11_tag
{
   unsigned int ACQ_IN_FIFO_CLEAR   : 1;   ///< ACQ in fifo clear
   unsigned int DCM_LOCK3_CLR       : 1;   ///< DCM LOCK3 CLR
   unsigned int RESERVED_BITS       : 30;  ///< RESERVED BITS

} PX14S_REG_DEV_11;

#define PX14REGIDX_ACQ_IN_FIFO_CLEAR            0x11
#define PX14REGMSK_ACQ_IN_FIFO_CLEAR            0x00000001
#define PX14REGSHR_ACQ_IN_FIFO_CLEAR            0
#define PX14REGIDX_DCM_LOCK3_CLR                0x11
#define PX14REGMSK_DCM_LOCK3_CLR                0x00000002
#define PX14REGSHR_DCM_LOCK3_CLR                1

#define PX14REGDEFAULT_11            0x00000000


// -- Union wrappers around device registers for register value access

typedef union _PX14U_REG_DEV_00_tag
{
    PX14S_REG_DEV_00    bits;
    unsigned int        val;

} PX14U_REG_DEV_00;

typedef union _PX14U_REG_DEV_01_tag
{
    PX14S_REG_DEV_01    bits;
    unsigned int        val;

} PX14U_REG_DEV_01;

typedef union _PX14U_REG_DEV_02_tag
{
    PX14S_REG_DEV_02    bits;
    unsigned int        val;

} PX14U_REG_DEV_02;

typedef union _PX14U_REG_DEV_03_tag
{
    PX14S_REG_DEV_03    bits;
    unsigned int        val;

} PX14U_REG_DEV_03;

typedef union _PX14U_REG_DEV_04_tag
{
    PX14S_REG_DEV_04    bits;
    unsigned int        val;

} PX14U_REG_DEV_04;

typedef union _PX14U_REG_DEV_05_tag
{
    PX14S_REG_DEV_05    bits;
    unsigned int        val;

} PX14U_REG_DEV_05;

typedef union _PX14U_REG_DEV_06_tag
{
    PX14S_REG_DEV_06    bits;
    unsigned int        val;

} PX14U_REG_DEV_06;

typedef union _PX14U_REG_DEV_07_tag
{
    PX14S_REG_DEV_07    bits;
    unsigned int        val;

} PX14U_REG_DEV_07;

typedef union _PX14U_REG_DEV_08_tag
{
    PX14S_REG_DEV_08    bits;
    unsigned int        val;

} PX14U_REG_DEV_08;

typedef union _PX14U_REG_DEV_09_tag
{
    PX14S_REG_DEV_09    bits;
    unsigned int        val;

} PX14U_REG_DEV_09;

typedef union _PX14U_REG_DEV_0A_tag
{
    PX14S_REG_DEV_0A    bits;
    unsigned int        val;

} PX14U_REG_DEV_0A;

typedef union _PX14U_REG_DEV_0B_tag
{
    PX14S_REG_DEV_0B    bits;
    unsigned int        val;

} PX14U_REG_DEV_0B;

typedef union _PX14U_REG_DEV_0C_tag
{
    PX14S_REG_DEV_0C    bits;
    unsigned int        val;

} PX14U_REG_DEV_0C;

typedef union _PX14U_REG_DEV_0D_tag
{
    PX14S_REG_DEV_0D    bits;
    unsigned int        val;

} PX14U_REG_DEV_0D;

typedef union _PX14U_REG_DEV_0E_tag
{
    PX14S_REG_DEV_0E    bits;
    unsigned int        val;

} PX14U_REG_DEV_0E;

typedef union _PX14U_REG_DEV_0F_tag
{
    PX14S_REG_DEV_0F    bits;
    unsigned int        val;

} PX14U_REG_DEV_0F;

typedef union _PX14U_REG_DEV_10_tag
{
    PX14S_REG_DEV_10    bits;
    unsigned int        val;

} PX14U_REG_DEV_10;

typedef union _PX14U_REG_DEV_11_tag
{
    PX14S_REG_DEV_11    bits;
    unsigned int        val;

} PX14U_REG_DEV_11;

// -- Aggregation of all PX14 device registers

typedef struct _PX14S_DEVICE_REGISTER_SET_tag
{
   PX14U_REG_DEV_00    reg0;     ///< Segment size
   PX14U_REG_DEV_01    reg1;     ///< Total sample count
   PX14U_REG_DEV_02    reg2;     ///< Starting address
   PX14U_REG_DEV_03    reg3;     ///< Delay/Pretrigger samples
   PX14U_REG_DEV_04    reg4;     ///< Trigger levels
   PX14U_REG_DEV_05    reg5;     ///< Trigger and clock
   PX14U_REG_DEV_06    reg6;     ///< Board settings
   PX14U_REG_DEV_07    reg7;     ///< Analog front end
   PX14U_REG_DEV_08    reg8;     ///< Unused
   PX14U_REG_DEV_09    reg9;     ///< SAB 1
   PX14U_REG_DEV_0A    regA;     ///< SAB 2
   PX14U_REG_DEV_0B    regB;     ///< Operating mode
   PX14U_REG_DEV_0C    regC;     ///< RAM address (read-only) - ADC Clock Measure (Read-only)
   PX14U_REG_DEV_0D    regD;     ///< Status/monitor (read-only)
   PX14U_REG_DEV_0E    regE;     ///< Timestamp (upper)
   PX14U_REG_DEV_0F    regF;     ///< Timestamp (upper)
   PX14U_REG_DEV_10   reg10;     ///< Trigger
   PX14U_REG_DEV_11   reg11;     ///< Clear status (Write-Only)

} PX14S_DEVICE_REGISTER_SET;

typedef union _PX14U_DEVICE_REGISTER_SET_tag
{
    unsigned int                values[PX14_DEVICE_REG_COUNT];
    PX14S_DEVICE_REGISTER_SET   fields;

} PX14U_DEVICE_REGISTER_SET;

#define PX14REGIDX_CLKGEN                   0x20

/** @brief Serial Data Register IO

    This is the structure of a serial data register read or write
    request. The Clock Generator devices are controled via this
   interface. The clock generator has a 32-bit device register that we
   use to control clock generator bits.

*/
typedef struct _PX14S_SDREGIO_tag
{
    unsigned int CG_DATA            : 8;    ///< Register data
    unsigned int CG_ADDR            : 10;   ///< Register offset
    unsigned int _nu_007C0000       : 5;
    unsigned int CG_RW_             : 1;    ///< Read or Write-
    unsigned int _nu_FF000000       : 8;

} PX14S_SDREG_IO;

typedef union _PX14U_SDREGIO_tag
{
    PX14S_SDREG_IO      bits;
    unsigned int        val;

} PX14U_SDREGIO;

#define PX14REGIDX_DC_DAC               0x08

typedef struct _PX14S_DACREGIO_tag
{
   unsigned int _nu_000000FF     : 8;
   unsigned int reg_data         : 12;
   unsigned int reg_addr         : 4;
   unsigned int cmd              : 4;
   unsigned int _nu_F0000000     : 4;

} PX14S_DACREG_IO;

typedef union _PX14U_DACREGIO_tag
{
   PX14S_DACREG_IO bits;
   unsigned int    val;

} PX14U_DACREGIO;

// -- DAC addresses
#define PX14DACADDR_I1_POS       0
#define PX14DACADDR_I1_NEG       1
#define PX14DACADDR_Q1_POS       2
#define PX14DACADDR_Q1_NEG       3
#define PX14DACADDR_I2_POS       4
#define PX14DACADDR_I2_NEG       5
#define PX14DACADDR_Q2_POS       6
#define PX14DACADDR_Q2_NEG       7
#define PX14DACADDR_ALL_DACS     8

// -- DAC commands
/// Write to and update DAC Channel N
#define PX14DACCMD_WRITE_AND_UPDATE    3
/// Power down/power up DAC
#define PX14DACCMD_POWER_UPDOWN        4
/// Reset (power-on reset)
#define PX14DACCMD_RESET               7
/// Set up internal REF register
#define PX14DACCMD_INIT_REF            8

////
// The PX14400D2 has a different DC offset DAC part
////

typedef struct _PX14S_DACREGD2IO_tag
{
   unsigned int dac_data         : 12; // 00000FFF
   unsigned int dac_addr         : 4;  // 0000F000
   unsigned int nu_FFFF0000      : 16; // FFFF0000

} PX14S_DACREGD2IO;

typedef union _PX14U_DACREGD2IO_tag
{
   PX14S_DACREGD2IO  bits;
   unsigned int      val;

} PX14U_DACREGD2IO;

/// Post write ms delay to use for PX14400D2 DAC IO
#define PX14DACD2_WDELAY_MS                 60

#define PX14DACD2ADDR_DACA                  0   // -> Ch 1 coarse
#define PX14DACD2ADDR_DACB                  1   // -> Ch 1 fine
#define PX14DACD2ADDR_DACC                  2   // -> Ch 2 fine
#define PX14DACD2ADDR_DACD                  3   // -> MSB of D2VR_CH2 for first few D2 cards
#define PX14DACD2ADDR_DACE                  4
#define PX14DACD2ADDR_DACF                  5   // -> Ch 3 coarse
#define PX14DACD2ADDR_DACG                  6
#define PX14DACD2ADDR_DACH                  7
#define PX14DACD2ADDR_CTRL0                 8
#define PX14DACD2ADDR_CTRL1                 9

//########################################################################//
//
// __CLOCK__
//
// -- Clock generation registers
//
// The PX14 has one clock genereration device (AD9516-3) that we use for
//  board operations. IO to this device is done through device register 32
//
// The actual clock gen registers are all 8-bit registers. To keep things
//  more simple on the software end, we pack these registers into 32-bit
//  logical registers. Each 32-bit logical register will contain the state
//  for up to 4 8-bit AD9516 registers
//
// Rules:
//
// 1. All structures should be 32-bits in size
// 2. Each significant bit field should have the following defined:
//    PX14CLKREGMSK_AAA_*   Mask in register AAA that defines the field
// 3. Each logical register should have the following defined:
//    PX14CLKREGDEF_*       Default register value that we should use when
//                          initializing registers to our preferred default
//                          values
//
//########################################################################//

#define PX14CLKREGDEF_00                    0x00000018
#define PX14CLKREGDEF_01                    0x0000648C
#define PX14CLKREGDEF_02                    0xB4060271
#define PX14CLKREGDEF_03                    0x00000006
#define PX14CLKREGDEF_04                    0x00000002
#define PX14CLKREGDEF_05                    0x00000001
#define PX14CLKREGDEF_06                    0x00000001
#define PX14CLKREGDEF_07                    0x00000001
#define PX14CLKREGDEF_08                    0x00000001
#define PX14CLKREGDEF_09                    0x09090808
#define PX14CLKREGDEF_10                    0x00000909
#define PX14CLKREGDEF_11                    0x01010101
#define PX14CLKREGDEF_12                    0x00018000
#define PX14CLKREGDEF_12_PX12500            0x00010000
#define PX14CLKREGDEF_13                    0x00008000
#define PX14CLKREGDEF_14                    0x00008000
#define PX14CLKREGDEF_15                    0x00000000
#define PX14CLKREGDEF_16                    0x00003000
#define PX14CLKREGDEF_17                    0x00000000
#define PX14CLKREGDEF_18                    0x00003000
#define PX14CLKREGDEF_19                    0x00000003
#define PX14CLKREGDEF_19_PX12500            0x00000000

#define PX14_CLOCK_REG_COUNT                20

/// CG logical register 0 { 00, 04, --, -- }
typedef struct _PX14S_REG_CLK_LOG_00_tag
{
    // Serial Port Configuration (Reg 0, Default 0x18)
    unsigned int SDO_Active         : 1;
    unsigned int LSB_First          : 1;
    unsigned int SoftReset          : 1;
    unsigned int LongInstruction    : 1;    // MnoitcurtsnI fo rorriM
    unsigned int LongInstructionM   : 1;    // Mirror of LongInstruction
    unsigned int SoftResetM         : 1;    // Mirror of SoftReset
    unsigned int LSB_FirstM         : 1;    // Mirror of LSB_First
    unsigned int SDO_ActiveM        : 1;    // Mirror of SDO_Active

    // Read Back Control (Base 04, Default 0x01)
    unsigned int ReadBackActiveRegs : 1;
    unsigned int _nu_0000FE00       : 7;

    unsigned int _nu_FFFF0000       : 16;

} PX14S_REG_CLK_LOG_00;

/// CG logical register 1 { 10, 11, 12, 13 }
typedef struct _PX14S_REG_CLK_LOG_01_tag
{
    // PFD and Charge Pump (Reg 0x10, Default 0x8C)
    unsigned int PLL_PowerDown      : 2;
    unsigned int ChargePumpMode     : 2;
    unsigned int ChargePumpCurrent  : 3;
    unsigned int PFD_Polarity       : 1;

    // R Counter (LSB) (Reg 0x11, Default 0x64)
    unsigned int R_Divider_LSB      : 8;

    // R Counter (MSB) (Reg 0x12, Default 0x00)
    unsigned int R_Divider_MSB      : 6;
    unsigned int _nu_00C00000       : 2;

    // A Counter (Reg 0x13, Default 0x00)
    unsigned int A_Counter          : 6;
    unsigned int _nu_C000000        : 2;

} PX14S_REG_CLK_LOG_01;

#define PX14CLKREGMSK_010_PLL_POWERDOWN     0x03
#define PX14CLKREGMSK_010_CP_MODE           0x0C
#define PX14CLKREGMSK_010_CP_CURRENT        0x70
#define PX14CLKREGMSK_010_PFD_POLARITY      0x80
#define PX14CLKREGMSK_011_R_DIV_LSB         0xFF
#define PX14CLKREGMSK_012_R_DIV_MSB         0x3F
#define PX14CLKREGMSK_013_A_COUNTER         0x3F

/// CG logical register 2 { 14, 15, 16, 17 }
typedef struct _PX14S_REG_CLK_LOG_02_tag
{
    // B Counter (LSB) (Reg 0x14, Default 0x71)
    unsigned int B_Counter_LSB      : 8;

    // B Counter (MSB) (Reg 0x15, Default 0x02)
    unsigned int B_Counter_MSB      : 5;
    unsigned int _nu_0000E000       : 3;

    // PLL Control 1 (Reg 0x16, HW Default 0x06)
    unsigned int Prescaler_P        : 3;
    unsigned int B_CounterBypass    : 1;
    unsigned int ResetAllCounters   : 1;
    unsigned int ResetABCounters    : 1;
    unsigned int ResetRCounter      : 1;
    unsigned int SetCpPinToVCPDiv2  : 1;

    // PLL Control 2 (Reg 0x17, HW Default 0x00)
    unsigned int AntibacklashPw     : 2;
    unsigned int StatusPinCtrl      : 6;

} PX14S_REG_CLK_LOG_02;

#define PX14CLKREGMSK_014_B_CNT_LSB         0xFF
#define PX14CLKREGMSK_015_B_CNT_MSB         0x1F
#define PX14CLKREGMSK_016_PRESCALER_P       0x07
#define PX14CLKREGMSK_016_B_CNT_BYPASS      0x08
#define PX14CLKREGMSK_016_RST_ALL_CNT       0x10
#define PX14CLKREGMSK_016_RESET_AB_CNT      0x20
#define PX14CLKREGMSK_016_RESET_R_CNT       0x40
#define PX14CLKREGMSK_017_STATUSPINCTRL     0xFC


/// CG logical register 3 { 18, 19, 1A, 1B }
typedef struct _PX14S_REG_CLK_LOG_03_tag
{
    // PLL Control 3 (Reg 0x18, HW Default 0x06)
    unsigned int VCO_CalNow         : 1;
    unsigned int VCO_CalDivider     : 2;
    unsigned int DisableDigLockDet  : 1;
    unsigned int DigitalLockDetWin  : 1;
    unsigned int LockDetectCounter  : 2;
    unsigned int _nu_00000080       : 1;

    // PLL Control 4 (Reg 0x19, HW Default 0x00)
    unsigned int N_PathDelay        : 3;
    unsigned int R_PathDelay        : 3;
    unsigned int CountersSyncPinRst : 2;

    // PLL Control 5 (Reg 0x1A, HW Default 0x00)
    unsigned int LD_PinControl      : 6;
    unsigned int RefFreqMonThresh   : 1;
    unsigned int _nu_00800000       : 1;

    // PLL Control 6 (Reg 0x1B, HW Default 0x00)
    unsigned int RefmonPinControl   : 5;
    unsigned int Ref1FreqMon        : 1;
    unsigned int Ref2FreqMon        : 1;
    unsigned int VCO_FreqMon        : 1;

} PX14S_REG_CLK_LOG_03;

/// CG logical register 4 { 1C, 1D, 1E, 1F }
typedef struct _PX14S_REG_CLK_LOG_04_tag
{
    // PLL Control 7 (Reg 0x1C, HW Default 0x02)
    unsigned int DifferentialRef    : 1;
    unsigned int Ref1PowerOn        : 1;
    unsigned int Ref2PowerOn        : 1;
    unsigned int StayOnRef2         : 1;
    unsigned int AutoRefSwitchover  : 1;
    unsigned int UseRefSelPin       : 1;
    unsigned int SelectRef2         : 1;
    unsigned int DsbleSwchOvDeglich : 1;

    // PLL Control 8 (Reg 0x1D, HW Default 00)
    unsigned int HoldoverEnable1    : 1;
    unsigned int ExtHoldoverCtrl    : 1;
    unsigned int HoldoverEnable2    : 1;
    unsigned int LD_PinCompEnable   : 1;
    unsigned int PLL_StatRegDisable : 1;
    unsigned int _nu_0000E000       : 3;

    // PLL Control 9 (Reg 0x1E, HW Default 0)
    unsigned int _nu_00FF0000       : 8;

    // PLL Readback (Reg 0x1F, HW Default --)
    unsigned int DigitalLockDetect  : 1;
    unsigned int Ref1FreqGtThresh   : 1;
    unsigned int Ref2FreqGtThresh   : 1;
    unsigned int VCO_FreqGtThresh   : 1;
    unsigned int Ref2Selected       : 1;
    unsigned int HoldoverActive     : 1;
    unsigned int VCO_CalFinished    : 1;
    unsigned int _nu_80000000       : 1;

} PX14S_REG_CLK_LOG_04;

#define PX14CLKREGMSK_01C_REF1POWERON       0x02
#define PX14CLKREGMSK_01C_REF2POWERON       0x04
#define PX14CLKREGMSK_01C_USEREFSELPIN      0x20
#define PX14CLKREGMSK_01C_SELECTREF2        0x40

/// CG logical register 5 { A0, A1, A2, -- }
typedef struct _PX14S_REG_CLK_LOG_05_tag
{
    // OUT6 Delay Bypass (Reg 0xA0, HW Default 0x01)
    unsigned int Out6DelayBypass    : 1;
    unsigned int _nu_000000FE       : 7;

    // OUT6 Delay Full-Scale
    unsigned int Out6RampCurrent    : 3;
    unsigned int Out6RampCaps       : 3;
    unsigned int _nu_0000C000       : 2;

    // OUT6 Delay Fraction
    unsigned int Out6DelayFunction  : 6;
    unsigned int _nu_00C00000       : 2;

    unsigned int _nu_FF000000       : 8;

} PX14S_REG_CLK_LOG_05;

/// CG logical register 6 { A3, A4, A5, -- }
typedef struct _PX14S_REG_CLK_LOG_06_tag
{
    // OUT7 Delay Bypass (Reg 0xA3, HW Default 0x01)
    unsigned int Out7DelayBypass    : 1;
    unsigned int _nu_000000FE       : 7;

    // OUT7 Delay Full-Scale (Reg 0xA4, HW Default 0)
    unsigned int Out7RampCurrent    : 3;
    unsigned int Out7RampCaps       : 3;
    unsigned int _nu_0000C000       : 2;

    // OUT7 Delay Fraction (Reg 0xA5, HW Default 0)
    unsigned int Out7DelayFunction  : 6;
    unsigned int _nu_00C00000       : 2;

    unsigned int _nu_FF000000       : 8;

} PX14S_REG_CLK_LOG_06;

/// CG logical register 7 { A6, A7, A8, -- }
typedef struct _PX14S_REG_CLK_LOG_07_tag
{
    // OUT8 Delay Bypass (Reg 0xA6, HW Default 0x01)
    unsigned int Out8DelayBypass    : 1;
    unsigned int _nu_000000FE       : 7;

    // OUT8 Delay Full-Scale (Reg 0xA7, HW Default 0)
    unsigned int Out8RampCurrent    : 3;
    unsigned int Out8RampCaps       : 3;
    unsigned int _nu_0000C000       : 2;

    // OUT8 Delay Fraction (Reg 0xA8, HW Default 0)
    unsigned int Out8DelayFunction  : 6;
    unsigned int _nu_00C00000       : 2;

    unsigned int _nu_FF000000       : 8;

} PX14S_REG_CLK_LOG_07;

/// CG logical register 8 { A9, AA, AB, -- }
typedef struct _PX14S_REG_CLK_LOG_08_tag
{
    // OUT9 Delay Bypass (Reg 0xA9, HW Default 0x01)
    unsigned int Out9DelayBypass    : 1;
    unsigned int _nu_000000FE       : 7;

    // OUT9 Delay Full-Scale (Reg 0xAA, HW Default 0)
    unsigned int Out9RampCurrent    : 3;
    unsigned int Out9RampCaps       : 3;
    unsigned int _nu_0000C000       : 2;

    // OUT9 Delay Fraction (Reg 0xAB, HW Default 0)
    unsigned int Out9DelayFunction  : 6;
    unsigned int _nu_00C00000       : 2;

    unsigned int _nu_FF000000       : 8;

} PX14S_REG_CLK_LOG_08;

/// CG logical register 9 { F0, F1, F2, F3 }
typedef struct _PX14S_REG_CLK_LOG_09_tag
{
    // OUT0 (Reg 0xF0, Default 0x08)
    unsigned int Out0PowerDown      : 2;
    unsigned int Out0LvpeclDiffVolt : 2;
    unsigned int Out0Invert         : 1;
    unsigned int _nu_000000E0       : 3;

    // OUT1 (Reg 0xF1, Default 0x08)
    unsigned int Out1PowerDown      : 2;
    unsigned int Out1LvpeclDiffVolt : 2;
    unsigned int Out1Invert         : 1;
    unsigned int _nu_0000E000       : 3;

    // OUT2 (Reg 0xF2, Default 0x09)
    unsigned int Out2PowerDown      : 2;
    unsigned int Out2LvpeclDiffVolt : 2;
    unsigned int Out2Invert         : 1;
    unsigned int _nu_00E00000       : 3;

    // OUT3 (Reg 0xF3, Default 0x09)
    unsigned int Out3PowerDown      : 2;
    unsigned int Out3LvpeclDiffVolt : 2;
    unsigned int Out3Invert         : 1;
    unsigned int _nu_E0000000       : 3;

} PX14S_REG_CLK_LOG_09;

/// CG logical register 10 { F4, F5, --, -- }
typedef struct _PX14S_REG_CLK_LOG_10_tag
{
    // OUT4 (Reg 0xF4, Default 0x09)
    unsigned int Out4PowerDown      : 2;
    unsigned int Out4LvpeclDiffVolt : 2;
    unsigned int Out4Invert         : 1;
    unsigned int _nu_000000E0       : 3;

    // OUT5 (Reg 0xF5, Default 0x09)
    unsigned int Out5PowerDown      : 2;
    unsigned int Out5LvpeclDiffVolt : 2;
    unsigned int Out5Invert         : 1;
    unsigned int _nu_0000E000       : 3;

    unsigned int _nu_FFFF0000       : 16;

} PX14S_REG_CLK_LOG_10;

// These masks apply to registers F0-F5

#define PX14CLKREGMSK_0FX_OUTXPOWERDOWN     0x03
#define PX14CLKREGMSK_0FX_OUTXDIFFVOLT      0x0C
#define PX14CLKREGMSK_0FX_OUTXINVERT        0x10

/// CG logical register 11 { 140, 141, 142, 143 }
typedef struct _PX14S_REG_CLK_LOG_11_tag
{
    // OUT6 (Reg 0x140, HW Default 0x42)
    unsigned int Out6PowerDown      : 1;
    unsigned int Out6LvdsOutCurrent : 2;
    unsigned int Out6SelectLvdsCmos : 1;
    unsigned int Out6CmosB          : 1;
    unsigned int Out6LvdsCmosOutPol : 1;
    unsigned int Out6CmosOutPol     : 2;

    // OUT7 (Reg 0x141, HW Default 0x43)
    unsigned int Out7PowerDown      : 1;
    unsigned int Out7LvdsOutCurrent : 2;
    unsigned int Out7SelectLvdsCmos : 1;
    unsigned int Out7CmosB          : 1;
    unsigned int Out7LvdsCmosOutPol : 1;
    unsigned int Out7CmosOutPol     : 2;

    // OUT8 (Reg 0x142, HW Default 0x42)
    unsigned int Out8PowerDown      : 1;
    unsigned int Out8LvdsOutCurrent : 2;
    unsigned int Out8SelectLvdsCmos : 1;
    unsigned int Out8CmosB          : 1;
    unsigned int Out8LvdsCmosOutPol : 1;
    unsigned int Out8CmosOutPol     : 2;

    // OUT9 (Reg 0x143, HW Default 0x43)
    unsigned int Out9PowerDown      : 1;
    unsigned int Out9LvdsOutCurrent : 2;
    unsigned int Out9SelectLvdsCmos : 1;
    unsigned int Out9CmosB          : 1;
    unsigned int Out9LvdsCmosOutPol : 1;
    unsigned int Out9CmosOutPol     : 2;

} PX14S_REG_CLK_LOG_11;

#define PX14CLKREGMSK_14X_OUTXPOWERDOWN     0x01

/// CG logical register 12 { 190, 191, 192, -- }
typedef struct _PX14S_REG_CLK_LOG_12_tag
{
    // Divider 0 (Reg 0x190, HW Default 00)
    unsigned int Div0HighCycles     : 4;
    unsigned int Div0LowCycles      : 4;

    // Divider 0 (Reg 0x191, HW Default 0x80)
    unsigned int Div0PhaseOffset    : 4;
    unsigned int Div0StartHigh      : 1;
    unsigned int Div0ForceHigh      : 1;
    unsigned int Div0NoSync         : 1;
    unsigned int Div0Bypass         : 1;

    // Divider 0 (Reg 0x192, HW Default 0)
    unsigned int Div0Dccoff         : 1;
    unsigned int Div0DirectToOutput : 1;
    unsigned int _nu_00FC0000       : 6;

    unsigned int _nu_FF000000       : 8;

} PX14S_REG_CLK_LOG_12;

#define PX14CLKREGMSK_19X_DIVXPHASEOFFSET    0x0F

// - Note: Masks defined here may also be used with logical regs 13 and 14

#define PX14CLKREGMSK_XXX_DIVX_HIGHCYCLES   0x0F
#define PX14CLKREGMSK_XXX_DIVX_LOWCYCLES    0xF0

#define PX14CLKREGMSK_XXX_DIVX_PHASEOFF     0x0F
#define PX14CLKREGMSK_XXX_DIVX_STARTHIGH    0x10
#define PX14CLKREGMSK_XXX_DIVX_FORCEHIGH    0x20
#define PX14CLKREGMSK_XXX_DIVX_NOSYNC       0x40
#define PX14CLKREGMSK_XXX_DIVX_BYPASS       0x80

#define PX14CLKREGMSK_XXX_DIVX_DCCOFF       0x01
#define PX14CLKREGMSK_XXX_DIVX_DIR_TO_OUT   0x02


/// CG logical register 13 { 193, 194, 195, -- }
typedef struct _PX14S_REG_CLK_LOG_13_tag
{
    // Divider 1 (Reg 0x193, HW Default 00)
    unsigned int Div1HighCycles     : 4;
    unsigned int Div1LowCycles      : 4;

    // Divider 1 (Reg 0x194, HW Default 0x80)
    unsigned int Div1PhaseOffset    : 4;
    unsigned int Div1StartHigh      : 1;
    unsigned int Div1ForceHigh      : 1;
    unsigned int Div1NoSync         : 1;
    unsigned int Div1Bypass         : 1;

    // Divider 1 (Reg 0x195, HW Default 0)
    unsigned int Div1Dccoff         : 1;
    unsigned int Div1DirectToOutput : 1;
    unsigned int _nu_00FC0000       : 6;

    unsigned int _nu_FF000000       : 8;

} PX14S_REG_CLK_LOG_13;

/// CG logical register 14 { 196, 197, 198, -- }
typedef struct _PX14S_REG_CLK_LOG_14_tag
{
    // Divider 2 (Reg 0x196, HW Default 00)
    unsigned int Div2HighCycles     : 4;
    unsigned int Div2LowCycles      : 4;

    // Divider 2 (Reg 0x197, HW Default 0x80)
    unsigned int Div2PhaseOffset    : 4;
    unsigned int Div2StartHigh      : 1;
    unsigned int Div2ForceHigh      : 1;
    unsigned int Div2NoSync         : 1;
    unsigned int Div2Bypass         : 1;

    // Divider 2 (Reg 0x198, HW Default 0)
    unsigned int Div2Dccoff         : 1;
    unsigned int Div2DirectToOutput : 1;
    unsigned int _nu_00FC0000       : 6;

    unsigned int _nu_FF000000       : 8;

} PX14S_REG_CLK_LOG_14;

/// CG logical register 15 { 199, -- }
typedef struct _PX14S_REG_CLK_LOG_15_tag
{
    // Divider 3 (Reg 0x199, HW Default 0x22)
    unsigned int HighCyclesDiv3_1   : 4;
    unsigned int LowCyclesDiv3_1    : 4;

    unsigned int _nu_FFFFFF00       : 24;

} PX14S_REG_CLK_LOG_15;

/// CG logical register 16 { 19A, 19B, 19C, 19D }
typedef struct _PX14S_REG_CLK_LOG_16_tag
{
    // Divider 3 (Reg 0x19A, HW Default 0)
    unsigned int PhaseOffsetDiv3_1  : 4;
    unsigned int PhaseOffsetDiv3_2  : 4;

    // Divider 3 (Reg 0x19B, HW Default 0x11)
    unsigned int HighCyclesDiv3_2   : 4;
    unsigned int LowCyclesDiv3_2    : 4;

    // Divider 3 (Reg 0x19C, HW Default 0)
    unsigned int StartHighDiv3_1    : 1;
    unsigned int StartHighDiv3_2    : 1;
    unsigned int Div3ForceHigh      : 1;
    unsigned int Div3Nosync         : 1;
    unsigned int BypassDiv3_1       : 1;
    unsigned int BypassDiv3_2       : 1;
    unsigned int _nu_00C00000       : 2;

    // Divider 3 (Reg 0x19D, HW Defaul 0)
    unsigned int Div3Dccoff         : 1;
    unsigned int _nu_FE000000       : 7;

} PX14S_REG_CLK_LOG_16;

/// CG logical register 17 { 19E, -- }
typedef struct _PX14S_REG_CLK_LOG_17_tag
{
    // Divider 4 (Reg 0x19E, HW Default 0x22)
    unsigned int HighCyclesDiv4_1   : 4;
    unsigned int LowCyclesDiv4_1    : 4;

    unsigned int _nu_FFFFFF00       : 24;

} PX14S_REG_CLK_LOG_17;

/// CG logical register 18 { 19F, 1A0, 1A1, 1A2 }
typedef struct _PX14S_REG_CLK_LOG_18_tag
{
    // Divider 4 (Reg 0x19F, HW Default 0)
    unsigned int PhaseOffsetDiv4_1  : 4;
    unsigned int PhaseOffsetDiv4_2  : 4;

    // Divider 4 (Reg 0x1A0, HW Default 0x11)
    unsigned int HighCyclesDiv4_2   : 4;
    unsigned int LowCyclesDiv4_2    : 4;

    // Divider 4 (Reg 0x1A1, HW Default 0)
    unsigned int StartHighDiv4_1    : 1;
    unsigned int StartHighDiv4_2    : 1;
    unsigned int Div4ForceHigh      : 1;
    unsigned int Div4Nosync         : 1;
    unsigned int BypassDiv4_1       : 1;
    unsigned int BypassDiv4_2       : 1;
    unsigned int _nu_00C00000       : 2;

    // Divider 4 (Reg 0x1A2, HW Defaul 0)
    unsigned int Div4Dccoff         : 1;
    unsigned int _nu_FE000000       : 7;

} PX14S_REG_CLK_LOG_18;

/// CG logical register 19 { 1E0, 1E1, 230 }
typedef struct _PX14S_REG_CLK_LOG_19_tag
{
    // VCO Divider (Reg 0x1E0, HW Default 0x02)
    unsigned int VCO_Divider        : 3;
    unsigned int _nu_000000F8       : 5;

    // Input CLKs (Reg 0x1E1, HW Default 0)
    unsigned int BypassVcoDivider   : 1;
    unsigned int SelectVcoOrClock   : 1;
    unsigned int PowerDownVcoClock  : 1;
    unsigned int PowerDownVcoClkInt : 1;
    unsigned int PowerDownClkInSel  : 1;
    unsigned int _nu_0000E000       : 3;

    // Power Down and Sync (Reg 0x230, HW Default 0)
    unsigned int SoftSync           : 1;
    unsigned int PdDistribRef       : 1;
    unsigned int PowerDownSync      : 1;
    unsigned int _nu_00F80000       : 5;

    unsigned int _nu_FF000000       : 8;

} PX14S_REG_CLK_LOG_19;

#define PX14CLKREGMSK_1E0_VCO_DIVIDER       0x07
#define PX14CLKREGMSK_1E1_BYPASS_VCO_DIV    0x01

// -- Union wrappers around clock gen registers for register value access

typedef union _PX14U_REG_CLK_LOG_00_tag
{
    PX14S_REG_CLK_LOG_00    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_00;

typedef union _PX14U_REG_CLK_LOG_01_tag
{
    PX14S_REG_CLK_LOG_01    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_01;

typedef union _PX14U_REG_CLK_LOG_02_tag
{
    PX14S_REG_CLK_LOG_02    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_02;

typedef union _PX14U_REG_CLK_LOG_03_tag
{
    PX14S_REG_CLK_LOG_03    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_03;

typedef union _PX14U_REG_CLK_LOG_04_tag
{
    PX14S_REG_CLK_LOG_04    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_04;

typedef union _PX14U_REG_CLK_LOG_05_tag
{
    PX14S_REG_CLK_LOG_05    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_05;

typedef union _PX14U_REG_CLK_LOG_06_tag
{
    PX14S_REG_CLK_LOG_06    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_06;

typedef union _PX14U_REG_CLK_LOG_07_tag
{
    PX14S_REG_CLK_LOG_07    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_07;

typedef union _PX14U_REG_CLK_LOG_08_tag
{
    PX14S_REG_CLK_LOG_08    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_08;

typedef union _PX14U_REG_CLK_LOG_09_tag
{
    PX14S_REG_CLK_LOG_09    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_09;

typedef union _PX14U_REG_CLK_LOG_10_tag
{
    PX14S_REG_CLK_LOG_10    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_10;

typedef union _PX14U_REG_CLK_LOG_11_tag
{
    PX14S_REG_CLK_LOG_11    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_11;

typedef union _PX14U_REG_CLK_LOG_12_tag
{
    PX14S_REG_CLK_LOG_12    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_12;

typedef union _PX14U_REG_CLK_LOG_13_tag
{
    PX14S_REG_CLK_LOG_13    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_13;

typedef union _PX14U_REG_CLK_LOG_14_tag
{
    PX14S_REG_CLK_LOG_14    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_14;

typedef union _PX14U_REG_CLK_LOG_15_tag
{
    PX14S_REG_CLK_LOG_15    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_15;

typedef union _PX14U_REG_CLK_LOG_16_tag
{
    PX14S_REG_CLK_LOG_16    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_16;

typedef union _PX14U_REG_CLK_LOG_17_tag
{
    PX14S_REG_CLK_LOG_17    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_17;

typedef union _PX14U_REG_CLK_LOG_18_tag
{
    PX14S_REG_CLK_LOG_18    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_18;

typedef union _PX14U_REG_CLK_LOG_19_tag
{
    PX14S_REG_CLK_LOG_19    bits;
    unsigned char           bytes[4];
    unsigned int            val;

} PX14U_REG_CLK_LOG_19;


// -- Aggregation of all PX14 CG registers

typedef struct _PX14S_CLKGEN_REG_SET_tag
{
    PX14U_REG_CLK_LOG_00        reg00;
    PX14U_REG_CLK_LOG_01        reg01;
    PX14U_REG_CLK_LOG_02        reg02;
    PX14U_REG_CLK_LOG_03        reg03;
    PX14U_REG_CLK_LOG_04        reg04;
    PX14U_REG_CLK_LOG_05        reg05;
    PX14U_REG_CLK_LOG_06        reg06;
    PX14U_REG_CLK_LOG_07        reg07;
    PX14U_REG_CLK_LOG_08        reg08;
    PX14U_REG_CLK_LOG_09        reg09;
    PX14U_REG_CLK_LOG_10        reg10;
    PX14U_REG_CLK_LOG_11        reg11;
    PX14U_REG_CLK_LOG_12        reg12;
    PX14U_REG_CLK_LOG_13        reg13;
    PX14U_REG_CLK_LOG_14        reg14;
    PX14U_REG_CLK_LOG_15        reg15;
    PX14U_REG_CLK_LOG_16        reg16;
    PX14U_REG_CLK_LOG_17        reg17;
    PX14U_REG_CLK_LOG_18        reg18;
    PX14U_REG_CLK_LOG_19        reg19;

} PX14S_CLKGEN_REG_SET;

/// Number of logical (32-bit) clock generator registers
#define PX14_CLKGEN_LOGICAL_REG_CNT         20
/// Count of actual 8-bit clock generator registers
#define PX14_CLKGEN_MAX_REG_IDX             0x232

typedef union _PX14U_CLKGEN_REGISTER_SET_tag
{
    PX14S_CLKGEN_REG_SET        fields;
    unsigned int                values[PX14_CLKGEN_LOGICAL_REG_CNT];
    unsigned char               bytes[PX14_CLKGEN_LOGICAL_REG_CNT*4];

} PX14U_CLKGEN_REGISTER_SET;

//########################################################################//
//
// __DRIVER__
//
// -- PX14 driver (software-only) registers
//
// There are a few hardware settings that are either write-only or pull
//  double-duty, so we maintain some 'driver' registers to cache this data
//  so it's available to all PX14 clients.
//
// Note that these register values have no direct effect on PX14 hardware
//
// Each driver register is 32-bits wide
//
// Driver register overview:
//  DR0 : Clock dividers (for external clock)
//  DR1 : Internal clock frequency (lower 32)
//  DR2 : Internal clock frequency (upper 32)
//  DR3 : Assumed external clock frequency (lower 32)
//  DR4 : Assumed external clock frequency (upper 32)
//  DR5 : DC-offsets (for DC-coupled devices)
//########################################################################//

#define PX14DREGDEFAULT_0           0x40080000      // No div; PLL enabled; mid fine offset
#define PX14DREGDEFAULT_1           0x502F9000      // 400 MHz
#define PX14DREGDEFAULT_1_PX12500   0xA43B7400      // 500 MHz
#define PX14DREGDEFAULT_2           0x00000009      // 400 MHz
#define PX14DREGDEFAULT_2_PX12500   0x0000000B      // 500 MHz
#define PX14DREGDEFAULT_3           0x502F9000      // 400 MHz
#define PX14DREGDEFAULT_3_PX12500   0xA43B7400      // 500 MHz
#define PX14DREGDEFAULT_4           0x00000009      // 400 MHz
#define PX14DREGDEFAULT_4_PX12500   0x0000000B      // 500 MHz
#define PX14DREGDEFAULT_5           0x00800800      // mid-scale offsets
#define PX14_DRIVER_REG_COUNT       6

#define PX14_ACQ_CLK_DRV_SCALE      100000000

/** @brief Driver register 0 : Ext clock dividers (software register)

    This register maintains clock divider value used when the external
    clock is selected. When using the internal clock, we use the actual
    clock divider to divide down to the desired acquisition rate. We
    then use this cached clock divider value when going back to the
    external clock selection. This allows us to go back and forth between
    clock sources without having to reset the clock divider value.
*/
typedef struct _PX14S_DREG_00_tag
{
   unsigned int ext_clk_div1        : 5;  ///< 0000001F: External clock divider #1
   unsigned int ext_clk_div2        : 3;  ///< 000000E0: External clock divider #2
   unsigned int pll_disable         : 1;  ///< 00000100: Disable PLL with int clock

   // Fine DC offset adjustment only on PX14400D2 devices
   unsigned int dc_fine_offset_ch1  : 11; ///< 000FFE00: Ch 1 fine DC offset adjustment
   unsigned int dc_fine_offset_ch2  : 11; ///< 7FF00000: Ch 2 fine DC offset adjustment

   unsigned int pd_override         : 1;  ///< 80000000: Power-down override

} PX14S_DREG_00;

#define PX14DREGIDX_CLK_DIV_1       0
#define PX14DREGMSK_CLK_DIV_1       0x0000001F
#define PX14DREGIDX_CLK_DIV_2       0
#define PX14DREGMSK_CLK_DIV_2       0x000000E0
#define PX14DREGIDX_PLL_DISABLE     0
#define PX14DREGMSK_PLL_DISABLE     0x00000100
#define PX14DREGMSK_DC_FINE_OFFSET_CH1 0x000FFE00
#define PX14DREGMSK_DC_FINE_OFFSET_CH2 0x7FF00000
#define PX14DREGMSK_PD_OVERRIDE     0x80000000

/** @brief Driver register 1 : Internal clock acquisition rate (low)

    Driver registers 1 and 2 maintain a 64-bit value that caches the
    acquisition rate (in MHz) used when the internal clock is selected.
    Like the clock dividers above, we can use this cached value to restore
    acquisition rate when switching from external to internal clock.

    The actual clock frequency is the full 64-bit unsigned value divided by
    (double)PX14_ACQ_CLK_DRV_SCALE
*/
typedef struct _PX14S_DREG_01_tag
{
    unsigned int int_acq_rate_low   : 32;

} PX14S_DREG_01;

/// Driver register 2 : Internal clock acquisition rate (high)
typedef struct _PX14S_DREG_02_tag
{
    unsigned int int_acq_rate_high  : 32;

} PX14S_DREG_02;

/** @brief Driver register 3 : External clock acquisition rate (low)

    Driver registers 3 and 4 maintain a 64-bit value that caches the
    assumed acquisition rate (in MHz) used when the external clock is
    selected.

    The actual clock frequency is the full 64-bit unsigned value divided by
    (double)PX14_ACQ_CLK_DRV_SCALE
*/
typedef struct _PX14S_DREG_03_tag
{
    unsigned int ext_acq_rate_low   : 32;

} PX14S_DREG_03;

/// Driver register 4 : External clock acquisition rate (high)
typedef struct _PX14S_DREG_04_tag
{
    unsigned int ext_acq_rate_high  : 32;

} PX14S_DREG_04;

typedef struct _PX14S_DREG_05_tag
{
   unsigned int dc_offset_ch1    : 12;    // 00000FFF
   unsigned int dc_offset_ch2    : 12;    // 00FFF000
   unsigned int _nu_FF000000     : 8;     // FF000000

} PX14S_DREG_05;

#define PX14DREGMSK_05_DC_OFFSET_CH1      0x00000FFF
#define PX14DREGMSK_05_DC_OFFSET_CH2      0x00FFF000

// -- Union wrappers around driver registers for register value access

typedef union _PX14U_DREG_00_tag
{
    PX14S_DREG_00       bits;
    unsigned int        val;

} PX14U_DREG_00;

typedef union _PX14U_DREG_01_tag
{
    PX14S_DREG_01       bits;
    unsigned int        val;

} PX14U_DREG_01;

typedef union _PX14U_DREG_02_tag
{
    PX14S_DREG_02       bits;
    unsigned int        val;

} PX14U_DREG_02;

typedef union _PX14U_DREG_03_tag
{
    PX14S_DREG_03       bits;
    unsigned int        val;

} PX14U_DREG_03;

typedef union _PX14U_DREG_04_tag
{
    PX14S_DREG_04       bits;
    unsigned int        val;

} PX14U_DREG_04;

typedef union _PX14U_DREG_05_tag
{
    PX14S_DREG_05       bits;
    unsigned int        val;

} PX14U_DREG_05;

// -- Aggregation of all PX14 driver registers

typedef struct _PX14S_DRIVER_REG_SET_tag
{
    PX14U_DREG_00   dreg0;      ///< External clock dividers
    PX14U_DREG_01   dreg1;      ///< Internal clock freq (low)
    PX14U_DREG_02   dreg2;      ///< Internal clock freq (high)
    PX14U_DREG_03   dreg3;      ///< External clock freq (low)
    PX14U_DREG_04   dreg4;      ///< External clock freq (high)
   PX14U_DREG_05   dreg5;      ///< DC-offsets

} PX14S_DRIVER_REG_SET;

typedef union _PX14U_DRIVER_REGISTER_SET_tag
{
    unsigned int            values[PX14_DRIVER_REG_COUNT];
    PX14S_DRIVER_REG_SET    fields;

} PX14U_DRIVER_REGISTER_SET;

//########################################################################//
//
// Various context structures used to communicate with underlying driver
//
// Rules:
//
// 1. All structures should be naturually aligned. (The compiler should not
//    have to add any padding. We've added compile time assertions to verify
//    this.)
//
// 2. Notation:
//    IN  = Initialized by user-space
//    OUT = Initialized by kernel-space
//
//########################################################################//

#define _PX14SO_PX14S_DEVICE_ID_V1      24
typedef struct _PX14S_DEVICE_ID_tag
{
    unsigned int    struct_size;        ///< IN: Structure size

    unsigned int    serial_num;         ///< OUT: Device serial number
    unsigned int    ord_num;            ///< OUT: Ordinal number of device
    unsigned int    feature_flags;      ///< OUT: RESERVED
    unsigned int    chan_imps;          ///< OUT: RESERVED
    unsigned int    dummy;

} PX14S_DEVICE_ID;

#define _PX14SO_PX14S_DRIVER_STATS_V1   40
#ifndef PX14S_DRIVER_STATS_STRUCT_DEFINED
#define PX14S_DRIVER_STATS_STRUCT_DEFINED
/// Used by the IOCTL_PMP1K_DRIVER_STATS device IO control
typedef struct _PX14_DRIVER_STATS_tag
{
    unsigned int    struct_size;        ///< IN: Structure size
    int             nConnections;       ///< Active connection count
    unsigned int    isr_cnt;            ///< Total ISR executions
    unsigned int    dma_finished_cnt;   ///< DMA xfers completed
    unsigned int    dma_started_cnt;    ///< DMA xfers started
    unsigned int    acq_started_cnt;    ///< Acqs started
    unsigned int    acq_finished_cnt;   ///< Acqs completed
    unsigned int    dcm_reset_cnt;      ///< DCM reset operation count
    unsigned long long  dma_bytes;      ///< Total bytes DMA'd

} PX14S_DRIVER_STATS;
#endif

#define _PX14SO_PX14S_JTAGIO_V1         20
/// Used by the IOCTL_PX14_JTAG_IO device IO control
typedef struct _PX14_JTAGIO_tag
{
    unsigned int        struct_size;    ///< IN: Structure size
    unsigned int        valW;           ///< IN:  Write value
    unsigned int        maskW;          ///< IN:  Write value mask
    unsigned int        valR;           ///< OUT: Read value
    unsigned int        flags;          ///< IN:  PX14JIOF_*

} PX14S_JTAGIO;

#define _PX14SO_PX14S_JTAG_STREAM_V1    12
#ifdef PX14PP_WANT_PX14S_JTAG_STREAM
/// Used by the IOCTL_PX14_JTAG_STREAM device IO control
typedef struct _PX14_JTAG_STREAM_tag
{
    unsigned int        struct_size;    ///< IN: Structure size
    unsigned int        flags;          ///< IN: PX14JSS_*
    unsigned int        nBits;          ///< IN: Bits to write

    unsigned char       dwData[];       ///< IN: TDI data, OUT: TDO data

} PX14S_JTAG_STREAM;
#endif

#define _PX14SO_PX14_DRIVER_VER_V1      16
/// Used by the IOCTL_PX14_DRIVER_VERSION device IO control
typedef struct _PX14S_DRIVER_VER_tag
{
    unsigned int        struct_size;    ///< IN: Structure size
    unsigned int        dummy;          ///< Dummy
    unsigned long long  version;        ///< OUT: Driver version

} PX14S_DRIVER_VER;

#define _PX14SO_DMA_BUFFER_ALLOC_V1     16
/// Used by the IOCTL_PX14_DMA_BUFFER_ALLOC device IO control
typedef struct _PX14S_DMA_BUFFER_ALLOC_tag
{
    unsigned int        struct_size;    ///< IN: Structure size
    unsigned int        req_bytes;      ///< IN: Requested buffer size in bytes
    unsigned long long  virt_addr;      ///< OUT: Virtual address of DMA buffer

} PX14S_DMA_BUFFER_ALLOC;

#define _PX14SO_DMA_BUFFER_FREE_V1      16
/// Used by the IOCTL_PX14_DMA_BUFFER_FREE device IO control
typedef struct _PX14S_DMA_BUFFER_FREE_tag
{
    unsigned int        struct_size;    ///< IN: Structure size
    unsigned int        free_all;       ///< IN: Used to free ALL DMA bufs
    unsigned long long  virt_addr;      ///< IN: Virt address of DMA buffer

} PX14S_DMA_BUFFER_FREE;

#define PX14_FREE_ALL_DMA_BUFFERS           0xFEEA77BF

#define _PX14SO_EEPROM_IO_V1            16
/// Used by the IOCTL_PX14_EEPROM_IO device IO control
typedef struct _PX14S_EEPROM_IO_tag
{
    unsigned int        struct_size;    ///< IN: Structure size
    int                 bRead;          ///< IN: Reading EEPROM?
    unsigned int        eeprom_addr;    ///< IN: EEPROM address to access
    unsigned int        eeprom_val;     ///< IN/OUT: EEPROM data

} PX14S_EEPROM_IO;

/// Maximum number of boot buffers supported
#define PX14_BOOTBUF_COUNT                4

// -- PX14400 Boot Buffer Operations (PX14BBOP_*)
/// Check out buffer with given index
#define PX14BBOP_CHECK_OUT                0
/// Check in buffer with given index
#define PX14BBOP_CHECK_IN                 1
/// Query boot buffer status; obtains status of allocated buffer(s)
#define PX14BBOP_QUERY                    2
/// Force reallocation of boot-buffers now; no change if buffer >= size unless new size is 0
#define PX14BBOP_REALLOC                  3
#define PX14BBOP__COUNT                   4

#define _PX14SO_PX14S_BOOTBUF_CTRL_V1     24
/// Used by the IOCTL_PX14_BOOTBUF_CTRL device IO control
typedef struct _PX14S_BOOTBUF_CTRL_tag
{
   unsigned int         struct_size;   ///< IN: Structure size
   unsigned short       operation;     ///< IN: PX14BBOP_*
   unsigned short       buf_idx;       ///< IN: index of buffer
   unsigned int         buf_samples;   ///< IN/OUT: Buffer size in samples
   unsigned int         reserved;      ///< Reserved for future use, pass 0
   unsigned long long   virt_addr;     ///< IN/OUT: User spave virtual address of buffer

} PX14S_BOOTBUF_CTRL;

#define _PX14SO_DBG_REPORT_V1           8
/// Used by the IOCTL_PX14_DBG_REPORT device IO control
typedef struct _PX14S_DBG_REPORT_tag
{
    unsigned int        struct_size;    ///< IN: Structure size
    int                 report_type;    ///< IN: PX14DBGRPT_*

} PX14S_DBG_REPORT;

#define _PX14SO_DEV_REG_WRITE_V1        40
/// Used by the IOCTL_PX14_DEVICE_REG_WRITE device IO control
typedef struct _PX14S_DEV_REG_WRITE_tag
{
    unsigned int        struct_size;    ///< IN: Structure size
    unsigned int        reg_set;        ///< IN: Reister set (PX14REGSET_*)
    unsigned int        reg_idx;        ///< IN: Device register index
    unsigned int        reg_mask;       ///< IN: Mask for write
    unsigned int        reg_val;        ///< IN: Register value
    unsigned int        pre_delay_us;   ///< IN: Pre-write delay in us
    unsigned int        post_delay_us;  ///< IN: Post-write delay in us
    // For clock register writes (reg_set == PX14REGSET_CLKGEN*)
    unsigned int        cg_log_reg;     ///< IN: Logical register index
    unsigned int        cg_byte_idx;    ///< IN: Byte index in logical reg
    unsigned int        cg_addr;        ///< IN: Clock gen reg addr
    // If MSB of cg_addr is set then an 'Update Registers' command will be
    //  sent to the clock part

} PX14S_DEV_REG_WRITE;

#define PX14CLKREGWRITE_UPDATE_REGS         0x80000000

#define _PX14SO_DEV_REG_READ_V1         40
typedef struct _PX14S_DEV_REG_READ_tag
{
    unsigned int        struct_size;    ///< IN: Structure size
    unsigned int        reg_set;        ///< IN: Reister set (PX14REGSET_*)
    unsigned int        reg_idx;        ///< IN: Device register index
    unsigned int        pre_delay_us;   ///< IN: Pre-write delay in us
    unsigned int        post_delay_us;  ///< IN: Post-write delay in us
    unsigned int        read_how;       ///< IN: PX14REGREAD_*
    // For clock register writes (reg_set == PX14REGSET_CLKGEN*)
    unsigned int        cg_log_reg;     ///< IN: Logical register idx
    unsigned int        cg_byte_idx;    ///< IN: Byte index in logical reg
    unsigned int        cg_addr;        ///< IN: Clock gen reg addr (16-bits)
    int                 dummy;

    // If cg_addr == 0xFFFFFFFF then the driver will return value of the
    //  32-bit logical register (from register cache, not hardware)

} PX14S_DEV_REG_READ;

#define PX14CLKREGREAD_LOGICAL_REG_ONLY     0xFFFFFFFF

#define _PX14SO_DMA_XFER_V1             24
/// Used by the IOCTL_PX14_DMA_XFER device IO control
typedef struct _PX14S_DMA_XFER_tag
{
    unsigned int        struct_size;    ///< IN: Structure size
    unsigned int        xfer_bytes;     ///< IN: Transfer size in bytes
    unsigned long long  virt_addr;      ///< IN: Virtual address
    int                 bRead;          ///< IN: PX14 -> PC ?
    int                 bAsynch;        ///< IN: Asynchronous xfer?

} PX14S_DMA_XFER;

#define _PX14SO_WAIT_OP_V1              16
/// Used by the IOCTL_PX14_WAIT_ACQ_OR_XFER device IO control
typedef struct _PX14S_WAIT_OP
{
    unsigned int        struct_size;    ///< IN: Structure size
    int                 wait_op;        ///< IN: PX14STATE_*
    unsigned int        timeout_ms;     ///< IN: Timeout (ms)
    int                 dummy;

} PX14S_WAIT_OP;

/// Context structure used for raw, uncached register IO
typedef struct _PX14S_RAW_REG_IO_tag
{
    int                 bRead;

    int                 nRegion;
    int                 nRegister;
    unsigned int        regVal;

} PX14S_RAW_REG_IO;

// -- Driver buffered transfer flags (PX14DBTF_*)
/// Mask for operating mode bits
#define PX14DBTF__MODE_MASK                 0x0000FFFF
/// The driver should deinterleave data
#define PX14DBTF_DEINTERLEAVE               0x80000000
/// The driver should interleave data
#define PX14DBTF_INTERLEAVE                 PX14DBTF_DEINTERLEAVE
/// Transfer is asynchronous; returns without waiting for xfer to complete
#define PX14DBTF_ASYNCHRONOUS               0x40000000
/// Region reserved for device driver; flags in this region for driver only
#define PX14DBTF__DRIVER_RESERVED           0x00FF0000
/// Doing a device write operation; host -> device
#define PX14DBTF_WRITING_TO_DEVICE          0x00100000
/// Restore Standby mode after operation
#define PX14DBTF_RESTORE_STANDBY_MODE       0x00200000

#define _PX14SO_DRIVER_BUFFERED_XFER_V1 32
/** @brief Used by IOCTL_PX14_DRIVER_BUFFERED_XFER

    This structure is used to specify context parameters to the driver when
    requesting a driver-mapped transfer. A driver-mapped transfer is a
    transfer to/from the board in which the driver's internal DMA buffer is
    used buffer the transfer data to/from user-space buffers.

    @param struct_size
        Size of the structure in bytes. User should init with sizeof(*this)
    @param control
        This parameter specifies two things. <br><br>
        The lower word indicates the operating mode to use for the operation
        or -1 if the driver should not alter operating mode. If an actual
        mode is specified, the driver will automatically put the board back
        into standby mode when the operation has completed. If using -1 for
        operating mode the caller is responsible for setting active memory
        region and operating mode. In this case, the operating mode should
        be set prior to driver request so the driver can validate operation
        for current operating mode. <br><br>
        The upper word is a set of PX14DBTF_* flags that further define
        request parameters. Note these flags are 32-bit and can be applied
        to control variable itself
    @param sample_start
        Starting PX14 RAM sample. This can be any valid sample, there are
        no alignment requirements. If the PX14DBTF_DEINTERLEAVE flag is
        specified then the actual RAM sample index will be 2*sample_start
        since we're assuming dual channel data.
    @param sample_count
        Total number of samples to transfer. This value is independent of
        channel count so if the PX14DBTF_DEINTERLEAVE is specified then the
        driver will transfer sample_count/2 samples for each channel.
    @param user_virt_addr
        The user-space virtual address of the buffer for the src/dst data.
        If the PX14DBTF_DEINTERLEAVE flag is set, this parameter may be NULL
        if channel 1 data is not needed/provided. (In this case, for
        transfers to the board, constant 0 data will be used.)
    @param user_virt_addr2
        The user-space virtual address of the buffer for the channel 2
        src/dst data. This parameter is only considered when the
        PX14DBTF_DEINTERLEAVE flag has been specified. This parameter may be
        NULL if channel 2 data is not needed/provided. (In this case, for
        transfers to the board, constant 0 data will be used.)
*/
typedef struct _PX14S_DRIVER_BUFFERED_XFER_tag
{
    unsigned int        struct_size;    ///< IN: Structure size
    unsigned int        flags;          ///< IN: PX14DBTF_*
    unsigned int        sample_start;   ///< IN: Desired starting sample
    unsigned int        sample_count;   ///< IN: Number of samples to read

    unsigned long long  user_virt_addr; ///< IN: User-space virt addr
    unsigned long long  user_virt_addr2;///< IN: User-space virt addr (ch2)

} PX14S_DRIVER_BUFFERED_XFER;

#define _PX14SO_PX14S_READ_TIMESTAMPS_V1    24

/// Used by IOCTL_PX14_READ_TIMESTAMPS
typedef struct _PX14S_READ_TIMESTAMPS_tag
{
    unsigned int        struct_size;    ///< IN: Structure size
    unsigned int        buffer_items;   ///< IN/OUT: TS in/out
    unsigned int        flags;          ///< IN: PX14TSREAD_*
    unsigned int        timeout_ms;     ///< IN: Timeout in ms
    unsigned long long  user_virt_addr; ///< OUT: Buffer for timestamps

} PX14S_READ_TIMESTAMPS;

#define _PX14SO_PX14S_FW_VERSIONS_V1    24

/// Used by IOCTL_PX14_FW_VERSIONS
typedef struct _PX14S_FW_VERSIONS_tag
{
    unsigned int        struct_size;    ///< IN: Structure size
    unsigned int        fw_ver_pci;     ///< OUT: PCI firmware version
    unsigned int        fw_ver_sab;     ///< OUT: SAB firmware version
    unsigned int        fw_ver_pkg;     ///< OUT: Firmware package version
    unsigned int        pre_rel_flags;  ///< OUT: PX14PRERELEASE_*
    unsigned int        reserved;       ///< IN/OUT: Zero

} PX14S_FW_VERSIONS;

#define _PX14SO_PX14S_HW_CONFIG_EX_V1       16
#define _PX14SO_PX14S_HW_CONFIG_EX_V2       24

/// Used by IOCTL_PX14_GET_HW_CONFIG_EX
typedef struct _PX14S_HW_CONFIG_EX_tag
{
    unsigned int        struct_size;    ///< IN: Strucutre size
    unsigned short      chan_imps;      ///< OUT: PX14CHANIMPF_*
    unsigned short      chan_filters;   ///< OUT: PX14CHANFILTER_*
    unsigned short      sys_fpga_type;  ///< OUT: PX14SYSFPGA_*
    unsigned short      sab_fpga_type;  ///< OUT: PX14SABFPGA_*
    unsigned short      board_rev;      ///< OUT: PX14BRDREV_*
    unsigned short      board_rev_sub;  ///< OUT:

   // -- Version 2

   unsigned short      custFwPkg;
   unsigned short      custFwPci;
   unsigned short      custFwSab;
   unsigned short      custHw;

} PX14S_HW_CONFIG_EX;

//########################################################################//
//
// __EEPROM__
//
//  PX14 configuration EEPROM map
//
//  0x00 - 0x4D -> Reserved
//
//  0x46        -> Requested boot buffer 0 size in samples (lower)
//  0x47        -> Requested boot buffer 0 size in samples (upper)
//  0x48        -> Requested boot buffer 1 size in samples (lower)
//  0x49        -> Requested boot buffer 1 size in samples (upper)
//  0x4A        -> Requested boot buffer 2 size in samples (lower)
//  0x4B        -> Requested boot buffer 2 size in samples (upper)
//  0x4C        -> Requested boot buffer 3 size in samples (lower)
//  0x4D        -> Requested boot buffer 3 size in samples (upper)
//  0x4E        -> Slave234 selection           (2=Slave2,3=Slave3,...)
//  0x4F        -> Board sub-revision           (PX14BRDREVSUB_*)
//  0x5A        -> Firmware info flags          (PX14FWIF_*)
//  0x5B        -> Channel filters              (PX14CHANFILTER_*; ch2|ch1)
//  0x5C        -> System FPGA type             (PX14SYSFPGA_*)
//  0x5D        -> SAB FPGA type                (PX14SABFPGA_*)
//  0x5E        -> Firmware package custom enum (0=Non-custom)
//  0x5F        -> Firmware package version (high)
//  0x6A        -> Firmware package version (low)
//  0x6B        -> Custom SAB logic Number      (0=Non-custom)
//  0x6C        -> Previous SAB logic sub-ver   (Hi8:sub-min, Low8:package)
//  0x6D        -> Previous SAB logic version   (Hi:8 maj, Low8:8 min)
//  0x6E        -> SAB logic sub-version number (Hi8:sub-min, Low8:package)
//  0x6F        -> SAB logic version number     (Hi8:maj, Low8:min)
//  0x70        -> Features present             (PX14FEATURE_*)
//  0x71        -> Channel implementation flags (PX14CHANIMPF_*; ch2|ch1)
//  0x72        -> Board serial number (low)
//  0x73        -> Board serial number (high)
//  0x74        -> Board revision               (PX14BRDREV_*)
//  0x75        -> Previous PCI logic sub-ver   (Hi8:sub-min, Low8:package)
//  0x76        -> Previous PCI logic version   (Hi:8 maj, Low8:8 min)
//  0x77        -> Pre-release settings         (0x1:Firmware, 0x2:Hardware)
//  0x78        -> Custom Hardware Number       (0=Non-custom)
//  0x79        -> Custom PCI logic Number      (0=Non-custom)
//  0x7A        -> Board Type Number            (PX14400 == 0x31)
//  0x7B        -> Hardware revision number     (Hi8:maj, Low8:min)
//  0x7C        -> PCI logic sub-version number (Hi8:sub-min, Low8:package)
//  0x7D        -> PCI logic version number     (Hi8:maj, Low8:min)
//  0x7E        -> Magic number                 (0x0B14)
//  0x7F        -> Checksum                     (Range: [0x00,0x7E])
//  0x80 - 0xFF -> User defined
//########################################################################//

/// Size of PX14 EEPROM in unsigned shorts
/// 4k bits -> 512 bytes -> 256 unsigned short addresses
#define PX14_EEPROM_WORDS                   256
/// Start of user-defined EEPROM range
#define PX14EA_USER_ADDR_MIN                0x80
/// End of user-defined EEPROM range
#define PX14EA_USER_ADDR_MAX                0xFF

/// Base EEPROM address for requested boot buffer sizes
#define PX14EA_BOOTBUF_BASE                 0x46
/// Address on EEPROM of Slave234 selection (See used in master/slave sel)
#define PX14EA_BOARD_SLAVE234               0x4E
#define PX14EA_BOARD_SLAVESEL               PX14EA_BOARD_SLAVE234
/// Address on EEPROM of board revision (PX14BRDREVSUB_*)
#define PX14EA_BOARD_REV_SUB                0x4F
/// EEPROM address of firmware info flags (PX14FWIF_*)
#define PX14EA_FWINFO                       0x5A
/// Address on EEPROM of channel filter types (PX14CHANFILTER_*)
#define PX14EA_CHANFILTERTYPES              0x5B
/// Address on EEPROM of system FPGA type (PX14SYSFPGA_*)
#define PX14EA_SYS_FPGA_TYPE                0x5C
/// Address on EEPROM of SAB FPGA type (PX14SABFPGA_*)
#define PX14EA_SAB_FPGA_TYPE                0x5D
/// EEPROM address of firmware package custom enumeration
#define PX14EA_CUST_FWPKG_ENUM              0x5E
/// Address on EEPROM of main firmware package version (upper)
#define PX14EA_FWPKG_VER_HIGH               0x5F
/// Address on EEPROM of main firmware package version (lower)
#define PX14EA_FWPKG_VER_LOW                0x6A
/// Address on EEPROM of custom SAB firmware enumeration
#define PX14EA_CUSTOM_SAB_LOGIC_ENUM        0x6B
/// Address on EEPROM of previous SAB firmware sub-logic version
#define PX14EA_PREV_SAB_LOGIC_SUB_VER       0x6C
/// Address on EEPROM of previous SAB firmware version
#define PX14EA_PREV_SAB_LOGIC_VER           0x6D
/// Address on EEPROM of SAB firmware sub-logic version
#define PX14EA_SAB_LOGIC_SUB_VER            0x6E
/// Address on EEPROM of current SAB firmware version
#define PX14EA_SAB_LOGIC_VER                0x6F
/// Address on EEPROM of implemented feature flags (PX14FEATURE_*)
#define PX14EA_FEATURES                     0x70
/// Address on EEPROM of channel implementations (PX14CHANIMP_*)
#define PX14EA_CHANNEL_IMP                  0x71
/// Address on EEPROM of lower 16 bits of board serial number
#define PX14EA_SERIALNUM_LOWER              0x72
/// Address on EEPROM of upper 16 bits of board serial number
#define PX14EA_SERIALNUM_UPPER              0x73
/// Address on EEPROM of board revision (PX14BRDREV_*)
#define PX14EA_BOARD_REV                    0x74
/// Address on EEPROM of previous firmware sub-logic version
#define PX14EA_PREV_LOGIC_SUB_VER           0x75
/// Address on EEPROM of previous firmware version
#define PX14EA_PREV_LOGIC_VER               0x76
/// Address of EEPROM of pre-release settings
#define PX14EA_PRE_RELEASE                  0x77
/// Address on EEPROM of custom hardware enumeration
#define PX14EA_CUSTOM_HW_ENUM               0x78
/// Address on EEPROM of custom firmware enumeration
#define PX14EA_CUSTOM_LOGIC_ENUM            0x79
/// Address on EEPROM of Signatec defined board type
#define PX14EA_BOARD_TYPE                   0x7A
/// Address on EEPROM of current hardware revision
#define PX14EA_HW_REV                       0x7B
/// Address on EEPROM of firmware sub-logic version
#define PX14EA_LOGIC_SUB_VER                0x7C
/// Address on EEPROM of current firmware version
#define PX14EA_LOGIC_VER                    0x7D
/// Address on EEPROM of a magic number
#define PX14EA_MAGIC_NUM                    0x7E
/// Address on EEPROM of EEPROM's checksum
#define PX14EA_CHECKSUM                     0x7F

/// The PX14 EEPROM magic number
#define PX14_EEPROM_MAGIC_NUMBER            0x0B14

/// Bit-mask for pre-release system firmware
#define PX14PRERELEASE_SYS_FW               0x0001
/// Bit-mask for pre-release SAB firmware
#define PX14PRERELEASE_SAB_FW               0x0004
/// Bit-mask for pre-release hardware
#define PX14PRERELEASE_HW                   0x0002

// -- PX14 custom hardware enumerations
/// Not custom; stock hardware
#define PX14CUSTHW_NOT                      0
/// One-shot custom
#define PX14CUSTHW_ONESHOT                  0xFFFF

/// System logic EEPROM #2 (xcf16p) is blank
#define PX14FWIF_PCIE_EEPROM_2_BLANK        0x0001
/// SAB logic EEPROM #2 (xcf16) is blank
#define PX14FWIF_SAB_EEPROM_2_BLANK         0x0002

//########################################################################//
//
// PX14 device state; the meat behind an PX14 device handle (HPX14)
//
//########################################################################//

#ifndef PX14PP_NO_CLASS_DEFS

class CVirtualCtxPX14;
class CRemoteCtxPX14;

// Signature of a PX14 service provider funtion
typedef int (* px14lib_svc_provider_t) (HPX14 hBrd,
                                        io_req_t req,
                                        void* ctxp,
                                        size_t in_bytes,
                                        size_t out_bytes);

/** @brief PX14 device state

    @note
        A HPX14 object is actually a pointer to an instance of this class.
*/
class CStatePX14
{
public:

   // -- Construction

   CStatePX14(dev_handle_t h, unsigned sn, unsigned on);

   // -- Implementation

   virtual ~CStatePX14();

   int DuplicateFrom (const CStatePX14& cpy);

   bool IsVirtual() const
   { return 0 != (m_flags & PX14LIBF_VIRTUAL); }
   bool IsLocalVirtual() const
   { return IsVirtual() && !IsRemote(); }
   bool IsRemote() const
   { return 0 != (m_flags & PX14LIBF_REMOTE_CONNECTED); }

   bool IsDriverVerLessThan (unsigned maj, unsigned min,
                             unsigned submin = 0, unsigned package = 0) const;
   bool IsDriverVerLessThan (const unsigned long long& ver) const;
   bool IsDriverVerGreaterThan (unsigned maj, unsigned min,
                                unsigned submin = 0, unsigned package = 0) const;
   bool IsDriverVerGreaterThan (const unsigned long long& ver) const;

   bool IsPX12500() const
   { return m_boardRev == PX14BRDREV_PX12500; }
   bool IsPX14400D2() const
   { return m_boardRev == PX14BRDREV_PX14400D2; }

   // Get assumed external clock rate from driver register cache
   double GetExtRateMHz() const;
   // Set assumed external clock rate in driver register cache
   void SetExtRateMHz (double dRateMHz);

   // Get internal clock rate from driver register cache
   double GetIntAcqRateMHz() const;
   // Set internal clock rate in driver register cache
   void SetIntAcqRateMHz (double dRateMHz);

   /// Returns true if given channel is amplified
   bool IsChanAmplified (int chanIdx) const;

   // -- Public member data

   typedef px14lib_svc_provider_t _SvcProc;
   typedef unsigned long long my_ull_t;

   unsigned int    m_magic;            ///< Must equal PX14_CTX_MAGIC

   dev_handle_t    m_hDev;             ///< Handle to PX14 device driver
   unsigned int    m_flags;            ///< PX14LIBF_*
   unsigned int    m_serial_num;       ///< Serial number
   unsigned int    m_ord_num;          ///< Ordinal # relative to system
   int             m_pll_defer_cnt;    ///< PLL lock check deferral reference count
   void*           m_userData;         ///< User-defined data value

   unsigned int    m_features;         ///< PX14FEATURE_*
   unsigned char   m_chanImpCh1;       ///< PX14CHANIMPF_*
   unsigned char   m_chanImpCh2;       ///< PX14CHANIMPF_*
   unsigned char   m_chanFiltCh1;      ///< PX14CHANFILTER_*
   unsigned char   m_chanFiltCh2;      ///< PX14CHANFILTER_*
   unsigned short  m_fpgaTypeSys;      ///< PX14SYSFPGA_*
   unsigned short  m_fpgaTypeSab;      ///< PX14SABFPGA_*
   unsigned short  m_boardRev;         ///< PX14BRDREV_*
   unsigned short  m_boardRevSub;      ///< PX14BRDREVSUB_*
   unsigned short  m_custFwPkg;        ///< Firmware package custom enum
   unsigned short  m_custFwPci;        ///< PCIe firmware custom enum
   unsigned short  m_custFwSab;        ///< SAB firmware custom enum
   unsigned short  m_custHw;           ///< Custom hardware enum
   double          m_slave_delay_ns;   ///< Slave clock delay in ns

   my_ull_t        m_driver_ver;       ///< Driver version    (VER64)
   my_ull_t        m_hw_revision;      ///< Hardware revision (VER64)
   unsigned int    m_fw_ver_pci;       ///< PCI firmware version
   unsigned int    m_fw_ver_sab;       ///< SAB firmware version
   unsigned int    m_fw_ver_pkg;       ///< Firmware package version
   unsigned int    m_fw_pre_rel;       ///< PX14PRERELEASE_*

   std::string     m_err_extra;        ///< Extra error information

   _SvcProc        m_pfnSvcProc;       ///< Service provider

   CVirtualCtxPX14* m_virtual_statep;  ///< Virtual PX14 state
   CRemoteCtxPX14*  m_client_statep;   ///< Remote PX14400 state

   // - Utility DMA buffers - freed on handle close
   px14_sample_t*  m_utility_bufp;
   unsigned int    m_utility_buf_samples;
   px14_sample_t*  m_utility_buf2p;
   unsigned int    m_utility_buf2_samples;
   PX14S_BUFNODE*  m_utility_buf_chainp;

   // - Local register cache
   PX14U_DEVICE_REGISTER_SET   m_regDev;       ///< Cached device registers
   PX14U_CLKGEN_REGISTER_SET   m_regClkGen;    ///< Cached clock gen regs
   PX14U_DRIVER_REGISTER_SET   m_regDriver;    ///< Driver (sw-only) regs
};

/// Encapsulation of client-side state for remote PX14400 implementation
class CRemoteCtxPX14
{
public:

    // -- Construction

    CRemoteCtxPX14() : m_sock(PX14_INVALID_SOCKET), m_recv_buf_bytes(0),
        m_srvPort(0)
    {}

    // -- Implementation

    void AllocateChunkBuffer()
    {
        m_recv_bufp = std::auto_ptr<char>(new char[s_recv_buf_bytes]);
        m_recv_buf_bytes = s_recv_buf_bytes;
    }

    virtual ~CRemoteCtxPX14()
    {
        if (PX14_INVALID_SOCKET != m_sock)
            SysCloseSocket(m_sock);
    }

    // Public data

    // Size of m_recv_bufp if allocated
    static const int s_recv_buf_bytes = 4096;

    px14_socket_t       m_sock;         ///< Socket to remote PX14400 server

    std::auto_ptr<char> m_recv_bufp;    ///< Response bucket buffer
    int                 m_recv_buf_bytes;

    std::string         m_strSrvErr;    ///< Server generated error text

    std::string         m_strAppName;   ///< Client application name
    std::string         m_strSrvAddr;   ///< Server address
    std::string         m_strSubSvcs;   ///< Subservices
    unsigned short      m_srvPort;      ///< Server's port address
};

/// Encapsulation of state for local, virtual PX14 devices
class CVirtualCtxPX14
{
public:

    // -- Construction

    CVirtualCtxPX14() : m_devState(PX14STATE_IDLE), m_bNeedDcmRst(false),
        m_startAddr(0)
    {
        memset (&m_drvStats, 0, sizeof(PX14S_DRIVER_STATS));
        m_drvStats.struct_size = sizeof(PX14S_DRIVER_STATS);
        m_drvStats.nConnections = 1;    // This one
    }

    // -- Implementation

    virtual ~CVirtualCtxPX14() {}

    // -- Public members

    PX14S_DRIVER_STATS      m_drvStats; ///< Simulated driver stats
    int                     m_devState; ///< PX14STATE_*
    bool                    m_bNeedDcmRst;
    unsigned int            m_startAddr;
};

// Ensures that given PX14 handle is valid and obtain handle state
int ValidateHandle (HPX14 hBrd, CStatePX14** brdpp = NULL);

// Send a request to the underlying PX14 device (driver or remote)
int DeviceRequest (HPX14 hBrd, io_req_t req,
                   void* p = NULL,
                   size_t in_bytes = 0,
                   size_t out_bytes = 0);

// Send a request to the PX14 driver
int DriverRequest (HPX14 hBrd, io_req_t req,
                   void* p = NULL,
                   size_t in_bytes = 0,
                   size_t out_bytes = 0);

/// Obtain and cache extended hardware configuration information
int CacheHwCfgEx (HPX14 hBrd);

#endif // PX14PP_NO_CLASS_DEFS not defined

/// Convert a CStatePX14 object address to a PX14 handle
#define PX14_B2H(p)     reinterpret_cast<HPX14>(p)
/// Convert a PX14 handle to a CStatePX14 object address
#define PX14_H2B(h)     reinterpret_cast<CStatePX14*>(h)
/// Convert a PX14 handle to a CStatePX14 reference. (Careful!)
#define PX14_H2Bref(h)  (*PX14_H2B(h))

/// Returns nonzero if given register index is of a status/volatile register
#define PX14_IS_STATUS_REG(n)   (((n)>=0xC) && ((n)<= 0xF))
/// Returns nonzero if given register index is of a special register
#define PX14_IS_SPECIAL_REG(n)  (0)     // No special registers yet
/// Returns nonzero if given register index if of an SAB register
#define PX14_IS_SAB_REG(n)      (((n)>=9)&&((n)<=11))

/// Returns nonzero if given mode is any PCI mode
#define PX14_IS_PCI_MODE(m)                 \
    (((m) == PX14MODE_ACQ_PCI_BUF) || ((m) == PX14MODE_RAM_READ_PCI) || \
     ((m) == PX14MODE_RAM_WRITE_PCI) || ((m) == PX14MODE_ACQ_PCI_SMALL_FIFO))
/// Returns nonzero if given mode is any acquisition mode
#define PX14_IS_ACQ_MODE(m)                 \
    (((m) >= PX14MODE_ACQ_RAM) && ((m) <= PX14MODE_ACQ_SAB_BUF))
/// Returns nonzero if given mode is any transfer mode
#define PX14_IS_XFER_MODE(m)                \
    (((m) >= PX14MODE_RAM_READ_PCI) && ((m) <= PX14MODE_RAM_WRITE_PCI))
/// Returns nonzero if given mode is idle (standby or off)
#define PX14_IS_IDLE_MODE(m)                \
    (((m) == PX14MODE_STANDBY) || ((m) == PX14MODE_OFF))
/// Returns nonzero if given mode is any SAB mode
#define PX14_IS_SAB_MODE(m)                 \
    (((m) == PX14MODE_ACQ_SAB)     ||       \
     ((m) == PX14MODE_ACQ_SAB_BUF) || ((m) == PX14MODE_RAM_READ_SAB))
/// Returns nonzero if given mode will generate an SCC interrupt
#define PX14_GENERATES_SCC_INTERRUPT(m)     \
    (((m)==PX14MODE_ACQ_RAM) ||             \
     ((m)==PX14MODE_ACQ_SAB) ||             \
     ((m)==PX14MODE_ACQ_SAB_BUF) ||         \
     ((m)==PX14MODE_RAM_READ_SAB))
/// Returns nonzero if given mode is Standby mode
#define PX14_IS_STANDBY(m)                ((m) == PX14MODE_STANDBY)

/// Fast alignment check; assumes req is a power of two
#define PX14_IS_ALIGNED_FAST(val,req)     (0 == ((val) & ((req) - 1)))
/// Safe alignment check; assumes req is non-zero
#define PX14_IS_ALIGNED_SAFE(val,req)     (0 == ((val) % (req)))

/// Expands to val aligned up to req; assumes req is a power-of-two
#define PX14_ALIGN_UP_FAST(val,req)       ((val+(req-1))&~(req-1))
/// Expands to val aligned up to req; assumes req !0 (may be a faster imp for this)
#define PX14_ALIGN_UP_SAFE(val,req)       ((val+(req-1))-((val+(req-1))%(req)))

/// Expands to val aligned down to req; assumes req is a power-of-two
#define PX14_ALIGN_DOWN_FAST(val,req)     ((val) & ~(req-1))
/// Expands to val aligned down to req;
#define PX14_ALIGN_DOWN_SAFE(val,req)     ((val) - ((val) % (req)))

/// Check input pointer; returns SIG_PX14_INVALIDARG_NULL_POINTER if NULL
#define PX14_ENSURE_POINTER(hBrd, p, t, name)           \
    do{ SIGASSERT_POINTER(p,t);                         \
        if (_PX14_UNLIKELY(NULL == (p))) {              \
            SetErrorExtra(hBrd, (name) ? (name) : NULL);\
            return SIG_PX14_INVALIDARG_NULL_POINTER;    \
        }                                               \
    } while(0)

/// If (p->struct_size < s), returns SIG_PX14_UNKNOWN_STRUCT_SIZE
#define PX14_ENSURE_STRUCT_SIZE(hBrd, p, s, name)       \
    do{ if (_PX14_UNLIKELY((p)->struct_size < s)) {     \
        SetErrorExtra(hBrd, (name) ? (name) : NULL);    \
        return SIG_PX14_UNKNOWN_STRUCT_SIZE;            \
    } } while (0)

/// Create a 32-bit version number
/*#define PX14_VER32(a,b,c,d)                  \
    ((unsigned int)(((a) & 0xFF) << 24) |    \
     (unsigned int)(((b) & 0xFF) << 16) |    \
     (unsigned int)(((c) & 0xFF) <<  8) |    \
     (unsigned int)(((d) & 0xFF)))*/

#define PX14_VER32(a,b,c,d)                  \
    ((unsigned int)(((a) ) >> 6) |    \
     (unsigned int)(((b) & 0x3F)) |    \
     0 |    \
     0)

/// Create a 64-bit version number
#define PX14_VER64(a,b,c,d)                       \
    (((unsigned long long)((a) & 0xFFFF) << 48) | \
     ((unsigned long long)((b) & 0xFFFF) << 32) | \
     ((unsigned long long)((c) & 0xFFFF) << 16) | \
     ((unsigned long long)((d) & 0xFFFF)))

#endif // __px14_private_header_defined


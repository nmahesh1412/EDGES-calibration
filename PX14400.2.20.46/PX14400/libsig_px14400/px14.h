/** @file				px14.h
	@brief				Primary interface to PX14400 user-space library
	@version			2.20
	@date				12/14/2012
	@author				Mike DeKoker
	@author				Felix Grenier

	Copyright (C) 2008-2012 DynamicSignals LLC.

	THE PROGRAM/LIBRARY IS PROVIDED "AS IS" WITHOUT WARRANTY OF ANY KIND,
	EITHER EXPRESSED OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE IMPLIED
	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE. THE
	ENTIRE RISK TO THE QUALITY AND PERFORMANCE OF THE PROGRAMS LIES WITH
	YOU. SHOULD THE PROGRAM PROVE DEFECTIVE, YOU (AND NOT DYNAMICSIGNALS
	LLC) ASSUME THE ENTIRE COST OF ALL NECESSARY SERVICING, REPAIR OR
	CORRECTION.

	This library supports the following Signatec hardware:
	 - PX14400   : 2 channel 400MHz 14-bit digitizer PCIe card
	 - PX14400D  : DC-coupled PX14400
	 - PX14400D2 : DC-coupled 2 channel 400MHz 14-bit digitizer PCIe card
	               with analog front end identical to previous generation
				   PDA14 digitizer.
	 - PX12500   : 2 channel 500MHz 12-bit digitizer PCIe card

	PX12500 support: PX12500 devices are PX14400 devices that use an
	alternate 12-bit 500MHz ADC. This is the only difference between the two
	devices. The same driver and library are used to control both devices.
	Unless explicitly stated otherwise, all references to PX14400 devices
	in the library API and documentation also apply to PX12500 devices. For
	both PX14400 and PX12500 devices, data samples are aligned to 16-bits
	with zero-padding applied to the unused lower significant bits.

	This header file defines PX14400 family library constants, type
	definitions, macros, and function prototypes for use with PX14400 client
	applications.

	ALL PX14400 access goes through this library. This is the only interface
	to the PX14400 device driver, which in turn, is the only interface to
	the PX14400 hardware. Other platforms (e.g. .NET, LabVIEW, Matlab, etc)
	all interface through this library.

	32-bit/64-bit user-mode code: Both 32- and 64-bit user mode code access
	the PX14400 library through the same library header.
	WINDOWS: The 32-bit PX14400 library is named PX14.dll with import
	library PX14.lib. The 64-bit PX14400 library is named PX14_64.dll with
	import library PX14_64.lib. If your compiler supports the 'comment'
	pragma (Microsoft compilers do) then the appropriate import library will
	be included automatically by the build process.
	LINUX: The library is built for whatever the currently running platform
	is, 32- or 64-bit, and the library name is always the same,
	libsig_px14400

	Windows: See LibDeps.txt if you need to rebuild this library
*/
/*
	The comments in this project are formatted for use with the Doxygen
	documentation generation tool. Doxygen is available under the GNU
	General Public License and may be downloaded from the Doxygen
	website, http://www.doxygen.org.
*/
#ifndef __px14_library_header_defined
#define __px14_library_header_defined

// -- PX14400 library function return values. (SIG_*)
/// Operation successful
#define SIG_SUCCESS                         0
/// Generic error (GetLastError() will usually provide more info.)
#define SIG_ERROR                           -1
/// Bad param or board not ready
#define SIG_INVALIDARG                      -2
/// Argument is out of valid bounds
#define SIG_OUTOFBOUNDS                     -3
/// Invalid board device
#define SIG_NODEV                           -4
/// Error allocating memory
#define SIG_OUTOFMEMORY                     -5
/// Error allocating a utility DMA buffer
#define SIG_DMABUFALLOCFAIL                 -6
/// Board with given serial # not found
#define SIG_NOSUCHBOARD                     -7
/// This feature is only available on NT
#define SIG_NT_ONLY                         -8
/// Invalid operation for current operating mode
#define SIG_INVALID_MODE                    -9
/// Operation was cancelled by user
#define SIG_CANCELLED                       -10
/// Asynchronous operation is pending
#define SIG_PENDING                         1
//
// PX14400-specific library return values
//
#define SIG_PX14__FIRST                     -512
/// This operation is not currently implemented
#define SIG_PX14_NOT_IMPLEMENTED            -512
/// An invalid PX14400 device handle (HPX14) was specified
#define SIG_PX14_INVALID_HANDLE             -513
/// A specified buffer is too small
#define SIG_PX14_BUFFER_TOO_SMALL           -514
/// Argument 1 is invalid
#define SIG_PX14_INVALID_ARG_1              -515
/// Argument 2 is invalid
#define SIG_PX14_INVALID_ARG_2              -516
/// Argument 3 is invalid
#define SIG_PX14_INVALID_ARG_3              -517
/// Argument 4 is invalid
#define SIG_PX14_INVALID_ARG_4              -518
/// Argument 5 is invalid
#define SIG_PX14_INVALID_ARG_5              -519
/// Argument 6 is invalid
#define SIG_PX14_INVALID_ARG_6              -520
/// Argument 7 is invalid
#define SIG_PX14_INVALID_ARG_7              -521
/// Argument 8 is invalid
#define SIG_PX14_INVALID_ARG_8              -522
/// Requested transfer size is too small
#define SIG_PX14_XFER_SIZE_TOO_SMALL        -523
/// Requested transfer size is too large
#define SIG_PX14_XFER_SIZE_TOO_LARGE        -524
/// Given address is not part of a DMA buffer
#define SIG_PX14_INVALID_DMA_ADDR           -525
/// Operation would overrun given buffer
#define SIG_PX14_WOULD_OVERRUN_BUFFER       -526
/// Device is busy; try again later
#define SIG_PX14_BUSY                       -527
/// Incorrect function for channel's implementation
#define SIG_PX14_INVALID_CHAN_IMP           -528
/// Invalid XML data was encountered
#define SIG_PX14_XML_MALFORMED              -529
/// XML data was well formed, but not valid
#define SIG_PX14_XML_INVALID                -530
/// Generic XML releated error
#define SIG_PX14_XML_GENERIC                -531
/// The specified rate is too fast
#define SIG_PX14_RATE_TOO_FAST              -532
/// The specified rate is too slow
#define SIG_PX14_RATE_TOO_SLOW              -533
/// The specified frequency is not available; see operator's manual
#define SIG_PX14_RATE_NOT_AVAILABLE         -534
/// An unexpected error occurred; debug builds will have failed assertion
#define SIG_PX14_UNEXPECTED                 -535
/// A socket error occurred
#define SIG_PX14_SOCKET_ERROR               -536
/// Network subsystem is not ready for network communication
#define SIG_PX14_NETWORK_NOT_READY          -537
/// Limit on number of tasks/processes using sockets has been reached
#define SIG_PX14_SOCKETS_TOO_MANY_TASKS     -538
/// Generic sockets implementation start up failure
#define SIG_PX14_SOCKETS_INIT_ERROR         -539
/// Not connected to a remote PX14400 device
#define SIG_PX14_NOT_REMOTE                 -540
/// Operation timed out
#define SIG_PX14_TIMED_OUT                  -541
/// Connection refused by server; service may not be running
#define SIG_PX14_CONNECTION_REFUSED         -542
/// Received an invalid client request
#define SIG_PX14_INVALID_CLIENT_REQUEST     -543
/// Received an invalid server response
#define SIG_PX14_INVALID_SERVER_RESPONSE    -544
/// Remote service call returned with an error
#define SIG_PX14_REMOTE_CALL_RETURNED_ERROR -545
/// Undefined method invoked on remote server
#define SIG_PX14_UNKNOWN_REMOTE_METHOD      -546
/// Server closed the connection
#define SIG_PX14_SERVER_DISCONNECTED        -547
/// Remote call for this operation is not implemented or available
#define SIG_PX14_REMOTE_CALL_NOT_AVAILABLE  -548
/// Unknown firmware file type
#define SIG_PX14_UNKNOWN_FW_FILE            -549
/// Firmware upload failed
#define SIG_PX14_FIRMWARE_UPLOAD_FAILED     -550
/// Invalid firmware upload file
#define SIG_PX14_INVALID_FW_FILE            -551
/// Failed to open destination file
#define SIG_PX14_DEST_FILE_OPEN_FAILED      -552
/// Failed to open source file
#define SIG_PX14_SOURCE_FILE_OPEN_FAILED    -553
/// File IO error
#define SIG_PX14_FILE_IO_ERROR              -554
/// Firmware is incompatible with specified PX14400
#define SIG_PX14_INCOMPATIBLE_FIRMWARE      -555
/// Unknown structure version specified to library function (X::struct_size)
#define SIG_PX14_UNKNOWN_STRUCT_SIZE        -556
/// An invalid hardware register read/write was attempted
#define SIG_PX14_INVALID_REGISTER           -557
/// An internal FIFO overflowed during acq; couldn't keep up with data rate
#define SIG_PX14_FIFO_OVERFLOW              -558
/// PX14400 firmware could not synchronize to acquisition clock
#define SIG_PX14_DCM_SYNC_FAILED            -559
/// Could not write all data; disk is full
#define SIG_PX14_DISK_FULL                  -560
/// An invalid object handle was used
#define SIG_PX14_INVALID_OBJECT_HANDLE      -561
/// Failed to create a thread
#define SIG_PX14_THREAD_CREATE_FAILURE      -562
/// Phase lock loop (PLL) failed to lock; clock may be bad
#define SIG_PX14_PLL_LOCK_FAILED            -563
/// Recording thread is not responding
#define SIG_PX14_THREAD_NOT_RESPONDING      -564
/// A recording sesssion error occurred
#define SIG_PX14_REC_SESSION_ERROR          -565
/// Cannot arm recording sesssion; already armed or stopped
#define SIG_PX14_REC_SESSION_CANNOT_ARM     -566
/// Snapshots not enabled for given recording session
#define SIG_PX14_SNAPSHOTS_NOT_ENABLED      -567
/// No data snapshot is available
#define SIG_PX14_SNAPSHOT_NOT_AVAILABLE     -568
/// An error occurred while processing .srdc file
#define SIG_PX14_SRDC_FILE_ERROR            -569
/// Named item could not be found
#define SIG_PX14_NAMED_ITEM_NOT_FOUND       -570
/// Could not find Signatec Recorded Data Context info
#define SIG_PX14_CANNOT_FIND_SRDC_DATA      -571
/// Feature is not implemented in current firmware version; upgrade firmware
#define SIG_PX14_NOT_IMPLEMENTED_IN_FIRMWARE -572
/// Timestamp FIFO overflowed during recording
#define SIG_PX14_TIMESTAMP_FIFO_OVERFLOW    -573
/// Cannot determine which firmware needs to be uploaded; update software
#define SIG_PX14_CANNOT_DETERMINE_FW_REQ    -574
/// Required firmware not found in firmware update file
#define SIG_PX14_REQUIRED_FW_NOT_FOUND      -575
/// Loaded firmware is up to date with firmware update file
#define SIG_PX14_FIRMWARE_IS_UP_TO_DATE     -576
/// Operation not implemented for virtual devices
#define SIG_PX14_NO_VIRTUAL_IMPLEMENTATION  -577
/// PX14400 device has been removed from system
#define SIG_PX14_DEVICE_REMOVED             -578
/// JTAG IO error
#define SIG_PX14_JTAG_IO_ERROR              -579
/// Access denied
#define SIG_PX14_ACCESS_DENIED              -580
/// Phase lock loop (PLL) failed to lock; lock not stable
#define SIG_PX14_PLL_LOCK_FAILED_UNSTABLE   -581
/// Item could not be set with the given constraints
#define SIG_PX14_UNREACHABLE                -582
/// Invalid operating mode change; all changes must go or come from Standby
#define SIG_PX14_INVALID_MODE_CHANGE        -583
/// Underlying device is not yet ready; try again shortly
#define SIG_PX14_DEVICE_NOT_READY           -584
/// A parameter is not aligned properly
#define SIG_PX14_ALIGNMENT_ERROR            -585
/// Invalid operation for current board configuration
#define SIG_PX14_INVALID_OP_FOR_BRD_CONFIG  -586
/// Unknown JTAG chain configuration; out-of-date software or hardware error
#define SIG_PX14_UNKNOWN_JTAG_CHAIN         -587
/// Slave device failed to synchronize to master device
#define SIG_PX14_SLAVE_FAILED_MASTER_SYNC   -588
/// Feature is not implemented in current driver version; upgrade software
#define SIG_PX14_NOT_IMPLEMENTED_IN_DRIVER  -589
/// An error occurred while converting text from one encoding to another
#define SIG_PX14_TEXT_CONV_ERROR            -590
/// An invalid pointer was specified
#define SIG_PX14_INVALIDARG_NULL_POINTER    -591
/// Operation not available in current logic package
#define SIG_PX14_INCORRECT_LOGIC_PACKAGE    -592
/// Configuration EEPROM validation failed
#define SIG_PX14_CFG_EEPROM_VALIDATE_ERROR  -593
/// Failed to allocate or create a system object such as semaphore, mutex, etc
#define SIG_PX14_RESOURCE_ALLOC_FAILURE     -594
/// Operation failed; additional context information available via GetErrorTextPX14
#define SIG_PX14_OPERATION_FAILED           -595
/// Requested buffer is already checked out
#define SIG_PX14_BUFFER_CHECKED_OUT         -596
/// Requested buffer does not exist
#define SIG_PX14_BUFFER_NOT_ALLOCATED       -597

/// Operation was quasi-successful; one or more items failed
#define SIG_PX14_QUASI_SUCCESSFUL           512

/// Data type of a PX14400 data sample
typedef unsigned short                      px14_sample_t;
/// Data type of a PX14400 timestamp
typedef unsigned long long                  px14_timestamp_t;
/// Size of a PX14400 data sample in bytes
#define PX14_SAMPLE_SIZE_IN_BYTES           2
/// Size of a PX14400 data sample in bits
#define PX14_SAMPLE_SIZE_IN_BITS            16
/// Maximum PX14400 sample value
#define PX14_SAMPLE_MAX_VALUE               0xFFFC
/// Midscale PX14400 sample value
#define PX14_SAMPLE_MID_VALUE               0x8000
/// Minimum PX14400 sample value
#define PX14_SAMPLE_MIN_VALUE               0x0000
/// Maximum PX14400 channel count
#define PX14_MAX_CHANNELS                   2

/// An invalid PX14400 device handle value
#define PX14_INVALID_HANDLE                 NULL
/// An invalid PX14400 board number
#define PX14_BOARD_NUM_INVALID              0xFFFFFFFF
/// Signatec PX14400 board family ID
#define PX14_BOARD_TYPE                     0x31
/// Assumed maximum local PX14400 device count (mutable)
#define PX14_MAX_DEVICES                    32
/// Maximum trigger level value
#define PX14_MAX_TRIGGER_LEVEL              PX14_SAMPLE_MAX_VALUE
/// ~Midscale trigger level
#define PX14_TRIGGER_LEVEL_MIDSCALE         0x8000
/// Maximum DC-offset value
#define PX14_MAX_DC_OFFSET                  4095
/// Maximum fine DC-offset value (PX14400D2 devices)
#define PX14_MAX_FINE_DC_OFFSET             2047
/// ~Midscale DC-offset value
#define PX14_DC_OFFSET_MIDSCALE             2048
/// Maximum PX14400 clock division for clock divider #1
#define PX14CLKDIV1_MAX                     32
/// Maximum PX14400 clock division for clock divider #2
#define PX14CLKDIV2_MAX                     6
/// Maximum SAB board number
#define PX14_MAX_SAB_BOARD_NUMBER           8
/// Sentry for free-run (infinite) acquisitions
#define PX14_FREE_RUN                       0
/// Minimum PX14400 ADC sampling rate in MHz
#define PX14_ADC_FREQ_MIN_MHZ               20
/// Maximum PX14400 ADC sampling rate in MHz
#define PX14_ADC_FREQ_MAX_MHZ               400
/// Minimum frequency we can use with the internal clock
#define PX14_VCO_FREQ_MIN_MHZ               20
/// Maximum frequency we can use with the internal clock
#define PX14_VCO_FREQ_MAX_MHZ               400
/// Preferred port for remote PX14400 servers
#define PX14_SERVER_PREFERRED_PORT          3492
/// Default timeout for a remote PX14400 service request in milliseconds
#define PX14_SERVER_REQ_TIMEOUT_DEF         3000
/// Boolean true
#define PX14_TRUE                           1
/// Boolean false
#define PX14_FALSE                          0

/// PX14400 RAM size in bytes
#define PX14_RAM_SIZE_IN_BYTES              (512 * 1024 * 1024)
/// PX14400 RAM size in samples; portable code use GetSampleRamSizePX14
#define PX14_RAM_SIZE_IN_SAMPLES            \
    (PX14_RAM_SIZE_IN_BYTES / PX14_SAMPLE_SIZE_IN_BYTES)

// -- PX14400 source clock values (PX14CLKSRC_*)
/// Internal VCO (Power-up default)
#define PX14CLKSRC_INT_VCO                  0
/// External clock
#define PX14CLKSRC_EXTERNAL                 1
#define PX14CLKSRC__COUNT                   2   // Invalid setting

// -- PX14400 ADC clock reference values (PX14CLKREF_*)
/// Internal 10MHz clock reference (Power-up default)
#define PX14CLKREF_INT_10MHZ                0
/// External clock reference
#define PX14CLKREF_EXT                      1
#define PX14CLKREF__COUNT                   2   // Invalid setting

// -- PX14400 post-ADC clock divider values (PX14POSTADCCLKDIV_*)
/// No post-ADC clock division (Power-up default)
#define PX14POSTADCCLKDIV_01                0
/// Post-ADC clock division of 2
#define PX14POSTADCCLKDIV_02                1
/// Post-ADC clock division of 4
#define PX14POSTADCCLKDIV_04                2
/// Post-ADC clock division of 8
#define PX14POSTADCCLKDIV_08                3
/// Post-ADC clock division of 16
#define PX14POSTADCCLKDIV_16                4
/// Post-ADC clock division of 32
#define PX14POSTADCCLKDIV_32                5
#define PX14POSTADCCLKDIV__COUNT            6   // Invalid setting

// -- PX14400 active channel selection constants (PX14CHANNEL_*)
/// Dual channel mode (Power-up default)
#define PX14CHANNEL_DUAL                    0
/// Single channel mode - Channel 1
#define PX14CHANNEL_ONE                     1
/// Single channel mode - Channel 2
#define PX14CHANNEL_TWO                     2
#define PX14CHANNEL__COUNT                  3   // Invalid setting

// -- PX14400 trigger mode values (PX14TRIGMODE_*)
/// Trigger event starts a single data acquisition (Power-up default)
#define PX14TRIGMODE_POST_TRIGGER           0
/// Each trigger event begins a new statically sized data acquisition
#define PX14TRIGMODE_SEGMENTED              1
#define PX14TRIGMODE__COUNT                 2   // Invalid setting

// -- PX14400 trigger source values (PX14TRIGSRC_*)
/// Internal trigger, channel 1 (Power-up default)
#define PX14TRIGSRC_INT_CH1                 0
/// Internal trigger, channel 2
#define PX14TRIGSRC_INT_CH2                 1
/// External trigger
#define PX14TRIGSRC_EXT                     2
#define PX14TRIGSRC__COUNT                  3   // Invalid setting

// -- PX14400 trigger direction values (PX14TRIGDIR_*)
/// Trigger occurs on positive going signal (Power-up default)
#define PX14TRIGDIR_POS                     0
/// Trigger occurs on negative going signal
#define PX14TRIGDIR_NEG                     1
#define PX14TRIGDIR__COUNT                  2   // Invalid setting

// -- PX14400 trigger occur during pre-trigger mode
#define TODPT_DISABLE						0
#define TODPT_ENABLE						1

// -- Device trigger selection

#define PX14TRIGSEL_A_AND_HYST				0
#define PX14TRIGSEL_A_AND_B					1
#define PX14TRIGSEL_RESERVED				2
#define PX14TRIGSEL_A_OR_B					3

// -- PX14400 Operating Mode values (PX14MODE_*)
/// Power down mode
#define PX14MODE_OFF                        0
/// Standby (ready) mode
#define PX14MODE_STANDBY                    1
/// RAM acquisition mode
#define PX14MODE_ACQ_RAM                    2
/// PCI acquisition mode using only PCI FIFO
#define PX14MODE_ACQ_PCI_SMALL_FIFO         3
/// RAM-buffered PCI acquisition mode; data buffered through PX14400 RAM
#define PX14MODE_ACQ_PCI_BUF                4
/// Signatec Auxiliary Bus (SAB) acquisition; acquire directly to SAB bus
#define PX14MODE_ACQ_SAB                    5
/// Buffered SAB acquisition; acquire to SAB, buffering through PX14400 RAM
#define PX14MODE_ACQ_SAB_BUF                6
/// PX14400 RAM read to PCI; transfer data from PX14400 RAM to host PC
#define PX14MODE_RAM_READ_PCI               7
/// PX14400 RAM read to SAB; transfer data from PX14400 RAM to SAB bus
#define PX14MODE_RAM_READ_SAB               8
/// PX14400 RAM write from PCI; transfer data from host PC to PX14400 RAM
#define PX14MODE_RAM_WRITE_PCI              9
/// Cleanup a pending DMA by sending a force Trigger
#define PX14MODE_CLEANUP_PENDING_DMA        10

#define PX14MODE__COUNT                     11  // Invalid setting

// -- PX14400A input voltage range values (PX14VOLTRNG_*) (AC-coupled devices only)
// - This setting is used for non-amplified devices only
/// Static input voltage range used for non-amplified PX14400 devices
#define PX14VOLTRNG_STATIC_1_100_VPP        0

#define PX14VOLTRNG__START					1
// - All settings below are used for amplified devices only
/// 220 mVp-p
#define PX14VOLTRNG_0_220_VPP               1
/// 247 mVp-p
#define PX14VOLTRNG_0_247_VPP               2
/// 277 mVp-p
#define PX14VOLTRNG_0_277_VPP               3
/// 311 mVp-p
#define PX14VOLTRNG_0_311_VPP               4
/// 349 mVp-p
#define PX14VOLTRNG_0_349_VPP               5
/// 391 mVp-p
#define PX14VOLTRNG_0_391_VPP               6
/// 439 mVp-p
#define PX14VOLTRNG_0_439_VPP               7
/// 493 mVp-p
#define PX14VOLTRNG_0_493_VPP               8
/// 553 mVp-p
#define PX14VOLTRNG_0_553_VPP               9
/// 620 mVp-p
#define PX14VOLTRNG_0_620_VPP               10
/// 696 mVp-p
#define PX14VOLTRNG_0_696_VPP               11
/// 781 mVp-p
#define PX14VOLTRNG_0_781_VPP               12
/// 876 mVp-p
#define PX14VOLTRNG_0_876_VPP               13
/// 983 mVp-p
#define PX14VOLTRNG_0_983_VPP               14
/// 1.103 Vp-p
#define PX14VOLTRNG_1_103_VPP               15
/// 1.237 Vp-p
#define PX14VOLTRNG_1_237_VPP               16
/// 1.388 Vp-p
#define PX14VOLTRNG_1_388_VPP               17
/// 1.557 Vp-p
#define PX14VOLTRNG_1_557_VPP               18
/// 1.748 Vp-p
#define PX14VOLTRNG_1_748_VPP               19
/// 1.961 Vp-p
#define PX14VOLTRNG_1_961_VPP               20
/// 2.2 Vp-p
#define PX14VOLTRNG_2_200_VPP               21
/// 2.468 Vp-p
#define PX14VOLTRNG_2_468_VPP               22
/// 2.77 Vp-p
#define PX14VOLTRNG_2_770_VPP               23
/// 3.108 Vp-p
#define PX14VOLTRNG_3_108_VPP               24
/// 3.487 Vp-p
#define PX14VOLTRNG_3_487_VPP               25
#define PX14VOLTRNG__END					25

// - This block of input voltage range selections are only valid
//   for PX14400D2 devices.
#define PX14VOLTRNG_D2__START               26
/// PX14400D2 devices only: 3Vp-p
#define PX14VOLTRNG_D2_3_00_VPP             26
/// PX14400D2 devices only: 1.6Vp-p
#define PX14VOLTRNG_D2_1_60_VPP             27
/// PX14400D2 devices only: 1.0Vp-p
#define PX14VOLTRNG_D2_1_00_VPP             28
/// PX14400D2 devices only: 600mVp-p
#define PX14VOLTRNG_D2_0_600_VPP            29
/// PX14400D2 devices only: 333mVp-p
#define PX14VOLTRNG_D2_0_333_VPP            30
/// PX14400D2 devices only: 200mVp-p
#define PX14VOLTRNG_D2_0_200_VPP            31
#define PX14VOLTRNG_D2__END                 31

#define PX14VOLTRNG__COUNT                  32  // Invalid setting

// -- PX14400D Input Gain selection values (PX14DGAIN_*) (DC-coupled devices only)
/// Low gain (1.2Vp-p input)
#define PX14DGAIN_LOW                       0
/// High gain (400mVp-p input)
#define PX14DGAIN_HIGH                      1
#define PX14DGAIN__COUNT                    2

// -- GetErrorTextPX14 flags (PX14ETF_*)
#define PX14ETF_IGNORE_SYSERROR             0x00000001
#define PX14ETF_NO_SYSERROR_TEXT            0x00000002
#define PX14ETF_FORCE_SYSERROR              0x00000004

// --  PX14400 version ID values (PX14VERID_*)
/// Request (package) version information for the current PX14400 firmware
#define PX14VERID_FIRMWARE                  0
/// Request version/revision information for the current PX14400 hardware
#define PX14VERID_HARDWARE                  1
/// Request version information for the PX14400 device driver
#define PX14VERID_DRIVER                    2
/// Request version information for the PX14400 library
#define PX14VERID_LIBRARY                   3
/// Request version information for the PX14400 software release
#define PX14VERID_PX14_SOFTWARE             4
/// Request version information for the current PX14400 system firmware
#define PX14VERID_PCI_FIRMWARE              5
/// Request version information for previous PX14400 system firmware
#define PX14VERID_PREV_PCI_FIRMWARE         6
/// Request version information for the current PX14400 SAB firmware
#define PX14VERID_SAB_FIRMWARE              7
/// Request version information for previous PX14400 SAB firmware
#define PX14VERID_PREV_SAB_FIRMWARE         8
#define PX14VERID__COUNT                    9   // Invalid setting

// -- PX14400 version string flags (PX14VERF_*)
/// No pre-release indication
#define PX14VERF_NO_PREREL                  0x00000001
/// No sub-minor info; imples PX14VERF_NO_PACKAGE
#define PX14VERF_NO_SUBMIN                  0x00000006
/// No package info
#define PX14VERF_NO_PACKAGE                 0x00000004
/// No custom enumeration info
#define PX14VERF_NO_CUSTOM                  0x00000008
/// Compact version string if possible (XX.YY.00.00 -> XX.YY)
#define PX14VERF_COMPACT_VER                0x00000010
/// Use 0 padding for one digit version numbers
#define PX14VERF_ZERO_PAD_SINGLE_DIGIT_VER  0x00000020
/// Allow function to return aliases for known versions
#define PX14VERF_ALLOW_ALIASES				0x00000040

// -- PX14400 SAB mode value (PX14SABMODE_*)
/// Inactive slave; neither send nor receive data (Power-up default)
#define PX14SABMODE_INACTIVE_SLAVE          0
/// Master; send data to SAB
#define PX14SABMODE_MASTER                  1
#define PX14SABMODE__COUNT                  2

// -- PX14400 SAB configuration values (PX14SABCFG_*)
/// 64-bit SAB transfers (Power-up default)
#define PX14SABCFG_64                       0
/// Lower 32 bits of SAB bus
#define PX14SABCFG_32                       1
/// Lower 32 bits of SAB bus
#define PX14SABCFG_32L                      1
/// Upper 32 bits of SAB bus
#define PX14SABCFG_32H                      2
#define PX14SABCFG__COUNT                   3

// -- PX14400 SAB clock values (PX14SABCLK_*)
/// Use acquisition clock
#define PX14SABCLK_ACQ_CLOCK                0
/// 62.5 MHz clock
#define PX14SABCLK_62_5MHZ                  1
/// 125 MHz clock (Power-up default)
#define PX14SABCLK_125MHZ                   2
#define PX14SABCLK__COUNT                   3   // Invalid setting

// -- PX14400 SAB direct command values (PX14SABCMD_*)
/// Enter standby mode
#define PX14SABCMD_MODE_STANDBY             0xF
/// Acquire to SAB (unbuffered)
#define PX14SABCMD_MODE_ACQ_SAB             0xB
/// Acquire to RAM
#define PX14SABCMD_MODE_ACQ_RAM             0xA
/// Transfer PX14400 RAM data over SAB
#define PX14SABCMD_MODE_READ_SAB            0xD

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

// -- PX14400 board revision identifiers
// (Note: This is not the same as a board's hardware revision value)
/// Original PX14400
#define PX14BRDREV_1                        0
/// Original PX14400
#define PX14BRDREV_PX14400                  0           // Alias for PX14BRDREV_1
/// PX12500 - 12-bit 500MHz based on PX14400 hardware
#define PX14BRDREV_PX12500                  1
/// PX14400D - DC-coupled PX14400 device
#define PX14BRDREV_PX14400D                 2
/// PX14400D2 - DC-coupled PX14400 device with PDA14 front end
#define PX14BRDREV_PX14400D2				3
#define PX14BRDREV__COUNT                   4

// -- PX14400 board sub-revision values; each set relative to PX14BRDREV_*
/// PX14400 SP - Signal Processing; has SAB
#define PX14BRDREVSUB_1_SP                  0
/// PX14400 DR - Data recorder; no SAB
#define PX14BRDREVSUB_1_DR                  1
#define PX14BRDREVSUB_1__COUNT              2

// -- PX14400 settings save/load XML flags (PX14XMLSET_*)
/// Serialize to a node only; do not include XML header info
#define PX14XMLSET_NODE_ONLY                0x00000001
/// Pretty-print XML output; add newlines and indentation for human eyes
#define PX14XMLSET_FORMAT_OUTPUT            0x00000002
/// Do not set default hardware settings prior to loading settings
#define PX14XMLSET_NO_PRELOAD_DEFAULTS      0x00000004

// -- PX14400 send request flags (PX14SRF_*)
/// Auto-handle response; useful if you're only receive pass/fail info
#define PX14SRF_AUTO_HANDLE_RESPONSE        0x00000001
/// Do not validate response at all; default is a quick, cursory check
#define PX14SRF_NO_VALIDATION               0x00000002
/// SendServiceRequestPX14 will free incoming request data
#define PX14SRF_AUTO_FREE_REQUEST           0x00000004
/// Reserved for internal use
#define PX14SRF_CONNECTING                  0x80000000

// -- PX14400 master/slave configuration values (PX14MSCFG_*)
/// Normal, standalone PX14400 (Power-up default)
#define PX14MSCFG_STANDALONE                0
/// Master with 1 slave; will provide clock and trigger for slave
#define PX14MSCFG_MASTER_WITH_1_SLAVE       1
/// Master with 2 slaves; will provide clock and trigger for slaves
#define PX14MSCFG_MASTER_WITH_2_SLAVES      2
/// Master with 3 slaves; will provide clock and trigger for slaves
#define PX14MSCFG_MASTER_WITH_3_SLAVES      3
/// Master with 4 slaves; will provide clock and trigger for slaves
#define PX14MSCFG_MASTER_WITH_4_SLAVES      4
/// Slave #1; clock and trigger will be synchronized with master
#define PX14MSCFG_SLAVE_1                   5
/// Slave #2; clock and trigger will be synchronized with master
#define PX14MSCFG_SLAVE_2                   6
/// Slave #3; clock and trigger will be synchronized with master
#define PX14MSCFG_SLAVE_3                   7
/// Slave #4; clock and trigger will be synchronized with master
#define PX14MSCFG_SLAVE_4                   8
#define PX14MSCFG__COUNT                    9   // Invalid setting

/// Macro to determine if given PX14MSCFG_* value is for a master device
#define PX14_IS_MASTER(mscfg)                \
    (((mscfg) >= PX14MSCFG_MASTER_WITH_1_SLAVE) && \
     ((mscfg) <= PX14MSCFG_MASTER_WITH_4_SLAVES))

/// Macro to determine if given PX14MSCFG_* value is for a slave device
#define PX14_IS_SLAVE(mscfg)                 \
    (((mscfg) >= PX14MSCFG_SLAVE_1) && ((mscfg) <= PX14MSCFG_SLAVE_4))

// -- PX14400 digital output mode values (PX14DIGIO_*)
/// OUTPUT: 0V (Power-up default)
#define PX14DIGIO_OUT_0V                    0
/// Digital out is synchronized trigger
#define PX14DIGIO_OUT_SYNC_TRIG             1
/// Digital out is analog-to-digial converter clock divided by 2
#define PX14DIGIO_OUT_ADC_CLOCK_DIV_2       2
#define PX14DIGIO_OUT_ADC_CLOCK             2		// older symbol name
/// TTL high level
#define PX14DIGIO_OUT_3_3V                  3
/// INPUT: Digital input pulse to generate timestamp
#define PX14DIGIO_IN_TS_GEN                 4
/// Reserved for future (5)
#define PX14DIGIO_RESERVED_5                5
/// Reserved for future (6)
#define PX14DIGIO_RESERVED_6                6
/// Reserved for future (7)
#define PX14DIGIO_RESERVED_7                7
#define PX14DIGIO__COUNT                    8   // Invalid setting

// -- PX14400 firmware release notes severity (PX14FWNOTESEV_*)
/// No firmware release notes provided
#define PX14FWNOTESEV_NONE                  0
/// Firmware release notes are available
#define PX14FWNOTESEV_NORMAL                1
/// Important firmware release notes available; always prompted
#define PX14FWNOTESEV_IMPORTANT             2
#define PX14FWNOTESEV__COUNT                3

// -- PX14400 Firmware Context flags (PX14FWCTXF_*)
/// Not really a firmware file; used for things like verify files
#define PX14FWCTXF_VERIFY_FILE              0x00000001
#define PX14FWCTXF__DEFAULT                 0

// -- PX14400 upload firmware flags (PX14UFWF_*)
/// Only upload firmware if given firmware is different from what is loaded
#define PX14UFWF_REFRESH_ONLY               0x00000001
/// Check that firmware file is compatible with hardware; no firmware loaded
#define PX14UFWF_COMPAT_CHECK_ONLY          0x00000002
/// Force erasing of unused EEPROMs even if they're already blank
#define PX14UFWF_FORCE_EEPROM_ERASE         0x00000004

// -- PX14400 upload firmware output flags (PX14UFWOUTF_*)
/// System must be shutdown in order for firmware update to have effect
#define PX14UFWOUTF_SHUTDOWN_REQUIRED       0x00000001
/// System must be reboot in order for firmware update to have effect
#define PX14UFWOUTF_REBOOT_REQUIRED         0x00000002
/// All firmware is up-to-date; no firmware uploaded
#define PX14UFWOUTF_FW_UP_TO_DATE           0x00000004
/// Firmware was verified; no new firmware was uploaded
#define PX14UFWOUTF_VERIFIED                0x00000008

// -- PX14400 file writing flags (PX14FILWF_*)
/// Append if file already exists; default is to overwrite
#define PX14FILWF_APPEND                    0x00000001
/// Use bytes_skip even if we're appending to a file
#define PX14FILWF_ALWAYS_SKIPBYTES          0x00000002
/// Save data as text; sample values will be whitespace-delimited
#define PX14FILWF_SAVE_AS_TEXT              0x00000004
/// Don't try to use fast, unbuffered IO
#define PX14FILWF_NO_UNBUFFERED_IO          0x00000008
/// Assume data is dual channel; applies to single-file text saves
#define PX14FILWF_ASSUME_DUAL_CHANNEL       0x00000010
/// Use hexadecimal output for; applies to text saves
#define PX14FILWF_HEX_OUTPUT                0x00000020
/// Deinterleave channel data into separate files
#define PX14FILWF_DEINTERLEAVE              0x00000040
/// Convert data to signed before saving
#define PX14FILWF_CONVERT_TO_SIGNED         0x00000080
/// Generate a Signatec Recorded Data Context (.srdc) file for each output
#define PX14FILWF_GENERATE_SRDC_FILE        0x00000100
/// Embed SRDC information in data file as NTFS alternate file stream
#define PX14FILWF_EMBED_SRDC_AS_AFS         0x00000200
/// Save PX14400 timstamp data in external file
#define PX14FILWF_SAVE_TIMESTAMPS           0x00000400
/// Save timestamps as newline-delimited text; use with above flag
#define PX14FILWF_TIMESTAMPS_AS_TEXT        0x00000800
/// Use timestamp FIFO overflow marker: F1F0F1F0F1F0F1F0 F1F0F1F0F1F0F1F0
#define PX14FILWF_USE_TS_FIFO_OVFL_MARKER   0x00001000
/// Abort and fail operation if timestamp FIFO overflows; for recordings
#define PX14FILWF_ABORT_OP_ON_TS_OVFL       0x00002000

// -- PX14400 file writing output flags (PX14FILWOUTF_*)
/// Timestamp FIFO overflowed during write/record process
#define PX14FILWOUTF_TIMESTAMP_FIFO_OVERFLOW 0x00000001
/// No timestamp data was available
#define PX14FILWOUTF_NO_TIMESTAMP_DATA      0x00000002

/// -- PX14400 Recording Session flags (PX14RECSESF_*)
/// Do a PX14400 RAM-buffered PCI acquisition recording
#define PX14RECSESF_REC_PCI_ACQ             0x00000001
/// Do a PX14400 RAM acquisition/transfer recording
#define PX14RECSESF_REC_RAM_ACQ             0x00000002
/// Mask for session recording type
#define PX14RECSESF_REC__MASK               0x0000000F
/// Do not auto arm recording; will be done with ArmRecordingSessionPX14
#define PX14RECSESF_DO_NOT_ARM              0x00000010
/// Periodically obtain snapshots of recording data
#define PX14RECSESF_DO_SNAPSHOTS            0x00000020
/// Use utility DMA buffers for data transfers
#define PX14RECSESF_USE_UTILITY_BUFFERS     0x00000040
/// Uses utility DMA buffer chain to buffer data
#define PX14RECSESF_DEEP_BUFFERING          0x00000080
/// Okay to use boot-time DMA buffers; 1x4MiS or 2x2MiS buffers required; ignored if PX14RECSESF_DEEP_BUFFERING set
#define PX14RECSESF_BOOT_BUFFERS_OKAY		0x00000100

// -- PX14400 Recording Session status (PX14RECSTAT_*)
/// Idle; recording not yet started
#define PX14RECSTAT_IDLE                    0
/// Operation in progress or waiting for trigger event
#define PX14RECSTAT_IN_PROGRESS             1
/// Operation complete or stopped by user
#define PX14RECSTAT_COMPLETE                2
/// Operation ended due to error
#define PX14RECSTAT_ERROR                   3
#define PX14RECSTAT__COUNT                  4

// -- PX14400 Recording Session progress query flags (PX14RECPROGF_*)
/// Do not obtain error text on error
#define PX14RECPROGF_NO_ERROR_TEXT          0x00000001
#define PX14RECPROGF__DEFAULT               0

// -- PX14400 SRDC file open flags (PX14SRDCOF_*)
/// Opens/create file, refresh and write settings, then close file
#define PX14SRDCOF_QUICK_SET                0x00000001
/// Open existing SRDC file; fail if file does not exist
#define PX14SRDCOF_OPEN_EXISTING            0x00000002
/// Create a new SDRC file; will ignore and overwrite existing data
#define PX14SDRCOF_CREATE_NEW               0x00000004
/// Given pathname is recorded data; have library search for SRDC
#define PX14SRDCOF_PATHNAME_IS_REC_DATA     0x00000008

// -- SRDC enumeration flags (PX14SRDCENUM_*)
/// Skip standard SRDC items; use to obtain only user-defined items
#define PX14SRDCENUM_SKIP_STD               0x00000001
/// Skip user-defined SRDC items; use to obtain only standard items
#define PX14SRDCENUM_SKIP_USER_DEFINED      0x00000002
/// Only include modified items
#define PX14SRDCENUM_MODIFIED_ONLY          0x00000004

// -- PX14400 Timestamp Mode values (PX14TSMODE_*)
/// No timestamps are generated
#define PX14TSMODE_NO_TIMESTAMPS            0
/// A timestamp is generated for each segment in a segmented acquisition
#define PX14TSMODE_SEGMENTS                 1
/// A timestamp is generated for each trigger received on external trigger
#define PX14TSMODE_TS_ON_EXT_TRIGGER        2
/// A timestamp is generated for each rising edge of digital input
#define PX14TSMODE_TS_ON_DIGITAL_INPUT      3
#define PX14TSMODE__COUNT                   4       // Invalid setting

// -- PX14400 Timestamp read flags (PX14TSREAD_*)
/// Call to read from a known-full timestamp FIFO
#define PX14TSREAD_READ_FROM_FULL_FIFO      0x00000001
/// Ignore FIFO flag state; always read requested number of timestamps (troubleshooting)
#define PX14TSREAD_IGNORE_FIFO_FLAGS		0x00000002
/// Mask for input flags
#define PX14TSREAD__INPUT_FLAG_MASK         0x000FFFFF
/// Mask for output flags
#define PX14TSREAD__OUTPUT_FLAG_MASK        0xFFF00000
/// Output flag; More timestamps are available
#define PX14TSREAD_MORE_AVAILABLE           0x01000000
/// Output flag; Timestamp FIFO overflowed
#define PX14TSREAD_FIFO_OVERFLOW            0x02000000

// -- PX14400 timestamp counter mode values (PX14TSCNTMODE_*)
/// Counter unconditionally increments during acquisition mode
#define PX14TSCNTMODE_DEFAULT               0
/// Counter only increments when digitizing; pauses when waiting for trigger
#define PX14TSCNTMODE_PAUSE_WHEN_ARMED      1
#define PX14TSCNTMODE__COUNT                2

// -- PX14400 firmware types (PX14FWTYPE_*)
/// System firmware
#define PX14FWTYPE_SYS                      0
/// PCI firmware (legacy name for system firmware)
#define PX14FWTYPE_PCI                      PX14FWTYPE_SYS
/// SAB firmware
#define PX14FWTYPE_SAB                      1
/// Generic firmware package; firmware version that end users see
#define PX14FWTYPE_PACKAGE                  2
#define PX14FWTYPE__COUNT                   3

// -- PX14400 version constraint types (PX14VCT_*)
/// >=
#define PX14VCT_GTOE                        0
/// <=
#define PX14VCT_LTOE                        1
/// >
#define PX14VCT_GT                          2
/// <
#define PX14VCT_LT                          3
/// ==
#define PX14VCT_E                           4
/// !=
#define PX14VCT_NE                          5
#define PX14VCT__COUNT                      6

// -- GetBoardNamePX14 flags (PX14GBNF_*)
/// Do not include board's serial (or ordinal) number
#define PX14GBNF_NO_SN                      0x00000001
/// Use ordinal number instead of serial number
#define PX14GBNF_USE_ORD_NUM                0x00000002
/// Include master/slave status
#define PX14GBNF_INC_MSCFG                  0x00000004
/// Include virtual status
#define PX14GBNF_INC_VIRT_STATUS            0x00000008
/// Include remote status
#define PX14GBNF_INC_REMOTE_STATUS          0x00000010
/// Alphanumeric only
#define PX14GBNF_ALPHANUMERIC_ONLY          0x00000020
/// Use underscores '_' in place of spaces
#define PX14GBNF_USE_UNDERSCORES            0x00000040
/// Include sub-revision info (DR/SP##)
#define PX14GBNF_INC_SUB_REVISION           0x00000080
/// Include channel-coupling info (A/D)
#define PX14GBNF_INC_CHAN_COUPLING          0x00000100
/// Include channel amplification info (XF)
#define PX14GBNF_INC_CHAN_AMP               0x00000200
#define PX14GBNF__DETAILED                  \
    (PX14GBNF_INC_MSCFG | PX14GBNF_INC_VIRT_STATUS | \
     PX14GBNF_INC_REMOTE_STATUS | PX14GBNF_INC_SUB_REVISION |\
     PX14GBNF_INC_CHAN_COUPLING | PX14GBNF_INC_CHAN_AMP)

// -- PX14400 channel implementation flags (PX14CHANIMP_*)
/// Channel is not amplified; static input voltage range
#define PX14CHANIMPF_NON_AMPLIFIED          0x01
/// Channel is DC coupled
#define PX14CHANIMPF_DC_COUPLED             0x02

// -- PX14400 channel analog filter types (PX14CHANFILTER_*)
/// No filter (or unknown) filter installed
#define PX14CHANFILTER_NONE                 0
/// 200MHz lowpass filter; default for amplified channels
#define PX14CHANFILTER_LOWPASS_200MHZ       1
/// 400MHz lowpass filter; default for non-amplified channels
#define PX14CHANFILTER_LOWPASS_400MHZ       2
/// 225MHz lowpass filter
#define PX14CHANFILTER_LOWPASS_225MHz       3
/// 500MHz lowpass filter; default for PX12500 devices
#define PX14CHANFILTER_LOWPASS_500MHZ       4
#define PX14CHANFILTER__COUNT               5

// -- Input voltage range selection method (PX14VRSEL_*)
/// Select voltage range not less than given value
#define PX14VRSEL_NOT_LT                    0
/// Select voltage range not greater than given value
#define PX14VRSEL_NOT_GT                    1
/// Select voltage range closest to given value
#define PX14VRSEL_CLOSEST                   2
#define PX14VRSEL__COUNT                    3

// -- PX14400 PCIe FPGA types (PX14SYSFPGA_*)
/// Virtex 5 LX50t
#define PX14SYSFPGA_V5_LX50T                0
#define PX14SYSFPGA__COUNT                  1

// -- PX14400 SAB FPGA types (PX14SABFPGA_*)
/// Virtex 5 SX50T
#define PX14SABFPGA_V5_SX50T                0
/// Virtex 5 SX95T
#define PX14SABFPGA_V5_SX95T                1
#define PX14SABFPGA__COUNT                  2

// --- Windows-specific stuff ---

#ifdef _WIN32

#if !defined(_WINSOCKAPI_) && !defined(_WINSOCK2API_)
# include <Winsock2.h>      // Windows sockets implementation
#endif

#include <windows.h>        // Main Windows definitions
#include <tchar.h>          // Generic text mappings

// Default calling convention used for all library exports
#define _PX14LIBCALLCONV                    __cdecl

// Determine if we are importing or exporting symbols from DLL.
#if defined _PX14_IMPLEMENTING_DLL
# define DllSpecPX14        __declspec(dllexport)
#else
# define DllSpecPX14        __declspec(dllimport)
#endif

// Define library export calling convension and linkage
#ifdef __cplusplus
# define PX14API_(t)        extern "C" DllSpecPX14 t _PX14LIBCALLCONV
#else
# define PX14API_(t)        DllSpecPX14 t _PX14LIBCALLCONV
#endif

/// Alias for Windows socket data type
typedef SOCKET              px14_socket_t;

// MSVC Users: If PX14PP_NO_LINK_LIBRARY is #defined before including this
// file then the build process will not attempt to link with the PX14
// import library (PX14.lib). Note that automatic linking will only work if
// px14.lib is within the search path of the build process.
#if defined(_MSC_VER) && !defined(PX14PP_NO_LINK_LIBRARY)
# ifdef _WIN64
#  pragma comment(lib, "px14_64.lib")
# else
#  pragma comment(lib, "px14.lib")
# endif
#endif

#endif      // _WIN32

// --- Linux-specific stuff ---

#ifdef __linux__

// Define library export calling convension and linkage
#ifdef __cplusplus
# define PX14API_(t)        extern "C" t
#else
# define PX14API_(t)        t
#endif

/// Alias for Linux socket data type
typedef int                 px14_socket_t;

/// should not be here really;
// on 32b os, int & long are 32b but long long is 64b
// on 64b os, int is 32b but long and long long are 64b
// so the following should work for both 32- and 64-b os
typedef unsigned long long  UINT64;

// cdecl is default on Linux systems
#define _PX14LIBCALLCONV

#endif      // __linux__

/// Most all library exports return an int (SIG_* or SIG_PX14_*)
#define PX14API     PX14API_(int)

/// Specify default function arguments for C++ clients...
#ifndef _PX14_DEF
# ifdef __cplusplus
#  define _PX14_DEF(v)      = v
# else
#  define _PX14_DEF(v)
# endif
#endif

// PX14400 types ------------------------------------------------------- //

/// A handle to a PX14400 device; consider an opaque object
typedef struct _px14hs_ { int hello_there; }* HPX14;

/// Signature of most all SetXXXPX14 routines
typedef int (_PX14LIBCALLCONV* px14lib_set_func_t)(HPX14, unsigned int);
/// Signature of most all GetXXXPX14 routines
typedef int (_PX14LIBCALLCONV* px14lib_get_func_t)(HPX14, int);

/// Signature of a couple SetXXXPX14 routines that take double arguments
typedef int (_PX14LIBCALLCONV* px14lib_setd_func_t)(HPX14, double);
/// Signature of a couple GetXXXPX14 routines that obtain double values
typedef int (_PX14LIBCALLCONV* px14lib_getd_func_t)(HPX14, double*, int);

typedef struct _PX14S_FW_VER_INFO_tag
{
    unsigned int        struct_size;        ///< struct size in bytes

    unsigned int        fw_pkg_ver;         // Firmware package version
    unsigned int        fw_pkg_cust_enum;   // Custom fw enum (package)

    int                 readme_sev;         // PX14FWNOTESEV_*
    int                 extra_flags;        // PX14FWCTXF_*

} PX14S_FW_VER_INFO;

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

/// Signature of optional file IO callback function; load/save board data
typedef int (*PX14_FILEIO_CALLBACK)(HPX14 hBrd,
                                    void* callbackCtx,
                                    px14_sample_t* sample_datap,
                                    int sample_count,
                                    const char* pathname,
                                    unsigned long long samps_in_file,
                                    unsigned long long samps_moved,
                                    unsigned long long samps_to_move);

/// Parameters for saving PX14400 data to file
typedef struct _PX14S_FILE_WRITE_PARAMS_tag
{
    unsigned int    struct_size;        ///< Init to struct size in bytes

    const char*     pathname;           ///< Destination file pathname
    const char*     pathname2;          ///< For dual channel
    unsigned int    flags;              ///< PX14FILWF_*
    unsigned int    init_bytes_skip;    ///< Initial bytes to skip in file
    unsigned long long max_file_seg;    ///< Max file samples; binary only

    PX14_FILEIO_CALLBACK pfnCallback;   ///< Optional progress callback
    void*           callbackCtx;        ///< User-defined callback context

    unsigned int    flags_out;          ///< Output flags; PX14FILWOUTF_*
    const char*     operator_notes;     ///< User-defined notes; SRDC data
    const char*     ts_filenamep;       ///< Timestamp filename override

} PX14S_FILE_WRITE_PARAMS;

/// Signature of optional recording session callback
typedef int (*PX14_REC_CALLBACK)(HPX14 hBrd,
                                 void* data,
                                 px14_sample_t* bufp,
                                 unsigned sample_count);

/// Recording session parameters
typedef struct _PX14S_REC_SESSION_PARAMS_tag
{
    unsigned int        struct_size;    ///< Init to struct size in bytes

    int                 rec_flags;      ///< PX14RECSESF_*
    unsigned long long  rec_samples;    ///< # samples or 0 for inf
    unsigned int        acq_samples;    ///< For RAM acq recordings
    unsigned int        xfer_samples;   ///< Transfer size of 0 for autoset

    PX14S_FILE_WRITE_PARAMS*    filwp;  ///< Defines output file(s)

    px14_sample_t*      dma_bufp;       ///< Optional DMA buffer address
    unsigned            dma_buf_samples;///< Size of DMA buffer in samps

    // Data snapshot parameters; valid when PX14RECSESF_DO_SNAPSHOTS set

    unsigned int        ss_len_samples; ///< Data snapshot length in samples
    unsigned int        ss_period_xfer; ///< Snapshot period in DMA xfers
    unsigned int        ss_period_ms;   ///<  or in milliseconds

    PX14_REC_CALLBACK   pfnCallback;    ///< Optional callback
    void*               callbackData;   ///< Context data for callback

} PX14S_REC_SESSION_PARAMS;

/// Recording session progress/status
typedef struct _PX14S_REC_SESSION_PROG_tag
{
    unsigned int        struct_size;    ///< Init to struct size in bytes

    int                 status;         ///< PX14RECSTAT_*
    unsigned long       elapsed_time_ms;///< Rec time in ms
    unsigned long long  samps_recorded;
    unsigned long long  samps_to_record;
    unsigned int        xfer_samples;   ///< Transfer size in samps
    unsigned int        xfer_count;     ///< Transfer counter
    unsigned int        snapshot_counter;///< Current snapshot counter

    // Valid when status == PX14RECSTAT_ERROR
    int                 err_res;        ///< SIG_*
    char*               err_textp;      ///< Caller free via FreMemoryPX14

} PX14S_REC_SESSION_PROG;

/// Recorded data information; used with GetRecordedDataInfoPX14
typedef struct _PX14S_RECORDED_DATA_INFO_tag
{
    unsigned int        struct_size;    ///< Init to struct size in bytes

    char                boardName[16];  ///< Name of board
    unsigned int        boardSerialNum; ///< Serial number

    unsigned int        channelCount;   ///< Channel count
    unsigned int        channelNum;     ///< Channel ID; single chan data
    unsigned int        sampSizeBytes;  ///< Sample size in bytes
    unsigned int        sampSizeBits;   ///< Sample size in bits
    int                 bSignedSamples; ///< Signed or unsigned

    double              sampleRateMHz;  ///< Sampling rate in MHz
    double              inputVoltRngPP; ///< Peak-to-peak input volt range

    unsigned int        segment_size;   ///< Segment size or zero

    unsigned int        header_bytes;   ///< Size of app-specific header
    int                 bTextData;      ///< Data is text (versus binary)?
    int                 textRadix;      ///< Radix of text data (10/16)

    unsigned int        trig_offset;    ///< Relates first sample to trig
    int                 bPreTrigger;    ///< ? Pre-trigger : trigger delay

} PX14S_RECORDED_DATA_INFO;

// PX14IMP: Update marshalling in ConnectToRemoteDeviceWPX14 when updated
typedef struct _PX14S_REMOTE_CONNECT_CTXA_tag
{
    unsigned int        struct_size;        ///< Init to struct size in bytes
    unsigned int        flags;              // Currently undefined, use 0
    unsigned short      port;
    const char*         pServerAddress;
    const char*         pApplicationName;   // Optional
    const char*         pSubServices;       // Optional

} PX14S_REMOTE_CONNECT_CTXA;

typedef struct _PX14S_REMOTE_CONNECT_CTXW_tag
{
    unsigned int        struct_size;        ///< Init to struct size in bytes
    unsigned int        flags;              // Currently undefined, use 0
    unsigned short      port;
    const wchar_t*      pServerAddress;
    const wchar_t*      pApplicationName;   // Optional
    const wchar_t*      pSubServices;       // Optional

} PX14S_REMOTE_CONNECT_CTXW;


// Macros -------------------------------------------------------------- //

// Generic-text mapping macros ----------------------------------------- //

//  For PX14400 library functions that take string parameters, both ASCII
//   and UNICODE versions may be implemented. ASCII implementations are
//   suffixed with 'APX14' and UNICODE implementations are suffixed with
//   'WPX14'. The following macros can be used for character-type agnostic
//   code. That is, for ASCII builds the ASCII versions will be used and for
//   UNICODE builds the UNICODE versions will be used.

#ifdef _UNICODE
# define GetBoardNamePX14                   GetBoardNameWPX14
# define GetErrorTextPX14                   GetErrorTextWPX14
# define GetVersionTextPX14                 GetVersionTextWPX14
# define UploadFirmwarePX14                 UploadFirmwareWPX14
# define QueryFirmwareVersionInfoPX14       QueryFirmwareVersionInfoWPX14
# define ExtractFirmwareNotesPX14           ExtractFirmwareNotesWPX14
# define OpenSrdcFilePX14                   OpenSrdcFileWPX14
# define SaveSrdcFilePX14                   SaveSrdcFileWPX14
# define GetSrdcItemPX14                    GetSrdcItemWPX14
# define PX14_SRDC_DOT_EXTENSION            PX14_SRDC_DOT_EXTENSIONW
# define PX14_SRDC_EXTENSION                PX14_SRDC_EXTENSIONW
# define GetRecordedDataInfoPX14            GetRecordedDataInfoWPX14
# define EnumSrdcItemsPX14                  EnumSrdcItemsWPX14
# define PX14_TIMESTAMP_FILE_DOT_EXTENSION  PX14_TIMESTAMP_FILE_DOT_EXTENSIONW
# define PX14_TIMESTAMP_FILE_EXTENSION      PX14_TIMESTAMP_FILE_EXTENSIONW
# define SaveSettingsToBufferXmlPX14        SaveSettingsToBufferXmlWPX14
# define LoadSettingsFromBufferXmlPX14      LoadSettingsFromBufferXmlWPX14
# define SaveSettingsToFileXmlPX14          SaveSettingsToFileXmlWPX14
# define LoadSettingsFromFileXmlPX14        LoadSettingsFromFileXmlWPX14
# define PX14S_REMOTE_CONNECT_CTX           PX14S_REMOTE_CONNECT_CTXW
# define ConnectToRemoteDevicePX14          ConnectToRemoteDeviceWPX14
# define ConnectToRemoteVirtualDevicePX14   ConnectToRemoteVirtualDeviceWPX14
# define GetRemoteDeviceCountPX14           GetRemoteDeviceCountWPX14
# define GetHostServerInfoPX14              GetHostServerInfoWPX14
# define DumpLibErrorPX14                   DumpLibErrorWPX14
#else
# define GetBoardNamePX14                   GetBoardNameAPX14
# define GetErrorTextPX14                   GetErrorTextAPX14
# define GetVersionTextPX14                 GetVersionTextAPX14
# define UploadFirmwarePX14                 UploadFirmwareAPX14
# define QueryFirmwareVersionInfoPX14       QueryFirmwareVersionInfoAPX14
# define ExtractFirmwareNotesPX14           ExtractFirmwareNotesAPX14
# define OpenSrdcFilePX14                   OpenSrdcFileAPX14
# define SaveSrdcFilePX14                   SaveSrdcFileAPX14
# define GetSrdcItemPX14                    GetSrdcItemAPX14
# define PX14_SRDC_DOT_EXTENSION            PX14_SRDC_DOT_EXTENSIONA
# define PX14_SRDC_EXTENSION                PX14_SRDC_EXTENSIONA
# define GetRecordedDataInfoPX14            GetRecordedDataInfoAPX14
# define EnumSrdcItemsPX14                  EnumSrdcItemsAPX14
# define PX14_TIMESTAMP_FILE_DOT_EXTENSION  PX14_TIMESTAMP_FILE_DOT_EXTENSIONA
# define PX14_TIMESTAMP_FILE_EXTENSION      PX14_TIMESTAMP_FILE_EXTENSIONA
# define SaveSettingsToBufferXmlPX14        SaveSettingsToBufferXmlAPX14
# define LoadSettingsFromBufferXmlPX14      LoadSettingsFromBufferXmlAPX14
# define SaveSettingsToFileXmlPX14          SaveSettingsToFileXmlAPX14
# define LoadSettingsFromFileXmlPX14        LoadSettingsFromFileXmlAPX14
# define PX14S_REMOTE_CONNECT_CTX           PX14S_REMOTE_CONNECT_CTXA
# define ConnectToRemoteDevicePX14          ConnectToRemoteDeviceAPX14
# define ConnectToRemoteVirtualDevicePX14   ConnectToRemoteVirtualDeviceAPX14
# define GetRemoteDeviceCountPX14           GetRemoteDeviceCountAPX14
# define GetHostServerInfoPX14              GetHostServerInfoAPX14
# define DumpLibErrorPX14                   DumpLibErrorAPX14
#endif

// --- Private, undocumented functions --- //

// Normal users should not be using these functions; they may be removed
//  or altered at any time.

// Normal users should use ReadConfigEepromPX14
PX14API GetEepromDataPX14 (HPX14 hBrd, unsigned int eeprom_addr,
                           unsigned short* eeprom_datap);
// Normal users should use WriteConfigEepromPX14
PX14API SetEepromDataPX14 (HPX14 hBrd, unsigned int eeprom_addr,
                           unsigned short eeprom_data);
// Recalculate and set configuration EEPROM checksum
PX14API ResetEepromChecksumPX14 (HPX14 hBrd);

// Ask driver to generate a report using kernel traces
PX14API RequestDbgReportPX14 (HPX14 hBrd, int report_type);

// Write a device register
PX14API WriteDeviceRegPX14 (HPX14 hBrd, unsigned int reg_idx,
                            unsigned int mask, unsigned int val,
                            unsigned int pre_delay_us _PX14_DEF(0),
                            unsigned int post_delay_us _PX14_DEF(0),
                            unsigned int reg_set _PX14_DEF(0));
// Read a device register
PX14API ReadDeviceRegPX14 (HPX14 hBrd, unsigned int reg_idx,
                           unsigned int* reg_valp _PX14_DEF(NULL),
                       unsigned int read_how _PX14_DEF(PX14REGREAD_DEFAULT),
                           unsigned int reg_set _PX14_DEF(0));

// Write to one of clock generator register
PX14API WriteClockGenRegPX14 (HPX14 hBrd, unsigned int reg_addr,
                              unsigned char mask, unsigned char val,
                              unsigned int log_idx, unsigned int byte_idx,
                              unsigned int pre_delay_us _PX14_DEF(0),
                              unsigned int post_delay_us _PX14_DEF(0),
                              int bUpdateRegs _PX14_DEF(0));

// Read one of the clock generator (logical) registers
PX14API ReadClockGenRegPX14 (HPX14 hBrd, unsigned int reg_addr,
                             unsigned int log_idx, unsigned int byte_idx,
                             unsigned char* reg_valp _PX14_DEF(NULL),
                      unsigned int read_how _PX14_DEF(PX14REGREAD_DEFAULT));

// Write DC-offset DAC
PX14API WriteDacRegPX14 (HPX14 hBrd, unsigned int reg_data,
						 unsigned int reg_addr, unsigned int command);


// Write all device registers with currently cached values
PX14API WriteAllDeviceRegistersPX14 (HPX14 hBrd);
// Read all device registers from hardware; updates all register caches
PX14API ReadAllDeviceRegistersPX14 (HPX14 hBrd);

// Initialize clock generator device
PX14API InitializeClockGeneratorPX14 (HPX14 hBrd);
// Resynchronize clock component outputs
PX14API ResyncClockOutputsPX14 (HPX14 hBrd);

// Enable/disable EEPROM protection
PX14API EnableEepromProtectionPX14 (HPX14 hBrd, int bEnable);
// Rescan EEPROM for hardware config changes
PX14API RefreshDriverHwConfigPX14 (HPX14 hBrd);

// Obtains the version of the previous PX14400 PCI firmware
PX14API GetPrevPciFirmwareVerPX14 (HPX14 hBrd, unsigned long long* verp);
// Obtain the given channels hardware implementation (PX14CHANIMP_*)
PX14API GetChannelImplementationPX14 (HPX14 hBrd, unsigned int chan_idx _PX14_DEF(0));
// Obtain filter type installed for the given channel (PX14CHANFILTER_*)
PX14API GetChannelFilterTypePX14 (HPX14 hBrd, unsigned int chan_idx _PX14_DEF(0));

// Obtains the features implemented on given PX14400
PX14API GetBoardFeaturesPX14 (HPX14 hBrd);

// Raw DMA transfer request; independent of operating mode
PX14API DmaTransferPX14 (HPX14 hBrd,
                         void* bufp,
                         unsigned int bytes,
                         int bRead _PX14_DEF(1),
                         int bAsynch _PX14_DEF(0));

// Transfer data from a buffer to a file using
PX14API _DumpRawDataPX14 (HPX14 hBrd,
                          px14_sample_t* bufp,
                          unsigned samples,
                          PX14S_FILE_WRITE_PARAMS* paramsp);


// Waits for phase lock loop to lock
PX14API _WaitForPllLockPX14 (HPX14 hBrd,
                             unsigned int max_wait_ms _PX14_DEF(7000));

PX14API GetJtagIdCodesAPX14 (HPX14 hBrd, char** bufpp);

// Set manual DCM disable state
PX14API SetManualDcmDisablePX14 (HPX14 hBrd, unsigned int bDisable);
// Get manual DCM disable state
PX14API GetManualDcmDisablePX14 (HPX14 hBrd, int bFromHardware _PX14_DEF(0));

// Troubleshooting: disable internal clock PLL usage
PX14API _SetPllDisablePX14 (HPX14 hBrd, int bDisable);
PX14API _GetPllDisablePX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Defer waiting for PLL lock until later
PX14API _DeferPllLockCheckPX14 (HPX14 hBrd, int bEnable);

// Library exports ----------------------------------------------------- //

// --- Device connection/management --- //

// Obtains number of PX14400 or PX12500 devices in the local system
PX14API GetDeviceCountPX14();

// Obtain a handle to a local PX14400 device
PX14API ConnectToDevicePX14 (HPX14* phDev, unsigned int brdNum);
// Closes a PX14400 device handle
PX14API DisconnectFromDevicePX14 (HPX14 hBrd);

// Obtain a handle to a virtual (fake) PX14400 device
PX14API ConnectToVirtualDevicePX14 (HPX14* phDev,
                                    unsigned int serialNum,
                                    unsigned int brdNum);

// Determines if a given PX14400 device handle is for a virtual device
PX14API IsDeviceVirtualPX14 (HPX14 hBrd);
// Determines if the given PX14400 device handle is connected to a device
PX14API IsHandleValidPX14 (HPX14 hBrd);
// Determines if a given PX14400 device handle is for a remote device
PX14API IsDeviceRemotePX14 (HPX14 hBrd);

// Duplicate a PX14400 device handle
PX14API DuplicateHandlePX14 (HPX14 hBrd, HPX14* phNew);

// Obtain the serial number of the PX14400 connected to the given handle
PX14API GetSerialNumberPX14 (HPX14 hBrd, unsigned int* snp);
// Obtain the ordinal number of the PX14400 connected to the given handle
PX14API GetOrdinalNumberPX14 (HPX14 hBrd, unsigned int* onp);
// Obtain board revision and/or sub-revision
PX14API GetBoardRevisionPX14 (HPX14 hBrd, unsigned int* revp, unsigned int* sub_revp);

// Copy hardware settings from another PX14400 device
PX14API CopyHardwareSettingsPX14 (HPX14 hBrdDst, HPX14 hBrdSrc);

// Obtain user-friendly name for given board (ASCII)
PX14API GetBoardNameAPX14 (HPX14 hBrd, char** bufpp,
                           int flags _PX14_DEF(0));
// Obtain user-friendly name for given board (UNICODE)
PX14API GetBoardNameWPX14 (HPX14 hBrd, wchar_t** bufpp,
                           int flags _PX14_DEF(0));

// --- Remote device routines --- //

// Call once per application instance to initialize sockets implementation
PX14API SocketsInitPX14();

// Sockets implementation cleanup; call if your app calls InitSocketsPX14
PX14API SocketsCleanupPX14();

// Obtain count (and optionally serial numbers) of remote PX14400 devices
PX14API GetRemoteDeviceCountAPX14 (const char* server_addrp,
               unsigned short port _PX14_DEF (PX14_SERVER_PREFERRED_PORT),
                               unsigned int** sn_bufpp _PX14_DEF (NULL));

// Obtain count (and optionally serial numbers) of remote PX14400 devices
PX14API GetRemoteDeviceCountWPX14 (const wchar_t* server_addrp,
               unsigned short port _PX14_DEF (PX14_SERVER_PREFERRED_PORT),
                               unsigned int** sn_bufpp _PX14_DEF (NULL));

// Obtain a handle to a PX14400 residing on another computer (ASCII)
PX14API ConnectToRemoteDeviceAPX14 (HPX14* phDev, unsigned int brdNum,
                                    PX14S_REMOTE_CONNECT_CTXA* ctxp);
// Obtain a handle to a PX14400 residing on another computer (UNICODE)
PX14API ConnectToRemoteDeviceWPX14 (HPX14* phDev, unsigned int brdNum,
                                    PX14S_REMOTE_CONNECT_CTXW* ctxp);

// Obtain a handle to a virtual (fake) PX14400 device on another computer
PX14API ConnectToRemoteVirtualDeviceAPX14 (HPX14* phDev,
                                           unsigned int brdNum,
                                           unsigned int ordNum,
                                           PX14S_REMOTE_CONNECT_CTXA* ctxp);

// Obtain a handle to a virtual (fake) PX14400 device on another computer
PX14API ConnectToRemoteVirtualDeviceWPX14 (HPX14* phDev,
                                           unsigned int brdNum,
                                           unsigned int ordNum,
                                           PX14S_REMOTE_CONNECT_CTXW* ctxp);

// Obtain socket of the underlying connection for remote PX14400
PX14API GetServiceSocketPX14 (HPX14 hBrd, px14_socket_t* sockp);

// Obtain remote server information (ASCII)
PX14API GetHostServerInfoAPX14 (HPX14 hBrd, char** server_addrpp,
                                unsigned short* portp);
// Obtain remote server information (UNICODE)
PX14API GetHostServerInfoWPX14 (HPX14 hBrd, wchar_t** server_addrpp,
                                unsigned short* portp);


// Send a request to a remote PX14400 service
PX14API SendServiceRequestPX14 (HPX14 hBrd,
                                const void* svc_reqp, int req_bytes,
                                void** responsepp,
                                unsigned int timeoutMs _PX14_DEF (3000),
                                unsigned int flags _PX14_DEF(0));

// Free a response from a previous remote service request
PX14API FreeServiceResponsePX14 (void* bufp);

// --- Acquisition routines --- //

// Acquire data to PX14400 RAM
PX14API AcquireToBoardRamPX14 (HPX14 hBrd,
                               unsigned int samp_start,
                               unsigned int samp_count,
                               unsigned int timeout_ms _PX14_DEF(0),
                               int bAsynchronous _PX14_DEF(0));

// Acquire data to Signatec Auxiliary Bus (SAB)
PX14API AcquireToSabPX14 (HPX14 hBrd, unsigned int samp_count,
                          int bRamBuffered _PX14_DEF(0),
                          unsigned int timeout_ms _PX14_DEF(0),
                          int bAsynchronous _PX14_DEF(0));

// Begin a buffered PCI acquisition
PX14API BeginBufferedPciAcquisitionPX14 (HPX14 hBrd,
                      unsigned int samp_count _PX14_DEF(PX14_FREE_RUN));

// Obtain fresh PCI acquisition data directly to a DMA buffer
PX14API GetPciAcquisitionDataFastPX14 (HPX14 hBrd,
                                       unsigned int samples,
                                       px14_sample_t* dma_bufp,
                                       int bAsynchronous _PX14_DEF(0));
// End a buffered PCI acquisition
PX14API EndBufferedPciAcquisitionPX14 (HPX14 hBrd);

// Obtain fresh PCI acquisition data given normal, non-DMA buffer
PX14API GetPciAcquisitionDataBufPX14 (HPX14 hBrd,
                                      unsigned int samples,
                                      px14_sample_t* heap_bufp,
                                      int bAsynchronous _PX14_DEF(0));

// Determine if RAM/SAB acquisition is currently in progress
PX14API IsAcquisitionInProgressPX14 (HPX14 hBrd);
// Wait for a RAM/SAB data acquisition to complete with optional timeout
PX14API WaitForAcquisitionCompletePX14 (HPX14 hBrd,
                                  unsigned int timeout_ms _PX14_DEF(0));

// --- DMA buffer routines --- //

//
//  -- Dynamic DMA buffer allocation: Buffers allocated on request
//

// Allocate a DMA buffer
PX14API AllocateDmaBufferPX14 (HPX14 hBrd, unsigned int samples,
                               px14_sample_t** bufpp);

// Free a DMA buffer
PX14API FreeDmaBufferPX14 (HPX14 hBrd, px14_sample_t* bufp);

// Ensures that the library managed utility DMA buffer is of the given size
PX14API EnsureUtilityDmaBufferPX14 (HPX14 hBrd, unsigned int sample_count);
// Frees the utility buffer associated with the given PX14400 handle
PX14API FreeUtilityDmaBufferPX14 (HPX14 hBrd);
// Get the library managed utility DMA buffer
PX14API GetUtilityDmaBufferPX14 (HPX14 hBrd, px14_sample_t** bufpp,
                                 unsigned int* buf_samplesp _PX14_DEF(NULL));

// Ensures that the library managed utility DMA buffer #2 is of the given size
PX14API EnsureUtilityDmaBuffer2PX14 (HPX14 hBrd, unsigned int sample_count);
// Frees the utility buffer #2 associated with the given PX14400 handle
PX14API FreeUtilityDmaBuffer2PX14 (HPX14 hBrd);
// Get the library managed utility DMA buffer #2
PX14API GetUtilityDmaBuffer2PX14 (HPX14 hBrd, px14_sample_t** bufpp,
                                  unsigned int* buf_samplesp _PX14_DEF(NULL));

//
//  -- Boot-buffer DMA allocation: Static buffers allocated at boot time
//

// Programmatically obtain maximum boot buffer count for given device
PX14API BootBufGetMaxCountPX14 (HPX14 hBrd);

// Check out boot buffer with given index
PX14API BootBufCheckOutPX14 (HPX14           hBrd,
							 unsigned short  buf_idx,			// 0-based index of buffer
							 px14_sample_t** dma_bufpp,			// Address to write buffer address
							 unsigned int*   pbuf_samples);		// Address to write buffer size in samples

// Check in boot buffer
PX14API BootBufCheckInPX14 (HPX14 hBrd, px14_sample_t* dma_bufp);

// Query boot buffer status
PX14API BootBufQueryPX14 (HPX14          hBrd,
						  unsigned short buf_idx,				// 0-based index of buffer
						  int*           pchecked_out,			// Address to write check-out status
						  unsigned int*  pbuf_samples);			// Address to write buffer size in samples

// Set boot buffer configuration; set requested boot buffer size
PX14API BootBufCfgSetPX14 (HPX14 hBrd, unsigned short buf_idx, unsigned int buf_samples);
// Get boot buffer configuration; get requested boot buffer size
PX14API BootBufCfgGetPX14 (HPX14 hBrd, unsigned short buf_idx, unsigned int* pbuf_samples);
// Request driver to reallocate boot buffers now
PX14API BootBufReallocNowPX14 (HPX14 hBrd);

//
//  -- DMA buffer chain allocation: Aggregated list of DMA buffers
//

/// DMA buffer descriptor node; used for chained buffers allocation
typedef struct _PX14S_BUFNODE_tag
{
	px14_sample_t*		bufp;			///< Address of DMA buffer
	unsigned int		buf_samples;	///< Size of buffer in samples

} PX14S_BUFNODE;

// -- Allocate DMA buffer chain flags (PX14DMACHAINF_*)
/// Do not fail if total requested amount cannot be allocated; return what was allocated
#define PX14DMACHAINF_LESS_IS_OKAY			0x00000001
/// Allocate a utility DMA buffer chain; freed when handle is closed
#define PX14DMACHAINF_UTILITY_CHAIN			0x00010000

// Allocate an aggregated chain of DMA buffers of a total size
PX14API AllocateDmaBufferChainPX14 (HPX14 hBrd, unsigned int total_samples,
									PX14S_BUFNODE** nodepp,
									int flags _PX14_DEF(0),
									int* buf_countp _PX14_DEF(NULL));

// Free a DMA buffer chain allocated with AllocateDmaBufferChainPX14
PX14API FreeDmaBufferChainPX14 (HPX14 hBrd, PX14S_BUFNODE* nodep);

// Obtain utility DMA buffer chain and/or aggregate length
PX14API GetUtilityDmaBufferChainPX14 (HPX14 hBrd,
									  PX14S_BUFNODE** nodepp,
									  unsigned int* total_samplesp _PX14_DEF(NULL));


// --- Data transfer routines --- //

// Transfer data in PX14400 RAM to a DMA buffer on the host PC
PX14API ReadSampleRamFastPX14 (HPX14 hBrd,
                               unsigned int sample_start,
                               unsigned int sample_count,
                               px14_sample_t* dma_bufp,
                               int bAsynchronous _PX14_DEF(0));

// Waits for data to be written into RAM; see manual before using this
PX14API WaitForRamWriteCompletionPX14 (HPX14 hBrd, unsigned int timeout_ms _PX14_DEF(0));

// Determine if transfer is currently in progress; for asynch transfers
PX14API IsTransferInProgressPX14 (HPX14 hBrd);
// Wait (sleep) for an asyncyh data xfer to complete with optional timeout
PX14API WaitForTransferCompletePX14 (HPX14 hBrd, unsigned int timeout_ms _PX14_DEF(0));

// Xfer data in PX14400 RAM to a buffer on the PC; any boundary or alignment
PX14API ReadSampleRamBufPX14 (HPX14 hBrd,
                              unsigned int sample_start,
                              unsigned int sample_count,
                              px14_sample_t* bufp,
                              int bAsynchronous _PX14_DEF(0));

// Xfer and deinterleave data in brd RAM to host PC; any boundary/alignment
PX14API ReadSampleRamDualChannelBufPX14 (HPX14 hBrd,
                                         unsigned int sample_start,
                                         unsigned int sample_count,
                                         px14_sample_t* buf_ch1p,
                                         px14_sample_t* buf_ch2p,
                                        int bAsynchronous _PX14_DEF(0));

// Transfer data in PX14400 RAM to local file on host PC
PX14API ReadSampleRamFileFastPX14 (HPX14 hBrd,
                                   unsigned int sample_start,
                                   unsigned int sample_count,
                                   px14_sample_t* dma_bufp,
                                   unsigned int dma_buf_samples,
                                   PX14S_FILE_WRITE_PARAMS* paramsp);

// Transfer data in PX14400 RAM to local file on host PC
PX14API ReadSampleRamFileBufPX14 (HPX14 hBrd,
                                  unsigned int sample_start,
                                  unsigned int sample_count,
                                  PX14S_FILE_WRITE_PARAMS* paramsp);


// Transfer data in PX14400 RAM to the SAB bus
PX14API TransferSampleRamToSabPX14 (HPX14 hBrd,
                                    unsigned int sample_start,
                                    unsigned int sample_count,
                                   unsigned int timeout_ms _PX14_DEF(0),
                                    int bAsynchronous _PX14_DEF(0));

// De-interleave dual channel data into separate buffers
PX14API DeInterleaveDataPX14 (const px14_sample_t* srcp,
                              unsigned int samples_in,
                              px14_sample_t* dst_ch1p,
                              px14_sample_t* dst_ch2p);

// (Re)interleave dual channel data into a single buffer
PX14API InterleaveDataPX14 (const px14_sample_t* src_ch1p,
                            const px14_sample_t* src_ch2p,
                            unsigned int samps_per_chan,
                            px14_sample_t* dstp);

// --- Acquisition Recording Session functions --- //

/// A handle to a PX14400 recording session
typedef struct _px14rsh_ { int reserved; }* HPX14RECORDING;

/// An invalid HPX14RECORDING value
#define INVALID_HPX14RECORDING_HANDLE		NULL

// Create a PX14400 acquisition recording session
PX14API CreateRecordingSessionPX14 (HPX14 hBrd,
                                    PX14S_REC_SESSION_PARAMS* rec_paramsp,
                                    HPX14RECORDING* handlep);

// Arm device for recording
PX14API ArmRecordingSessionPX14 (HPX14RECORDING hRec);

// Obtain progress/status for current recording session
PX14API GetRecordingSessionProgressPX14 (HPX14RECORDING hRec,
                                         PX14S_REC_SESSION_PROG* progp,
                       unsigned flags _PX14_DEF(PX14RECPROGF__DEFAULT));

// Obtain data snapshot from current recording
PX14API GetRecordingSnapshotPX14 (HPX14RECORDING hRec,
                                  px14_sample_t* bufp,
                                  unsigned int samples,
                                  unsigned int* samples_gotp,
                                  unsigned int* ss_countp);

// Abort the current recording session
PX14API AbortRecordingSessionPX14 (HPX14RECORDING hRec);

// Delete a PX14400 recording session
PX14API DeleteRecordingSessionPX14 (HPX14RECORDING hRec);

// Obtain recording session output flags; only valid after recording stopped
PX14API GetRecordingSessionOutFlagsPX14 (HPX14RECORDING hRec,
                                         unsigned int* flagsp);

// --- Timestamp routines --- //

/// Extension used for PX14400 timestamp files (.px14ts)
#define PX14_TIMESTAMP_FILE_DOT_EXTENSIONA	".px14ts"
/// Extension used for PX14400 timestamp files (.px14ts)
#define PX14_TIMESTAMP_FILE_DOT_EXTENSIONW	L".px14ts"
/// Extension used for PX14400 timestamp files (.px14ts)
#define PX14_TIMESTAMP_FILE_EXTENSIONA		"px14ts"
/// Extension used for PX14400 timestamp files (.px14ts)
#define PX14_TIMESTAMP_FILE_EXTENSIONW		L"px14ts"


// Manually reset the timestamp counter
PX14API ResetTimestampCounterPX14 (HPX14 hBrd);

// Read Timestamp Overflow Flag from PX14400 hardware
PX14API GetTimestampOverflowFlagPX14 (HPX14 hBrd);

// Determine if any timestamps are available in timestamp FIFO
PX14API GetTimestampAvailabilityPX14 (HPX14 hBrd);

// Set the timestamp mode; determines how timestamps are recorded
PX14API SetTimestampModePX14 (HPX14 hBrd, unsigned int val);

// Get the timestamp mode; determines how timestamps are recorded
PX14API GetTimestampModePX14 (HPX14 hBrd,
                              int bFromCache _PX14_DEF(1));

// Set timestamp counter mode; determines how timestamp counter increments
PX14API SetTimestampCounterModePX14 (HPX14 hBrd, unsigned int val);

// Get timestamp counter mode; determines how timestamp counter increments
PX14API GetTimestampCounterModePX14 (HPX14 hBrd,
                                   int bFromCache _PX14_DEF(1));

// Read timestamp data from PX14400 timestamp FIFO
PX14API ReadTimestampDataPX14 (HPX14 hBrd, px14_timestamp_t* bufp,
                               unsigned int ts_count,
                               unsigned int* ts_readp,
                               unsigned int flags _PX14_DEF(0),
                               unsigned int timeout_ms _PX14_DEF(0),
                               unsigned int* flags_outp _PX14_DEF(NULL));

// Obtain size of PX14400 timestamp FIFO, in timestamp elements
PX14API GetTimestampFifoDepthPX14 (HPX14 hBrd, unsigned int* ts_elementsp);

// Reset the timestamp FIFO; all content lost
PX14API ResetTimestampFifoPX14 (HPX14 hBrd);

// --- Hardware configuration functions --- //

// Get size of PX14400 sample RAM in samples
PX14API GetSampleRamSizePX14 (HPX14 hBrd, unsigned int* samplesp);

// Get type of SAB FPGA present on PX14400; (PX14SABFPGA_*)
PX14API GetSabFpgaTypePX14 (HPX14 hBrd);

// Obtain slave [2-4] hardware configuration
PX14API GetSlave234TypePX14 (HPX14 hBrd);

// --- Hardware settings functions --- //

// Determine if the PX14400 is idle; in Standby or Off mode
PX14API InIdleModePX14 (HPX14 hBrd);

// Determine if the PX14400 is acquiring (or waiting to acquire)
PX14API InAcquisitionModePX14 (HPX14 hBrd);

// Set the PX14400's power-down override
PX14API SetPowerDownOverridePX14 (HPX14 hBrd, unsigned int bEnable);
// Get the PX14400's power-down override
PX14API GetPowerDownOverridePX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set the PX14400's operating mode
PX14API SetOperatingModePX14 (HPX14 hBrd, unsigned int val);
// Get the PX14400's operating mode
PX14API GetOperatingModePX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set the PX14400's segment size for use with Segmented triggering mode
PX14API SetSegmentSizePX14 (HPX14 hBrd, unsigned int val);
// Get the PX14400's segment size for use with Segmented triggering mode
PX14API GetSegmentSizePX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set the PX14400's total sample count; defines length of acq/xfer
PX14API SetSampleCountPX14 (HPX14 hBrd, unsigned int val);
// Get the PX14400's total sample count; defines length of acq/xfer
PX14API GetSampleCountPX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set the PX14400's start sample; defines start of acq/xfer
PX14API SetStartSamplePX14 (HPX14 hBrd, unsigned int val);
// Get the PX14400's start sample; defines start of acq/xfer
PX14API GetStartSamplePX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set trigger delay samples; count of samples to skip after trigger
PX14API SetTriggerDelaySamplesPX14 (HPX14 hBrd, unsigned int val);
// Get trigger delay samples; count of samples to skip after trigger
PX14API GetTriggerDelaySamplesPX14 (HPX14 hBrd,int bFromCache _PX14_DEF(1));

// Set pre-trigger sample count; count of samples to keep prior to trigger
PX14API SetPreTriggerSamplesPX14 (HPX14 hBrd, unsigned int val);
// Get pre-trigger sample count; count of samples to keep prior to trigger
PX14API GetPreTriggerSamplesPX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set trigger A level; defines threshold for an internal trigger event
PX14API SetTriggerLevelAPX14 (HPX14 hBrd, unsigned int val);
// Get trigger A level; defines threshold for an internal trigger event
PX14API GetTriggerLevelAPX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set trigger B level; defines threshold for an internal trigger event
PX14API SetTriggerLevelBPX14 (HPX14 hBrd, unsigned int val);
// Get trigger B level; defines threshold for an internal trigger event
PX14API GetTriggerLevelBPX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set triggering mode; relates trigger events to how data is saved
PX14API SetTriggerModePX14 (HPX14 hBrd, unsigned int val);
// Get triggering mode; relates trigger events to how data is saved
PX14API GetTriggerModePX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

//Set Trigger Selection
PX14API SetTriggerSelectionPX14 (HPX14 hBrd, int val);
//Get Trigger Selection
PX14API GetTriggerSelectionPX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

//Set Trigger Hysteresis
PX14API SetTriggerHysteresisPX14 (HPX14 hBrd, int val);
//Get Trigger Hysteresis
PX14API GetTriggerHysteresisPX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

//TODPT mode (Trigger occured during pre-trigger)
PX14API SetTODPT_Enable(HPX14 hBrd, int val);
PX14API GetTODPT_Enable(HPX14 hBrd, int val);

PX14API GetFPGATemperaturePX14(HPX14 hBrd, int bFromCache);

// Set trigger source; defines where trigger events originate
PX14API SetTriggerSourcePX14 (HPX14 hBrd, unsigned int val);
// Get trigger source; defines where trigger events originate
PX14API GetTriggerSourcePX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set trigger A direction; defines the direction that defines a trigger
PX14API SetTriggerDirectionAPX14 (HPX14 hBrd, unsigned int val);
// Get trigger A direction; defines the direction that defines a trigger
PX14API GetTriggerDirectionAPX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set trigger B direction; defines the direction that defines a trigger
PX14API SetTriggerDirectionBPX14 (HPX14 hBrd, unsigned int val);
// Get trigger B direction; defines the direction that defines a trigger
PX14API GetTriggerDirectionBPX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set external trigger direction
PX14API SetTriggerDirectionExtPX14 (HPX14 hBrd, unsigned int val);
// Get external trigger direction
PX14API GetTriggerDirectionExtPX14 (HPX14 hBrd,int bFromCache _PX14_DEF(1));

// Issue a software-generated trigger event to a PX14400
PX14API IssueSoftwareTriggerPX14 (HPX14 hBrd);

// AC-coupled devices: Set channel 1 input voltage range; (PX14VOLTRNG_*)
PX14API SetInputVoltRangeCh1PX14 (HPX14 hBrd, unsigned int val);
// AC-coupled devices: Get channel 1 input voltage range; (PX14VOLTRNG_*)
PX14API GetInputVoltRangeCh1PX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// AC-coupled devices: Set channel 2 input voltage range; (PX14VOLTRNG_*)
PX14API SetInputVoltRangeCh2PX14 (HPX14 hBrd, unsigned int val);
// AC-coupled devices: Get channel 2 input voltage range; (PX14VOLTRNG_*)
PX14API GetInputVoltRangeCh2PX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Obtain actual peak-to-peak voltage for given PX14VOLTRNG_*
PX14API GetInputVoltRangeFromSettingPX14 (int vr_sel, double* vpp);

// AC-coupled devices: Obtain actual peak-to-peak voltage for channel 1 input
PX14API GetInputVoltageRangeVoltsCh1PX14 (HPX14 hBrd, double* voltsp,
                                          int bFromCache _PX14_DEF(1));
// AC-coupled devices: Obtain actual peak-to-peak voltage for channel 2 input
PX14API GetInputVoltageRangeVoltsCh2PX14 (HPX14 hBrd, double* voltsp,
                                          int bFromCache _PX14_DEF(1));

// AC-coupled devices: Select voltage range for channel 1 and/or 2
PX14API SelectInputVoltRangePX14 (HPX14 hBrd, double vr_pp,
                                  int bCh1 _PX14_DEF(PX14_TRUE),
                                  int bCh2 _PX14_DEF(PX14_TRUE),
                                  int sel_how _PX14_DEF(PX14VRSEL_NOT_LT));

// PX14400D devices: Set input gain level (PX14DGAIN_*)
PX14API SetInputGainLevelDcPX14 (HPX14 hBrd, unsigned int val);
// PX14400D devices: Get input gain level (PX14DGAIN_*)
PX14API GetInputGainLevelDcPX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set the ADC internal clock reference
PX14API SetInternalAdcClockReferencePX14 (HPX14 hBrd, unsigned int val);
// Get the ADC internal clock reference
PX14API GetInternalAdcClockReferencePX14 (HPX14 hBrd,
                                          int bFromCache _PX14_DEF(1));

// Set the assumed external clock rate
PX14API SetExternalClockRatePX14 (HPX14 hBrd, double dRateMHz);
// Get the assumed external clock rate
PX14API GetExternalClockRatePX14 (HPX14 hBrd, double* ratep,
                                  int bFromCache _PX14_DEF(1));

// Set the ADC clock rate; only applies to internal 1.5 GHz VCO
PX14API SetInternalAdcClockRatePX14 (HPX14 hBrd, double dRateMHz);
// Get the ADC clock rate
PX14API GetInternalAdcClockRatePX14 (HPX14 hBrd, double* ratep,
                                     int bFromCache _PX14_DEF(1));

// Set the PX14400's source ADC clock
PX14API SetAdcClockSourcePX14 (HPX14 hBrd, unsigned int val);
// Get the PX14400's source ADC clock
PX14API GetAdcClockSourcePX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set the PX14400's external clock divider values
PX14API SetExtClockDividersPX14 (HPX14 hBrd,
                                 unsigned int div1, unsigned int div2);
PX14API SetExtClockDivider1PX14 (HPX14 hBrd, unsigned int div1);
PX14API SetExtClockDivider2PX14 (HPX14 hBrd, unsigned int div2);

// Get the PX14400's #1 external clock divider value
PX14API GetExtClockDivider1PX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));
// Get the PX14400's #2 external clock divider value
PX14API GetExtClockDivider2PX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set the PX14400's post-ADC clock divider; division by dropping samples
PX14API SetPostAdcClockDividerPX14 (HPX14 hBrd, unsigned int val);
// Get the PX14400's post-ADC clock divider; division by dropping samples
PX14API GetPostAdcClockDividerPX14 (HPX14 hBrd,int bFromCache _PX14_DEF(1));

// Obtain effective acquisition rate considering post-ADC division
PX14API GetEffectiveAcqRatePX14 (HPX14 hBrd, double* pRateMHz);
// Obtain the actual ADC acquisition rate
PX14API GetActualAdcAcqRatePX14 (HPX14 hBrd, double* pRateMHz);

// Set master/slave configuration; slaves are clock/trig synched with master
PX14API SetMasterSlaveConfigurationPX14 (HPX14 hBrd, unsigned int val);
// Get master/slave configuration; slaves are clock/trig synched with master
PX14API GetMasterSlaveConfigurationPX14 (HPX14 hBrd,
                                         int bFromCache _PX14_DEF(1));

// Set the digital output mode; determines what is output on digital output
PX14API SetDigitalIoModePX14 (HPX14 hBrd, unsigned int val);
// Get the digital output mode; determines what is output on digital output
PX14API GetDigitalIoModePX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Enable/disable the PX14400's digital output
PX14API SetDigitalIoEnablePX14 (HPX14 hBrd, unsigned int bEnable);
// Get state of PX14400's digital output enable
PX14API GetDigitalIoEnablePX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set active channels; defines which channels are digitized
PX14API SetActiveChannelsPX14 (HPX14 hBrd, unsigned int val);
// Get active channels; defines which channels are digitized
PX14API GetActiveChannelsPX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// - Alternate API for active channel selection; uses a generic bitmask
//   instead of a device specific encoding (PX14CHANNEL_*). Bitmask
//   interpretation: 0x1 = Channel 1, 0x2 = Channel 2, 0x3 = Dual channel

// Set active channel mask; defines which channels are digitized
PX14API SetActiveChannelsMaskPX14 (HPX14 hBrd, unsigned int chan_mask);
// Get active channel mask; defines which channels are digitized
PX14API GetActiveChannelMaskPX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Returns non-zero if given active channel mask is supported by the device
PX14API IsActiveChannelMaskSupportedPX14 (HPX14 hBrd, unsigned int chan_mask);
// Returns the absolute 0-based channel index from the relative channel index of a particular channel mask
PX14API GetAbsChanIdxFromChanMaskPX14 (int chan_mask, int rel_idx);
// Returns the relative 0-based channel index from the absolulte channel index of a particular channel mask
PX14API GetRelChanIdxFromChanMaskPX14 (int chan_mask, int abs_idx);
// Returns the number of channels active in the given channel mask
PX14API GetChanCountFromChanMaskPX14 (int chan_mask);

// Set FPGA processing enable
PX14API SetBoardProcessingEnablePX14 (HPX14 hBrd, unsigned int bEnable);
// Get FPGA processing enable
PX14API GetBoardProcessingEnablePX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set a FPGA processing parameter value
PX14API SetBoardProcessingParamPX14 (HPX14 hBrd, unsigned short idx,
                                     unsigned short value);
// Get an FPGA processing parameter value from hardware
PX14API GetBoardProcessingParamPX14 (HPX14 hBrd, unsigned short idx,
                                     unsigned short* pVal);

// Set SAB board number; uniquely identifies a board on SAB bus
PX14API SetSabBoardNumberPX14 (HPX14 hBrd, unsigned int val);
// Get SAB board number; uniquely identifies a board on SAB bus
PX14API GetSabBoardNumberPX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set SAB configuration; defines which bus lines are used
PX14API SetSabConfigurationPX14 (HPX14 hBrd, unsigned int val);
// Get SAB configuration; defines which bus lines are used
PX14API GetSabConfigurationPX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set SAB clock; governs rate when writing to SAB bus
PX14API SetSabClockPX14 (HPX14 hBrd, unsigned int val);
// Get SAB clock; governs rate when writing to SAB bus
PX14API GetSabClockPX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set channel 1 DC offset; DC-coupled devices only
PX14API SetDcOffsetCh1PX14 (HPX14 hBrd, unsigned int val);
// Get channel 1 DC offset; DC-coupled devices only
PX14API GetDcOffsetCh1PX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set channel 2 DC offset; DC-coupled devices only
PX14API SetDcOffsetCh2PX14 (HPX14 hBrd, unsigned int val);
// Get channel 2 DC offset; DC-coupled devices only
PX14API GetDcOffsetCh2PX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Adjust DC offset for given channel such that average sample value sits a specified value
PX14API CenterSignalPX14 (HPX14 hBrd, int chan_idx,
						  px14_sample_t center_to _PX14_DEF(PX14_SAMPLE_MID_VALUE));

// Set channel 1 fine DC offset; PX14400D2 devices only
PX14API SetFineDcOffsetCh1PX14 (HPX14 hBrd, unsigned int val);
// Get channel 1 fine DC offset; PX14400D2 devices only
PX14API GetFineDcOffsetCh1PX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));

// Set channel 2 fine DC offset; PX14400D2 devices only
PX14API SetFineDcOffsetCh2PX14 (HPX14 hBrd, unsigned int val);
// Get channel 2 fine DC offset; PX14400D2 devices only
PX14API GetFineDcOffsetCh2PX14 (HPX14 hBrd, int bFromCache _PX14_DEF(1));


// --- Status register flags --- //

// Get status of the samples complete flag; set when a RAM/SAB acq. finishes
PX14API GetSamplesCompleteFlagPX14 (HPX14 hBrd);
// Get status of the FIFO full flag; use with PCIBUF acquisitions
PX14API GetFifoFullFlagPX14 (HPX14 hBrd);
// Get PLL lock status; only valid for internal clock
PX14API GetPllLockStatusPX14 (HPX14 hBrd);

// --- Clear Status flags --- //

//Clear_ACQ_IN_FIFO
PX14API ClearAcqFifoFlagPX14(HPX14 hBrd);
//Clear_DCM_LOCK3
PX14API ClearDcmLockPX14(HPX14 hBrd);

// --- Measure --- //

//Get system ADC clock measure
PX14API GetSystemInternalClockRatePX14(HPX14 hBrd);

//ACQ FIFO LATCHED
PX14API GetAcqFifoLatchedOverflowFlagPX14(HPX14 hBrd);

//ACQ DCM LOCK
PX14API GetAcqDcmLockStatusPX14(HPX14 hBrd);

//ACQ DCM LOCK Latched
PX14API GetAcqDcmLatchedLockStatusPX14(HPX14 hBrd);


// --- Device register access routines --- //

// Refresh local device register cache from driver's cache; no hardware read
PX14API RefreshLocalRegisterCachePX14 (HPX14 hBrd, int bFromHardware _PX14_DEF(0));

// Bring hardware settings up to date with current cache settings
PX14API RewriteHardwareSettingsPX14 (HPX14 hBrd);

// Restores all PX14400 settings to power up default values
PX14API SetPowerupDefaultsPX14 (HPX14 hBrd);

// --- Fixed logic parameters --- //

// The PX14400 can be loaded with one of serveral fixed-logic packages. Each
//  fixed-logic package implements a particular operation in the PX14400
//  firmware.
//
// NOTE: As of this library version, none of the fixed-logic parameters
//  are affected by a SetPowerupDefaultsPX14 operation.
//
// Fixed-logic functionality is only enabled when board processing is
//  enabled. (SetBoardProcessingEnablePX14)

// -- PX14400 Fixed Logic Package custom enumerations (PX14FLP_*)
/// Programmable decimation (C2)
#define PX14FLP_C2_DECIMATION					0x0002
/// FFT (C4)
#define PX14FLP_C4_FFT							0x0004
/// FIR filtering - single channel (C5)
#define PX14FLP_C5_FIRFILT_SINGLE_CHAN			0x0005
/// FIR filtering - dual chanel (C6)
#define PX14FLP_C6_FIRFILT_DUAL_CHAN			0x0006

// Determine if given fixed logic package is available
PX14API IsFixedLogicPackagePresentPX14 (HPX14 hBrd, unsigned short flp);

// -- Fixed logic package C2: Programmable Decimation

// -- C2 Decimation factors
#define PX14C2DECF_NONE							0	// No decimation -- NOT CURRENTLY IMPLEMENTED

#define PX14C2DECF_16X							4
#define PX14C2DECF_32X							5
#define PX14C2DECF_64X							6
#define PX14C2DECF_128X							7
#define PX14C2DECF_256X							8
#define PX14C2DECF_512X							9
#define PX14C2DECF_1024X						10
#define PX14C2DECF_2048X						11
#define PX14C2DECF_4096X						12
#define PX14C2DECF_8192X						13
#define PX14C2DECF__COUNT						14

// Set decimation factor (PX14C2DECF_*)
PX14API FLPC2_SetDecimationFactorPX14 (HPX14 hBrd, unsigned val);

// Set the NCO frequency in MHz; call after acqusition clock rate has been set
PX14API FLPC2_SetNcoFrequencyPX14 (HPX14 hBrd, double nco_freqMHz);

// -- Fixed logic package C4: FFT

// Obtain minimum/maximum supported FFT sizes
PX14API FLPC4_GetFftSizeLimitsPX14 (HPX14 hBrd, unsigned int* min_fft_sizep,
									unsigned int* max_fft_sizep);
// Set the FFT size (1024 or 2048)
PX14API FLPC4_SetFftSizePX14 (HPX14 hBrd, unsigned fft_size);
// Set the number of data points to zero-pad (appended)
PX14API FLPC4_SetZeroPadPointsPX14 (HPX14 hBrd, unsigned zero_points);
// Set the magnitude-squared enable
PX14API FLPC4_SetMagnitudeSquaredEnablePX14 (HPX14 hBrd, int bEnable);
// Load FFT window data
PX14API FLPC4_LoadWindow (HPX14 hBrd, const short* coefficients,
						  unsigned coefficient_count);

// -- Fixed logic package C5: FIR Filtering (Single channel)

// Load the 129 filter coefficients
PX14API FLPC5_LoadCoefficientsPX14 (HPX14 hBrd,
									const short* coefficients,
									int ordering _PX14_DEF(0));

// -- Fixed logic package C6: FIR Filtering (Dual channel)

// Load the 65 channel 1 filter coefficients
PX14API FLPC6_LoadCoefficientsCh1PX14 (HPX14 hBrd,
									   const short* coefficients,
									   int ordering _PX14_DEF(0));
// Load the 65 channel 2 filter coefficients
PX14API FLPC6_LoadCoefficientsCh2PX14 (HPX14 hBrd,
									   const short* coefficients,
									   int ordering _PX14_DEF(0));

// -- XML routines -- //

// Save board settings to a library allocated buffer (ASCII)
PX14API SaveSettingsToBufferXmlAPX14 (HPX14 hBrd, unsigned int flags,
                                      char** bufpp, int* buflenp);
// Save board settings to a library allocated buffer (UNICODE)
PX14API SaveSettingsToBufferXmlWPX14 (HPX14 hBrd, unsigned int flags,
                                      wchar_t** bufpp, int* buflenp);


// Load board settings from an XML buffer (ASCII)
PX14API LoadSettingsFromBufferXmlAPX14 (HPX14 hBrd, unsigned int flags,
                                        const char* bufp);
// Load board settings from an XML buffer (UNICODE)
PX14API LoadSettingsFromBufferXmlWPX14 (HPX14 hBrd, unsigned int flags,
                                        const wchar_t* bufp);

// Save board settings to an XML file (ASCII)
PX14API SaveSettingsToFileXmlAPX14 (HPX14 hBrd, unsigned int flags,
                                    const char* pathnamep,
                                    const char* encodingp _PX14_DEF("UTF-8"));
// Save board settings to an XML file (UNICODE)
PX14API SaveSettingsToFileXmlWPX14 (HPX14 hBrd, unsigned int flags,
                                    const wchar_t* pathnamep,
                                    const wchar_t* encodingp _PX14_DEF(L"UTF-8"));

// Load board settings from an XML file (ASCII)
PX14API LoadSettingsFromFileXmlAPX14 (HPX14 hBrd, unsigned int flags,
                                      const char* pathnamep);
// Load board settings from an XML file (UNICODE)
PX14API LoadSettingsFromFileXmlWPX14 (HPX14 hBrd, unsigned int flags,
                                      const wchar_t* pathnamep);

// --- Firmware uploading routines --- //

/** @brief
        Prototype for optional callback function for PX14400 firmware updating

    Special case: We sometimes need to wait for a specified amount of time
    in order for certain operations to complete. This can appear as dead
    time during the firmware update process. Firmware update shells may want
    to provide some kind of notification to the operator for these long
    waits. (They can be a couple of minutes.)

    When this callback is called with the file_cur parameter of zero, then
    the callback is a wait notification:
     If file_cur == 0 then:
      If file_total > 0 then we are about to wait for file_total ms.
      If file_total == 0 then we have just finished a wait
     If file cur != 0 then parameters are interpreted as normal (below).

    @param prog_cur
        Current progress value for the current firmware file
    @param prog_total
        Total progress counts for the current firmware file
    @param file_cur
        1-based index of current firmware file being uploaded
    @param file_total
        Total number of firmware files to be uploaded for the current
        firmware upload operation
*/
typedef void (*PX14_FW_UPLOAD_CALLBACK)(HPX14 hBrd, void* callback_ctx,
                                        int prog_cur, int prog_total,
                                        int file_cur, int file_total);

// Upload PX14400 firmware (ASCII)
PX14API UploadFirmwareAPX14 (HPX14 hBrd, const char* fw_pathnamep,
                             unsigned int flags _PX14_DEF(0),
                             unsigned int* out_flagsp _PX14_DEF(NULL),
                             PX14_FW_UPLOAD_CALLBACK callbackp _PX14_DEF(NULL),
                             void* callback_ctx _PX14_DEF(NULL));
// Upload PX14400 firmware (UNICODE)
PX14API UploadFirmwareWPX14 (HPX14 hBrd, const wchar_t* fw_pathnamep,
                             unsigned int flags _PX14_DEF(0),
                             unsigned int* out_flagsp _PX14_DEF(NULL),
                             PX14_FW_UPLOAD_CALLBACK callbackp _PX14_DEF(NULL),
                             void* callback_ctx _PX14_DEF(NULL));

// Obtain firmware version information (ASCII)
PX14API QueryFirmwareVersionInfoAPX14 (const char* fw_pathnamep,
                                       PX14S_FW_VER_INFO* infop);
// Obtain firmware version information (UNICODE)
PX14API QueryFirmwareVersionInfoWPX14 (const wchar_t* fw_pathnamep,
                                       PX14S_FW_VER_INFO* infop);

// Obtain PX14400 firmware release notes from firmware file (ASCII)
PX14API ExtractFirmwareNotesAPX14 (const char* fw_pathnamep,
                                   char** notes_pathpp,
                                   int* severityp);
// Obtain PX14400 firmware release notes from firmware file (UNICODE)
PX14API ExtractFirmwareNotesWPX14 (const wchar_t* fw_pathnamep,
                                   wchar_t** notes_pathpp,
                                   int* severityp);

// --- Signatec Recorded Data (*.srdc) file support --- //

/// Invalid SRDC file handle
#define PX14_INVALID_HPX14SRDC              NULL

/// A handle to a SRDC file
typedef struct _px14srdchs_ { int reserved; }* HPX14SRDC;

/// Extension used for Signatec Recorded Data Context (.srdc) files
#define PX14_SRDC_DOT_EXTENSIONA            ".srdc"
/// Extension used for Signatec Recorded Data Context (.srdc) files
#define PX14_SRDC_DOT_EXTENSIONW            L".srdc"
/// Extension used for Signatec Recorded Data Context (.srdc) files
#define PX14_SRDC_EXTENSIONA                "srdc"
/// Extension used for Signatec Recorded Data Context (.srdc) files
#define PX14_SRDC_EXTENSIONW                L"srdc"

// Open a new or existing .srdc file
PX14API OpenSrdcFileAPX14 (HPX14 hBrd, HPX14SRDC* handlep,
                           const char* pathnamep,
                           unsigned flags _PX14_DEF(0));
// Open a new or existing .srdc file (UNICODE)
PX14API OpenSrdcFileWPX14 (HPX14 hBrd, HPX14SRDC* handlep,
                           const wchar_t* pathnamep,
                           unsigned flags _PX14_DEF(0));

// Refresh SRDC data with current board settings; not written to file
PX14API RefreshSrdcParametersPX14 (HPX14SRDC hFile, unsigned flags _PX14_DEF(0));

// Look up SRDC item with given name; name is case-sensitive
PX14API GetSrdcItemAPX14 (HPX14SRDC hFile, const char* namep, char** valuepp);
// Look up SRDC item with given name; name is case-sensitive (UNICODE)
PX14API GetSrdcItemWPX14 (HPX14SRDC hFile, const wchar_t* namep, wchar_t** valuepp);

// Add/modify SRDC item with given name; not written to file
PX14API SetSrdcItemAPX14 (HPX14SRDC hFile, const char* namep, const char* valuep);
// Add/modify SRDC item with given name; not written to file (UNICODE)
PX14API SetSrdcItemWPX14 (HPX14SRDC hFile, const wchar_t* namep, const wchar_t* valuep);

// Write SRDC data to file
PX14API SaveSrdcFileAPX14 (HPX14SRDC hFile, const char* pathnamep);
// Write SRDC data to file (UNICODE)
PX14API SaveSrdcFileWPX14 (HPX14SRDC hFile, const wchar_t* pathnamep);

// Close given SRDC file without updating contents
PX14API CloseSrdcFilePX14 (HPX14SRDC hFile);

// Obtain information on data recorded to given file
PX14API GetRecordedDataInfoAPX14 (const char* pathnamep,
                                  PX14S_RECORDED_DATA_INFO* infop,
                                  char** operator_notespp _PX14_DEF(NULL));
// Obtain information on data recorded to given file (UNICODE)
PX14API GetRecordedDataInfoWPX14 (const wchar_t* pathnamep,
                                  PX14S_RECORDED_DATA_INFO* infop,
                                  wchar_t** operator_notespp _PX14_DEF(NULL));

// Fill in PX41S_RECORDED_DATA_INFO structure for given device
PX14API InitRecordedDataInfoPX14 (HPX14 hBrd, PX14S_RECORDED_DATA_INFO* infop);

// Obtain enumeration of all SRDC items with given constraints
PX14API EnumSrdcItemsAPX14 (HPX14SRDC hFile, char** itemspp,
                          unsigned int flags _PX14_DEF(0));

// Obtain enumeration of all SRDC items with given constraints (UNICODE)
PX14API EnumSrdcItemsWPX14 (HPX14SRDC hFile, wchar_t** itemspp,
                            unsigned int flags _PX14_DEF(0));

// Returns > 0 if SRDC data is modified
PX14API IsSrdcFileModifiedPX14 (HPX14SRDC hFile);

// --- Console application helper functions --- //

#include <stdio.h>

#if defined(__linux__) && !defined(PX14PP_NO_LINUX_CONSOLE_HELP_FUNCS)

// Equivalent of old DOS getch function
PX14API getch_PX14();
// Equivalent of old DOS kbhit function
PX14API kbhit_PX14();

#endif

// Dump library error description to given file or stream (ASCII)
PX14API DumpLibErrorAPX14 (int res, const char* pPreamble,
                           HPX14 hBrd _PX14_DEF(PX14_INVALID_HANDLE),
                           FILE* filp _PX14_DEF(NULL)); // NULL == stderr

// Dump library error description to standard file or stream (UNICODE)
PX14API DumpLibErrorWPX14 (int res, const wchar_t* pPreamble,
                           HPX14 hBrd _PX14_DEF(PX14_INVALID_HANDLE),
                           FILE* filp _PX14_DEF(NULL)); // NULL == stderr

// --- Miscellaneous functions --- //

// Synchronize the PX14400 firmware to the ADC clock
PX14API SyncFirmwareToAdcClockPX14 (HPX14 hBrd,
                                    int bForce _PX14_DEF(0));

// Obtains the version of the PX14400 library
PX14API GetLibVersionPX14 (unsigned long long* verp);
// Obtains the version of the PX14400 driver
PX14API GetDriverVersionPX14 (HPX14 hBrd, unsigned long long* verp);
// Obtains the version of the PX14400 firmware
PX14API GetFirmwareVersionPX14 (HPX14 hBrd, unsigned long long* verp);
// Obtains the version of the PX14400 SAB firmware
PX14API GetSabFirmwareVersionPX14 (HPX14 hBrd, unsigned long long* verp);
// Obtains the version of the PX14400 PCI firmware
PX14API GetSysFirmwareVersionPX14 (HPX14 hBrd, unsigned long long* verp);
// Obtains the revision of the PX14400 hardware
PX14API GetHardwareRevisionPX14 (HPX14 hBrd, unsigned long long* verp);
// Obtains the version of the PX14400 software release
PX14API GetSoftwareReleaseVersionPX14 (unsigned long long* verp);
// Obtains an ASCII string describing the version of an item
PX14API GetVersionTextAPX14 (HPX14 hBrd, unsigned int ver_type,
                             char** bufpp, unsigned int flags);
// Obtains a UNICODE string describing the version of an item
PX14API GetVersionTextWPX14 (HPX14 hBrd, unsigned int ver_type,
                             wchar_t** bufpp, unsigned int flags);

PX14API SetUserDataPX14 (HPX14 hBrd, void* data);
PX14API GetUserDataPX14 (HPX14 hBrd, void** datap);

// Obtain PX14400 driver/device statistics
PX14API GetDriverStatsPX14 (HPX14 hBrd, PX14S_DRIVER_STATS* statsp,
                            int bReset _PX14_DEF(0));

// Read an element from the PX14400 configuration EEPROM
PX14API ReadConfigEepromPX14 (HPX14 hBrd, unsigned int eeprom_addr,
                              unsigned short* eeprom_datap);
// Write an element from the PX14400 configuration EEPROM
PX14API WriteConfigEepromPX14 (HPX14 hBrd, unsigned int eeprom_addr,
                               unsigned short eeprom_data);

// Obtains a user-friendly ASCII string describing the given SIG_* error
PX14API GetErrorTextAPX14 (int res, char** bufpp, unsigned int flags,
						   HPX14 hBrd _PX14_DEF(PX14_INVALID_HANDLE));
// Obtains a user-friendly UNICODE string describing the given SIG_* error
PX14API GetErrorTextWPX14 (int res, wchar_t** bufpp, unsigned int flags,
                           HPX14 hBrd _PX14_DEF(PX14_INVALID_HANDLE));

// Ensures that desired firmware matches desired constraint
/*PX14API CheckFirmwareVerPX14 (HPX14 hBrd, unsigned int fw_ver,
                              int fw_type _PX14_DEF(PX14FWTYPE_SYS),
                              int constraint _PX14_DEF(PX14VCT_GTOE));*/

/// Ensures that given device supports given feature (PX14FEATURE_*)
PX14API CheckFeaturePX14 (HPX14 hBrd, unsigned int feature);

// Free memory allocated by this library
PX14API FreeMemoryPX14 (void* p);

// Run an integrity check on configuration EEPROM data
PX14API ValidateConfigEepromPX14 (HPX14 hBrd);

#endif // __px14_library_header_defined



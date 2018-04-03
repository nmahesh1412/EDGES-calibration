/** @file				px14_config.h
	@brief				Secondary interface to PX14400 user-space library
	@version			2.9
	@date				05/07/2012
	@author				Felix Grenier (Felix.Grenier@Gage-Applied.com)

	Copyright (C) 2008-2012 DynamicSignals LLC.

	This file contains all the functions that configures or communicates directly
	with the hardware, but that should not be present in the API
*/

#ifndef px14_config_h
#define px14_config_h

/** @brief Verify if it a D2 version
*/
PX14API VerifyD2config(HPX14 p_hBrd);

/** @brief ResetDCM
*/
PX14API ResetDcmPX14(HPX14 p_hBrd);

#endif


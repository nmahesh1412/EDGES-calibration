// stdafx.h : include file for standard system include files,
// or project specific include files that are used frequently, but
// are changed infrequently
//

#ifdef _WIN32

// ##
// ## Windows includes
// ##

#pragma once

#define _CRT_SECURE_NO_WARNINGS

#ifndef _WIN32_WINNT
// Windows XP or later
#define _WIN32_WINNT	0x0501
#endif						

#include <windows.h>
#include <tchar.h>
#include <conio.h>

#endif

#ifdef __GNUC__

// ##
// ## Linux includes
// ##


#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/time.h>

#define MAX_PATH 	256

#define _getch		getch_PX14
#define _kbhit		kbhit_PX14

#endif

#include <stdio.h>
#include <string.h>
#include <ctype.h>

//#include "px14.h"
#include <px14.h>



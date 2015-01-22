// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	LinuxPlatformIncludes.h: Includes the platform specific headers for Linux
==================================================================================*/

#pragma once
#include "GenericPlatform/GenericPlatformAffinity.h"
#include "GenericPlatform/GenericPlatformFramePacer.h"
#include "Linux/LinuxSystemIncludes.h"
#include "Misc/Build.h"
#include "Misc/CoreMiscDefines.h"

// include platform implementations
#include "Linux/LinuxPlatformMemory.h"
#include "Linux/LinuxPlatformString.h"
#include "Linux/LinuxPlatformMisc.h"
#include "Linux/LinuxPlatformStackWalk.h"
#include "Linux/LinuxPlatformMath.h"
#include "Linux/LinuxPlatformTime.h"
#include "Linux/LinuxPlatformProcess.h"
#include "Linux/LinuxPlatformOutputDevices.h"
#include "Linux/LinuxPlatformAtomics.h"
#include "Linux/LinuxPlatformTLS.h"
#include "Linux/LinuxPlatformSplash.h"
#include "Linux/LinuxPlatformFile.h"
#include "Linux/LinuxPlatformSurvey.h"
#include "Linux/LinuxPlatformHttp.h"

typedef FGenericPlatformRHIFramePacer FPlatformRHIFramePacer;

typedef FGenericPlatformAffinity FPlatformAffinity;

// include platform properties and typedef it for the runtime
#include "Linux/LinuxPlatformProperties.h"

#ifndef WITH_EDITORONLY_DATA
#error "WITH_EDITORONLY_DATA must be defined"
#endif

#ifndef UE_SERVER
#error "UE_SERVER must be defined"
#endif

typedef FLinuxPlatformProperties<WITH_EDITORONLY_DATA, UE_SERVER, !WITH_SERVER_CODE> FPlatformProperties;

// In glibc 2.17, __secure_getenv was renamed to secure_getenv.
#if defined(__GLIBC__) && (__GLIBC__ == 2) && (__GLIBC_MINOR__ < 17)
#  define secure_getenv __secure_getenv
#endif

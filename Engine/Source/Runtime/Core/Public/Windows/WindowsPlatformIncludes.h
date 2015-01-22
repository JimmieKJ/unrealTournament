// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "GenericPlatform/GenericPlatformAffinity.h"
#include "GenericPlatform/GenericPlatformFramePacer.h"
#include "Misc/Build.h"
#include "Misc/CoreMiscDefines.h"
#include "Windows/WindowsSystemIncludes.h"

// include platform implementations
#include "Windows/WindowsPlatformMemory.h"
#include "Windows/WindowsPlatformString.h"
#include "Windows/WindowsPlatformMisc.h"
#include "Windows/WindowsPlatformStackWalk.h"
#include "Windows/WindowsPlatformMath.h"
#include "Windows/WindowsPlatformNamedPipe.h"
#include "Windows/WindowsPlatformTime.h"
#include "Windows/WindowsPlatformProcess.h"
#include "Windows/WindowsPlatformOutputDevices.h"
#include "Windows/WindowsPlatformAtomics.h"
#include "Windows/WindowsPlatformTLS.h"
#include "Windows/WindowsPlatformSplash.h"
#include "Windows/WindowsPlatformFile.h"
#include "Windows/WindowsPlatformSurvey.h"
#include "Windows/WindowsPlatformHttp.h"


typedef FGenericPlatformRHIFramePacer FPlatformRHIFramePacer;
typedef FGenericPlatformAffinity FPlatformAffinity;


// include platform properties and typedef it for the runtime
#include "Windows/WindowsPlatformProperties.h"


#ifndef WITH_EDITORONLY_DATA
	#error "WITH_EDITORONLY_DATA must be defined"
#endif

#ifndef UE_SERVER
	#error "WITH_EDITORONLY_DATA must be defined"
#endif


typedef FWindowsPlatformProperties<WITH_EDITORONLY_DATA, UE_SERVER, !WITH_SERVER_CODE> FPlatformProperties;

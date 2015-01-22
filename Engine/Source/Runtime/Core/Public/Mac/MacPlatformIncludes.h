// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*================================================================================
	MacPlatformIncludes.h: Includes the platform specific headers for Mac
==================================================================================*/

#pragma once

#include "GenericPlatform/GenericPlatformAffinity.h"
#include "GenericPlatform/GenericPlatformFramePacer.h"
#include "Mac/MacSystemIncludes.h"
#include "Misc/CoreMiscDefines.h"

// include platform implementations
#include "Mac/MacPlatformMemory.h"
#include "Apple/ApplePlatformString.h"
#include "Mac/MacPlatformMisc.h"
#include "Apple/ApplePlatformStackWalk.h"
#include "Mac/MacPlatformMath.h"
#include "Apple/ApplePlatformTime.h"
#include "Mac/MacPlatformProcess.h"
#include "Mac/MacPlatformOutputDevices.h"
#include "Apple/ApplePlatformAtomics.h"
#include "Apple/ApplePlatformTLS.h"
#include "Mac/MacPlatformSplash.h"
#include "Apple/ApplePlatformFile.h"
#include "Mac/MacPlatformSurvey.h"
#include "Mac/MacPlatformHttp.h"

typedef FGenericPlatformRHIFramePacer FPlatformRHIFramePacer;

typedef FGenericPlatformAffinity FPlatformAffinity;

// include platform properties and typedef it for the runtime

#include "Mac/MacPlatformProperties.h"

#ifndef WITH_EDITORONLY_DATA
#error "WITH_EDITORONLY_DATA must be defined"
#endif

#ifndef UE_SERVER
#error "WITH_EDITORONLY_DATA must be defined"
#endif

typedef FMacPlatformProperties<WITH_EDITORONLY_DATA, UE_SERVER, !WITH_SERVER_CODE> FPlatformProperties;

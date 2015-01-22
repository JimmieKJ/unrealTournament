// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "ImageWrapper.h"


/* Dependencies
 *****************************************************************************/

#include "Core.h"
#include "ModuleManager.h"


/* Private includes
 *****************************************************************************/

DECLARE_LOG_CATEGORY_EXTERN(LogImageWrapper, Log, All);


#include "ImageWrapperBase.h"

#if WITH_UNREALJPEG
	#include "JpegImageWrapper.h"
#endif

#if WITH_UNREALPNG
	#include "PngImageWrapper.h"
#endif

#if WITH_UNREALEXR
#include "ExrImageWrapper.h"
#endif

#include "BmpImageWrapper.h"
#include "IcoImageWrapper.h"
#include "IcnsImageWrapper.h"

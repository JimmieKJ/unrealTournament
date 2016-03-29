// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.


#include "EnginePrivate.h"
#include "UpdateTextureShaders.h"

IMPLEMENT_SHADER_TYPE(,FUpdateTexture2DSubresouceCS,TEXT("UpdateTextureShaders"),TEXT("UpdateTexture2DSubresourceCS"),SF_Compute);
IMPLEMENT_SHADER_TYPE(,FUpdateTexture3DSubresouceCS, TEXT("UpdateTextureShaders"), TEXT("UpdateTexture3DSubresourceCS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(,FCopyTexture2DCS,TEXT("UpdateTextureShaders"),TEXT("CopyTexture2DCS"),SF_Compute);

IMPLEMENT_SHADER_TYPE(template<>, TCopyDataCS<2>, TEXT("UpdateTextureShaders"), TEXT("CopyData2CS"), SF_Compute);
IMPLEMENT_SHADER_TYPE(template<>, TCopyDataCS<1>, TEXT("UpdateTextureShaders"), TEXT("CopyData1CS"), SF_Compute);

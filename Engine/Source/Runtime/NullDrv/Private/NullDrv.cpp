// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "NullDrvPrivate.h"
#include "ModuleManager.h"
#include "RenderCore.h"
#include "RenderResource.h"


bool FNullDynamicRHIModule::IsSupported()
{
	return true;
}


IMPLEMENT_MODULE(FNullDynamicRHIModule, NullDrv);

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma  once 

/*------------------------------------------------------------------------------------
IAndroid_MultiTargetPlatformModule interface 
------------------------------------------------------------------------------------*/

#include "Core.h"
#include "ModuleInterface.h"
#include "ITargetPlatformModule.h"

class IAndroid_MultiTargetPlatformModule
	: public ITargetPlatformModule
{
public:
	//
	// Called by AndroidRuntimeSettings to notify us when the user changes the selected texture formats
	//
	virtual void NotifySelectedFormatsChanged() = 0;
};
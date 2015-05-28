// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SlateCorePrivatePCH.h"
#include "ModuleManager.h"


DEFINE_LOG_CATEGORY(LogSlate);
DEFINE_LOG_CATEGORY(LogSlateStyles);

const float FOptionalSize::Unspecified = -1.0f;


/**
 * Implements the SlateCore module.
 */
class FSlateCoreModule
	: public IModuleInterface
{
public:

};


IMPLEMENT_MODULE(FSlateCoreModule, SlateCore);

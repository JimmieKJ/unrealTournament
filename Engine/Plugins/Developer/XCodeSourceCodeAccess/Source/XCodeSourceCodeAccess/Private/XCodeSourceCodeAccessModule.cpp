// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "XCodeSourceCodeAccessPrivatePCH.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "XCodeSourceCodeAccessModule.h"

IMPLEMENT_MODULE( FXCodeSourceCodeAccessModule, XCodeSourceCodeAccess );

void FXCodeSourceCodeAccessModule::StartupModule()
{
	// Bind our source control provider to the editor
	IModularFeatures::Get().RegisterModularFeature(TEXT("SourceCodeAccessor"), &XCodeSourceCodeAccessor );
}

void FXCodeSourceCodeAccessModule::ShutdownModule()
{
	// unbind provider from editor
	IModularFeatures::Get().UnregisterModularFeature(TEXT("SourceCodeAccessor"), &XCodeSourceCodeAccessor);
}
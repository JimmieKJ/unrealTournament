// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "NullSourceCodeAccessPrivatePCH.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "NullSourceCodeAccessModule.h"

IMPLEMENT_MODULE( FNullSourceCodeAccessModule, NullSourceCodeAccess );

void FNullSourceCodeAccessModule::StartupModule()
{
	// Bind our source control provider to the editor
	IModularFeatures::Get().RegisterModularFeature(TEXT("SourceCodeAccessor"), &NullSourceCodeAccessor );
}

void FNullSourceCodeAccessModule::ShutdownModule()
{
	// unbind provider from editor
	IModularFeatures::Get().UnregisterModularFeature(TEXT("SourceCodeAccessor"), &NullSourceCodeAccessor);
}

FNullSourceCodeAccessor& FNullSourceCodeAccessModule::GetAccessor()
{
	return NullSourceCodeAccessor;
}

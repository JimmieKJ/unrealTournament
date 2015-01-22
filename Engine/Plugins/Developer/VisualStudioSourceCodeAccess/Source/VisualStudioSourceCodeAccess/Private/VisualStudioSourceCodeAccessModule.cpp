// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VisualStudioSourceCodeAccessPrivatePCH.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "VisualStudioSourceCodeAccessModule.h"

IMPLEMENT_MODULE( FVisualStudioSourceCodeAccessModule, VisualStudioSourceCodeAccess );

void FVisualStudioSourceCodeAccessModule::StartupModule()
{
	VisualStudioSourceCodeAccessor.Startup();

	// Bind our source control provider to the editor
	IModularFeatures::Get().RegisterModularFeature(TEXT("SourceCodeAccessor"), &VisualStudioSourceCodeAccessor );
}

void FVisualStudioSourceCodeAccessModule::ShutdownModule()
{
	// unbind provider from editor
	IModularFeatures::Get().UnregisterModularFeature(TEXT("SourceCodeAccessor"), &VisualStudioSourceCodeAccessor);

	VisualStudioSourceCodeAccessor.Shutdown();
}

FVisualStudioSourceCodeAccessor& FVisualStudioSourceCodeAccessModule::GetAccessor()
{
	return VisualStudioSourceCodeAccessor;
}
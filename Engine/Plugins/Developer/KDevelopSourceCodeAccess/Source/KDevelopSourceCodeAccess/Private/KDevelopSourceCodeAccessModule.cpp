// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "KDevelopSourceCodeAccessPrivatePCH.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "KDevelopSourceCodeAccessModule.h"

IMPLEMENT_MODULE( FKDevelopSourceCodeAccessModule, KDevelopSourceCodeAccess );

void FKDevelopSourceCodeAccessModule::StartupModule()
{
	KDevelopSourceCodeAccessor.Startup();

	// Bind our source control provider to the editor
	IModularFeatures::Get().RegisterModularFeature(TEXT("SourceCodeAccessor"), &KDevelopSourceCodeAccessor );
}

void FKDevelopSourceCodeAccessModule::ShutdownModule()
{
	// unbind provider from editor
	IModularFeatures::Get().UnregisterModularFeature(TEXT("SourceCodeAccessor"), &KDevelopSourceCodeAccessor);

	KDevelopSourceCodeAccessor.Shutdown();
}

FKDevelopSourceCodeAccessor& FKDevelopSourceCodeAccessModule::GetAccessor()
{
	return KDevelopSourceCodeAccessor;
}


// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CodeLiteSourceCodeAccessPrivatePCH.h"
#include "Runtime/Core/Public/Features/IModularFeatures.h"
#include "CodeLiteSourceCodeAccessModule.h"

IMPLEMENT_MODULE( FCodeLiteSourceCodeAccessModule, CodeLiteSourceCodeAccess );

void FCodeLiteSourceCodeAccessModule::StartupModule()
{
	CodeLiteSourceCodeAccessor.Startup();

	IModularFeatures::Get().RegisterModularFeature(TEXT("SourceCodeAccessor"), &CodeLiteSourceCodeAccessor );
}

void FCodeLiteSourceCodeAccessModule::ShutdownModule()
{
	IModularFeatures::Get().UnregisterModularFeature(TEXT("SourceCodeAccessor"), &CodeLiteSourceCodeAccessor);

	CodeLiteSourceCodeAccessor.Shutdown();
}

FCodeLiteSourceCodeAccessor& FCodeLiteSourceCodeAccessModule::GetAccessor()
{
	return CodeLiteSourceCodeAccessor;
}

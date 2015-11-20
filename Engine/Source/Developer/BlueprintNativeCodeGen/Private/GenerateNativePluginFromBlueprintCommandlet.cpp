// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "BlueprintNativeCodeGenPCH.h"
#include "GenerateNativePluginFromBlueprintCommandlet.h"
#include "NativeCodeGenCommandlineParams.h"
#include "BlueprintNativeCodeGenCoordinator.h"
#include "BlueprintNativeCodeGenUtils.h"
#include "BlueprintNativeCodeGenManifest.h"
#include "FileManager.h"

/*******************************************************************************
 * UGenerateNativePluginFromBlueprintCommandlet
*******************************************************************************/

//------------------------------------------------------------------------------
UGenerateNativePluginFromBlueprintCommandlet::UGenerateNativePluginFromBlueprintCommandlet(FObjectInitializer const& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

//------------------------------------------------------------------------------
int32 UGenerateNativePluginFromBlueprintCommandlet::Main(FString const& Params)
{
	TArray<FString> Tokens, Switches;
	ParseCommandLine(*Params, Tokens, Switches);

	FNativeCodeGenCommandlineParams CommandlineParams(Switches);

	if (CommandlineParams.bHelpRequested)
	{
		UE_LOG(LogBlueprintCodeGen, Display, TEXT("%s"), *FNativeCodeGenCommandlineParams::HelpMessage);
		return 0;
	}

	FBlueprintNativeCodeGenUtils::FScopedFeedbackContext ScopedErrorTracker;
	const bool bSuccess = FBlueprintNativeCodeGenUtils::GeneratePlugin(CommandlineParams);

	return bSuccess && ScopedErrorTracker.HasErrors();
}


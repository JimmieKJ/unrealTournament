// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
RHIUtilities.cpp:
=============================================================================*/
#include "RHIPrivatePCH.h"
#include "RHIUtilities.h"
#include "Core.h"


TAutoConsoleVariable<FString> FDumpTransitionsHelper::CVarDumpTransitionsForResource(
	TEXT("r.DumpTransitionsForResource"),
	TEXT(""),
	TEXT("Prints callstack when the given resource is transitioned. Only implemented for DX11 at the moment.")
	TEXT("Name of the resource to dump"),
	ECVF_Default);

FName FDumpTransitionsHelper::DumpTransitionForResource = NAME_None;
void FDumpTransitionsHelper::DumpTransitionForResourceHandler()
{
	const FString NewValue = CVarDumpTransitionsForResource.GetValueOnGameThread();
	DumpTransitionForResource = FName(*NewValue);
}

void FDumpTransitionsHelper::DumpResourceTransition(const FName& ResourceName, const EResourceTransitionAccess TransitionType)
{
	const FName ResourceDumpName = FDumpTransitionsHelper::DumpTransitionForResource;
	if ((ResourceDumpName != NAME_None) && (ResourceDumpName == ResourceName))
	{
		const uint32 DumpCallstackSize = 2047;
		ANSICHAR DumpCallstack[DumpCallstackSize] = { 0 };

		FPlatformStackWalk::StackWalkAndDump(DumpCallstack, DumpCallstackSize, 2);
		UE_LOG(LogRHI, Log, TEXT("%s transition to: %s"), *ResourceDumpName.ToString(), *FResourceTransitionUtility::ResourceTransitionAccessStrings[(int32)TransitionType]);
		UE_LOG(LogRHI, Log, TEXT("%s"), ANSI_TO_TCHAR(DumpCallstack));
	}
}

FAutoConsoleVariableSink FDumpTransitionsHelper::CVarDumpTransitionsForResourceSink(FConsoleCommandDelegate::CreateStatic(&FDumpTransitionsHelper::DumpTransitionForResourceHandler));
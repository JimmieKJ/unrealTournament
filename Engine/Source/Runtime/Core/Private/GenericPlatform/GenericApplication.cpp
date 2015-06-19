// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CorePrivatePCH.h"
#include "GenericApplication.h"


void FDisplayMetrics::PrintToLog() const
{
	UE_LOG(LogInit, Log, TEXT("Display metrics:"));
	UE_LOG(LogInit, Log, TEXT("  PrimaryDisplayWidth: %d"), PrimaryDisplayWidth);
	UE_LOG(LogInit, Log, TEXT("  PrimaryDisplayHeight: %d"), PrimaryDisplayHeight);
	UE_LOG(LogInit, Log, TEXT("  PrimaryDisplayWorkAreaRect:"));
	UE_LOG(LogInit, Log, TEXT("    Left=%d, Top=%d, Right=%d, Bottom=%d"), 
		PrimaryDisplayWorkAreaRect.Left, PrimaryDisplayWorkAreaRect.Top, 
		PrimaryDisplayWorkAreaRect.Right, PrimaryDisplayWorkAreaRect.Bottom);

	UE_LOG(LogInit, Log, TEXT("  VirtualDisplayRect:"));
	UE_LOG(LogInit, Log, TEXT("    Left=%d, Top=%d, Right=%d, Bottom=%d"), 
		VirtualDisplayRect.Left, VirtualDisplayRect.Top, 
		VirtualDisplayRect.Right, VirtualDisplayRect.Bottom);

	UE_LOG(LogInit, Log, TEXT("  TitleSafePaddingSize: %s"), *TitleSafePaddingSize.ToString());
	UE_LOG(LogInit, Log, TEXT("  ActionSafePaddingSize: %s"), *ActionSafePaddingSize.ToString());

	const int NumMonitors = MonitorInfo.Num();
	UE_LOG(LogInit, Log, TEXT("  Number of monitors: %d"), NumMonitors);

	for (int MonitorIdx = 0; MonitorIdx < NumMonitors; ++MonitorIdx)
	{
		const FMonitorInfo & Info = MonitorInfo[MonitorIdx];
		UE_LOG(LogInit, Log, TEXT("    Monitor %d"), MonitorIdx);
		UE_LOG(LogInit, Log, TEXT("      Name: %s"), *Info.Name);
		UE_LOG(LogInit, Log, TEXT("      ID: %s"), *Info.ID);
		UE_LOG(LogInit, Log, TEXT("      NativeWidth: %d"), Info.NativeWidth);
		UE_LOG(LogInit, Log, TEXT("      NativeHeight: %d"), Info.NativeHeight);
		UE_LOG(LogInit, Log, TEXT("      bIsPrimary: %s"), Info.bIsPrimary ? TEXT("true") : TEXT("false"));
	}
}

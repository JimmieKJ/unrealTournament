// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "WindowsTargetPlatformPrivatePCH.h"


/* UWindowsTargetSettings structors
 *****************************************************************************/

UWindowsTargetSettings::UWindowsTargetSettings( const FObjectInitializer& ObjectInitializer )
	: Super(ObjectInitializer)
{
	MinimumOSVersion = EMinimumSupportedOS::MSOS_Vista;
}

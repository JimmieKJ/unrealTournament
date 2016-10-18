// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"

#include "Components/SafeZoneSlot.h"
#include "Components/SafeZone.h"

USafeZoneSlot::USafeZoneSlot()
{
	bIsTitleSafe = true;
	SafeAreaScale = FMargin(1, 1, 1, 1);
}

void USafeZoneSlot::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if ( IsValid( Parent ) )
	{
		CastChecked< USafeZone >( Parent )->UpdateWidgetProperties();
	}
}

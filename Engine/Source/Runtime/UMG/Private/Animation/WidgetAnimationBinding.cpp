// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "WidgetAnimationBinding.h"


/* FWidgetAnimationBinding interface
 *****************************************************************************/

UObject* FWidgetAnimationBinding::FindRuntimeObject( UWidgetTree& WidgetTree ) const
{
	UObject* FoundObject = FindObject<UObject>(&WidgetTree, *WidgetName.ToString());

	if (FoundObject && (SlotWidgetName != NAME_None))
	{
		// if we were animating the slot, look up the slot that contains the widget 
		UWidget* WidgetObject = Cast<UWidget>(FoundObject);

		if (WidgetObject->Slot)
		{
			FoundObject = WidgetObject->Slot;
		}
	}

	return FoundObject;
}

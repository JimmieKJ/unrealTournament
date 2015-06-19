// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Interface.h"
#include "NamedSlotInterface.generated.h"

class UWidget;

/**  */
UINTERFACE(meta=( CannotImplementInterfaceInBlueprint ))
class UMG_API UNamedSlotInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class UMG_API INamedSlotInterface
{
	GENERATED_IINTERFACE_BODY()

	/**  */
	virtual void GetSlotNames(TArray<FName>& SlotNames) const = 0;

	/**  */
	virtual UWidget* GetContentForSlot(FName SlotName) const = 0;

	/**  */
	virtual void SetContentForSlot(FName SlotName, UWidget* Content) = 0;

	/**  */
	bool ContainsContent(UWidget* Content) const;
};

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CompositeFont.h"
#include "FontProviderInterface.generated.h"

UINTERFACE(MinimalAPI, meta=(CannotImplementInterfaceInBlueprint))
class UFontProviderInterface : public UInterface
{
	GENERATED_UINTERFACE_BODY()
};

class IFontProviderInterface
{
	GENERATED_IINTERFACE_BODY()

	virtual const FCompositeFont* GetCompositeFont() const = 0;
};

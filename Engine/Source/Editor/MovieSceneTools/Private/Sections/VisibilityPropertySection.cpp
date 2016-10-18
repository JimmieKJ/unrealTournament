// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "BoolKeyArea.h"
#include "VisibilityPropertySection.h"


void FVisibilityPropertySection::SetIntermediateValue(FPropertyChangedParams PropertyChangedParams)
{
	KeyArea->SetIntermediateValue(!PropertyChangedParams.GetPropertyValue<bool>());
}

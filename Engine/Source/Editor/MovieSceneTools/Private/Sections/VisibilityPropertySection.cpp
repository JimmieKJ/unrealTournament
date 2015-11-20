// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "VisibilityPropertySection.h"

void FVisibilityPropertySection::SetIntermediateValue( FPropertyChangedParams PropertyChangedParams )
{
	KeyArea->SetIntermediateValue( !PropertyChangedParams.GetPropertyValue<bool>() );
}

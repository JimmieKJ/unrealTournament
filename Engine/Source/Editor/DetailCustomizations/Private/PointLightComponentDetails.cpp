// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PointLightComponentDetails.h"
#include "Components/LightComponentBase.h"
#include "DetailLayoutBuilder.h"

#define LOCTEXT_NAMESPACE "PointLightComponentDetails"

TSharedRef<IDetailCustomization> FPointLightComponentDetails::MakeInstance()
{
	return MakeShareable( new FPointLightComponentDetails );
}

void FPointLightComponentDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	TSharedPtr<IPropertyHandle> LightIntensityProperty = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(ULightComponentBase, Intensity), ULightComponentBase::StaticClass());

	// Inverse squared falloff point lights (the default) are in units of lumens, instead of just being a brightness scale
	LightIntensityProperty->SetInstanceMetaData("UIMin",TEXT("0.0f"));
	LightIntensityProperty->SetInstanceMetaData("UIMax", TEXT("100000.0f"));
	LightIntensityProperty->SetInstanceMetaData("SliderExponent", TEXT("2.0f"));
}

#undef LOCTEXT_NAMESPACE

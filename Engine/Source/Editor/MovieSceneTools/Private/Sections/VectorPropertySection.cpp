// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "MovieSceneToolsPrivatePCH.h"
#include "FloatCurveKeyArea.h"
#include "MovieSceneVectorSection.h"
#include "VectorPropertySection.h"


void FVectorPropertySection::GenerateSectionLayout(class ISectionLayoutBuilder& LayoutBuilder) const
{
	UMovieSceneVectorSection* VectorSection = Cast<UMovieSceneVectorSection>(&SectionObject);
	int32 ChannelsUsed = VectorSection->GetChannelsUsed();
	check(ChannelsUsed >= 2 && ChannelsUsed <= 4);

	XKeyArea = MakeShareable(new FFloatCurveKeyArea(&VectorSection->GetCurve(0), VectorSection));
	LayoutBuilder.AddKeyArea("Vector.X", NSLOCTEXT("FVectorPropertySection", "XArea", "X"), XKeyArea.ToSharedRef());

	YKeyArea = MakeShareable(new FFloatCurveKeyArea(&VectorSection->GetCurve(1), VectorSection));
	LayoutBuilder.AddKeyArea("Vector.Y", NSLOCTEXT("FVectorPropertySection", "YArea", "Y"), YKeyArea.ToSharedRef());

	if (ChannelsUsed >= 3)
	{
		ZKeyArea = MakeShareable(new FFloatCurveKeyArea(&VectorSection->GetCurve(2), VectorSection));
		LayoutBuilder.AddKeyArea("Vector.Z", NSLOCTEXT("FVectorPropertySection", "ZArea", "Z"), ZKeyArea.ToSharedRef());
	}

	if (ChannelsUsed >= 4)
	{
		WKeyArea = MakeShareable(new FFloatCurveKeyArea(&VectorSection->GetCurve(3), VectorSection));
		LayoutBuilder.AddKeyArea("Vector.W", NSLOCTEXT("FVectorPropertySection", "WArea", "W"), WKeyArea.ToSharedRef());
	}
}


void FVectorPropertySection::SetIntermediateValue(FPropertyChangedParams PropertyChangedParams)
{
	const UStructProperty* StructProp = Cast<const UStructProperty>(PropertyChangedParams.PropertyPath.Last());

	FName StructName = StructProp->Struct->GetFName();

	bool bIsVector2D = StructName == NAME_Vector2D;
	bool bIsVector = StructName == NAME_Vector;
	bool bIsVector4 = StructName == NAME_Vector4;

	// Get the vector value from the property
	if (bIsVector2D)
	{
		FVector2D Vector =  PropertyChangedParams.GetPropertyValue<FVector2D>();
		XKeyArea->SetIntermediateValue(Vector.X);
		YKeyArea->SetIntermediateValue(Vector.Y);
	}
	else if (bIsVector)
	{
		FVector Vector = PropertyChangedParams.GetPropertyValue<FVector>();
		XKeyArea->SetIntermediateValue(Vector.X);
		YKeyArea->SetIntermediateValue(Vector.Y);
		ZKeyArea->SetIntermediateValue(Vector.Z);
	}
	else // if (bIsVector4)
	{
		FVector4 Vector = PropertyChangedParams.GetPropertyValue<FVector4>();
		XKeyArea->SetIntermediateValue(Vector.X);
		YKeyArea->SetIntermediateValue(Vector.Y);
		ZKeyArea->SetIntermediateValue(Vector.Z);
		WKeyArea->SetIntermediateValue(Vector.W);
	}
}


void FVectorPropertySection::ClearIntermediateValue()
{
	XKeyArea->ClearIntermediateValue();
	YKeyArea->ClearIntermediateValue();

	if (ZKeyArea.IsValid())
	{
		ZKeyArea->ClearIntermediateValue();
	}

	if (WKeyArea.IsValid())
	{
		WKeyArea->ClearIntermediateValue();
	}
}

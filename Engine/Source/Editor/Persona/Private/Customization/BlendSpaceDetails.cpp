// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "Customization/BlendSpaceDetails.h"

#include "IDetailsView.h"
#include "IDetailGroup.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"

#include "Animation/BlendSpaceBase.h"
#include "Animation/BlendSpace1D.h"

#define LOCTEXT_NAMESPACE "BlendSpaceDetails"

void FBlendSpaceDetails::CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder)
{
	TArray< TWeakObjectPtr<UObject> > Objects;
	DetailBuilder.GetObjectsBeingCustomized(Objects);

	if (Objects.Num())
	{
		const bool b1DBlendSpace = Objects[0]->IsA<UBlendSpace1D>();

		IDetailCategoryBuilder& CategoryBuilder = DetailBuilder.EditCategory(FName("Axis Settings"));
		IDetailGroup* Groups[2] = 
		{
			&CategoryBuilder.AddGroup(FName("Horizontal Axis"), LOCTEXT("HorizontalAxisName", "Horizontal Axis")),
			b1DBlendSpace ? nullptr : &CategoryBuilder.AddGroup(FName("Vertical Axis"), LOCTEXT("VerticalAxisName", "Vertical Axis"))
		};

		// Hide the default blend and interpolation parameters
		TSharedPtr<IPropertyHandle> BlendParameters = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBlendSpaceBase, BlendParameters), UBlendSpaceBase::StaticClass());
		TSharedPtr<IPropertyHandle> InterpolationParameters = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBlendSpaceBase, InterpolationParam), UBlendSpaceBase::StaticClass());
		DetailBuilder.HideProperty(BlendParameters);
		DetailBuilder.HideProperty(InterpolationParameters);

		// Add the properties to the corresponding groups created above (third axis will always be hidden since it isn't used)
		int32 HideIndex = b1DBlendSpace ? 1 : 2;
		for (int32 AxisIndex = 0; AxisIndex < 3; ++AxisIndex)
		{
			TSharedPtr<IPropertyHandle> BlendParameter = BlendParameters->GetChildHandle(AxisIndex);
			TSharedPtr<IPropertyHandle> InterpolationParameter = InterpolationParameters->GetChildHandle(AxisIndex);

			if (AxisIndex < HideIndex)
			{
				Groups[AxisIndex]->AddPropertyRow(BlendParameter.ToSharedRef());
				Groups[AxisIndex]->AddPropertyRow(InterpolationParameter.ToSharedRef());
			}
			else
			{
				DetailBuilder.HideProperty(BlendParameter);
				DetailBuilder.HideProperty(InterpolationParameter);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

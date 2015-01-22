// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "SkeletalControlNodeDetails.h"


#define LOCTEXT_NAMESPACE "SkeletalControlNodeDetails"

/////////////////////////////////////////////////////
// FSkeletalControlNodeDetails 

TSharedRef<IDetailCustomization> FSkeletalControlNodeDetails::MakeInstance()
{
	return MakeShareable(new FSkeletalControlNodeDetails());
}

void FSkeletalControlNodeDetails::CustomizeDetails(class IDetailLayoutBuilder& DetailBuilder)
{
	DetailCategory = &DetailBuilder.EditCategory("PinOptions");
	TSharedRef<IPropertyHandle> AvailablePins = DetailBuilder.GetProperty("ShowPinForProperties");

	//@TODO: Shouldn't show this if the available pins array is empty!
	const bool bGenerateHeader = true;
	const bool bDisplayResetToDefault = false;
	TSharedRef<FDetailArrayBuilder> AvailablePinsBuilder = MakeShareable( new FDetailArrayBuilder( AvailablePins, bGenerateHeader, bDisplayResetToDefault ) );
	AvailablePinsBuilder->OnGenerateArrayElementWidget( FOnGenerateArrayElementWidget::CreateSP( this, &FSkeletalControlNodeDetails::OnGenerateElementForPropertyPin ) );

	const bool bForAdvanced = false;
	DetailCategory->AddCustomBuilder( AvailablePinsBuilder, bForAdvanced );
}

void FSkeletalControlNodeDetails::OnGenerateElementForPropertyPin(TSharedRef<IPropertyHandle> ElementProperty, int32 ElementIndex, IDetailChildrenBuilder& ChildrenBuilder )
{
	TSharedPtr<IPropertyHandle> PropertyNameHandle = ElementProperty->GetChildHandle("PropertyFriendlyName");
	FText PropertyFriendlyName(LOCTEXT("Invalid", "Invalid"));
	 
	if (PropertyNameHandle.IsValid())
	{
		FString DisplayFriendlyName;
		switch (PropertyNameHandle->GetValue(/*out*/ DisplayFriendlyName))
		{
		case FPropertyAccess::Success:
			{
				PropertyFriendlyName = FText::FromString(DisplayFriendlyName);

				//DetailBuilder.EditCategory
				//DisplayNameAsName
			}
			break;
		case FPropertyAccess::MultipleValues:
			ChildrenBuilder.AddChildContent( FText::GetEmpty() ) 
			[
				 SNew(STextBlock).Text(LOCTEXT("OnlyWorksInSingleSelectMode", "Multiple types selected"))
			];
			return;
		case FPropertyAccess::Fail:
		default:
			check(false);
			break;
		}
	}
	
	ChildrenBuilder.AddChildContent( PropertyFriendlyName )
	[
		SNew(SProperty, ElementProperty->GetChildHandle("bShowPin"))
		.DisplayName(PropertyFriendlyName)
	];
}


/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE


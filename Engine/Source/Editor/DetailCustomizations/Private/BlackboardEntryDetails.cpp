// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "BlackboardEntryDetails.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType.h"

#define LOCTEXT_NAMESPACE "BlackboardEntryDetails"

TSharedRef<IPropertyTypeCustomization> FBlackboardEntryDetails::MakeInstance()
{
	return MakeShareable( new FBlackboardEntryDetails );
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void FBlackboardEntryDetails::CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	MyNameProperty = StructPropertyHandle->GetChildHandle(TEXT("EntryName"));
	MyDescriptionProperty = StructPropertyHandle->GetChildHandle(TEXT("EntryDescription"));
	MyValueProperty = StructPropertyHandle->GetChildHandle(TEXT("KeyType"));

	// dont show a header row
	HeaderRow.WholeRowContent()
	[
		SNullWidget::NullWidget
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

void FBlackboardEntryDetails::CustomizeChildren( TSharedRef<class IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	if (StructPropertyHandle->IsValidHandle())
	{
		StructBuilder.AddChildProperty(MyNameProperty.ToSharedRef());
		StructBuilder.AddChildProperty(MyDescriptionProperty.ToSharedRef());
		StructBuilder.AddChildProperty(MyValueProperty.ToSharedRef());
	}
}

#undef LOCTEXT_NAMESPACE

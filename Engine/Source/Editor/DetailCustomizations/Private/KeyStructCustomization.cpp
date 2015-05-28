// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "KeyStructCustomization.h"
#include "ScopedTransaction.h"
#include "SKeySelector.h"

#define LOCTEXT_NAMESPACE "FKeyStructCustomization"


/* FKeyStructCustomization static interface
 *****************************************************************************/

TSharedRef<IPropertyTypeCustomization> FKeyStructCustomization::MakeInstance( )
{
	return MakeShareable(new FKeyStructCustomization);
}


/* IPropertyTypeCustomization interface
 *****************************************************************************/

void FKeyStructCustomization::CustomizeHeader( TSharedRef<class IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	PropertyHandle = StructPropertyHandle;

	TArray<void*> StructPtrs;
	StructPropertyHandle->AccessRawData(StructPtrs);
	check(StructPtrs.Num() != 0);
	FKey* SelectedKey = (FKey*)StructPtrs[0];

	bool bMultipleValues = false;
	for (int32 StructPtrIndex = 1; StructPtrIndex < StructPtrs.Num(); ++StructPtrIndex)
	{
		if (*(FKey*)StructPtrs[StructPtrIndex] != *SelectedKey)
		{
			bMultipleValues = true;
			break;
		}
	}

	// create struct header
	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	.MinDesiredWidth(125.0f)
	.MaxDesiredWidth(325.0f)
	[
		SNew(SKeySelector)
		.CurrentKey(this, &FKeyStructCustomization::GetCurrentKey)
		.OnKeyChanged(this, &FKeyStructCustomization::OnKeyChanged)
		.Font(StructCustomizationUtils.GetRegularFont())
		.HasMultipleValues(bMultipleValues)
	];
}

FKey FKeyStructCustomization::GetCurrentKey() const
{
	TArray<void*> StructPtrs;
	PropertyHandle->AccessRawData(StructPtrs);
	if (StructPtrs.Num() != 0)
	{
		FKey* SelectedKey = (FKey*)StructPtrs[0];

		bool bMultipleValues = false;
		for(int32 StructPtrIndex = 1; StructPtrIndex < StructPtrs.Num(); ++StructPtrIndex)
		{
			if (SelectedKey && *(FKey*)StructPtrs[StructPtrIndex] != *SelectedKey)
			{
				bMultipleValues = true;
				break;
			}
		}

		if( !bMultipleValues && SelectedKey )
		{
			return *SelectedKey;
		}
	}
	return FKey();
}

void FKeyStructCustomization::OnKeyChanged(TSharedPtr<FKey> SelectedKey)
{
	PropertyHandle->SetValueFromFormattedString(SelectedKey->ToString());
}

#undef LOCTEXT_NAMESPACE

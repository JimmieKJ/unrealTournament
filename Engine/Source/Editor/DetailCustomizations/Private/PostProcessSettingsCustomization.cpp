// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "PostProcessSettingsCustomization.h"
#include "ObjectEditorUtils.h"

#define LOCTEXT_NAMESPACE "PostProcessSettingsCustomization"

void FPostProcessSettingsCustomization::CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& StructBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	TMap<FString, IDetailGroup*> CategoryNameToGroupMap;

	uint32 NumChildren = 0;
	FPropertyAccess::Result Result = StructPropertyHandle->GetNumChildren(NumChildren);
	if( Result == FPropertyAccess::Success && NumChildren > 0)
	{
		for( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
		{
			TSharedPtr<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle( ChildIndex );
			if( ChildHandle.IsValid() && ChildHandle->GetProperty() )
			{
				UProperty* Property = ChildHandle->GetProperty();
				FString Category = FObjectEditorUtils::GetCategory( Property );
				IDetailGroup* Group = CategoryNameToGroupMap.FindRef( Category );

				if( !Group )
				{
					Group = &StructBuilder.AddChildGroup( FName(*Category), FText::FromString( FName::NameToDisplayString( Category, false ) ) );
					CategoryNameToGroupMap.Add( Category, Group );
				}
				
				Group->AddPropertyRow( ChildHandle.ToSharedRef() );
			}
		}
	}
	
}


void FPostProcessSettingsCustomization::CustomizeHeader( TSharedRef<IPropertyHandle> StructPropertyHandle, class FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	HeaderRow.NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	];

	HeaderRow.ValueContent()
	[
		StructPropertyHandle->CreatePropertyValueWidget()
	];
}


#undef LOCTEXT_NAMESPACE

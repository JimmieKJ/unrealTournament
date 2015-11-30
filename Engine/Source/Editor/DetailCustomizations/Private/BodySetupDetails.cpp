// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "BodySetupDetails.h"
#include "ScopedTransaction.h"
#include "ObjectEditorUtils.h"
#include "IDocumentation.h"
#include "PhysicsEngine/BodySetup.h"
#include "SNumericEntryBox.h"

#define LOCTEXT_NAMESPACE "BodySetupDetails"

TSharedRef<IDetailCustomization> FBodySetupDetails::MakeInstance()
{
	return MakeShareable( new FBodySetupDetails );
}

void FBodySetupDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	// Customize collision section
	{
		if ( DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBodySetup, DefaultInstance))->IsValidHandle() )
		{
			DetailBuilder.GetObjectsBeingCustomized(ObjectsCustomized);
			BodyInstanceCustomizationHelper = MakeShareable(new FBodyInstanceCustomizationHelper(ObjectsCustomized));
			BodyInstanceCustomizationHelper->CustomizeDetails(DetailBuilder, DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBodySetup, DefaultInstance)));

			IDetailCategoryBuilder& CollisionCategory = DetailBuilder.EditCategory("Collision");

			TSharedPtr<IPropertyHandle> BodyInstanceHandler = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBodySetup, DefaultInstance));
			DetailBuilder.HideProperty(BodyInstanceHandler);

			TSharedPtr<IPropertyHandle> CollisionTraceHandler = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UBodySetup, CollisionTraceFlag));
			DetailBuilder.HideProperty(CollisionTraceHandler);

			// add physics properties to physics category
			uint32 NumChildren = 0;
			BodyInstanceHandler->GetNumChildren(NumChildren);

			static const FName CollisionCategoryName(TEXT("Collision"));

			// add all properties of this now - after adding 
			for (uint32 ChildIndex=0; ChildIndex < NumChildren; ++ChildIndex)
			{
				TSharedPtr<IPropertyHandle> ChildProperty = BodyInstanceHandler->GetChildHandle(ChildIndex);
				FName CategoryName = FObjectEditorUtils::GetCategoryFName(ChildProperty->GetProperty());
				if (CategoryName == CollisionCategoryName)
				{
					CollisionCategory.AddProperty(ChildProperty);
				}
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE


// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "DestructibleMeshDetails.h"
#include "ScopedTransaction.h"
#include "ObjectEditorUtils.h"
#include "IDocumentation.h"
#include "Engine/DestructibleMesh.h"

#define LOCTEXT_NAMESPACE "DestructibleMeshDetails"

TSharedRef<IDetailCustomization> FDestructibleMeshDetails::MakeInstance()
{
	return MakeShareable(new FDestructibleMeshDetails);
}

void AddStructToDetails(FName CategoryName, FName PropertyName, IDetailLayoutBuilder& DetailBuilder, bool bInline = true, bool bAdvanced = false)
{
	IDetailCategoryBuilder& Category = DetailBuilder.EditCategory(CategoryName, FText::GetEmpty(), ECategoryPriority::Important);
	TSharedPtr<IPropertyHandle> Params = DetailBuilder.GetProperty(PropertyName);
	if (Params.IsValid())
	{
		EPropertyLocation::Type PropertyLocation = bAdvanced ? EPropertyLocation::Advanced : EPropertyLocation::Default;
		if (bInline)
		{
			uint32 NumChildren = 0;
			Params->GetNumChildren(NumChildren);

			// add all collision properties
			for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
			{
				TSharedPtr<IPropertyHandle> ChildProperty = Params->GetChildHandle(ChildIndex);
				Category.AddProperty(ChildProperty, PropertyLocation);
			}
		}
		else
		{
			Category.AddProperty(Params, PropertyLocation);
		}
	}
}

void FDestructibleMeshDetails::CustomizeDetails(IDetailLayoutBuilder& DetailBuilder)
{
		//we always hide bodysetup as it's not useful in this editor
		TSharedPtr<IPropertyHandle> BodySetupHandler = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UDestructibleMesh, BodySetup));
		if (BodySetupHandler.IsValid())
		{
			DetailBuilder.HideProperty(BodySetupHandler);
		}
		
		//rest of customization is just moving stuff out of DefaultDestructibleParameters so it's nicer to view
		TSharedPtr<IPropertyHandle> DefaultParams = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UDestructibleMesh, DefaultDestructibleParameters));
		if (DefaultParams.IsValid() == false)
		{
			return;
		}

		AddStructToDetails("Damage", "DefaultDestructibleParameters.DamageParameters", DetailBuilder);
		AddStructToDetails("Damage", "DefaultDestructibleParameters.AdvancedParameters", DetailBuilder, true, true);
		AddStructToDetails("Debris", "DefaultDestructibleParameters.DebrisParameters", DetailBuilder);
		AddStructToDetails("Flags", "DefaultDestructibleParameters.Flags", DetailBuilder);
		AddStructToDetails("HierarchyDepth", "DefaultDestructibleParameters.SpecialHierarchyDepths", DetailBuilder);
		AddStructToDetails("HierarchyDepth", "DefaultDestructibleParameters.DepthParameters", DetailBuilder, false, true);

		

		//hide the default params as we've taken everything out of it
		DetailBuilder.HideProperty(DefaultParams);
}

#undef LOCTEXT_NAMESPACE


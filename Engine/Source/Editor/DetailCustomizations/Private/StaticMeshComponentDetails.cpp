// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "StaticMeshComponentDetails.h"
#include "Components/StaticMeshComponent.h"
#include "PropertyHandle.h"
#include "DetailLayoutBuilder.h"

#define LOCTEXT_NAMESPACE "StaticMeshComponentDetails"


TSharedRef<IDetailCustomization> FStaticMeshComponentDetails::MakeInstance()
{
	return MakeShareable( new FStaticMeshComponentDetails );
}

void FStaticMeshComponentDetails::CustomizeDetails( IDetailLayoutBuilder& DetailBuilder )
{
	// Create a category so this is displayed early in the properties
	DetailBuilder.EditCategory( "StaticMesh", FText::GetEmpty(), ECategoryPriority::Important);

	TSharedRef<IPropertyHandle> UseDefaultCollision = DetailBuilder.GetProperty(GET_MEMBER_NAME_CHECKED(UStaticMeshComponent, bUseDefaultCollision));
	UseDefaultCollision->MarkHiddenByCustomization();
}

#undef LOCTEXT_NAMESPACE

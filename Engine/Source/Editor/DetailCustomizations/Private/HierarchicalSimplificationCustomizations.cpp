// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "DetailCustomizationsPrivatePCH.h"
#include "HierarchicalSimplificationCustomizations.h"
#include "Developer/MeshUtilities/Public/MeshUtilities.h"
#include "GameFramework/WorldSettings.h"

TSharedRef<IPropertyTypeCustomization> FHierarchicalSimplificationCustomizations::MakeInstance() 
{
	return MakeShareable( new FHierarchicalSimplificationCustomizations );
}

void FHierarchicalSimplificationCustomizations::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	HeaderRow.
	NameContent()
	[
		StructPropertyHandle->CreatePropertyNameWidget()
	]
	.ValueContent()
	[
		StructPropertyHandle->CreatePropertyValueWidget()
	];
}

void FHierarchicalSimplificationCustomizations::CustomizeChildren( TSharedRef<IPropertyHandle> StructPropertyHandle, class IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils )
{
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren( NumChildren );

	TMap<FName, TSharedPtr< IPropertyHandle > > PropertyHandles;

	for( uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex )
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle( ChildIndex ).ToSharedRef();
		const FName PropertyName = ChildHandle->GetProperty()->GetFName();

		PropertyHandles.Add(PropertyName, ChildHandle);
	}

	SimplifyMeshPropertyHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FHierarchicalSimplification, bSimplifyMesh));

	IDetailPropertyRow& SimplifyMeshRow = ChildBuilder.AddChildProperty(SimplifyMeshPropertyHandle.ToSharedRef());
	SimplifyMeshRow.Visibility(TAttribute<EVisibility>(this, &FHierarchicalSimplificationCustomizations::IsSimplifyMeshVisible));

	TSharedPtr< IPropertyHandle > ProxyMeshSettingPropertyHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FHierarchicalSimplification, ProxySetting));
	TSharedPtr< IPropertyHandle > MergeMeshSettingPropertyHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FHierarchicalSimplification, MergeSetting));

	for( auto Iter(PropertyHandles.CreateConstIterator()); Iter; ++Iter  )
	{
		if (Iter.Value() != SimplifyMeshPropertyHandle && Iter.Value() != ProxyMeshSettingPropertyHandle && Iter.Value() != MergeMeshSettingPropertyHandle)
		{
			ChildBuilder.AddChildProperty(Iter.Value().ToSharedRef());
		}
	}

	IDetailPropertyRow& ProxyMeshSettingRow = ChildBuilder.AddChildProperty(ProxyMeshSettingPropertyHandle.ToSharedRef());
	ProxyMeshSettingRow.Visibility(TAttribute<EVisibility>(this, &FHierarchicalSimplificationCustomizations::IsProxyMeshSettingVisible));

	IDetailPropertyRow& MergeMeshSettingRow = ChildBuilder.AddChildProperty(MergeMeshSettingPropertyHandle.ToSharedRef());
	MergeMeshSettingRow.Visibility(TAttribute<EVisibility>(this, &FHierarchicalSimplificationCustomizations::IsMergeMeshSettingVisible));
}

EVisibility FHierarchicalSimplificationCustomizations::IsSimplifyMeshVisible() const
{
	IMeshUtilities& MeshUtilities = FModuleManager::Get().LoadModuleChecked<IMeshUtilities>("MeshUtilities");
	if (MeshUtilities.GetMeshMergingInterface() != nullptr)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}

EVisibility FHierarchicalSimplificationCustomizations::IsProxyMeshSettingVisible() const
{
	bool bSimplifyMesh;

	if (SimplifyMeshPropertyHandle->GetValue(bSimplifyMesh) == FPropertyAccess::Result::Success)
	{
		if (IsSimplifyMeshVisible() == EVisibility::Visible && bSimplifyMesh)
		{
			return EVisibility::Visible;
		}
	}
		
	return EVisibility::Hidden;
}

EVisibility FHierarchicalSimplificationCustomizations::IsMergeMeshSettingVisible() const
{
	if(IsProxyMeshSettingVisible() == EVisibility::Hidden)
	{
		return EVisibility::Visible;
	}

	return EVisibility::Hidden;
}
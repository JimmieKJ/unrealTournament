// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

class FFbxImportUIDetails : public IDetailCustomization
{
public:
	/** Makes a new instance of this detail layout class for a specific detail view requesting it */
	static TSharedRef<IDetailCustomization> MakeInstance();

	/** IDetailCustomization interface */
	virtual void CustomizeDetails( IDetailLayoutBuilder& DetailBuilder ) override;
	/** End IDetailCustomization interface */

	void CollectChildPropertiesRecursive(TSharedPtr<IPropertyHandle> Node, TArray<TSharedPtr<IPropertyHandle>>& OutProperties);
	
	/** Checks whether a metadata string is valid for a given import type 
	* @param ImportType the type of mesh being imported
	* @param MetaData the metadata string to validate
	*/
	bool IsImportTypeMetaDataValid(EFBXImportType& ImportType, FString& MetaData);
	
	/** Called if the mesh mode (static / skeletal / SubDSurface) changes */
	void MeshImportModeChanged();

	/** Called if the import mesh option for skeletal meshes is changed */
	void ImportMeshToggleChanged();

	TWeakObjectPtr<UFbxImportUI> ImportUI;		// The UI data object being customised
	IDetailLayoutBuilder* CachedDetailBuilder;	// The detail builder for this cusomtomisation

private:

	/** Use MakeInstance to create an instance of this class */
	FFbxImportUIDetails();

	/** Sets a custom widget for the StaticMeshLODGroup property */
	void SetStaticMeshLODGroupWidget(IDetailPropertyRow& PropertyRow, const TSharedPtr<IPropertyHandle>& Handle);

	/** Called when the StaticMeshLODGroup spinbox is changed */
	void OnLODGroupChanged(TSharedPtr<FString> NewValue, ESelectInfo::Type SelectInfo, TWeakPtr<IPropertyHandle> HandlePtr);

	/** Called to determine the visibility of the VertexOverrideColor property */
	bool GetVertexOverrideColorEnabledState() const;

	/** LOD group options. */
	TArray<FName> LODGroupNames;
	TArray<TSharedPtr<FString>> LODGroupOptions;

	/** Cached StaticMeshLODGroup property handle */
	TSharedPtr<IPropertyHandle> StaticMeshLODGroupPropertyHandle;

	/** Cached VertexColorImportOption property handle */
	TSharedPtr<IPropertyHandle> VertexColorImportOptionHandle;
};

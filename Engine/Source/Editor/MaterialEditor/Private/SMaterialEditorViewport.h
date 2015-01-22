// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PreviewScene.h"
#include "SEditorViewport.h"


/**
 * Material Editor Preview viewport widget
 */
class SMaterialEditorViewport : public SEditorViewport, public FGCObject
{
public:
	SLATE_BEGIN_ARGS( SMaterialEditorViewport ){}
		SLATE_ARGUMENT(TWeakPtr<IMaterialEditor>, MaterialEditor)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	~SMaterialEditorViewport();
	
	virtual void AddReferencedObjects( FReferenceCollector& Collector ) override;

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	void RefreshViewport();
	
	/**
	 * Sets the mesh on which to preview the material.  One of either InStaticMesh or InSkeletalMesh must be non-NULL!
	 * Does nothing if a skeletal mesh was specified but the material has bUsedWithSkeletalMesh=false.
	 *
	 * @return	true if a mesh was set successfully, false otherwise.
	 */
	bool SetPreviewMesh(UStaticMesh* InStaticMesh, USkeletalMesh* InSkeletalMesh);

	/**
	 * Sets the preview mesh to the named mesh, if it can be found.  Checks static meshes first, then skeletal meshes.
	 * Does nothing if the named mesh is not found or if the named mesh is a skeletal mesh but the material has
	 * bUsedWithSkeletalMesh=false.
	 *
	 * @return	true if the named mesh was found and set successfully, false otherwise.
	 */
	bool SetPreviewMesh(const TCHAR* InMeshName);

	void SetPreviewMaterial(UMaterialInterface* InMaterialInterface);
	
	void ToggleRealtime();

	/** @return The list of commands known by the material editor */
	TSharedRef<FUICommandList> GetMaterialEditorCommands() const;

	/** If true, use PreviewSkeletalMeshComponent as the preview mesh; if false, use PreviewMeshComponent. */
	bool bUseSkeletalMeshAsPreview;

	/** Component for the preview static mesh. */
	UMaterialEditorMeshComponent* PreviewMeshComponent;
	
	/** Component for the preview skeletal mesh. */
	USkeletalMeshComponent*	PreviewSkeletalMeshComponent;

	/** The preview primitive we are using. */
	EThumbnailPrimType PreviewPrimType;
	
	/** If true, render background object in the preview scene. */
	bool bShowBackground;

	/** If true, render grid the preview scene. */
	bool bShowGrid;
	
	FPreviewScene PreviewScene;
	/** The material editor has been added to a tab */
	void OnAddedToTab( const TSharedRef<SDockTab>& OwnerTab );



	/** Event handlers */
	void OnSetPreviewPrimitive(EThumbnailPrimType PrimType);
	bool IsPreviewPrimitiveChecked(EThumbnailPrimType PrimType) const;
	void OnSetPreviewMeshFromSelection();
	void TogglePreviewGrid();
	bool IsTogglePreviewGridChecked() const;
	void TogglePreviewBackground();
	bool IsTogglePreviewBackgroundChecked() const;

protected:
	/** SEditorViewport interface */
	virtual TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override;
	virtual TSharedPtr<SWidget> MakeViewportToolbar() override;
	virtual EVisibility OnGetViewportContentVisibility() const override;
	virtual void BindCommands() override;
	virtual void OnFocusViewportToSelection() override;
private:
	/** The parent tab where this viewport resides */
	TWeakPtr<SDockTab> ParentTab;

	bool IsVisible() const;

	/** Pointer back to the material editor tool that owns us */
	TWeakPtr<IMaterialEditor> MaterialEditorPtr;
	
	/** Level viewport client */
	TSharedPtr<class FMaterialEditorViewportClient> EditorViewportClient;
};

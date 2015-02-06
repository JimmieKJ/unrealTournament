// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**
 * Wraps a details panel customized for viewing actors
 */
class SActorDetails : public SCompoundWidget, public FEditorUndoClient
{
public:
	SLATE_BEGIN_ARGS(SActorDetails) {}
		SLATE_ARGUMENT( TSharedPtr<FExtender>, ActorMenuExtender )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, const FName TabIdentifier);
	~SActorDetails();

	/**
	 * Sets the objects to be viewed by the details panel
	 *
	 * @param InObjects	The objects to set
	 */
	void SetObjects(const TArray<UObject*>& InObjects);

	/** FEditorUndoClient Interface */
	virtual void PostUndo(bool bSuccess) override;
	virtual void PostRedo(bool bSuccess) override;

private:
	AActor* GetSelectedActorInEditor() const;
	AActor* GetActorContext() const;

	void OnComponentsEditedInWorld();
	void OnEditorSelectionChanged(UObject* Object);
	void OnSCSEditorRootSelected(AActor* Actor);
	void OnSCSEditorTreeViewSelectionChanged(const TArray<TSharedPtr<class FSCSEditorTreeNode> >& SelectedNodes);
	void UpdateComponentTreeFromEditorSelection();
	bool IsPropertyEditingEnabled() const;
	void OnBlueprintWarningHyperlinkClicked( const FSlateHyperlinkRun::FMetadata& Metadata );
	EVisibility GetBlueprintComponentWarningVisibility() const;

private:
	TSharedPtr<SSplitter> DetailsSplitter;
	TSharedPtr<class IDetailsView> DetailsView;
	TSharedPtr<SBox> ComponentsBox;
	TSharedPtr<class SSCSEditor> SCSEditor;

	// The actor selected when the details panel was locked
	TWeakObjectPtr<AActor> LockedActorSelection;

	// Used to prevent reentrant changes
	bool bSelectionGuard;

	// True if the actor "root" node in the SCS editor is currently shown as selected
	bool bShowingRootActorNodeSelected;
};
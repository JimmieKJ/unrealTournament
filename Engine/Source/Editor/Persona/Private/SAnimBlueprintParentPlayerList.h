// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Persona.h"
#include "IDetailsView.h"
#include "EditorObjectsTracker.h"

class SAnimBlueprintParentPlayerList : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAnimBlueprintParentPlayerList)
		: _Persona()
		{}
	SLATE_ARGUMENT(TWeakPtr<FPersona>, Persona)
		SLATE_END_ARGS()

		SAnimBlueprintParentPlayerList();
	~SAnimBlueprintParentPlayerList();

	void Construct(const FArguments& InArgs);

private:

	// Called when the root blueprint is changed. depending on the action
	// we need to refresh the list of available overrides incase we need
	// to remove or add some
	void OnRootBlueprintChanged(UBlueprint* InBlueprint);

	// Called when the current blueprint changes. Used to detect reparenting
	// So the data and UI can be updated accordingly
	void OnCurrentBlueprintChanged(UBlueprint* InBlueprint);

	// Called when an override is changed on a less derived blueprint in the
	// current blueprint's hierarchy so we can copy them if we haven't overridden
	// the same asset
	void OnHierarchyOverrideChanged(FGuid NodeGuid, UAnimationAsset* NewAsset);

	void RefreshDetailView();

	// Persona instance we're currently in
	TWeakPtr<FPersona> PersonaPtr;

	// Object tracker to maintain single instance of editor objects
	FEditorObjectTracker ObjectTracker;

	// The blueprint currently in use
	UAnimBlueprint* CurrentBlueprint;

	// Parent class of the current blueprint, used to detect reparenting
	UClass* CurrentParentClass;

	// Root blueprint if one exists
	UAnimBlueprint* RootBlueprint;

	// The details view for the list
	TSharedPtr<IDetailsView> DetailView;
};
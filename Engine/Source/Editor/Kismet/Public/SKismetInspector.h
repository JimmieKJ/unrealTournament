// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.


#pragma once

#include "BlueprintEditor.h"
#include "Editor/PropertyEditor/Public/PropertyEditorModule.h"

typedef TSet<class UObject*> FInspectorSelectionSet;

//////////////////////////////////////////////////////////////////////////
// SKismetInspector

/** Widget that shows properties and tools related to the selected node(s) */
class KISMET_API SKismetInspector : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SKismetInspector )
		: _ShowPublicViewControl(false)
		, _HideNameArea(false)
		, _SetNotifyHook(true)
		, _ShowTitleArea(false)
		{}

		SLATE_ARGUMENT(TWeakPtr<FBlueprintEditor>, Kismet2)
		SLATE_ARGUMENT(TWeakPtr<SMyBlueprint>, MyBlueprintWidget)
		SLATE_ARGUMENT( bool, ShowPublicViewControl )
		SLATE_ARGUMENT( bool, HideNameArea )
		SLATE_ARGUMENT( FIsPropertyEditingEnabled, IsPropertyEditingEnabledDelegate )
		SLATE_ARGUMENT( FOnFinishedChangingProperties::FDelegate, OnFinishedChangingProperties )
		SLATE_ARGUMENT( FName, ViewIdentifier)
		SLATE_ARGUMENT( bool, SetNotifyHook)
		SLATE_ARGUMENT( bool, ShowTitleArea)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	/** Options for ShowDetails */
	struct FShowDetailsOptions
	{
		FString ForcedTitle;
		bool bForceRefresh;
		bool bHideFilterArea;

		FShowDetailsOptions(const FString& InForcedTitle = FString(), bool bInForceRefresh = false)
			:ForcedTitle(InForcedTitle)
			,bForceRefresh(bInForceRefresh)
			,bHideFilterArea(false)
		{}
	};

	/** Update the inspector window to show information on the supplied object */
	void ShowDetailsForSingleObject(UObject* Object, const FShowDetailsOptions& Options = FShowDetailsOptions());

	/** Update the inspector window to show information on the supplied objects */
	void ShowDetailsForObjects(const TArray<UObject*>& PropertyObjects, const FShowDetailsOptions& Options = FShowDetailsOptions());

	/** Used to control visibility of a property in the property window */
	bool IsPropertyVisible( const struct FPropertyAndParent& PropertyAndParent ) const;

	/** Turns on or off details customization for components */
	void EnableComponentDetailsCustomization(bool bEnable);

	TSharedPtr<class IDetailsView> GetPropertyView() const { return PropertyView; }

protected:
	/** Update the inspector window to show information on the supplied objects */
	void UpdateFromObjects(const TArray<UObject*>& PropertyObjects, struct FKismetSelectionInfo& SelectionInfo, const FShowDetailsOptions& Options);

	/** Add this property and all its child properties to SelectedObjectProperties */
	void AddPropertiesRecursive(UProperty* Property);

	/** Pointer back to the kismet 2 tool that owns us */
	TWeakPtr<FBlueprintEditor> Kismet2Ptr;

	/** String used as the title above the property window */
	FString PropertyViewTitle;

	/** Should we currently show the property view */
	bool bShowInspectorPropertyView;

	/** State of CheckBox representing whether to show only the public variables*/
	ECheckBoxState	PublicViewState;

	/** Property viewing widget */
	TSharedPtr<class IDetailsView> PropertyView;

	/** Selected objects for this detail view */
	TArray< TWeakObjectPtr<UObject> > SelectedObjects;

	/** The widget used to edit the names of properties */
	TSharedPtr<class SNameEditingWidget> EditNameWidget;

	/** Border widget that wraps a dynamic context-sensitive widget for editing objects that the property window is displaying */
	TSharedPtr<SBorder> ContextualEditingBorderWidget;

	/** If true show the public view control */
	bool bShowPublicView;

	/** If true show the kismet inspector title widget */
	bool bShowTitleArea;

	/** Set of object properties that should be visible */
	TSet<TWeakObjectPtr<UProperty> > SelectedObjectProperties;
	
	/** User defined delegate for OnFinishedChangingProperties */
	FOnFinishedChangingProperties::FDelegate UserOnFinishedChangingProperties;

protected:
	/** Show properties of the selected object */
	void SetPropertyWindowContents(TArray<UObject*> Objects);

	/** Returns whether the property view be visible */
	EVisibility GetPropertyViewVisibility() const;

	/**
	 * Generates a widget that is used to edit the specified object array contextually.  This widget
	 * will be displayed along with a property view in the level editor
	 */
	TSharedRef<SWidget> MakeContextualEditingWidget(struct FKismetSelectionInfo& SelectionInfo, const FShowDetailsOptions& Options);

	/**
	 * Generates the text for the title in the contextual editing widget
	 */
	FString GetContextualEditingWidgetTitle() const;

	ECheckBoxState GetPublicViewCheckboxState() const;
	void SetPublicViewCheckboxState(ECheckBoxState InIsChecked);
};

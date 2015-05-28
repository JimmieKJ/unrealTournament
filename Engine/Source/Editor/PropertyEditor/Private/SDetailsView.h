// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SDetailsViewBase.h"

class SDetailsView : public SDetailsViewBase
{
	friend class FPropertyDetailsUtilities;
public:

	SLATE_BEGIN_ARGS( SDetailsView )
		: _DetailsViewArgs()
		{}
		/** The user defined args for the details view */
		SLATE_ARGUMENT( FDetailsViewArgs, DetailsViewArgs )
	SLATE_END_ARGS()

	virtual ~SDetailsView();

	/** Causes the details view to be refreshed (new widgets generated) with the current set of objects */
	void ForceRefresh() override;

	/**
	 * Constructs the property view widgets                   
	 */
	void Construct(const FArguments& InArgs);

	/** IDetailsView interface */
	virtual void SetObjects( const TArray<UObject*>& InObjects, bool bForceRefresh = false, bool bOverrideLock = false ) override;
	virtual void SetObjects( const TArray< TWeakObjectPtr< UObject > >& InObjects, bool bForceRefresh = false, bool bOverrideLock = false ) override;
	virtual void SetObject( UObject* InObject, bool bForceRefresh = false ) override;
	virtual void RemoveInvalidObjects() override;

	/**
	 * Replaces objects being observed by the view with new objects
	 *
	 * @param OldToNewObjectMap	Mapping from objects to replace to their replacement
	 */
	void ReplaceObjects( const TMap<UObject*, UObject*>& OldToNewObjectMap );

	/**
	 * Removes objects from the view because they are about to be deleted
	 *
	 * @param DeletedObjects	The objects to delete
	 */
	void RemoveDeletedObjects( const TArray<UObject*>& DeletedObjects );

	/** Sets the callback for when the property view changes */
	virtual void SetOnObjectArrayChanged( FOnObjectArrayChanged OnObjectArrayChangedDelegate) override;

	/** @return	Returns list of selected objects we're inspecting */
	virtual const TArray< TWeakObjectPtr<UObject> >& GetSelectedObjects() const override
	{
		return SelectedObjects;
	} 

	/** @return	Returns list of selected actors we're inspecting */
	virtual const TArray< TWeakObjectPtr<AActor> >& GetSelectedActors() const override
	{
		return SelectedActors;
	}

	/** @return Returns information about the selected set of actors */
	virtual const FSelectedActorInfo& GetSelectedActorInfo() const override
	{
		return SelectedActorInfo;
	}

	virtual bool HasClassDefaultObject() const override
	{
		return bViewingClassDefaultObject;
	}

	/** Gets the base class being viewed */
	const UClass* GetBaseClass() const override;
	UClass* GetBaseClass() override;
	UStruct* GetBaseStruct() const override;

	/**
	 * Adds an external property root node to the list of root nodes that the details new needs to manage
	 *
	 * @param InExternalRootNode	The node to add
	 */
	void AddExternalRootPropertyNode( TSharedRef<FPropertyNode> ExternalRootNode ) override;
	
	/**
	 * @return True if a category is hidden by any of the uobject classes currently in view by this details panel
	 */
	bool IsCategoryHiddenByClass(FName CategoryName) const override;

	virtual bool IsConnected() const override;

	virtual TSharedPtr<FComplexPropertyNode> GetRootNode() override
	{
		return RootPropertyNode;
	}

	virtual bool DontUpdateValueWhileEditing() const override
	{ 
		return false; 
	}
private:
	void RegisterInstancedCustomPropertyLayout( UClass* Class, FOnGetDetailCustomizationInstance DetailLayoutDelegate ) override;
	void UnregisterInstancedCustomPropertyLayout( UClass* Class ) override;
	void SetObjectArrayPrivate( const TArray< TWeakObjectPtr< UObject > >& InObjects );

	TSharedRef<SDetailTree> ConstructTreeView( TSharedRef<SScrollBar>& ScrollBar );

	/**
	 * Returns whether or not new objects need to be set. If the new objects being set are identical to the objects 
	 * already in the details panel, nothing needs to be set
	 *
	 * @param InObjects The potential new objects to set
	 * @return true if the new objects should be set
	 */
	bool ShouldSetNewObjects( const TArray< TWeakObjectPtr< UObject > >& InObjects ) const;

	/** Called before during SetObjectArray before we change the objects being observed */
	void PreSetObject();

	/** Called at the end of SetObjectArray after we change the objects being observed */
	void PostSetObject();
	
	/** Called when the filter button is clicked */
	void OnFilterButtonClicked();

	/** Called to get the visibility of the actor name area */
	EVisibility GetActorNameAreaVisibility() const;

	/** Called to get the visibility of the scrollbar */
	EVisibility GetScrollBarVisibility() const;

	/** Returns the name of the image used for the icon on the locked button */
	const FSlateBrush* OnGetLockButtonImageResource() const;
	/**
	 * Called to open the raw property editor (property matrix)                                                              
	 */
	FReply OnOpenRawPropertyEditorClicked();

private:
	/** Information about the current set of selected actors */
	FSelectedActorInfo SelectedActorInfo;
	/** Selected objects for this detail view.  */
	TArray< TWeakObjectPtr<UObject> > SelectedObjects;

	/** 
	 * Selected actors for this detail view.  Note that this is not necessarily the same editor selected actor set.  If this detail view is locked
	 * It will only contain actors from when it was locked 
	 */
	TArray< TWeakObjectPtr<AActor> > SelectedActors;
	/** The root property node of the property tree for a specific set of UObjects */
	TSharedPtr<FObjectPropertyNode> RootPropertyNode;
	/** Callback to send when the property view changes */
	FOnObjectArrayChanged OnObjectArrayChanged;
	/** True if at least one viewed object is a CDO (blueprint editing) */
	bool bViewingClassDefaultObject;
};

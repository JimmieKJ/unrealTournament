// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "PropertyEditorConstants.h"
#include "PropertyEditorAssetConstants.h"
#include "PropertyCustomizationHelpers.h"

/**
 * A widget used to edit Asset-type properties (UObject-derived properties).
 * Can also be used (with a NULL FPropertyEditor) to edit a raw weak object pointer.
 */
class SPropertyEditorAsset : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS( SPropertyEditorAsset )
		: _AssetFont( FEditorStyle::GetFontStyle("PropertyEditor.AssetName.Font") ) 
		, _ClassFont( FEditorStyle::GetFontStyle("PropertyEditor.AssetClass.Font") ) 
		, _AllowClear(true)
		, _DisplayThumbnail(false)
		, _DisplayUseSelected(true)
		, _DisplayBrowse(true)
		, _ThumbnailPool(NULL)
		, _ThumbnailSize( FIntPoint(64, 64) )
		, _ObjectPath()
		, _Class(NULL)
		, _CustomContentSlot()
		, _ResetToDefaultSlot()
		{}
		SLATE_ATTRIBUTE( FSlateFontInfo, AssetFont )
		SLATE_ATTRIBUTE( FSlateFontInfo, ClassFont )
		SLATE_ARGUMENT( bool, AllowClear )
		SLATE_ARGUMENT( bool, DisplayThumbnail )
		SLATE_ARGUMENT( bool, DisplayUseSelected )
		SLATE_ARGUMENT( bool, DisplayBrowse )
		SLATE_ARGUMENT( TSharedPtr<FAssetThumbnailPool>, ThumbnailPool )
		SLATE_ARGUMENT( FIntPoint, ThumbnailSize )
		SLATE_ATTRIBUTE( FString, ObjectPath )
		SLATE_ARGUMENT( UClass*, Class )
		SLATE_ARGUMENT( TOptional<TArray<UFactory*>>, NewAssetFactories )
		SLATE_EVENT( FOnSetObject, OnSetObject )
		SLATE_EVENT(FOnShouldFilterAsset, OnShouldFilterAsset)
		SLATE_NAMED_SLOT( FArguments, CustomContentSlot )
		SLATE_NAMED_SLOT( FArguments, ResetToDefaultSlot )
		SLATE_ARGUMENT( TSharedPtr<IPropertyHandle>, PropertyHandle )

	SLATE_END_ARGS()

	static bool Supports( const TSharedRef< class FPropertyEditor >& InPropertyEditor );
	static bool Supports( const UProperty* NodeProperty );

	/**
	 * Construct the widget.
	 * @param	InArgs				Arguments for widget construction
	 * @param	InPropertyEditor	The property editor that this widget will operate on. If this is NULL, we expect
	 *                              the Class member of InArgs to be a valid UClass, so we know what objects this 
	 *                              widget can accept.
	 */
	void Construct( const FArguments& InArgs, const TSharedPtr<FPropertyEditor>& InPropertyEditor = NULL );

	void GetDesiredWidth( float& OutMinDesiredWidth, float &OutMaxDesiredWidth );

	/** Override the tick method so we can check for thumbnail differences & update if necessary */
	virtual void Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime ) override;

private:
	struct FObjectOrAssetData
	{
		UObject* Object;
		FAssetData AssetData;

		FObjectOrAssetData( UObject* InObject = NULL )
			: Object( InObject )
		{
			AssetData = InObject != NULL && !InObject->IsA<AActor>() ? FAssetData( InObject ) : FAssetData();
		}

		FObjectOrAssetData( const FAssetData& InAssetData )
			: Object( NULL )
			, AssetData( InAssetData )
		{}

		bool IsValid() const
		{
			return Object != NULL || AssetData.IsValid();
		}
	};

	/** 
	 * Helper function for Construct - determines whether we should display a thumbnail or not
	 * @param	InArgs	The args passed into Construct()
	 * @returns true if we should display the thumbnail 
	 */
	bool ShouldDisplayThumbnail( const FArguments& InArgs );

	/** Gets the border brush to show around the thumbnail, changes when the user hovers on it. */
	const FSlateBrush* GetThumbnailBorder() const;

	/** 
	 * Get the content to be displayed in the asset/actor picker menu 
	 * @returns the widget for the menu content
	 */
	TSharedRef<SWidget> OnGetMenuContent();

	/**
	 * Called when the asset menu is closed, we handle this to force the destruction of the asset menu to
	 * ensure any settings the user set are saved.
	 */
	void OnMenuOpenChanged(bool bOpen);

	/**
	 * Returns whether the actor should be filtered out from selection.
	 */
	bool IsFilteredActor( const AActor* const Actor) const;

	/**
	 * Closes the combo button for the asset name.
	 */
	void CloseComboButton();

	/** 
	 * Get the name to be displayed for this asset
	 * @returns the name of the asset
	 */
	FText OnGetAssetName() const;

	/** 
	 * Get the class name to be displayed for this asset
	 * @returns the class name of this asset
	 */
	FText OnGetAssetClassName() const;

	/** 
	 * Get the tooltip to be displayed for this asset
	 * @returns the tooltip string
	 */
	FText OnGetToolTip() const;

	/** 
	 * Set the value of the asset referenced by this property editor.
	 * Will set the underlying property handle if there is one.
	 * @param	AssetData	The asset data for the object to set the reference to
	 */
	void SetValue( const FAssetData& AssetData );

	/** 
	 * Get the value referenced by this widget.
	 * @returns the referenced object
	 */
	FPropertyAccess::Result GetValue( FObjectOrAssetData& OutValue ) const;

	/** 
	 * Get the UClass we will display in the UI.
	 * This is the class of the object (if valid) or the property handles class (if any)
	 * or the Class value this widget was constructed with.
	 * @returns the UClass to display
	 */
	UClass* GetDisplayedClass() const;

	/** 
	 * Delegate for handling selection in the asset browser.
	 * @param	Object	The chosen asset
	 */
	void OnAssetSelected( const class FAssetData& AssetData );

	/** 
	 * Delegate for handling selection in the scene outliner.
	 * @param	InActor	The chosen actor
	 */
	void OnActorSelected( AActor* InActor );

	/** 
	 * Delegate for handling classes of objects that can be picked.
	 * @param	AllowedClasses	The array of classes we allow
	 */
	void OnGetAllowedClasses(TArray<const UClass*>& AllowedClasses);

	/** 
	 * Opens the asset editor for the viewed object
	 */
	void OnOpenAssetEditor();

	/** 
	 * Browse for the object referenced by this widget, either in the Content Browser
	 * or the scene (for Actors)
	 */
	void OnBrowse();

	/** 
	 * Get the tooltip text for the Browse button
	 */
	FText GetOnBrowseToolTip() const;

	/** 
	 * Use the selected object (replaces the referenced object if valid)
	 */
	void OnUse();

	/** 
	 * Clear the referenced object
	 */
	void OnClear();

	/** 
	 * Assets have an associated color, this is used to supply that color in the UI.
	 * @returns the color to display for this asset
	 */
	FSlateColor GetAssetClassColor();

	/** 
	 * Delegate used to check whether we can drop an object on this widget.
	 * @param	InObject	The object we are dragging
	 * @returns true if the object can be dropped
	 */
	bool OnAssetDraggedOver( const UObject* InObject ) const;

	/** 
	 * Delegate handling dropping an object on this widget
	 * @param	InObject	The object we are dropping
	 */
	void OnAssetDropped( UObject* InObject );

	/** 
	 * Delegate handling ctrl+c
	 */
	void OnCopy();

	/** 
	 * Delegate handling ctrl+v
	 */
	void OnPaste();

	/**
	 * @return True of the current clipboard contents can be pasted
	 */
	bool CanPaste();

	/** 
	 * Handle double clicking the asset thumbnail. this 'Edits' the displayed asset 
	 */
	FReply OnAssetThumbnailDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent );

	/** @return True if the property can be edited */
	bool CanEdit() const;

	/** @return true if the passed in AssetData can be used to set the property based on the list of custom classes */
	bool CanSetBasedOnCustomClasses( const FAssetData& InAssetData ) const;
private:

	/** Main combobutton */
	TSharedPtr< class SComboButton > AssetComboButton;

	/** The border surrounding the thumbnail image. */
	TSharedPtr< class SBorder > ThumbnailBorder;

	/** The property editor, if any */
	TSharedPtr<class FPropertyEditor> PropertyEditor;

	/** Path to the object being edited instead of accessing the value directly with a property handle */
	TAttribute<FString> ObjectPath;

	/** Cached data */
	mutable FAssetData CachedAssetData;

	/** The class of the object we are editing */
	UClass* ObjectClass;

	/** Classes that can be used with this property */
	TArray<const UClass*> CustomClassFilters;

	/** A list of the factories we can use to create new assets */
	TArray<UFactory*> NewAssetFactories;

	/** Whether the asset can be 'None' in this case */
	bool bAllowClear;

	/** Whether the object we are editing is an Actor (i.e. requires a Scene Outliner to be displayed) */
	bool bIsActor;

	/** Delegate to call when our object value is set */
	FOnSetObject OnSetObject;

	/** Delegate for filtering valid assets */
	FOnShouldFilterAsset OnShouldFilterAsset;

	/** Thumbnail for the asset */
	TSharedPtr<class FAssetThumbnail> AssetThumbnail;

	/** The property handle, if any */
	TSharedPtr<class IPropertyHandle> PropertyHandle;
};
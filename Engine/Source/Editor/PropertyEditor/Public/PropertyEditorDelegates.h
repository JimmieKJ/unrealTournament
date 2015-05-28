// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once



struct FPropertyAndParent
{
	FPropertyAndParent( const UProperty& InProperty, const UProperty* const InParentProperty )
		: Property( InProperty )
		, ParentProperty( InParentProperty )
	{}

	/** The property always exists */
	const UProperty& Property;

	/** The parent property may not exist */
	const UProperty* const ParentProperty;
};

/** Delegate called to see if a property should be visible */
DECLARE_DELEGATE_RetVal_OneParam( bool, FIsPropertyVisible, const FPropertyAndParent& );

/** Delegate called to see if a property should be visible */
DECLARE_DELEGATE_RetVal_OneParam( bool, FIsPropertyReadOnly, const FPropertyAndParent& );

/** Delegate called to get a detail layout for a specific object class */
DECLARE_DELEGATE_RetVal( TSharedRef<class IDetailCustomization>, FOnGetDetailCustomizationInstance );

/** Delegate called to get a property layout for a specific property type */
DECLARE_DELEGATE_RetVal( TSharedRef<class IPropertyTypeCustomization>, FOnGetPropertyTypeCustomizationInstance );

/** Notification for when a property view changes */
DECLARE_DELEGATE_TwoParams( FOnObjectArrayChanged, const FString&, const TArray< TWeakObjectPtr< UObject > >& );

/** Notification for when displayed properties changes (for instance, because the user has filtered some properties */
DECLARE_DELEGATE( FOnDisplayedPropertiesChanged );

/** Notification for when a property selection changes. */
DECLARE_DELEGATE_OneParam( FOnPropertySelectionChanged, UProperty* )

/** Notification for when a property is double clicked by the user*/
DECLARE_DELEGATE_OneParam( FOnPropertyDoubleClicked, UProperty* )

/** Notification for when a property is clicked by the user*/
DECLARE_DELEGATE_OneParam( FOnPropertyClicked, const TSharedPtr< class FPropertyPath >& )

/** */
DECLARE_DELEGATE_OneParam( FConstructExternalColumnHeaders, const TSharedRef< class SHeaderRow >& )

DECLARE_DELEGATE_RetVal_TwoParams( TSharedRef< class SWidget >, FConstructExternalColumnCell,  const FName& /*ColumnName*/, const TSharedRef< class IPropertyTreeRow >& /*RowWidget*/)

/** Delegate called to see if a property editing is enabled */
DECLARE_DELEGATE_RetVal(bool, FIsPropertyEditingEnabled );

/**
 * A delegate which is called after properties have been edited and PostEditChange has been called on all objects.
 * This can be used to safely make changes to data that the details panel is observing instead of during PostEditChange (which is
 * unsafe)
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FOnFinishedChangingProperties, const FPropertyChangedEvent&);


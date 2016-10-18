// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	UnRedirector.h: Object redirector definition.
=============================================================================*/

#pragma once

#include "ObjectBase.h"

/**
 * This class will redirect an object load to another object, so if an object is renamed
 * to a different package or group, external references to the object can be found
 */
class UObjectRedirector : public UObject
{
	DECLARE_CASTED_CLASS_INTRINSIC_WITH_API(UObjectRedirector, UObject, 0, TEXT("/Script/CoreUObject"), CASTCLASS_None, COREUOBJECT_API)

	// Variables.
	UObject*		DestinationObject;
	// UObject interface.
	virtual void PreSave(const class ITargetPlatform* TargetPlatform) override;
	void Serialize( FArchive& Ar ) override;
	virtual bool NeedsLoadForClient() const override;
	virtual bool NeedsLoadForEditorGame() const override;
	virtual void GetAssetRegistryTags(TArray<FAssetRegistryTag>& OutTags) const override;

	/**
	 * Callback for retrieving a textual representation of natively serialized properties.  Child classes should implement this method if they wish
	 * to have natively serialized property values included in things like diffcommandlet output.
	 *
	 * @param	out_PropertyValues	receives the property names and values which should be reported for this object.  The map's key should be the name of
	 *								the property and the map's value should be the textual representation of the property's value.  The property value should
	 *								be formatted the same way that UProperty::ExportText formats property values (i.e. for arrays, wrap in quotes and use a comma
	 *								as the delimiter between elements, etc.)
	 * @param	ExportFlags			bitmask of EPropertyPortFlags used for modifying the format of the property values
	 *
	 * @return	return true if property values were added to the map.
	 */
	virtual bool GetNativePropertyValues( TMap<FString,FString>& out_PropertyValues, uint32 ExportFlags=0 ) const override;
};

/**
 * Callback structure to respond to redirect-followed callbacks to determine
 * if a redirector was followed from a single object name. Will auto
 * register and unregister the callback on construction/destruction
 */
class COREUOBJECT_API FScopedRedirectorCatcher
{
public:
	/**
	 * Constructor for the callback device, will register itself with the RedirectorFollowed delegate
	 *
	 * @param InObjectPathNameToMatch Full pathname of the object refrence that is being compiled by script
	 */
	FScopedRedirectorCatcher(const FString& InObjectPathNameToMatch);

	/**
	 * Destructor. Will unregister the callback
	 */
	~FScopedRedirectorCatcher();

	/**
	 * Returns whether or not a redirector was followed for the ObjectPathName
	 */
	inline bool WasRedirectorFollowed()
	{
		return bWasRedirectorFollowed;
	}

	/**
	 * Responds to FCoreDelegates::RedirectorFollowed. Records all followed redirections
	 * so they can be cleaned later.
	 *
	 * @param InString Name of the package that pointed to the redirect
	 * @param InObject The UObjectRedirector that was followed
	 */
	virtual void OnRedirectorFollowed( const FString& InString, UObject* InObject);

private:
	/** The full path name of the object that we want to match */
	FString ObjectPathNameToMatch;

	/** Was a redirector followed, ONLY for the ObjectPathNameToMatch? */
	bool bWasRedirectorFollowed;
};

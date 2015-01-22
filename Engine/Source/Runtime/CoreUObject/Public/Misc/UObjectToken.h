// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "TokenizedMessage.h"
#include "WeakObjectPtr.h"

/**
 * A Message Log token that links to an object, with default behavior to link to the object
 * in the content browser/scene.
 */
class FUObjectToken : public IMessageToken
{
public:
	/** Factory method, tokens can only be constructed as shared refs */
	COREUOBJECT_API static TSharedRef<FUObjectToken> Create( UObject* InObject, const FText& InLabelOverride = FText() )
	{
		return MakeShareable(new FUObjectToken(InObject, InLabelOverride));
	}

	/** Begin IMessageToken interface */
	virtual EMessageToken::Type GetType() const override
	{
		return EMessageToken::Object;
	}

	virtual const FOnMessageTokenActivated& GetOnMessageTokenActivated() const override;
	/** End IMessageToken interface */

	/** Get the object referenced by this token */
	virtual const FWeakObjectPtr& GetObject() const
	{
		return ObjectBeingReferenced;
	}

	/** Get the delegate for default token activation */
	COREUOBJECT_API static FOnMessageTokenActivated& DefaultOnMessageTokenActivated()
	{
		return DefaultMessageTokenActivated;
	}

	/** Get the delegate for displaying the object name */
	DECLARE_DELEGATE_RetVal_TwoParams(FText, FOnGetDisplayName, UObject*, bool);
	COREUOBJECT_API static FOnGetDisplayName& DefaultOnGetObjectDisplayName()
	{
		return DefaultGetObjectDisplayName;
	}

private:
	/** Private constructor */
	FUObjectToken( UObject* InObject,  const FText& InLabelOverride );

	/** An object being referenced by this token, if any */
	FWeakObjectPtr ObjectBeingReferenced;

	/** The default activation method, if any */
	static FOnMessageTokenActivated DefaultMessageTokenActivated;

	/** The default object name method, if any */
	static FOnGetDisplayName DefaultGetObjectDisplayName;
};
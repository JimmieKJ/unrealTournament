// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "UObjectToken.h"

FOnMessageTokenActivated FUObjectToken::DefaultMessageTokenActivated;

FUObjectToken::FOnGetDisplayName FUObjectToken::DefaultGetObjectDisplayName;

FUObjectToken::FUObjectToken( UObject* InObject,  const FText& InLabelOverride )
	: ObjectBeingReferenced(InObject)
{
	if ( !InLabelOverride.IsEmpty() )
	{
		CachedText = InLabelOverride;
	}
	else
	{
		if ( DefaultGetObjectDisplayName.IsBound() )
		{
			CachedText = DefaultGetObjectDisplayName.Execute(InObject, false);
		}
		else if ( InObject )
		{
			CachedText = FText::FromString( InObject->GetName() );
		}
		else
		{
			CachedText = NSLOCTEXT("MessageLog", "NoneObjectToken", "<None>");
		}
	}
}

const FOnMessageTokenActivated& FUObjectToken::GetOnMessageTokenActivated() const
{
	if(MessageTokenActivated.IsBound())
	{
		return MessageTokenActivated;
	}
	else
	{
		return DefaultMessageTokenActivated;
	}
}
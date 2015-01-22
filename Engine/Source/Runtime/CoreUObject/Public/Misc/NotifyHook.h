// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CoreMisc.h: General-purpose file utilities.
=============================================================================*/

#pragma once
 

// Notification hook.
class COREUOBJECT_API FNotifyHook
{
public:
	virtual void NotifyPreChange( UProperty* PropertyAboutToChange ) {}
	virtual void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, UProperty* PropertyThatChanged ) {}
	virtual void NotifyPreChange( class FEditPropertyChain* PropertyAboutToChange );
	virtual void NotifyPostChange( const FPropertyChangedEvent& PropertyChangedEvent, class FEditPropertyChain* PropertyThatChanged );
};

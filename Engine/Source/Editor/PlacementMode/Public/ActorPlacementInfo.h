// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

class FActorPlacementInfo 
{
public:

	FActorPlacementInfo( const FString& InObjectPath, const FString& InFactory )
		: ObjectPath( InObjectPath )
		, Factory( InFactory )
	{
	}

	FActorPlacementInfo( const FString& String )
	{
		if ( !String.Split( FString( TEXT(";") ), &ObjectPath, &Factory ) )
		{
			ObjectPath = String;
		}
	}

	bool operator==( const FActorPlacementInfo& Other ) const
	{
		return ObjectPath == Other.ObjectPath;
	}

	bool operator!=( const FActorPlacementInfo& Other ) const
	{
		return ObjectPath != Other.ObjectPath;
	}

	FString ToString() const
	{
		return ObjectPath + TEXT(";") + Factory;
	}


public:

	FString ObjectPath;
	FString Factory;
};
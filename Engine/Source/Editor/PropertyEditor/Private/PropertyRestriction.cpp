// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "PropertyRestriction.h"

FText FPropertyRestriction::GetReason()const
{
	return Reason; 
}

bool FPropertyRestriction::IsValueRestricted(const FString& InValue)const
{
	for( int i = 0; i < Values.Num(); ++i )
	{
		if( InValue == Values[i] )
		{
			return true;
		}
	}
	return false;
}

void FPropertyRestriction::AddValue(const FString& InValue)
{
	Values.Add(InValue);
}

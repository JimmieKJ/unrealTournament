// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LiveEditorListenServerPrivatePCH.h"
#include "LiveEditorTransactionHistory.h"

FLiveEditorTransactionHistory::FLiveEditorTransactionHistory()
{
}

void FLiveEditorTransactionHistory::AppendTransaction( const FString &ClassName, const FString &PropertyName, const FString &PropertyValue )
{
	TMap<FString,FString> *ExistingValues = EditHistory.Find( ClassName );
	if ( ExistingValues != NULL )
	{
		ExistingValues->Add( PropertyName, PropertyValue );
	}
	else
	{
		TMap<FString,FString> NewMap;
		NewMap.Add( PropertyName, PropertyValue );
		EditHistory.Add( ClassName, NewMap );
	}
}

void FLiveEditorTransactionHistory::FindTransactionsForObject( const UObject *Object, TMap<FString,FString> &OutValues ) const
{
	for ( TMap< FString, TMap<FString,FString> >::TConstIterator ClassIt(EditHistory); ClassIt; ++ClassIt )
	{
		const FString &ClassName = (*ClassIt).Key;
		UClass *BaseClass = FindObject<UClass>(ANY_PACKAGE, *ClassName);
		if ( BaseClass == NULL || !Object->IsA(BaseClass) )
			continue;

		const TMap<FString,FString> *ExistingValues = EditHistory.Find( ClassName );
		if ( ExistingValues == NULL )
			continue;

		for ( TMap<FString,FString>::TConstIterator TransactionIt(*ExistingValues); TransactionIt; ++TransactionIt )
		{
			const FString &PropertyName = (*TransactionIt).Key;
			const FString &PropertyValue = (*TransactionIt).Value;
			OutValues.Add( PropertyName, PropertyValue );
		}
	}
}

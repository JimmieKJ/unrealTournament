// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FLiveEditorTransactionHistory
{
public:
	FLiveEditorTransactionHistory();

	void AppendTransaction( const FString &ClassName, const FString &PropertyName, const FString &PropertyValue );
	void FindTransactionsForObject( const UObject *Object, TMap<FString,FString> &OutValues ) const;

private:
	TMap< FString, TMap<FString,FString> > EditHistory;
};

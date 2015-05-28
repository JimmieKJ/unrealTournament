// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "SlateBasics.h"
#include "AssetData.h"

struct FSearchEntry
{
	FString Title;
	FString URL;
	bool bCategory;
#if WITH_EDITOR
	FAssetData AssetData;
#endif
	static FSearchEntry* MakeCategoryEntry(const FString & InTitle);
};



class FSuperSearchModule : public IModuleInterface
{
public:
	/* 
	 * Delegate for search text changing.
	 * @param Search string
	 * @param Suggestion array
	 */
	DECLARE_MULTICAST_DELEGATE_TwoParams(FSuperSearchTextChanged, const FString&, TArray< TSharedPtr<FSearchEntry> >&);
	/* 
	 * Delegate for search text click.
	 * @param Search entry clicked.
	 */	
	DECLARE_MULTICAST_DELEGATE_OneParam(FActOnSuperSearchEntry, TSharedPtr<FSearchEntry> );

	virtual void StartupModule();
	virtual void ShutdownModule();

	// Get the search text changed delegate
	FSuperSearchTextChanged& GetSearchTextChanged()
	{
		return OnSearchTextChanged;
	}
	// Get the search hit clicked changed delegate
	FActOnSuperSearchEntry& GetActOnSearchTextClicked()
	{
		return OnActOnSearchEntry;
	}
	
	/** Generates a SuperSearch box widget.  Remember, this widget will become invalid if the SuperSearch DLL is unloaded on the fly. */
	virtual TSharedRef< SWidget > MakeSearchBox(TSharedPtr< SEditableTextBox >& OutExposedEditableTextBox, const TOptional<const FSearchBoxStyle*> InStyle = TOptional<const FSearchBoxStyle*>()) const;
private:
	
	FSuperSearchTextChanged	OnSearchTextChanged;	
	FActOnSuperSearchEntry OnActOnSearchEntry;
};

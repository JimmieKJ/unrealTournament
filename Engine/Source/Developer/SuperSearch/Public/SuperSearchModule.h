// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "UObject/ObjectMacros.h"
#include "Widgets/SWidget.h"
#if WITH_EDITOR
#include "AssetData.h"
#endif
#include "Modules/ModuleInterface.h"
#include "Modules/ModuleManager.h"

class SEditableTextBox;
class SSuperSearchBox;
struct FSuperSearchStyle;

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

UENUM()
enum class ESearchEngine : uint8
{
	Google,
	Bing
};

class SSuperSearchBox;

class SUPERSEARCH_API FSuperSearchModule : public IModuleInterface
{
public:
	FSuperSearchModule();

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

	/**
	* Singleton-like access to this module's interface.  This is just for convenience!
	* Beware of calling this during the shutdown phase, though.  Your module might have been unloaded already.
	*
	* @return Returns singleton instance, loading the module on demand if needed
	*/
	static inline FSuperSearchModule& Get()
	{
		return FModuleManager::LoadModuleChecked< FSuperSearchModule >("SuperSearch");
	}

	/**
	* Checks to see if this module is loaded and ready.  It is only valid to call Get() if IsAvailable() returns true.
	*
	* @return True if the module is loaded and ready to use
	*/
	static inline bool IsAvailable()
	{
		return FModuleManager::Get().IsModuleLoaded("SuperSearch");
	}

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

	void SetSearchEngine(ESearchEngine SearchEngine);
	
	/** Generates a SuperSearch box widget.  Remember, this widget will become invalid if the SuperSearch DLL is unloaded on the fly. */
	virtual TSharedRef< SWidget > MakeSearchBox(TSharedPtr< SEditableTextBox >& OutExposedEditableTextBox, const TOptional<const FSuperSearchStyle*> InStyle = TOptional<const FSuperSearchStyle*>()) const;

private:
	
	FSuperSearchTextChanged	OnSearchTextChanged;	
	FActOnSuperSearchEntry OnActOnSearchEntry;

	ESearchEngine SearchEngine;

	mutable TArray< TWeakPtr<SSuperSearchBox> > SuperSearchBoxes;
};

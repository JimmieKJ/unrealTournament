// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SuperSearchPrivatePCH.h"
#include "SSuperSearch.h"

IMPLEMENT_MODULE( FSuperSearchModule,SuperSearch );

namespace SuperSearchModule
{
	static const FName FNameSuperSearchApp = FName(TEXT("SuperSearchApp"));
}

FSearchEntry * FSearchEntry::MakeCategoryEntry(const FString & InTitle)
{
	FSearchEntry * SearchEntry = new FSearchEntry();
	SearchEntry->Title = InTitle;
	SearchEntry->bCategory = true;

	return SearchEntry;
}


void FSuperSearchModule::StartupModule()
{
}

void FSuperSearchModule::ShutdownModule()
{
}

TSharedRef< SWidget > FSuperSearchModule::MakeSearchBox(TSharedPtr< SEditableTextBox >& OutExposedEditableTextBox, const FString& ConfigFilename, const TOptional<const FSearchBoxStyle*> InStyle, const TOptional<const FComboBoxStyle*> InSearchEngineStyle) const
{
	ESearchEngine CurrentSearchEngine = ESearchEngine::Google;

	int32 SearchEngineInt;
	if ( GConfig->GetInt(TEXT("SuperSearch"), TEXT("SearchEngine"), SearchEngineInt, ConfigFilename) )
	{
		CurrentSearchEngine = (ESearchEngine)SearchEngineInt;
	}

	TSharedRef< SSuperSearchBox > NewSearchBox =
		SNew(SSuperSearchBox)
		.Style(InStyle)
		.SearchEngineComboBoxStyle(InSearchEngineStyle)
		.SearchEngine(CurrentSearchEngine)
		.OnSearchEngineChanged_Lambda(
			[=] (ESearchEngine NewSearchEngine)
			{
				GConfig->SetInt(TEXT("SuperSearch"), TEXT("SearchEngine"), (int32)NewSearchEngine, ConfigFilename);
				GConfig->Flush(false, ConfigFilename);
			});

	OutExposedEditableTextBox = NewSearchBox->GetEditableTextBox();
	return NewSearchBox;
}

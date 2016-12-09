// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SuperSearchModule.h"
#include "Widgets/DeclarativeSyntaxSupport.h"
#include "Application/SlateWindowHelper.h"
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

FSuperSearchModule::FSuperSearchModule()
	: SearchEngine(ESearchEngine::Google)
{
}

void FSuperSearchModule::StartupModule()
{
}

void FSuperSearchModule::ShutdownModule()
{
}

void FSuperSearchModule::SetSearchEngine(ESearchEngine InSearchEngine)
{
	SearchEngine = InSearchEngine;

	for ( TWeakPtr<SSuperSearchBox>& SearchBoxPtr : SuperSearchBoxes )
	{
		TSharedPtr<SSuperSearchBox> SearchBox = SearchBoxPtr.Pin();
		if ( SearchBox.IsValid() )
		{
			SearchBox->SetSearchEngine(SearchEngine);
		}
	}
}

TSharedRef< SWidget > FSuperSearchModule::MakeSearchBox(TSharedPtr< SEditableTextBox >& OutExposedEditableTextBox, const TOptional<const FSuperSearchStyle*> InStyle) const
{
	// Remove any search box that has expired.
	for ( int32 i = SuperSearchBoxes.Num() - 1; i >= 0; i-- )
	{
		if ( !SuperSearchBoxes[i].IsValid() )
		{
			SuperSearchBoxes.RemoveAtSwap(i);
		}
	}

	TSharedRef< SSuperSearchBox > NewSearchBox =
		SNew(SSuperSearchBox)
		.Style(InStyle)
		.SearchEngine(SearchEngine);

	OutExposedEditableTextBox = NewSearchBox->GetEditableTextBox();

	SuperSearchBoxes.Add(NewSearchBox);

	return NewSearchBox;
}

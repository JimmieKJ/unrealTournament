// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "SuperSearchPrivatePCH.h"
#include "SSearchBox.h"
#include "SSuperSearch.h"
#include "SScrollBorder.h"

#if WITH_EDITOR
#include "AssetRegistryModule.h"
#include "IIntroTutorials.h"
#include "EditorTutorial.h"
#endif

#include "SSearchBox.h"

#if WITH_EDITOR	
#include "AssetData.h"
#endif

static TSharedRef<FSearchEntry> OtherCategory(new FSearchEntry());
static TSharedRef<FSearchEntry> AskQuestionEntry (new FSearchEntry());

SSuperSearchBox::SSuperSearchBox()
	: SelectedSuggestion(-1)
	, bIgnoreUIUpdate(false)
{
	CategoryToIconMap.Add("Documentation", FName("LevelEditor.BrowseDocumentation") );
	CategoryToIconMap.Add("Wiki", FName("MainFrame.VisitWiki") );
	CategoryToIconMap.Add("Answerhub", FName("MainFrame.VisitSearchForAnswersPage") );
	CategoryToIconMap.Add("API", FName("LevelEditor.BrowseAPIReference") );
	CategoryToIconMap.Add("Tutorials", FName("LevelEditor.Tutorials") );


	OtherCategory->Title = NSLOCTEXT("SuperSearchBox", "OtherCategory", "Help").ToString();
	OtherCategory->bCategory = true;

	AskQuestionEntry->Title = NSLOCTEXT("SuperSearchBox", "AskQuestion", "Ask Online").ToString();
	AskQuestionEntry->URL = "https://answers.unrealengine.com/questions/ask.html";
	AskQuestionEntry->bCategory = false;
	
}

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION
void SSuperSearchBox::Construct( const FArguments& InArgs )
{
	// Allow style to be optionally overridden, but fallback to SSearchBox default if not specified
	const FSearchBoxStyle* InStyle = InArgs._Style.IsSet() ? InArgs._Style.GetValue() : &FCoreStyle::Get().GetWidgetStyle<FSearchBoxStyle>("SearchBox");

	ChildSlot
	[
		SAssignNew( SuggestionBox, SMenuAnchor )
			.Placement( InArgs._SuggestionListPlacement )
		.Method( EPopupMethod::UseCurrentWindow )
			[
				SAssignNew(InputText, SSearchBox)
					.Style(InStyle)
					.OnTextCommitted(this, &SSuperSearchBox::OnTextCommitted)
					.HintText( NSLOCTEXT( "SuperSearchBox", "HelpHint", "Search For Help" ) )
					.OnTextChanged(this, &SSuperSearchBox::OnTextChanged)
					.SelectAllTextWhenFocused(false)
					.DelayChangeNotificationsWhileTyping(true)
					.MinDesiredWidth(200)
			]
			.MenuContent
			(
				SNew(SBorder)
				.BorderImage(FCoreStyle::Get().GetBrush("Menu.Background"))
				.Padding( FMargin(2) )
				[
					SNew(SBox)
				//.HeightOverride(250) // avoids flickering, ideally this would be adaptive to the content without flickering
					[
						SAssignNew(SuggestionListView, SListView< TSharedPtr<FSearchEntry> >)
							.ListItemsSource(&Suggestions)
							.SelectionMode( ESelectionMode::Single )							// Ideally the mouse over would not highlight while keyboard controls the UI
							.OnGenerateRow(this, &SSuperSearchBox::MakeSuggestionListItemWidget)
						.OnMouseButtonClick(this, &SSuperSearchBox::SuggestionSelectionChanged)
							.ItemHeight(18)
					]
				]
			)
			.OnMenuOpenChanged(this, &SSuperSearchBox::OnMenuOpenChanged)
	];
}
END_SLATE_FUNCTION_BUILD_OPTIMIZATION

int32 GetNumRealSuggestions(const TArray< TSharedPtr<FSearchEntry> > & Suggestions)
{
	int32 NumSuggestions = 0;
	for (auto Entry : Suggestions)
	{
		if (Entry->bCategory == false)
	{
			++NumSuggestions;
	}
	}

	return NumSuggestions;
}

void SSuperSearchBox::OnMenuOpenChanged( bool bIsOpen )
{
#if WITH_EDITOR
	if (bIsOpen == false)
	{
		if(FEngineAnalytics::IsAvailable())
		{
			TArray< FAnalyticsEventAttribute > SearchAttribs;
			SearchAttribs.Add(FAnalyticsEventAttribute(TEXT("SearchTerm"), InputText->GetText().ToString() ));
			SearchAttribs.Add(FAnalyticsEventAttribute(TEXT("NumFoundResults"), GetNumRealSuggestions(Suggestions)));
			SearchAttribs.Add(FAnalyticsEventAttribute(TEXT("ClickedTitle"), EntryClicked.Get() ? EntryClicked->Title : "") );
			SearchAttribs.Add(FAnalyticsEventAttribute(TEXT("ClickedOnSomething"), EntryClicked.IsValid()) );

			FEngineAnalytics::GetProvider().RecordEvent(TEXT("Editor.Usage.SuperSearch"), SearchAttribs);
		}

		InputText->SetText(FText::GetEmpty());
	}
#endif
	EntryClicked = NULL;
}

/** Input string should have no leading & or ? */
static FString AddQueryStringToURL(FString const& URL, TCHAR const* StringToAdd)
{
	if (StringToAdd)
	{
		// split url into path, querystring, and anchor parts
		FString Anchor;
		FString Path;
		FString QueryString;
		FString PathAndQueryString;
		if (URL.Split(TEXT("#"), &PathAndQueryString, &Anchor) == false)
		{
			PathAndQueryString = URL;
		}
		if (PathAndQueryString.Split(TEXT("?"), &Path, &QueryString) == false)
		{
			Path = URL;
		}

		if (QueryString.IsEmpty())
		{
			QueryString = FString::Printf(TEXT("?%s"), StringToAdd);
		}
		else
		{
			QueryString = FString::Printf(TEXT("%s&%s"), *QueryString, StringToAdd);
		}

		// reassemble and return
		return Path + QueryString + Anchor;
	}

	return URL;
}

void SSuperSearchBox::ActOnSuggestion(TSharedPtr<FSearchEntry> SearchEntry, FString const& Category)
{
	if (SearchEntry->bCategory == false)
	{
		EntryClicked = SearchEntry;
#if WITH_EDITOR
		// Broadcast text change to anyone registered for it
		FSuperSearchModule& SuperSearchModule = FModuleManager::LoadModuleChecked< FSuperSearchModule >(TEXT("SuperSearch"));		
		SuperSearchModule.GetActOnSearchTextClicked().Broadcast(SearchEntry);

		// See if the search has tutorial hits
		if (SearchEntry->URL.IsEmpty())
		{
			UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, *(SearchEntry->AssetData.ObjectPath.ToString()));
			if( Blueprint != nullptr )
			{
				UEditorTutorial* Tutorial = Blueprint->GeneratedClass->GetDefaultObject<UEditorTutorial>();
				if( Tutorial!= nullptr)
				{
					IIntroTutorials& IntroTutorials = IIntroTutorials::Get();

					FWidgetPath OutWidgetPath;
					TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(SuggestionBox.ToSharedRef(), OutWidgetPath);

					IntroTutorials.LaunchTutorial(Tutorial, IIntroTutorials::ETutorialStartType::TST_RESTART, ParentWindow);
				}
			} 
		}
		else
#endif
		{
			// Check for documentation hits
			if (Category == TEXT("documentation"))
			{
				// append some tracking data to the URL
				FString const FinalURL = AddQueryStringToURL(SearchEntry->URL, TEXT("utm_source=editor&utm_medium=search&utm_campaign=user_initiated"));
				FPlatformProcess::LaunchURL(*FinalURL, NULL, NULL);
			}
			else
			{
				FPlatformProcess::LaunchURL(*SearchEntry->URL, NULL, NULL);
			}
		}

		SuggestionBox->SetIsOpen(false);
	}
}

void SSuperSearchBox::SuggestionSelectionChanged(TSharedPtr<FSearchEntry> NewValue)
{
	if(bIgnoreUIUpdate)
	{
		return;
	}

	if (NewValue.Get() == NULL || NewValue->bCategory)
	{
		return;
	}

	int32 CurrentCategory = INDEX_NONE;
	for(int32 i = 0; i < Suggestions.Num(); ++i)
	{
		if (Suggestions[i]->bCategory)
		{
			CurrentCategory = i;
		}

		if(NewValue == Suggestions[i])
		{
			SelectedSuggestion = i;
			MarkActiveSuggestion();

			FString const Category = CurrentCategory != INDEX_NONE ? Suggestions[CurrentCategory]->Title : FString();
			ActOnSuggestion(NewValue, Category);

			break;
		}
	}
}


TSharedRef<ITableRow> SSuperSearchBox::MakeSuggestionListItemWidget(TSharedPtr<FSearchEntry> Suggestion, const TSharedRef<STableViewBase>& OwnerTable)
{
	check(Suggestion.IsValid());

	FString Left, Right, Combined;

	if (Suggestion->bCategory)
	{
		Combined = "- ";
	}

	if(Suggestion->Title.Split(TEXT("\t"), &Left, &Right))
	{
		Combined += Left + Right;
	}
	else
	{
		Combined += Suggestion->Title;
	}

	bool bCategory = Suggestion->bCategory;


	TSharedPtr<SWidget> IconWidget;
	if (bCategory)
	{
#if WITH_EDITOR
		FName* IconResult = CategoryToIconMap.Find(Combined);
		SAssignNew(IconWidget, SImage)
			.Image(FEditorStyle::GetBrush(IconResult ? *IconResult : FName("MainFrame.VisitEpicGamesDotCom") ));
#endif
	}
	else
	{
		SAssignNew(IconWidget, SSpacer);
	}

	return
		SNew(STableRow< TSharedPtr<FString> >, OwnerTable)
		.ShowSelection(bCategory == false)
		.Padding(FMargin(bCategory ? 2.5 : 16, 2.5, 2.5, 2.5))
		[
			SNew(SHorizontalBox)
#if WITH_EDITOR
			+ SHorizontalBox::Slot()
			.Padding(FMargin(0,0,2.5,0))
			[
				IconWidget.ToSharedRef()
			]
#endif
			+SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(STextBlock)
				.Text(FText::FromString(Combined))
#if WITH_EDITOR
				.TextStyle(&FEditorStyle::Get().GetWidgetStyle<FTextBlockStyle>(bCategory ? "SuperSearchCategoryText" : "NormalText"))
#endif
				.MinDesiredWidth(600)
			]
			
		];
}

void SSuperSearchBox::OnTextChanged(const FText& InText)
{
	if(bIgnoreUIUpdate)
	{
		return;
	}

	const FString& InputTextStr = InputText->GetText().ToString();
	if(!InputTextStr.IsEmpty())
	{
		//search through google for online content
		{
			TSharedRef<class IHttpRequest> HttpRequest = FHttpModule::Get().CreateRequest();

			// build the url
				FString UrlEncodedString = FGenericPlatformHttp::UrlEncode(InText.ToString());	//we need to url encode for special characters (especially other languages)
			// use partial response to only look at items and things we care about for items right now (title,link and label name)
				const FText QueryURL = FText::Format(FText::FromString("https://www.googleapis.com/customsearch/v1?key=AIzaSyCMGfdDaSfjqv5zYoS0mTJnOT3e9MURWkU&cx=009868829633250020713:y7tfd8hlcgg&fields=items(title,link,labels/name)&q={0}+less:forums"), FText::FromString(UrlEncodedString));

			//save http request into map to ensure correct ordering
			FText & Query = RequestQueryMap.FindOrAdd(HttpRequest);
			Query = InText;

			

			// kick off http request to read friends
			HttpRequest->OnProcessRequestComplete().BindRaw(this, &SSuperSearchBox::Query_HttpRequestComplete);
			HttpRequest->SetURL(QueryURL.ToString());
			HttpRequest->SetHeader(TEXT("Content-Type"), TEXT("application/json"));
			HttpRequest->SetVerb(TEXT("GET"));
			HttpRequest->ProcessRequest();
		}

#if WITH_EDITOR
		//search local tutorials
		{
			FAssetRegistryModule& AssetRegistry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));

			FARFilter Filter;
			Filter.ClassNames.Add(UBlueprint::StaticClass()->GetFName());
			Filter.bRecursiveClasses = true;
			Filter.TagsAndValues.Add(TEXT("ParentClass"), FString::Printf(TEXT("%s'%s'"), *UClass::StaticClass()->GetName(), *UEditorTutorial::StaticClass()->GetPathName()));

			TArray<FAssetData> AssetData;
			AssetRegistry.Get().GetAssets(Filter, AssetData);

			//add search result into cache
			FSearchResults& SearchResults = SearchResultsCache.FindOrAdd(InText.ToString());
			TArray<FSearchEntry>& TutorialResults = SearchResults.TutorialResults;
			TutorialResults.Empty();

			for (const FAssetData& Asset : AssetData)
			{
				const FString* SearchTag = Asset.TagsAndValues.Find("SearchTags");
				const FString* ResultTitle = Asset.TagsAndValues.Find("Title");
				if( ResultTitle)
				{
					if (ResultTitle->Contains(InText.ToString()))
					{
						FSearchEntry SearchEntry;
						SearchEntry.Title = *ResultTitle;
						SearchEntry.URL = "";
						SearchEntry.bCategory = false;
						SearchEntry.AssetData = Asset;
						TutorialResults.Add(SearchEntry);
					}
				}

				// If the asset has search tags, search them
				if ((SearchTag) && (SearchTag->IsEmpty()== false))
				{
					TArray<FString> SearchTags;			
					SearchTag->ParseIntoArray(SearchTags,TEXT(","));
					//trim any xs spaces off the strings.
					for (int32 iTag = 0; iTag < SearchTags.Num() ; iTag++)
					{
						SearchTags[iTag] = SearchTags[iTag].Trim();
						SearchTags[iTag] = SearchTags[iTag].TrimTrailing();
					}
					if (SearchTags.Find(InText.ToString()) != INDEX_NONE)
					{
						FSearchEntry SearchEntry;
						SearchEntry.Title = *ResultTitle;
						SearchEntry.URL = "";
						SearchEntry.bCategory = false;
						SearchEntry.AssetData = Asset;
						TutorialResults.Add(SearchEntry);
					}
				}
			}
		}
#endif
	}
	else
	{
		ClearSuggestions();
	}
}

void SSuperSearchBox::OnTextCommitted( const FText& InText, ETextCommit::Type CommitInfo)
{
	if (CommitInfo == ETextCommit::OnEnter)
	{
		if (SelectedSuggestion >= 1)
		{
			// find the category
			FString Category;
			for (int32 i = SelectedSuggestion; i >= 0; --i)
			{
				if (Suggestions[i]->bCategory)
				{
					Category = Suggestions[i]->Title;
					break;
				}
			}

			ActOnSuggestion(Suggestions[SelectedSuggestion], Category);
		}
		else
		{
			OnTextChanged(InText);
		}

	}
}

void SSuperSearchBox::Query_HttpRequestComplete(FHttpRequestPtr HttpRequest, FHttpResponsePtr HttpResponse, bool bSucceeded)
{
	bool bResult = false;
	FString ResponseStr, ErrorStr;

	if (bSucceeded && HttpResponse.IsValid())
	{
		ResponseStr = HttpResponse->GetContentAsString();
		if (EHttpResponseCodes::IsOk(HttpResponse->GetResponseCode()))
		{
			// Create the Json parser
			TSharedPtr<FJsonObject> JsonObject;
			TSharedRef<TJsonReader<> > JsonReader = TJsonReaderFactory<>::Create(ResponseStr);

			if (FJsonSerializer::Deserialize(JsonReader, JsonObject) && JsonObject.IsValid())
			{
				if (FText * QueryText = RequestQueryMap.Find(HttpRequest))
				{
					const TArray<TSharedPtr<FJsonValue> > * JsonItems = nullptr;
					if (JsonObject->TryGetArrayField(TEXT("items"), JsonItems))
					{
						//add search result into cache
						FSearchResults & SearchResults = SearchResultsCache.FindOrAdd(QueryText->ToString());
						FCategoryResults & OnlineResults = SearchResults.OnlineResults;
						OnlineResults.Empty();	//reset online results since we just got updated

						for (TSharedPtr<FJsonValue> JsonItem : *JsonItems)
						{
							const TSharedPtr<FJsonObject> & ItemObject = JsonItem->AsObject();
							FSearchEntry SearchEntry;
							SearchEntry.Title = ItemObject->GetStringField("title");
							SearchEntry.URL = ItemObject->GetStringField("link");
							SearchEntry.bCategory = false;

							bool bValidLabel = false;
							TArray<TSharedPtr<FJsonValue> > Labels = ItemObject->GetArrayField(TEXT("labels"));
							for (TSharedPtr<FJsonValue> Label : Labels)
							{
								const TSharedPtr<FJsonObject> & LabelObject = Label->AsObject();
								FString LabelString = LabelObject->GetStringField(TEXT("name"));
								TArray<FSearchEntry> & SuggestionCategory = OnlineResults.FindOrAdd(LabelString);
								SuggestionCategory.Add(SearchEntry);
							}
						}
					}

					bResult = true;
				}
				
			}
		}
		else
		{
			ErrorStr = FString::Printf(TEXT("Invalid response. code=%d error=%s"),
				HttpResponse->GetResponseCode(), *ResponseStr);
		}
	}

	if (bResult)
	{
		UpdateSuggestions();
	}
}


FReply SSuperSearchBox::OnKeyDown( const FGeometry& MyGeometry, const FKeyEvent& KeyEvent )
{
	if(SuggestionBox->IsOpen() && Suggestions.Num())
	{
		if(KeyEvent.GetKey() == EKeys::Up || KeyEvent.GetKey() == EKeys::Down)
		{
			if(KeyEvent.GetKey() == EKeys::Up)
			{
				//if we're at the top swing around
				if(SelectedSuggestion == 1)
				{
					SelectedSuggestion = Suggestions.Num() - 1;
				}
				else
				{
					// got one up
					--SelectedSuggestion;

					//make sure we're not selecting category
					if (Suggestions[SelectedSuggestion]->bCategory)
					{
						//we know that category is never shown empty so above us will be fine
						--SelectedSuggestion;
					}
				}
			}

			if(KeyEvent.GetKey() == EKeys::Down)
			{
				if(SelectedSuggestion < Suggestions.Num() - 1)
				{
					// go one down, possibly from edit control to top
					++SelectedSuggestion;

					//make sure we're not selecting category
					if (Suggestions[SelectedSuggestion]->bCategory)
					{
						//we know that category is never shown empty so below us will be fine
						++SelectedSuggestion;
					}
				}
				else
				{
					// back to edit control
					SelectedSuggestion = 1;
				}
			}

			MarkActiveSuggestion();

			return FReply::Handled();
		}
	}
	
	return FReply::Unhandled();
}



void UpdateSuggestionHelper(const FText & CategoryLabel, const TArray<FSearchEntry> & Elements, TArray<TSharedPtr< FSearchEntry > > & OutSuggestions)
{
	if (Elements.Num())
	{
		OutSuggestions.Add(MakeShareable(FSearchEntry::MakeCategoryEntry(CategoryLabel.ToString())));
	}

	for (uint32 i = 0; i < (uint32)Elements.Num(); ++i)
	{
		OutSuggestions.Add(MakeShareable(new FSearchEntry(Elements[i])));
	}
}

void SSuperSearchBox::UpdateSuggestions()
{
	const FText & Query = InputText->GetText();
	FSearchResults * SearchResults = SearchResultsCache.Find(Query.ToString());

	//still waiting on results for current query
	if (SearchResults == NULL)
	{
		return;
	}

	//go through and build new suggestion list for list view widget
	Suggestions.Empty();
	SelectedSuggestion = -1;
	
	//first tutorials
	UpdateSuggestionHelper(NSLOCTEXT("SuperSearch", "tutorials", "Tutorials"), SearchResults->TutorialResults, Suggestions);

	//then docs
	UpdateSuggestionHelper(NSLOCTEXT("SuperSearch", "docs", "Documentation"), SearchResults->OnlineResults.FindOrAdd(TEXT("documentation")), Suggestions);

	//then api
	UpdateSuggestionHelper(NSLOCTEXT("SuperSearch", "API", "API"), SearchResults->OnlineResults.FindOrAdd(TEXT("api")), Suggestions);

	//then wiki
	UpdateSuggestionHelper(NSLOCTEXT("SuperSearch", "wiki", "Wiki"), SearchResults->OnlineResults.FindOrAdd(TEXT("wiki")), Suggestions);

	//then answerhub
	UpdateSuggestionHelper(NSLOCTEXT("SuperSearch", "answers", "Answerhub"), SearchResults->OnlineResults.FindOrAdd(TEXT("answers")), Suggestions);

	FSuperSearchModule& SuperSearchModule = FModuleManager::LoadModuleChecked< FSuperSearchModule >(TEXT("SuperSearch"));
	//Broadcast to anyone registered 
	SuperSearchModule.GetSearchTextChanged().Broadcast(InputText->GetText().ToString(),Suggestions);

	//finally add other category
	Suggestions.Add(OtherCategory);
	Suggestions.Add(AskQuestionEntry);

		SelectedSuggestion = 1;
		// Ideally if the selection box is open the output window is not changing it's window title (flickers)
		SuggestionBox->SetIsOpen(true, false);
		MarkActiveSuggestion();

		// Force the textbox back into focus.
	FSlateApplication::Get().SetKeyboardFocus(InputText.ToSharedRef(), EFocusCause::SetDirectly);
}

void SSuperSearchBox::OnFocusLost( const FFocusEvent& InFocusEvent )
{
//	SuggestionBox->SetIsOpen(false);
}

void SSuperSearchBox::MarkActiveSuggestion()
{
	bIgnoreUIUpdate = true;
	if(SelectedSuggestion >= 1)
	{
		SuggestionListView->SetSelection(Suggestions[SelectedSuggestion]);
		SuggestionListView->RequestScrollIntoView(Suggestions[SelectedSuggestion]);	// Ideally this would only scroll if outside of the view
	}
	else
	{
		SuggestionListView->ClearSelection();
	}
	bIgnoreUIUpdate = false;
}

void SSuperSearchBox::ClearSuggestions()
{
	SelectedSuggestion = -1;
	SuggestionBox->SetIsOpen(false);
	Suggestions.Empty();
}

FString SSuperSearchBox::GetSelectionText() const
{
	FString ret = Suggestions[SelectedSuggestion]->Title;
	
	ret = ret.Replace(TEXT("\t"), TEXT(""));

	return ret;
}

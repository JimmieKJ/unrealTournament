// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "InputBindingEditorPrivatePCH.h"
#include "SSearchBox.h"

#define LOCTEXT_NAMESPACE "SInputBindingEditorPanel"


/* SChordEditor interface
 *****************************************************************************/

void SInputBindingEditorPanel::Construct(const FArguments& InArgs)
{
	ChildSlot
	[
		SNew(SBorder)
		.BorderImage( FEditorStyle::GetBrush("ToolPanel.GroupBorder")  )
		.Padding( 5.0f )
		[
			SNew( SVerticalBox )
			+ SVerticalBox::Slot()
			.AutoHeight()
			.HAlign(HAlign_Left)
			[
				SNew( SBox )
				.WidthOverride( 300 )
				[
					SAssignNew( SearchBox, SSearchBox )
					.OnTextChanged( this, &SInputBindingEditorPanel::OnSearchChanged )
				]
			]
			+ SVerticalBox::Slot()
			.FillHeight(1)
			[
				SAssignNew( ChordTree, SChordTree )
				.TreeItemsSource( &ContextVisibleList )
				.OnGetChildren( this, &SInputBindingEditorPanel::OnGetChildrenForTreeItem )
				.OnGenerateRow( this, &SInputBindingEditorPanel::OnGenerateWidgetForTreeItem )
				.SelectionMode( ESelectionMode::None )
				.ItemHeight( 26 )
				.HeaderRow
				(
					SNew(SHeaderRow)
					+SHeaderRow::Column("Name")
					.FillWidth(.75)
					[
						SNew( SBox )
						.VAlign(VAlign_Center)
						[
							SNew( SButton )
							.ContentPadding( 0 )
							.ButtonStyle( FEditorStyle::Get(), "InputBindingEditor.HeaderButton" )
							.ForegroundColor( FSlateColor::UseForeground() )
							.Text( LOCTEXT("NameLabel", "Name") )
							.OnClicked( this, &SInputBindingEditorPanel::OnNameColumnClicked )
						]
					]
					+SHeaderRow::Column("Binding")
					.FillWidth(.25)
					[
						SNew( SBox )
						.VAlign(VAlign_Center)
						[
							SNew( SButton )
							.ContentPadding( 0 )
							.ButtonStyle( FEditorStyle::Get(), "InputBindingEditor.HeaderButton" )
							.ForegroundColor( FSlateColor::UseForeground() )
							.Text( LOCTEXT("BindingLabel", "Binding") )
							.OnClicked( this, &SInputBindingEditorPanel::OnBindingColumnClicked )
						]
					]		
				)
			]
		]
	];

	UpdateContextMasterList();

	FBindingContext::CommandsChanged.AddSP( SharedThis( this ), &SInputBindingEditorPanel::OnCommandsChanged );

	if( ContextVisibleList.Num() )
	{
		// Expand everything by default
		for( auto CurContextIt( ContextVisibleList.CreateConstIterator() ); CurContextIt; ++CurContextIt )
		{
			const bool bShouldExpand = true;
			ChordTree->SetItemExpansion( *CurContextIt, bShouldExpand );
		}
	}


	ChordTree->RequestTreeRefresh();
}


static bool StringPassesFilter( const FText& Text, const TArray<FString>& FilterStrings)
{
	//@todo Use localization safe comparison methods [10/11/2013 justin.sargent]
	FString String = Text.ToString();
	bool bAcceptable = true;

	// Check all filter strings against our filter match. If one matches then we should be visible
	for (int32 Index = 0; Index < FilterStrings.Num(); ++Index)
	{
		const FString& FilterString = FilterStrings[Index];

		if ( !String.Contains( FilterString ) ) 
		{
			bAcceptable = false;
			break;
		}
	}

	return bAcceptable;
}


void SInputBindingEditorPanel::OnSearchChanged( const FText& NewSearch )
{
	FilterStrings.Empty();

	FString ParseString = NewSearch.ToString();
	// Remove whitespace from the front and back of the string
	ParseString.Trim();
	ParseString.TrimTrailing();
	ParseString.ParseIntoArray(FilterStrings, TEXT(" "), true);

	FilterVisibleContextList();
}


FReply SInputBindingEditorPanel::OnBindingColumnClicked()
{
	static bool bSortUp = false;
	bSortUp = !bSortUp;

	const bool bSortName = false;

	ChordSortMode = FChordSort( bSortName, bSortUp );
	ChordTree->RequestTreeRefresh();
	return FReply::Handled();
}


FReply SInputBindingEditorPanel::OnNameColumnClicked()
{
	static bool bSortUp = false;
	bSortUp = !bSortUp;

	const bool bSortName = true;

	ChordSortMode = FChordSort( bSortName, bSortUp );
	ChordTree->RequestTreeRefresh();

	return FReply::Handled();
}


void SInputBindingEditorPanel::UpdateContextMasterList()
{
	ContextMasterList.Empty();
	ContextVisibleList.Empty();

	TArray< TSharedPtr<FBindingContext> > Contexts;

	FInputBindingManager::Get().GetKnownInputContexts( Contexts );
	
	struct FContextNameSort
	{
		bool operator()( const TSharedPtr<FBindingContext>& A, const TSharedPtr<FBindingContext>& B ) const
		{
			return A->GetContextDesc().CompareTo( B->GetContextDesc() ) == -1;
		}
	};

	Contexts.Sort( FContextNameSort() );

	for( int32 ListIndex = 0; ListIndex < Contexts.Num(); ++ListIndex )
	{
		const auto& Context = Contexts[ListIndex];

		TSharedRef<FChordTreeItem> TreeItem( new FChordTreeItem );
		TreeItem->BindingContext = Context;
		ContextMasterList.Add( TreeItem );
	}

	ContextVisibleList = ContextMasterList;
}


void SInputBindingEditorPanel::FilterVisibleContextList()
{
	if( FilterStrings.Num() == 0 )
	{
		ContextVisibleList = ContextMasterList;
	}
	else
	{
		ContextVisibleList.Empty();
		for( int32 Context = 0; Context < ContextMasterList.Num(); ++Context )
		{
			bool bContextIsVisible = true;
			TSharedPtr<FBindingContext> BindingContext = ContextMasterList[Context]->GetBindingContext();
			// If the context doesn't pass the filter we'll need to check its children to see if they pass the filter.
			// If no children pass the filter then the context isnt visible
			if( !StringPassesFilter( BindingContext->GetContextDesc(), FilterStrings ) )
			{
				bContextIsVisible = false;
				TArray< TSharedPtr<FUICommandInfo> > CommandInfos;
				FInputBindingManager::Get().GetCommandInfosFromContext( BindingContext->GetContextName(), CommandInfos );

				for( int32 CommandIndex = 0; CommandIndex < CommandInfos.Num(); ++CommandIndex )
				{
					if( StringPassesFilter( CommandInfos[CommandIndex]->GetLabel(), FilterStrings ) || StringPassesFilter( CommandInfos[CommandIndex]->GetInputText(), FilterStrings ) )
					{
						// At least one child is visible so this context is visible
						bContextIsVisible = true;
						// we can stop searching
						break;
					}
				}
			}

			if( bContextIsVisible )
			{
				
				ContextVisibleList.Add( ContextMasterList[Context] );

				// Expand all filtered contexts
				ChordTree->SetItemExpansion( ContextMasterList[Context], true );
			}
		}
	}

	ChordTree->RequestTreeRefresh();
}


void SInputBindingEditorPanel::OnCommandsChanged()
{
	UpdateContextMasterList();

	// Make sure we aren't restarting after importing/resetting to default
	if (FSlateApplication::IsInitialized())
	{
		FilterVisibleContextList();
	}
	
}


TSharedRef< ITableRow > SInputBindingEditorPanel::OnGenerateWidgetForTreeItem( TSharedPtr<FChordTreeItem> InTreeItem, const TSharedRef<STableViewBase>& OwnerTable )
{
	if( InTreeItem->IsContext() )
	{
		static const FName InvertedForegroundName("InvertedForeground");
		// contexts do not need columns
		return
			SNew( STableRow< TSharedPtr<FChordTreeItem> >, OwnerTable )
			.Padding(3)
			[
				SNew( SBorder )
				.BorderImage( FEditorStyle::GetBrush( "InputBindingEditor.ContextBorder" ) )
				.VAlign(VAlign_Center)
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(1, 0, 0, 0))
					[
						SNew( SImage )
						.Image( FEditorStyle::GetOptionalBrush( FEditorStyle::Join( FName( TEXT("InputBindingEditor.") ), TCHAR_TO_ANSI( *InTreeItem->GetBindingContext()->GetContextName().ToString() ) ) ) )
						.ColorAndOpacity( FLinearColor::Black )
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(FMargin(2, 0, 0, 0))
					[
						SNew( STextBlock )
						.Font( FEditorStyle::GetFontStyle( "InputBindingEditor.ContextFont" ) )
						.ColorAndOpacity(FEditorStyle::GetSlateColor(InvertedForegroundName) )
						.Text( InTreeItem->GetBindingContext()->GetContextDesc() )
					]
				]
			];
	}
	else
	{
		return SNew( SChordTreeItem, OwnerTable, InTreeItem );
	}
}


void SInputBindingEditorPanel::OnGetChildrenForTreeItem( TSharedPtr<FChordTreeItem> InTreeItem, TArray< TSharedPtr< FChordTreeItem > >& OutChildren )
{
	if( InTreeItem->IsContext() )
	{
		TArray< TSharedPtr<FUICommandInfo> > CommandInfos;
		FInputBindingManager::Get().GetCommandInfosFromContext( InTreeItem->GetBindingContext()->GetContextName(), CommandInfos );

		bool bContextPassesFilter = StringPassesFilter( InTreeItem->GetBindingContext()->GetContextDesc(), FilterStrings );

		for( int32 CommandIndex = 0; CommandIndex < CommandInfos.Num(); ++CommandIndex )
		{
			if( FilterStrings.Num() == 0 || bContextPassesFilter || StringPassesFilter( CommandInfos[CommandIndex]->GetLabel(), FilterStrings ) || StringPassesFilter( CommandInfos[CommandIndex]->GetInputText(), FilterStrings ) )
			{
				TSharedPtr<FChordTreeItem> NewItem( new FChordTreeItem );
				NewItem->CommandInfo = CommandInfos[ CommandIndex ];
				OutChildren.Add( NewItem );
			}
		}

		OutChildren.Sort( ChordSortMode );
	}
}


bool SInputBindingEditorPanel::SupportsKeyboardFocus() const
{
	return SearchBox->SupportsKeyboardFocus();
}


FReply SInputBindingEditorPanel::OnFocusReceived( const FGeometry& MyGeometry, const FFocusEvent& InFocusEvent )
{
	FReply Reply = FReply::Handled();

	if( InFocusEvent.GetCause() != EFocusCause::Cleared )
	{
		Reply.SetUserFocus(SearchBox.ToSharedRef(), InFocusEvent.GetCause());
	}

	return Reply;
}


#undef LOCTEXT_NAMESPACE

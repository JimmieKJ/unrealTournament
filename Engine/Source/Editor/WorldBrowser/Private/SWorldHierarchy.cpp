// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "WorldBrowserPrivatePCH.h"

#include "Editor/LevelEditor/Public/LevelEditor.h"
#include "TextFilter.h"
#include "SWorldHierarchy.h"
#include "SWorldHierarchyItem.h"

#include "Editor/UnrealEd/Public/AssetSelection.h"
#include "Editor/UnrealEd/Public/DragAndDrop/AssetDragDropOp.h"
#include "SSearchBox.h"

#define LOCTEXT_NAMESPACE "WorldBrowser"

typedef TTextFilter<const FLevelModel* > LevelTextFilter;

class SLevelsTreeWidget : public STreeView<TSharedPtr<FLevelModel>>
{
public:
	void Construct(const FArguments& InArgs, const TSharedPtr<FLevelCollectionModel>& InWorldModel)
	{
		STreeView<TSharedPtr<FLevelModel>>::Construct(InArgs);

		WorldModel = InWorldModel;
	}

	void OnDragEnter(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
	{
		TArray<FAssetData> AssetList;
		if ( GetWorldAssetsFromDrag(DragDropEvent, AssetList) )
		{
			TSharedPtr< FAssetDragDropOp > DragDropOp = DragDropEvent.GetOperationAs< FAssetDragDropOp >();
			check(DragDropOp.IsValid());

			DragDropOp->SetToolTip(LOCTEXT("OnDragWorldAssetsOverFolder", "Add Level(s)"), FEditorStyle::GetBrush(TEXT("Graph.ConnectorFeedback.OK")));
		}
	}

	FReply OnDragOver(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
	{
		TArray<FAssetData> AssetList;
		if (GetWorldAssetsFromDrag(DragDropEvent, AssetList))
		{
			return FReply::Handled();
		}

		return FReply::Unhandled();
	}

	void OnDragLeave(const FDragDropEvent& DragDropEvent)
	{
		TSharedPtr< FAssetDragDropOp > DragDropOp = DragDropEvent.GetOperationAs< FAssetDragDropOp >();
		if (DragDropOp.IsValid())
		{
			DragDropOp->ResetToDefaultToolTip();
		}
	}

	FReply OnDrop(const FGeometry& MyGeometry, const FDragDropEvent& DragDropEvent)
	{
		if ( WorldModel.IsValid() )
		{
			// Handle adding dropped levels to world
			TArray<FAssetData> AssetList;
			if ( GetWorldAssetsFromDrag(DragDropEvent, AssetList) )
			{
				WorldModel->AddExistingLevelsFromAssetData(AssetList);
				return FReply::Handled();
			}
		}

		return FReply::Unhandled();
	}

private:
	bool GetWorldAssetsFromDrag(const FDragDropEvent& DragDropEvent, TArray<FAssetData>& OutWorldAssetList)
	{
		TArray<FAssetData> AssetList = AssetUtil::ExtractAssetDataFromDrag(DragDropEvent);
		for (const auto& AssetData : AssetList)
		{
			if (AssetData.AssetClass == UWorld::StaticClass()->GetFName())
			{
				OutWorldAssetList.Add(AssetData);
			}
		}

		return OutWorldAssetList.Num() > 0;
	}

	TSharedPtr<FLevelCollectionModel> WorldModel;
};

//----------------------------------------------------------------
//
//
//----------------------------------------------------------------
class SWorldHierarchyImpl 
	: public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SWorldHierarchyImpl) {}
		SLATE_ARGUMENT(TSharedPtr<FLevelCollectionModel>, InWorldModel)
	SLATE_END_ARGS()

	SWorldHierarchyImpl()
		: bUpdatingSelection(false)
	{
	}

	~SWorldHierarchyImpl()
	{
		WorldModel->SelectionChanged.RemoveAll(this);
		WorldModel->HierarchyChanged.RemoveAll(this);
		WorldModel->CollectionChanged.RemoveAll(this);
	}
	
	void Construct(const FArguments& InArgs)
	{
		WorldModel = InArgs._InWorldModel;
		check(WorldModel.IsValid());
	
		WorldModel->SelectionChanged.AddSP(this, &SWorldHierarchyImpl::OnUpdateSelection);
		WorldModel->HierarchyChanged.AddSP(this, &SWorldHierarchyImpl::RefreshView);
		WorldModel->CollectionChanged.AddSP(this, &SWorldHierarchyImpl::RefreshView);
		
		SearchBoxLevelFilter = MakeShareable(new LevelTextFilter(
				LevelTextFilter::FItemToStringArray::CreateSP(this, &SWorldHierarchyImpl::TransformLevelToString)
		));

		HeaderRowWidget =
			SNew( SHeaderRow )
			.Visibility(EVisibility::Collapsed)

			/** Level visibility column */
			+ SHeaderRow::Column(HierarchyColumns::ColumnID_Visibility)
			.FixedWidth(24.0f)
			.HeaderContent()
			[
				SNew(STextBlock)
				.ToolTipText(NSLOCTEXT("WorldBrowser", "Visibility", "Visibility"))
			]

			/** LevelName label column */
			+ SHeaderRow::Column( HierarchyColumns::ColumnID_LevelLabel )
				.FillWidth( 0.45f )
				.HeaderContent()
				[
					SNew(STextBlock)
						.ToolTipText(LOCTEXT("Column_LevelNameLabel", "Level"))
				]

			/** Level lock column */
			+ SHeaderRow::Column( HierarchyColumns::ColumnID_Lock )
				.FixedWidth( 24.0f )
				.HeaderContent()
				[
					SNew(STextBlock)
						.ToolTipText(NSLOCTEXT("WorldBrowser", "Lock", "Lock"))
				]
	
			/** Level kismet column */
			+ SHeaderRow::Column( HierarchyColumns::ColumnID_Kismet )
				.FixedWidth( 24.0f )
				.HeaderContent()
				[
					SNew(STextBlock)
						.ToolTipText(NSLOCTEXT("WorldBrowser", "Blueprint", "Open the level blueprint for this Level"))
				]

			/** Level SCC status column */
			+ SHeaderRow::Column( HierarchyColumns::ColumnID_SCCStatus )
				.FixedWidth( 24.0f )
				.HeaderContent()
				[
					SNew(STextBlock)
						.ToolTipText(NSLOCTEXT("WorldBrowser", "SCCStatus", "Status in Source Control"))
				]

			/** Level save column */
			+ SHeaderRow::Column( HierarchyColumns::ColumnID_Save )
				.FixedWidth( 24.0f )
				.HeaderContent()
				[
					SNew(STextBlock)
						.ToolTipText(NSLOCTEXT("WorldBrowser", "Save", "Save this Level"))
				]

			/** Level color column */
			+ SHeaderRow::Column(HierarchyColumns::ColumnID_Color)
				.FixedWidth(24.0f)
				.HeaderContent()
				[
					SNew(STextBlock)
					.ToolTipText(NSLOCTEXT("WorldBrowser", "Color", "Color used for visualization of Level"))
				];


		ChildSlot
		[
			SNew(SVerticalBox)
			// Filter box
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew( SSearchBox )
				.ToolTipText(LOCTEXT("FilterSearchToolTip", "Type here to search Levels"))
				.HintText(LOCTEXT("FilterSearchHint", "Search Levels"))
				.OnTextChanged(SearchBoxLevelFilter.Get(), &LevelTextFilter::SetRawFilterText)
			]
			// Hierarchy
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			[
				SAssignNew(TreeWidget, SLevelsTreeWidget, WorldModel)
				.TreeItemsSource(&WorldModel->GetRootLevelList())
				.SelectionMode(ESelectionMode::Multi)
				.OnGenerateRow(this, &SWorldHierarchyImpl::GenerateTreeRow)
				.OnGetChildren( this, &SWorldHierarchyImpl::GetChildrenForTree)
				.OnSelectionChanged(this, &SWorldHierarchyImpl::OnSelectionChanged)
				.OnExpansionChanged(this, &SWorldHierarchyImpl::OnExpansionChanged)
				.OnMouseButtonDoubleClick(this, &SWorldHierarchyImpl::OnTreeViewMouseButtonDoubleClick)
				.OnContextMenuOpening(this, &SWorldHierarchyImpl::ConstructLevelContextMenu)
				.HeaderRow(HeaderRowWidget.ToSharedRef())
			]

			// Separator
			+SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 1)
			[
				SNew(SSeparator)
			]
		
			// View options
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				// Asset count
				+SHorizontalBox::Slot()
				.FillWidth(1.f)
				.VAlign(VAlign_Center)
				.Padding(8, 0)
				[
					SNew( STextBlock )
					.Text( this, &SWorldHierarchyImpl::GetFilterStatusText )
					.ColorAndOpacity( this, &SWorldHierarchyImpl::GetFilterStatusTextColor )
				]

				// View mode combo button
				+SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew( ViewOptionsComboButton, SComboButton )
					.ContentPadding(0)
					.ForegroundColor( this, &SWorldHierarchyImpl::GetViewButtonForegroundColor )
					.ButtonStyle( FEditorStyle::Get(), "ToggleButton" ) // Use the tool bar item style for this button
					.OnGetMenuContent( this, &SWorldHierarchyImpl::GetViewButtonContent )
					.ButtonContent()
					[
						SNew(SHorizontalBox)

						+SHorizontalBox::Slot()
						.AutoWidth()
						.VAlign(VAlign_Center)
						[
							SNew(SImage).Image( FEditorStyle::GetBrush("GenericViewButton") )
						]

						+SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(2, 0, 0, 0)
						.VAlign(VAlign_Center)
						[
							SNew(STextBlock).Text( LOCTEXT("ViewButton", "View Options") )
						]
					]
				]
			]
		];
	
		WorldModel->AddFilter(SearchBoxLevelFilter.ToSharedRef());
		OnUpdateSelection();
	}

	/** Regenerates current items */
	void RefreshView()
	{
		TreeWidget->RequestTreeRefresh();
	
		// Sync items expansion state
		struct FExpander : public FLevelModelVisitor 
		{
			TSharedPtr<SLevelsTreeWidget> TreeWidget;
			virtual void Visit(FLevelModel& Item) override
			{
				TreeWidget->SetItemExpansion(Item.AsShared(), Item.GetLevelExpansionFlag());
			};
		} Expander;
		Expander.TreeWidget = TreeWidget;
		
		//Apply expansion
		WorldModel->IterateHierarchy(Expander);
	}

private:
	/** Creates an item for the tree view */
	TSharedRef<ITableRow> GenerateTreeRow(TSharedPtr<FLevelModel> Item, const TSharedRef<STableViewBase>& OwnerTable)
	{
		check(Item.IsValid());

		return SNew(SWorldHierarchyItem, OwnerTable)
				.InWorldModel(WorldModel)
				.InItemModel(Item)
				.IsItemExpanded(this, &SWorldHierarchyImpl::IsTreeItemExpanded, Item)
				.HighlightText(this, &SWorldHierarchyImpl::GetSearchBoxText);

	}

	/** Handler for returning a list of children associated with a particular tree node */
	void GetChildrenForTree(TSharedPtr<FLevelModel> Item, FLevelModelList& OutChildren)
	{
		OutChildren = Item->GetChildren();
	}

	/** @return the SWidget containing the context menu */
	TSharedPtr<SWidget> ConstructLevelContextMenu() const
	{
		if (!WorldModel->IsReadOnly())
		{
			FMenuBuilder MenuBuilder(true, WorldModel->GetCommandList());
			WorldModel->BuildHierarchyMenu(MenuBuilder);
			return MenuBuilder.MakeWidget();
		}

		return SNullWidget::NullWidget;	
	}

	/** Called by TreeView widget whenever tree item expanded or collapsed */
	void OnExpansionChanged(TSharedPtr<FLevelModel> Item, bool bIsItemExpanded)
	{
		Item->SetLevelExpansionFlag(bIsItemExpanded);
	}

	/** Called by TreeView widget whenever selection is changed */
	void OnSelectionChanged(const TSharedPtr<FLevelModel> Item, ESelectInfo::Type SelectInfo)
	{
		if (bUpdatingSelection)
		{
			return;
		}

		bUpdatingSelection = true;
		WorldModel->SetSelectedLevels(TreeWidget->GetSelectedItems());
		bUpdatingSelection = false;
	}

	/** Handles selection changes in data source */
	void OnUpdateSelection()
	{
		if (bUpdatingSelection)
		{
			return;
		}

		bUpdatingSelection = true;
		const auto& SelectedItems = WorldModel->GetSelectedLevels();
		TreeWidget->ClearSelection();
		for (auto It = SelectedItems.CreateConstIterator(); It; ++It)
		{
			TreeWidget->SetItemSelection(*It, true);
		}

		if (SelectedItems.Num() == 1)
		{
			TreeWidget->RequestScrollIntoView(SelectedItems[0]);
		}
	
		RefreshView();

		bUpdatingSelection = false;
	}

	/** 
	 *	Called by STreeView when the user double-clicks on an item 
	 *
	 *	@param	Item	The item that was double clicked
	 */
	void OnTreeViewMouseButtonDoubleClick(TSharedPtr<FLevelModel> Item)
	{
		Item->MakeLevelCurrent();
	}

	/**
	 * Checks to see if this widget supports keyboard focus.  Override this in derived classes.
	 *
	 * @return  True if this widget can take keyboard focus
	 */
	virtual bool SupportsKeyboardFocus() const override
	{
		return true;
	}

	/**
	 * Called after a key is pressed when this widget has focus
	 *
	 * @param MyGeometry The Geometry of the widget receiving the event
	 * @param  InKeyEvent  Key event
	 *
	 * @return  Returns whether the event was handled, along with other possible actions
	 */
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		if (WorldModel->GetCommandList()->ProcessCommandBindings(InKeyEvent))
		{
			return FReply::Handled();
		}
	
		return SCompoundWidget::OnKeyDown(MyGeometry, InKeyEvent);
	}

private:
	/** @returns Whether specified item should be expanded */
	bool IsTreeItemExpanded(TSharedPtr<FLevelModel> Item) const
	{
		return Item->GetLevelExpansionFlag();
	}

	/** Appends the Level's name to the OutSearchStrings array if the Level is valid */
	void TransformLevelToString(const FLevelModel* Level, TArray<FString>& OutSearchStrings) const
	{
		if (Level != nullptr && Level->HasValidPackage())
		{
			OutSearchStrings.Add(FPackageName::GetShortName(Level->GetLongPackageName()));
		}
	}
	
	/** @return Text entered in search box */
	FText GetSearchBoxText() const
	{
		return SearchBoxLevelFilter->GetRawFilterText();
	}

	/** @return	Returns the filter status text */
	FText GetFilterStatusText() const
	{
		const int32 SelectedLevelsCount = WorldModel->GetSelectedLevels().Num();
		const int32 TotalLevelsCount = WorldModel->GetAllLevels().Num();
		const int32 FilteredLevelsCount = WorldModel->GetFilteredLevels().Num();
		
		if (!WorldModel->IsFilterActive())
		{
			if (SelectedLevelsCount == 0)
			{
				return FText::Format( LOCTEXT("ShowingAllLevelsFmt", "{0} levels"), FText::AsNumber(TotalLevelsCount) );
			}
			else
			{
				return FText::Format( LOCTEXT("ShowingAllLevelsSelectedFmt", "{0} levels ({1} selected)"), FText::AsNumber(TotalLevelsCount), FText::AsNumber(SelectedLevelsCount) );
			}
		}
		else if(WorldModel->IsFilterActive() && FilteredLevelsCount == 0)
		{
			return FText::Format( LOCTEXT("ShowingNoLevelsFmt", "No matching levels ({0} total)"), FText::AsNumber(TotalLevelsCount) );
		}
		else if (SelectedLevelsCount != 0)
		{
			return FText::Format( LOCTEXT("ShowingOnlySomeLevelsSelectedFmt", "Showing {0} of {1} levels ({2} selected)"), FText::AsNumber(FilteredLevelsCount), FText::AsNumber(TotalLevelsCount), FText::AsNumber(SelectedLevelsCount) );
		}
		else
		{
			return FText::Format( LOCTEXT("ShowingOnlySomeLevelsFmt", "Showing {0} of {1} levels"), FText::AsNumber(FilteredLevelsCount), FText::AsNumber(TotalLevelsCount) );
		}
	}


	/** @return Returns color for the filter status text message, based on success of search filter */
	FSlateColor GetFilterStatusTextColor() const
	{
		if (!WorldModel->IsFilterActive() )
		{
			// White = no text filter
			return FLinearColor( 1.0f, 1.0f, 1.0f );
		}
		else if(WorldModel->GetFilteredLevels().Num() == 0)
		{
			// Red = no matching actors
			return FLinearColor( 1.0f, 0.4f, 0.4f );
		}
		else
		{
			// Green = found at least one match!
			return FLinearColor( 0.4f, 1.0f, 0.4f );
		}
	}

	/** @return the content for the view button */
	TSharedRef<SWidget> GetViewButtonContent()
	{
		FMenuBuilder MenuBuilder(true, NULL);

		MenuBuilder.BeginSection("SubLevelsViewMenu", LOCTEXT("ShowHeading", "Show"));
		{
			MenuBuilder.AddMenuEntry(LOCTEXT("ToggleDisplayPaths", "Display Paths"), 
									LOCTEXT("ToggleDisplayPaths_Tooltip", "If enabled, displays the path for each level"), 
									FSlateIcon(), 
									FUIAction(
										FExecuteAction::CreateSP(this, &SWorldHierarchyImpl::ToggleDisplayPaths_Executed), 
										FCanExecuteAction(),
										FIsActionChecked::CreateSP(this, &SWorldHierarchyImpl::GetDisplayPathsState)),
									NAME_None, 
									EUserInterfaceActionType::ToggleButton
			);

			MenuBuilder.AddMenuEntry(LOCTEXT("ToggleDisplayActorsCount", "Display Actors Count"), 
									LOCTEXT("ToggleDisplayActorsCount_Tooltip", "If enabled, displays actors count for each level"), 
									FSlateIcon(), 
									FUIAction(
										FExecuteAction::CreateSP(this, &SWorldHierarchyImpl::ToggleDisplayActorsCount_Executed), 
										FCanExecuteAction(),
										FIsActionChecked::CreateSP(this, &SWorldHierarchyImpl::GetDisplayActorsCountState)),
									NAME_None, 
									EUserInterfaceActionType::ToggleButton
			);

		}
		MenuBuilder.EndSection();

		return MenuBuilder.MakeWidget();
	}

	/** @return the foreground color for the view button */
	FSlateColor GetViewButtonForegroundColor() const
	{
		static const FName InvertedForegroundName("InvertedForeground");
		static const FName DefaultForegroundName("DefaultForeground");

		return ViewOptionsComboButton->IsHovered() ? FEditorStyle::GetSlateColor(InvertedForegroundName) : FEditorStyle::GetSlateColor(DefaultForegroundName);
	}

	/** Toggles state of 'display path' */
	void ToggleDisplayPaths_Executed()
	{
		WorldModel->SetDisplayPathsState(!WorldModel->GetDisplayPathsState());
	}

	/** Gets the state of the 'display paths' enabled/disabled */
	bool GetDisplayPathsState() const
	{
		return WorldModel->GetDisplayPathsState();
	}

	/** Toggles state of 'display actors count' */
	void ToggleDisplayActorsCount_Executed()
	{
		WorldModel->SetDisplayActorsCountState(!WorldModel->GetDisplayActorsCountState());
	}

	/** Gets the state of the 'display actors count' enabled/disabled */
	bool GetDisplayActorsCountState() const
	{
		return WorldModel->GetDisplayActorsCountState();
	}

private:
	/**	Whether the view is currently updating the viewmodel selection */
	bool								bUpdatingSelection;
	/** Our list view widget */
	TSharedPtr<SLevelsTreeWidget>		TreeWidget;
	
	/** Items collection to display */
	TSharedPtr<FLevelCollectionModel>	WorldModel;

	/** The Header Row for the hierarchy */
	TSharedPtr<SHeaderRow>				HeaderRowWidget;

	/**	The LevelTextFilter that constrains which Levels appear in the hierarchy */
	TSharedPtr<LevelTextFilter>			SearchBoxLevelFilter;

	/**	Button representing view options on bottom */
	TSharedPtr<SComboButton>			ViewOptionsComboButton;
};

//----------------------------------------------------------------
//
//
//----------------------------------------------------------------
SWorldHierarchy::SWorldHierarchy()
{
}

SWorldHierarchy::~SWorldHierarchy()
{
	FWorldBrowserModule& WorldBrowserModule = FModuleManager::GetModuleChecked<FWorldBrowserModule>("WorldBrowser");
	WorldBrowserModule.OnBrowseWorld.RemoveAll(this);
}

void SWorldHierarchy::Construct(const FArguments& InArgs)
{
	FWorldBrowserModule& WorldBrowserModule = FModuleManager::GetModuleChecked<FWorldBrowserModule>("WorldBrowser");
	WorldBrowserModule.OnBrowseWorld.AddSP(this, &SWorldHierarchy::OnBrowseWorld);

	OnBrowseWorld(InArgs._InWorld);
}

void SWorldHierarchy::OnBrowseWorld(UWorld* InWorld)
{
	// Remove all binding to an old world
	ChildSlot
	[
		SNullWidget::NullWidget
	];

	WorldModel = nullptr;
		
	// Bind to a new world
	if (InWorld)
	{
		FWorldBrowserModule& WorldBrowserModule = FModuleManager::GetModuleChecked<FWorldBrowserModule>("WorldBrowser");
		WorldModel = WorldBrowserModule.SharedWorldModel(InWorld);

		ChildSlot
		[
			SNew(SVerticalBox)

			// Toolbar
			+SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
				[
					SNew(SHorizontalBox)
					
					// Toolbar
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						// Levels menu
						SNew( SComboButton )
						.ComboButtonStyle(FEditorStyle::Get(), "ToolbarComboButton")
						.ForegroundColor(FLinearColor::White)
						.ContentPadding(0)
						.OnGetMenuContent(this, &SWorldHierarchy::GetFileButtonContent)
						//.ToolTipText(this, &SWorldHierarchy::GetNewAssetToolTipText)
						//.IsEnabled(this, &SWorldHierarchy::IsAssetPathSelected )
						.ButtonContent()
						[
							SNew(SHorizontalBox)

							// Icon
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SImage)
								.Image(this, &SWorldHierarchy::GetLevelsMenuBrush)
							]

							// Text
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							.Padding(0,0,2,0)
							[
								SNew(STextBlock)
								.TextStyle(FEditorStyle::Get(), "ContentBrowser.TopBar.Font")
								.Text(LOCTEXT("LevelsButton", "Levels"))
							]
						]
					]

					// Button to summon level details tab
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(SButton)
						.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
						.OnClicked(this, &SWorldHierarchy::OnSummonDetails)
						.ToolTipText(LOCTEXT("SummonDetailsToolTipText", "Summons level details"))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Content()
						[
							SNew(SImage)
							.Image(this, &SWorldHierarchy::GetSummonDetailsBrush)
						]
					]

					// Button to summon world composition tab
					+SHorizontalBox::Slot()
					.AutoWidth()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Left)
					[
						SNew(SButton)
						.Visibility(this, &SWorldHierarchy::GetCompositionButtonVisibility)
						.ButtonStyle(FEditorStyle::Get(), "ToggleButton")
						.OnClicked(this, &SWorldHierarchy::OnSummonComposition)
						.ToolTipText(LOCTEXT("SummonHierarchyToolTipText", "Summons world composition"))
						.HAlign(HAlign_Center)
						.VAlign(VAlign_Center)
						.Content()
						[
							SNew(SImage)
							.Image(this, &SWorldHierarchy::GetSummonCompositionBrush)
						]
					]
				]
			]
			
			// Hierarchy
			+SVerticalBox::Slot()
			.FillHeight(1.f)
			.Padding(0,4,0,0)
			[
				SNew(SBorder)
				.BorderImage(FEditorStyle::GetBrush(TEXT("ToolPanel.GroupBorder")))
				[
					SNew(SWorldHierarchyImpl).InWorldModel(WorldModel)
				]
			]
		];
	}
}

FReply SWorldHierarchy::OnSummonDetails()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>( "LevelEditor" );
	LevelEditorModule.SummonWorldBrowserDetails();
	return FReply::Handled();
}

const FSlateBrush* SWorldHierarchy::GetLevelsMenuBrush() const
{
	return FEditorStyle::GetBrush("WorldBrowser.LevelsMenuBrush");
}

const FSlateBrush* SWorldHierarchy::GetSummonDetailsBrush() const
{
	return FEditorStyle::GetBrush("WorldBrowser.DetailsButtonBrush");
}

EVisibility SWorldHierarchy::GetCompositionButtonVisibility() const
{
	return WorldModel->IsTileWorld() ? EVisibility::Visible : EVisibility::Collapsed;
}

FReply SWorldHierarchy::OnSummonComposition()
{
	FLevelEditorModule& LevelEditorModule = FModuleManager::LoadModuleChecked<FLevelEditorModule>( "LevelEditor" );
	LevelEditorModule.SummonWorldBrowserComposition();
	return FReply::Handled();
}

const FSlateBrush* SWorldHierarchy::GetSummonCompositionBrush() const
{
	return FEditorStyle::GetBrush("WorldBrowser.CompositionButtonBrush");
}

TSharedRef<SWidget> SWorldHierarchy::GetFileButtonContent()
{
	FMenuBuilder MenuBuilder(true, WorldModel->GetCommandList());

	// let current level collection model fill additional 'File' commands
	WorldModel->CustomizeFileMainMenu(MenuBuilder);

	return MenuBuilder.MakeWidget();
}

#undef LOCTEXT_NAMESPACE
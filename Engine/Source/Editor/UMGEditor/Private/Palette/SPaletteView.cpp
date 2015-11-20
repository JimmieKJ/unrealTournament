// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGEditorPrivatePCH.h"

#include "SPaletteView.h"
#include "UMGEditorActions.h"

#include "PreviewScene.h"
#include "SceneViewport.h"

#include "BlueprintEditor.h"

#include "DecoratedDragDropOp.h"
#include "WidgetTemplateDragDropOp.h"

#include "WidgetTemplate.h"
#include "WidgetTemplateClass.h"
#include "WidgetTemplateBlueprintClass.h"

#include "Developer/HotReload/Public/IHotReload.h"

#include "AssetRegistryModule.h"
#include "SSearchBox.h"

#include "WidgetBlueprintCompiler.h"
#include "WidgetBlueprintEditorUtils.h"

#include "Blueprint/UserWidget.h"
#include "WidgetBlueprint.h"
#include "ObjectEditorUtils.h"

#define LOCTEXT_NAMESPACE "UMG"

class SPaletteViewItem : public SCompoundWidget
{
public:

	SLATE_BEGIN_ARGS(SPaletteViewItem) {}

		/** The current text to highlight */
		SLATE_ATTRIBUTE(FText, HighlightText)

	SLATE_END_ARGS()

	/**
	* Constructs this widget
	*
	* @param InArgs    Declaration from which to construct the widget
	*/
	void Construct(const FArguments& InArgs, TSharedPtr<FWidgetTemplate> InTemplate)
	{
		Template = InTemplate;

		ChildSlot
		[
			SNew(SHorizontalBox)
			.Visibility(EVisibility::Visible)
			.ToolTip(Template->GetToolTip())

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SImage)
				.ColorAndOpacity(FLinearColor(1, 1, 1, 0.5))
				.Image(Template->GetIcon())
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2, 0, 0, 0)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(Template->Name)
				.HighlightText(InArgs._HighlightText)
			]
		];
	}

	virtual FReply OnMouseButtonDoubleClick( const FGeometry& InMyGeometry, const FPointerEvent& InMouseEvent ) override
	{
		return Template->OnDoubleClicked();
	}

private:
	TSharedPtr<FWidgetTemplate> Template;
};

class FWidgetTemplateViewModel : public FWidgetViewModel
{
public:
	virtual ~FWidgetTemplateViewModel()
	{
	}

	virtual FText GetName() const override
	{
		return Template->Name;
	}

	virtual FString GetFilterString() const override
	{
		return Template->Name.ToString();
	}

	virtual TSharedRef<ITableRow> BuildRow(const TSharedRef<STableViewBase>& OwnerTable) override
	{
		return SNew(STableRow< TSharedPtr<FWidgetViewModel> >, OwnerTable)
			.Padding(2.0f)
			.Style(FEditorStyle::Get(), "UMGEditor.PaletteItem")
			.OnDragDetected(this, &FWidgetTemplateViewModel::OnDraggingWidgetTemplateItem)
			[
				SNew(SPaletteViewItem, Template)
				.HighlightText(OwnerView, &SPaletteView::GetSearchText)
			];
	}

	FReply OnDraggingWidgetTemplateItem(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent)
	{
		return FReply::Handled().BeginDragDrop(FWidgetTemplateDragDropOp::New(Template));
	}

	SPaletteView* OwnerView;
	TSharedPtr<FWidgetTemplate> Template;
};

class FWidgetHeaderViewModel : public FWidgetViewModel
{
public:
	virtual ~FWidgetHeaderViewModel()
	{
	}

	virtual FText GetName() const override
	{
		return GroupName;
	}

	virtual FString GetFilterString() const override
	{
		// Headers should never be included in filtering to avoid showing a header with all of
		// it's widgets filtered out, so return an empty filter string.
		return TEXT("");
	}
	
	virtual TSharedRef<ITableRow> BuildRow(const TSharedRef<STableViewBase>& OwnerTable) override
	{
		return SNew(STableRow< TSharedPtr<FWidgetViewModel> >, OwnerTable)
			.Style( FEditorStyle::Get(), "UMGEditor.PaletteHeader" )
			.Padding(2.0f)
			.ShowSelection(false)
			[
				SNew(STextBlock)
				.Text(GroupName)
				.Font(FEditorStyle::GetFontStyle("DetailsView.CategoryFontStyle"))
				.ShadowOffset(FVector2D(1.0f, 1.0f))
			];
	}

	virtual void GetChildren(TArray< TSharedPtr<FWidgetViewModel> >& OutChildren) override
	{
		for ( TSharedPtr<FWidgetViewModel>& Child : Children )
		{
			OutChildren.Add(Child);
		}
	}

	FText GroupName;
	TArray< TSharedPtr<FWidgetViewModel> > Children;
};

void SPaletteView::Construct(const FArguments& InArgs, TSharedPtr<FBlueprintEditor> InBlueprintEditor)
{
	// Register for events that can trigger a palette rebuild
	GEditor->OnBlueprintReinstanced().AddRaw(this, &SPaletteView::OnBlueprintReinstanced);
	FEditorDelegates::OnAssetsDeleted.AddSP(this, &SPaletteView::HandleOnAssetsDeleted);
	IHotReloadModule::Get().OnHotReload().AddSP(this, &SPaletteView::HandleOnHotReload);
	
	// register for any objects replaced
	GEditor->OnObjectsReplaced().AddRaw(this, &SPaletteView::OnObjectsReplaced);

	BlueprintEditor = InBlueprintEditor;

	UBlueprint* BP = InBlueprintEditor->GetBlueprintObj();

	WidgetFilter = MakeShareable(new WidgetViewModelTextFilter(
		WidgetViewModelTextFilter::FItemToStringArray::CreateSP(this, &SPaletteView::TransformWidgetViewModelToString)));

	FilterHandler = MakeShareable(new PaletteFilterHandler());
	FilterHandler->SetFilter(WidgetFilter.Get());
	FilterHandler->SetRootItems(&WidgetViewModels, &TreeWidgetViewModels);
	FilterHandler->SetGetChildrenDelegate(PaletteFilterHandler::FOnGetChildren::CreateRaw(this, &SPaletteView::OnGetChildren));

	SAssignNew(WidgetTemplatesView, STreeView< TSharedPtr<FWidgetViewModel> >)
		.ItemHeight(1.0f)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SPaletteView::OnGenerateWidgetTemplateItem)
		.OnGetChildren(FilterHandler.ToSharedRef(), &PaletteFilterHandler::OnGetFilteredChildren)
		.TreeItemsSource(&TreeWidgetViewModels);
		

	FilterHandler->SetTreeView(WidgetTemplatesView.Get());

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot()
		.Padding(4)
		.AutoHeight()
		[
			SAssignNew(SearchBoxPtr, SSearchBox)
			.HintText(LOCTEXT("SearchTemplates", "Search Palette"))
			.OnTextChanged(this, &SPaletteView::OnSearchChanged)
		]

		+ SVerticalBox::Slot()
		.FillHeight(1.0f)
		[
			SNew(SScrollBorder, WidgetTemplatesView.ToSharedRef())
			[
				WidgetTemplatesView.ToSharedRef()
			]
		]
	];

	bRefreshRequested = true;

	BuildWidgetList();
	LoadItemExpansion();

	bRebuildRequested = false;
}

SPaletteView::~SPaletteView()
{
	// If the filter is enabled, disable it before saving the expanded items since
	// filtering expands all items by default.
	if (FilterHandler->GetIsEnabled())
	{
		FilterHandler->SetIsEnabled(false);
		FilterHandler->RefreshAndFilterTree();
	}

	GEditor->OnBlueprintReinstanced().RemoveAll(this);
	FEditorDelegates::OnAssetsDeleted.RemoveAll(this);
	IHotReloadModule::Get().OnHotReload().RemoveAll(this);
	GEditor->OnObjectsReplaced().RemoveAll( this );
	

	SaveItemExpansion();
}

void SPaletteView::OnSearchChanged(const FText& InFilterText)
{
	bRefreshRequested = true;
	FilterHandler->SetIsEnabled(!InFilterText.IsEmpty());
	WidgetFilter->SetRawFilterText(InFilterText);
	SearchBoxPtr->SetError(WidgetFilter->GetFilterErrorText());
	SearchText = InFilterText;
}

FText SPaletteView::GetSearchText() const
{
	return SearchText;
}

void SPaletteView::LoadItemExpansion()
{
	// Restore the expansion state of the widget groups.
	for ( TSharedPtr<FWidgetViewModel>& ViewModel : WidgetViewModels )
	{
		bool IsExpanded;
		if ( GConfig->GetBool(TEXT("WidgetTemplatesExpanded"), *ViewModel->GetName().ToString(), IsExpanded, GEditorPerProjectIni) && IsExpanded )
		{
			WidgetTemplatesView->SetItemExpansion(ViewModel, true);
		}
	}
}

void SPaletteView::SaveItemExpansion()
{
	// Restore the expansion state of the widget groups.
	for ( TSharedPtr<FWidgetViewModel>& ViewModel : WidgetViewModels )
	{
		const bool IsExpanded = WidgetTemplatesView->IsItemExpanded(ViewModel);
		GConfig->SetBool(TEXT("WidgetTemplatesExpanded"), *ViewModel->GetName().ToString(), IsExpanded, GEditorPerProjectIni);
	}
}

UWidgetBlueprint* SPaletteView::GetBlueprint() const
{
	if ( BlueprintEditor.IsValid() )
	{
		UBlueprint* BP = BlueprintEditor.Pin()->GetBlueprintObj();
		return Cast<UWidgetBlueprint>(BP);
	}

	return NULL;
}

void SPaletteView::BuildWidgetList()
{
	// Clear the current list of view models and categories
	WidgetViewModels.Reset();
	WidgetTemplateCategories.Reset();

	// Generate a list of templates
	BuildClassWidgetList();
	BuildSpecialWidgetList();

	// For each entry in the category create a view model for the widget template
	for ( auto& Entry : WidgetTemplateCategories )
	{
		TSharedPtr<FWidgetHeaderViewModel> Header = MakeShareable(new FWidgetHeaderViewModel());
		Header->GroupName = FText::FromString(Entry.Key);

		for ( auto& Template : Entry.Value )
		{
			TSharedPtr<FWidgetTemplateViewModel> TemplateViewModel = MakeShareable(new FWidgetTemplateViewModel());
			TemplateViewModel->Template = Template;
			TemplateViewModel->OwnerView = this;
			Header->Children.Add(TemplateViewModel);
		}

		Header->Children.Sort([] (TSharedPtr<FWidgetViewModel> L, TSharedPtr<FWidgetViewModel> R) { return R->GetName().CompareTo(L->GetName()) > 0; });

		WidgetViewModels.Add(Header);
	}

	// Sort the view models by name
	WidgetViewModels.Sort([] (TSharedPtr<FWidgetViewModel> L, TSharedPtr<FWidgetViewModel> R) { return R->GetName().CompareTo(L->GetName()) > 0; });
}

void SPaletteView::BuildClassWidgetList()
{
	static const FName DevelopmentStatusKey(TEXT("DevelopmentStatus"));

	TMap<FName, TSubclassOf<UUserWidget>> LoadedWidgetBlueprintClassesByName;

	auto ActiveWidgetBlueprintClass = GetBlueprint()->GeneratedClass;
	FName ActiveWidgetBlueprintClassName = ActiveWidgetBlueprintClass->GetFName();

	// Locate all UWidget classes from code and loaded widget BPs
	for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
	{
		UClass* WidgetClass = *ClassIt;

		if ( FWidgetBlueprintEditorUtils::IsUsableWidgetClass(WidgetClass) )
		{
			const bool bIsSameClass = WidgetClass->GetFName() == ActiveWidgetBlueprintClassName;

			// Check that the asset that generated this class is valid (necessary b/c of a larger issue wherein force delete does not wipe the generated class object)
			if ( bIsSameClass )
			{
				continue;
			}

			if (WidgetClass->IsChildOf(UUserWidget::StaticClass()))
			{
				if ( WidgetClass->ClassGeneratedBy )
				{
					// Track the widget blueprint classes that are already loaded
					LoadedWidgetBlueprintClassesByName.Add(WidgetClass->ClassGeneratedBy->GetFName()) = WidgetClass;
				}
			}
			else
			{
				TSharedPtr<FWidgetTemplateClass> Template = MakeShareable(new FWidgetTemplateClass(WidgetClass));

				AddWidgetTemplate(Template);
			}

			//TODO UMG does not prevent deep nested circular references
		}
	}

	// Locate all widget BP assets (include unloaded)
	const FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
	TArray<FAssetData> AllWidgetBPsAssetData;
	AssetRegistryModule.Get().GetAssetsByClass(UWidgetBlueprint::StaticClass()->GetFName(), AllWidgetBPsAssetData, true);

	FName ActiveWidgetBlueprintName = ActiveWidgetBlueprintClass->ClassGeneratedBy->GetFName();
	for (auto& WidgetBPAssetData : AllWidgetBPsAssetData)
	{
		if (WidgetBPAssetData.AssetName == ActiveWidgetBlueprintName)
		{
			continue;
		}

		// If the blueprint generated class was found earlier, pass it to the template
		TSubclassOf<UUserWidget> WidgetBPClass = nullptr;
		auto LoadedWidgetBPClass = LoadedWidgetBlueprintClassesByName.Find(WidgetBPAssetData.AssetName);
		if (LoadedWidgetBPClass)
		{
			WidgetBPClass = *LoadedWidgetBPClass;
		}

		auto Template = MakeShareable(new FWidgetTemplateBlueprintClass(WidgetBPAssetData, WidgetBPClass));

		AddWidgetTemplate(Template);
	}
}

void SPaletteView::BuildSpecialWidgetList()
{
	//AddWidgetTemplate(MakeShareable(new FWidgetTemplateButton()));
	//AddWidgetTemplate(MakeShareable(new FWidgetTemplateCheckBox()));

	//TODO UMG Make this pluggable.
}

void SPaletteView::AddWidgetTemplate(TSharedPtr<FWidgetTemplate> Template)
{
	FString Category = Template->GetCategory().ToString();
	WidgetTemplateArray& Group = WidgetTemplateCategories.FindOrAdd(Category);
	Group.Add(Template);
}

void SPaletteView::OnGetChildren(TSharedPtr<FWidgetViewModel> Item, TArray< TSharedPtr<FWidgetViewModel> >& Children)
{
	return Item->GetChildren(Children);
}

TSharedRef<ITableRow> SPaletteView::OnGenerateWidgetTemplateItem(TSharedPtr<FWidgetViewModel> Item, const TSharedRef<STableViewBase>& OwnerTable)
{
	return Item->BuildRow(OwnerTable);
}

void SPaletteView::Tick( const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime )
{
	if ( bRebuildRequested )
	{
		bRebuildRequested = false;

		// Save the old expanded items temporarily
		TSet<TSharedPtr<FWidgetViewModel>> ExpandedItems;
		WidgetTemplatesView->GetExpandedItems(ExpandedItems);

		BuildWidgetList();

		// Restore the expansion state
		for ( TSharedPtr<FWidgetViewModel>& ExpandedItem : ExpandedItems )
		{
			for ( TSharedPtr<FWidgetViewModel>& ViewModel : WidgetViewModels )
			{
				if ( ViewModel->GetName().EqualTo(ExpandedItem->GetName()) )
				{
					WidgetTemplatesView->SetItemExpansion(ViewModel, true);
				}
			}
		}
	}

	if (bRefreshRequested)
	{
		bRefreshRequested = false;
		FilterHandler->RefreshAndFilterTree();
	}
}

void SPaletteView::TransformWidgetViewModelToString(TSharedPtr<FWidgetViewModel> WidgetViewModel, OUT TArray< FString >& Array)
{
	Array.Add(WidgetViewModel->GetFilterString());
}

void SPaletteView::OnObjectsReplaced(const TMap<UObject*, UObject*>& ReplacementMap)
{
	//bRefreshRequested = true;
	//bRebuildRequested = true;
}

void SPaletteView::OnBlueprintReinstanced()
{
	bRebuildRequested = true;
	bRefreshRequested = true;
}

void SPaletteView::HandleOnHotReload(bool bWasTriggeredAutomatically)
{
	bRebuildRequested = true;
	bRefreshRequested = true;
}

void SPaletteView::HandleOnAssetsDeleted(const TArray<UClass*>& DeletedAssetClasses)
{
	for (auto DeletedAssetClass : DeletedAssetClasses)
	{
		if (DeletedAssetClass->IsChildOf(UWidgetBlueprint::StaticClass()))
		{
			bRebuildRequested = true;
			bRefreshRequested = true;
		}
	}
	
}

#undef LOCTEXT_NAMESPACE

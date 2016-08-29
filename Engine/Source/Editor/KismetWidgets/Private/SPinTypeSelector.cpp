// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#include "KismetWidgetsPrivatePCH.h"
#include "UnrealEd.h"
#include "ClassIconFinder.h"
#include "SPinTypeSelector.h"
#include "IDocumentation.h"
#include "Editor/UnrealEd/Public/SListViewSelectorDropdownMenu.h"
#include "SSearchBox.h"
#include "SSubMenuHandler.h"
#include "BlueprintEditorUtils.h"

#define LOCTEXT_NAMESPACE "PinTypeSelector"

static const FString BigTooltipDocLink = TEXT("Shared/Editor/Blueprint/VariableTypes");

/** Manages items in the Object Reference Type list, the sub-menu of the PinTypeSelector */
struct FObjectReferenceType
{
	/** Item that is being referenced */
	FPinTypeTreeItem PinTypeItem;

	/** Widget to display for this item */
	TSharedPtr< SWidget > WidgetToDisplay;

	/** Category that should be used when this item is selected */
	FString PinCategory;

	FObjectReferenceType(FPinTypeTreeItem InPinTypeItem, TSharedRef< SWidget > InWidget, FString InPinCategory)
		: PinTypeItem(InPinTypeItem)
		, WidgetToDisplay(InWidget)
		, PinCategory(InPinCategory)
	{}
};

class SPinTypeRow : public SComboRow<FPinTypeTreeItem>
{
public:
	SLATE_BEGIN_ARGS( SPinTypeRow )
	{}
		SLATE_DEFAULT_SLOT( FArguments, Content )
		SLATE_EVENT( FOnGetContent, OnGetMenuContent )
	SLATE_END_ARGS()
public:
	void Construct(const FArguments& InArgs, const TSharedRef<STableViewBase>& InOwnerTable, TWeakPtr<SMenuOwner> InMenuOwner)
	{
		SComboRow<FPinTypeTreeItem>::Construct( SComboRow<FPinTypeTreeItem>::FArguments()
			.ToolTip(InArgs._ToolTip)
			[
				SAssignNew(SubMenuHandler, SSubMenuHandler, InMenuOwner)
				.OnGetMenuContent(InArgs._OnGetMenuContent)
				.MenuContent(nullptr)
				[
					InArgs._Content.Widget
				]
			],
			InOwnerTable);
	}

	// SWidget interface
	virtual bool IsHovered() const override
	{
		return SComboRow<FPinTypeTreeItem>::IsHovered() || SubMenuHandler.Pin()->ShouldSubMenuAppearHovered();
	}
	// End of SWidget interface

	/** Returns TRUE if there is a Sub-Menu available to open */
	bool HasSubMenu() const
	{
		return SubMenuHandler.Pin()->HasSubMenu();
	}

	/** Returns TRUE if there is a Sub-Menu open */
	bool IsSubMenuOpen() const
	{
		return SubMenuHandler.Pin()->IsSubMenuOpen();
	}

	/** Forces the sub-menu open, clobbering any other open ones in the process */
	void RequestSubMenuToggle(bool bInImmediate = false)
	{
		SubMenuHandler.Pin()->RequestSubMenuToggle(true, true, bInImmediate);
	}

private:
	/** The Sub-MenuHandler which is managing the sub-menu content so that mousing over other rows will not close the sub-menus immediately */
	TWeakPtr<SSubMenuHandler> SubMenuHandler;
};

void SPinTypeSelector::Construct(const FArguments& InArgs, FGetPinTypeTree GetPinTypeTreeFunc)
{
	SearchText = FText::GetEmpty();

	OnTypeChanged = InArgs._OnPinTypeChanged;
	OnTypePreChanged = InArgs._OnPinTypePreChanged;

	check(GetPinTypeTreeFunc.IsBound());
	GetPinTypeTree = GetPinTypeTreeFunc;

	Schema = (UEdGraphSchema_K2*)(InArgs._Schema);
	TypeTreeFilter = InArgs._TypeTreeFilter;
	TreeViewWidth = InArgs._TreeViewWidth;
	TreeViewHeight = InArgs._TreeViewHeight;

	TargetPinType = InArgs._TargetPinType;
	bIsCompactSelector = InArgs._bCompactSelector;

	bIsRightMousePressed = false;

	// Depending on if this is a compact selector or not, we generate a different compound widget
	TSharedPtr<SWidget> Widget;

	if (InArgs._bCompactSelector)
	{
		// Only have a combo button with an icon
		Widget = SAssignNew( TypeComboButton, SComboButton )
			.OnGetMenuContent(this, &SPinTypeSelector::GetMenuContent)
			.ContentPadding(0)
			.ToolTipText(this, &SPinTypeSelector::GetToolTipForComboBoxType)
			.HasDownArrow(!InArgs._bCompactSelector)
			.ButtonStyle(FEditorStyle::Get(),  "BlueprintEditor.CompactPinTypeSelector")
			.ButtonContent()
			[
				SNew(SImage)
				.Image( this, &SPinTypeSelector::GetTypeIconImage )
				.ColorAndOpacity( this, &SPinTypeSelector::GetTypeIconColor )
			];
	}
	else
	{
		// Traditional Pin Type Selector with a combo button, the icon, the current type name, and a toggle button for being an array
		Widget = SNew(SHorizontalBox)
		+SHorizontalBox::Slot()
		[
			SAssignNew( TypeComboButton, SComboButton )
			.OnGetMenuContent(this, &SPinTypeSelector::GetMenuContent)
			.ContentPadding(0)
			.ToolTipText(this, &SPinTypeSelector::GetToolTipForComboBoxType)
			.ButtonContent()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SImage)
					.Image( this, &SPinTypeSelector::GetTypeIconImage )
					.ColorAndOpacity( this, &SPinTypeSelector::GetTypeIconColor )
				]
				+SHorizontalBox::Slot()
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(STextBlock)
					.Text( this, &SPinTypeSelector::GetTypeDescription )
					.Font(InArgs._Font)
				]
			]
		]

		+SHorizontalBox::Slot()
		.AutoWidth()
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew( SCheckBox )
			.ToolTip(IDocumentation::Get()->CreateToolTip( TAttribute<FText>(this, &SPinTypeSelector::GetToolTipForArrayWidget), NULL, *BigTooltipDocLink, TEXT("Array")))
			.Padding( 1 )
			.OnCheckStateChanged( this, &SPinTypeSelector::OnArrayCheckStateChanged )
			.IsChecked( this, &SPinTypeSelector::IsArrayChecked )
			.IsEnabled( TargetPinType.Get().PinCategory != Schema->PC_Exec )
			.Style( FEditorStyle::Get(), "ToggleButtonCheckbox" )
			.Visibility(InArgs._bAllowArrays ? EVisibility::Visible : EVisibility::Collapsed)
			[
				SNew(SImage)
				.Image( FEditorStyle::GetBrush(TEXT("Kismet.VariableList.ArrayTypeIcon")) )
				.ColorAndOpacity( this, &SPinTypeSelector::GetTypeIconColor )
			]
		];
	}
	this->ChildSlot
	[
		Widget.ToSharedRef()
	];
}

//=======================================================================
// Attribute Helpers

FText SPinTypeSelector::GetTypeDescription() const
{
	const FString& PinSubCategory = TargetPinType.Get().PinSubCategory;
	const UObject* PinSubCategoryObject = TargetPinType.Get().PinSubCategoryObject.Get();
	if (PinSubCategory != UEdGraphSchema_K2::PSC_Bitmask && PinSubCategoryObject)
	{
		if (auto Field = Cast<const UField>(PinSubCategoryObject))
		{
			return Field->GetDisplayNameText();
		}
		return FText::FromString(PinSubCategoryObject->GetName());
	}
	else
	{
		return UEdGraphSchema_K2::GetCategoryText(TargetPinType.Get().PinCategory, true);
	}
}

const FSlateBrush* SPinTypeSelector::GetTypeIconImage() const
{
	return FBlueprintEditorUtils::GetIconFromPin( TargetPinType.Get() );
}

FSlateColor SPinTypeSelector::GetTypeIconColor() const
{
	return Schema->GetPinTypeColor(TargetPinType.Get());
}

ECheckBoxState SPinTypeSelector::IsArrayChecked() const
{
	return TargetPinType.Get().bIsArray ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void SPinTypeSelector::OnArrayCheckStateChanged(ECheckBoxState NewState)
{
	FEdGraphPinType NewTargetPinType = TargetPinType.Get();
	NewTargetPinType.bIsArray = (NewState == ECheckBoxState::Checked)? true : false;

	OnTypeChanged.ExecuteIfBound(NewTargetPinType);
}

void SPinTypeSelector::OnArrayStateToggled()
{
	OnArrayCheckStateChanged((IsArrayChecked() == ECheckBoxState::Checked)? ECheckBoxState::Unchecked : ECheckBoxState::Checked);
}

//=======================================================================
// Type TreeView Support
TSharedRef<ITableRow> SPinTypeSelector::GenerateTypeTreeRow(FPinTypeTreeItem InItem, const TSharedRef<STableViewBase>& OwnerTree)
{
	const bool bHasChildren = (InItem->Children.Num() > 0);
	const FText Description = InItem->GetDescription();
	const FEdGraphPinType& PinType = InItem->GetPinType(false);

	// Determine the best icon the to represents this item
	const FSlateBrush* IconBrush = FBlueprintEditorUtils::GetIconFromPin(PinType);

	// Use tooltip if supplied, otherwise just repeat description
	const FText OrgTooltip = InItem->GetToolTip();
	const FText Tooltip = !OrgTooltip.IsEmpty() ? OrgTooltip : Description;

	const FString PinTooltipExcerpt = ((PinType.PinCategory != UEdGraphSchema_K2::PC_Byte || PinType.PinSubCategoryObject == nullptr) ? PinType.PinCategory : TEXT("Enum")); 

	// If there is a sub-menu for this pin type, we need to bind the function to handle the sub-menu
	FOnGetContent OnGetContent;
	if (InItem->GetPossibleObjectReferenceTypes() != static_cast<uint8>(EObjectReferenceType::NotAnObject))
	{
		OnGetContent.BindSP(this, &SPinTypeSelector::GetAllowedObjectTypes, InItem);
	}

	TSharedPtr< SHorizontalBox > HorizontalBox;
	TSharedRef< ITableRow > ReturnWidget = SNew( SPinTypeRow, OwnerTree, MenuContent )
		.ToolTip( IDocumentation::Get()->CreateToolTip( Tooltip, NULL, *BigTooltipDocLink, PinTooltipExcerpt) )
		.OnGetMenuContent(OnGetContent)
		[
			SAssignNew(HorizontalBox, SHorizontalBox)
			+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(1.f)
			[
				SNew(SImage)
					.Image(IconBrush)
					.ColorAndOpacity(Schema->GetPinTypeColor(PinType))
					.Visibility( InItem->bReadOnly ? EVisibility::Collapsed : EVisibility::Visible )
			]
			+SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(1.f)
			[
				SNew(STextBlock)
					.Text(Description)
					.HighlightText(SearchText)
					.Font( bHasChildren ? FEditorStyle::GetFontStyle(TEXT("Kismet.TypePicker.CategoryFont")) : FEditorStyle::GetFontStyle(TEXT("Kismet.TypePicker.NormalFont")) )
			]
		];

	// Add a sub-menu indicator arrow to inform the user that there are sub-items to be displayed
	if (OnGetContent.IsBound())
	{
		HorizontalBox->AddSlot()
			.FillWidth(1.0f)
			.VAlign( VAlign_Center )
			.HAlign( HAlign_Right )
			[
				SNew( SBox )
				.Padding(FMargin(7,0,0,0))
				[
					SNew( SImage )
					.Image( FEditorStyle::Get().GetBrush( "ToolBar.SubMenuIndicator" ) )
				]
			];
	}

	return ReturnWidget;
}

TSharedRef<SWidget> SPinTypeSelector::CreateObjectReferenceWidget(FPinTypeTreeItem InItem, FEdGraphPinType& InPinType, const FSlateBrush* InIconBrush, FText InSimpleTooltip) const
{
	return SNew(SHorizontalBox)
		.ToolTip(IDocumentation::Get()->CreateToolTip(InSimpleTooltip, NULL, *BigTooltipDocLink, InPinType.PinCategory))
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(1.f)
		[
			SNew(SImage)
			.Image(InIconBrush)
			.ColorAndOpacity(Schema->GetPinTypeColor(InPinType))
		]
		+SHorizontalBox::Slot()
		.AutoWidth()
		.Padding(1.f)
		[
			SNew(STextBlock)
			.Text(UEdGraphSchema_K2::GetCategoryText(InPinType.PinCategory))
			.Font(FEditorStyle::GetFontStyle(TEXT("Kismet.TypePicker.NormalFont")) )
		];
}

class SObjectReferenceWidget : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS( SObjectReferenceWidget )
	{}
		SLATE_DEFAULT_SLOT( FArguments, Content )
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TWeakPtr< SMenuOwner > InMenuOwner)
	{
		MenuOwner = InMenuOwner;

		this->ChildSlot
		[
			InArgs._Content.Widget
		];
	}

	// SWidget interface
	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& KeyEvent) override
	{
		if (MenuOwner.IsValid() && (KeyEvent.GetKey() == EKeys::Left || KeyEvent.GetKey() == EKeys::Escape))
		{
			MenuOwner.Pin()->CloseSummonedMenus();
			return FReply::Handled();
		}
		return FReply::Unhandled();
	}
	// End of SWidget interface

private:
	TWeakPtr< SMenuOwner > MenuOwner;
};

TSharedRef<ITableRow> SPinTypeSelector::GenerateObjectReferenceTreeRow(FObjectReferenceListItem InItem, const TSharedRef<STableViewBase>& OwnerTree)
{
	return SNew(SComboRow<FObjectReferenceListItem>, OwnerTree)
		[
			InItem->WidgetToDisplay.ToSharedRef()
		];
}

void SPinTypeSelector::OnObjectReferenceSelectionChanged(FObjectReferenceListItem InItem, ESelectInfo::Type SelectInfo)
{
	if (SelectInfo != ESelectInfo::OnNavigation)
	{
		OnSelectPinType(InItem->PinTypeItem, InItem->PinCategory);
	}
}

TSharedRef< SWidget > SPinTypeSelector::GetAllowedObjectTypes(FPinTypeTreeItem InItem)
{
	AllowedObjectReferenceTypes.Reset();

	// Do not force the pin type here, that causes a load of the Blueprint (if unloaded)
	FEdGraphPinType PinType = InItem->GetPinType(false);
	const FSlateBrush* IconBrush = FBlueprintEditorUtils::GetIconFromPin(PinType);

	FFormatNamedArguments Args;

	if(PinType.PinSubCategory != UEdGraphSchema_K2::PSC_Bitmask && PinType.PinSubCategoryObject.IsValid())
	{
		Args.Add(TEXT("TypeName"), InItem->GetDescription());
	}

	uint8 PossibleObjectReferenceTypes = InItem->GetPossibleObjectReferenceTypes();

	// Per each object reference type, change the category to the type and add a menu entry (this will get the color to be correct)

	if (PossibleObjectReferenceTypes & static_cast<uint8>(EObjectReferenceType::ObjectReference))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Object;
		TSharedRef<SWidget> Widget = CreateObjectReferenceWidget(InItem, PinType, IconBrush, FText::Format(LOCTEXT("ObjectTooltip", "Reference an instanced object of type \'{TypeName}\'"), Args));
		FObjectReferenceListItem ObjectReferenceType = MakeShareable(new FObjectReferenceType(InItem, Widget, PinType.PinCategory));
		AllowedObjectReferenceTypes.Add(ObjectReferenceType);
	}

	if (PossibleObjectReferenceTypes & static_cast<uint8>(EObjectReferenceType::ClassReference))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Class;
		TSharedRef<SWidget> Widget = CreateObjectReferenceWidget(InItem, PinType, IconBrush, FText::Format(LOCTEXT("ClassTooltip", "Reference a class of type \'{TypeName}\'"), Args));
		FObjectReferenceListItem ObjectReferenceType = MakeShareable(new FObjectReferenceType(InItem, Widget, PinType.PinCategory));
		AllowedObjectReferenceTypes.Add(ObjectReferenceType);
	}

	if (PossibleObjectReferenceTypes & static_cast<uint8>(EObjectReferenceType::AssetID))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_Asset;
		TSharedRef<SWidget> Widget = CreateObjectReferenceWidget(InItem, PinType, IconBrush, FText::Format(LOCTEXT("AssetTooltip", "Path to an instanced object of type \'{Typename}\' which may be in an unloaded state. Can be utilized to asynchronously load the object reference."), Args));
		FObjectReferenceListItem ObjectReferenceType = MakeShareable(new FObjectReferenceType(InItem, Widget, PinType.PinCategory));
		AllowedObjectReferenceTypes.Add(ObjectReferenceType);
	}

	if (PossibleObjectReferenceTypes & static_cast<uint8>(EObjectReferenceType::ClassAssetID))
	{
		PinType.PinCategory = UEdGraphSchema_K2::PC_AssetClass;
		TSharedRef<SWidget> Widget = CreateObjectReferenceWidget(InItem, PinType, IconBrush, FText::Format(LOCTEXT("ClassAssetTooltip", "Path to a class object of type \'{Typename}\' which may be in an unloaded state. Can be utilized to asynchronously load the class."), Args));
		FObjectReferenceListItem ObjectReferenceType = MakeShareable(new FObjectReferenceType(InItem, Widget, PinType.PinCategory));
		AllowedObjectReferenceTypes.Add(ObjectReferenceType);
	}

	TSharedPtr<SListView<FObjectReferenceListItem>> ListView;
	SAssignNew(ListView, SListView<FObjectReferenceListItem>)
		.ListItemsSource(&AllowedObjectReferenceTypes)
		.SelectionMode(ESelectionMode::Single)
		.OnGenerateRow(this, &SPinTypeSelector::GenerateObjectReferenceTreeRow)
		.OnSelectionChanged(this, &SPinTypeSelector::OnObjectReferenceSelectionChanged);

	WeakListView = ListView;
	if (AllowedObjectReferenceTypes.Num())
	{
		ListView->SetSelection(AllowedObjectReferenceTypes[0], ESelectInfo::OnNavigation);
	}

	return 
		SNew(SBorder)
		.BorderImage(FEditorStyle::GetBrush("Menu.Background"))
		[
			SNew(SObjectReferenceWidget, PinTypeSelectorMenuOwner)
			[
				SNew(SListViewSelectorDropdownMenu<FObjectReferenceListItem>, nullptr, ListView)
				[
					ListView.ToSharedRef()
				]
			]
		];
}

void SPinTypeSelector::OnSelectPinType(FPinTypeTreeItem InItem, FString InPinCategory)
{
	const FScopedTransaction Transaction( LOCTEXT("ChangeParam", "Change Parameter Type") );

	FEdGraphPinType NewTargetPinType = TargetPinType.Get();
	//Call delegate in order to notify pin type change is about to happen
	OnTypePreChanged.ExecuteIfBound(NewTargetPinType);

	const FEdGraphPinType& SelectionPinType = InItem->GetPinType(true);

	// Change the pin's type
	NewTargetPinType.PinCategory = InPinCategory;
	NewTargetPinType.PinSubCategory = SelectionPinType.PinSubCategory;
	NewTargetPinType.PinSubCategoryObject = SelectionPinType.PinSubCategoryObject;

	TypeComboButton->SetIsOpen(false);

	if( NewTargetPinType.PinCategory == Schema->PC_Exec )
	{
		NewTargetPinType.bIsArray = false;
	}

	OnTypeChanged.ExecuteIfBound(NewTargetPinType);
}

void SPinTypeSelector::OnTypeSelectionChanged(FPinTypeTreeItem Selection, ESelectInfo::Type SelectInfo)
{
	// When the user is navigating, do not act upon the selection change
	if(SelectInfo == ESelectInfo::OnNavigation)
	{
		// Unless mouse clicking on an item with a sub-menu, all attempts to auto-select should open the sub-menu
		TSharedPtr<SPinTypeRow> PinRow = StaticCastSharedPtr<SPinTypeRow>(TypeTreeView->WidgetFromItem(Selection));
		if (PinRow.IsValid() && PinTypeSelectorMenuOwner.IsValid())
		{
			PinTypeSelectorMenuOwner.Pin()->CloseSummonedMenus();
		}
		return;
	}

	// Only handle selection for non-read only items, since STreeViewItem doesn't actually support read-only
	if( Selection.IsValid() )
	{
		if( !Selection->bReadOnly )
		{
			// Unless mouse clicking on an item with a sub-menu, all attempts to auto-select should open the sub-menu
			TSharedPtr<SPinTypeRow> PinRow = StaticCastSharedPtr<SPinTypeRow>(TypeTreeView->WidgetFromItem(Selection));
			if (SelectInfo != ESelectInfo::OnMouseClick && PinRow.IsValid() && PinRow->HasSubMenu() && !PinRow->IsSubMenuOpen())
			{
				PinRow->RequestSubMenuToggle(true);
				FSlateApplication::Get().SetKeyboardFocus(WeakListView.Pin(), EFocusCause::SetDirectly);
			}
			else
			{
				OnSelectPinType(Selection, Selection->GetPossibleObjectReferenceTypes() == static_cast<uint8>(EObjectReferenceType::AllTypes)? Schema->PC_Object : Selection->GetPinType(false).PinCategory);
			}
		}
		else
		{
			// Expand / contract the category, if applicable
			if( Selection->Children.Num() > 0 )
			{
				const bool bIsExpanded = TypeTreeView->IsItemExpanded(Selection);
				TypeTreeView->SetItemExpansion(Selection, !bIsExpanded);

				if(SelectInfo == ESelectInfo::OnMouseClick)
				{
					TypeTreeView->ClearSelection();
				}
			}
		}
	}
}

void SPinTypeSelector::GetTypeChildren(FPinTypeTreeItem InItem, TArray<FPinTypeTreeItem>& OutChildren)
{
	OutChildren = InItem->Children;
}

TSharedRef<SWidget>	SPinTypeSelector::GetMenuContent()
{
	GetPinTypeTree.Execute(TypeTreeRoot, TypeTreeFilter);

	// Remove read-only root items if they have no children; there will be no subtree to select non read-only items from in that case
	int32 RootItemIndex = 0;
	while(RootItemIndex < TypeTreeRoot.Num())
	{
		FPinTypeTreeItem TypeTreeItemPtr = TypeTreeRoot[RootItemIndex];
		if(TypeTreeItemPtr.IsValid()
			&& TypeTreeItemPtr->bReadOnly
			&& TypeTreeItemPtr->Children.Num() == 0)
		{
			TypeTreeRoot.RemoveAt(RootItemIndex);
		}
		else
		{
			++RootItemIndex;
		}
	}

	FilteredTypeTreeRoot = TypeTreeRoot;

	if( !MenuContent.IsValid() )
	{
		// Pre-build the tree view and search box as it is needed as a parameter for the context menu's container.
		SAssignNew(TypeTreeView, SPinTypeTreeView)
			.TreeItemsSource(&FilteredTypeTreeRoot)
			.SelectionMode(ESelectionMode::Single)
			.OnGenerateRow(this, &SPinTypeSelector::GenerateTypeTreeRow)
			.OnSelectionChanged(this, &SPinTypeSelector::OnTypeSelectionChanged)
			.OnGetChildren(this, &SPinTypeSelector::GetTypeChildren);

		SAssignNew(FilterTextBox, SSearchBox)
			.OnTextChanged( this, &SPinTypeSelector::OnFilterTextChanged )
			.OnTextCommitted( this, &SPinTypeSelector::OnFilterTextCommitted );

		MenuContent = SAssignNew(PinTypeSelectorMenuOwner, SMenuOwner)
			[
				SNew(SListViewSelectorDropdownMenu<FPinTypeTreeItem>, FilterTextBox, TypeTreeView)
				[
					SNew( SVerticalBox )
					+SVerticalBox::Slot()
					.AutoHeight()
					.Padding(4.f, 4.f, 4.f, 4.f)
					[
						FilterTextBox.ToSharedRef()
					]
					+SVerticalBox::Slot()
						.AutoHeight()
						.Padding(4.f, 4.f, 4.f, 4.f)
						[
							SNew(SBox)
							.HeightOverride(TreeViewHeight)
							.WidthOverride(TreeViewWidth)
							[
								TypeTreeView.ToSharedRef()
							]
						]
				]
			];
			

			TypeComboButton->SetMenuContentWidgetToFocus(FilterTextBox);
	}
	else
	{
		// Clear the selection in such a way as to also clear the keyboard selector
		TypeTreeView->SetSelection(NULL, ESelectInfo::OnNavigation);
		TypeTreeView->ClearExpandedItems();
	}

	// Clear the filter text box with each opening
	if( FilterTextBox.IsValid() )
	{
		FilterTextBox->SetText( FText::GetEmpty() );
	}

	return MenuContent.ToSharedRef();
}

//=======================================================================
// Search Support
void SPinTypeSelector::OnFilterTextChanged(const FText& NewText)
{
	SearchText = NewText;
	FilteredTypeTreeRoot.Empty();

	GetChildrenMatchingSearch(NewText, TypeTreeRoot, FilteredTypeTreeRoot);
	TypeTreeView->RequestTreeRefresh();

	// Select the first non-category item
	auto SelectedItems = TypeTreeView->GetSelectedItems();
	if(FilteredTypeTreeRoot.Num() > 0)
	{
		// Categories have children, we don't want to select categories
		if(FilteredTypeTreeRoot[0]->Children.Num() > 0)
		{
			TypeTreeView->SetSelection(FilteredTypeTreeRoot[0]->Children[0], ESelectInfo::OnNavigation);
		}
		else
		{
			TypeTreeView->SetSelection(FilteredTypeTreeRoot[0], ESelectInfo::OnNavigation);
		}
	}
}

void SPinTypeSelector::OnFilterTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if(CommitInfo == ETextCommit::OnEnter)
	{
		auto SelectedItems = TypeTreeView->GetSelectedItems();
		if(SelectedItems.Num() > 0)
		{
			TypeTreeView->SetSelection(SelectedItems[0]);
		}
	}
}

bool SPinTypeSelector::GetChildrenMatchingSearch(const FText& InSearchText, const TArray<FPinTypeTreeItem>& UnfilteredList, TArray<FPinTypeTreeItem>& OutFilteredList)
{
	// Trim and sanitized the filter text (so that it more likely matches the action descriptions)
	FString TrimmedFilterString = FText::TrimPrecedingAndTrailing(InSearchText).ToString();

	// Tokenize the search box text into a set of terms; all of them must be present to pass the filter
	TArray<FString> FilterTerms;
	TrimmedFilterString.ParseIntoArray(FilterTerms, TEXT(" "), true);

	// Generate a list of sanitized versions of the strings
	TArray<FString> SanitizedFilterTerms;
	for (int32 iFilters = 0; iFilters < FilterTerms.Num() ; iFilters++)
	{
		FString EachString = FName::NameToDisplayString( FilterTerms[iFilters], false );
		EachString = EachString.Replace( TEXT( " " ), TEXT( "" ) );
		SanitizedFilterTerms.Add( EachString );
	}
	// Both of these should match!
	ensure( SanitizedFilterTerms.Num() == FilterTerms.Num() );

	bool bReturnVal = false;

	for( auto it = UnfilteredList.CreateConstIterator(); it; ++it )
	{
		FPinTypeTreeItem Item = *it;
		FPinTypeTreeItem NewInfo = MakeShareable( new UEdGraphSchema_K2::FPinTypeTreeInfo(Item) );
		TArray<FPinTypeTreeItem> ValidChildren;

		const bool bHasChildrenMatchingSearch = GetChildrenMatchingSearch(InSearchText, Item->Children, ValidChildren);
		const bool bIsEmptySearch = InSearchText.IsEmpty();
		bool bFilterTextMatches = true;

		// If children match the search filter or it's an empty search, let's not do any checks against the FilterTerms
		if ( !bHasChildrenMatchingSearch && !bIsEmptySearch)
		{
			FString DescriptionString =  Item->GetDescription().ToString();
			DescriptionString = DescriptionString.Replace( TEXT( " " ), TEXT( "" ) );
			for (int32 FilterIndex = 0; (FilterIndex < FilterTerms.Num()) && bFilterTextMatches; ++FilterIndex)
			{
				const bool bMatchesTerm = ( DescriptionString.Contains( FilterTerms[FilterIndex] ) || ( DescriptionString.Contains( SanitizedFilterTerms[FilterIndex] ) == true ) );
				bFilterTextMatches = bFilterTextMatches && bMatchesTerm;
			}
		}
		if( bHasChildrenMatchingSearch
			|| bIsEmptySearch
			|| bFilterTextMatches )
		{
			NewInfo->Children = ValidChildren;
			OutFilteredList.Add(NewInfo);

			TypeTreeView->SetItemExpansion(NewInfo, !InSearchText.IsEmpty());

			bReturnVal = true;
		}
	}

	return bReturnVal;
}

FText SPinTypeSelector::GetToolTipForComboBoxType() const
{
	if(IsEnabled())
	{
		if (bIsCompactSelector)
		{
			return LOCTEXT("CompactPinTypeSelector", "Left click to select the variable's pin type. Right click to toggle the type as an array.");
		}
		else
		{
			return LOCTEXT("PinTypeSelector", "Select the variable's pin type.");
		}
	}

	return LOCTEXT("PinTypeSelector_Disabled", "Cannot edit variable type when they are inherited from parent.");
}

FText SPinTypeSelector::GetToolTipForArrayWidget() const
{
	if(IsEnabled())
	{
		// The entire widget may be enabled, but the array button disabled because it is an "exec" pin.
		if(TargetPinType.Get().PinCategory == Schema->PC_Exec)
		{
			return LOCTEXT("ArrayCheckBox_ExecDisabled", "Exec pins cannot be arrays.");
		}
		return LOCTEXT("ArrayCheckBox", "Make this variable an array of selected type.");
	}

	return LOCTEXT("ArrayCheckBox_Disabled", "Cannot edit variable type while the variable is placed in a graph or inherited from parent.");
}

FReply SPinTypeSelector::OnMouseButtonDown( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if(bIsCompactSelector && MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		bIsRightMousePressed = true;
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

FReply SPinTypeSelector::OnMouseButtonUp( const FGeometry& MyGeometry, const FPointerEvent& MouseEvent )
{
	if(bIsCompactSelector && MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
	{
		if (bIsRightMousePressed)
		{
			OnArrayStateToggled();
		}
		return FReply::Handled();
	}

	return FReply::Unhandled();
}

void SPinTypeSelector::OnMouseLeave( const FPointerEvent& MouseEvent )
{
	SCompoundWidget::OnMouseLeave(MouseEvent);
	bIsRightMousePressed = false;
}

#undef LOCTEXT_NAMESPACE

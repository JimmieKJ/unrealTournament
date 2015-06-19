// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "KismetWidgetsPrivatePCH.h"
#include "UnrealEd.h"
#include "ClassIconFinder.h"
#include "SPinTypeSelector.h"
#include "IDocumentation.h"
#include "Editor/UnrealEd/Public/SListViewSelectorDropdownMenu.h"
#include "SSearchBox.h"

#define LOCTEXT_NAMESPACE "PinTypeSelector"

static const FString BigTooltipDocLink = TEXT("Shared/Editor/Blueprint/VariableTypes");

void SPinTypeSelector::Construct(const FArguments& InArgs, FGetPinTypeTree GetPinTypeTreeFunc)
{
	SearchText = FText::GetEmpty();

	OnTypeChanged = InArgs._OnPinTypeChanged;
	OnTypePreChanged = InArgs._OnPinTypePreChanged;

	check(GetPinTypeTreeFunc.IsBound());
	GetPinTypeTree = GetPinTypeTreeFunc;

	Schema = (UEdGraphSchema_K2*)(InArgs._Schema);
	bAllowExec = InArgs._bAllowExec;
	bAllowWildcard = InArgs._bAllowWildcard;
	TreeViewWidth = InArgs._TreeViewWidth;
	TreeViewHeight = InArgs._TreeViewHeight;

	TargetPinType = InArgs._TargetPinType;

	this->ChildSlot
	[
		SNew(SHorizontalBox)
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
		]
	];
}

//=======================================================================
// Attribute Helpers

FText SPinTypeSelector::GetTypeDescription() const
{
	const UObject* PinSubCategoryObject = TargetPinType.Get().PinSubCategoryObject.Get();
	if (PinSubCategoryObject)
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
	return GetIconFromPin( TargetPinType.Get() );
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

//=======================================================================
// Type TreeView Support
TSharedRef<ITableRow> SPinTypeSelector::GenerateTypeTreeRow(FPinTypeTreeItem InItem, const TSharedRef<STableViewBase>& OwnerTree)
{
	const bool bHasChildren = (InItem->Children.Num() > 0);
	const FText Description = InItem->GetDescription();
	const FEdGraphPinType& PinType = InItem->GetPinType(false);

	// Determine the best icon the to represents this item
	const FSlateBrush* IconBrush = GetIconFromPin(PinType);

	// Use tooltip if supplied, otherwise just repeat description
	const FText OrgTooltip = InItem->GetToolTip();
	const FText Tooltip = !OrgTooltip.IsEmpty() ? OrgTooltip : Description;

	const FString PinTooltipExcerpt = ((PinType.PinCategory != UEdGraphSchema_K2::PC_Byte || PinType.PinSubCategoryObject == nullptr) ? PinType.PinCategory : TEXT("Enum")); 

	return SNew( SComboRow<FPinTypeTreeItem>, OwnerTree )
		.ToolTip( IDocumentation::Get()->CreateToolTip( Tooltip, NULL, *BigTooltipDocLink, PinTooltipExcerpt) )
		[
			SNew(SHorizontalBox)
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
}

void SPinTypeSelector::OnTypeSelectionChanged(FPinTypeTreeItem Selection, ESelectInfo::Type SelectInfo)
{
	// When the user is navigating, do not act upon the selection change
	if(SelectInfo == ESelectInfo::OnNavigation)
	{
		return;
	}

	// Only handle selection for non-read only items, since STreeViewItem doesn't actually support read-only
	if( Selection.IsValid() )
	{
		if( !Selection->bReadOnly )
		{
			const FScopedTransaction Transaction( LOCTEXT("ChangeParam", "Change Paramater Type") );
			
			FEdGraphPinType NewTargetPinType = TargetPinType.Get();
			//Call delegate in order to notify pin type change is about to happen
			OnTypePreChanged.ExecuteIfBound(NewTargetPinType);

			const FEdGraphPinType& SelectionPinType = Selection->GetPinType(true);

			// Change the pin's type
			NewTargetPinType.PinCategory = SelectionPinType.PinCategory;
			NewTargetPinType.PinSubCategory = SelectionPinType.PinSubCategory;
			NewTargetPinType.PinSubCategoryObject = SelectionPinType.PinSubCategoryObject;

			TypeComboButton->SetIsOpen(false);

			if( NewTargetPinType.PinCategory == Schema->PC_Exec )
			{
				NewTargetPinType.bIsArray = false;
			}

			OnTypeChanged.ExecuteIfBound(NewTargetPinType);
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
	GetPinTypeTree.Execute(TypeTreeRoot, bAllowExec, bAllowWildcard);

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

		MenuContent = SNew(SListViewSelectorDropdownMenu<FPinTypeTreeItem>, FilterTextBox, TypeTreeView)
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

	GetChildrenMatchingSearch(FName::NameToDisplayString(NewText.ToString(), false), TypeTreeRoot, FilteredTypeTreeRoot);
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

bool SPinTypeSelector::GetChildrenMatchingSearch(const FString& InSearchText, const TArray<FPinTypeTreeItem>& UnfilteredList, TArray<FPinTypeTreeItem>& OutFilteredList)
{
	bool bReturnVal = false;

	for( auto it = UnfilteredList.CreateConstIterator(); it; ++it )
	{
		FPinTypeTreeItem Item = *it;
		FPinTypeTreeItem NewInfo = MakeShareable( new UEdGraphSchema_K2::FPinTypeTreeInfo(Item) );
		TArray<FPinTypeTreeItem> ValidChildren;

		// Have to run GetChildrenMatchingSearch first, so that we can make sure we get valid children for the list!
		if( GetChildrenMatchingSearch(InSearchText, Item->Children, ValidChildren)
			|| InSearchText.IsEmpty()
			|| Item->GetDescription().ToString().Contains(InSearchText) )
		{
			NewInfo->Children = ValidChildren;
			OutFilteredList.Add(NewInfo);

			TypeTreeView->SetItemExpansion(NewInfo, !InSearchText.IsEmpty());

			bReturnVal = true;
		}
	}

	return bReturnVal;
}

const FSlateBrush* SPinTypeSelector::GetIconFromPin( const FEdGraphPinType& PinType ) const
{
	const FSlateBrush* IconBrush = FEditorStyle::GetBrush(TEXT("Kismet.VariableList.TypeIcon"));
	const UObject* PinSubObject = PinType.PinSubCategoryObject.Get();
	if( (PinType.bIsArray || (IsArrayChecked() == ECheckBoxState::Checked)) && PinType.PinCategory != Schema->PC_Exec )
	{
		IconBrush = FEditorStyle::GetBrush(TEXT("Kismet.VariableList.ArrayTypeIcon"));
	}
	else if( PinSubObject )
	{
		UClass* VarClass = FindObject<UClass>(ANY_PACKAGE, *PinSubObject->GetName());
		if( VarClass )
		{
			IconBrush = FClassIconFinder::FindIconForClass( VarClass );
		}
	}
	return IconBrush;
}

FText SPinTypeSelector::GetToolTipForComboBoxType() const
{
	if(IsEnabled())
	{
		return LOCTEXT("PinTypeSelector", "Select the variable's pin type.");
	}

	return LOCTEXT("PinTypeSelector_Disabled", "Cannot edit variable type while the variable is placed in a graph or inherited from parent.");
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
#undef LOCTEXT_NAMESPACE

// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "BlueprintEditorPrivatePCH.h"
#include "UserDefinedEnumEditor.h"
#include "Editor/WorkspaceMenuStructure/Public/WorkspaceMenuStructureModule.h"

#include "PropertyCustomizationHelpers.h"
#include "SDockTab.h"
#include "IDocumentation.h"
#include "Engine/UserDefinedEnum.h"

#define LOCTEXT_NAMESPACE "UserDefinedEnumEditor"

const FName FUserDefinedEnumEditor::EnumeratorsTabId( TEXT( "UserDefinedEnum_EnumeratorEditor" ) );
const FName FUserDefinedEnumEditor::UserDefinedEnumEditorAppIdentifier( TEXT( "UserDefinedEnumEditorApp" ) );

void FUserDefinedEnumEditor::RegisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	WorkspaceMenuCategory = InTabManager->AddLocalWorkspaceMenuCategory(LOCTEXT("WorkspaceMenu_UserDefinedEnumEditor", "User-Defined Enum Editor"));

	FAssetEditorToolkit::RegisterTabSpawners(InTabManager);

	InTabManager->RegisterTabSpawner( EnumeratorsTabId, FOnSpawnTab::CreateSP(this, &FUserDefinedEnumEditor::SpawnEnumeratorsTab) )
		.SetDisplayName( LOCTEXT("EnumeratorEditor", "Enumerators") )
		.SetGroup(WorkspaceMenuCategory.ToSharedRef())
		.SetIcon(FSlateIcon(FEditorStyle::GetStyleSetName(), "GraphEditor.Enum_16x"));
}

void FUserDefinedEnumEditor::UnregisterTabSpawners(const TSharedRef<class FTabManager>& InTabManager)
{
	FAssetEditorToolkit::UnregisterTabSpawners(InTabManager);

	InTabManager->UnregisterTabSpawner( EnumeratorsTabId );
}

void FUserDefinedEnumEditor::InitEditor(const EToolkitMode::Type Mode, const TSharedPtr< class IToolkitHost >& InitToolkitHost, UUserDefinedEnum* EnumToEdit)
{
	const TSharedRef<FTabManager::FLayout> StandaloneDefaultLayout = FTabManager::NewLayout( "Standalone_UserDefinedEnumEditor_Layout_v1" )
	->AddArea
	(
		FTabManager::NewPrimaryArea() ->SetOrientation(Orient_Vertical)
		->Split
		(
			FTabManager::NewStack()
			->SetSizeCoefficient(0.1f)
			->SetHideTabWell( true )
			->AddTab(GetToolbarTabId(), ETabState::OpenedTab)
		)
		->Split
		(
			FTabManager::NewSplitter()
			->Split
			(
				FTabManager::NewStack()
				->AddTab( EnumeratorsTabId, ETabState::OpenedTab )
			)
		)
	);

	const bool bCreateDefaultStandaloneMenu = true;
	const bool bCreateDefaultToolbar = true;
	FAssetEditorToolkit::InitAssetEditor( Mode, InitToolkitHost, UserDefinedEnumEditorAppIdentifier, StandaloneDefaultLayout, bCreateDefaultStandaloneMenu, bCreateDefaultToolbar, EnumToEdit );

	// @todo toolkit world centric editing
	/*if (IsWorldCentricAssetEditor())
	{
		SpawnToolkitTab(GetToolbarTabId(), FString(), EToolkitTabSpot::ToolBar);
		const FString TabInitializationPayload(TEXT(""));	
		SpawnToolkitTab( EnumeratorsTabId, TabInitializationPayload, EToolkitTabSpot::Details );
	}*/

	//
}

TSharedRef<SDockTab> FUserDefinedEnumEditor::SpawnEnumeratorsTab(const FSpawnTabArgs& Args)
{
	check( Args.GetTabId() == EnumeratorsTabId );

	UUserDefinedEnum* EditedEnum = NULL;
	const TArray<UObject*>& EditingObjs = GetEditingObjects();
	if (EditingObjs.Num())
	{
		EditedEnum = Cast<UUserDefinedEnum>(EditingObjs[ 0 ]);
	}

	// Create a property view
	FPropertyEditorModule& EditModule = FModuleManager::Get().GetModuleChecked<FPropertyEditorModule>("PropertyEditor");

	FDetailsViewArgs DetailsViewArgs( /*bUpdateFromSelection=*/ false, /*bLockable=*/ false, /*bAllowSearch=*/ false, FDetailsViewArgs::HideNameArea, /*bHideSelectionTip=*/ true );
	DetailsViewArgs.bShowOptions = false;

	PropertyView = EditModule.CreateDetailView( DetailsViewArgs );

	FOnGetDetailCustomizationInstance LayoutEnumDetails = FOnGetDetailCustomizationInstance::CreateStatic(&FEnumDetails::MakeInstance);
	PropertyView->RegisterInstancedCustomPropertyLayout(UUserDefinedEnum::StaticClass(), LayoutEnumDetails);

	PropertyView->SetObject(EditedEnum);

	return SNew(SDockTab)
		.Icon( FEditorStyle::GetBrush("GenericEditor.Tabs.Properties") )
		.Label( LOCTEXT("EnumeratorEditor", "Enumerators") )
		.TabColorScale( GetTabColorScale() )
		[
			PropertyView.ToSharedRef()
		];
}

FUserDefinedEnumEditor::~FUserDefinedEnumEditor()
{
}

FName FUserDefinedEnumEditor::GetToolkitFName() const
{
	return FName("EnumEditor");
}

FText FUserDefinedEnumEditor::GetBaseToolkitName() const
{
	return LOCTEXT( "AppLabel", "Enum Editor" );
}

FText FUserDefinedEnumEditor::GetToolkitName() const
{
	if (1 == GetEditingObjects().Num())
	{
		return FAssetEditorToolkit::GetToolkitName();
	}
	return GetBaseToolkitName();
}

FText FUserDefinedEnumEditor::GetToolkitToolTipText() const
{
	if (1 == GetEditingObjects().Num())
	{
		return FAssetEditorToolkit::GetToolkitToolTipText();
	}
	return GetBaseToolkitName();
}

FString FUserDefinedEnumEditor::GetWorldCentricTabPrefix() const
{
	return LOCTEXT("UDEnumWorldCentricTabPrefix", "Enum ").ToString();
}

FLinearColor FUserDefinedEnumEditor::GetWorldCentricTabColorScale() const
{
	return FLinearColor( 0.5f, 0.0f, 0.0f, 0.5f );
}



void FEnumDetails::CustomizeDetails( IDetailLayoutBuilder& DetailLayout )
{
	const TArray<TWeakObjectPtr<UObject>> Objects = DetailLayout.GetDetailsView().GetSelectedObjects();
	check(Objects.Num() > 0);

	if (Objects.Num() == 1)
	{
		TargetEnum = CastChecked<UUserDefinedEnum>(Objects[0].Get());
		TSharedRef<IPropertyHandle> PropertyHandle = DetailLayout.GetProperty(FName("Names"), UEnum::StaticClass());

		const FString DocLink = TEXT("Shared/Editors/BlueprintEditor/EnumDetails");

		IDetailCategoryBuilder& InputsCategory = DetailLayout.EditCategory("Enumerators", LOCTEXT("EnumDetailsEnumerators", "Enumerators"));

		InputsCategory.AddCustomRow( LOCTEXT("FunctionNewInputArg", "New") )
			[
				SNew(SBox)
				.HAlign(HAlign_Right)
				[
					SNew(SButton)
					.Text(LOCTEXT("FunctionNewInputArg", "New"))
					.OnClicked(this, &FEnumDetails::OnAddNewEnumerator)
				]
			];

		Layout = MakeShareable( new FUserDefinedEnumLayout(TargetEnum.Get()) );
		InputsCategory.AddCustomBuilder( Layout.ToSharedRef() );

		TSharedPtr<SToolTip> BitmaskFlagsTooltip = IDocumentation::Get()->CreateToolTip(LOCTEXT("BitmaskFlagsTooltip", "When enabled, this enumeration can be used as a set of explicitly-named bitmask flags. Each enumerator's value will correspond to the index of the bit (flag) in the mask."), nullptr, DocLink, TEXT("Bitmask Flags"));

		InputsCategory.AddCustomRow(LOCTEXT("BitmaskFlagsAttributeLabel", "Bitmask Flags"), true)
		.NameContent()
		[
			SNew(STextBlock)
			.Text(LOCTEXT("BitmaskFlagsAttributeLabel", "Bitmask Flags"))
			.ToolTip(BitmaskFlagsTooltip)
		]
		.ValueContent()
		[
			SNew(SCheckBox)
			.IsChecked(this, &FEnumDetails::OnGetBitmaskFlagsAttributeState)
			.OnCheckStateChanged(this, &FEnumDetails::OnBitmaskFlagsAttributeStateChanged)
			.ToolTip(BitmaskFlagsTooltip)
		];
	}
}

FEnumDetails::~FEnumDetails()
{
}

void FEnumDetails::OnForceRefresh()
{
	if (Layout.IsValid())
	{
		Layout->Refresh();
	}
}

void FEnumDetails::PreChange(const class UUserDefinedEnum* Enum, FEnumEditorUtils::EEnumEditorChangeInfo Info)
{
}

void FEnumDetails::PostChange(const class UUserDefinedEnum* Enum, FEnumEditorUtils::EEnumEditorChangeInfo Info)
{
	if (Enum && (TargetEnum.Get() == Enum))
	{
		OnForceRefresh();
	}
}

FReply FEnumDetails::OnAddNewEnumerator()
{
	FEnumEditorUtils::AddNewEnumeratorForUserDefinedEnum(TargetEnum.Get());
	return FReply::Handled();
}

ECheckBoxState FEnumDetails::OnGetBitmaskFlagsAttributeState() const
{
	return FEnumEditorUtils::IsEnumeratorBitflagsType(TargetEnum.Get()) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

void FEnumDetails::OnBitmaskFlagsAttributeStateChanged(ECheckBoxState InNewState)
{
	FEnumEditorUtils::SetEnumeratorBitflagsTypeState(TargetEnum.Get(), InNewState == ECheckBoxState::Checked);
}

bool FUserDefinedEnumLayout::CausedChange() const
{
	for(auto Iter = Children.CreateConstIterator(); Iter; ++Iter)
	{
		const auto Child = (*Iter);
		if (Child.IsValid() && Child.Pin()->CausedChange())
		{
			return true;
		}
	}
	return false;
}

void FUserDefinedEnumLayout::GenerateChildContent( IDetailChildrenBuilder& ChildrenBuilder )
{
	Children.Empty();
	const int32 EnumToShowNum = FMath::Max<int32>(0, TargetEnum->NumEnums() - 1);
	for (int32 EnumIdx = 0; EnumIdx < EnumToShowNum; ++EnumIdx)
	{
		TSharedRef<class FUserDefinedEnumIndexLayout> EnumIndexLayout = MakeShareable(new FUserDefinedEnumIndexLayout(TargetEnum.Get(), EnumIdx) );
		ChildrenBuilder.AddChildCustomBuilder(EnumIndexLayout);
		Children.Add(EnumIndexLayout);
	}
}



void FUserDefinedEnumIndexLayout::GenerateHeaderRowContent( FDetailWidgetRow& NodeRow )
{
	const bool bIsEditable = true;
	const bool bIsMoveUpEnabled = (TargetEnum->NumEnums() != 1) && (EnumeratorIndex != 0) && bIsEditable;
	const bool bIsMoveDownEnabled = (TargetEnum->NumEnums() != 1) && (EnumeratorIndex < TargetEnum->NumEnums() - 2) && bIsEditable;

	TSharedRef< SWidget > ClearButton = PropertyCustomizationHelpers::MakeClearButton(FSimpleDelegate::CreateSP(this, &FUserDefinedEnumIndexLayout::OnEnumeratorRemove));
	ClearButton->SetEnabled(bIsEditable);

	NodeRow
		.WholeRowWidget
		[
			SNew(SHorizontalBox)

			+SHorizontalBox::Slot()
			[
				SAssignNew(EnumeratorNameWidget, SEditableTextBox)
				.OnTextCommitted(this, &FUserDefinedEnumIndexLayout::OnEnumeratorNameCommitted)
				.OnTextChanged(this, &FUserDefinedEnumIndexLayout::OnEnumeratorNameChanged)
				.IsEnabled(bIsEditable)
				.Text(this, &FUserDefinedEnumIndexLayout::GetEnumeratorName)
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2, 0)
			[
				SNew(SButton)
				.ContentPadding(0)
				.OnClicked(this, &FUserDefinedEnumIndexLayout::OnMoveEnumeratorUp)
				.IsEnabled( bIsMoveUpEnabled )
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("BlueprintEditor.Details.ArgUpButton"))
				]
			]
			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2, 0)
			[
				SNew(SButton)
				.ContentPadding(0)
				.OnClicked(this, &FUserDefinedEnumIndexLayout::OnMoveEnumeratorDown)
				.IsEnabled( bIsMoveDownEnabled )
				[
					SNew(SImage)
					.Image(FEditorStyle::GetBrush("BlueprintEditor.Details.ArgDownButton"))
				]
			]

			+SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(2, 0)
			[
				ClearButton
			]
		];
}

FText FUserDefinedEnumIndexLayout::GetEnumeratorName() const
{
	return FText::FromString(FEnumEditorUtils::GetEnumeratorDisplayName(TargetEnum.Get(), EnumeratorIndex));
}

void FUserDefinedEnumIndexLayout::OnEnumeratorRemove()
{
	FEnumEditorUtils::RemoveEnumeratorFromUserDefinedEnum(TargetEnum.Get(), EnumeratorIndex);
}

FReply FUserDefinedEnumIndexLayout::OnMoveEnumeratorUp()
{
	FEnumEditorUtils::MoveEnumeratorInUserDefinedEnum(TargetEnum.Get(), EnumeratorIndex, true);
	return FReply::Handled();
}

FReply FUserDefinedEnumIndexLayout::OnMoveEnumeratorDown()
{
	FEnumEditorUtils::MoveEnumeratorInUserDefinedEnum(TargetEnum.Get(), EnumeratorIndex, false);
	return FReply::Handled();
}

struct FScopeTrue
{
	bool& bRef;

	FScopeTrue(bool& bInRef) : bRef(bInRef)
	{
		ensure(!bRef);
		bRef = true;
	}

	~FScopeTrue()
	{
		ensure(bRef);
		bRef = false;
	}
};

void FUserDefinedEnumIndexLayout::OnEnumeratorNameCommitted(const FText& NewText, ETextCommit::Type InTextCommit)
{
	const FString NewDisplayName = NewText.ToString();
	if (FEnumEditorUtils::IsEnumeratorDisplayNameValid(TargetEnum.Get(), NewDisplayName))
	{
		FScopeTrue ScopeTrue(bCausedChange);
		FEnumEditorUtils::SetEnumeratorDisplayName(TargetEnum.Get(), EnumeratorIndex, NewDisplayName);
	}

	auto EnumeratorNameWidgetSP = EnumeratorNameWidget.Pin();
	if (EnumeratorNameWidgetSP.IsValid())
	{
		EnumeratorNameWidgetSP->SetError(FString());
	}
}

void FUserDefinedEnumIndexLayout::OnEnumeratorNameChanged(const FText& NewText)
{
	IsValidEnumeratorDisplayName(NewText);
}

bool FUserDefinedEnumIndexLayout::IsValidEnumeratorDisplayName(const FText& NewText) const
{
	bool bValidName = true;

	const FString NewName = NewText.ToString();
	bool bUnchangedName = (NewName == FEnumEditorUtils::GetEnumeratorDisplayName(TargetEnum.Get(), EnumeratorIndex));
	FText ErrorMsg;
	if (NewText.IsEmpty())
	{
		ErrorMsg = LOCTEXT("NameMissingError", "You must provide a name.");
		bValidName = false;
	}
	else if (!bUnchangedName && !FEnumEditorUtils::IsEnumeratorDisplayNameValid(TargetEnum.Get(), NewName))
	{
		ErrorMsg = FText::Format(LOCTEXT("NameInUseError", "'{0}' is already in use."), NewText);
		bValidName = false;
	}

	auto EnumeratorNameWidgetSP = EnumeratorNameWidget.Pin();
	if (EnumeratorNameWidgetSP.IsValid())
	{
		EnumeratorNameWidgetSP->SetError(ErrorMsg);
	}

	return bValidName && !bUnchangedName;
}

#undef LOCTEXT_NAMESPACE
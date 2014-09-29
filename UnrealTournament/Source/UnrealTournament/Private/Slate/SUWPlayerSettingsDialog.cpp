// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../Public/UnrealTournament.h"
#include "SUWPlayerSettingsDialog.h"
#include "SUWindowsStyle.h"
#include "Runtime/AppFramework/Public/Widgets/Colors/SColorPicker.h"
#include "AssetData.h"

// scale factor for weapon/view bob sliders (i.e. configurable value between 0 and this)
static const float BOB_SCALING_FACTOR = 2.0f;

#if !UE_SERVER
void SUWPlayerSettingsDialog::Construct(const FArguments& InArgs)
{
	SUWDialog::Construct(SUWDialog::FArguments()
							.PlayerOwner(InArgs._PlayerOwner)
							.DialogTitle(InArgs._DialogTitle)
							.DialogSize(InArgs._DialogSize)
							.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
							.DialogPosition(InArgs._DialogPosition)
							.DialogAnchorPoint(InArgs._DialogAnchorPoint)
							.ContentPadding(InArgs._ContentPadding)
							.ButtonMask(InArgs._ButtonMask)
							.OnDialogResult(InArgs._OnDialogResult)
						);

	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);
	FVector2D ResolutionScale(ViewportSize.X / 1280.0f, ViewportSize.Y / 720.0f);

	UUTGameUserSettings* Settings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());

	WeaponLibrary = UObjectLibrary::CreateLibrary(AUTWeapon::StaticClass(), true, false);
	WeaponLibrary->LoadBlueprintAssetDataFromPath(TEXT("/Game/")); // TODO: do we need to iterate through all the root paths?
	WeaponLibrary->LoadBlueprintAssetDataFromPaths(TArray<FString>()); // HACK: library code doesn't properly handle giving it a root path
	{
		TArray<FAssetData> AssetList;
		WeaponLibrary->GetAssetDataList(AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
			const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
			if (ClassPath != NULL)
			{
				UClass* TestClass = LoadObject<UClass>(NULL, **ClassPath);
				if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(AUTWeapon::StaticClass()))
				{
					WeaponList.Add(TestClass);
				}
			}
		}
	}

	struct FWeaponListSort
	{
		bool operator()(UClass& A, UClass& B) const
		{
			return (A.GetDefaultObject<AUTWeapon>()->GetAutoSwitchPriority() > B.GetDefaultObject<AUTWeapon>()->GetAutoSwitchPriority());
		}
	};
	WeaponList.Sort(FWeaponListSort());

	SelectedPlayerColor = GetDefault<AUTPlayerController>()->FFAPlayerColor;

	if (DialogContent.IsValid())
	{
		const float MessageTextPaddingX = 10.0f;
		TSharedPtr<STextBlock> MessageTextBlock;
		DialogContent->AddSlot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Top)
			.HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(STextBlock)
					.ColorAndOpacity(FLinearColor::Black)
					.Text(NSLOCTEXT("SUWPlayerSettingsDialog", "PlayerName", "Name:").ToString())
				]
				+ SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				.AutoWidth()
				.VAlign(VAlign_Center)
				[
					SAssignNew(PlayerName, SEditableTextBox)
					.OnTextChanged(this, &SUWPlayerSettingsDialog::OnNameTextChanged)
					.MinDesiredWidth(300.0f)
					.Text(FText::FromString(GetPlayerOwner()->GetNickname()))
				]
			]
			+ SVerticalBox::Slot()
			.Padding(0.0f, 10.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SAssignNew(AutoWeaponSwitch, SCheckBox)
				.ForegroundColor(FLinearColor::Black)
				.IsChecked(GetDefault<AUTPlayerController>()->bAutoWeaponSwitch ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked)
				.Content()
				[
					SNew(STextBlock)
					.ColorAndOpacity(FLinearColor::Black)
					.Text(NSLOCTEXT("SUWPlayerSettingsDialog", "AutoWeaponSwitch", "Weapon Switch on Pickup").ToString())
				]
			]
			+ SVerticalBox::Slot()
			.Padding(0.0f, 10.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.ColorAndOpacity(FLinearColor::Black)
				.Text(NSLOCTEXT("SUWPlayerSettingsDialog", "WeaponPriorities", "Weapon Priorities"))
			]
			+ SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(0.0f, 0.0f, 10.0f, 0.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Left)
				[
					SNew(SBorder)
					.Content()
					[
						SAssignNew(WeaponPriorities, SListView<UClass*>)
						.SelectionMode(ESelectionMode::Single)
						.ListItemsSource(&WeaponList)
						.OnGenerateRow(this, &SUWPlayerSettingsDialog::GenerateWeaponListRow)
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(10.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Right)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.Padding(0.0f, 10.0f, 0.0f, 10.0f)
					.AutoHeight()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
						.DesiredSizeScale(FVector2D(1.7f, 1.0f))
						.Text(FString(TEXT("UP"))) // TODO: should be graphical arrow
						.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
						.OnClicked(this, &SUWPlayerSettingsDialog::WeaponPriorityUp)
					]
					+ SVerticalBox::Slot()
					.Padding(0.0f, 10.0f, 0.0f, 10.0f)
					.AutoHeight()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
						.Text(FString(TEXT("DOWN"))) // TODO: should be graphical arrow
						.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
						.OnClicked(this, &SUWPlayerSettingsDialog::WeaponPriorityDown)
					]
				]
			]
			+ SVerticalBox::Slot()
			.Padding(0.0f, 10.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SBox)
					.HAlign(HAlign_Center)
					.WidthOverride(80.0f * ViewportSize.X / 1280.0f)
					.Content()
					[
						SNew(STextBlock)
						.ColorAndOpacity(FLinearColor::Black)
						.Text(NSLOCTEXT("SUWPlayerSettingsDialog", "WeaponBobScaling", "Weapon Bob").ToString())
					]
				]
				+ SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(250.0f * ViewportSize.X / 1280.0f)
					.Content()
					[
						SAssignNew(WeaponBobScaling, SSlider)
						.Orientation(Orient_Horizontal)
						.Value(GetDefault<AUTPlayerController>()->WeaponBobGlobalScaling / BOB_SCALING_FACTOR)
					]
				]
			]
			+ SVerticalBox::Slot()
			.Padding(0.0f, 5.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SBox)
					.HAlign(HAlign_Center)
					.WidthOverride(80.0f * ViewportSize.X / 1280.0f)
					.Content()
					[
						SNew(STextBlock)
						.ColorAndOpacity(FLinearColor::Black)
						.Text(NSLOCTEXT("SUWPlayerSettingsDialog", "ViewBobScaling", "View Bob").ToString())
					]
				]
				+ SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SBox)
					.WidthOverride(250.0f * ViewportSize.X / 1280.0f)
					.Content()
					[
						SAssignNew(ViewBobScaling, SSlider)
						.Orientation(Orient_Horizontal)
						.Value(GetDefault<AUTPlayerController>()->EyeOffsetGlobalScaling / BOB_SCALING_FACTOR)
					]
				]
			]
			+ SVerticalBox::Slot()
			.Padding(0.0f, 10.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				.AutoWidth()
				.VAlign(VAlign_Center)
				.HAlign(HAlign_Center)
				[
					SNew(SBox)
					.HAlign(HAlign_Center)
					.Content()
					[
						SNew(STextBlock)
						.ColorAndOpacity(FLinearColor::Black)
						.Text(NSLOCTEXT("SUWPlayerSettingsDialog", "FFAPlayerColor", "Free for All Player Color").ToString())
					]
				]
				+ SHorizontalBox::Slot()
				.Padding(10.0f, 0.0f, 10.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SColorBlock)
					.Color(this, &SUWPlayerSettingsDialog::GetSelectedPlayerColor)
					.IgnoreAlpha(true)
					.Size(FVector2D(32.0f * ResolutionScale.X, 16.0f * ResolutionScale.Y))
					.OnMouseButtonDown(this, &SUWPlayerSettingsDialog::PlayerColorClicked)
				]
			]
		];

		WeaponPriorities->SetSelection(WeaponList[0]);
	}
}

void SUWPlayerSettingsDialog::OnNameTextChanged(const FText& NewText)
{
	FString AdjustedText = NewText.ToString();
	FString InvalidNameChars = FString(INVALID_NAME_CHARACTERS);
	for (int32 i = AdjustedText.Len() - 1; i >= 0; i--)
	{
		if (InvalidNameChars.GetCharArray().Contains(AdjustedText.GetCharArray()[i]))
		{
			AdjustedText.GetCharArray().RemoveAt(i);
		}
	}
	if (AdjustedText != NewText.ToString())
	{
		PlayerName->SetText(FText::FromString(AdjustedText));
	}
}

TSharedRef<ITableRow> SUWPlayerSettingsDialog::GenerateWeaponListRow(UClass* WeaponType, const TSharedRef<STableViewBase>& OwningList)
{
	checkSlow(WeaponType->IsChildOf(AUTWeapon::StaticClass()));

	return SNew(STableRow<UClass*>, OwningList)
		.Padding(5)
		[
			SNew(STextBlock)
			.ColorAndOpacity(FLinearColor::Black)
			.Text(WeaponType->GetDefaultObject<AUTWeapon>()->DisplayName.ToString())
		];
}

FReply SUWPlayerSettingsDialog::WeaponPriorityUp()
{
	TArray<UClass*> SelectedItems = WeaponPriorities->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		int32 Index = WeaponList.Find(SelectedItems[0]);
		if (Index > 0)
		{
			Exchange(WeaponList[Index], WeaponList[Index - 1]);
			WeaponPriorities->RequestListRefresh();
		}
	}
	return FReply::Handled();
}
FReply SUWPlayerSettingsDialog::WeaponPriorityDown()
{
	TArray<UClass*> SelectedItems = WeaponPriorities->GetSelectedItems();
	if (SelectedItems.Num() > 0)
	{
		int32 Index = WeaponList.Find(SelectedItems[0]);
		if (Index != INDEX_NONE && Index < WeaponList.Num() - 1)
		{
			Exchange(WeaponList[Index], WeaponList[Index + 1]);
			WeaponPriorities->RequestListRefresh();
		}
	}
	return FReply::Handled();
}

FReply SUWPlayerSettingsDialog::PlayerColorClicked(const FGeometry& Geometry, const FPointerEvent& Event)
{
	if (Event.GetEffectingButton() == FKey(TEXT("LeftMouseButton")))
	{
		FColorPickerArgs Params;
		Params.bIsModal = true;
		Params.ParentWidget = SharedThis(this);
		Params.OnColorCommitted.BindSP(this, &SUWPlayerSettingsDialog::PlayerColorChanged);
		Params.InitialColorOverride = SelectedPlayerColor;
		OpenColorPicker(Params);
	}
	return FReply::Handled();
}
void SUWPlayerSettingsDialog::PlayerColorChanged(FLinearColor NewValue)
{
	SelectedPlayerColor = NewValue;
}

FReply SUWPlayerSettingsDialog::OKClick()
{
	UUTGameUserSettings* Settings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	// name
	GetPlayerOwner()->ViewportClient->ConsoleCommand(*FString::Printf(TEXT("setname %s%s"), *PlayerName->GetText().ToString()));
	GetPlayerOwner()->SetNickname(PlayerName->GetText().ToString());
	// auto weapon switch, weapon/view bob
	for (TObjectIterator<AUTPlayerController> It(RF_NoFlags); It; ++It)
	{
		It->bAutoWeaponSwitch = AutoWeaponSwitch->IsChecked();
		It->WeaponBobGlobalScaling = WeaponBobScaling->GetValue() * BOB_SCALING_FACTOR;
		It->EyeOffsetGlobalScaling = ViewBobScaling->GetValue() * BOB_SCALING_FACTOR;
		It->FFAPlayerColor = SelectedPlayerColor;
	}
	AUTPlayerController::StaticClass()->GetDefaultObject()->SaveConfig();
	// call to characters to apply FFA color change
	if (GetPlayerOwner()->PlayerController != NULL)
	{
		for (FConstPawnIterator It = GetPlayerOwner()->PlayerController->GetWorld()->GetPawnIterator(); It; ++It)
		{
			AUTCharacter* C = Cast<AUTCharacter>(*It);
			if (C != NULL)
			{
				C->NotifyTeamChanged();
			}
		}
	}

	// note that the array mirrors the list widget so we can use it directly
	for (int32 i = 0; i < WeaponList.Num(); i++)
	{
		WeaponList[i]->GetDefaultObject<AUTWeapon>()->AutoSwitchPriority = float(WeaponList.Num() - i); // top of list is highest priority
		WeaponList[i]->GetDefaultObject<AUTWeapon>()->SaveConfig();
	}

	Settings->SaveSettings();
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	GetPlayerOwner()->SaveProfileSettings();

	return FReply::Handled();
}

FReply SUWPlayerSettingsDialog::CancelClick()
{
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

FReply SUWPlayerSettingsDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_OK) OKClick();
	else if (ButtonID == UTDIALOG_BUTTON_CANCEL) CancelClick();
	return FReply::Handled();
}

#endif
// Copyright 1998-2014 Epic Games, Inc. All Rights Reserved.

#include "../UnrealTournament.h"
#include "SUWPlayerSettingsDialog.h"
#include "SUWindowsStyle.h"

// scale factor for weapon/view bob sliders (i.e. configurable value between 0 and this)
static const float BOB_SCALING_FACTOR = 2.0f;

void SUWPlayerSettingsDialog::AddWeapon(const FString& WeaponPath)
{
	UClass* WeaponClass = LoadClass<AUTWeapon>(NULL, *WeaponPath, NULL, 0, NULL);
	if (WeaponClass != NULL)
	{
		WeaponList.Add(WeaponClass);
	}
}

void SUWPlayerSettingsDialog::Construct(const FArguments& InArgs)
{
	SUWDialog::Construct(SUWDialog::FArguments().PlayerOwner(InArgs._PlayerOwner));

	FVector2D ViewportSize;
	GetPlayerOwner()->ViewportClient->GetViewportSize(ViewportSize);

	UUTGameUserSettings* Settings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());

	// TODO: some way to detect all available weapon classes
	AddWeapon(TEXT("/Game/RestrictedAssets/Weapons/Enforcer/Enforcer.Enforcer_C"));
	AddWeapon(TEXT("/Game/RestrictedAssets/Weapons/Flak/BP_FlakCannon.BP_FlakCannon_C"));
	AddWeapon(TEXT("/Game/RestrictedAssets/Weapons/LinkGun/BP_LinkGun.BP_LinkGun_C"));
	AddWeapon(TEXT("/Game/RestrictedAssets/Weapons/Minigun/BP_Minigun.BP_Minigun_C"));
	AddWeapon(TEXT("/Game/RestrictedAssets/Weapons/ShockRifle/ShockRifle.ShockRifle_C"));
	AddWeapon(TEXT("/Game/RestrictedAssets/Weapons/Sniper/BP_Sniper.BP_Sniper_C"));
	AddWeapon(TEXT("/Game/UserContent/BioRifle/BP_BioRifle.BP_BioRifle_C"));
	AddWeapon(TEXT("/Game/RestrictedAssets/Weapons/RocketLauncher/BP_RocketLauncher.BP_RocketLauncher_C"));

	struct FWeaponListSort
	{
		bool operator()(UClass& A, UClass& B) const
		{
			return (A.GetDefaultObject<AUTWeapon>()->GetAutoSwitchPriority() > B.GetDefaultObject<AUTWeapon>()->GetAutoSwitchPriority());
		}
	};
	WeaponList.Sort(FWeaponListSort());

	ChildSlot
		.VAlign(VAlign_Center)
		.HAlign(HAlign_Center)
		[
			SNew(SBorder)
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			.BorderImage(SUWindowsStyle::Get().GetBrush("UWindows.Standard.MenuBar.Background"))
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
						.Text(FText::FromString(Settings->GetPlayerName()))
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
				.AutoHeight()
				.VAlign(VAlign_Bottom)
				.HAlign(HAlign_Right)
				.Padding(5.0f, 5.0f, 5.0f, 5.0f)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FMargin(10.0f, 10.0f, 10.0f, 10.0f))
					+ SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
						.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
						.Text(NSLOCTEXT("SUWMessageBox", "OKButton", "OK").ToString())
						.OnClicked(this, &SUWPlayerSettingsDialog::OKClick)
					]
					+ SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.HAlign(HAlign_Center)
						.ButtonStyle(SUWindowsStyle::Get(), "UWindows.Standard.Button")
						.ContentPadding(FMargin(5.0f, 5.0f, 5.0f, 5.0f))
						.Text(NSLOCTEXT("SUWMessageBox", "CancelButton", "Cancel").ToString())
						.OnClicked(this, &SUWPlayerSettingsDialog::CancelClick)
					]
				]
			]
		];

		WeaponPriorities->SetSelection(WeaponList[0]);
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

FReply SUWPlayerSettingsDialog::OKClick()
{
	UUTGameUserSettings* Settings = Cast<UUTGameUserSettings>(GEngine->GetGameUserSettings());
	// name
	GetPlayerOwner()->ViewportClient->ConsoleCommand(*FString::Printf(TEXT("setname %s%s"), *PlayerName->GetText().ToString()));
	Settings->SetPlayerName(PlayerName->GetText().ToString());
	// auto weapon switch, weapon/view bob
	for (TObjectIterator<AUTPlayerController> It(RF_NoFlags); It; ++It)
	{
		It->bAutoWeaponSwitch = AutoWeaponSwitch->IsChecked();
		It->WeaponBobGlobalScaling = WeaponBobScaling->GetValue() * BOB_SCALING_FACTOR;
		It->EyeOffsetGlobalScaling = ViewBobScaling->GetValue() * BOB_SCALING_FACTOR;
	}
	AUTPlayerController::StaticClass()->GetDefaultObject()->SaveConfig();

	// note that the array mirrors the list widget so we can use it directly
	for (int32 i = 0; i < WeaponList.Num(); i++)
	{
		WeaponList[i]->GetDefaultObject<AUTWeapon>()->AutoSwitchPriority = float(WeaponList.Num() - i); // top of list is highest priority
		WeaponList[i]->GetDefaultObject<AUTWeapon>()->SaveConfig();
	}

	Settings->SaveSettings();
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

FReply SUWPlayerSettingsDialog::CancelClick()
{
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

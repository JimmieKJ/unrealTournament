// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#include "../Public/UnrealTournament.h"
#include "SUWWeaponConfigDialog.h"
#include "SUWindowsStyle.h"

#if !UE_SERVER

void SUWWeaponConfigDialog::Construct(const FArguments& InArgs)
{
	SUWDialog::Construct(SUWDialog::FArguments()
		.PlayerOwner(InArgs._PlayerOwner)
		.DialogTitle(NSLOCTEXT("SUWindowsDesktop", "WeaponSettings", "Weapon Settings"))
		.DialogSize(InArgs._DialogSize)
		.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
		.DialogPosition(InArgs._DialogPosition)
		.DialogAnchorPoint(InArgs._DialogAnchorPoint)
		.ContentPadding(InArgs._ContentPadding)
		.ButtonMask(UTDIALOG_BUTTON_OK | UTDIALOG_BUTTON_CANCEL)
		.OnDialogResult(InArgs._OnDialogResult)
		);

	{
		TArray<FAssetData> AssetList;
		GetAllBlueprintAssetData(AUTWeapon::StaticClass(), AssetList);
		for (const FAssetData& Asset : AssetList)
		{
			static FName NAME_GeneratedClass(TEXT("GeneratedClass"));
			const FString* ClassPath = Asset.TagsAndValues.Find(NAME_GeneratedClass);
			if (ClassPath != NULL && !ClassPath->Contains(TEXT("/EpicInternal/"))) // exclude debug/test weapons
			{
				UClass* TestClass = LoadObject<UClass>(NULL, **ClassPath);
				if (TestClass != NULL && !TestClass->HasAnyClassFlags(CLASS_Abstract) && TestClass->IsChildOf(AUTWeapon::StaticClass()) && !TestClass->GetDefaultObject<AUTWeapon>()->bHideInMenus)
				{
					WeaponList.Add(TestClass);
				}
			}
		}
	}

	UUTProfileSettings* ProfileSettings = GetPlayerOwner()->GetProfileSettings();
	if (ProfileSettings)
	{
		for (int32 i = 0; i < WeaponList.Num(); i++)
		{
			float Pri = ProfileSettings->GetWeaponPriority(GetNameSafe(WeaponList[i]), WeaponList[i]->GetDefaultObject<AUTWeapon>()->AutoSwitchPriority);
			WeaponList[i]->GetDefaultObject<AUTWeapon>()->AutoSwitchPriority = Pri;
		}
	}

	struct FWeaponListSort
	{
		bool operator()(UClass& A, UClass& B) const
		{
			return (A.GetDefaultObject<AUTWeapon>()->AutoSwitchPriority > B.GetDefaultObject<AUTWeapon>()->AutoSwitchPriority);
		}
	};
	WeaponList.Sort(FWeaponListSort());

	WeaponHandDesc.Add(NSLOCTEXT("UT", "Right", "Right"));
	WeaponHandDesc.Add(NSLOCTEXT("UT", "Left", "Left"));
	WeaponHandDesc.Add(NSLOCTEXT("UT", "Center", "Center"));
	WeaponHandDesc.Add(NSLOCTEXT("UT", "Hidden", "Hidden"));

	WeaponHandList.Add(MakeShareable(new FText(WeaponHandDesc[HAND_Right])));
	//WeaponHandList.Add(MakeShareable(new FText(WeaponHandDesc[HAND_Left])));
	//WeaponHandList.Add(MakeShareable(new FText(WeaponHandDesc[HAND_Center])));
	WeaponHandList.Add(MakeShareable(new FText(WeaponHandDesc[HAND_Hidden])));

	TSharedPtr<FText> InitiallySelectedHand = WeaponHandList[0];
	for (TSharedPtr<FText> TestItem : WeaponHandList)
	{
		if (TestItem.Get()->EqualTo(WeaponHandDesc[GetDefault<AUTPlayerController>()->GetWeaponHand()]))
		{
			InitiallySelectedHand = TestItem;
		}
	}
	

	if (DialogContent.IsValid())
	{
		const float MessageTextPaddingX = 10.0f;
		TSharedPtr<STextBlock> MessageTextBlock;
		DialogContent->AddSlot()
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.Padding(0.0f, 10.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(SHorizontalBox)
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(750)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(NSLOCTEXT("SUWWeaponConfigDialog", "AutoWeaponSwitch", "Weapon Switch on Pickup"))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(AutoWeaponSwitch, SCheckBox)
					.Style(SUWindowsStyle::Get(), "UT.Common.CheckBox")
					.ForegroundColor(FLinearColor::White)
					.IsChecked(GetDefault<AUTPlayerController>()->bAutoWeaponSwitch ? ESlateCheckBoxState::Checked : ESlateCheckBoxState::Unchecked)
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
				.AutoWidth()
				[
					SNew(SBox)
					.WidthOverride(500)
					[
						SNew(STextBlock)
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.NormalText")
						.Text(NSLOCTEXT("SUWWeaponConfigDialog", "WeaponHand", "Weapon Hand"))
					]
				]
				+ SHorizontalBox::Slot()
				.AutoWidth()
				[
					SAssignNew(WeaponHand, SComboBox< TSharedPtr<FText> >)
					.InitiallySelectedItem(InitiallySelectedHand)
					.ComboBoxStyle(SUWindowsStyle::Get(), "UT.ComboBox")
					.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
					.OptionsSource(&WeaponHandList)
					.OnGenerateWidget(this, &SUWWeaponConfigDialog::GenerateHandListWidget)
					.OnSelectionChanged(this, &SUWWeaponConfigDialog::OnHandSelected)
					.ContentPadding(FMargin(10.0f, 0.0f, 10.0f, 0.0f))
					.Content()
					[
						SAssignNew(SelectedWeaponHand, STextBlock)
						.Text(*InitiallySelectedHand.Get())
						.TextStyle(SUWindowsStyle::Get(), "UT.Common.ButtonText.Black")
					]
				]
			]
			+ SVerticalBox::Slot()
			.Padding(0.0f, 10.0f, 0.0f, 5.0f)
			.AutoHeight()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Center)
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SUWWeaponConfigDialog", "WeaponPriorities", "Weapon Priorities"))
				.TextStyle(SUWindowsStyle::Get(),"UT.Common.BoldText")
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
					SNew(SBox)
					.WidthOverride(700)
					.HeightOverride(600)
					[
						SNew(SBorder)
						.Content()
						[
							SAssignNew(WeaponPriorities, SListView<UClass*>)
							.SelectionMode(ESelectionMode::Single)
							.ListItemsSource(&WeaponList)
							.OnGenerateRow(this, &SUWWeaponConfigDialog::GenerateWeaponListRow)
						]
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
						SNew(SBox)
						.HeightOverride(56)
						.WidthOverride(56)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
							.ContentPadding(FMargin(4.0f, 4.0f, 4.0f, 4.0f))
							.OnClicked(this, &SUWWeaponConfigDialog::WeaponPriorityUp)
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.UpArrow"))
							]
						]
					]
					+ SVerticalBox::Slot()
					.Padding(0.0f, 10.0f, 0.0f, 10.0f)
					.AutoHeight()
					.VAlign(VAlign_Center)
					.HAlign(HAlign_Center)
					[
						SNew(SBox)
						.HeightOverride(56)
						.WidthOverride(56)
						[
							SNew(SButton)
							.HAlign(HAlign_Center)
							.ButtonStyle(SUWindowsStyle::Get(), "UT.Button.White")
							.ContentPadding(FMargin(4.0f, 4.0f, 4.0f, 4.0f))
							.OnClicked(this, &SUWWeaponConfigDialog::WeaponPriorityDown)
							[
								SNew(SImage)
								.Image(SUWindowsStyle::Get().GetBrush("UT.Icon.DownArrow"))
							]

						]
					]
				]
			]
		];

		WeaponPriorities->SetSelection(WeaponList[0]);
	}
}

TSharedRef<ITableRow> SUWWeaponConfigDialog::GenerateWeaponListRow(UClass* WeaponType, const TSharedRef<STableViewBase>& OwningList)
{
	checkSlow(WeaponType->IsChildOf(AUTWeapon::StaticClass()));

	return SNew(STableRow<UClass*>, OwningList)
		.Padding(5)
		[
			SNew(STextBlock)
			.Text(WeaponType->GetDefaultObject<AUTWeapon>()->DisplayName)
			.TextStyle(SUWindowsStyle::Get(),"UT.Common.NormalText")
		];
}

TSharedRef<SWidget> SUWWeaponConfigDialog::GenerateHandListWidget(TSharedPtr<FText> InItem)
{
	return SNew(SBox)
		.Padding(5)
		[
			SNew(STextBlock)
			.Text(*InItem.Get())
			.TextStyle(SUWindowsStyle::Get(), "UT.ContextMenu.TextStyle")
		];
}

void SUWWeaponConfigDialog::OnHandSelected(TSharedPtr<FText> NewSelection, ESelectInfo::Type SelectInfo)
{
	SelectedWeaponHand->SetText(*NewSelection.Get());
}

FReply SUWWeaponConfigDialog::WeaponPriorityUp()
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
FReply SUWWeaponConfigDialog::WeaponPriorityDown()
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

FReply SUWWeaponConfigDialog::OKClick()
{
	EWeaponHand NewHand = HAND_Right;
	for (int32 i = 0; i < WeaponHandDesc.Num(); i++)
	{
		if (WeaponHandDesc[i].EqualTo(*WeaponHand->GetSelectedItem().Get()))
		{
			NewHand = EWeaponHand(i);
			break;
		}
	}

	AUTPlayerController* UTPlayerController = Cast<AUTPlayerController>(GetPlayerOwner()->PlayerController);
	if (UTPlayerController != NULL)
	{
		UTPlayerController->bAutoWeaponSwitch = AutoWeaponSwitch->IsChecked();
		
		UTPlayerController->SetWeaponHand(NewHand);
		UTPlayerController->SaveConfig();
	}
	else
	{
		AUTPlayerController* DefaultPC = AUTPlayerController::StaticClass()->GetDefaultObject<AUTPlayerController>();
		DefaultPC->bAutoWeaponSwitch = AutoWeaponSwitch->IsChecked();
		DefaultPC->SetWeaponHand(NewHand);
		DefaultPC->SaveConfig();
	}

	UUTProfileSettings* ProfileSettings = GetPlayerOwner()->GetProfileSettings();

	// note that the array mirrors the list widget so we can use it directly
	for (int32 i = 0; i < WeaponList.Num(); i++)
	{
		float NewPriority = float(WeaponList.Num() - i); // top of list is highest priority
		WeaponList[i]->GetDefaultObject<AUTWeapon>()->AutoSwitchPriority = NewPriority;
		WeaponList[i]->GetDefaultObject<AUTWeapon>()->SaveConfig();
		// update instances so changes take effect immediately
		if (UTPlayerController != NULL)
		{
			for (TActorIterator<AUTWeapon> It(UTPlayerController->GetWorld()); It; ++It)
			{
				if (It->GetClass() == WeaponList[i])
				{
					It->AutoSwitchPriority = NewPriority;
				}
			}
		}
		if (ProfileSettings != NULL)
		{
			ProfileSettings->SetWeaponPriority(GetNameSafe(WeaponList[i]), WeaponList[i]->GetDefaultObject<AUTWeapon>()->AutoSwitchPriority);
		}
	}

	GetPlayerOwner()->SaveProfileSettings();
	GetPlayerOwner()->CloseDialog(SharedThis(this));

	return FReply::Handled();
}

FReply SUWWeaponConfigDialog::CancelClick()
{
	GetPlayerOwner()->CloseDialog(SharedThis(this));
	return FReply::Handled();
}

FReply SUWWeaponConfigDialog::OnButtonClick(uint16 ButtonID)
{
	if (ButtonID == UTDIALOG_BUTTON_OK)
	{
		OKClick();
	}
	else if (ButtonID == UTDIALOG_BUTTON_CANCEL)
	{
		CancelClick();
	}
	return FReply::Handled();
}

#endif
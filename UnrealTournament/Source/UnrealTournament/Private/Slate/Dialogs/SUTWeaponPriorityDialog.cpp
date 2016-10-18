// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealTournament.h"
#include "UTLocalPlayer.h"
#include "SUTWeaponPriorityDialog.h"
#include "../Widgets/SUTButton.h"
#include "../SUTStyle.h"
#include "UTPlayerInput.h"
#include "Scalability.h"
#include "UTWorldSettings.h"
#include "UTGameEngine.h"
#include "../SUTUtils.h"
#include "../Widgets/SUTBorder.h"

#if !UE_SERVER


void SUTWeaponPriorityDialog::Construct(const FArguments& InArgs)
{
	SUTDialogBase::Construct(SUTDialogBase::FArguments()
							.PlayerOwner(InArgs._PlayerOwner)
							.DialogTitle(InArgs._DialogTitle)
							.DialogSize(InArgs._DialogSize)
							.bDialogSizeIsRelative(InArgs._bDialogSizeIsRelative)
							.DialogPosition(InArgs._DialogPosition)
							.DialogAnchorPoint(InArgs._DialogAnchorPoint)
							.ContentPadding(InArgs._ContentPadding)
							.ButtonMask(InArgs._ButtonMask)
							.bShadow(true)
							.IsScrollable(false)
							.OnDialogResult(InArgs._OnDialogResult)
						);

	if (DialogContent.IsValid())
	{

		DialogContent->AddSlot()
		[

			SNew(SVerticalBox)
			+SVerticalBox::Slot()
			.Padding(FMargin(10.0f, 10.0f, 10.0f, 10.0f))
			.HAlign(HAlign_Center)
			.AutoHeight()
			[
				SNew(STextBlock)
				.Text(NSLOCTEXT("SUTWeaponPriorityDialog","Instructions","Use the movement buttons to configure the auto-switch priorities for all of your weapons.  Weapons higher in the list have higher priority."))
				.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Medium.Bold")
				.AutoWrapText(true)
			]
			+SVerticalBox::Slot()
			.FillHeight(1.0)
			[
				SNew(SUTBorder)
				.BorderImage(SUTStyle::Get().GetBrush("UT.HeaderBackground.Dark"))
				[
					SAssignNew(WeaponListView, SListView<TSharedPtr<FWeaponListEntry>>)
					.SelectionMode(ESelectionMode::Single)
					.ListItemsSource(&WeaponList)
					.ItemHeight(96)
					.OnGenerateRow(this, &SUTWeaponPriorityDialog::GenerateWeaponListRow)
					.OnSelectionChanged(this, &SUTWeaponPriorityDialog::OnWeaponListSelectionChanged)
				]
			]
		];
	}

}

void SUTWeaponPriorityDialog::InitializeList(TArray<FWeaponInfo>& AllWeapons)
{
	for (int32 i=0; i < AllWeapons.Num(); i++)
	{
		if (AllWeapons[i].WeaponDefaultObject.IsValid())
		{
			bool bAdd = true;
			FWeaponInfo* WeaponInfoPtr = &(AllWeapons[i]);

			UE_LOG(UT,Log,TEXT("IL: %s %f"),*GetNameSafe(AllWeapons[i].WeaponClass), AllWeapons[i].WeaponCustomizationInfo->WeaponAutoSwitchPriority)
			if (WeaponList.Num() > 0)
			{
				// Insert Sort to find it's place.
				for (int32 j = 0; j < WeaponList.Num(); j++)
				{
					if (WeaponList[j]->WeaponInfoPtr->WeaponCustomizationInfo->WeaponAutoSwitchPriority < AllWeapons[i].WeaponCustomizationInfo->WeaponAutoSwitchPriority)
					{
						WeaponList.Insert(FWeaponListEntry::Make(AllWeapons[i].WeaponDefaultObject, WeaponInfoPtr), j);
						bAdd = false;
						break;
					}
				}
			}

			if (bAdd)
			{
				WeaponList.Add(FWeaponListEntry::Make(AllWeapons[i].WeaponDefaultObject, WeaponInfoPtr));
			}
		}
	}
}


TSharedRef<ITableRow> SUTWeaponPriorityDialog::GenerateWeaponListRow(TSharedPtr<FWeaponListEntry> Item, const TSharedRef<STableViewBase>& OwningList)
{
	return SNew(STableRow<TSharedPtr<FWeaponListEntry>>, OwningList)
		.Padding(5)
		.Style(SUTStyle::Get(),"UT.PlayerList.Row")
		[
			SNew(SVerticalBox)
			+SVerticalBox::Slot().AutoHeight()
			[
				SNew(SHorizontalBox)
				+SHorizontalBox::Slot().FillWidth(1.0).VAlign(VAlign_Center)
				[
					SNew(STextBlock)
					.Text(Item->Weapon->DisplayName)
					.TextStyle(SUTStyle::Get(), "UT.Font.NormalText.Large")
				]
				+SHorizontalBox::Slot().Padding(5.0f,0.0f,0.0f,0.0f).AutoWidth()
				[
					SAssignNew(Item->ButtonBox, SVerticalBox)
					.Visibility(EVisibility::Collapsed)
					+SVerticalBox::Slot().AutoHeight().VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot().VAlign(VAlign_Center)
						.AutoWidth()
						[
							SNew(SBox).HeightOverride(48).WidthOverride(48)
							[
								SNew(SUTButton)
								.ButtonStyle(SUTStyle::Get(), "UT.WeaponConfig.Button")
								.OnClicked(this, &SUTWeaponPriorityDialog::MoveUpClicked,Item)
								.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTWeaponPriorityDialog","UpPriorityTT","Give this weapon a higher priority.")))
								[
									SNew(SImage)
									.Image(SUTStyle::Get().GetBrush("UT.Icon.SortUp.Big"))
								]
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox).HeightOverride(48).WidthOverride(48)
							[
								SNew(SUTButton)
								.ButtonStyle(SUTStyle::Get(), "UT.WeaponConfig.Button")
								.OnClicked(this, &SUTWeaponPriorityDialog::MoveTopClicked, Item)
								.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTWeaponPriorityDialog", "TopPriorityTT", "Give this weapon the highest priority.")))
								[
									SNew(SImage)
									.Image(SUTStyle::Get().GetBrush("UT.Icon.SortTop.Big"))
								]
							]
						]

					]
					+ SVerticalBox::Slot().AutoHeight()
					[
						SNew(SHorizontalBox)
						+SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox).HeightOverride(48).WidthOverride(48)
							[
								SNew(SUTButton)
								.ButtonStyle(SUTStyle::Get(), "UT.WeaponConfig.Button")
								.OnClicked(this, &SUTWeaponPriorityDialog::MoveDownClicked, Item)
								.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTWeaponPriorityDialog", "DownPriorityTT", "Give this weapon a lower priority.")))
								[
									SNew(SImage)
									.Image(SUTStyle::Get().GetBrush("UT.Icon.SortDown.Big"))
								]
							]
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						[
							SNew(SBox).HeightOverride(48).WidthOverride(48)
							[
								SNew(SUTButton)
								.ButtonStyle(SUTStyle::Get(), "UT.WeaponConfig.Button")
								.OnClicked(this, &SUTWeaponPriorityDialog::MoveBottomClicked, Item)
								.ToolTip(SUTUtils::CreateTooltip(NSLOCTEXT("SUTWeaponPriorityDialog", "BottomPriorityTT", "Give this weapon the lowest priority.")))
								[
									SNew(SImage)
									.Image(SUTStyle::Get().GetBrush("UT.Icon.SortBottom.Big"))
								]
							]
						]
					]
				]
			]

		];
}

FReply SUTWeaponPriorityDialog::MoveUpClicked(TSharedPtr<FWeaponListEntry> Item)
{
	int32 CurrentIndex = WeaponList.Find(Item);
	if (CurrentIndex != INDEX_NONE && CurrentIndex > 0)
	{
		WeaponList.Swap(CurrentIndex, CurrentIndex-1);
		WeaponListView->RequestListRefresh();
	}

	return FReply::Handled();
}


FReply SUTWeaponPriorityDialog::MoveTopClicked(TSharedPtr<FWeaponListEntry> Item)
{
	WeaponList.Remove(Item);
	WeaponList.Insert(Item, 0);
	WeaponListView->RequestListRefresh();
	WeaponListView->ScrollToTop();
	return FReply::Handled();
}

FReply SUTWeaponPriorityDialog::MoveDownClicked(TSharedPtr<FWeaponListEntry> Item)
{
	int32 CurrentIndex = WeaponList.Find(Item);
	if (CurrentIndex != INDEX_NONE && CurrentIndex < WeaponList.Num()-1)
	{
		WeaponList.Swap(CurrentIndex, CurrentIndex + 1);
		WeaponListView->RequestListRefresh();
	}


	return FReply::Handled();
}


FReply SUTWeaponPriorityDialog::MoveBottomClicked(TSharedPtr<FWeaponListEntry> Item)
{
	WeaponList.Remove(Item);
	WeaponList.Add(Item);
	WeaponListView->RequestListRefresh();
	WeaponListView->ScrollToBottom();
	return FReply::Handled();
}


void SUTWeaponPriorityDialog::OnWeaponListSelectionChanged(TSharedPtr<FWeaponListEntry> Item, ESelectInfo::Type SelectInfo)
{
	for (int32 i = 0; i < WeaponList.Num(); i++)
	{
		if (WeaponList[i]->ButtonBox.IsValid())
		{
			WeaponList[i]->ButtonBox->SetVisibility(EVisibility::Collapsed);
		}
	}

	Item->ButtonBox->SetVisibility(EVisibility::Visible);
}

float SUTWeaponPriorityDialog::GetPriorityFor(TWeakObjectPtr<AUTWeapon> Weapon)
{
	for (int32 i = 0; i < WeaponList.Num(); i++)
	{
		if (WeaponList[i]->Weapon.Get() == Weapon.Get())
		{
			return 1000.0f - (1000.0f * (float(i) / float(WeaponList.Num())));
		}
	}
	return -1.0f;
}


#endif
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "Slate/SlateGameResources.h"
#include "SUWindowsDesktop.h"

#if !UE_SERVER

class FLoadoutData 
{
public:
	TWeakObjectPtr<AUTReplicatedLoadoutInfo> LoadoutInfo;
	TWeakObjectPtr<AUTWeapon> DefaultWeaponObject;
	TSharedPtr<FSlateDynamicImageBrush> WeaponImage;

	FLoadoutData(TWeakObjectPtr<AUTReplicatedLoadoutInfo> inLoadoutInfo)
		: LoadoutInfo(inLoadoutInfo)
	{
		if (LoadoutInfo.IsValid())
		{
			DefaultWeaponObject = LoadoutInfo->WeaponClass->GetDefaultObject<AUTWeapon>();
			if (DefaultWeaponObject.IsValid() && DefaultWeaponObject->MenuGraphic)
			{
				FString ImageName = DefaultWeaponObject->GetName() + TEXT("_Img");

				WeaponImage = MakeShareable(new FSlateDynamicImageBrush(DefaultWeaponObject->MenuGraphic, FVector2D(256.0,128.0),FName(*ImageName)));
			}
		}
	};

	static TSharedRef<FLoadoutData> Make(TWeakObjectPtr<AUTReplicatedLoadoutInfo> inLoadoutInfo)
	{
		return MakeShareable( new FLoadoutData( inLoadoutInfo) );
	}
};


class SUTLoadoutMenu : public SUWindowsDesktop
{
protected:
	virtual void CreateDesktop();
	void CollectWeapons();
	TSharedRef<ITableRow> GenerateAvailableLoadoutInfoListWidget( TSharedPtr<FLoadoutData> InItem, const TSharedRef<STableViewBase>& OwnerTable );
	TSharedRef<ITableRow> GenerateSelectedLoadoutInfoListWidget( TSharedPtr<FLoadoutData> InItem, const TSharedRef<STableViewBase>& OwnerTable );

	TSharedPtr<SListView<TSharedPtr<FLoadoutData>>> AvailableWeaponList;
	TSharedPtr<SListView<TSharedPtr<FLoadoutData>>> SelectedWeaponList;

	TSharedPtr<FLoadoutData> CurrentWeapon;

	TArray<TSharedPtr<FLoadoutData>> AvailableWeapons;
	TArray<TSharedPtr<FLoadoutData>> SelectedWeapons;

	FReply OnAddClicked();
	FReply OnRemovedClicked();

	float TotalCostOfCurrentLoadout;
	void TallyLoadout();

	FText GetLoadoutTitle() const;

	void AvailableWeaponChanged(TSharedPtr<FLoadoutData> NewSelection, ESelectInfo::Type SelectInfo);
	void SelectedWeaponChanged(TSharedPtr<FLoadoutData> NewSelection, ESelectInfo::Type SelectInfo);

	const FSlateBrush* SUTLoadoutMenu::GetDescriptionImage() const;
	FText GetDescriptionText() const;

	FReply OnAcceptClicked();
	bool OnAcceptEnabled() const;
	FReply OnCancelledClicked();
};

#endif

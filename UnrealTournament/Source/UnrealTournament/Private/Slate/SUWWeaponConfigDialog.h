// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "SUWDialog.h"

#if !UE_SERVER
class UNREALTOURNAMENT_API SUWWeaponConfigDialog : public SUWDialog, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SUWWeaponConfigDialog)
		: _DialogSize(FVector2D(1000,1000))
		, _bDialogSizeIsRelative(false)
		, _DialogPosition(FVector2D(0.5f, 0.5f))
		, _DialogAnchorPoint(FVector2D(0.5f, 0.5f))
		, _ContentPadding(FVector2D(10.0f, 5.0f))
	{}
	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)
		SLATE_ARGUMENT(FVector2D, DialogSize)
		SLATE_ARGUMENT(bool, bDialogSizeIsRelative)
		SLATE_ARGUMENT(FVector2D, DialogPosition)
		SLATE_ARGUMENT(FVector2D, DialogAnchorPoint)
		SLATE_ARGUMENT(FVector2D, ContentPadding)
		SLATE_EVENT(FDialogResultDelegate, OnDialogResult)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

	TSharedPtr<SCheckBox> AutoWeaponSwitch;
	TArray<UClass*> WeaponList;
	TSharedPtr< SListView<UClass*> > WeaponPriorities;

	TArray< TSharedPtr<FText> > WeaponHandList;
	TArray<FText> WeaponHandDesc;
	TSharedPtr< SComboBox< TSharedPtr<FText> > > WeaponHand;
	TSharedPtr<STextBlock> SelectedWeaponHand;

	TSharedPtr<class SUTTabWidget> TabWidget;

	TSharedRef<ITableRow> GenerateWeaponListRow(UClass* WeaponType, const TSharedRef<STableViewBase>& OwningList);
	TSharedRef<SWidget> GenerateHandListWidget(TSharedPtr<FText> InItem);
	void OnHandSelected(TSharedPtr<FText> NewSelection, ESelectInfo::Type SelectInfo);
	FReply WeaponPriorityUp();
	FReply WeaponPriorityDown();


	/** render target for the WYSIWYG crosshair */
	class UUTCanvasRenderTarget2D* CrosshairPreviewTexture;
	/** material for the crosshair preview since Slate doesn't support rendering the target directly */
	class UMaterialInstanceDynamic* CrosshairPreviewMID;
	/** Slate brush to render the preview */
	FSlateBrush* CrosshairPreviewBrush;

	virtual void UpdateCrosshairRender(UCanvas* C, int32 Width, int32 Height);

	bool bCustomWeaponCrosshairs;
	ECheckBoxState GetCustomWeaponCrosshairs() const;
	void SetCustomWeaponCrosshairs(ECheckBoxState NewState);

	TSharedPtr<SImage> CrosshairImage;
	TSharedPtr<class SUTColorPicker> ColorPicker;
	TSharedPtr<STextBlock> CrosshairText;

	TSharedPtr<FCrosshairInfo> SelectedCrosshairInfo;
	TArray<TSharedPtr<FCrosshairInfo> > CrosshairInfos;
	TSharedPtr< SListView<TSharedPtr<FCrosshairInfo> > > CrosshairInfosList;
	void OnCrosshairInfoSelected(TSharedPtr<FCrosshairInfo> NewSelection, ESelectInfo::Type SelectInfo);
	TSharedRef<ITableRow> GenerateCrosshairListRow(TSharedPtr<FCrosshairInfo> CrosshairInfo, const TSharedRef<STableViewBase>& OwningList);

	TMap<FString, UClass*> WeaponMap;
	TMap<FString, UClass*> CrosshairMap;

	TArray<UClass*> CrosshairList;
	TArray<TWeakObjectPtr<UClass> > WeakCrosshairList;
	TSharedPtr<SComboBox< TWeakObjectPtr<UClass>  > > CrosshairComboBox;
	void OnCrosshairSelected(TWeakObjectPtr<UClass> NewSelection, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> GenerateCrosshairRow(TWeakObjectPtr<UClass>  CrosshairClass);

	TOptional<float> GetCrosshairScale() const
	{
		return SelectedCrosshairInfo.IsValid() ? TOptional<float>(SelectedCrosshairInfo->Scale) : TOptional<float>(1.0f);
	}

	void SetCrosshairScale(float InScale)
	{
		if (SelectedCrosshairInfo.IsValid())
		{
			SelectedCrosshairInfo->Scale = InScale;
		}
	}

	FLinearColor GetCrosshairColor() const
	{
		return SelectedCrosshairInfo.IsValid() ? SelectedCrosshairInfo->Color : FLinearColor::White;
	}

	void SetCrosshairColor(FLinearColor NewColor)
	{
		if (SelectedCrosshairInfo.IsValid())
		{
			SelectedCrosshairInfo->Color = NewColor;
		}
	}

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual FReply OnButtonClick(uint16 ButtonID);
	FReply OKClick();
	FReply CancelClick();

	bool CanMoveWeaponPriorityUp() const;
	bool CanMoveWeaponPriorityDown() const;
};
#endif
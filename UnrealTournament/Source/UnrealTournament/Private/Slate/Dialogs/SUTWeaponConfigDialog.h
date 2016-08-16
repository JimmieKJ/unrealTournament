// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "../Base/SUTDialogBase.h"
#include "UTDroppedPickup.h"

#if !UE_SERVER

class SUTButton;
class UTCrosshair;
class SUTWeaponPriorityDialog;
class SUTWeaponWheelConfigDialog;

struct FWeaponInfo
{
	UClass* WeaponClass;
	TWeakObjectPtr<AUTWeapon> WeaponDefaultObject;
	FSlateDynamicImageBrush* WeaponIconBrush;
	FWeaponCustomizationInfo* WeaponCustomizationInfo;
	TSharedPtr<SUTButton> WeaponButton;
	FString WeaponSkinClassname;

	FWeaponInfo()
	{
		WeaponClass = nullptr;
		WeaponDefaultObject.Reset();
		WeaponIconBrush = nullptr;
		WeaponSkinClassname = TEXT("");
	}

	FWeaponInfo(UClass* inClass, AUTWeapon* inWeapon)
	{
		WeaponClass = inClass;
		WeaponDefaultObject = inWeapon;

		FVector2D WeaponIconSize = FVector2D(WeaponDefaultObject->MenuGraphic->GetSizeX(), WeaponDefaultObject->MenuGraphic->GetSizeY());
		WeaponIconBrush = new FSlateDynamicImageBrush(WeaponDefaultObject->MenuGraphic, WeaponIconSize, NAME_None);
	}
};

class UNREALTOURNAMENT_API SUTWeaponConfigDialog : public SUTDialogBase, public FGCObject
{
public:
	SLATE_BEGIN_ARGS(SUTWeaponConfigDialog)
		: _DialogSize(FVector2D(1920.0,1060.0))
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

	virtual ~SUTWeaponConfigDialog();
	void Construct(const FArguments& InArgs);
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;
	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	virtual TSharedRef<class SWidget> BuildCustomButtonBar();
	virtual FReply OnButtonClick(uint16 ButtonID) override;

protected:
	TSharedPtr<SButton> WeaponWheelConfigButton;
	TSharedPtr<SButton> WeaponPriorityConfigButton;

	TSharedPtr<SUTButton> VariantsButton;
	TSharedPtr<SUTButton> CrosshairButton;

protected:

	// The 3d world preview

	class UWorld* PreviewWorld;
	FSceneViewStateReference PreviewViewState;
	class UUTCanvasRenderTarget2D* PreviewTexture;
	class UMaterialInstanceDynamic* PreviewMID;
	FSlateBrush* PreviewBrush;
	AActor* PreviewEnvironment;

	FRotator PreviewActorRotation;
	FVector PreviewCameraLocation;
	FVector DefaultPreviewCameraLocation;
	FVector DesiredPreviewCameraLocation;

	// Holds the offset to add to the camera for the selected variant
	FVector PreviewCameraOffset;

	// Holds a second offset that is applied to the camera to allow for some fine tune movement
	FVector PreviewCameraFreeLookOffset;

	float TimeSinceLastCameraAdjustment;

	virtual void ConstructPreviewWorld(FVector2D ViewportSize);
	virtual void UpdatePreviewRender(UCanvas* C, int32 Width, int32 Height);

	int32 CurrentWeaponIndex;

	TArray<AUTDroppedPickup*> PickupPreviewActors;
	TArray<AUTWeapon*> WeaponPreviewActors;
	TArray<UUTWeaponSkin*> PreviewWeaponSkins;

	void RecreateWeaponPreview();

	void DragPreview(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	void ZoomPreview(float WheelDelta);

	int32 CurrentWeaponPreviewIndex;
	
	float AnimStartY;
	float AnimEndY;
	float AnimTimer;

	bool bAnimating;


protected:

	TArray<FWeaponCustomizationInfo*> DeleteList;

	// All of the weapon specifics

	TArray<FWeaponInfo> AllWeapons;

	void GatherWeaponData(UUTProfileSettings* ProfileSettings);

	bool bShowingCrosshairs;
	TSharedPtr<SVerticalBox> Page;
	void GeneratePage();

	FReply OnModeChanged(int32 NewMode);

protected:

	// Weapon Hand

	TArray< TSharedPtr<FText> > WeaponHandList;
	TArray<FText> WeaponHandDesc;
	TSharedPtr< SComboBox< TSharedPtr<FText> > > WeaponHand;
	TSharedPtr<STextBlock> SelectedWeaponHand;

	TSharedRef<SWidget> GenerateWeaponHand(UUTProfileSettings* Profile);
	TSharedRef<SWidget> GenerateHandListWidget(TSharedPtr<FText> InItem);
	void OnHandSelected(TSharedPtr<FText> NewSelection, ESelectInfo::Type SelectInfo);

protected:

	// Auto-Weapon Switch

	TSharedPtr<SCheckBox> AutoWeaponSwitch;
	TSharedRef<SWidget> GenerateAutoSwitch(UUTProfileSettings* Profile);


protected:

	// Custom Crosshairs

	bool bCustomWeaponCrosshairs;

	TSharedPtr<SCheckBox> CustomWeaponCrosshairs;
	TSharedRef<SWidget> GenerateCustomWeaponCrosshairs(UUTProfileSettings* Profile);
	ECheckBoxState GetCustomWeaponCrosshairs() const;
	void SetCustomWeaponCrosshairs(ECheckBoxState NewState);


protected:

	// Weapon Groups
	
	TArray<TSharedPtr<SUTButton>> WeaponGroupButtons;
	TSharedPtr<SHorizontalBox> WeaponGroupBox;
	void GenerateWeaponGroups();
	void UpdateWeaponGroups(int32 NewWeaponGroup);
	FReply WeaponGroupClicked(int32 NewWeaponGroup);

protected:

	// Weapon List

	TSharedPtr<SScrollBox> WeaponScrollBox;
	FReply WeaponClicked(int32 NewWeaponIndex);
	void GenerateWeaponList(UClass* DesiredSelectedWeaponClass);

protected:

	// Weapon Skins
	TArray<UUTWeaponSkin*> WeaponSkinGCList;
	TMap< FName, TArray<UUTWeaponSkin*> > AllWeaponSkins;
	TArray<UUTWeaponSkin*>* CurrentWeaponSkinArray;
	void GatherWeaponSkins(UUTProfileSettings* ProfileSettings);
	FText GetVariantTitle() const;
	FText GetVariantCount() const;

	FReply HandleNextPrevious(int32 Step);
	EVisibility LeftArrowVis() const;
	EVisibility RightArrowVis() const;
	
	EVisibility VarientSelectVis() const;


	FText GetVarientText() const;
	FText GetVarientSetText() const;

	// returns true if this varient is the currently selected variant
	bool IsCurrentSkin() const;
	FReply SetWeaponSkin();

protected:

	// Crosshairs

	int32 CurrentCrosshairIndex;
	TArray<UUTCrosshair*> AllCrosshairs;
	void GatherCrosshairs(UUTProfileSettings* ProfileSettings);

	TSharedPtr<class SUTColorPicker> ColorPicker;

	TOptional<float> GetCrosshairScale() const;
	void SetCrosshairScale(float InScale);
	FLinearColor GetCrosshairColor() const;
	void SetCrosshairColor(FLinearColor NewColor);


protected:
	FReply SetAutoSwitchPriorities();
	TSharedPtr<SUTWeaponPriorityDialog> AutoSwitchDialog;
	void AutoSwitchDialogClosed(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);

protected:

	TSharedPtr<SUTWeaponWheelConfigDialog> WheelConfigDialog;
	FReply SUTWeaponConfigDialog::OnConfigureWheelClick();
	void WheelConfigDialogClosed(TSharedPtr<SCompoundWidget> Widget, uint16 ButtonID);

	bool bRequiresSave;


};
#endif
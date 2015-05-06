// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "SUWDialog.h"

#if !UE_SERVER
class UNREALTOURNAMENT_API SUWPlayerInfoDialog : public SUWDialog, public FGCObject
{
public:

	SLATE_BEGIN_ARGS(SUWPlayerInfoDialog)
	: _DialogSize(FVector2D(960,700))
	, _bDialogSizeIsRelative(false)
	, _DialogPosition(FVector2D(0.5f,0.5f))
	, _DialogAnchorPoint(FVector2D(0.5f,0.5f))
	, _ContentPadding(FVector2D(10.0f, 5.0f))
	{}
	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)												
	SLATE_ARGUMENT(TWeakObjectPtr<class AUTPlayerState>, TargetPlayerState)
	SLATE_ARGUMENT(FVector2D, DialogSize)										
	SLATE_ARGUMENT(bool, bDialogSizeIsRelative)									
	SLATE_ARGUMENT(FVector2D, DialogPosition)									
	SLATE_ARGUMENT(FVector2D, DialogAnchorPoint)								
	SLATE_ARGUMENT(FVector2D, ContentPadding)									
	SLATE_EVENT(FDialogResultDelegate, OnDialogResult)							
	SLATE_END_ARGS()


	void Construct(const FArguments& InArgs);
	virtual ~SUWPlayerInfoDialog();

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

protected:

	TWeakObjectPtr<class AUTPlayerState> TargetPlayerState;
	TSharedPtr<FUniqueNetId> TargetUniqueId;

	/** world for rendering the player preview */
	class UWorld* PlayerPreviewWorld;
	/** view state for player preview (needed for LastRenderTime to work) */
	FSceneViewStateReference ViewState;
	/** preview actors */
	class AUTCharacter* PlayerPreviewMesh;
	/** preview weapon */
	AUTWeaponAttachment* PreviewWeapon;
	/** render target for player mesh and cosmetic items */
	class UUTCanvasRenderTarget2D* PlayerPreviewTexture;
	/** material for the player preview since Slate doesn't support rendering the target directly */
	class UMaterialInstanceDynamic* PlayerPreviewMID;
	/** Slate brush to render the preview */
	FSlateBrush* PlayerPreviewBrush;
	/** Do you want the player model to spin? */
	bool bSpinPlayer;
	/** The Zoom offset to apply to the camera. */
	float ZoomOffset;

	AActor* PreviewEnvironment;
	UAnimationAsset* PoseAnimation;


	virtual TSharedRef<class SWidget> BuildCustomButtonBar();
	virtual FReply OnButtonClick(uint16 ButtonID);

	virtual void DragPlayerPreview(FVector2D MouseDelta);
	virtual void ZoomPlayerPreview(float WheelDelta);
	virtual void RecreatePlayerPreview();
	virtual void UpdatePlayerRender(UCanvas* C, int32 Width, int32 Height);
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	// Friends...

	TSharedPtr<class SVerticalBox> InfoPanel;
	TSharedPtr<class SHorizontalBox> FriendPanel;
	TSharedPtr<class SButton> KickButton;
	FName FriendStatus;

	FText GetFunnyText();
	virtual void BuildFriendPanel();
	virtual FReply OnSendFriendRequest();

	virtual FReply KickVote();

};
#endif
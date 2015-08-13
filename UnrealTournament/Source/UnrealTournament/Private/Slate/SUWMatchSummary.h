// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "SUWDialog.h"
#include "UTFlagInfo.h"

enum EViewMode
{
	VM_Team,
	VM_Player,
	VM_All,
};

enum ECameraState
{
	CS_FreeCam,
	CS_CamIntro,
	CS_CamAuto,
};

#if !UE_SERVER
class UNREALTOURNAMENT_API SUWMatchSummary : public SCompoundWidget, public FGCObject
{
public:

	SLATE_BEGIN_ARGS(SUWMatchSummary)
	: _DialogSize(FVector2D(1000,900))
	, _bDialogSizeIsRelative(false)
	, _DialogPosition(FVector2D(0.5f,0.5f))
	, _DialogAnchorPoint(FVector2D(0.5f,0.5f))
	, _ContentPadding(FVector2D(10.0f, 5.0f))
	{}
	SLATE_ARGUMENT(TWeakObjectPtr<class UUTLocalPlayer>, PlayerOwner)	
	SLATE_ARGUMENT(TWeakObjectPtr<class AUTGameState>, GameState)	
	SLATE_ARGUMENT(FVector2D, DialogSize)										
	SLATE_ARGUMENT(bool, bDialogSizeIsRelative)									
	SLATE_ARGUMENT(FVector2D, DialogPosition)									
	SLATE_ARGUMENT(FVector2D, DialogAnchorPoint)								
	SLATE_ARGUMENT(FVector2D, ContentPadding)									
	SLATE_EVENT(FDialogResultDelegate, OnDialogResult)							
	SLATE_END_ARGS()


	void Construct(const FArguments& InArgs);
	virtual ~SUWMatchSummary();

	virtual void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override;

	inline TWeakObjectPtr<class UUTLocalPlayer> GetPlayerOwner()
	{
		return PlayerOwner;
	}

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyUp(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override;
	virtual FReply OnKeyChar(const FGeometry& InGeometry, const FCharacterEvent& InCharacterEvent) override;
	virtual void PlayTauntByIndex(AUTPlayerState* PS, int32 TauntIndex);
	virtual void SelectPlayerState(AUTPlayerState* PS);

protected:

	/** world for rendering the player preview */
	class UWorld* PlayerPreviewWorld;
	/** view state for player preview (needed for LastRenderTime to work) */
	FSceneViewStateReference ViewState;

	UClass* PlayerPreviewAnimBlueprint;

	/** render target for player mesh and cosmetic items */
	class UUTCanvasRenderTarget2D* PlayerPreviewTexture;
	/** material for the player preview since Slate doesn't support rendering the target directly */
	class UMaterialInstanceDynamic* PlayerPreviewMID;
	/** Slate brush to render the preview */
	FSlateBrush* PlayerPreviewBrush;

	TSharedPtr<class SUInGameHomePanel> ChatPanel;
	TSharedPtr<class SScrollBox> ChatScroller;
	TSharedPtr<class SRichTextBlock> ChatDisplay;
	void UpdateChatText();
	int32 LastChatCount;

	TSharedPtr<class SOverlay> InfoPanel;
	TSharedPtr<class SUTTabWidget> TabWidget;
	TArray<TSharedPtr<TAttributeStat> > StatList;
	virtual void BuildInfoPanel();
	virtual void OnTabButtonSelectionChanged(const FText& NewText);

	int32 OldSSRQuality;

	virtual void SetViewMode(EViewMode NewViewMode);
	virtual void ViewCharacter(AUTCharacter* NewChar);
	virtual void ViewTeam(int32 NewTeam);
	virtual void ViewAll();
	
	EViewMode ViewMode;
	FVector2D MousePos;
	int32 ViewedTeamNum;
	TWeakObjectPtr<class AUTCharacter> HighlightedChar;
	TWeakObjectPtr<class AUTCharacter> ViewedChar;

	ECameraState CameraState;
	FTransform CameraTransform;
	FTransform DesiredCameraTransform;
	FTransform TeamStartCamera;
	FTransform TeamEndCamera;
	float TeamCamAlpha;
	bool bAutoScrollTeam;
	float AutoScrollTeamDirection;

	double IntroTime;

	virtual FReply OnSwitcherNext();
	virtual FReply OnSwitcherPrevious();
	virtual FText GetSwitcherText() const;
	virtual FSlateColor GetSwitcherColor() const;
	EVisibility GetSwitcherVisibility() const;
	EVisibility GetSwitcherButtonVisibility() const;

	FOptionalSize GetStatsWidth() const;
	mutable float StatsWidth;

	/** preview actors */
	TArray<class AUTCharacter*> PlayerPreviewMeshs;
	/**preview characters separated by team*/
	TArray<TArray<class AUTCharacter*> > TeamPreviewMeshs;
	/** preview weapon */
	TArray<class AUTWeaponAttachment*> PreviewWeapons;
	/** preview flagsFlag*/
	TArray<class ASkeletalMeshActor*> PreviewFlags;
	/**Each team is attached to an Actor for easy team tranforms*/
	TArray<class AActor*> TeamAnchors;
	AActor* PreviewEnvironment;

	TWeakObjectPtr<class AUTGameState> GameState;

	virtual void DragPlayerPreview(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual void ZoomPlayerPreview(float WheelDelta);
	virtual void OnMouseDownPlayerPreview(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);
	virtual void OnMouseMovePlayerPreview(const FGeometry& MyGeometry, const FPointerEvent& MouseEvent);

	virtual AUTCharacter* RecreatePlayerPreview(AUTPlayerState* NewPS, FVector Location, FRotator Rotation);
	virtual void RecreateAllPlayers();
	virtual class AUTWeaponAttachment* GetFavoriteWeaponAttachment(AUTPlayerState* PS);
	virtual void UpdatePlayerRender(UCanvas* C, int32 Width, int32 Height);
	virtual void UpdateIntroCam();
	virtual void UpdateAutoCam();
	virtual class AUTCharacter* FindCharacter(class AUTPlayerState* PS);

	class UUTScoreboard* GetScoreboard();
	virtual bool ShouldShowScoreboard();
	virtual bool CanClickScoreboard();

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual FReply OnClose();

private:
	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;
};
#endif
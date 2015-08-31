// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "SlateBasics.h"
#include "SUWDialog.h"
#include "UTFlagInfo.h"

enum ECamFlags
{
	CF_CanInteract = 0x0001,
	CF_ShowScoreboard = 0x0002,
	CF_ShowPlayerNames = 0x0004,
	CF_ShowInfoWidget = 0x0008,
	CF_ShowSwitcher = 0x0010,
	CF_Intro = 0x0020,
	CF_Team = 0x0040,
	CF_Player = 0x0080,
	CF_All = 0x0100,
};

struct FMatchCamera
{
public:
	FMatchCamera() : Time(0.0f), VInterpSpeed(5.0f), RInterpSpeed(5.0f), CamFlags(0){}
	virtual ~FMatchCamera() {};

	/**Called when the cam is first viewed*/
	virtual void InitCam(class SUWMatchSummary* MatchWidget) {}

	/**Return true if finished. 0 time plays forever*/
	virtual bool TickCamera(class SUWMatchSummary* MatchWidget, float ElapsedTime, float DeltaTime, FTransform& InOutCamera)
	{
		InOutCamera.SetLocation(FMath::VInterpTo(InOutCamera.GetLocation(), CameraTransform.GetLocation(), DeltaTime, VInterpSpeed));
		InOutCamera.SetRotation(FMath::RInterpTo(InOutCamera.Rotator(), CameraTransform.Rotator(), DeltaTime, RInterpSpeed).Quaternion());
		return ElapsedTime > Time && Time != 0.0f;
	}

	float Time;
	FTransform CameraTransform;
	float VInterpSpeed;
	float RInterpSpeed;
	uint32 CamFlags;
};

struct FTeamCamera : FMatchCamera
{
public:
	FTeamCamera(int32 InTeamNum) : TeamNum(InTeamNum) {}

	virtual void InitCam(class SUWMatchSummary* MatchWidget) override;
	virtual bool TickCamera(class SUWMatchSummary* MatchWidget, float ElapsedTime, float DeltaTime, FTransform& InOutCamera)
	{
		CameraTransform.Blend(CamStart, CamEnd, FMath::Clamp(ElapsedTime / Time, 0.0f, 1.0f));
		return FMatchCamera::TickCamera(MatchWidget, ElapsedTime, DeltaTime, InOutCamera);
	}

	int32 TeamNum;
	FTransform CamStart;
	FTransform CamEnd;
};

struct FCharacterCamera : FMatchCamera
{
public:
	FCharacterCamera(TWeakObjectPtr<class AUTCharacter> InChar) : Character(InChar) {}
	virtual void InitCam(class SUWMatchSummary* MatchWidget) override;

	TWeakObjectPtr<class AUTCharacter> Character;
};

struct FAllCamera : FMatchCamera
{
public:
	virtual void InitCam(class SUWMatchSummary* MatchWidget) override;
};

#if !UE_SERVER
class UNREALTOURNAMENT_API SUWMatchSummary : public SCompoundWidget, public FGCObject
{
public:
	friend struct FTeamCamera;
	friend struct FCharacterCamera;
	friend struct FAllCamera;

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
	virtual void PlayTauntByClass(AUTPlayerState* PS, TSubclassOf<AUTTaunt> TauntToPlay, float EmoteSpeed);
	virtual void SetEmoteSpeed(AUTPlayerState* PS, float EmoteSpeed);
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

	virtual void ViewCharacter(AUTCharacter* NewChar);
	virtual void ViewTeam(int32 NewTeam);
	virtual void ViewAll();

	/**Shows the team and hides the rest*/
	virtual void ShowTeam(int32 TeamNum);
	/**Shows a character and hides the rest*/
	virtual void ShowCharacter(AUTCharacter* UTC);
	/**Unhides all character and weapons*/
	virtual void ShowAllCharacters();

	// Camera offset when viewing all players
	virtual float GetAllCameraOffset();
	
	FVector2D MousePos;
	int32 ViewedTeamNum;
	TWeakObjectPtr<class AUTCharacter> HighlightedChar;
	TWeakObjectPtr<class AUTCharacter> ViewedChar;

	FTransform CameraTransform;
	FTransform TeamStartCamera;
	FTransform TeamEndCamera;
	float TeamCamAlpha;
	bool bAutoScrollTeam;
	float AutoScrollTeamDirection;

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
	virtual void UpdatePlayerRender(UCanvas* C, int32 Width, int32 Height);
	virtual class AUTCharacter* FindCharacter(class AUTPlayerState* PS);

	class UUTScoreboard* GetScoreboard();
	
	virtual void AddReferencedObjects(FReferenceCollector& Collector) override;

	virtual FReply OnClose();


	// Friends...
	TSharedPtr<class SHorizontalBox> FriendPanel;
	FName FriendStatus;

	FText GetFunnyText();
	virtual void BuildFriendPanel();
	virtual FReply OnSendFriendRequest();

	TArray<TSharedPtr<FMatchCamera> > CameraShots;
	float ShotStartTime;
	int32 CurrentShot;
	virtual void SetCamShot(int32 ShotIndex);
	virtual TSharedPtr<FMatchCamera> GetCurrentShot() const
	{
		if (CameraShots.IsValidIndex(CurrentShot))
		{
			return CameraShots[CurrentShot];
		}
		return nullptr;
	}
	virtual bool HasCamFlag(ECamFlags CamFlag) const;

	virtual void GetTeamCamTransforms(int32 TeamNum, FTransform& Start, FTransform& End);

	virtual void SetupIntroCam();
	virtual void SetupMatchCam();

	TSharedPtr<class SOverlay> XPOverlay;
	TSharedPtr<class SUTXPBar> XPBar;

private:

	TWeakObjectPtr<class UUTLocalPlayer> PlayerOwner;
};
#endif
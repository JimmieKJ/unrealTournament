// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

//TODOTIM
//fix Highlighted characters in player intro (shouldn't highlight)
//fix NextCharacter
//Hide non viewed chars n teams
//Players in floor. Prob due to scale change. Dont hardcode this
//make it so next char can view all characters if more players than trophy spots (just swap with first player in group and hide the rest)
//Hide non relevant hud messages when in trophy room view 
//Make the trophy cameras use more than just the transform (eg FOV)
//Update to steves player name tag highlight info
//Make sure player typing chat keeps summary window open after player intro
//use old default cam/player transforms for the new trophy room
//fix show/hide editor cameras on initial placement
//Improve the auto trophyroom placement
//Make rendering comp show nice meshes in camera preview
//Test server, Test teams, Test teams with only ffa starts, Test ffa with only team starts, Test team with no tropy room, Test ffa with no tropy room, Test taunts, Test wtf happens in Replay
//clean this mess

#pragma once

#include "GameFramework/Actor.h"
#include "UTTrophyRoom.generated.h"

#define MAX_TROPHY_PLAYERS 5

USTRUCT()
struct FTrophyGroup
{
	GENERATED_USTRUCT_BODY()

	FTrophyGroup() { Players.AddDefaulted(MAX_TROPHY_PLAYERS); }

	UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "Trophy Room")
	uint8 Team;

	UPROPERTY(VisibleAnywhere, meta = (MakeEditWidget = ""))
	TArray<FTransform> Players;
};


UCLASS()
class UNREALTOURNAMENT_API AUTTrophyRoom : public AActor
{
	GENERATED_BODY()
	
public:	
	// Sets default values for this actor's properties
	AUTTrophyRoom();

#if WITH_EDITORONLY_DATA
	// Reference to sprite visualization component
	UPROPERTY()
	class UBillboardComponent* SpriteComponent;

	UPROPERTY()
	class UUTTrophyRoomRenderingComponent* TrophyRenderComp;
#endif // WITH_EDITORONLY_DATA

	UPROPERTY(Category = CameraActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* CameraComponent;

	UPROPERTY(Category = CameraActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* CameraTeamRed;

	UPROPERTY(Category = CameraActor, VisibleAnywhere, BlueprintReadOnly, meta = (AllowPrivateAccess = "true"))
	class UCameraComponent* CameraTeamBlue;

	/**Automatically align the player position to the floor*/
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "Trophy Room")
	bool bSnapToFloor;

	/**Grouped locations for each team*/
	UPROPERTY(VisibleAnywhere, Category = "Trophy Room")
	TArray<FTrophyGroup> TeamGroups;

	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Trophy Room")
	TEnumAsByte<ETrophyType::Type> TrophyType;

	virtual void BeginPlay() override;
	virtual void Destroyed() override;
	virtual void BecomeViewTarget(APlayerController* PC) override;
	virtual void EndViewTarget(APlayerController* PC) override;
	virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;
	virtual void PostRenderFor(APlayerController* PC, UCanvas* Canvas, FVector CameraPosition, FVector CameraDir) override;


	virtual void AddPlayerState(AUTPlayerState* PlayerState);
	virtual void RemovePlayerState(AUTPlayerState* PlayerState);

	virtual void RecreateCharacters();
	virtual void DestroyCharacters();

	AUTCharacter* RecreatePlayerPreview(AUTPlayerState* NewPS);

	virtual AUTCharacter* FindCharacter(AUTPlayerState* PS);


	virtual void GetTeamCameraTransform(uint8 Team, FTransform& Transform);
	virtual void GetAllCameraTransform(FTransform& Transform);

	void PositionPlayers();

	UPROPERTY()
	TArray<AUTPlayerState*> PlayerStates;
	UPROPERTY()
	TArray<AUTCharacter*> Characters;
	UPROPERTY()
	TArray<class AUTWeaponAttachment*> PreviewWeapons;
	UPROPERTY()
	TArray<class UAnimationAsset*> PreviewAnimations;

	UFUNCTION(BlueprintCallable, Category = "Trophy Room", meta = (HidePin = "WorldContextObject", DefaultToSelf = "WorldContextObject"))
	static AUTTrophyRoom* GetTrophyRoom(UObject* WorldContextObject, ETrophyType::Type WantedType);


	static int32 GetNumTeamsFromType(ETrophyType::Type WantedType);


	virtual void CollectPlayerStates();

	bool FindSpot(uint8 Team, int32 Index, FTransform& OutSpot);

	AUTPlayerState* NextCharacter(AUTPlayerState* OldPS, bool bBackward = false);

	virtual void SetCameraTransform(const FTransform& NewTransform);

	void ShowCharacters(bool bShow);

	bool bShowCharacters;

#if WITH_EDITOR
	virtual void PreEditChange(UProperty* PropertyThatWillChange) override;
	virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostEditMove(bool bFinished) override;
#endif

	void SnapToFloor();
	void CenterPivot();

	void SetupGroups();


	UPROPERTY(EditAnywhere, meta = (MakeEditWidget = ""))
	FVector CameraPivot;

protected:

	/**This is used for the camera when the trophy room is in the level. Set by SUWMatchSummary*/
	UPROPERTY()
	FTransform CamTransform;


	UTexture2D* ScoreboardAtlas;
};

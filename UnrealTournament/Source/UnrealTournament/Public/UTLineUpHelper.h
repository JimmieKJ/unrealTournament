#pragma  once

#include "UTLineUpZone.h"
#include "UTLineUpHelper.generated.h"

UCLASS()
class UNREALTOURNAMENT_API AUTLineUpHelper : public AActor
{
	GENERATED_UCLASS_BODY()

	UFUNCTION()
	void HandleLineUp(LineUpTypes IntroType);

	UFUNCTION()
	void OnPlayerChange();

	UFUNCTION()
	void CleanUp();

	void ForceCharacterAnimResetForLineUp(AUTCharacter* UTChar);

	void SetupDelayedLineUp();

	UPROPERTY()
	bool bIsActive;
	
	UPROPERTY()
	LineUpTypes LastActiveType;

	UPROPERTY()
	bool bIsPlacingPlayers;
	
	static LineUpTypes GetLineUpTypeToPlay(UWorld* World);

	static AUTLineUpZone* GetAppropriateSpawnList(UWorld* World, LineUpTypes ZoneType);

	static AActor* GetCameraActorForLineUp(UWorld* World, LineUpTypes ZoneType);

	/*Handles all the clean up for a particular player when a line-up is ending*/
	static void CleanUpPlayerAfterLineUp(AUTPlayerController* UTPC);

protected:

	void ClientUpdatePlayerClones();

	UFUNCTION()
	void HandleIntro(LineUpTypes ZoneType);

	UFUNCTION()
	void HandleIntermission(LineUpTypes IntermissionType);

	UFUNCTION()
	void HandleEndMatchSummary(LineUpTypes SummaryType);

	UFUNCTION()
	void SortPlayers();

	UFUNCTION()
	void MovePlayers(LineUpTypes ZoneType);

	UFUNCTION()
	void MovePlayersDelayed(LineUpTypes ZoneType, FTimerHandle& TimerHandleToStart, float TimeDelay);

	UFUNCTION()
	void SpawnPlayerWeapon(AUTCharacter* UTChar);

	UFUNCTION()
	void DestroySpawnedClones();
	
	UFUNCTION()
	void MovePreviewCharactersToLineUpSpawns(LineUpTypes LineUpType);

	/*Flag can be in a bad state since we recreate pawns during Line Up. This function re-assigns the flag to the correct player pawn*/
	UFUNCTION()
	void FlagFixUp();

	TWeakPtr<AUTCharacter> SelectedCharacter;

	float TimerDelayForIntro;
	float TimerDelayForIntermission;
	float TimerDelayForEndMatch;

	FTimerHandle IntroHandle;
	FTimerHandle IntermissionHandle;
	FTimerHandle MatchSummaryHandle;

	/** preview actors */
	TArray<class AUTCharacter*> PlayerPreviewCharacters;

	/** preview weapon */
	TArray<class AUTWeapon*> PreviewWeapons;

	TArray<class UAnimationAsset*> PreviewAnimations;
};
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "UTPlayerController.h"

#include "UTDemoRecSpectator.generated.h"

UNREALTOURNAMENT_API DECLARE_LOG_CATEGORY_EXTERN(LogUTDemoRecSpectator, Log, All);

UCLASS()
class UNREALTOURNAMENT_API AUTDemoRecSpectator : public AUTPlayerController
{
	GENERATED_UCLASS_BODY()

	virtual void PlayerTick(float DeltaTime) override;

	virtual void ReceivedPlayer() override;

	virtual void ViewPlayerState(APlayerState* PS) override;
	virtual void ViewSelf(FViewTargetTransitionParams TransitionParams = FViewTargetTransitionParams()) override;
	virtual void ViewPawn(APawn* PawnToView) override;
	virtual void ServerViewProjectileShim() override;
	virtual void SetPlayer(UPlayer* InPlayer) override;
	virtual void ViewPlayerNum(int32 Index, uint8 TeamNum) override;
	virtual void EnableAutoCam() override;
	virtual void ChooseBestCamera() override;
	virtual void OnAltFire() override;
	virtual void ViewProjectile() override;

	virtual void ViewAPlayer(int32 dir) override;
	virtual APlayerState* GetNextViewablePlayer(int32 dir) override;

	virtual bool CallRemoteFunction(UFunction* Function, void* Parameters, FOutParmRec* OutParms, FFrame* Stack) override;

	virtual void ClientTravelInternal_Implementation(const FString& URL, ETravelType TravelType, bool bSeamless, FGuid MapPackageGuid) override;
	virtual void ClientReceiveLocalizedMessage_Implementation(TSubclassOf<ULocalMessage> Message, int32 Switch, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject) override;

	virtual void ClientToggleScoreboard_Implementation(bool bShow) override;
	virtual void ShowEndGameScoreboard() override;

	virtual void ClientGameEnded_Implementation(AActor* EndGameFocus, bool bIsWinner) override;

	virtual void InitPlayerState() override;
	virtual void CleanupPlayerState() override
	{
		// don't do AddInactivePlayer() stuff for demo spectator
		AController::CleanupPlayerState();
	}

	virtual void BeginPlay() override;
	virtual void OnNetCleanup(class UNetConnection* Connection) override;

	UFUNCTION(Exec)
	void ToggleReplayWindow();

	virtual void ShowMenu(const FString& Parameters) override;
	virtual void HideMenu() override;


	virtual void SmoothTargetViewRotation(APawn* TargetPawn, float DeltaSeconds) override;

	virtual void ViewFlag(uint8 Index) override;

	UFUNCTION(Client, UnReliable)
	virtual void DemoNotifyCausedHit(APawn* InstigatorPawn, AUTCharacter* HitPawn, uint8 AppliedDamage, FVector Momentum, const FDamageEvent& DamageEvent);

	UFUNCTION(NetMulticast, Reliable)
	virtual void MulticastReceiveLocalizedMessage(TSubclassOf<ULocalMessage> Message, int32 Switch, APlayerState* RelatedPlayerState_1, APlayerState* RelatedPlayerState_2, UObject* OptionalObject);

	UPROPERTY()
	APlayerState* QueuedPlayerStateToView;
	
	UPROPERTY()
	float LastKillcamSeekTime;

	virtual bool IsKillcamSpectator() const;

	FNetworkGUID QueuedViewTargetGuid;
	void SetQueuedViewTargetGuid(FNetworkGUID InViewTargetGuid) { QueuedViewTargetGuid = InViewTargetGuid; }
	void ViewQueuedGuid();

	FUniqueNetIdRepl QueuedViewTargetNetId;
	void SetQueuedViewTargetNetId(FUniqueNetIdRepl InViewTargetNetId) { QueuedViewTargetNetId = InViewTargetNetId; }
	void ViewQueuedNetId();

#if !UE_SERVER
	virtual void UpdateInputMode() override 
	{
		if (!IsKillcamSpectator())
		{
			Super::UpdateInputMode();
		}
	
	};
#endif

	UFUNCTION(Exec)
	void DumpSpecInfo();

};
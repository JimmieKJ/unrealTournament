// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GameplayDebuggerModuleSettings.h"
#include "Debug/GameplayDebuggerBaseObject.h"
#include "GameFramework/HUD.h"
#include "GameplayDebuggerReplicator.generated.h"
/**
*	Transient actor used to communicate between server and client, mostly for RPC
*/

class UGameplayDebuggingComponent;
class UGameplayDebuggerBaseObject;
class AGameplayDebuggingHUDComponent;
class ADebugCameraController;

UCLASS(NotBlueprintable, Transient, hidecategories = Actor, notplaceable)
class AGameplayDebuggerReplicator : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(Transient, Replicated)
	APlayerController* LocalPlayerOwner;

	UPROPERTY(Transient, Replicated)
	AActor*	LastSelectedActorToDebug;

	UPROPERTY(Transient, Replicated)
	TArray<class UGameplayDebuggerBaseObject*> ReplicatedObjects;

	UPROPERTY(Transient, ReplicatedUsing = OnRep_ReplicatedCategories)
	TArray<FGameplayDebuggerCategorySettings> Categories;

	UFUNCTION()
	virtual void OnRep_ReplicatedCategories();

	UPROPERTY()
	class UTexture2D* DefaultTexture_Red;

	UPROPERTY()
	class UTexture2D* DefaultTexture_Green;

	UPROPERTY(Transient)
	class UInputComponent* DebugCameraInputComponent;

	UFUNCTION(Reliable, Client, WithValidation)
	void ClientEnableTargetSelection(bool bEnable, APlayerController* Context);

	UFUNCTION(Reliable, Server, WithValidation)
	void ServerToggleCategory(const FString& CategoryName);

	bool IsCategoryEnabled(const FString& CategoryName);

	virtual class UNetConnection* GetNetConnection() const override;

	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;

	virtual void BeginPlay() override;

	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

	virtual void BeginDestroy() override;

	virtual bool ReplicateSubobjects(class UActorChannel *Channel, class FOutBunch *Bunch, FReplicationFlags *RepFlags) override;

	void SelectTargetToDebug();

	void EnableDraw(bool bEnable);

	void SetLocalPlayerOwner(APlayerController* PC);
	APlayerController* GetLocalPlayerOwner() { return LocalPlayerOwner; }

	FORCEINLINE AActor* GetSelectedActorToDebug() { return LastSelectedActorToDebug; }

	UFUNCTION(Reliable, Server, WithValidation)
	void ServerSetActorToDebug(AActor* InActor);

	UFUNCTION(Reliable, Client, WithValidation)
	void ClientActivateGameplayDebugger(bool bActivate);

	UFUNCTION(Reliable, Server, WithValidation)
	void ServerActivateGameplayDebugger(bool bActivate);

	/**
	* Iterates through the pawn list to find the next pawn of the specified type to debug
	*/
	void DebugNextPawn(UClass* CompareClass, APawn* CurrentPawn = nullptr);

	/**
	* Iterates through the pawn list to find the previous pawn of the specified type to debug
	*/
	void DebugPrevPawn(UClass* CompareClass, APawn* CurrentPawn = nullptr);

	void OnActivationKeyPressed();

	void ActivateFromCommand();

protected:
	void BindKeyboardInput(class UInputComponent*& InInputComponent);
	void OnDebugAIDelegate(class UCanvas* Canvas, class APlayerController* PC);
	void DrawDebugDataDelegate(class UCanvas* Canvas, class APlayerController* PC);
	void DrawDebugData(class UCanvas* Canvas, class APlayerController* PC, bool bHideMenu = false);
	void DrawMenu(UGameplayDebuggerBaseObject::FPrintContext& DefaultContext, UGameplayDebuggerBaseObject::FPrintContext& OverHeadContext);

	void OnSelectPlayer();
	void OnNumPadZero();
	void OnNumPadOne();
	void OnNumPadTwo();
	void OnNumPadThree();
	void OnNumPadFour();
	void OnNumPadFive();
	void OnNumPadSix();
	void OnNumPadSeven();
	void OnNumPadEight();
	void OnNumPadNine();

	void OnNextMenuLine();
	void OnPreviousMenuLine();

	void OnActivationKeyReleased();

	void ToggleCategory(int32 CategoryIndex);
	void ToggleDebugCamera();
	void ToggleOnScreenDebugMessages();
	void ToggleGameHUD();

private:
	TWeakObjectPtr<ADebugCameraController> DebugCameraController;

	uint32 bEnabledTargetSelection : 1;
	uint8 bActivationKeyPressed : 1;
	int32 CurrentMenuLine;
	float PlayerControllersUpdateDelay;
	float ActivationKeyTime;

	float MaxMenuWidth;
	FString ActivationKeyDisplayName;
	FString ActivationKeyName;
	FDelegateHandle OnSelectionChangedHandle;
};

UCLASS()
class UGameplayDebuggerDrawComponent : public UPrimitiveComponent
{
	GENERATED_UCLASS_BODY()
public:
	virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
	virtual FBoxSphereBounds CalcBounds(const FTransform &LocalToWorld) const override;
	virtual void CreateRenderState_Concurrent() override;
	virtual void DestroyRenderState_Concurrent() override;
};

UCLASS(NotBlueprintable, Transient, NotBlueprintType, hidedropdown)
class AGameplayDebuggerHUD : public AHUD
{
	GENERATED_UCLASS_BODY()

	FFontRenderInfo TextRenderInfo;

	//~ Begin AActor Interface
	virtual void PostRender() override;
	//~ End AActor Interface

	TWeakObjectPtr<AHUD> RedirectedHUD;
};

UCLASS(NotBlueprintable, Transient, hidecategories = Actor, notplaceable)
class AGameplayDebuggerServerHelper : public AActor
{
	GENERATED_UCLASS_BODY()

	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;
};


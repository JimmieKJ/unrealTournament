// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "GameplayDebuggingTypes.h"
#include "GameplayDebuggingReplicator.generated.h"
/**
*	Transient actor used to communicate between server and client, mostly for RPC
*/

class UGameplayDebuggingComponent;
class AGameplayDebuggingHUDComponent;

DECLARE_MULTICAST_DELEGATE_OneParam(FOnSelectionChanged, class AActor*);

UCLASS(config = Engine, NotBlueprintable, Transient, hidecategories = Actor, notplaceable)
class GAMEPLAYDEBUGGER_API AGameplayDebuggingReplicator : public AActor
{
	GENERATED_UCLASS_BODY()

	UPROPERTY(config)
	FString DebugComponentClassName;

	UPROPERTY(config)
	FString DebugComponentHUDClassName;

	UPROPERTY(config)
	FString DebugComponentControllerClassName;

	UPROPERTY(config)
	int32 MaxEQSQueries;

	UPROPERTY(Replicated, Transient)
	UGameplayDebuggingComponent* DebugComponent;

	UPROPERTY(Replicated, Transient)
	APlayerController* LocalPlayerOwner;

	UPROPERTY(Replicated, Transient)
	AActor*	LastSelectedActorToDebug;

	UPROPERTY(Replicated, Transient)
	bool bIsGlobalInWorld;

	UPROPERTY(ReplicatedUsing = OnRep_AutoActivate, Transient)
	bool bAutoActivate;

	UPROPERTY(Transient, EditAnywhere, Category = "DataView")
	bool OverHead;
	
	UPROPERTY(Transient, EditAnywhere, Category = "DataView")
	bool Basic;
	
	UPROPERTY(Transient, EditAnywhere, Category = "DataView")
	bool BehaviorTree;
	
	UPROPERTY(Transient, EditAnywhere, Category = "DataView|EQS")
	bool EQS;

	UPROPERTY(Transient, EditAnywhere, Category = "DataView|EQS", meta = (EditCondition = "EQS"))
	bool EnableEQSOnHUD;

	UPROPERTY(Transient, EditAnywhere, Category = "DataView|EQS", meta = (EditCondition = "EQS"))
	int32 ActiveEQSIndex;

	UPROPERTY(Transient, EditAnywhere, Category = "DataView")
	bool Perception;
	
	UPROPERTY(Transient, EditAnywhere, Category = "DataView")
	bool GameView1;
	
	UPROPERTY(Transient, EditAnywhere, Category = "DataView")
	bool GameView2;
	
	UPROPERTY(Transient, EditAnywhere, Category = "DataView")
	bool GameView3;
	
	UPROPERTY(Transient, EditAnywhere, Category = "DataView")
	bool GameView4;
	
	UPROPERTY(Transient, EditAnywhere, Category = DataView)
	bool GameView5;

	UPROPERTY()
	class UTexture2D* DefaultTexture_Red;

	UPROPERTY()
	class UTexture2D* DefaultTexture_Green;

	UFUNCTION(reliable, server, WithValidation)
	void ServerReplicateMessage(class AActor* Actor, uint32 InMessage, uint32 DataView = 0);

	UFUNCTION(reliable, client, WithValidation)
	void ClientReplicateMessage(class AActor* Actor, uint32 InMessage, uint32 DataView = 0);

	UFUNCTION(Reliable, Client)
	void ClientAutoActivate();

	UFUNCTION(Reliable, Client, WithValidation)
	void ClientEnableTargetSelection(bool bEnable, APlayerController* Context);

#if WITH_EDITOR
	void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#endif

	UFUNCTION()
	virtual void OnRep_AutoActivate();

	virtual class UNetConnection* GetNetConnection() const override;

	virtual bool IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;

	virtual void PostNetInit() override;
	
	virtual void PostInitializeComponents() override;

	virtual void BeginPlay() override;

	virtual void TickActor(float DeltaTime, enum ELevelTick TickType, FActorTickFunction& ThisTickFunction) override;

	virtual void BeginDestroy() override;

	UGameplayDebuggingComponent* GetDebugComponent();

	bool IsToolCreated();
	void CreateTool();
	void EnableTool();
	bool IsDrawEnabled();
	void EnableDraw(bool bEnable);
	void SetAsGlobalInWorld(bool IsGlobal) { bIsGlobalInWorld = IsGlobal;  }
	bool IsGlobalInWorld() { return bIsGlobalInWorld;  }

	void SetLocalPlayerOwner(APlayerController* PC) { LocalPlayerOwner = PC; }
	APlayerController* GetLocalPlayerOwner() { return LocalPlayerOwner; }

	FORCEINLINE AActor* GetSelectedActorToDebug() { return LastSelectedActorToDebug; }
	
	UFUNCTION(reliable, server, WithValidation)
	void ServerSetActorToDebug(AActor* InActor);

	/**
	 * Iterates through the pawn list to find the next pawn of the specified type to debug
	 */
	void DebugNextPawn(UClass* CompareClass, APawn* CurrentPawn = nullptr);

	/**
	 * Iterates through the pawn list to find the previous pawn of the specified type to debug
	 */
	void DebugPrevPawn(UClass* CompareClass, APawn* CurrentPawn = nullptr);

	uint32 DebuggerShowFlags;

	static FOnSelectionChanged OnSelectionChangedDelegate;
	FOnCycleDetailsView OnCycleDetailsView;
protected:
	void OnDebugAIDelegate(class UCanvas* Canvas, class APlayerController* PC);
	void DrawDebugDataDelegate(class UCanvas* Canvas, class APlayerController* PC);
	void DrawDebugData(class UCanvas* Canvas, class APlayerController* PC);

private:
	uint32 bEnabledDraw : 1;
	uint32 LastDrawAtFrame;
	float PlayerControllersUpdateDelay;

	TWeakObjectPtr<UClass> DebugComponentClass;
	TWeakObjectPtr<UClass> DebugComponentHUDClass;
	TWeakObjectPtr<UClass> DebugComponentControllerClass;
	TWeakObjectPtr<AGameplayDebuggingHUDComponent>	DebugRenderer;
};

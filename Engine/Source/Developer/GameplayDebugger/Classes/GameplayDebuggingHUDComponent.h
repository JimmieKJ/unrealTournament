// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

//////////////////////////////////////////////////////////////////////////
// THIS CLASS IS NOW DEPRECATED AND WILL BE REMOVED IN NEXT VERSION
// Please check GameplayDebugger.h for details.

#pragma once
#include "ShowFlags.h"
#include "GameplayDebuggerSettings.h"
#include "GameplayDebuggingHUDComponent.generated.h"

class AGameplayDebuggingReplicator;

struct GAMEPLAYDEBUGGER_API FDebugCategoryView
{
	FString Desc;
	TEnumAsByte<EAIDebugDrawDataView::Type> View;

	FDebugCategoryView() {}
	FDebugCategoryView(EAIDebugDrawDataView::Type InView, const FString& Description) : Desc(Description), View(InView) {}
};

#define ADD_GAMEVIEW_CATEGORY(__Category__) \
{\
	UGameplayDebuggerSettings* GDS = UGameplayDebuggerSettings::StaticClass()->GetDefaultObject<UGameplayDebuggerSettings>();\
	Categories.Add(FDebugCategoryView(EAIDebugDrawDataView::__Category__, GDS->GetCustomViewNames().__Category__.Len() ? GDS->GetCustomViewNames().__Category__ : TEXT(#__Category__)));\
}

UCLASS(config = Engine, notplaceable)
class GAMEPLAYDEBUGGER_API AGameplayDebuggingHUDComponent : public AActor
{
	GENERATED_UCLASS_BODY()

	struct FPrintContext
	{
	public:
		class UCanvas* Canvas; 
		class UFont* Font;
		float CursorX, CursorY;
		float DefaultX, DefaultY;
		FFontRenderInfo FontRenderInfo;
	public:
		FPrintContext() {}
		FPrintContext(class UFont* InFont, class UCanvas* InCanvas, float InX, float InY) : Canvas(InCanvas), Font(InFont), CursorX(InX), CursorY(InY), DefaultX(InX), DefaultY(InY) {}
	};

public:
	virtual void Render();
	void SetWorld(UWorld* InWorld) { World = InWorld; }
	virtual UWorld* GetWorld() const override { return World != NULL ? World : Super::GetWorld(); }

	/** Set the canvas to use during drawing */
	void SetCanvas(class UCanvas* InCanvas) { Canvas = InCanvas; }
	void SetPlayerOwner(APlayerController* InPlayerOwner) { PlayerOwner = InPlayerOwner; }
	APlayerController* GetPlayerOwner() { return PlayerOwner; }

protected:
	//virtual void DrawOnCanvas(class UCanvas* Canvas, APlayerController* PC);
	virtual void DrawPath(APlayerController* PC, class UGameplayDebuggingComponent *DebugComponent);
	virtual void DrawOverHeadInformation(APlayerController* PC, class UGameplayDebuggingComponent *DebugComponent);
	virtual void DrawBasicData(APlayerController* PC, class UGameplayDebuggingComponent *DebugComponent);
	virtual void DrawBehaviorTreeData(APlayerController* PC, class UGameplayDebuggingComponent *DebugComponent);
	virtual void DrawEQSData(APlayerController* PC, class UGameplayDebuggingComponent *DebugComponent);
	virtual void DrawPerception(APlayerController* PC, class UGameplayDebuggingComponent *DebugComponent);
	virtual void DrawNavMeshSnapshot(APlayerController* PC, class UGameplayDebuggingComponent *DebugComponent);

#if !ENABLE_OLD_GAMEPLAY_DEBUGGER
	DEPRECATED_FORGAME(4.13, "GameplayDebuggingHUDComponent class is now deprecated, please check GameplayDebugger.h for details.")
#endif // !ENABLE_OLD_GAMEPLAY_DEBUGGER
	virtual void DrawGameSpecificView(APlayerController* PC, class UGameplayDebuggingComponent *DebugComponent) {}

	void PrintAllData();
	void DrawMenu(const float X, const float Y, class UGameplayDebuggingComponent* DebugComponent);
	static void CalulateStringSize(const AGameplayDebuggingHUDComponent::FPrintContext& DefaultContext, UFont* Font, const FString& InString, float& OutX, float& OutY);
	static void CalulateTextSize(const AGameplayDebuggingHUDComponent::FPrintContext& DefaultContext, UFont* Font, const FText& InText, float& OutX, float& OutY);
	static FVector ProjectLocation(const AGameplayDebuggingHUDComponent::FPrintContext& Context, const FVector& Location);
	static void DrawItem(const AGameplayDebuggingHUDComponent::FPrintContext& DefaultContext, class FCanvasItem& Item, float X, float Y );
	static void DrawIcon(const AGameplayDebuggingHUDComponent::FPrintContext& DefaultContext, const FColor& InColor, const FCanvasIcon& Icon, float X, float Y, float Scale = 0.f);

	virtual void GetKeyboardDesc(TArray<FDebugCategoryView>& Categories);

	void PrintString(FPrintContext& Context, const FString& InString );
	void PrintString(FPrintContext& Context, const FColor& InColor, const FString& InString );
	void PrintString(FPrintContext& Context, const FColor& InColor, const FString& InString, float X, float Y );

	void DrawEQSItemDetails(int32 ItemIdx, class UGameplayDebuggingComponent *DebugComponent);

	AGameplayDebuggingReplicator* GetDebuggingReplicator();

private:
	// local player related draw from PostRender
	void DrawDebugComponentData(APlayerController* PC, class UGameplayDebuggingComponent *DebugComponent);

public:
	UPROPERTY(config)
	float MenuStartX;
	UPROPERTY(config)
	float MenuStartY;
	UPROPERTY(config)
	float DebugInfoStartX;
	UPROPERTY(config)
	float DebugInfoStartY;

protected:
	UPROPERTY(Transient)
	UCanvas* Canvas;

	UPROPERTY(Transient)
	APlayerController* PlayerOwner;

	TWeakObjectPtr<AGameplayDebuggingReplicator> CachedDebuggingReplicator;

	FString HugeOutputString;

	FPrintContext OverHeadContext;
	FPrintContext DefaultContext;
	UWorld* World;
	FEngineShowFlags EngineShowFlags;
	float BlackboardFinishY;

private:
	float ItemDescriptionWidth;
	float ItemScoreWidth;
	float TestScoreWidth;
};

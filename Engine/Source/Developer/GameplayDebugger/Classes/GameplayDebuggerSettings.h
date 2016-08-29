// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

//////////////////////////////////////////////////////////////////////////
// THIS CLASS IS NOW DEPRECATED AND WILL BE REMOVED IN NEXT VERSION
// Please check GameplayDebugger.h for details.

#pragma once


#include "GameplayDebuggingTypes.h"
#include "GameplayDebuggerSettings.generated.h"

#define ADD_LEVEL_EDITOR_EXTENSIONS 0

USTRUCT()
struct FGDTCustomViewNames
{
	GENERATED_USTRUCT_BODY()

	UPROPERTY(EditAnywhere, config, Category = "Settings")
	FString GameView1;

	UPROPERTY(EditAnywhere, config, Category = "Settings")
	FString GameView2;

	UPROPERTY(EditAnywhere, config, Category = "Settings")
	FString GameView3;

	UPROPERTY(EditAnywhere, config, Category = "Settings")
	FString GameView4;

	UPROPERTY(EditAnywhere, config, Category = "Settings")
	FString GameView5;
};

UCLASS(config = EditorPerProjectUserSettings)
class GAMEPLAYDEBUGGER_API UGameplayDebuggerSettings : public UObject
{
	GENERATED_UCLASS_BODY()
public:
#if WITH_EDITOR
	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual void PostInitProperties() override;
	//~ End UObject Interface
#endif

	DECLARE_EVENT_OneParam(UGameplayDebuggerSettings, FSettingChangedEvent, FName /*PropertyName*/);
	FSettingChangedEvent& OnSettingChanged() { return SettingChangedEvent; }

	uint32& GetSettings()
	{
		static uint32 Settings = 0;
		Settings = OverHead ? Settings | (1 << EAIDebugDrawDataView::OverHead) : Settings & ~(1 << EAIDebugDrawDataView::OverHead);
		Settings = Basic ? Settings | (1 << EAIDebugDrawDataView::Basic) : Settings & ~(1 << EAIDebugDrawDataView::Basic);
		Settings = BehaviorTree ? Settings | (1 << EAIDebugDrawDataView::BehaviorTree) : Settings & ~(1 << EAIDebugDrawDataView::BehaviorTree);
		Settings = EQS ? Settings | (1 << EAIDebugDrawDataView::EQS) : Settings & ~(1 << EAIDebugDrawDataView::EQS);
		Settings = Perception ? Settings | (1 << EAIDebugDrawDataView::Perception) : Settings & ~(1 << EAIDebugDrawDataView::Perception);
		Settings = GameView1 ? Settings | (1 << EAIDebugDrawDataView::GameView1) : Settings & ~(1 << EAIDebugDrawDataView::GameView1);
		Settings = GameView2 ? Settings | (1 << EAIDebugDrawDataView::GameView2) : Settings & ~(1 << EAIDebugDrawDataView::GameView2);
		Settings = GameView3 ? Settings | (1 << EAIDebugDrawDataView::GameView3) : Settings & ~(1 << EAIDebugDrawDataView::GameView3);
		Settings = GameView4 ? Settings | (1 << EAIDebugDrawDataView::GameView4) : Settings & ~(1 << EAIDebugDrawDataView::GameView4);
		Settings = GameView5 ? Settings | (1 << EAIDebugDrawDataView::GameView5) : Settings & ~(1 << EAIDebugDrawDataView::GameView5);

		return Settings;
	}

	const FGDTCustomViewNames& GetCustomViewNames() { return CustomViewNames; }

#if ADD_LEVEL_EDITOR_EXTENSIONS
	/** Experimental setting to extend viewport menu in Simulate, to have quick access to GameplayDebugger settings. */
	bool bExtendViewportMenu;
#endif

protected:
	UPROPERTY(EditAnywhere, config, Category = "GameplayDebugger")
	FGDTCustomViewNames CustomViewNames;
	
	UPROPERTY(EditAnywhere, config, Category = "GameplayDebugger")
	bool OverHead;

	UPROPERTY(EditAnywhere, config, Category = "GameplayDebugger")
	bool Basic;

	UPROPERTY(EditAnywhere, config, Category = "GameplayDebugger")
	bool BehaviorTree;

	UPROPERTY(EditAnywhere, config, Category = "GameplayDebugger|EQS")
	bool EQS;

	UPROPERTY(EditAnywhere, config, Category = "GameplayDebugger|EQS", meta = (EditCondition = "EQS"))
	bool EnableEQSOnHUD;

	UPROPERTY(EditAnywhere, config, Category = "GameplayDebugger|EQS", meta = (EditCondition = "EQS"))
	int32 ActiveEQSIndex;

	UPROPERTY(EditAnywhere, config, Category = "GameplayDebugger")
	bool Perception;

	UPROPERTY(EditAnywhere, config, Category = "GameplayDebugger")
	bool GameView1;

	UPROPERTY(EditAnywhere, config, Category = "GameplayDebugger")
	bool GameView2;

	UPROPERTY(EditAnywhere, config, Category = "GameplayDebugger")
	bool GameView3;

	UPROPERTY(EditAnywhere, config, Category = "GameplayDebugger")
	bool GameView4;

	UPROPERTY(EditAnywhere, config, Category = "GameplayDebugger")
	bool GameView5;

	// Holds an event delegate that is executed when a setting has changed.
	FSettingChangedEvent SettingChangedEvent;
};

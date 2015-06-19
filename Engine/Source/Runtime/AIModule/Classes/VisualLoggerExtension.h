// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#if ENABLE_VISUAL_LOG
#include "VisualLogger/VisualLogger.h"
#endif
#include "VisualLoggerExtension.generated.h"

class UWorld;
class AActor;

namespace EVisLogTags
{
	const FString TAG_EQS = TEXT("LogEQS");
}

#if ENABLE_VISUAL_LOG
struct FVisualLogDataBlock;
struct FLogEntryItem;
class UCanvas;

class FVisualLoggerExtension : public FVisualLogExtensionInterface
{
public:
	FVisualLoggerExtension();
	virtual void OnTimestampChange(float Timestamp, UWorld* InWorld, AActor* HelperActor) override;
	virtual void DrawData(UWorld* InWorld, UCanvas* Canvas, AActor* HelperActor, const FName& TagName, const FVisualLogDataBlock& DataBlock, float Timestamp) override;
	virtual void DisableDrawingForData(UWorld* InWorld, UCanvas* Canvas, AActor* HelperActor, const FName& TagName, const FVisualLogDataBlock& DataBlock, float Timestamp) override;
	virtual void LogEntryLineSelectionChanged(TSharedPtr<FLogEntryItem> SelectedItem, int64 UserData, FName TagName) override;

private:
	void DisableEQSRendering(AActor* HelperActor);

protected:
	uint32 CachedEQSId;
	uint32 SelectedEQSId;
	float CurrentTimestamp;
};
#endif //ENABLE_VISUAL_LOG

UCLASS(Abstract)
class AIMODULE_API UVisualLoggerExtension : public UObject
{
	GENERATED_BODY()
};

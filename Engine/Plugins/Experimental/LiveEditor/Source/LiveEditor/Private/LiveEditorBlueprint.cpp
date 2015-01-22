// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "LiveEditorPrivatePCH.h"
#include "LiveEditorUserData.h"

namespace nLiveEditorBlueprint
{
	FString GetSafeBlueprintName( const ULiveEditorBlueprint &Instance )
	{
		FString BlueprintName = Instance.GetName();
		int32 chopIndex = BlueprintName.Find( TEXT("_C"), ESearchCase::CaseSensitive, ESearchDir::FromEnd );
		if ( chopIndex != INDEX_NONE )
		{
			BlueprintName = BlueprintName.LeftChop( BlueprintName.Len() - chopIndex );
		}
		return BlueprintName;
	}
}

ULiveEditorBlueprint::ULiveEditorBlueprint(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

UWorld* ULiveEditorBlueprint::GetWorld() const
{
	return FLiveEditorManager::Get().GetLiveEditorWorld();
}

void ULiveEditorBlueprint::DoInit()
{
	if ( FLiveEditorManager::Get().GetLiveEditorUserData() )
	{
		FString BlueprintName = nLiveEditorBlueprint::GetSafeBlueprintName( *this );
		FLiveEditorManager::Get().GetLiveEditorUserData()->LoadCheckpointData( BlueprintName, CheckpointMap );
	}
	OnInit();
}

void ULiveEditorBlueprint::SaveCheckpoint( int32 CheckpointID, const TArray<FLiveEditorCheckpointData> &CheckpointData )
{
	CheckpointMap.Add( CheckpointID, CheckpointData );

	if ( FLiveEditorManager::Get().GetLiveEditorUserData() )
	{
		FString BlueprintName = nLiveEditorBlueprint::GetSafeBlueprintName( *this );
		FLiveEditorManager::Get().GetLiveEditorUserData()->SaveCheckpointData( BlueprintName, CheckpointMap );
	}
}

void ULiveEditorBlueprint::LoadCheckpoint( int32 CheckpointID, TArray<FLiveEditorCheckpointData> &CheckpointData )
{
	if ( !CheckpointMap.Contains(CheckpointID) )
		return;

	CheckpointData = CheckpointMap[CheckpointID];
	return;
}

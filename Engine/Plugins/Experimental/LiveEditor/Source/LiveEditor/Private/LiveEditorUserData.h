// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FLiveEditorUserData
{
public:
	FLiveEditorUserData();
	~FLiveEditorUserData();

	void AddBlueprintBinding( const FString &BlueprintName, const FString &EventName, const FLiveEditBinding &Binding, bool SaveNow = true );
	bool HasBlueprintBindings( const class UBlueprint &Blueprint ) const;
	const struct FLiveEditBinding *GetBinding( class FString &BlueprintName, const FString &EventName ) const;
	
	void SaveCheckpointData( const FString &BlueprintName, const TMap< int32, TArray<FLiveEditorCheckpointData> > &Data, bool SaveNow = true );
	bool LoadCheckpointData( const FString &BlueprintName, TMap< int32, TArray<FLiveEditorCheckpointData> > &Data );

	void Save();

private:
	void SaveToDisk();
	void LoadFromDisk();

	//for each LiveEditorBlueprint::Name we store a map of EventName -> FLiveEditBinding Data
	TMap< FString, TMap<FString, struct FLiveEditBinding> > BlueprintBindings;

	//for each LiveEditorBlueprint::Name we store their checkpoint map
	TMap< FString, TMap< int32, TArray<FLiveEditorCheckpointData> > > Checkpoints;
};

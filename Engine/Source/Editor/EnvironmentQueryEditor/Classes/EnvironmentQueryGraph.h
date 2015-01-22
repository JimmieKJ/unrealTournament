// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once
#include "EnvironmentQueryGraph.generated.h"

UCLASS()
class UEnvironmentQueryGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()

	UPROPERTY()
	int32 GraphVersion;

	void UpdateVersion();
	void MarkVersion();

	void CalculateAllWeights();
	void UpdateAsset();
	void RemoveOrphanedNodes();
	void CreateEnvQueryFromGraph(class UEnvironmentQueryGraphNode* RootEdNode);

	void LockUpdates();
	void UnlockUpdates();

protected:

	/** if set, graph modifications won't cause updates in internal tree structure (skipping UpdateAsset)
	 * flag allows freezing update during heavy changes like pasting new nodes */
	uint32 bLockUpdates : 1;

	void UpdateVersion_NestedNodes();
	void UpdateVersion_FixupOuters();
};


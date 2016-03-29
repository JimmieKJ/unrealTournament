// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "AIGraph.h"
#include "EnvironmentQueryGraph.generated.h"

class UEnvQueryOption;
class UEnvQueryTest;
class UEnvironmentQueryGraphNode;

UCLASS()
class UEnvironmentQueryGraph : public UAIGraph
{
	GENERATED_UCLASS_BODY()

	virtual void Initialize() override;
	virtual void OnLoaded() override;
	virtual void UpdateVersion() override;
	virtual void MarkVersion() override;
	virtual void UpdateAsset(int32 UpdateFlags = 0) override;

	void UpdateDeprecatedGeneratorClasses();
	void SpawnMissingNodes();
	void CalculateAllWeights();
	void CreateEnvQueryFromGraph(class UEnvironmentQueryGraphNode* RootEdNode);

protected:

	void UpdateVersion_NestedNodes();
	void UpdateVersion_FixupOuters();
	void UpdateVersion_CollectClassData();

	virtual void CollectAllNodeInstances(TSet<UObject*>& NodeInstances) override;
	virtual void OnNodeInstanceRemoved(UObject* NodeInstance) override;
	virtual void OnNodesPasted(const FString& ImportStr) override;

	void SpawnMissingSubNodes(UEnvQueryOption* Option, TSet<UEnvQueryTest*> ExistingTests, UEnvironmentQueryGraphNode* OptionNode);
};

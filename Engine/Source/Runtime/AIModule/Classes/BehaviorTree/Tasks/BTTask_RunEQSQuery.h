// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EnvironmentQuery/EnvQueryTypes.h"
#include "BTTask_RunEQSQuery.generated.h"

class UEnvQuery;

struct FBTEnvQueryTaskMemory
{
	/** Query request ID */
	int32 RequestID;
};

/**
 * Run Environment Query System Query task node.
 * Runs the specified environment query when executed.
 */
UCLASS()
class AIMODULE_API UBTTask_RunEQSQuery : public UBTTask_BlackboardBase
{
	GENERATED_UCLASS_BODY()

	/** query to run */
	UPROPERTY(Category=Node, EditAnywhere)
	UEnvQuery* QueryTemplate;

	/** optional parameters for query */
	UPROPERTY(Category=Node, EditAnywhere)
	TArray<FEnvNamedValue> QueryParams;

	/** determines which item will be stored (All = only first matching) */
	UPROPERTY(Category = Node, EditAnywhere)
	TEnumAsByte<EEnvQueryRunMode::Type> RunMode;

	virtual EBTNodeResult::Type ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;
	virtual EBTNodeResult::Type AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory) override;

	virtual void DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const override;
	virtual FString GetStaticDescription() const override;
	virtual uint16 GetInstanceMemorySize() const override;

	/** finish task */
	void OnQueryFinished(TSharedPtr<FEnvQueryResult> Result);

#if WITH_EDITOR
	/** prepare query params */
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent) override;
	virtual FName GetNodeIconName() const override;
#endif

protected:

	/** gather all filters from existing EnvQueryItemTypes */
	void CollectKeyFilters();
};

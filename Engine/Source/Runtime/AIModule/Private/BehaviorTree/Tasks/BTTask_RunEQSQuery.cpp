// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "BehaviorTree/Tasks/BTTask_RunEQSQuery.h"
#include "EnvironmentQuery/Items/EnvQueryItemType.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQuery.h"

UBTTask_RunEQSQuery::UBTTask_RunEQSQuery(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
	NodeName = "Run EQS Query";

	// filter with keys that have matching env query values, only for instances
	// CDO won't be able to access types from game dlls
	if (GIsEditor && !HasAnyFlags(RF_ClassDefaultObject))
	{
		CollectKeyFilters();
	}
}

EBTNodeResult::Type UBTTask_RunEQSQuery::ExecuteTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	AActor* QueryOwner = OwnerComp.GetOwner();
	AController* ControllerOwner = Cast<AController>(QueryOwner);
	if (ControllerOwner)
	{
		QueryOwner = ControllerOwner->GetPawn();
	}

	FEnvQueryRequest QueryRequest(QueryTemplate, QueryOwner);
	QueryRequest.SetNamedParams(QueryParams);

	FBTEnvQueryTaskMemory* MyMemory = (FBTEnvQueryTaskMemory*)NodeMemory;
	MyMemory->RequestID = QueryRequest.Execute(RunMode, this, &UBTTask_RunEQSQuery::OnQueryFinished);

	const bool bValid = (MyMemory->RequestID >= 0);
	if (bValid)
	{
		WaitForMessage(OwnerComp, UBrainComponent::AIMessage_QueryFinished, MyMemory->RequestID);
		return EBTNodeResult::InProgress;
	}

	return EBTNodeResult::Failed;
}

EBTNodeResult::Type UBTTask_RunEQSQuery::AbortTask(UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory)
{
	UWorld* MyWorld = OwnerComp.GetWorld();
	UEnvQueryManager* QueryManager = UEnvQueryManager::GetCurrent(MyWorld);
	
	if (QueryManager)
	{
		FBTEnvQueryTaskMemory* MyMemory = (FBTEnvQueryTaskMemory*)NodeMemory;
		QueryManager->AbortQuery(MyMemory->RequestID);
	}

	return EBTNodeResult::Aborted;
}

FString UBTTask_RunEQSQuery::GetStaticDescription() const
{
	return FString::Printf(TEXT("%s: %s\nBlackboard key: %s"), *Super::GetStaticDescription(),
		*GetNameSafe(QueryTemplate), *BlackboardKey.SelectedKeyName.ToString());
}

void UBTTask_RunEQSQuery::DescribeRuntimeValues(const UBehaviorTreeComponent& OwnerComp, uint8* NodeMemory, EBTDescriptionVerbosity::Type Verbosity, TArray<FString>& Values) const
{
	Super::DescribeRuntimeValues(OwnerComp, NodeMemory, Verbosity, Values);

	if (Verbosity == EBTDescriptionVerbosity::Detailed)
	{
		FBTEnvQueryTaskMemory* MyMemory = (FBTEnvQueryTaskMemory*)NodeMemory;
		Values.Add(FString::Printf(TEXT("request: %d"), MyMemory->RequestID));
	}
}

uint16 UBTTask_RunEQSQuery::GetInstanceMemorySize() const
{
	return sizeof(FBTEnvQueryTaskMemory);
}

void UBTTask_RunEQSQuery::OnQueryFinished(TSharedPtr<FEnvQueryResult> Result)
{
	if (Result->IsAborted())
	{
		return;
	}

	AActor* MyOwner = Cast<AActor>(Result->Owner.Get());
	if (APawn* PawnOwner = Cast<APawn>(MyOwner))
	{
		MyOwner = PawnOwner->GetController();
	}

	UBehaviorTreeComponent* MyComp = MyOwner ? MyOwner->FindComponentByClass<UBehaviorTreeComponent>() : NULL;
	if (!MyComp)
	{
		UE_LOG(LogBehaviorTree, Warning, TEXT("Unable to find behavior tree to notify about finished query from %s!"), *GetNameSafe(MyOwner));
		return;
	}

	bool bSuccess = (Result->Items.Num() >= 1);
	if (bSuccess)
	{
		UBlackboardComponent* MyBlackboard = MyComp->GetBlackboardComponent();
		UEnvQueryItemType* ItemTypeCDO = (UEnvQueryItemType*)Result->ItemType->GetDefaultObject();

		bSuccess = ItemTypeCDO->StoreInBlackboard(BlackboardKey, MyBlackboard, Result->RawData.GetData() + Result->Items[0].DataOffset);		
		if (!bSuccess)
		{
			UE_VLOG(MyOwner, LogBehaviorTree, Warning, TEXT("Failed to store query result! item:%s key:%s"),
				*UEnvQueryTypes::GetShortTypeName(Result->ItemType).ToString(),
				*UBehaviorTreeTypes::GetShortTypeName(BlackboardKey.SelectedKeyType));
		}
	}

	FAIMessage::Send(MyComp, FAIMessage(UBrainComponent::AIMessage_QueryFinished, this, Result->QueryID, bSuccess));
}

void UBTTask_RunEQSQuery::CollectKeyFilters()
{
	for (int32 TypeIndex = 0; TypeIndex < UEnvQueryManager::RegisteredItemTypes.Num(); TypeIndex++)
	{
		UEnvQueryItemType* ItemTypeCDO = (UEnvQueryItemType*)UEnvQueryManager::RegisteredItemTypes[TypeIndex]->GetDefaultObject();
		ItemTypeCDO->AddBlackboardFilters(BlackboardKey, this);
	}
}

#if WITH_EDITOR
void UBTTask_RunEQSQuery::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	if (PropertyChangedEvent.Property &&
		PropertyChangedEvent.Property->GetFName() == GET_MEMBER_NAME_CHECKED(UBTTask_RunEQSQuery, QueryTemplate))
	{
		if (QueryTemplate)
		{
			QueryTemplate->CollectQueryParams(QueryParams);
		}
	}
}

FName UBTTask_RunEQSQuery::GetNodeIconName() const
{
	return FName("BTEditor.Graph.BTNode.Task.RunEQSQuery.Icon");
}

#endif

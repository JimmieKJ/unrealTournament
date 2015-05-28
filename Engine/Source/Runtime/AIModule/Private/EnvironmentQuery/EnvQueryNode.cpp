// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "EnvironmentQuery/EnvQueryNode.h"
#include "EnvironmentQuery/EnvQueryManager.h"

UEnvQueryNode::UEnvQueryNode(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UEnvQueryNode::UpdateNodeVersion()
{
	VerNum = 0;
}

FText UEnvQueryNode::GetDescriptionTitle() const
{
	return UEnvQueryTypes::GetShortTypeName(this);
}

FText UEnvQueryNode::GetDescriptionDetails() const
{
	return FText::GetEmpty();
}

#if WITH_EDITOR && USE_EQS_DEBUGGER
void UEnvQueryNode::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
	UEnvQueryManager::NotifyAssetUpdate(NULL);
}
#endif //WITH_EDITOR && USE_EQS_DEBUGGER

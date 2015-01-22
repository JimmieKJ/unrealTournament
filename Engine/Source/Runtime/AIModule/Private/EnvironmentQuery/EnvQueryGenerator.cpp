// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"

UEnvQueryGenerator::UEnvQueryGenerator(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UEnvQueryGenerator::UpdateGeneratorVersion()
{
	VerNum = EnvQueryGeneratorVersion::Latest;
}

FText UEnvQueryGenerator::GetDescriptionTitle() const
{
	return UEnvQueryTypes::GetShortTypeName(this);
}

FText UEnvQueryGenerator::GetDescriptionDetails() const
{
	return FText::GetEmpty();
}

#if WITH_EDITOR && USE_EQS_DEBUGGER
void UEnvQueryGenerator::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) 
{
	Super::PostEditChangeProperty(PropertyChangedEvent);
#if USE_EQS_DEBUGGER
	UEnvQueryManager::NotifyAssetUpdate(NULL);
#endif
}
#endif //WITH_EDITOR && USE_EQS_DEBUGGER

void UEnvQueryGenerator::PostLoad()
{
	Super::PostLoad();
	UpdateGeneratorVersion();
}
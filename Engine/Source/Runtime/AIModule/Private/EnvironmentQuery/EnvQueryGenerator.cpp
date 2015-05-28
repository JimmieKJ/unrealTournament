// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "EnvironmentQuery/EnvQueryManager.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"

UEnvQueryGenerator::UEnvQueryGenerator(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

void UEnvQueryGenerator::UpdateNodeVersion()
{
	VerNum = EnvQueryGeneratorVersion::Latest;
}

void UEnvQueryGenerator::PostLoad()
{
	Super::PostLoad();
	UpdateNodeVersion();
}

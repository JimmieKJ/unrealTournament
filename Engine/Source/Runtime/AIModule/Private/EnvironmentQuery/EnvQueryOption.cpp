// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "AIModulePrivate.h"
#include "EnvironmentQuery/EnvQueryGenerator.h"
#include "EnvironmentQuery/EnvQueryOption.h"

#define LOCTEXT_NAMESPACE "EnvQueryGenerator"

UEnvQueryOption::UEnvQueryOption(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer)
{
}

FText UEnvQueryOption::GetDescriptionTitle() const
{
	return Generator ? Generator->GetDescriptionTitle() : FText::GetEmpty();
}

FText UEnvQueryOption::GetDescriptionDetails() const
{
	FText DetailsDesc = FText::GetEmpty();
	if (Generator)
	{
		FText OptionDetails = Generator->GetDescriptionDetails();
		DetailsDesc = Generator->bAutoSortTests ? OptionDetails :
			FText::Format(FText::FromString("{0}\n{1}"), OptionDetails, LOCTEXT("NoSortMode", "TEST SORTING DISABLED"));
	}

	return DetailsDesc;
}

#undef LOCTEXT_NAMESPACE

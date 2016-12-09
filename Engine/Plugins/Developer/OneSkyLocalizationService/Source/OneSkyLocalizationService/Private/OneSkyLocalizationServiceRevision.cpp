// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "OneSkyLocalizationServiceRevision.h"

#define LOCTEXT_NAMESPACE "OneSkyLocalizationService"

int32 FOneSkyLocalizationServiceRevision::GetRevisionNumber() const
{
	return RevisionNumber;
}

const FString& FOneSkyLocalizationServiceRevision::GetRevision() const
{
	return Revision;
}

const FString& FOneSkyLocalizationServiceRevision::GetUserName() const
{
	return UserName;
}

const FDateTime& FOneSkyLocalizationServiceRevision::GetDate() const
{
	return Date;
}

#undef LOCTEXT_NAMESPACE

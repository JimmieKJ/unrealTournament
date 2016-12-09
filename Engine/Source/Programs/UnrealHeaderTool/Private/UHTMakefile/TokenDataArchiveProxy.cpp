// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "TokenDataArchiveProxy.h"
#include "UnrealHeaderTool.h"
#include "ParserHelper.h"
#include "UHTMakefile.h"
#include "TokenArchiveProxy.h"

FTokenDataArchiveProxy::FTokenDataArchiveProxy(const FUHTMakefile& UHTMakefile, const FTokenData* TokenData)
{
	TokenIndex = UHTMakefile.GetTokenIndex(&TokenData->Token);
}

void FTokenDataArchiveProxy::AddReferencedNames(const FTokenData* TokenData, FUHTMakefile& UHTMakefile)
{ }

FTokenData* FTokenDataArchiveProxy::CreateTokenData(const FUHTMakefile& UHTMakefile) const
{
	return new FTokenData();
}

void FTokenDataArchiveProxy::Resolve(FTokenData& TokenData, const FUHTMakefile& UHTMakefile) const
{
	TokenData.Token = *UHTMakefile.GetTokenByIndex(TokenIndex);
}

FArchive& operator<<(FArchive& Ar, FTokenDataArchiveProxy& TokenDataArchiveProxy)
{
	Ar << TokenDataArchiveProxy.TokenIndex;
	return Ar;
}

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	EmptyQuery.cpp: Empty query RHI implementation.
=============================================================================*/

#include "EmptyRHIPrivate.h"


FEmptyRenderQuery::FEmptyRenderQuery(ERenderQueryType InQueryType)
{

}

FEmptyRenderQuery::~FEmptyRenderQuery()
{

}

void FEmptyRenderQuery::Begin()
{

}

void FEmptyRenderQuery::End()
{

}






FRenderQueryRHIRef FEmptyDynamicRHI::RHICreateRenderQuery(ERenderQueryType QueryType)
{
	return new FEmptyRenderQuery(QueryType);
}

void FEmptyDynamicRHI::RHIResetRenderQuery(FRenderQueryRHIParamRef QueryRHI)
{
	DYNAMIC_CAST_EMPTYRESOURCE(RenderQuery,Query);

}

bool FEmptyDynamicRHI::RHIGetRenderQueryResult(FRenderQueryRHIParamRef QueryRHI,uint64& OutNumPixels,bool bWait)
{
	DYNAMIC_CAST_EMPTYRESOURCE(RenderQuery,Query);

	return false;
}

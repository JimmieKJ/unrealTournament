// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "SeparableSSS.h"
#include "RendererInterface.h"
#include "Engine/SubDSurface.h"

DEFINE_LOG_CATEGORY_STATIC(LogSubDSurface, Log, All);

// ------------------------------------------------------

UVertexAttributeStream::UVertexAttributeStream(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, AttributeType(VAST_unknown)
{
}

FVector* UVertexAttributeStream::CreateFVectorUninitialized(uint32 Count)
{
	AttributeType = EVertexAttributeStreamType::VAST_float3;

	// can be optimized
	Data.Empty();
	Data.AddUninitialized(sizeof(FVector) * Count);

	return (FVector*)Data.GetData();
}

FVector2D* UVertexAttributeStream::CreateFVector2DUninitialized(uint32 Count)
{
	AttributeType = EVertexAttributeStreamType::VAST_float2;

	// can be optimized
	Data.Empty();
	Data.AddUninitialized(sizeof(FVector2D) * Count);

	return (FVector2D*)Data.GetData();
}

FVector* UVertexAttributeStream::GetFVector(uint32& OutCount)
{
	if(AttributeType == EVertexAttributeStreamType::VAST_float3)
	{
		OutCount = Data.Num() / sizeof(FVector);
		return (FVector*)Data.GetData();
	}

	OutCount = 0;
	return 0;
}

FVector2D* UVertexAttributeStream::GetFVector2D(uint32& OutCount)
{
	if(AttributeType == EVertexAttributeStreamType::VAST_float2)
	{
		OutCount = Data.Num() / sizeof(FVector2D);
		return (FVector2D*)Data.GetData();
	}

	OutCount = 0;
	return 0;
}


uint32 UVertexAttributeStream::GetTypeSize() const
{
	switch(AttributeType)
	{
		case EVertexAttributeStreamType::VAST_float:
			return sizeof(float);

		case EVertexAttributeStreamType::VAST_float2:
			return sizeof(FVector2D);

		case EVertexAttributeStreamType::VAST_float3:
			return sizeof(FVector);

		case EVertexAttributeStreamType::VAST_float4:
			return sizeof(FVector4);
	}

	check(0);
	return 0;
}

uint32 UVertexAttributeStream::Num() const
{
	if(EVertexAttributeStreamType::VAST_unknown)
	{
		return 0;
	}

	return Data.Num() / GetTypeSize();
}

// ------------------------------------------------------

USubDSurface::USubDSurface(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}




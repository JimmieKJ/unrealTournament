// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "UObject/TlsObjectInitializers.h"

TArray<FObjectInitializer*>* FTlsObjectInitializers::GetTlsObjectInitializers()
{
	/** Thread local slot for component overrides. */
	static uint32 ComponentOverridesTlsId = FPlatformTLS::AllocTlsSlot();

	auto TLS = static_cast<TArray<FObjectInitializer*>*>(FPlatformTLS::GetTlsValue(ComponentOverridesTlsId));
	if (!TLS)
	{
		TLS = new TArray<FObjectInitializer*>();
		FPlatformTLS::SetTlsValue(ComponentOverridesTlsId, TLS);
	}

	return TLS;
}

FObjectInitializer* FTlsObjectInitializers::Top()
{
	auto ObjectInitializers = GetTlsObjectInitializers();
	return ObjectInitializers->Last();
}

void FTlsObjectInitializers::Pop()
{
	auto ObjectInitializers = GetTlsObjectInitializers();
	ObjectInitializers->RemoveAt(ObjectInitializers->Num() - 1);
}

void FTlsObjectInitializers::Push(FObjectInitializer* Initializer)
{
	auto ObjectInitializers = GetTlsObjectInitializers();
	ObjectInitializers->Push(Initializer);
}
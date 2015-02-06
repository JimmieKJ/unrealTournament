// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "CoreUObjectPrivate.h"
#include "UObject/TlsObjectInitializers.h"
#include "AssertionMacros.h"

DECLARE_LOG_CATEGORY_EXTERN(LogTlsObjectInitializers, Log, All);
DEFINE_LOG_CATEGORY(LogTlsObjectInitializers);

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
	return ObjectInitializers->Num() ? ObjectInitializers->Last() : nullptr;
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

FObjectInitializer& FTlsObjectInitializers::TopChecked()
{
	auto* ObjectInitializerPtr = Top();
	UE_CLOG(!ObjectInitializerPtr, LogTlsObjectInitializers, Fatal, TEXT("Tried to get the current ObjectInitializer, but none is set. Please use NewObject or NewNamedObject to construct new UObject-derived classes."));
	return *ObjectInitializerPtr;
}

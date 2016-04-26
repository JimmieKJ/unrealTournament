// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UnrealHeaderTool.h"
#include "UHTMakefile/UHTMakefile.h"
#include "UHTMakefile/FunctionArchiveProxy.h"

FFunctionArchiveProxy::FFunctionArchiveProxy(FUHTMakefile& UHTMakefile, const UFunction* Function)
	: FStructArchiveProxy(UHTMakefile, Function)
{
	FunctionFlags = Function->FunctionFlags;
	RepOffset = Function->RepOffset;
	NumParms = Function->NumParms;
	ParmsSize = Function->ParmsSize;
	ReturnValueOffset = Function->ReturnValueOffset;
	RPCId = Function->RPCId;
	RPCResponseId = Function->RPCResponseId;
	FirstPropertyToInitIndex = UHTMakefile.GetPropertyIndex(Function->FirstPropertyToInit);
#if UE_BLUEPRINT_EVENTGRAPH_FASTCALLS
	EventGraphFunctionIndex = UHTMakefile.GetFunctionIndex(Function->EventGraphFunction);
	EventGraphCallOffset = Function->EventGraphCallOffset;
#else
	EventGraphFunctionIndex = -1;
	EventGraphCallOffset = -1;
#endif // UE_BLUEPRINT_EVENTGRAPH_FASTCALLS
}

UFunction* FFunctionArchiveProxy::CreateFunction(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UFunction* Function = new(EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UFunction(FObjectInitializer());
	PostConstruct(Function);
	return Function;
}

void FFunctionArchiveProxy::PostConstruct(UFunction* Function) const
{
	Function->FunctionFlags = FunctionFlags;
	Function->RepOffset = RepOffset;
	Function->NumParms = NumParms;
	Function->ParmsSize = ParmsSize;
	Function->ReturnValueOffset = ReturnValueOffset;
	Function->RPCId = RPCId;
	Function->RPCResponseId = RPCResponseId;
	Function->EventGraphCallOffset = EventGraphCallOffset;
}

void FFunctionArchiveProxy::Resolve(UFunction* Function, const FUHTMakefile& UHTMakefile) const
{
	Function->FirstPropertyToInit = UHTMakefile.GetPropertyByIndex(FirstPropertyToInitIndex);
#if UE_BLUEPRINT_EVENTGRAPH_FASTCALLS
	Function->EventGraphFunction = UHTMakefile.GetFunctionByIndex(EventGraphFunctionIndex);
#else
	Function->EventGraphFunction = nullptr;
#endif // UE_BLUEPRINT_EVENTGRAPH_FASTCALLS
}

FArchive& operator<<(FArchive& Ar, FFunctionArchiveProxy& FunctionArchiveProxy)
{
	Ar << static_cast<FStructArchiveProxy&>(FunctionArchiveProxy);
	Ar << FunctionArchiveProxy.FunctionFlags;
	Ar << FunctionArchiveProxy.RepOffset;
	Ar << FunctionArchiveProxy.NumParms;
	Ar << FunctionArchiveProxy.ParmsSize;
	Ar << FunctionArchiveProxy.ReturnValueOffset;
	Ar << FunctionArchiveProxy.RPCId;
	Ar << FunctionArchiveProxy.RPCResponseId;
	Ar << FunctionArchiveProxy.FirstPropertyToInitIndex;
	Ar << FunctionArchiveProxy.EventGraphFunctionIndex;
	Ar << FunctionArchiveProxy.EventGraphCallOffset;

	return Ar;
}

FDelegateFunctionArchiveProxy::FDelegateFunctionArchiveProxy(FUHTMakefile& UHTMakefile, const UDelegateFunction* DelegateFunction)
	: FFunctionArchiveProxy(UHTMakefile, DelegateFunction)
{ }

UDelegateFunction* FDelegateFunctionArchiveProxy::CreateDelegateFunction(const FUHTMakefile& UHTMakefile) const
{
	UObject* Outer = UHTMakefile.GetObjectByIndex(OuterIndex);
	UDelegateFunction* DelegateFunction = new(EC_InternalUseOnlyConstructor, Outer, Name.CreateName(UHTMakefile), (EObjectFlags)ObjectFlagsUint32) UDelegateFunction(FObjectInitializer());
	PostConstruct(DelegateFunction);
	return DelegateFunction;
}

void FDelegateFunctionArchiveProxy::PostConstruct(UDelegateFunction* DelegateFunction) const
{
	FFunctionArchiveProxy::PostConstruct(DelegateFunction);
}

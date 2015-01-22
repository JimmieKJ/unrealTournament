// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"

UBlueprintFunctionLibrary::UBlueprintFunctionLibrary(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

int32 UBlueprintFunctionLibrary::GetFunctionCallspace(UFunction* Function, void* Parameters, FFrame* Stack)
{
	// This is really awkward and totally sucks.
	//
	// This used to call GEngine->GetNetMode. With multiple netdrivers and worlds, this no longer makes sense.
	//
	// Without an actor or world to give us context, we don't know what netdriver to look at for the netmode.
	//
	// We could ban KismetAuthorityOnly for static functions, and force the authority check to be done manually in the function.
	// This is awkward though since we lose the automatic 'server only' icon in kismet, which is pretty important.
	//
	// For now, we will use globabls to indicate what world we are in, and essentially default to GWorld and GameNetDriver to 
	// see whether we should absorb the function call.

	const bool Absorb = (Function->HasAllFunctionFlags(FUNC_BlueprintAuthorityOnly) && GEngine->ShouldAbsorbAuthorityOnlyEvent()) ||
						(Function->HasAllFunctionFlags(FUNC_BlueprintCosmetic) && GEngine->ShouldAbsorbCosmeticOnlyEvent());
	return Absorb ? FunctionCallspace::Absorbed : FunctionCallspace::Local;
}

// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	CoreNative.h: Native function lookup table.
=============================================================================*/

#pragma once

extern COREUOBJECT_API Native GCasts[];
uint8 COREUOBJECT_API GRegisterCast( int32 CastCode, const Native& Func );


/** A struct that maps a string name to a native function */
struct FNativeFunctionRegistrar
{
	FNativeFunctionRegistrar(class UClass* Class, const ANSICHAR* InName, Native InPointer)
	{
		RegisterFunction(Class, InName, InPointer);
	}
	static COREUOBJECT_API void RegisterFunction(class UClass* Class, const ANSICHAR* InName, Native InPointer);
	// overload for types generated from blueprints, which can have unicode names:
	static COREUOBJECT_API void RegisterFunction(class UClass* Class, const WIDECHAR* InName, Native InPointer);
};

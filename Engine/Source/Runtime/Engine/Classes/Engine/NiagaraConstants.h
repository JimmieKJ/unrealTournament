// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/**  List of built-in constants
*	The values of these enums correspond directly to indices for the compiler and interpreter
*/
namespace NiagaraConstants
{
	enum {
		DeltaSeconds = 1,
		EmitterPosition,
		EmitterAge,
		ActorXAxis,
		ActorYAxis,
		ActorZAxis,
		NumBuiltinConstants
	} ConstantDef;

	FName ConstantNames[] = {
		"Delta Time",
		"Emitter position",
		"Emitter age",
		"Emitter X Axis",
		"Emitter Y Axis",
		"Emitter Z Axis",
		""
	};
};


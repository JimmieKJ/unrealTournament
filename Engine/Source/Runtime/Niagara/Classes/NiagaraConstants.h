// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

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

	FName GConstantNames[] = {
		"Undefined",
		"Delta Time",
		"Emitter Position",
		"Emitter Age",
		"Emitter X Axis",
		"Emitter Y Axis",
		"Emitter Z Axis",
		""
	};
};

#define BUILTIN_CONST_DELTATIME  NiagaraConstants::GConstantNames[NiagaraConstants::DeltaSeconds]
#define BUILTIN_CONST_EMITTERPOS  NiagaraConstants::GConstantNames[NiagaraConstants::EmitterPosition]
#define BUILTIN_CONST_EMITTERAGE  NiagaraConstants::GConstantNames[NiagaraConstants::EmitterAge]
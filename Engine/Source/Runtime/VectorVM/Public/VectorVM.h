// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Core.h"
#include "CoreUObject.h"

class FNiagaraDataObject;

namespace VectorVM
{
	/** Constants. */
	enum
	{
		NumTempRegisters = 100,
		MaxInputRegisters = 100,
		MaxOutputRegisters = MaxInputRegisters,
		MaxConstants = 256,
		FirstInputRegister = NumTempRegisters,
		FirstOutputRegister = FirstInputRegister + MaxInputRegisters,
		FirstConstantRegister = FirstOutputRegister + MaxOutputRegisters,
		MaxRegisters = NumTempRegisters + MaxInputRegisters + MaxOutputRegisters + MaxConstants,
	};

	enum EOperandType
	{
		UndefinedOperandType = 0,
		RegisterOperandType,
		ConstantOperandType,
		DataObjConstantOperandType,
		TemporaryOperandType
	};

	/** List of opcodes supported by the VM. */
	enum class EOp
	{
			done,
			add,
			sub,
			mul,
			mad,
			lerp,
			rcp,
			rsq,
			sqrt,
			neg,
			abs,
			exp,
			exp2,
			log,
			log2,
			sin,
			cos,
			tan,
			asin,
			acos,
			atan,
			atan2,
			ceil,
			floor,
			fmod,
			frac,
			trunc,
			clamp,
			min,
			max,
			pow,
			sign,
			step,
			dot,
			cross,
			normalize,
			random,
			length,
			noise,
			splatx,
			splaty,
			splatz,
			splatw,
			compose,
			composex,
			composey,
			composez,
			composew,
			output,
			lessthan,
			sample,
			bufferwrite,
			eventbroadcast,
			easein,
			easeinout,
			NumOpcodes
	};

	/** Get total number of op-codes */
	VECTORVM_API uint8 GetNumOpCodes();

	VECTORVM_API uint8 CreateSrcOperandMask(bool bIsOp0Constant, bool bIsOp1Constant = false, bool bIsOp2Constant = false, bool bIsOp3Constant = false);
	VECTORVM_API uint8 CreateSrcOperandMask(VectorVM::EOperandType Type1, VectorVM::EOperandType Type2 = VectorVM::RegisterOperandType, VectorVM::EOperandType Type3 = VectorVM::RegisterOperandType, VectorVM::EOperandType Type4 = VectorVM::RegisterOperandType);

	/**
	 * Execute VectorVM bytecode.
	 */
	VECTORVM_API void Exec(
		uint8 const* Code,
		VectorRegister** InputRegisters,
		int32 NumInputRegisters,
		VectorRegister** OutputRegisters,
		int32 NumOutputRegisters,
		FVector4 const* ConstantTable,
		FNiagaraDataObject* *DataObjTable,
		int32 NumVectors
		);

	VECTORVM_API void Init();


} // namespace VectorVM



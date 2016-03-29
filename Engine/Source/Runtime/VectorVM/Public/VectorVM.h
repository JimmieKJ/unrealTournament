// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "Core.h"
#include "CoreUObject.h"

//TODO: move to a per platform header and have VM scale vectorization according to vector width.
#define VECTOR_WIDTH (128)
#define VECTOR_WIDTH_BYTES (16)
#define VECTOR_WIDTH_FLOATS (4)

class UNiagaraDataObject;
class FVectorVMSharedDataView;
struct FVectorVMContext;
class FVectorVMDebuggerImpl;

UENUM()
enum class EVectorVMOperandLocation : uint8
{
	TemporaryRegister,
	InputRegister,
	OutputRegister,
	Constant,
	DataObjConstant,
	SharedData,
	Undefined,
	Num
};

UENUM()
enum class EVectorVMOp : uint8
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
	select,
	sample,
	bufferwrite,
	easein,
	easeinout,
	div,
	aquireshareddataindex,
	aquireshareddataindexwrap,
	consumeshareddataindex,
	consumeshareddataindexwrap,
	shareddataread,
	shareddatawrite,
	shareddataindexvalid,
	NumOpcodes
};

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

	/** Get total number of op-codes */
	VECTORVM_API uint8 GetNumOpCodes();

#if WITH_EDITOR
	VECTORVM_API FString GetOpName(EVectorVMOp Op);
	VECTORVM_API FString GetOperandLocationName(EVectorVMOperandLocation Location);
#endif

	VECTORVM_API uint8 CreateSrcOperandMask(EVectorVMOperandLocation Type1, EVectorVMOperandLocation Type2 = EVectorVMOperandLocation::TemporaryRegister, EVectorVMOperandLocation Type3 = EVectorVMOperandLocation::TemporaryRegister, EVectorVMOperandLocation Type4 = EVectorVMOperandLocation::TemporaryRegister);

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
		UNiagaraDataObject* *DataObjTable,
		FVectorVMSharedDataView* SharedDataTable,
		int32 NumVectors
		);

	VECTORVM_API void Init();

	//////////////////////////////////////////////////////////////////////////
	//Debugging

	//VM Deubgger is currently broken. I wrote it and had it working on win 64 and unknowingly used some C++11 features not supported by the XBone compiler.
	//I've had to hack the ENABLE_VM_DEBUGGING (0) code to get this to build but I've not done the ENABLE_VM_DEBUGGING (1) path as of yet.
#define ENABLE_VM_DEBUGGING (0)
//#define ENABLE_VM_DEBUGGING (1)

#define MAX_VM_OP_ARGS (4)
#define NUM_VM_OP_DEBUG_VALUES (MAX_VM_OP_ARGS + 1)
#define MAX_INSTANCES_PER_VM_OP (4)//8 and 16 in future

	enum class EVMType : uint8
	{
		//Float,
		//Int32,
		Vector4,
	};

	enum class EKernelArgs : uint8
	{
		Dest,
		Arg0,
		Arg1,
		Arg2,
		Arg3,
		Max,
	};

#if ENABLE_VM_DEBUGGING

	struct FDebugValue
	{
		FVector4 v;//TODO, replace with int/float union?

		void Set(float Val)
		{
			v = FVector4(Val, Val, Val, Val);
		}
		void Set(int32 Val)
		{
			int32* i = (int32*)&v;
			i[0] = Val;	i[1] = Val; i[2] = Val; i[3] = Val;
		}
		void Set(VectorRegister Val)
		{
			VectorStoreAligned(Val, (float*)&v);
		}
		template<typename T>
		void Set(T Val)
		{
			v = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
		}
	};

	struct FDataLocationInfo
	{
		EVectorVMOperandLocation Location;
		int32 PrimaryLocationIndex;
		int32 SecondaryLocationIndex;
		int32 TertiaryLocationIndex;
		const void* MemoryAddress;

		FDataLocationInfo(FVectorVMContext& Context, EVectorVMOperandLocation InLocation, int32 InPrimaryIndex, int32 InSecondaryIndex = INDEX_NONE, int32 InTertiaryIndex = INDEX_NONE);
		FDataLocationInfo()
			: Location(EVectorVMOperandLocation::Undefined)
			, PrimaryLocationIndex(INDEX_NONE)
			, SecondaryLocationIndex(INDEX_NONE)
			, TertiaryLocationIndex(INDEX_NONE)
			, MemoryAddress(NULL)
		{}
	};

	struct VECTORVM_API FOpDebugInfo
	{
		EVectorVMOp Op;
		EVMType OpType;
		int32 NumArgs;

		FDebugValue PreOpValues[NUM_VM_OP_DEBUG_VALUES];
		FDebugValue PostOpValues[NUM_VM_OP_DEBUG_VALUES];
		FDataLocationInfo LocationInfo[NUM_VM_OP_DEBUG_VALUES];
	};

	class VECTORVM_API FVectorVMDebugger
	{
	private:
		FVectorVMDebuggerImpl* Impl;

	public:
		FVectorVMDebugger();
		~FVectorVMDebugger();
		void AddInstanceToGather(int32 Instance);
		const TArray<FOpDebugInfo>* GetDebugInfoForInstance(int32 Instance);
		FVectorVMDebuggerImpl* GetImpl();
		void InitForScriptRun(int32 StartInstance, const TArray<int32> InstancesToDebug);
	};

	VECTORVM_API void AttachDebugger(FVectorVMDebugger* Debugger);
	VECTORVM_API void DetachDebugger(FVectorVMDebugger* Debugger);
#endif

} // namespace VectorVM


/**
Encapsulates a view of a number of shared data buffers with a shared counter and size.
This is temporary until we can use integers in the constants for the size and counter.
*/
class FVectorVMSharedDataView
{
private:
	/** Array of data buffers.*/
	TArray<FVector4*> Buffers;
	/** Total size of buffer in vectors. */
	int32 Size;
	/** Counter used to acquire and release slots for reading and writing. */
	int32 Counter;//Need to ensure this is atomically updated if threading withing a single VM script run. Though I don't think we should do that.

	// Dummy read and write data for when the VM requests an invalid buffer pointer.
	// This can happen because we have no branching currently. Writing to OOB indices must still continue but just not do any damage.
	FVector4 DummyRead;
	FVector4 DummyWrite;

public:
	FVectorVMSharedDataView(int32 InSize, int32 InCounter)
		: Size(InSize)
		, Counter(InCounter)
		, DummyRead(FVector4(0.0f, 0.0f, 0.0f, 0.0f))
	{
	}

	FORCEINLINE bool ValidIndex(int32 Index){ return Index >= 0 && Index < Size; }

	FORCEINLINE FVector4* GetReadBuffer(int32 BufferIndex, int32 DataIndex) { return ValidIndex(DataIndex) && Buffers[BufferIndex] ? Buffers[BufferIndex] + DataIndex : &DummyRead; }
	FORCEINLINE FVector4* GetWriteBuffer(int32 BufferIndex, int32 DataIndex) { return ValidIndex(DataIndex) && Buffers[BufferIndex] ? Buffers[BufferIndex] + DataIndex : &DummyWrite; }

	FORCEINLINE void AddBuffer(FVector4* Buffer)
	{
		Buffers.Add(Buffer);
	}

	FORCEINLINE int32 GetCounter(){ return Counter; }
	FORCEINLINE int32 GetSize(){ return Size; }

	FORCEINLINE int32 AcquireIndexWrap(){ int32 Index = Counter; IncrementWrap(); return Index; }
	FORCEINLINE int32 AcquireIndex(){ int32 Index = Counter; Increment(); return Index; }

	FORCEINLINE int32 ConsumeIndexWrap(){ int32 Index = Counter; DecrementWrap(); return Index; }
	FORCEINLINE int32 ConsumeIndex(){ int32 Index = Counter; Decrement(); return Index; }

	FORCEINLINE void Increment(){ Counter = FMath::Min(Counter + 1, Size); }
	FORCEINLINE void Decrement(){ Counter = FMath::Max(Counter - 1, 0); }

	FORCEINLINE int32 Modulo(int32 X, int32 Max)
	{
		return Max == 0
			? 0
			: (X < 0 ? (X % Max) + Max : (X % Max));
	}
	FORCEINLINE void IncrementWrap(){ Counter = Size == 0 ? INDEX_NONE : Modulo(Counter + 1, Size - 1); }
	FORCEINLINE void DecrementWrap(){ Counter = Size == 0 ? INDEX_NONE : Modulo(Counter - 1, Size - 1); }
};


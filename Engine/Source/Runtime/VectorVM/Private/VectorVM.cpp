// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VectorVMPrivate.h"
#include "CurveVector.h"
#include "VectorVMDataObject.h"
#include "ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, VectorVM);

DEFINE_LOG_CATEGORY_STATIC(LogVectorVM, All, All);

//#define VM_FORCEINLINE 
#define VM_FORCEINLINE FORCEINLINE

#define SRCOP_RRRR 0x00
#define SRCOP_RRRC 0x01
#define SRCOP_RRCR 0x02
#define SRCOP_RRCC 0x03
#define SRCOP_RCRR 0x04
#define SRCOP_RCRC 0x05
#define SRCOP_RCCR 0x06
#define SRCOP_RCCC 0x07
#define SRCOP_CRRR 0x08
#define SRCOP_CRRC 0x09
#define SRCOP_CRCR 0x0a
#define SRCOP_CRCC 0x0b
#define SRCOP_CCRR 0x0c
#define SRCOP_CCRC 0x0d
#define SRCOP_CCCR 0x0e
#define SRCOP_CCCC 0x0f

#define SRCOP_RRRB 0x10
#define SRCOP_RRBR 0x20
#define SRCOP_RRBB 0x30







/**
 * Context information passed around during VM execution.
 */
struct FVectorVMContext
{
	/** Pointer to the next element in the byte code. */
	uint8 const* RESTRICT Code;
	/** Pointer to the table of vector register arrays. */
	VectorRegister* RESTRICT * RESTRICT RegisterTable;
	/** Pointer to the constant table. */
	FVector4 const* RESTRICT ConstantTable;
	/** Pointer to the data object constant table. */
	FNiagaraDataObject * RESTRICT *DataObjConstantTable;

	/** The number of vectors to process. */
	int32 NumVectors;

	/** Initialization constructor. */
	FVectorVMContext(
		const uint8* InCode,
		VectorRegister** InRegisterTable,
		const FVector4* InConstantTable,
		FNiagaraDataObject** InDataObjTable,
		int32 InNumVectors
		)
		: Code(InCode)
		, RegisterTable(InRegisterTable)
		, ConstantTable(InConstantTable)
		, DataObjConstantTable(InDataObjTable)
		, NumVectors(InNumVectors)
	{
	}
};




/** Decode the next operation contained in the bytecode. */
static VM_FORCEINLINE VectorVM::EOp DecodeOp(FVectorVMContext& Context)
{
	return static_cast<VectorVM::EOp>(*Context.Code++);
}

/** Decode a register from the bytecode. */
static VM_FORCEINLINE VectorRegister* DecodeRegister(FVectorVMContext& Context)
{
	return Context.RegisterTable[*Context.Code++];
}

/** Decode a constant from the bytecode. */
static VM_FORCEINLINE VectorRegister DecodeConstant(FVectorVMContext& Context)
{
	const FVector4* vec = &Context.ConstantTable[*Context.Code++];
	return VectorLoad(vec);
}

/** Decode a constant from the bytecode. */
static /*VM_FORCEINLINE*/ const FNiagaraDataObject *DecodeDataObjConstant(FVectorVMContext& Context)
{
	const FNiagaraDataObject* Obj = Context.DataObjConstantTable[*Context.Code++];
	return Obj;
}

/** Decode a constant from the bytecode. */
static /*VM_FORCEINLINE*/ FNiagaraDataObject *DecodeWritableDataObjConstant(FVectorVMContext& Context)
{
	FNiagaraDataObject* Obj = Context.DataObjConstantTable[*Context.Code++];
	return Obj;
}


static VM_FORCEINLINE uint8 DecodeSrcOperandTypes(FVectorVMContext& Context)
{
	return *Context.Code++;
}

static VM_FORCEINLINE uint8 DecodeByte(FVectorVMContext& Context)
{
	return *Context.Code++;
}



/** Handles reading of a constant. */
struct FConstantHandler
{
	VectorRegister Constant;
	FConstantHandler(FVectorVMContext& Context)
		: Constant(DecodeConstant(Context))
	{}
	VM_FORCEINLINE const VectorRegister& Get(){ return Constant; }
};

/** Handles reading of a data object constant. */
struct FDataObjectConstantHandler
{
	const FNiagaraDataObject *Constant;
	FDataObjectConstantHandler(FVectorVMContext& Context)
		: Constant(DecodeDataObjConstant(Context))
	{}
	VM_FORCEINLINE const FNiagaraDataObject *Get(){ return Constant; }
};

/** Handles reading of a data object constant. */
struct FWritableDataObjectConstantHandler
{
	FNiagaraDataObject *Constant;
	FWritableDataObjectConstantHandler(FVectorVMContext& Context)
		: Constant(DecodeWritableDataObjConstant(Context))
	{}
	VM_FORCEINLINE FNiagaraDataObject *Get(){ return Constant; }
};


/** Handles reading of a register, advancing the pointer with each read. */
struct FRegisterHandler
{
	VectorRegister* Register;
	VectorRegister RegisterValue;
	FRegisterHandler(FVectorVMContext& Context)
		: Register(DecodeRegister(Context))
	{}
	VM_FORCEINLINE const VectorRegister& Get(){ RegisterValue = *Register++;  return RegisterValue; }
};

template<typename Kernel, typename Arg0Handler>
VM_FORCEINLINE void VectorUnaryLoop(FVectorVMContext& Context, VectorRegister* RESTRICT Dst, int32 NumVectors)
{
	Arg0Handler Arg0(Context);
	for (int32 i = 0; i < NumVectors; ++i)
	{
		Kernel::DoKernel(Dst++, Arg0.Get());
	}
}

template<typename Kernel, typename Arg0Handler, typename Arg1Handler>
VM_FORCEINLINE void VectorBinaryLoop(FVectorVMContext& Context, VectorRegister* RESTRICT Dst, int32 NumVectors)
{
	Arg0Handler Arg0(Context);
	Arg1Handler Arg1(Context);
	for (int32 i = 0; i < NumVectors; ++i)
	{
		Kernel::DoKernel(Dst++, Arg0.Get(), Arg1.Get());
	}
}


template<typename Kernel, typename Arg0Handler, typename Arg1Handler, typename Arg2Handler>
VM_FORCEINLINE void VectorTrinaryLoop(FVectorVMContext& Context, VectorRegister* RESTRICT Dst, int32 NumVectors)
{
	Arg0Handler Arg0(Context);
	Arg1Handler Arg1(Context);
	Arg2Handler Arg2(Context);
	for (int32 i = 0; i < NumVectors; ++i)
	{
		Kernel::DoKernel(Dst++, Arg0.Get(), Arg1.Get(), Arg2.Get());
	}
}

template<typename Kernel, typename Arg0Handler, typename Arg1Handler, typename Arg2Handler, typename Arg3Handler>
VM_FORCEINLINE void VectorQuatenaryLoop(FVectorVMContext& Context, VectorRegister* RESTRICT Dst, int32 NumVectors)
{
	Arg0Handler Arg0(Context);
	Arg1Handler Arg1(Context);
	Arg2Handler Arg2(Context);
	Arg3Handler Arg3(Context);
	for (int32 i = 0; i < NumVectors; ++i)
	{
		Kernel::DoKernel(Dst++, Arg0.Get(), Arg1.Get(), Arg2.Get(), Arg3.Get());
	}
}

/** Base class of vector kernels with a single operand. */
template <typename Kernel>
struct TUnaryVectorKernel
{
	static void Exec(FVectorVMContext& Context)
	{
		VectorRegister* RESTRICT Dst = DecodeRegister(Context);
		uint32 SrcOpTypes = DecodeSrcOperandTypes(Context);
		int32 NumVectors = Context.NumVectors;
		switch (SrcOpTypes)
		{
		case SRCOP_RRRR: 	VectorUnaryLoop<Kernel, FRegisterHandler>(Context, Dst, NumVectors); break;
		case SRCOP_RRRC:	VectorUnaryLoop<Kernel, FConstantHandler>(Context, Dst, NumVectors); break;
		default: check(0); break;
		};
	}
};

/** Base class of Vector kernels with 2 operands. */
template <typename Kernel>
struct TBinaryVectorKernel
{
	static void Exec(FVectorVMContext& Context)
	{
		VectorRegister* RESTRICT Dst = DecodeRegister(Context);
		uint32 SrcOpTypes = DecodeSrcOperandTypes(Context);
		int32 NumVectors = Context.NumVectors;
		switch (SrcOpTypes)
		{
		case SRCOP_RRRR: 	VectorBinaryLoop<Kernel, FRegisterHandler, FRegisterHandler>(Context, Dst, NumVectors); break;
		case SRCOP_RRRC:	VectorBinaryLoop<Kernel, FConstantHandler, FRegisterHandler>(Context, Dst, NumVectors); break;
		case SRCOP_RRCR: 	VectorBinaryLoop<Kernel, FRegisterHandler, FConstantHandler>(Context, Dst, NumVectors); break;
		case SRCOP_RRCC:	VectorBinaryLoop<Kernel, FConstantHandler, FConstantHandler>(Context, Dst, NumVectors); break;
		default: check(0); break;
		};
	}
};




/** Base class of Vector kernels with 2 operands, one of which can be a data object. */
template <typename Kernel>
struct TBinaryVectorKernelData
{
	static void Exec(FVectorVMContext& Context)
	{
		VectorRegister* RESTRICT Dst = DecodeRegister(Context);
		uint32 SrcOpTypes = DecodeSrcOperandTypes(Context);
		int32 NumVectors = Context.NumVectors;
		switch (SrcOpTypes)
		{
		case SRCOP_RRRB:	VectorBinaryLoop<Kernel, FDataObjectConstantHandler, FRegisterHandler>(Context, Dst, NumVectors); break;
		default: check(0); break;
		};
	}
};



/** Base class of Vector kernels with 2 operands, one of which can be a data object. */
template <typename Kernel>
struct TTrinaryVectorKernelData
{
	static void Exec(FVectorVMContext& Context)
	{
		VectorRegister* RESTRICT Dst = DecodeRegister(Context);
		uint32 SrcOpTypes = DecodeSrcOperandTypes(Context);
		int32 NumVectors = Context.NumVectors;
		switch (SrcOpTypes)
		{
		case SRCOP_RRRB:	VectorTrinaryLoop<Kernel, FWritableDataObjectConstantHandler, FRegisterHandler, FRegisterHandler>(Context, Dst, NumVectors); break;
		default: check(0); break;
		};
	}
};




/** Base class of Vector kernels with 3 operands. */
template <typename Kernel>
struct TTrinaryVectorKernel
{
	static void Exec(FVectorVMContext& Context)
	{
		VectorRegister* RESTRICT Dst = DecodeRegister(Context);
		uint32 SrcOpTypes = DecodeSrcOperandTypes(Context);
		int32 NumVectors = Context.NumVectors;
		switch (SrcOpTypes)
		{
		case SRCOP_RRRR: 	VectorTrinaryLoop<Kernel, FRegisterHandler, FRegisterHandler, FRegisterHandler>(Context, Dst, NumVectors); break;
		case SRCOP_RRRC:	VectorTrinaryLoop<Kernel, FConstantHandler, FRegisterHandler, FRegisterHandler>(Context, Dst, NumVectors); break;
		case SRCOP_RRCR: 	VectorTrinaryLoop<Kernel, FRegisterHandler, FConstantHandler, FRegisterHandler>(Context, Dst, NumVectors); break;
		case SRCOP_RRCC:	VectorTrinaryLoop<Kernel, FConstantHandler, FConstantHandler, FRegisterHandler>(Context, Dst, NumVectors); break;
		case SRCOP_RCRR: 	VectorTrinaryLoop<Kernel, FRegisterHandler, FRegisterHandler, FConstantHandler>(Context, Dst, NumVectors); break;
		case SRCOP_RCRC:	VectorTrinaryLoop<Kernel, FConstantHandler, FRegisterHandler, FConstantHandler>(Context, Dst, NumVectors); break;
		case SRCOP_RCCR: 	VectorTrinaryLoop<Kernel, FRegisterHandler, FConstantHandler, FConstantHandler>(Context, Dst, NumVectors); break;
		case SRCOP_RCCC:	VectorTrinaryLoop<Kernel, FConstantHandler, FConstantHandler, FConstantHandler>(Context, Dst, NumVectors); break;
		default: check(0); break;
		};
	}
};

/** Base class of Vector kernels with 4 operands. */
template <typename Kernel>
struct TQuatenaryVectorKernel
{
	static void Exec(FVectorVMContext& Context)
	{
		VectorRegister* RESTRICT Dst = DecodeRegister(Context);
		uint32 SrcOpTypes = DecodeSrcOperandTypes(Context);
		int32 NumVectors = Context.NumVectors;
		switch (SrcOpTypes)
		{
		case SRCOP_RRRR: 	VectorQuatenaryLoop<Kernel, FRegisterHandler, FRegisterHandler, FRegisterHandler, FRegisterHandler>(Context, Dst, NumVectors); break;
		case SRCOP_RRRC:	VectorQuatenaryLoop<Kernel, FConstantHandler, FRegisterHandler, FRegisterHandler, FRegisterHandler>(Context, Dst, NumVectors); break;
		case SRCOP_RRCR: 	VectorQuatenaryLoop<Kernel, FRegisterHandler, FConstantHandler, FRegisterHandler, FRegisterHandler>(Context, Dst, NumVectors); break;
		case SRCOP_RRCC:	VectorQuatenaryLoop<Kernel, FConstantHandler, FConstantHandler, FRegisterHandler, FRegisterHandler>(Context, Dst, NumVectors); break;
		case SRCOP_RCRR: 	VectorQuatenaryLoop<Kernel, FRegisterHandler, FRegisterHandler, FConstantHandler, FRegisterHandler>(Context, Dst, NumVectors); break;
		case SRCOP_RCRC:	VectorQuatenaryLoop<Kernel, FConstantHandler, FRegisterHandler, FConstantHandler, FRegisterHandler>(Context, Dst, NumVectors); break;
		case SRCOP_RCCR: 	VectorQuatenaryLoop<Kernel, FRegisterHandler, FConstantHandler, FConstantHandler, FRegisterHandler>(Context, Dst, NumVectors); break;
		case SRCOP_RCCC:	VectorQuatenaryLoop<Kernel, FConstantHandler, FConstantHandler, FConstantHandler, FRegisterHandler>(Context, Dst, NumVectors); break;
		case SRCOP_CRRR: 	VectorQuatenaryLoop<Kernel, FRegisterHandler, FRegisterHandler, FRegisterHandler, FConstantHandler>(Context, Dst, NumVectors); break;
		case SRCOP_CRRC:	VectorQuatenaryLoop<Kernel, FConstantHandler, FRegisterHandler, FRegisterHandler, FConstantHandler>(Context, Dst, NumVectors); break;
		case SRCOP_CRCR: 	VectorQuatenaryLoop<Kernel, FRegisterHandler, FConstantHandler, FRegisterHandler, FConstantHandler>(Context, Dst, NumVectors); break;
		case SRCOP_CRCC:	VectorQuatenaryLoop<Kernel, FConstantHandler, FConstantHandler, FRegisterHandler, FConstantHandler>(Context, Dst, NumVectors); break;
		case SRCOP_CCRR: 	VectorQuatenaryLoop<Kernel, FRegisterHandler, FRegisterHandler, FConstantHandler, FConstantHandler>(Context, Dst, NumVectors); break;
		case SRCOP_CCRC:	VectorQuatenaryLoop<Kernel, FConstantHandler, FRegisterHandler, FConstantHandler, FConstantHandler>(Context, Dst, NumVectors); break;
		case SRCOP_CCCR: 	VectorQuatenaryLoop<Kernel, FRegisterHandler, FConstantHandler, FConstantHandler, FConstantHandler>(Context, Dst, NumVectors); break;
		case SRCOP_CCCC:	VectorQuatenaryLoop<Kernel, FConstantHandler, FConstantHandler, FConstantHandler, FConstantHandler>(Context, Dst, NumVectors); break;
		default: check(0); break;
		};
	}
};

/*------------------------------------------------------------------------------
	Implementation of all kernel operations.
------------------------------------------------------------------------------*/

struct FVectorKernelAdd : public TBinaryVectorKernel<FVectorKernelAdd>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0,VectorRegister Src1)
	{
		*Dst = VectorAdd(Src0, Src1);
	}
};

struct FVectorKernelSub : public TBinaryVectorKernel<FVectorKernelSub>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0,VectorRegister Src1)
	{
		*Dst = VectorSubtract(Src0, Src1);
	}
};

struct FVectorKernelMul : public TBinaryVectorKernel<FVectorKernelMul>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0,VectorRegister Src1)
	{
		*Dst = VectorMultiply(Src0, Src1);
	}
};

struct FVectorKernelMad : public TTrinaryVectorKernel<FVectorKernelMad>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0,VectorRegister Src1,VectorRegister Src2)
	{
		*Dst = VectorMultiplyAdd(Src0, Src1, Src2);
	}
};

struct FVectorKernelLerp : public TTrinaryVectorKernel<FVectorKernelLerp>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0,VectorRegister Src1,VectorRegister Src2)
	{
		const VectorRegister One = MakeVectorRegister(1.0f, 1.0f, 1.0f, 1.0f);
		const VectorRegister OneMinusAlpha = VectorSubtract(One, Src2);
		const VectorRegister Tmp = VectorMultiply(Src0, OneMinusAlpha);
		*Dst = VectorMultiplyAdd(Src1, Src2, Tmp);
	}
};

struct FVectorKernelRcp : public TUnaryVectorKernel<FVectorKernelRcp>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0)
	{
		*Dst = VectorReciprocal(Src0);
	}
};

struct FVectorKernelRsq : public TUnaryVectorKernel<FVectorKernelRsq>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0)
	{
		*Dst = VectorReciprocalSqrt(Src0);
	}
};

struct FVectorKernelSqrt : public TUnaryVectorKernel<FVectorKernelSqrt>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0)
	{
		// TODO: Need a SIMD sqrt!
		*Dst = VectorReciprocal(VectorReciprocalSqrt(Src0));
	}
};

struct FVectorKernelNeg : public TUnaryVectorKernel<FVectorKernelNeg>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0)
	{
		*Dst = VectorNegate(Src0);
	}
};

struct FVectorKernelAbs : public TUnaryVectorKernel<FVectorKernelAbs>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0)
	{
		*Dst = VectorAbs(Src0);
	}
};

struct FVectorKernelExp : public TUnaryVectorKernel<FVectorKernelExp>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorExp(Src0);
	}
}; 

struct FVectorKernelExp2 : public TUnaryVectorKernel<FVectorKernelExp2>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorExp2(Src0);
	}
};

struct FVectorKernelLog : public TUnaryVectorKernel<FVectorKernelLog>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorLog(Src0);
	}
};

struct FVectorKernelLog2 : public TUnaryVectorKernel<FVectorKernelLog2>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorLog2(Src0);
	}
};

struct FVectorKernelClamp : public TTrinaryVectorKernel<FVectorKernelClamp>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0,VectorRegister Src1,VectorRegister Src2)
	{
		const VectorRegister Tmp = VectorMax(Src0, Src1);
		*Dst = VectorMin(Tmp, Src2);
	}
};

struct FVectorKernelSin : public TUnaryVectorKernel<FVectorKernelSin>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorSin( VectorMultiply(Src0, GlobalVectorConstants::Pi));
	}
};

struct FVectorKernelCos : public TUnaryVectorKernel<FVectorKernelCos>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
 	{
		*Dst = VectorCos(VectorMultiply(Src0, GlobalVectorConstants::Pi));
	}
};

struct FVectorKernelTan : public TUnaryVectorKernel<FVectorKernelTan>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorTan(VectorMultiply(Src0, GlobalVectorConstants::Pi));
	}
};

struct FVectorKernelASin : public TUnaryVectorKernel<FVectorKernelASin>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorASin(Src0);
	}
};

struct FVectorKernelACos : public TUnaryVectorKernel<FVectorKernelACos>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorACos(Src0);
	}
};

struct FVectorKernelATan : public TUnaryVectorKernel<FVectorKernelATan>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorATan(Src0);
	}
};

struct FVectorKernelATan2 : public TBinaryVectorKernel<FVectorKernelATan2>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0, VectorRegister Src1)
	{
		*Dst = VectorATan2(Src0, Src1);
	}
};

struct FVectorKernelCeil : public TUnaryVectorKernel<FVectorKernelCeil>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorCeil(Src0);
	}
};

struct FVectorKernelFloor : public TUnaryVectorKernel<FVectorKernelFloor>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorFloor(Src0);
	}
};

struct FVectorKernelMod : public TBinaryVectorKernel<FVectorKernelMod>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0, VectorRegister Src1)
	{
		*Dst = VectorMod(Src0, Src1);
	}
};

struct FVectorKernelFrac : public TUnaryVectorKernel<FVectorKernelFrac>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorFractional(Src0);
	}
};

struct FVectorKernelTrunc : public TUnaryVectorKernel<FVectorKernelTrunc>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorTruncate(Src0);
	}
};

struct FVectorKernelLessThan : public TBinaryVectorKernel<FVectorKernelLessThan>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0, VectorRegister Src1)
	{
		const VectorRegister Large = MakeVectorRegister(BIG_NUMBER, BIG_NUMBER, BIG_NUMBER, BIG_NUMBER);
		const VectorRegister One = MakeVectorRegister(1.0f, 1.0f, 1.0f, 1.0f);
		const VectorRegister Zero = MakeVectorRegister(0.0f, 0.0f, 0.0f, 0.0f);
		VectorRegister Tmp = VectorSubtract(Src1, Src0);
		Tmp = VectorMultiply(Tmp, Large);
		Tmp = VectorMin(Tmp, One);
		*Dst = VectorMax(Tmp, Zero);
	}
};



struct FVectorKernelSample : public TBinaryVectorKernelData<FVectorKernelSample>
{
	static void FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, const FNiagaraDataObject *Src0, VectorRegister Src1)
	{
		if (Src0)
		{
			float const* FloatSrc1 = reinterpret_cast<float const*>(&Src1);
			FVector4 Tmp = Src0->Sample(FVector4(FloatSrc1[0], FloatSrc1[1], FloatSrc1[2], FloatSrc1[3]));
			*Dst = VectorLoad(&Tmp);
		}
	}
};

struct FVectorKernelBufferWrite : public TTrinaryVectorKernelData<FVectorKernelBufferWrite>
{
	static void FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, FNiagaraDataObject *Src0, VectorRegister Src1, VectorRegister Src2)
	{
		if (Src0)
		{
			float const* FloatSrc1 = reinterpret_cast<float const*>(&Src1);	// Coords
			float const* FloatSrc2 = reinterpret_cast<float const*>(&Src2);	// Value
			FVector4 Tmp = Src0->Write(FVector4(FloatSrc1[0], FloatSrc1[1], FloatSrc1[2], FloatSrc1[3]), FVector4(FloatSrc2[0], FloatSrc2[1], FloatSrc2[2], FloatSrc2[3]));
			*Dst = VectorLoad(&Tmp);
		}
	}
};


struct FVectorKernelDot : public TBinaryVectorKernel<FVectorKernelDot>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0, VectorRegister Src1)
	{
		*Dst = VectorDot4(Src0, Src1);
	}
};

struct FVectorKernelLength : public TUnaryVectorKernel<FVectorKernelLength>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		VectorRegister Temp = VectorReciprocalLen(Src0);
		*Dst = VectorReciprocal(Temp);
	}
};

struct FVectorKernelCross : public TBinaryVectorKernel<FVectorKernelCross>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0, VectorRegister Src1)
	{
		*Dst = VectorCross(Src0, Src1);
	}
};

struct FVectorKernelNormalize : public TUnaryVectorKernel<FVectorKernelNormalize>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorNormalize(Src0);
	}
};

struct FVectorKernelRandom : public TUnaryVectorKernel<FVectorKernelRandom>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		const float rm = RAND_MAX;
		VectorRegister Result = MakeVectorRegister(static_cast<float>(FMath::Rand()) / rm,
			static_cast<float>(FMath::Rand()) / rm,
			static_cast<float>(FMath::Rand()) / rm,
			static_cast<float>(FMath::Rand()) / rm);
		*Dst = VectorMultiply(Result, Src0);
	}
};



struct FVectorKernelMin : public TBinaryVectorKernel<FVectorKernelMin>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0,VectorRegister Src1)
	{
		*Dst = VectorMin(Src0, Src1);
	}
};

struct FVectorKernelMax : public TBinaryVectorKernel<FVectorKernelMax>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0,VectorRegister Src1)
	{
		*Dst = VectorMax(Src0, Src1);
	}
};

struct FVectorKernelPow : public TBinaryVectorKernel<FVectorKernelPow>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst,VectorRegister Src0,VectorRegister Src1)
	{
		*Dst = VectorPow(Src0, Src1);
	}
};

struct FVectorKernelSign : public TUnaryVectorKernel<FVectorKernelSign>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorSign(Src0);
	}
};

struct FVectorKernelStep : public TUnaryVectorKernel<FVectorKernelStep>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorStep(Src0);
	}
};

struct FVectorKernelNoise : public TUnaryVectorKernel<FVectorKernelNoise>
{
	static VectorRegister RandomTable[10][10][10];

	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		const VectorRegister One = MakeVectorRegister(1.0f, 1.0f, 1.0f, 1.0f);
		const VectorRegister VecEight = MakeVectorRegister(8.0f, 8.0f, 8.0f, 8.0f);
		const VectorRegister OneHalf = MakeVectorRegister(0.5f, 0.5f, 0.5f, 0.5f);
		*Dst = MakeVectorRegister(0.0f, 0.0f, 0.0f, 0.0f);
		
		for (uint32 i = 1; i < 2; i++)
		{
			float Di = 0.2f * (1.0f/(1<<i));
			VectorRegister Div = MakeVectorRegister(Di, Di, Di, Di);
			VectorRegister Coords = VectorMod( VectorAbs( VectorMultiply(Src0, Div) ), VecEight );
			const float *CoordPtr = reinterpret_cast<float const*>(&Coords);
			const int32 Cx = CoordPtr[0];
			const int32 Cy = CoordPtr[1];
			const int32 Cz = CoordPtr[2];

			VectorRegister Frac = VectorFractional(Coords);
			VectorRegister Alpha = VectorReplicate(Frac, 0);
			VectorRegister OneMinusAlpha = VectorSubtract(One, Alpha);
			
			VectorRegister XV1 = VectorMultiplyAdd(RandomTable[Cx][Cy][Cz], Alpha, VectorMultiply(RandomTable[Cx+1][Cy][Cz], OneMinusAlpha));
			VectorRegister XV2 = VectorMultiplyAdd(RandomTable[Cx][Cy+1][Cz], Alpha, VectorMultiply(RandomTable[Cx+1][Cy+1][Cz], OneMinusAlpha));
			VectorRegister XV3 = VectorMultiplyAdd(RandomTable[Cx][Cy][Cz+1], Alpha, VectorMultiply(RandomTable[Cx+1][Cy][Cz+1], OneMinusAlpha));
			VectorRegister XV4 = VectorMultiplyAdd(RandomTable[Cx][Cy+1][Cz+1], Alpha, VectorMultiply(RandomTable[Cx+1][Cy+1][Cz+1], OneMinusAlpha));

			Alpha = VectorReplicate(Frac, 1);
			OneMinusAlpha = VectorSubtract(One, Alpha);
			VectorRegister YV1 = VectorMultiplyAdd(XV1, Alpha, VectorMultiply(XV2, OneMinusAlpha));
			VectorRegister YV2 = VectorMultiplyAdd(XV3, Alpha, VectorMultiply(XV4, OneMinusAlpha));

			Alpha = VectorReplicate(Frac, 2);
			OneMinusAlpha = VectorSubtract(One, Alpha);
			VectorRegister ZV = VectorMultiplyAdd(YV1, Alpha, VectorMultiply(YV2, OneMinusAlpha));

			*Dst = VectorAdd(*Dst, ZV);
		}
	}
};

VectorRegister FVectorKernelNoise::RandomTable[10][10][10];

template<int32 Component>
struct FVectorKernelSplat : public TUnaryVectorKernel<FVectorKernelSplat<Component>>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorReplicate(Src0, Component);
	}
};

template<int32 Cmp0, int32 Cmp1, int32 Cmp2, int32 Cmp3>
struct FVectorKernelCompose : public TQuatenaryVectorKernel<FVectorKernelCompose<Cmp0, Cmp1, Cmp2, Cmp3>>
{
	//Passing as const refs as some compilers cant handle > 3 aligned vectorregister params.
	//inlined so shouldn't impact perf
	//Todo: ^^^^ test this
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, const VectorRegister& Src0, const VectorRegister& Src1, const VectorRegister& Src2, const VectorRegister& Src3)
	{
		//TODO - There's probably a faster way to do this.
		VectorRegister Tmp0 = VectorShuffle(Src0, Src1, Cmp0, Cmp0, Cmp1, Cmp1);
		VectorRegister Tmp1 = VectorShuffle(Src2, Src3, Cmp2, Cmp2, Cmp3, Cmp3);		
		*Dst = VectorShuffle(Tmp0, Tmp1, 0, 2, 0, 2);
	}
};

// Ken Perlin's smootherstep function (zero first and second order derivatives at 0 and 1)
// calculated separately for each channel of Src2
struct FVectorKernelEaseIn : public TTrinaryVectorKernel<FVectorKernelEaseIn>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, const VectorRegister& Src0, const VectorRegister& Src1, const VectorRegister& Src2)
	{
		VectorRegister X = VectorMin( VectorDivide(VectorSubtract(Src2, Src0), VectorSubtract(Src1, Src0)), MakeVectorRegister(1.0f, 1.0f, 1.0f, 1.0f) );
		X = VectorMax(X, MakeVectorRegister(0.0f, 0.0f, 0.0f, 0.0f));

		VectorRegister X3 = VectorMultiply( VectorMultiply(X, X), X);
		VectorRegister N6 = MakeVectorRegister(6.0f, 6.0f, 6.0f, 6.0f);
		VectorRegister N15 = MakeVectorRegister(15.0f, 15.0f, 15.0f, 15.0f);
		VectorRegister T = VectorSubtract( VectorMultiply(X, N6), N15 );
		T = VectorAdd(VectorMultiply(X, T), MakeVectorRegister(10.0f, 10.0f, 10.0f, 10.0f));

		*Dst = VectorMultiply(X3, T);
	}
};

// smoothly runs 0->1->0
struct FVectorKernelEaseInOut : public TUnaryVectorKernel<FVectorKernelEaseInOut>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, const VectorRegister& Src0)
	{
		VectorRegister T = VectorMultiply(Src0, MakeVectorRegister(2.0f, 2.0f, 2.0f, 2.0f));
		T = VectorSubtract(T, MakeVectorRegister(1.0f, 1.0f, 1.0f, 1.0f));
		VectorRegister X2 = VectorMultiply(T, T);
		
		VectorRegister R = VectorMultiply(X2, MakeVectorRegister(0.9604f, 0.9604f, 0.9604f, 0.9604f));
		R = VectorSubtract(R, MakeVectorRegister(1.96f, 1.96f, 1.96f, 1.96f));
		R = VectorMultiply(R, X2);
		*Dst = VectorAdd(R, MakeVectorRegister(1.0f, 1.0f, 1.0f, 1.0f));
	}
};


struct FVectorKernelOutput : public TUnaryVectorKernel<FVectorKernelOutput>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* Dst, VectorRegister Src0)
	{
		VectorStoreAlignedStreamed(Src0, Dst);
	}
};

void VectorVM::Init()
{
	static bool Inited = false;
	if (Inited == false)
	{
		for (int z = 0; z < 10; z++)
		{
			for (int y = 0; y < 10; y++)
			{
				for (int x = 0; x < 10; x++)
				{
					float f1 = (float)FMath::FRandRange(-1.0f, 1.0f);
					float f2 = (float)FMath::FRandRange(-1.0f, 1.0f);
					float f3 = (float)FMath::FRandRange(-1.0f, 1.0f);
					float f4 = (float)FMath::FRandRange(-1.0f, 1.0f);

					FVectorKernelNoise::RandomTable[x][y][z] = MakeVectorRegister(f1, f2, f3, f4);
				}
			}
		}

		Inited = true;
	}
}

void VectorVM::Exec(
	uint8 const* Code,
	VectorRegister** InputRegisters,
	int32 NumInputRegisters,
	VectorRegister** OutputRegisters,
	int32 NumOutputRegisters,
	FVector4 const* ConstantTable,
	FNiagaraDataObject* *DataObjConstTable,
	int32 NumVectors
	)
{
	VectorRegister TempRegisters[NumTempRegisters][VectorsPerChunk];
	VectorRegister* RegisterTable[MaxRegisters] = {0};

	// Map temporary registers.
	for (int32 i = 0; i < NumTempRegisters; ++i)
	{
		RegisterTable[i] = TempRegisters[i];
	}

	// Process one chunk at a time.
	int32 NumChunks = (NumVectors + VectorsPerChunk - 1) / VectorsPerChunk;
	for (int32 ChunkIndex = 0; ChunkIndex < NumChunks; ++ChunkIndex)
	{
		// Map input and output registers.
		for (int32 i = 0; i < NumInputRegisters; ++i)
		{
			RegisterTable[NumTempRegisters + i] = InputRegisters[i] + ChunkIndex * VectorsPerChunk;
		}
		for (int32 i = 0; i < NumOutputRegisters; ++i)
		{
			RegisterTable[NumTempRegisters + MaxInputRegisters + i] = OutputRegisters[i] + ChunkIndex * VectorsPerChunk;
		}

		// Setup execution context.
		int32 VectorsThisChunk = FMath::Min<int32>(NumVectors, VectorsPerChunk);
		FVectorVMContext Context(Code, RegisterTable, ConstantTable, DataObjConstTable, VectorsThisChunk);
		EOp Op = EOp::done;

		// Execute VM on all vectors in this chunk.
		do 
		{
			Op = DecodeOp(Context);
			switch (Op)
			{
			// Dispatch kernel ops.
			case EOp::add: FVectorKernelAdd::Exec(Context); break;
			case EOp::sub: FVectorKernelSub::Exec(Context); break;
			case EOp::mul: FVectorKernelMul::Exec(Context); break;
			case EOp::mad: FVectorKernelMad::Exec(Context); break;
			case EOp::lerp: FVectorKernelLerp::Exec(Context); break;
			case EOp::rcp: FVectorKernelRcp::Exec(Context); break;
			case EOp::rsq: FVectorKernelRsq::Exec(Context); break;
			case EOp::sqrt: FVectorKernelSqrt::Exec(Context); break;
			case EOp::neg: FVectorKernelNeg::Exec(Context); break;
			case EOp::abs: FVectorKernelAbs::Exec(Context); break;
			case EOp::exp: FVectorKernelExp::Exec(Context); break;
			case EOp::exp2: FVectorKernelExp2::Exec(Context); break;
			case EOp::log: FVectorKernelLog::Exec(Context); break;
			case EOp::log2: FVectorKernelLog2::Exec(Context); break;
			case EOp::sin: FVectorKernelSin::Exec(Context); break;
			case EOp::cos: FVectorKernelCos::Exec(Context); break;
			case EOp::tan: FVectorKernelTan::Exec(Context); break;
			case EOp::asin: FVectorKernelASin::Exec(Context); break;
			case EOp::acos: FVectorKernelACos::Exec(Context); break;
			case EOp::atan: FVectorKernelATan::Exec(Context); break;
			case EOp::atan2: FVectorKernelATan2::Exec(Context); break;
			case EOp::ceil: FVectorKernelCeil::Exec(Context); break;
			case EOp::floor: FVectorKernelFloor::Exec(Context); break;
			case EOp::fmod: FVectorKernelMod::Exec(Context); break;
			case EOp::frac: FVectorKernelFrac::Exec(Context); break;
			case EOp::trunc: FVectorKernelTrunc::Exec(Context); break;
			case EOp::clamp: FVectorKernelClamp::Exec(Context); break;
			case EOp::min: FVectorKernelMin::Exec(Context); break;
			case EOp::max: FVectorKernelMax::Exec(Context); break;
			case EOp::pow: FVectorKernelPow::Exec(Context); break;
			case EOp::sign: FVectorKernelSign::Exec(Context); break;
			case EOp::step: FVectorKernelStep::Exec(Context); break;
			case EOp::dot: FVectorKernelDot::Exec(Context); break;
			case EOp::cross: FVectorKernelCross::Exec(Context); break;
			case EOp::normalize: FVectorKernelNormalize::Exec(Context); break;
			case EOp::random: FVectorKernelRandom::Exec(Context); break;
			case EOp::length: FVectorKernelLength::Exec(Context); break;
			case EOp::noise: FVectorKernelNoise::Exec(Context); break;
			case EOp::splatx: FVectorKernelSplat<0>::Exec(Context); break;
			case EOp::splaty: FVectorKernelSplat<1>::Exec(Context); break;
			case EOp::splatz: FVectorKernelSplat<2>::Exec(Context); break;
			case EOp::splatw: FVectorKernelSplat<3>::Exec(Context); break;
			case EOp::compose: FVectorKernelCompose<0,1,2,3>::Exec(Context); break;
			case EOp::composex: FVectorKernelCompose<0, 0, 0, 0>::Exec(Context); break;
			case EOp::composey: FVectorKernelCompose<1, 1, 1, 1>::Exec(Context); break;
			case EOp::composez: FVectorKernelCompose<2, 2, 2, 2>::Exec(Context); break;
			case EOp::composew: FVectorKernelCompose<3, 3, 3, 3>::Exec(Context); break;
			case EOp::lessthan: FVectorKernelLessThan::Exec(Context); break;
			case EOp::sample: FVectorKernelSample::Exec(Context); break;
			case EOp::bufferwrite: FVectorKernelBufferWrite::Exec(Context); break;
			case EOp::eventbroadcast: break;
			case EOp::easein: FVectorKernelEaseIn::Exec(Context); break;
			case EOp::easeinout: FVectorKernelEaseInOut::Exec(Context); break;
			case EOp::output: FVectorKernelOutput::Exec(Context); break;
				
			// Execution always terminates with a "done" opcode.
			case EOp::done:
				break;

			// Opcode not recognized / implemented.
			default:
				UE_LOG(LogVectorVM, Error, TEXT("Unknown op code 0x%02x"), (uint32)Op);
				return;//BAIL
			}
		} while (Op != EOp::done);

		NumVectors -= VectorsPerChunk;
	}
}

uint8 VectorVM::GetNumOpCodes()
{
	return (uint8)EOp::NumOpcodes;
}

uint8 VectorVM::CreateSrcOperandMask(bool bIsOp0Constant, bool bIsOp1Constant, bool bIsOp2Constant, bool bIsOp3Constant)
{
	return	(bIsOp0Constant ? (1 << 0) : 0) |
			(bIsOp1Constant ? (1 << 1) : 0) |
			(bIsOp2Constant ? (1 << 2) : 0) |
			(bIsOp3Constant ? (1 << 3) : 0) ;
}

uint8 VectorVM::CreateSrcOperandMask(VectorVM::EOperandType Type1, VectorVM::EOperandType Type2, VectorVM::EOperandType Type3, VectorVM::EOperandType Type4)
{
	return	(Type1==VectorVM::ConstantOperandType ? (1 << 0) : 0) |
		(Type2 == VectorVM::ConstantOperandType ? (1 << 1) : 0) |
		(Type3 == VectorVM::ConstantOperandType ? (1 << 2) : 0) |
		(Type4 == VectorVM::ConstantOperandType ? (1 << 3) : 0) |
		(Type1 == VectorVM::DataObjConstantOperandType ? (1 << 4) : 0) | 
		(Type2 == VectorVM::DataObjConstantOperandType ? (1 << 5) : 0) |
		(Type3 == VectorVM::DataObjConstantOperandType ? (1 << 6) : 0) |
		(Type4 == VectorVM::DataObjConstantOperandType ? (1 << 7) : 0);
}

/*------------------------------------------------------------------------------
	Automation test for the VM.
------------------------------------------------------------------------------*/

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVectorVMTest, "System.Core.Math.Vector VM", EAutomationTestFlags::ATF_SmokeTest)

bool FVectorVMTest::RunTest(const FString& Parameters)
{
	uint8 TestCode[] =
	{
		(uint8)VectorVM::EOp::mul, 0x00, SRCOP_RRRR, 0x0 + VectorVM::NumTempRegisters, 0x0 + VectorVM::NumTempRegisters,       // mul r0, r8, r8
		(uint8)VectorVM::EOp::mad, 0x01, SRCOP_RRRR, 0x01 + VectorVM::NumTempRegisters, 0x01 + VectorVM::NumTempRegisters, 0x00, // mad r1, r9, r9, r0
		(uint8)VectorVM::EOp::mad, 0x00, SRCOP_RRRR, 0x02 + VectorVM::NumTempRegisters, 0x02 + VectorVM::NumTempRegisters, 0x01, // mad r0, r10, r10, r1
		(uint8)VectorVM::EOp::add, 0x01, SRCOP_RRCR, 0x00, 0x01,       // addi r1, r0, c1
		(uint8)VectorVM::EOp::neg, 0x00, SRCOP_RRRR, 0x01,             // neg r0, r1
		(uint8)VectorVM::EOp::clamp, VectorVM::FirstOutputRegister, SRCOP_RCCR, 0x00, 0x02, 0x03, // clampii r40, r0, c2, c3
		0x00 // terminator
	};

	VectorRegister TestRegisters[4][VectorVM::VectorsPerChunk];
	VectorRegister* InputRegisters[3] = { TestRegisters[0], TestRegisters[1], TestRegisters[2] };
	VectorRegister* OutputRegisters[1] = { TestRegisters[3] };

	VectorRegister Inputs[3][VectorVM::VectorsPerChunk];
	for (int32 i = 0; i < VectorVM::ChunkSize; i++)
	{
		reinterpret_cast<float*>(&Inputs[0])[i] = static_cast<float>(i);
		reinterpret_cast<float*>(&Inputs[1])[i] = static_cast<float>(i);
		reinterpret_cast<float*>(&Inputs[2])[i] = static_cast<float>(i);
		reinterpret_cast<float*>(InputRegisters[0])[i] = static_cast<float>(i);
		reinterpret_cast<float*>(InputRegisters[1])[i] = static_cast<float>(i);
		reinterpret_cast<float*>(InputRegisters[2])[i] = static_cast<float>(i);
	}

	FVector4 ConstantTable[VectorVM::MaxConstants];
	ConstantTable[0] = FVector4(0.0f, 0.0f, 0.0f, 0.0f);
	ConstantTable[1] = FVector4(5.0f, 5.0f, 5.0f, 5.0f);
	ConstantTable[2] = FVector4(-20.0f, -20.0f, -20.0f, -20.0f);
	ConstantTable[3] = FVector4(20.0f, 20.0f, 20.0f, 20.0f);

	VectorVM::Exec(
		TestCode,
		InputRegisters, 3,
		OutputRegisters, 1,
		ConstantTable,
		nullptr,
		VectorVM::VectorsPerChunk
		);

	for (int32 i = 0; i < VectorVM::ChunkSize; i++)
	{
		float Ins[3];

		// Verify that the input registers were not overwritten.
		for (int32 InputIndex = 0; InputIndex < 3; ++InputIndex)
		{
			float In = Ins[InputIndex] = reinterpret_cast<float*>(&Inputs[InputIndex])[i];
			float R = reinterpret_cast<float*>(InputRegisters[InputIndex])[i];
			if (In != R)
			{
				UE_LOG(LogVectorVM,Error,TEXT("Input register %d vector %d element %d overwritten. Has %f expected %f"),
					InputIndex,
					i / VectorVM::ElementsPerVector,
					i % VectorVM::ElementsPerVector,
					R,
					In
					);
				return false;
			}
		}

		// Verify that outputs match what we expect.
		float Out = reinterpret_cast<float*>(OutputRegisters[0])[i];
		float Expected = FMath::Clamp<float>(-(Ins[0] * Ins[0] + Ins[1] * Ins[1] + Ins[2] * Ins[2] + 5.0f), -20.0f, 20.0f);
		if (Out != Expected)
		{
			UE_LOG(LogVectorVM,Error,TEXT("Output register %d vector %d element %d is wrong. Has %f expected %f"),
				0,
				i / VectorVM::ElementsPerVector,
				i % VectorVM::ElementsPerVector,
				Out,
				Expected
				);
			return false;
		}
	}

	return true;
}

#undef VM_FORCEINLINE
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "VectorVMPrivate.h"
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
	/** The number of vectors to process. */
	int32 NumVectors;

	/** Initialization constructor. */
	FVectorVMContext(
		uint8 const* InCode,
		VectorRegister** InRegisterTable,
		FVector4 const* InConstantTable,
		int32 InNumVectors
		)
		: Code(InCode)
		, RegisterTable(InRegisterTable)
		, ConstantTable(InConstantTable)
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
		float* RESTRICT FloatDst = reinterpret_cast<float* RESTRICT>(Dst);
		float const* FloatSrc0 = reinterpret_cast<float const*>(&Src0);
		FloatDst[0] = FMath::Sqrt(FloatSrc0[0]);
		FloatDst[1] = FMath::Sqrt(FloatSrc0[1]);
		FloatDst[2] = FMath::Sqrt(FloatSrc0[2]);
		FloatDst[3] = FMath::Sqrt(FloatSrc0[3]);
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
	static void FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0, VectorRegister Src1)
	{
		const VectorRegister Large = MakeVectorRegister(BIG_NUMBER, BIG_NUMBER, BIG_NUMBER, BIG_NUMBER);
		const VectorRegister One = MakeVectorRegister(1.0f, 1.0f, 1.0f, 1.0f);
		const VectorRegister Zero = MakeVectorRegister(0.0f, 0.0f, 0.0f, 0.0f);

		float const* FloatSrc0 = reinterpret_cast<float const*>(&Src0);
		float const* FloatSrc1 = reinterpret_cast<float const*>(&Src1);
		VectorRegister Tmp = VectorSubtract(Src1, Src0);
		Tmp = VectorMultiply(Tmp, Large);
		Tmp = VectorMin(Tmp, One);
		*Dst = VectorMax(Tmp, Zero);
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
		VectorRegister Temp = VectorDot4(Src0, Src0);
		float const* FloatSrc = reinterpret_cast<float const*>(&Temp);
		float SDot = FMath::Sqrt(FloatSrc[0]);
		*Dst = MakeVectorRegister(SDot, SDot, SDot, SDot);
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
		const VectorRegister OneHalf = MakeVectorRegister(0.5f, 0.5f, 0.5f, 0.5f);
		VectorRegister RndVals[2][2][2];
		*Dst = MakeVectorRegister(0.0f, 0.0f, 0.0f, 0.0f);

		float const* FloatSrc = reinterpret_cast<float const*>(&Src0);

		for (uint32 i = 1; i < 2; i++)
		{
			float FX = FloatSrc[0] / 5 / (1<<i);
			float FY = FloatSrc[1] / 5 / (1<<i);
			float FZ = FloatSrc[2] / 5 / (1<<i);
			uint32 X = FMath::Abs( (int32)(FX) % 8 );
			uint32 Y = FMath::Abs( (int32)(FY) % 8 );
			uint32 Z = FMath::Abs( (int32)(FZ) % 8 );

			for (int32 z = 0; z < 2; z++)
			{
				for (int32 y = 0; y < 2; y++)
				{
					for (int32 x = 0; x < 2; x++)
					{
						RndVals[x][y][z] = RandomTable[X+x][Y+y][Z+z];
					}
				}
			}

			const int32 IntCoords[3] = { static_cast<int32>(FX), static_cast<int32>(FY), static_cast<int32>(FZ) };
			const float Fractionals[3] = { FX - IntCoords[0], FY - IntCoords[1], FZ - IntCoords[2] };
			VectorRegister Alpha = MakeVectorRegister(Fractionals[0], Fractionals[0], Fractionals[0], Fractionals[0]);

			VectorRegister OneMinusAlpha = VectorSubtract(One, Alpha);
			VectorRegister XV1 = VectorAdd(VectorMultiply(RndVals[0][0][0], Alpha), VectorMultiply(RndVals[1][0][0], OneMinusAlpha));
			VectorRegister XV2 = VectorAdd(VectorMultiply(RndVals[0][1][0], Alpha), VectorMultiply(RndVals[1][1][0], OneMinusAlpha));
			VectorRegister XV3 = VectorAdd(VectorMultiply(RndVals[0][0][1], Alpha), VectorMultiply(RndVals[1][0][1], OneMinusAlpha));
			VectorRegister XV4 = VectorAdd(VectorMultiply(RndVals[0][1][1], Alpha), VectorMultiply(RndVals[1][1][1], OneMinusAlpha));

			Alpha = MakeVectorRegister(Fractionals[1], Fractionals[1], Fractionals[1], Fractionals[1]);
			OneMinusAlpha = VectorSubtract(One, Alpha);
			VectorRegister YV1 = VectorAdd(VectorMultiply(XV1, Alpha), VectorMultiply(XV2, OneMinusAlpha));
			VectorRegister YV2 = VectorAdd(VectorMultiply(XV3, Alpha), VectorMultiply(XV4, OneMinusAlpha));


			Alpha = MakeVectorRegister(Fractionals[2], Fractionals[2], Fractionals[2], Fractionals[2]);
			OneMinusAlpha = VectorSubtract(One, Alpha);
			VectorRegister ZV = VectorAdd(VectorMultiply(YV1, Alpha), VectorMultiply(YV2, OneMinusAlpha));

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
		FVectorVMContext Context(Code, RegisterTable, ConstantTable, VectorsThisChunk);
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

/*------------------------------------------------------------------------------
	Automation test for the VM.
------------------------------------------------------------------------------*/

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FVectorVMTest, "Core.Math.Vector VM", EAutomationTestFlags::ATF_SmokeTest)

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
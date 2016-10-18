// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "VectorVMPrivate.h"
#include "CurveVector.h"
#include "ModuleManager.h"

IMPLEMENT_MODULE(FDefaultModuleImpl, VectorVM);

DEFINE_LOG_CATEGORY_STATIC(LogVectorVM, All, All);

//#define VM_FORCEINLINE
#define VM_FORCEINLINE FORCEINLINE

#define OP_REGISTER (0)
#define OP0_CONST (1 << 0)
#define OP1_CONST (1 << 1)
#define OP2_CONST (1 << 2)
#define OP3_CONST (1 << 3)

#define SRCOP_RRRR (OP_REGISTER | OP_REGISTER | OP_REGISTER | OP_REGISTER)
#define SRCOP_RRRC (OP_REGISTER | OP_REGISTER | OP_REGISTER | OP0_CONST)
#define SRCOP_RRCR (OP_REGISTER | OP_REGISTER | OP1_CONST | OP_REGISTER)
#define SRCOP_RRCC (OP_REGISTER | OP_REGISTER | OP1_CONST | OP0_CONST)
#define SRCOP_RCRR (OP_REGISTER | OP2_CONST | OP_REGISTER | OP_REGISTER)
#define SRCOP_RCRC (OP_REGISTER | OP2_CONST | OP_REGISTER | OP0_CONST)
#define SRCOP_RCCR (OP_REGISTER | OP2_CONST | OP1_CONST | OP_REGISTER)
#define SRCOP_RCCC (OP_REGISTER | OP2_CONST | OP1_CONST | OP0_CONST)
#define SRCOP_CRRR (OP3_CONST | OP_REGISTER | OP_REGISTER | OP_REGISTER)
#define SRCOP_CRRC (OP3_CONST | OP_REGISTER | OP_REGISTER | OP0_CONST)
#define SRCOP_CRCR (OP3_CONST | OP_REGISTER | OP1_CONST | OP_REGISTER)
#define SRCOP_CRCC (OP3_CONST | OP_REGISTER | OP1_CONST | OP0_CONST)
#define SRCOP_CCRR (OP3_CONST | OP2_CONST | OP_REGISTER | OP_REGISTER)
#define SRCOP_CCRC (OP3_CONST | OP2_CONST | OP_REGISTER | OP0_CONST)
#define SRCOP_CCCR (OP3_CONST | OP2_CONST | OP1_CONST | OP_REGISTER)
#define SRCOP_CCCC (OP3_CONST | OP2_CONST | OP1_CONST | OP0_CONST)

uint8 VectorVM::CreateSrcOperandMask(EVectorVMOperandLocation Type1, EVectorVMOperandLocation Type2, EVectorVMOperandLocation Type3, EVectorVMOperandLocation Type4)
{
	return	(Type1 == EVectorVMOperandLocation::Constant ? OP0_CONST : OP_REGISTER) |
		(Type2 == EVectorVMOperandLocation::Constant ? OP1_CONST : OP_REGISTER) |
		(Type3 == EVectorVMOperandLocation::Constant ? OP2_CONST : OP_REGISTER) |
		(Type4 == EVectorVMOperandLocation::Constant ? OP3_CONST : OP_REGISTER);
}


#if WITH_EDITOR
TArray<FString> OpNames;
TArray<FString> OperandLocationNames;
#endif

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
	/** Pointer to the shared data table. */
	FVectorVMSharedDataView* RESTRICT SharedDataTable;

	/** The number of vectors to process. */
	int32 NumVectors;

	/** The number of instances to process. */
	int32 NumInstances;

	/** The Operation currently executing. */
	EVectorVMOp CurrOp;

	/** The instance we're currently starting at. Advances with each chunk processed. */
	int32 StartInstance;
	/** Initialization constructor. */
	FVectorVMContext(
		const uint8* InCode,
		VectorRegister** InRegisterTable,
		const FVector4* InConstantTable,
		FVectorVMSharedDataView* InSharedDataTable,
		int32 InNumVectors,
		int32 InNumInstances,
		int32 InStartInstance
		)
		: Code(InCode)
		, RegisterTable(InRegisterTable)
		, ConstantTable(InConstantTable)
		, SharedDataTable(InSharedDataTable)
		, NumVectors(InNumVectors)
		, NumInstances(InNumInstances)
		, CurrOp(EVectorVMOp::done)
		, StartInstance(InStartInstance)
	{
	}

	FORCEINLINE void SetOp(EVectorVMOp InOp){ CurrOp = InOp; }
};


static VM_FORCEINLINE uint8 DecodeU8(FVectorVMContext& Context)
{
	return *Context.Code++;
}

static VM_FORCEINLINE uint8 DecodeU16(FVectorVMContext& Context)
{
	return (*((uint16*)Context.Code))++;
}

static VM_FORCEINLINE uint8 DecodeU32(FVectorVMContext& Context)
{
	return (*((uint32*)Context.Code))++;
}

/** Decode the next operation contained in the bytecode. */
static VM_FORCEINLINE EVectorVMOp DecodeOp(FVectorVMContext& Context)
{
	return static_cast<EVectorVMOp>(DecodeU8(Context));
}

static VM_FORCEINLINE uint8 DecodeSrcOperandTypes(FVectorVMContext& Context)
{
	return DecodeU8(Context);
}

//////////////////////////////////////////////////////////////////////////
/** Constant handler. */

struct FConstantHandlerBase
{
	uint8 ConstantIndex;
	FConstantHandlerBase(FVectorVMContext& Context)
		: ConstantIndex(DecodeU8(Context))
	{}

	VM_FORCEINLINE void Advance(){ }
};

template<typename T>
struct FConstantHandler : public FConstantHandlerBase
{
	T Constant;
	FConstantHandler(FVectorVMContext& Context)
		: FConstantHandlerBase(Context)
		, Constant(Context.ConstantTable[ConstantIndex])
	{}
	VM_FORCEINLINE const T& Get(){ return Constant; }
};

template<>
struct FConstantHandler<VectorRegister> : public FConstantHandlerBase
{
	VectorRegister Constant;
	FConstantHandler(FVectorVMContext& Context)
		: FConstantHandlerBase(Context)
		, Constant(VectorLoadAligned(&Context.ConstantTable[ConstantIndex]))
	{}
	VM_FORCEINLINE const VectorRegister Get(){ return Constant; }
};

typedef FConstantHandler<VectorRegister> FVectorConstantHandler;

//////////////////////////////////////////////////////////////////////////
// Register handlers.
// Handle reading of a register, advancing the pointer with each read.

struct FRegisterHandlerBase
{
	int32 RegisterIndex;
	FRegisterHandlerBase(FVectorVMContext& Context)
		: RegisterIndex(DecodeU8(Context))
	{}
};

template<typename T, int32 NumInstancesPerOp=1>
struct FRegisterHandler : public FRegisterHandlerBase
{
	T* RESTRICT Register;
	FRegisterHandler(FVectorVMContext& Context)
		: FRegisterHandlerBase(Context)
		, Register((T*)Context.RegisterTable[RegisterIndex])
	{}
	VM_FORCEINLINE const T Get(){ return *Register; }
	VM_FORCEINLINE void Advance(){ Register += NumInstancesPerOp; }
};

template<int32 NumInstancesPerOp> struct FRegisterHandler<VectorRegister, NumInstancesPerOp> : public FRegisterHandlerBase
{
	VectorRegister* RESTRICT Register;
	FRegisterHandler(FVectorVMContext& Context)
		: FRegisterHandlerBase(Context)
		, Register((VectorRegister*)Context.RegisterTable[RegisterIndex])
	{}
	VM_FORCEINLINE const VectorRegister Get(){ return VectorLoadAligned(Register); }
	VM_FORCEINLINE void Advance(){ Register += NumInstancesPerOp; }
};

typedef FRegisterHandler<VectorRegister> FVectorRegisterHandler;

/** Handles writing to a register, advancing the pointer with each write. */
template<typename T, int32 NumInstancesPerOp = 1>
struct FRegisterDestHandler : public FRegisterHandlerBase
{
	T* RESTRICT Register;
	FRegisterDestHandler(FVectorVMContext& Context)
		: FRegisterHandlerBase(Context)
		, Register((T*)Context.RegisterTable[RegisterIndex])
	{}
	VM_FORCEINLINE T* RESTRICT Get(){ return Register; }
	VM_FORCEINLINE T GetValue(){ return *Register; }
	VM_FORCEINLINE void Advance(){ Register += NumInstancesPerOp; }
};

template<int32 NumInstancesPerOp>
struct FRegisterDestHandler<VectorRegister, NumInstancesPerOp> : public FRegisterHandlerBase
{
	VectorRegister* RESTRICT Register;
	FRegisterDestHandler(FVectorVMContext& Context)
		: FRegisterHandlerBase(Context)
		, Register((VectorRegister*)Context.RegisterTable[RegisterIndex])
	{}
	VM_FORCEINLINE VectorRegister* RESTRICT Get(){ return Register; }
	VM_FORCEINLINE VectorRegister GetValue(){ return VectorLoadAligned(Register); }
	VM_FORCEINLINE void Advance(){ Register += NumInstancesPerOp; }
};

//////////////////////////////////////////////////////////////////////////
// Kernels
template<typename Kernel, typename DstHandler, typename Arg0Handler>
struct TUnaryKernel
{
	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		DstHandler Dst(Context);
		Arg0Handler Arg0(Context);

		for (int32 i = 0; i < Context.NumInstances; i += Kernel::NumInstancesPerOp)
		{
			Kernel::DoKernel(Dst.Get(), Arg0.Get());
			Dst.Advance();	Arg0.Advance();
		}
	}
};

template<typename Kernel, typename DstHandler, typename Arg0Handler, typename Arg1Handler>
struct TBinaryKernel
{
	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		DstHandler Dst(Context); 
		Arg0Handler Arg0(Context); 
		Arg1Handler Arg1(Context);
		for (int32 i = 0; i < Context.NumInstances; i += Kernel::NumInstancesPerOp)
		{
			Kernel::DoKernel(Dst.Get(), Arg0.Get(), Arg1.Get());
			Dst.Advance(); Arg0.Advance(); Arg1.Advance();
		}
	}
};

template<typename Kernel, typename DstHandler, typename Arg0Handler, typename Arg1Handler, typename Arg2Handler>
struct TTrinaryKernel
{
	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		DstHandler Dst(Context);
		Arg0Handler Arg0(Context);
		Arg1Handler Arg1(Context);
		Arg2Handler Arg2(Context);
		for (int32 i = 0; i < Context.NumInstances; i += Kernel::NumInstancesPerOp)
		{
			Kernel::DoKernel(Dst.Get(), Arg0.Get(), Arg1.Get(), Arg2.Get());
			Dst.Advance(); Arg0.Advance(); Arg1.Advance(); Arg2.Advance();
		}
	}
};

template<typename Kernel, typename DstHandler, typename Arg0Handler, typename Arg1Handler, typename Arg2Handler, typename Arg3Handler>
struct TQuaternaryKernel
{
	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		DstHandler Dst(Context);
		Arg0Handler Arg0(Context);
		Arg1Handler Arg1(Context);
		Arg2Handler Arg2(Context);
		Arg3Handler Arg3(Context);
		for (int32 i = 0; i < Context.NumInstances; i += Kernel::NumInstancesPerOp)
		{
			Kernel::DoKernel(Dst.Get(), Arg0.Get(), Arg1.Get(), Arg2.Get(), Arg3.Get());
			Dst.Advance(); Arg0.Advance(); Arg1.Advance(); Arg2.Advance(); Arg3.Advance();
		}
	}
};

/** Base class of vector kernels with a single operand. */
template <typename Kernel, typename DstHandler = FRegisterDestHandler<VectorRegister>>
struct TUnaryVectorKernel
{
	static const int32 NumInstancesPerOp = 1;
	static void Exec(FVectorVMContext& Context)
	{
		uint32 SrcOpTypes = DecodeSrcOperandTypes(Context);
		switch (SrcOpTypes)
		{
		case SRCOP_RRRR: 	TUnaryKernel<Kernel, DstHandler, FVectorRegisterHandler>::Exec(Context); break;
		case SRCOP_RRRC:	TUnaryKernel<Kernel, DstHandler, FVectorConstantHandler>::Exec(Context); break;
		default: check(0); break; 
		};
	}
};

/** Base class of Vector kernels with 2 operands. */
template <typename Kernel, typename DstHandler = FRegisterDestHandler<VectorRegister>>
struct TBinaryVectorKernel
{
	static const int32 NumInstancesPerOp = 1;
	static void Exec(FVectorVMContext& Context)
	{
		uint32 SrcOpTypes = DecodeSrcOperandTypes(Context);
		switch (SrcOpTypes)
		{
		case SRCOP_RRRR: 	TBinaryKernel<Kernel, DstHandler, FVectorRegisterHandler, FVectorRegisterHandler>::Exec(Context); break;
		case SRCOP_RRRC:	TBinaryKernel<Kernel, DstHandler, FVectorConstantHandler, FVectorRegisterHandler>::Exec(Context); break;
		case SRCOP_RRCR: 	TBinaryKernel<Kernel, DstHandler, FVectorRegisterHandler, FVectorConstantHandler>::Exec(Context); break;
		case SRCOP_RRCC:	TBinaryKernel<Kernel, DstHandler, FVectorConstantHandler, FVectorConstantHandler>::Exec(Context); break;
		default: check(0); break;
		};
	}
};

/** Base class of Vector kernels with 3 operands. */
template <typename Kernel, typename DstHandler = FRegisterDestHandler<VectorRegister>>
struct TTrinaryVectorKernel
{
	static const int32 NumInstancesPerOp = 1;
	static void Exec(FVectorVMContext& Context)
	{
		uint32 SrcOpTypes = DecodeSrcOperandTypes(Context);
		switch (SrcOpTypes)
		{
		case SRCOP_RRRR: 	TTrinaryKernel<Kernel, DstHandler, FVectorRegisterHandler, FVectorRegisterHandler, FVectorRegisterHandler>::Exec(Context); break;
		case SRCOP_RRRC:	TTrinaryKernel<Kernel, DstHandler, FVectorConstantHandler, FVectorRegisterHandler, FVectorRegisterHandler>::Exec(Context); break;
		case SRCOP_RRCR: 	TTrinaryKernel<Kernel, DstHandler, FVectorRegisterHandler, FVectorConstantHandler, FVectorRegisterHandler>::Exec(Context); break;
		case SRCOP_RRCC:	TTrinaryKernel<Kernel, DstHandler, FVectorConstantHandler, FVectorConstantHandler, FVectorRegisterHandler>::Exec(Context); break;
		case SRCOP_RCRR: 	TTrinaryKernel<Kernel, DstHandler, FVectorRegisterHandler, FVectorRegisterHandler, FVectorConstantHandler>::Exec(Context); break;
		case SRCOP_RCRC:	TTrinaryKernel<Kernel, DstHandler, FVectorConstantHandler, FVectorRegisterHandler, FVectorConstantHandler>::Exec(Context); break;
		case SRCOP_RCCR: 	TTrinaryKernel<Kernel, DstHandler, FVectorRegisterHandler, FVectorConstantHandler, FVectorConstantHandler>::Exec(Context); break;
		case SRCOP_RCCC:	TTrinaryKernel<Kernel, DstHandler, FVectorConstantHandler, FVectorConstantHandler, FVectorConstantHandler>::Exec(Context); break;
		default: check(0); break;
		};
	}
};

/** Base class of Vector kernels with 4 operands. */
template <typename Kernel, typename DstHandler = FRegisterDestHandler<VectorRegister>>
struct TQuatenaryVectorKernel
{
	static const int32 NumInstancesPerOp = 1;
	static void Exec(FVectorVMContext& Context)
	{
		uint32 SrcOpTypes = DecodeSrcOperandTypes(Context);
		switch (SrcOpTypes)
		{
		case SRCOP_RRRR: 	TQuaternaryKernel<Kernel, DstHandler, FVectorRegisterHandler, FVectorRegisterHandler, FVectorRegisterHandler, FVectorRegisterHandler>::Exec(Context); break;
		case SRCOP_RRRC:	TQuaternaryKernel<Kernel, DstHandler, FVectorConstantHandler, FVectorRegisterHandler, FVectorRegisterHandler, FVectorRegisterHandler>::Exec(Context); break;
		case SRCOP_RRCR: 	TQuaternaryKernel<Kernel, DstHandler, FVectorRegisterHandler, FVectorConstantHandler, FVectorRegisterHandler, FVectorRegisterHandler>::Exec(Context); break;
		case SRCOP_RRCC:	TQuaternaryKernel<Kernel, DstHandler, FVectorConstantHandler, FVectorConstantHandler, FVectorRegisterHandler, FVectorRegisterHandler>::Exec(Context); break;
		case SRCOP_RCRR: 	TQuaternaryKernel<Kernel, DstHandler, FVectorRegisterHandler, FVectorRegisterHandler, FVectorConstantHandler, FVectorRegisterHandler>::Exec(Context); break;
		case SRCOP_RCRC:	TQuaternaryKernel<Kernel, DstHandler, FVectorConstantHandler, FVectorRegisterHandler, FVectorConstantHandler, FVectorRegisterHandler>::Exec(Context); break;
		case SRCOP_RCCR: 	TQuaternaryKernel<Kernel, DstHandler, FVectorRegisterHandler, FVectorConstantHandler, FVectorConstantHandler, FVectorRegisterHandler>::Exec(Context); break;
		case SRCOP_RCCC:	TQuaternaryKernel<Kernel, DstHandler, FVectorConstantHandler, FVectorConstantHandler, FVectorConstantHandler, FVectorRegisterHandler>::Exec(Context); break;
		case SRCOP_CRRR: 	TQuaternaryKernel<Kernel, DstHandler, FVectorRegisterHandler, FVectorRegisterHandler, FVectorRegisterHandler, FVectorConstantHandler>::Exec(Context); break;
		case SRCOP_CRRC:	TQuaternaryKernel<Kernel, DstHandler, FVectorConstantHandler, FVectorRegisterHandler, FVectorRegisterHandler, FVectorConstantHandler>::Exec(Context); break;
		case SRCOP_CRCR: 	TQuaternaryKernel<Kernel, DstHandler, FVectorRegisterHandler, FVectorConstantHandler, FVectorRegisterHandler, FVectorConstantHandler>::Exec(Context); break;
		case SRCOP_CRCC:	TQuaternaryKernel<Kernel, DstHandler, FVectorConstantHandler, FVectorConstantHandler, FVectorRegisterHandler, FVectorConstantHandler>::Exec(Context); break;
		case SRCOP_CCRR: 	TQuaternaryKernel<Kernel, DstHandler, FVectorRegisterHandler, FVectorRegisterHandler, FVectorConstantHandler, FVectorConstantHandler>::Exec(Context); break;
		case SRCOP_CCRC:	TQuaternaryKernel<Kernel, DstHandler, FVectorConstantHandler, FVectorRegisterHandler, FVectorConstantHandler, FVectorConstantHandler>::Exec(Context); break;
		case SRCOP_CCCR: 	TQuaternaryKernel<Kernel, DstHandler, FVectorRegisterHandler, FVectorConstantHandler, FVectorConstantHandler, FVectorConstantHandler>::Exec(Context); break;
		case SRCOP_CCCC:	TQuaternaryKernel<Kernel, DstHandler, FVectorConstantHandler, FVectorConstantHandler, FVectorConstantHandler, FVectorConstantHandler>::Exec(Context); break;
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

struct FVectorKernelDiv : public TBinaryVectorKernel<FVectorKernelDiv>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0, VectorRegister Src1)
	{
		*Dst = VectorDivide(Src0, Src1);
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
		const VectorRegister OneMinusAlpha = VectorSubtract(GlobalVectorConstants::FloatOne, Src2);
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
		*Dst = VectorSin(VectorMultiply(Src0, GlobalVectorConstants::TwoPi));
	}
};

struct FVectorKernelCos : public TUnaryVectorKernel<FVectorKernelCos>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
 	{
		*Dst = VectorCos(VectorMultiply(Src0, GlobalVectorConstants::TwoPi));
	}
};

struct FVectorKernelTan : public TUnaryVectorKernel<FVectorKernelTan>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorTan(VectorMultiply(Src0, GlobalVectorConstants::TwoPi));
	}
};

struct FVectorKernelASin : public TUnaryVectorKernel<FVectorKernelASin>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorMultiply(VectorASin(Src0), GlobalVectorConstants::OneOverTwoPi);
	}
};

struct FVectorKernelACos : public TUnaryVectorKernel<FVectorKernelACos>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorMultiply(VectorACos(Src0), GlobalVectorConstants::OneOverTwoPi);
	}
};

struct FVectorKernelATan : public TUnaryVectorKernel<FVectorKernelATan>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		*Dst = VectorMultiply(VectorATan(Src0), GlobalVectorConstants::OneOverTwoPi);
	}
};

struct FVectorKernelATan2 : public TBinaryVectorKernel<FVectorKernelATan2>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0, VectorRegister Src1)
	{
		*Dst = VectorMultiply(VectorATan2(Src0, Src1), GlobalVectorConstants::OneOverTwoPi);
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
		VectorRegister Tmp = VectorSubtract(Src1, Src0);
		Tmp = VectorMultiply(Tmp, GlobalVectorConstants::BigNumber);
		Tmp = VectorMin(Tmp, GlobalVectorConstants::FloatOne);
		*Dst = VectorMax(Tmp, GlobalVectorConstants::FloatZero);
	}
};

struct FVectorKernelSelect : public TTrinaryVectorKernel<FVectorKernelSelect>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Mask, VectorRegister A, VectorRegister B)
	{
		//Currently works by cmpgt 0 to match the current, all vector float vm/scripts but with int support, this should probably change to direct use of a mask.
		*Dst = VectorSelect(VectorCompareGT(Mask, GlobalVectorConstants::FloatZero), A, B);
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


/* gaussian distribution random number (not working yet) */
struct FVectorKernelRandomGauss : public TBinaryVectorKernel<FVectorKernelRandomGauss>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0, VectorRegister Src1)
	{
		const float rm = RAND_MAX;
		VectorRegister Result = MakeVectorRegister(static_cast<float>(FMath::Rand()) / rm,
			static_cast<float>(FMath::Rand()) / rm,
			static_cast<float>(FMath::Rand()) / rm,
			static_cast<float>(FMath::Rand()) / rm);

		Result = VectorSubtract(Result, MakeVectorRegister(0.5f, 0.5f, 0.5f, 0.5f));
		Result = VectorMultiply(MakeVectorRegister(3.0f, 3.0f, 3.0f, 3.0f), Result);

		// taylor series gaussian approximation
		const VectorRegister SPi2 = VectorReciprocal(VectorReciprocalSqrt(MakeVectorRegister(2 * PI, 2 * PI, 2 * PI, 2 * PI)));
		VectorRegister Gauss = VectorReciprocal(SPi2);
		VectorRegister Div = VectorMultiply(MakeVectorRegister(2.0f, 2.0f, 2.0f, 2.0f), SPi2);
		Gauss = VectorSubtract(Gauss, VectorDivide(VectorMultiply(Result, Result), Div));
		Div = VectorMultiply(MakeVectorRegister(8.0f, 8.0f, 8.0f, 8.0f), SPi2);
		Gauss = VectorAdd(Gauss, VectorDivide(VectorPow(MakeVectorRegister(4.0f, 4.0f, 4.0f, 4.0f), Result), Div));
		Div = VectorMultiply(MakeVectorRegister(48.0f, 48.0f, 48.0f, 48.0f), SPi2);
		Gauss = VectorSubtract(Gauss, VectorDivide(VectorPow(MakeVectorRegister(6.0f, 6.0f, 6.0f, 6.0f), Result), Div));

		Gauss = VectorDivide(Gauss, MakeVectorRegister(0.4f, 0.4f, 0.4f, 0.4f));
		Gauss = VectorMultiply(Gauss, Src0);
		*Dst = Gauss;
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
	static VectorRegister RandomTable[17][17][17];

	static void VM_FORCEINLINE DoKernel(VectorRegister* RESTRICT Dst, VectorRegister Src0)
	{
		const VectorRegister One = MakeVectorRegister(1.0f, 1.0f, 1.0f, 1.0f);
		const VectorRegister VecSize = MakeVectorRegister(16.0f, 16.0f, 16.0f, 16.0f);

		*Dst = MakeVectorRegister(0.0f, 0.0f, 0.0f, 0.0f);
		
		for (uint32 i = 1; i < 2; i++)
		{
			float Di = 0.2f * (1.0f/(1<<i));
			VectorRegister Div = MakeVectorRegister(Di, Di, Di, Di);
			VectorRegister Coords = VectorMod( VectorAbs( VectorMultiply(Src0, Div) ), VecSize );
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

VectorRegister FVectorKernelNoise::RandomTable[17][17][17];

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

struct FVectorKernelOutputStreamed : public TUnaryVectorKernel<FVectorKernelOutputStreamed>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* Dst, VectorRegister Src0)
	{
		VectorStoreAlignedStreamed(Src0, Dst);
	}
};

struct FVectorKernelOutput : public TUnaryVectorKernel<FVectorKernelOutput>
{
	static void VM_FORCEINLINE DoKernel(VectorRegister* Dst, VectorRegister Src0)
	{
		VectorStoreAligned(Src0, Dst);
	}
};

//////////////////////////////////////////////////////////////////////////
//Shared data

struct FSharedDataHandlerBase
{
	int32 SharedDataIdx;
	FVectorVMSharedDataView& SharedData;
	int32 VarIndex;
	FRegisterHandler<int32, 4> IndexRegisterHandler;

	FSharedDataHandlerBase(FVectorVMContext& Context)
		: SharedDataIdx(DecodeU8(Context))
		, SharedData(Context.SharedDataTable[SharedDataIdx])
		, VarIndex(DecodeU8(Context))
		, IndexRegisterHandler(Context)
	{
	}

	VM_FORCEINLINE void Advance(){ IndexRegisterHandler.Advance(); }
	VM_FORCEINLINE int32 GetSharedDataIndex(){ return SharedDataIdx; }
	VM_FORCEINLINE int32 GetVarIndex(){ return VarIndex; }
	VM_FORCEINLINE int32 GetDataIndex(){ return IndexRegisterHandler.Get(); }
};

struct FSharedDataHandler : public FSharedDataHandlerBase
{
	FSharedDataHandler(FVectorVMContext& Context)
	: FSharedDataHandlerBase(Context)
	{
	}

	VM_FORCEINLINE VectorRegister Get()
	{
		return VectorLoad((VectorRegister*)SharedData.GetReadBuffer(VarIndex, IndexRegisterHandler.Get()));
	}
};

struct FSharedDataDestHandler : public FSharedDataHandlerBase
{
	FSharedDataDestHandler(FVectorVMContext& Context)
	: FSharedDataHandlerBase(Context)
	{
	}

	VM_FORCEINLINE VectorRegister* Get()
	{
		return (VectorRegister*)SharedData.GetWriteBuffer(VarIndex, IndexRegisterHandler.Get());
	}
	VM_FORCEINLINE VectorRegister GetValue()
	{
		return VectorLoadAligned(Get());
	}
};

/** 
Temporary Vector only version of this.
*/
struct FVectorKernelSharedDataWrite : public TUnaryKernel<FVectorKernelSharedDataWrite, FSharedDataDestHandler, FRegisterHandler<VectorRegister>>
{
	static const int32 NumInstancesPerOp = 1;
	static void VM_FORCEINLINE DoKernel(VectorRegister* Buffer, VectorRegister Data)
	{
		VectorStoreAlignedStreamed(Data, (float*)(Buffer));
	}
}; 

/**
Temporary Vector only version of this.
*/
struct FVectorKernelSharedDataRead : public TUnaryKernel<FVectorKernelSharedDataRead, FRegisterDestHandler<VectorRegister>, FSharedDataHandler>
{
	static const int32 NumInstancesPerOp = 1;
	static void VM_FORCEINLINE DoKernel(VectorRegister* Dst, VectorRegister Data)
	{
		*Dst = Data;
	}
};

struct FSharedDataIndexHandlerBase
{
	int32 SharedDataIndex;
	int32 CurrIndex;
	FVectorVMSharedDataView& SharedData;

	VM_FORCEINLINE void Advance(){}
	int32 Get(){ return CurrIndex; }
	FSharedDataIndexHandlerBase(FVectorVMContext& Context) 
		: SharedDataIndex(DecodeU8(Context))
		, SharedData(Context.SharedDataTable[SharedDataIndex])
	{	}
};
struct FSharedDataIndexHandler_Acquire : public FSharedDataIndexHandlerBase
{
	FSharedDataIndexHandler_Acquire(FVectorVMContext& Context) : FSharedDataIndexHandlerBase(Context)	{	}
	int32 GetNextIndex(){ CurrIndex = SharedData.AcquireIndexWrap(); return CurrIndex; }
};
struct FSharedDataIndexHandler_AcquireWrap : public FSharedDataIndexHandlerBase
{
	FSharedDataIndexHandler_AcquireWrap(FVectorVMContext& Context) : FSharedDataIndexHandlerBase(Context)	{	}
	int32 GetNextIndex(){ CurrIndex = SharedData.AcquireIndexWrap();  return CurrIndex; }
};
struct FSharedDataIndexHandler_Consume : public FSharedDataIndexHandlerBase
{
	FSharedDataIndexHandler_Consume(FVectorVMContext& Context) : FSharedDataIndexHandlerBase(Context)	{	}
	int32 GetNextIndex(){ CurrIndex = SharedData.ConsumeIndex();  return CurrIndex; }
};
struct FSharedDataIndexHandler_ConsumeWrap : public FSharedDataIndexHandlerBase
{
	FSharedDataIndexHandler_ConsumeWrap(FVectorVMContext& Context) : FSharedDataIndexHandlerBase(Context)	{	}
	int32 GetNextIndex(){ CurrIndex = SharedData.ConsumeIndexWrap();  return CurrIndex; }
};

//Temporary until after scalarization and we can store the size and counter of shared data in constant data.
template <typename IndexHandler>
struct FKernelSharedDataGetAppendIndexBase
{
	static const int32 NumInstancesPerOp = 1;
	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		FRegisterDestHandler<int32, 4> IndexDest(Context);
		FRegisterHandler<float, 4> ValidSrc(Context);
		IndexHandler IdxHandler(Context);
		int32 NumInstances = Context.NumInstances;
		for (int32 i = 0; i < NumInstances; ++i)
		{
			int32 Index = ValidSrc.Get() > 0.0f ? IdxHandler.GetNextIndex() : INDEX_NONE;
			int32* Dest = IndexDest.Get();
			Dest[0] = Index; Dest[1] = Index; Dest[2] = Index; Dest[3] = Index;

			ValidSrc.Advance();
			IndexDest.Advance();
			IdxHandler.Advance();
		}
	}
};

typedef FKernelSharedDataGetAppendIndexBase<FSharedDataIndexHandler_Acquire> FKernelSharedDataGetAppendIndex;
typedef FKernelSharedDataGetAppendIndexBase<FSharedDataIndexHandler_AcquireWrap> FKernelSharedDataGetAppendIndex_Wrap;

//Temporary until after scalarization and we can store the size and counter of shared data in constant data.
template<typename IndexHandler>
struct FKernelSharedDataGetConsumeIndexBase
{
	static const int32 NumInstancesPerOp = 1;

	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		FRegisterDestHandler<int32, 4> IndexDest(Context);
		IndexHandler IdxHandler(Context);
		int32 NumInstances = Context.NumInstances;
		for (int32 i = 0; i < NumInstances; ++i)
		{
			int32 Index = IdxHandler.GetNextIndex();
			//Better to just stay in int pipeline?
			*IndexDest.Get() = Index;//Only need index in X;

			IndexDest.Advance();
			IdxHandler.Advance();
		}
	}
};

typedef FKernelSharedDataGetConsumeIndexBase<FSharedDataIndexHandler_Consume> FKernelSharedDataGetConsumeIndex;
typedef FKernelSharedDataGetConsumeIndexBase<FSharedDataIndexHandler_ConsumeWrap> FKernelSharedDataGetConsumeIndex_Wrap;

//Temporary until after scalarization and we can store the size and counter of shared data in constant data.
struct FKernelSharedDataIndexValid
{
	static VM_FORCEINLINE void Exec(FVectorVMContext& Context)
	{
		FRegisterDestHandler<float, 4> ValidDest(Context);
		FRegisterHandler<int32, 4> IndexSrc(Context);
		FVectorVMSharedDataView& SharedData = Context.SharedDataTable[DecodeU8(Context)];
		int32 NumInstances = Context.NumInstances;
		for (int32 i = 0; i < NumInstances; ++i)
		{
			int32 Index = IndexSrc.Get();
			float Valid = SharedData.ValidIndex(Index) ? 1.0f : 0.0f;
			//Better to just stay in int pipeline?
			float* Dst = ValidDest.Get();
			Dst[0] = Valid; Dst[1] = Valid; Dst[2] = Valid; Dst[3] = Valid;

			ValidDest.Advance();
			IndexSrc.Advance();;
		}
	}
};

void VectorVM::Init()
{
	static bool Inited = false;
	if (Inited == false)
	{
		// random noise
		float TempTable[17][17][17];
		for (int z = 0; z < 17; z++)
		{
			for (int y = 0; y < 17; y++)
			{
				for (int x = 0; x < 17; x++)
				{
					float f1 = (float)FMath::FRandRange(-1.0f, 1.0f);
					TempTable[x][y][z] = f1;
				}
			}
		}

		// pad
		for (int i = 0; i < 17; i++)
		{
			for (int j = 0; j < 17; j++)
			{
				TempTable[i][j][16] = TempTable[i][j][0];
				TempTable[i][16][j] = TempTable[i][0][j];
				TempTable[16][j][i] = TempTable[0][j][i];
			}
		}

		// compute gradients
		FVector TempTable2[17][17][17];
		for (int z = 0; z < 16; z++)
		{
			for (int y = 0; y < 16; y++)
			{
				for (int x = 0; x < 16; x++)
				{
					FVector XGrad = FVector(1.0f, 0.0f, TempTable[x][y][z] - TempTable[x+1][y][z]);
					FVector YGrad = FVector(0.0f, 1.0f, TempTable[x][y][z] - TempTable[x][y + 1][z]);
					FVector ZGrad = FVector(0.0f, 1.0f, TempTable[x][y][z] - TempTable[x][y][z+1]);

					FVector Grad = FVector(XGrad.Z, YGrad.Z, ZGrad.Z);
					TempTable2[x][y][z] = Grad;
				}
			}
		}

		// pad
		for (int i = 0; i < 17; i++)
		{
			for (int j = 0; j < 17; j++)
			{
				TempTable2[i][j][16] = TempTable2[i][j][0];
				TempTable2[i][16][j] = TempTable2[i][0][j];
				TempTable2[16][j][i] = TempTable2[0][j][i];
			}
		}


		// compute curl of gradient field
		for (int z = 0; z < 16; z++)
		{
			for (int y = 0; y < 16; y++)
			{
				for (int x = 0; x < 16; x++)
				{
					FVector Dy = TempTable2[x][y][z] - TempTable2[x][y + 1][z];
					FVector Sy = TempTable2[x][y][z] + TempTable2[x][y + 1][z];
					FVector Dx = TempTable2[x][y][z] - TempTable2[x + 1][y][z];
					FVector Sx = TempTable2[x][y][z] + TempTable2[x + 1][y][z];
					FVector Dz = TempTable2[x][y][z] - TempTable2[x][y][z + 1];
					FVector Sz = TempTable2[x][y][z] + TempTable2[x][y][z + 1];
					FVector Dir = FVector(Dy.Z - Sz.Y, Dz.X - Sx.Z, Dx.Y - Sy.X);

					FVectorKernelNoise::RandomTable[x][y][z] = MakeVectorRegister(Dir.X, Dir.Y, Dir.Z, 0.0f);
				}
			}
		}

#if WITH_EDITOR

		OpNames.AddDefaulted((int32)EVectorVMOp::NumOpcodes);
		if (UEnum* EnumStateObj = FindObject<UEnum>(ANY_PACKAGE, TEXT("EVectorVMOp"), true))
		{
			for (int32 i = 0; i < (int32)EVectorVMOp::NumOpcodes; ++i)
			{
				OpNames[i] = EnumStateObj->GetDisplayNameText(i).ToString();
			}
		}

		OperandLocationNames.AddDefaulted((int32)EVectorVMOperandLocation::Num);
		if (UEnum* EnumStateObj = FindObject<UEnum>(ANY_PACKAGE, TEXT("EVectorVMOperandLocation"), true))
		{
			for (int32 i = 0; i < (int32)EVectorVMOperandLocation::Num; ++i)
			{
				OperandLocationNames[i] = EnumStateObj->GetDisplayNameText(i).ToString();
			}
		}

#endif

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
	FVectorVMSharedDataView* SharedDataTable,
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
		int32 InstancesThisChunk = VectorsThisChunk;
		FVectorVMContext Context(Code, RegisterTable, ConstantTable, SharedDataTable, VectorsThisChunk, InstancesThisChunk, VectorsPerChunk * ChunkIndex);
		EVectorVMOp Op = EVectorVMOp::done;

		// Execute VM on all vectors in this chunk.
		do 
		{
			Op = DecodeOp(Context);
			Context.SetOp(Op);
			switch (Op)
			{
			// Dispatch kernel ops.
			case EVectorVMOp::add: FVectorKernelAdd::Exec(Context); break;
			case EVectorVMOp::sub: FVectorKernelSub::Exec(Context); break;
			case EVectorVMOp::mul: FVectorKernelMul::Exec(Context); break;
			case EVectorVMOp::div: FVectorKernelDiv::Exec(Context); break;
			case EVectorVMOp::mad: FVectorKernelMad::Exec(Context); break;
			case EVectorVMOp::lerp: FVectorKernelLerp::Exec(Context); break;
			case EVectorVMOp::rcp: FVectorKernelRcp::Exec(Context); break;
			case EVectorVMOp::rsq: FVectorKernelRsq::Exec(Context); break;
			case EVectorVMOp::sqrt: FVectorKernelSqrt::Exec(Context); break;
			case EVectorVMOp::neg: FVectorKernelNeg::Exec(Context); break;
			case EVectorVMOp::abs: FVectorKernelAbs::Exec(Context); break;
			case EVectorVMOp::exp: FVectorKernelExp::Exec(Context); break;
			case EVectorVMOp::exp2: FVectorKernelExp2::Exec(Context); break;
			case EVectorVMOp::log: FVectorKernelLog::Exec(Context); break;
			case EVectorVMOp::log2: FVectorKernelLog2::Exec(Context); break;
			case EVectorVMOp::sin: FVectorKernelSin::Exec(Context); break;
			case EVectorVMOp::cos: FVectorKernelCos::Exec(Context); break;
			case EVectorVMOp::tan: FVectorKernelTan::Exec(Context); break;
			case EVectorVMOp::asin: FVectorKernelASin::Exec(Context); break;
			case EVectorVMOp::acos: FVectorKernelACos::Exec(Context); break;
			case EVectorVMOp::atan: FVectorKernelATan::Exec(Context); break;
			case EVectorVMOp::atan2: FVectorKernelATan2::Exec(Context); break;
			case EVectorVMOp::ceil: FVectorKernelCeil::Exec(Context); break;
			case EVectorVMOp::floor: FVectorKernelFloor::Exec(Context); break;
			case EVectorVMOp::fmod: FVectorKernelMod::Exec(Context); break;
			case EVectorVMOp::frac: FVectorKernelFrac::Exec(Context); break;
			case EVectorVMOp::trunc: FVectorKernelTrunc::Exec(Context); break;
			case EVectorVMOp::clamp: FVectorKernelClamp::Exec(Context); break;
			case EVectorVMOp::min: FVectorKernelMin::Exec(Context); break;
			case EVectorVMOp::max: FVectorKernelMax::Exec(Context); break;
			case EVectorVMOp::pow: FVectorKernelPow::Exec(Context); break;
			case EVectorVMOp::sign: FVectorKernelSign::Exec(Context); break;
			case EVectorVMOp::step: FVectorKernelStep::Exec(Context); break;
			case EVectorVMOp::dot: FVectorKernelDot::Exec(Context); break;
			case EVectorVMOp::cross: FVectorKernelCross::Exec(Context); break;
			case EVectorVMOp::normalize: FVectorKernelNormalize::Exec(Context); break;
			case EVectorVMOp::random: FVectorKernelRandom::Exec(Context); break;
			case EVectorVMOp::length: FVectorKernelLength::Exec(Context); break;
			case EVectorVMOp::noise: FVectorKernelNoise::Exec(Context); break;
			case EVectorVMOp::splatx: FVectorKernelSplat<0>::Exec(Context); break;
			case EVectorVMOp::splaty: FVectorKernelSplat<1>::Exec(Context); break;
			case EVectorVMOp::splatz: FVectorKernelSplat<2>::Exec(Context); break;
			case EVectorVMOp::splatw: FVectorKernelSplat<3>::Exec(Context); break;
			case EVectorVMOp::compose: FVectorKernelCompose<0,1,2,3>::Exec(Context); break;
			case EVectorVMOp::composex: FVectorKernelCompose<0, 0, 0, 0>::Exec(Context); break;
			case EVectorVMOp::composey: FVectorKernelCompose<1, 1, 1, 1>::Exec(Context); break;
			case EVectorVMOp::composez: FVectorKernelCompose<2, 2, 2, 2>::Exec(Context); break;
			case EVectorVMOp::composew: FVectorKernelCompose<3, 3, 3, 3>::Exec(Context); break;
			case EVectorVMOp::lessthan: FVectorKernelLessThan::Exec(Context); break;
			case EVectorVMOp::easein: FVectorKernelEaseIn::Exec(Context); break;
			case EVectorVMOp::easeinout: FVectorKernelEaseInOut::Exec(Context); break;
			case EVectorVMOp::aquireshareddataindex: FKernelSharedDataGetAppendIndex::Exec(Context); break;
			case EVectorVMOp::aquireshareddataindexwrap: FKernelSharedDataGetAppendIndex_Wrap::Exec(Context); break;
			case EVectorVMOp::consumeshareddataindex: FKernelSharedDataGetConsumeIndex::Exec(Context); break;
			case EVectorVMOp::consumeshareddataindexwrap: FKernelSharedDataGetConsumeIndex_Wrap::Exec(Context); break;
			case EVectorVMOp::shareddataread: FVectorKernelSharedDataRead::Exec(Context); break;
			case EVectorVMOp::shareddatawrite: FVectorKernelSharedDataWrite::Exec(Context); break;
			case EVectorVMOp::shareddataindexvalid: FKernelSharedDataIndexValid::Exec(Context); break;
			case EVectorVMOp::select: FVectorKernelSelect::Exec(Context); break;
			case EVectorVMOp::output:
			{
				FVectorKernelOutput::Exec(Context);
			}
				break;

			// Execution always terminates with a "done" opcode.
			case EVectorVMOp::done:
				break;

			// Opcode not recognized / implemented.
			default:
				UE_LOG(LogVectorVM, Error, TEXT("Unknown op code 0x%02x"), (uint32)Op);
				return;//BAIL
			}
		} while (Op != EVectorVMOp::done);

		NumVectors -= VectorsPerChunk;
	}
}

uint8 VectorVM::GetNumOpCodes()
{
	return (uint8)EVectorVMOp::NumOpcodes;
}

#if WITH_EDITOR
FString VectorVM::GetOpName(EVectorVMOp Op)
{
	return OpNames[(int32)Op];
}

FString VectorVM::GetOperandLocationName(EVectorVMOperandLocation Location)
{
	return OperandLocationNames[(int32)Location];
}
#endif

#undef VM_FORCEINLINE
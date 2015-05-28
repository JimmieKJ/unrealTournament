// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	RHIMETHOD_SPECIFIERSs.h: The RHI method definitions.  The same methods are defined multiple places, so they're simply included from this file where necessary.
=============================================================================*/

// DEFINE_RHIMETHOD is used by the includer to modify the RHI method definitions.
// It's defined by the dynamic RHI to pass parameters from the statically bound RHI method to the dynamically bound RHI method.
// To enable that, the parameter list must be given a second time, as they will be passed to the dynamically bound RHI method.
// The last parameter should be return if the method returns a value, and nothing otherwise.
#ifndef DEFINE_RHIMETHOD
#define GENERATE_VAX_FUNCTION_DECLARATIONS
#endif

#undef DEFINE_RHIMETHOD_0
#undef DEFINE_RHIMETHOD_1
#undef DEFINE_RHIMETHOD_2
#undef DEFINE_RHIMETHOD_3
#undef DEFINE_RHIMETHOD_4
#undef DEFINE_RHIMETHOD_5
#undef DEFINE_RHIMETHOD_6
#undef DEFINE_RHIMETHOD_7
#undef DEFINE_RHIMETHOD_8
#undef DEFINE_RHIMETHOD_9
#undef DEFINE_RHIMETHOD_CMDLIST_0
#undef DEFINE_RHIMETHOD_CMDLIST_1
#undef DEFINE_RHIMETHOD_CMDLIST_2
#undef DEFINE_RHIMETHOD_CMDLIST_3
#undef DEFINE_RHIMETHOD_CMDLIST_4
#undef DEFINE_RHIMETHOD_CMDLIST_5
#undef DEFINE_RHIMETHOD_CMDLIST_6
#undef DEFINE_RHIMETHOD_CMDLIST_7
#undef DEFINE_RHIMETHOD_CMDLIST_8
#undef DEFINE_RHIMETHOD_CMDLIST_9
#undef DEFINE_RHIMETHOD_GLOBAL_0
#undef DEFINE_RHIMETHOD_GLOBAL_1
#undef DEFINE_RHIMETHOD_GLOBAL_2
#undef DEFINE_RHIMETHOD_GLOBAL_3
#undef DEFINE_RHIMETHOD_GLOBAL_4
#undef DEFINE_RHIMETHOD_GLOBAL_5
#undef DEFINE_RHIMETHOD_GLOBAL_6
#undef DEFINE_RHIMETHOD_GLOBAL_7
#undef DEFINE_RHIMETHOD_GLOBAL_8
#undef DEFINE_RHIMETHOD_GLOBAL_9
#undef DEFINE_RHIMETHOD_GLOBALFLUSH_0
#undef DEFINE_RHIMETHOD_GLOBALFLUSH_1
#undef DEFINE_RHIMETHOD_GLOBALFLUSH_2
#undef DEFINE_RHIMETHOD_GLOBALFLUSH_3
#undef DEFINE_RHIMETHOD_GLOBALFLUSH_4
#undef DEFINE_RHIMETHOD_GLOBALFLUSH_5
#undef DEFINE_RHIMETHOD_GLOBALFLUSH_6
#undef DEFINE_RHIMETHOD_GLOBALFLUSH_7
#undef DEFINE_RHIMETHOD_GLOBALFLUSH_8
#undef DEFINE_RHIMETHOD_GLOBALFLUSH_9
#undef DEFINE_RHIMETHOD_GLOBALTHREADSAFE_0
#undef DEFINE_RHIMETHOD_GLOBALTHREADSAFE_1
#undef DEFINE_RHIMETHOD_GLOBALTHREADSAFE_2
#undef DEFINE_RHIMETHOD_GLOBALTHREADSAFE_3
#undef DEFINE_RHIMETHOD_GLOBALTHREADSAFE_4
#undef DEFINE_RHIMETHOD_GLOBALTHREADSAFE_5
#undef DEFINE_RHIMETHOD_GLOBALTHREADSAFE_6
#undef DEFINE_RHIMETHOD_GLOBALTHREADSAFE_7
#undef DEFINE_RHIMETHOD_GLOBALTHREADSAFE_8
#undef DEFINE_RHIMETHOD_GLOBALTHREADSAFE_9

#ifdef GENERATE_VAX_FUNCTION_DECLARATIONS

	#define DEFINE_RHIMETHOD_0(ReturnType,MethodName,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal();
	#define DEFINE_RHIMETHOD_1(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal(ParameterTypeA ParameterNameA);
	#define DEFINE_RHIMETHOD_2(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB);
	#define DEFINE_RHIMETHOD_3(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC);
	#define DEFINE_RHIMETHOD_4(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD);
	#define DEFINE_RHIMETHOD_5(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE);
	#define DEFINE_RHIMETHOD_6(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF);
	#define DEFINE_RHIMETHOD_7(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG);
	#define DEFINE_RHIMETHOD_8(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH);
	#define DEFINE_RHIMETHOD_9(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ParameterTypeI,ParameterNameI,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH, ParameterTypeI ParameterNameI);

	#define DEFINE_RHIMETHOD_CMDLIST_0(ReturnType,MethodName,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal();
	#define DEFINE_RHIMETHOD_CMDLIST_1(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal(ParameterTypeA ParameterNameA);
	#define DEFINE_RHIMETHOD_CMDLIST_2(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB);
	#define DEFINE_RHIMETHOD_CMDLIST_3(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC);
	#define DEFINE_RHIMETHOD_CMDLIST_4(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD);
	#define DEFINE_RHIMETHOD_CMDLIST_5(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE);
	#define DEFINE_RHIMETHOD_CMDLIST_6(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF);
	#define DEFINE_RHIMETHOD_CMDLIST_7(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG);
	#define DEFINE_RHIMETHOD_CMDLIST_8(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH);
	#define DEFINE_RHIMETHOD_CMDLIST_9(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ParameterTypeI,ParameterNameI,ReturnStatement,NullImplementation) \
		ReturnType MethodName##_Internal(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH, ParameterTypeI ParameterNameI);

	#define DEFINE_RHIMETHOD_GLOBAL_0(ReturnType,MethodName,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName();
	#define DEFINE_RHIMETHOD_GLOBAL_1(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA);
	#define DEFINE_RHIMETHOD_GLOBAL_2(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB);
	#define DEFINE_RHIMETHOD_GLOBAL_3(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC);
	#define DEFINE_RHIMETHOD_GLOBAL_4(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD);
	#define DEFINE_RHIMETHOD_GLOBAL_5(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE);
	#define DEFINE_RHIMETHOD_GLOBAL_6(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF);
	#define DEFINE_RHIMETHOD_GLOBAL_7(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG);
	#define DEFINE_RHIMETHOD_GLOBAL_8(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH);
	#define DEFINE_RHIMETHOD_GLOBAL_9(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ParameterTypeI,ParameterNameI,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH, ParameterTypeI ParameterNameI);

	#define DEFINE_RHIMETHOD_GLOBALFLUSH_0(ReturnType,MethodName,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName();
	#define DEFINE_RHIMETHOD_GLOBALFLUSH_1(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA);
	#define DEFINE_RHIMETHOD_GLOBALFLUSH_2(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB);
	#define DEFINE_RHIMETHOD_GLOBALFLUSH_3(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC);
	#define DEFINE_RHIMETHOD_GLOBALFLUSH_4(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD);
	#define DEFINE_RHIMETHOD_GLOBALFLUSH_5(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE);
	#define DEFINE_RHIMETHOD_GLOBALFLUSH_6(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF);
	#define DEFINE_RHIMETHOD_GLOBALFLUSH_7(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG);
	#define DEFINE_RHIMETHOD_GLOBALFLUSH_8(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH);
	#define DEFINE_RHIMETHOD_GLOBALFLUSH_9(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ParameterTypeI,ParameterNameI,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH, ParameterTypeI ParameterNameI);

	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_0(ReturnType,MethodName,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName();
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_1(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA);
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_2(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB);
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_3(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC);
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_4(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD);
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_5(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE);
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_6(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF);
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_7(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG);
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_8(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH);
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_9(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ParameterTypeI,ParameterNameI,ReturnStatement,NullImplementation) \
		ReturnType RHI##MethodName(ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH, ParameterTypeI ParameterNameI);

#else

	#define DEFINE_RHIMETHOD_0(ReturnType,MethodName,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType,MethodName,(),(),ReturnStatement,NullImplementation)
	#define DEFINE_RHIMETHOD_1(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType,MethodName,(ParameterTypeA ParameterNameA),(ParameterNameA),ReturnStatement,NullImplementation)
	#define DEFINE_RHIMETHOD_2(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType,MethodName,(ParameterTypeA ParameterNameA,ParameterTypeB ParameterNameB),(ParameterNameA,ParameterNameB),ReturnStatement,NullImplementation)
	#define DEFINE_RHIMETHOD_3(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType,MethodName,(ParameterTypeA ParameterNameA,ParameterTypeB ParameterNameB,ParameterTypeC ParameterNameC),(ParameterNameA,ParameterNameB,ParameterNameC),ReturnStatement,NullImplementation)
	#define DEFINE_RHIMETHOD_4(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType,MethodName,(ParameterTypeA ParameterNameA,ParameterTypeB ParameterNameB,ParameterTypeC ParameterNameC,ParameterTypeD ParameterNameD),(ParameterNameA,ParameterNameB,ParameterNameC,ParameterNameD),ReturnStatement,NullImplementation)
	#define DEFINE_RHIMETHOD_5(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType,MethodName,(ParameterTypeA ParameterNameA,ParameterTypeB ParameterNameB,ParameterTypeC ParameterNameC,ParameterTypeD ParameterNameD,ParameterTypeE ParameterNameE),(ParameterNameA,ParameterNameB,ParameterNameC,ParameterNameD,ParameterNameE),ReturnStatement,NullImplementation)
	#define DEFINE_RHIMETHOD_6(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType,MethodName,(ParameterTypeA ParameterNameA,ParameterTypeB ParameterNameB,ParameterTypeC ParameterNameC,ParameterTypeD ParameterNameD,ParameterTypeE ParameterNameE,ParameterTypeF ParameterNameF),(ParameterNameA,ParameterNameB,ParameterNameC,ParameterNameD,ParameterNameE,ParameterNameF),ReturnStatement,NullImplementation)
	#define DEFINE_RHIMETHOD_7(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType,MethodName,(ParameterTypeA ParameterNameA,ParameterTypeB ParameterNameB,ParameterTypeC ParameterNameC,ParameterTypeD ParameterNameD,ParameterTypeE ParameterNameE,ParameterTypeF ParameterNameF,ParameterTypeG ParameterNameG),(ParameterNameA,ParameterNameB,ParameterNameC,ParameterNameD,ParameterNameE,ParameterNameF,ParameterNameG),ReturnStatement,NullImplementation)
	#define DEFINE_RHIMETHOD_8(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType,MethodName,(ParameterTypeA ParameterNameA,ParameterTypeB ParameterNameB,ParameterTypeC ParameterNameC,ParameterTypeD ParameterNameD,ParameterTypeE ParameterNameE,ParameterTypeF ParameterNameF,ParameterTypeG ParameterNameG,ParameterTypeH ParameterNameH),(ParameterNameA,ParameterNameB,ParameterNameC,ParameterNameD,ParameterNameE,ParameterNameF,ParameterNameG,ParameterNameH),ReturnStatement,NullImplementation)
	#define DEFINE_RHIMETHOD_9(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ParameterTypeI,ParameterNameI,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType,MethodName,(ParameterTypeA ParameterNameA,ParameterTypeB ParameterNameB,ParameterTypeC ParameterNameC,ParameterTypeD ParameterNameD,ParameterTypeE ParameterNameE,ParameterTypeF ParameterNameF,ParameterTypeG ParameterNameG,ParameterTypeH ParameterNameH,ParameterTypeI ParameterNameI),(ParameterNameA,ParameterNameB,ParameterNameC,ParameterNameD,ParameterNameE,ParameterNameF,ParameterNameG,ParameterNameH,ParameterNameI),ReturnStatement,NullImplementation)

	#if defined(DEFINE_RHIMETHOD_CMDLIST)
		#define DEFINE_RHIMETHOD_CMDLIST_0(ReturnType,MethodName,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_CMDLIST(ReturnType, MethodName, (), (), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_CMDLIST_1(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_CMDLIST(ReturnType, MethodName, (ParameterTypeA ParameterNameA), (ParameterNameA), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_CMDLIST_2(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_CMDLIST(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB), (ParameterNameA, ParameterNameB), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_CMDLIST_3(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_CMDLIST(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC), (ParameterNameA, ParameterNameB, ParameterNameC), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_CMDLIST_4(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_CMDLIST(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_CMDLIST_5(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_CMDLIST(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_CMDLIST_6(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_CMDLIST(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_CMDLIST_7(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_CMDLIST(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_CMDLIST_8(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_CMDLIST(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG, ParameterNameH), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_CMDLIST_9(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ParameterTypeI,ParameterNameI,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_CMDLIST(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH, ParameterTypeI ParameterNameI), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG, ParameterNameH, ParameterNameI), ReturnStatement, NullImplementation)
	#else
		#define DEFINE_RHIMETHOD_CMDLIST_0(ReturnType,MethodName,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (), (), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_CMDLIST_1(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA), (ParameterNameA), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_CMDLIST_2(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB), (ParameterNameA, ParameterNameB), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_CMDLIST_3(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC), (ParameterNameA, ParameterNameB, ParameterNameC), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_CMDLIST_4(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_CMDLIST_5(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_CMDLIST_6(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_CMDLIST_7(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_CMDLIST_8(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG, ParameterNameH), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_CMDLIST_9(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ParameterTypeI,ParameterNameI,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH, ParameterTypeI ParameterNameI), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG, ParameterNameH, ParameterNameI), ReturnStatement, NullImplementation)
	#endif

	#if defined(DEFINE_RHIMETHOD_GLOBAL)
		#define DEFINE_RHIMETHOD_GLOBAL_0(ReturnType,MethodName,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBAL(ReturnType, MethodName, (), (), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBAL_1(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBAL(ReturnType, MethodName, (ParameterTypeA ParameterNameA), (ParameterNameA), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBAL_2(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBAL(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB), (ParameterNameA, ParameterNameB), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBAL_3(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBAL(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC), (ParameterNameA, ParameterNameB, ParameterNameC), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBAL_4(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBAL(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBAL_5(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBAL(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBAL_6(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBAL(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBAL_7(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBAL(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBAL_8(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBAL(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG, ParameterNameH), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBAL_9(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ParameterTypeI,ParameterNameI,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBAL(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH, ParameterTypeI ParameterNameI), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG, ParameterNameH, ParameterNameI), ReturnStatement, NullImplementation)
	#else
		#define DEFINE_RHIMETHOD_GLOBAL_0(ReturnType,MethodName,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (), (), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBAL_1(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA), (ParameterNameA), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBAL_2(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB), (ParameterNameA, ParameterNameB), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBAL_3(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC), (ParameterNameA, ParameterNameB, ParameterNameC), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBAL_4(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBAL_5(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBAL_6(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBAL_7(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBAL_8(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG, ParameterNameH), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBAL_9(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ParameterTypeI,ParameterNameI,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH, ParameterTypeI ParameterNameI), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG, ParameterNameH, ParameterNameI), ReturnStatement, NullImplementation)
	#endif

	#if defined(DEFINE_RHIMETHOD_GLOBALFLUSH)
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_0(ReturnType,MethodName,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBALFLUSH(ReturnType, MethodName, (), (), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_1(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBALFLUSH(ReturnType, MethodName, (ParameterTypeA ParameterNameA), (ParameterNameA), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_2(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBALFLUSH(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB), (ParameterNameA, ParameterNameB), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_3(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBALFLUSH(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC), (ParameterNameA, ParameterNameB, ParameterNameC), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_4(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBALFLUSH(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_5(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBALFLUSH(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_6(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBALFLUSH(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_7(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBALFLUSH(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_8(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBALFLUSH(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG, ParameterNameH), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_9(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ParameterTypeI,ParameterNameI,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD_GLOBALFLUSH(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH, ParameterTypeI ParameterNameI), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG, ParameterNameH, ParameterNameI), ReturnStatement, NullImplementation)
	#else
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_0(ReturnType,MethodName,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (), (), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_1(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA), (ParameterNameA), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_2(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB), (ParameterNameA, ParameterNameB), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_3(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC), (ParameterNameA, ParameterNameB, ParameterNameC), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_4(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_5(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_6(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_7(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_8(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG, ParameterNameH), ReturnStatement, NullImplementation)
		#define DEFINE_RHIMETHOD_GLOBALFLUSH_9(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ParameterTypeI,ParameterNameI,ReturnStatement,NullImplementation) \
			DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH, ParameterTypeI ParameterNameI), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG, ParameterNameH, ParameterNameI), ReturnStatement, NullImplementation)
	#endif

#if defined(DEFINE_RHIMETHOD_GLOBALTHREADSAFE)
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_0(ReturnType,MethodName,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD_GLOBALTHREADSAFE(ReturnType, MethodName, (), (), ReturnStatement, NullImplementation)
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_1(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD_GLOBALTHREADSAFE(ReturnType, MethodName, (ParameterTypeA ParameterNameA), (ParameterNameA), ReturnStatement, NullImplementation)
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_2(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD_GLOBALTHREADSAFE(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB), (ParameterNameA, ParameterNameB), ReturnStatement, NullImplementation)
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_3(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD_GLOBALTHREADSAFE(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC), (ParameterNameA, ParameterNameB, ParameterNameC), ReturnStatement, NullImplementation)
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_4(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD_GLOBALTHREADSAFE(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD), ReturnStatement, NullImplementation)
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_5(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD_GLOBALTHREADSAFE(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE), ReturnStatement, NullImplementation)
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_6(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD_GLOBALTHREADSAFE(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF), ReturnStatement, NullImplementation)
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_7(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD_GLOBALTHREADSAFE(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG), ReturnStatement, NullImplementation)
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_8(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD_GLOBALTHREADSAFE(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG, ParameterNameH), ReturnStatement, NullImplementation)
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_9(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ParameterTypeI,ParameterNameI,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD_GLOBALTHREADSAFE(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH, ParameterTypeI ParameterNameI), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG, ParameterNameH, ParameterNameI), ReturnStatement, NullImplementation)
#else
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_0(ReturnType,MethodName,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType, MethodName, (), (), ReturnStatement, NullImplementation)
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_1(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA), (ParameterNameA), ReturnStatement, NullImplementation)
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_2(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB), (ParameterNameA, ParameterNameB), ReturnStatement, NullImplementation)
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_3(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC), (ParameterNameA, ParameterNameB, ParameterNameC), ReturnStatement, NullImplementation)
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_4(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD), ReturnStatement, NullImplementation)
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_5(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE), ReturnStatement, NullImplementation)
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_6(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF), ReturnStatement, NullImplementation)
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_7(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG), ReturnStatement, NullImplementation)
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_8(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG, ParameterNameH), ReturnStatement, NullImplementation)
	#define DEFINE_RHIMETHOD_GLOBALTHREADSAFE_9(ReturnType,MethodName,ParameterTypeA,ParameterNameA,ParameterTypeB,ParameterNameB,ParameterTypeC,ParameterNameC,ParameterTypeD,ParameterNameD,ParameterTypeE,ParameterNameE,ParameterTypeF,ParameterNameF,ParameterTypeG,ParameterNameG,ParameterTypeH,ParameterNameH,ParameterTypeI,ParameterNameI,ReturnStatement,NullImplementation) \
		DEFINE_RHIMETHOD(ReturnType, MethodName, (ParameterTypeA ParameterNameA, ParameterTypeB ParameterNameB, ParameterTypeC ParameterNameC, ParameterTypeD ParameterNameD, ParameterTypeE ParameterNameE, ParameterTypeF ParameterNameF, ParameterTypeG ParameterNameG, ParameterTypeH ParameterNameH, ParameterTypeI ParameterNameI), (ParameterNameA, ParameterNameB, ParameterNameC, ParameterNameD, ParameterNameE, ParameterNameF, ParameterNameG, ParameterNameH, ParameterNameI), ReturnStatement, NullImplementation)
#endif

#endif

//
// Interface for run-time gpu perf counters.
//

DEFINE_RHIMETHOD_2(
	void,GpuTimeBegin,
	uint32,Hash,
	bool,bCompute,
	return,return;
	);

DEFINE_RHIMETHOD_2(
	void,GpuTimeEnd,
	uint32,Hash,
	bool,bCompute,
	return,return;
	);


//
// RHI resource management functions.
//

DEFINE_RHIMETHOD_GLOBALTHREADSAFE_1(
	FSamplerStateRHIRef,CreateSamplerState,
	const FSamplerStateInitializerRHI&,Initializer,
	return,return new FRHISamplerState();
	);
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_1(
	FRasterizerStateRHIRef,CreateRasterizerState,
	const FRasterizerStateInitializerRHI&,Initializer,
	return,return new FRHIRasterizerState();
	);
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_1(
	FDepthStencilStateRHIRef,CreateDepthStencilState,
	const FDepthStencilStateInitializerRHI&,Initializer,
	return,return new FRHIDepthStencilState();
	);
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_1(
	FBlendStateRHIRef,CreateBlendState,
	const FBlendStateInitializerRHI&,Initializer,
	return,return new FRHIBlendState();
	);

DEFINE_RHIMETHOD_GLOBAL_1(
	FVertexDeclarationRHIRef,CreateVertexDeclaration,
	const FVertexDeclarationElementList&,Elements,
	return,return new FRHIVertexDeclaration();
	);

DEFINE_RHIMETHOD_GLOBAL_1(
	FPixelShaderRHIRef,CreatePixelShader,
	const TArray<uint8>&,Code,
	return,return new FRHIPixelShader();
	);

DEFINE_RHIMETHOD_GLOBAL_1(
	FVertexShaderRHIRef,CreateVertexShader,
	const TArray<uint8>&,Code,
	return,return new FRHIVertexShader();
	);

DEFINE_RHIMETHOD_GLOBAL_1(
	FHullShaderRHIRef,CreateHullShader,
	const TArray<uint8>&,Code,
	return,return new FRHIHullShader();
	);

DEFINE_RHIMETHOD_GLOBAL_1(
	FDomainShaderRHIRef,CreateDomainShader,
	const TArray<uint8>&,Code,
	return,return new FRHIDomainShader();
	);
	
DEFINE_RHIMETHOD_GLOBAL_1(
	FGeometryShaderRHIRef,CreateGeometryShader,
	const TArray<uint8>&,Code,
	return,return new FRHIGeometryShader();
	);

/** Creates a geometry shader with stream output ability, defined by ElementList. */
DEFINE_RHIMETHOD_GLOBAL_5(
	FGeometryShaderRHIRef,CreateGeometryShaderWithStreamOutput,
	const TArray<uint8>&,Code,
	const FStreamOutElementList&,ElementList,
	uint32,NumStrides,
	const uint32*,Strides,
	int32,RasterizedStream,
	return,return new FRHIGeometryShader();
	);

DEFINE_RHIMETHOD_GLOBAL_1(
	FComputeShaderRHIRef,CreateComputeShader,
	const TArray<uint8>&,Code,
	return,return new FRHIComputeShader();
	);

/**
 * Creates a bound shader state instance which encapsulates a decl, vertex shader, hull shader, domain shader and pixel shader
 * CAUTION: Even though this is marked as threadsafe, it is only valid to call from the render thread or the RHI thread. It need not be threadsafe unless the RHI support parallel translation.
 * CAUTION: Platforms that support RHIThread but don't actually have a threadsafe implementation must flush internally with FRHICommandListExecutor::GetImmediateCommandList().ImmediateFlush(EImmediateFlushType::WaitForRHIThread); when the call is from the render thread
 * @param VertexDeclaration - existing vertex decl
 * @param VertexShader - existing vertex shader
 * @param HullShader - existing hull shader
 * @param DomainShader - existing domain shader
 * @param GeometryShader - existing geometry shader
 * @param PixelShader - existing pixel shader
 */

DEFINE_RHIMETHOD_GLOBALTHREADSAFE_6(
	FBoundShaderStateRHIRef,CreateBoundShaderState,
	FVertexDeclarationRHIParamRef,VertexDeclaration,
	FVertexShaderRHIParamRef,VertexShader,
	FHullShaderRHIParamRef,HullShader,
	FDomainShaderRHIParamRef,DomainShader,
	FPixelShaderRHIParamRef,PixelShader,
	FGeometryShaderRHIParamRef,GeometryShader,
	return,return new FRHIBoundShaderState();
);

/**
 *Sets the current compute shader.  Mostly for compliance with platforms
 *that require shader setting before resource binding.
 */
DEFINE_RHIMETHOD_CMDLIST_1(
	void,SetComputeShader,
	FComputeShaderRHIParamRef,ComputeShader,
	,
	);

DEFINE_RHIMETHOD_CMDLIST_3(
	void,DispatchComputeShader,	
	uint32,ThreadGroupCountX,
	uint32,ThreadGroupCountY,
	uint32,ThreadGroupCountZ,
	,
	);

DEFINE_RHIMETHOD_CMDLIST_2(
	void,DispatchIndirectComputeShader,	
	FVertexBufferRHIParamRef,ArgumentBuffer,
	uint32,ArgumentOffset,
	,
	);

DEFINE_RHIMETHOD_CMDLIST_1(
	void,AutomaticCacheFlushAfterComputeShader,
	bool,bEnable,
	,
	);

DEFINE_RHIMETHOD_CMDLIST_0(
	void,FlushComputeShaderCache,
	,
	);

// Useful when used with geometry shader (emit polygons to different viewports), otherwise SetViewPort() is simpler
// @param Count >0
// @param Data must not be 0
DEFINE_RHIMETHOD_CMDLIST_2(
	void,SetMultipleViewports,
	uint32,Count,
	const FViewportBounds*,Data,
	,
	);

/**
 * Creates a uniform buffer.  The contents of the uniform buffer are provided in a parameter, and are immutable.
 * CAUTION: Even though this is marked as threadsafe, it is only valid to call from the render thread or the RHI thread. Thus is need not be threadsafe on platforms that do not support or aren't using an RHIThread
 * @param Contents - A pointer to a memory block of size NumBytes that is copied into the new uniform buffer.
 * @param NumBytes - The number of bytes the uniform buffer should contain.
 * @return The new uniform buffer.
 */
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_3(
	FUniformBufferRHIRef,CreateUniformBuffer,
	const void*,Contents,
	const FRHIUniformBufferLayout&,Layout,
	EUniformBufferUsage,Usage,
	return,return new FRHIUniformBuffer(Layout);
);

DEFINE_RHIMETHOD_GLOBAL_4(
	FIndexBufferRHIRef,CreateIndexBuffer,
	uint32,Stride,
	uint32,Size,
	uint32,InUsage,
	FRHIResourceCreateInfo&,CreateInfo,
	return,if (CreateInfo.ResourceArray) { CreateInfo.ResourceArray->Discard(); } return new FRHIIndexBuffer(Stride,Size,InUsage);
	);

DEFINE_RHIMETHOD_GLOBALFLUSH_4(
	void*,LockIndexBuffer,
	FIndexBufferRHIParamRef,IndexBuffer,
	uint32,Offset,
	uint32,Size,
	EResourceLockMode,LockMode,
	return,return GetStaticBuffer();
	);
DEFINE_RHIMETHOD_GLOBALFLUSH_1(
	void,UnlockIndexBuffer,
	FIndexBufferRHIParamRef,IndexBuffer,
	,
	);

/**
 * @param ResourceArray - An optional pointer to a resource array containing the resource's data.
 */
DEFINE_RHIMETHOD_GLOBAL_3(
	FVertexBufferRHIRef,CreateVertexBuffer,
	uint32,Size,
	uint32,InUsage,
	FRHIResourceCreateInfo&,CreateInfo,
	return,if (CreateInfo.ResourceArray) { CreateInfo.ResourceArray->Discard(); } return new FRHIVertexBuffer(Size,InUsage);
	);

DEFINE_RHIMETHOD_GLOBALFLUSH_4(
	void*,LockVertexBuffer,
	FVertexBufferRHIParamRef,VertexBuffer,
	uint32,Offset,
	uint32,SizeRHI,
	EResourceLockMode,LockMode,
	return,return GetStaticBuffer();
	);
DEFINE_RHIMETHOD_GLOBALFLUSH_1(
	void,UnlockVertexBuffer,
	FVertexBufferRHIParamRef,VertexBuffer,
	,
	);

/** Copies the contents of one vertex buffer to another vertex buffer.  They must have identical sizes. */
DEFINE_RHIMETHOD_2(
	void,CopyVertexBuffer,
	FVertexBufferRHIParamRef,SourceBuffer,
	FVertexBufferRHIParamRef,DestBuffer,
	,
	);

/**
 * @param ResourceArray - An optional pointer to a resource array containing the resource's data.
 */
DEFINE_RHIMETHOD_GLOBAL_4(
	FStructuredBufferRHIRef,CreateStructuredBuffer,
	uint32,Stride,
	uint32,Size,
	uint32,InUsage,
	FRHIResourceCreateInfo&,CreateInfo,
	return,if (CreateInfo.ResourceArray) { CreateInfo.ResourceArray->Discard(); } return new FRHIStructuredBuffer(Stride,Size,InUsage);
	);

DEFINE_RHIMETHOD_GLOBALFLUSH_4(
	void*,LockStructuredBuffer,
	FStructuredBufferRHIParamRef,StructuredBuffer,
	uint32,Offset,
	uint32,SizeRHI,
	EResourceLockMode,LockMode,
	return,return GetStaticBuffer();
	);
DEFINE_RHIMETHOD_GLOBALFLUSH_1(
	void,UnlockStructuredBuffer,
	FStructuredBufferRHIParamRef,StructuredBuffer,
	,
	);

/** Creates an unordered access view of the given structured buffer. */
DEFINE_RHIMETHOD_GLOBAL_3(
	FUnorderedAccessViewRHIRef,CreateUnorderedAccessView,
	FStructuredBufferRHIParamRef,StructuredBuffer,
	bool,bUseUAVCounter,
	bool,bAppendBuffer,
	return,return new FRHIUnorderedAccessView();
	);

/** Creates an unordered access view of the given texture. */
	DEFINE_RHIMETHOD_GLOBAL_1(
	FUnorderedAccessViewRHIRef,CreateUnorderedAccessView,
	FTextureRHIParamRef,Texture,
	return,return new FRHIUnorderedAccessView();
	);
	
/** Creates an unordered access view of the given texture. */
	DEFINE_RHIMETHOD_GLOBAL_2(
	FUnorderedAccessViewRHIRef,CreateUnorderedAccessView,
	FVertexBufferRHIParamRef,VertexBuffer,
	uint8,Format,
	return,return new FRHIUnorderedAccessView();
	);

/** Creates a shader resource view of the given structured buffer. */
	DEFINE_RHIMETHOD_GLOBAL_1(
	FShaderResourceViewRHIRef,CreateShaderResourceView,
	FStructuredBufferRHIParamRef,StructuredBuffer,
	return,return new FRHIShaderResourceView();
	);

/** Creates a shader resource view of the given vertex buffer. */
	DEFINE_RHIMETHOD_GLOBAL_3(
	FShaderResourceViewRHIRef,CreateShaderResourceView,
	FVertexBufferRHIParamRef,VertexBuffer,
	uint32,Stride,
	uint8,Format,
	return,return new FRHIShaderResourceView();
	);

/** Clears a UAV to the multi-component value provided. */
	DEFINE_RHIMETHOD_CMDLIST_2(
	void,ClearUAV,
	FUnorderedAccessViewRHIParamRef,UnorderedAccessViewRHI,
	const uint32*,Values,
	,
	);

/**
 * Computes the total size of a 2D texture with the specified parameters.
 *
 * @param SizeX - width of the texture to compute
 * @param SizeY - height of the texture to compute
 * @param Format - EPixelFormat texture format
 * @param NumMips - number of mips to compute or 0 for full mip pyramid
 * @param NumSamples - number of MSAA samples, usually 1
 * @param Flags - ETextureCreateFlags creation flags
 * @param OutAlign - Alignment required for this texture.  Output parameter.
 */
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_7(
	uint64,CalcTexture2DPlatformSize,
	uint32, SizeX,
	uint32, SizeY,	
	uint8, Format,
	uint32, NumMips,
	uint32, NumSamples,
	uint32, Flags,
	uint32&, OutAlign,
	return, OutAlign = 0; return 0);

/**
* Computes the total size of a 3D texture with the specified parameters.
* @param SizeX - width of the texture to create
* @param SizeY - height of the texture to create
* @param SizeZ - depth of the texture to create
* @param Format - EPixelFormat texture format
* @param NumMips - number of mips to generate or 0 for full mip pyramid
* @param Flags - ETextureCreateFlags creation flags
* @param OutAlign - Alignment required for this texture.  Output parameter.
*/
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_7(
	uint64,CalcTexture3DPlatformSize,
	uint32,SizeX,
	uint32,SizeY,
	uint32,SizeZ,
	uint8,Format,
	uint32,NumMips,
	uint32,Flags,	
	uint32&, OutAlign,
	return, OutAlign = 0; return 0);

/**
* Computes the total size of a cube texture with the specified parameters.
* @param Size - width/height of the texture to create
* @param Format - EPixelFormat texture format
* @param NumMips - number of mips to generate or 0 for full mip pyramid
* @param Flags - ETextureCreateFlags creation flags
* @param OutAlign - Alignment required for this texture.  Output parameter.
*/
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_5(
	uint64,CalcTextureCubePlatformSize,
	uint32,Size,
	uint8,Format,
	uint32,NumMips,
	uint32,Flags,	
	uint32&, OutAlign,
	return, OutAlign = 0; return 0);

/**
 * Retrieves texture memory stats.
 * safe to call on the main thread
 */
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_1(
	void,GetTextureMemoryStats,
	FTextureMemoryStats&,OutStats,
	,
	);

/**
 * Fills a texture with to visualize the texture pool memory.
 *
 * @param	TextureData		Start address
 * @param	SizeX			Number of pixels along X
 * @param	SizeY			Number of pixels along Y
 * @param	Pitch			Number of bytes between each row
 * @param	PixelSize		Number of bytes each pixel represents
 *
 * @return true if successful, false otherwise
 */
DEFINE_RHIMETHOD_5(
	bool,GetTextureMemoryVisualizeData,
	FColor*,TextureData,
	int32,SizeX,
	int32,SizeY,
	int32,Pitch,
	int32,PixelSize,
	return,return false
	);

DEFINE_RHIMETHOD_GLOBAL_1(
	FTextureReferenceRHIRef,CreateTextureReference,
	FLastRenderTimeContainer*,LastRenderTime,
	return, return new FRHITextureReferenceNullImpl();
	);

DEFINE_RHIMETHOD_GLOBALFLUSH_2(
	void,UpdateTextureReference,
	FTextureReferenceRHIParamRef,TextureRef,
	FTextureRHIParamRef,NewTexture,
	,if (TextureRef) { ((FRHITextureReferenceNullImpl*)TextureRef)->SetReferencedTexture(NewTexture); }
	);

/**
* Creates a 2D RHI texture resource
* @param SizeX - width of the texture to create
* @param SizeY - height of the texture to create
* @param Format - EPixelFormat texture format
* @param NumMips - number of mips to generate or 0 for full mip pyramid
* @param NumSamples - number of MSAA samples, usually 1
* @param Flags - ETextureCreateFlags creation flags
*/
DEFINE_RHIMETHOD_GLOBAL_7(
	FTexture2DRHIRef,CreateTexture2D,
	uint32,SizeX,
	uint32,SizeY,
	uint8,Format,
	uint32,NumMips,
	uint32,NumSamples,
	uint32,Flags,
	FRHIResourceCreateInfo&,CreateInfo,
	return,return new FRHITexture2D(SizeX,SizeY,NumMips,NumSamples,(EPixelFormat)Format,Flags);
	);

/**
 * Thread-safe function that can be used to create a texture outside of the
 * rendering thread. This function can ONLY be called if GRHISupportsAsyncTextureCreation
 * is true.
 * @param SizeX - width of the texture to create
 * @param SizeY - height of the texture to create
 * @param Format - EPixelFormat texture format
 * @param NumMips - number of mips to generate or 0 for full mip pyramid
 * @param Flags - ETextureCreateFlags creation flags
 * @param InitialMipData - pointers to mip data with which to create the texture
 * @param NumInitialMips - how many mips are provided in InitialMipData
 * @returns a reference to a 2D texture resource
 */
DEFINE_RHIMETHOD_GLOBAL_7(
	FTexture2DRHIRef,AsyncCreateTexture2D,
	uint32,SizeX,
	uint32,SizeY,
	uint8,Format,
	uint32,NumMips,
	uint32,Flags,
	void**,InitialMipData,
	uint32,NumInitialMips,
	return,return FTexture2DRHIRef();
	);

/**
 * Copies shared mip levels from one texture to another. The textures must have
 * full mip chains, share the same format, and have the same aspect ratio. This
 * copy will not cause synchronization with the GPU.
 * @param DestTexture2D - destination texture
 * @param SrcTexture2D - source texture
 */
DEFINE_RHIMETHOD_GLOBALFLUSH_2(
	void,CopySharedMips,
	FTexture2DRHIParamRef,DestTexture2D,
	FTexture2DRHIParamRef,SrcTexture2D,
	return,return;
	);

/**
* Creates a Array RHI texture resource
* @param SizeX - width of the texture to create
* @param SizeY - height of the texture to create
* @param SizeZ - depth of the texture to create
* @param Format - EPixelFormat texture format
* @param NumMips - number of mips to generate or 0 for full mip pyramid
* @param Flags - ETextureCreateFlags creation flags
*/
DEFINE_RHIMETHOD_GLOBAL_7(
	FTexture2DArrayRHIRef,CreateTexture2DArray,
	uint32,SizeX,
	uint32,SizeY,
	uint32,SizeZ,
	uint8,Format,
	uint32,NumMips,
	uint32,Flags,
	FRHIResourceCreateInfo&,CreateInfo,
	return,return new FRHITexture2DArray(SizeX,SizeY,SizeZ,NumMips,(EPixelFormat)Format,Flags);
	);

/**
* Creates a 3d RHI texture resource
* @param SizeX - width of the texture to create
* @param SizeY - height of the texture to create
* @param SizeZ - depth of the texture to create
* @param Format - EPixelFormat texture format
* @param NumMips - number of mips to generate or 0 for full mip pyramid
* @param Flags - ETextureCreateFlags creation flags
*/
DEFINE_RHIMETHOD_GLOBAL_7(
	FTexture3DRHIRef,CreateTexture3D,
	uint32,SizeX,
	uint32,SizeY,
	uint32,SizeZ,
	uint8,Format,
	uint32,NumMips,
	uint32,Flags,
	FRHIResourceCreateInfo&,CreateInfo,
	return,return new FRHITexture3D(SizeX,SizeY,SizeZ,NumMips,(EPixelFormat)Format,Flags);
	);

/**
 * @param Ref may be 0
 */
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_2(
	void,GetResourceInfo,
	FTextureRHIParamRef,Ref,
	FRHIResourceInfo&,OutInfo,
	return,return;
	);

/**
* Creates a shader resource view for a 2d texture, viewing only a single
* mip level. Useful when rendering to one mip while sampling from another.
*/
DEFINE_RHIMETHOD_GLOBAL_2(
	FShaderResourceViewRHIRef,CreateShaderResourceView,
	FTexture2DRHIParamRef,Texture2DRHI,
	uint8, MipLevel,
	return,return new FRHIShaderResourceView();
	);

/**FRHIResourceInfo
* Creates a shader resource view for a 2d texture, with a different
* format from the original.  Useful when sampling stencil.
*/
DEFINE_RHIMETHOD_GLOBAL_4(
	FShaderResourceViewRHIRef,CreateShaderResourceView,
	FTexture2DRHIParamRef,Texture2DRHI,
	uint8, MipLevel,
	uint8, NumMipLevels,
	uint8, Format,		
	return,return new FRHIShaderResourceView();
);

/**
* Generates mip maps for a texture.
*/
DEFINE_RHIMETHOD_1(
	void,GenerateMips,
	FTextureRHIParamRef,Texture,
	return,return;
	);

/**
 * Computes the size in memory required by a given texture.
 *
 * @param	TextureRHI		- Texture we want to know the size of, 0 is safely ignored
 * @return					- Size in Bytes
 */
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_1(
	uint32,ComputeMemorySize,
	FTextureRHIParamRef,TextureRHI,
	return,return 0;
	);

/**
 * Starts an asynchronous texture reallocation. It may complete immediately if the reallocation
 * could be performed without any reshuffling of texture memory, or if there isn't enough memory.
 * The specified status counter will be decremented by 1 when the reallocation is complete (success or failure).
 *
 * Returns a new reference to the texture, which will represent the new mip count when the reallocation is complete.
 * RHIFinalizeAsyncReallocateTexture2D() must be called to complete the reallocation.
 *
 * @param Texture2D		- Texture to reallocate
 * @param NewMipCount	- New number of mip-levels
 * @param NewSizeX		- New width, in pixels
 * @param NewSizeY		- New height, in pixels
 * @param RequestStatus	- Will be decremented by 1 when the reallocation is complete (success or failure).
 * @return				- New reference to the texture, or an invalid reference upon failure
 */
DEFINE_RHIMETHOD_GLOBALFLUSH_5(
	FTexture2DRHIRef,AsyncReallocateTexture2D,
	FTexture2DRHIParamRef,Texture2D,
	int32,NewMipCount,
	int32,NewSizeX,
	int32,NewSizeY,
	FThreadSafeCounter*,RequestStatus,
	return,return new FRHITexture2D(NewSizeX,NewSizeY,NewMipCount,1,Texture2D->GetFormat(),Texture2D->GetFlags());
	);

/**
 * Finalizes an async reallocation request.
 * If bBlockUntilCompleted is false, it will only poll the status and finalize if the reallocation has completed.
 *
 * @param Texture2D				- Texture to finalize the reallocation for
 * @param bBlockUntilCompleted	- Whether the function should block until the reallocation has completed
 * @return						- Current reallocation status:
 *	TexRealloc_Succeeded	Reallocation succeeded
 *	TexRealloc_Failed		Reallocation failed
 *	TexRealloc_InProgress	Reallocation is still in progress, try again later
 */
DEFINE_RHIMETHOD_GLOBAL_2(
	ETextureReallocationStatus,FinalizeAsyncReallocateTexture2D,
	FTexture2DRHIParamRef,Texture2D,
	bool,bBlockUntilCompleted,
	return,return TexRealloc_Succeeded;
	);

/**
 * Cancels an async reallocation for the specified texture.
 * This should be called for the new texture, not the original.
 *
 * @param Texture				Texture to cancel
 * @param bBlockUntilCompleted	If true, blocks until the cancellation is fully completed
 * @return						Reallocation status
 */
DEFINE_RHIMETHOD_GLOBAL_2(
	ETextureReallocationStatus,CancelAsyncReallocateTexture2D,
	FTexture2DRHIParamRef,Texture2D,
	bool,bBlockUntilCompleted,
	return,return TexRealloc_Succeeded;
	);

/**
* Locks an RHI texture's mip-map for read/write operations on the CPU
* @param Texture - the RHI texture resource to lock, must not be 0
* @param MipIndex - index of the mip level to lock
* @param LockMode - Whether to lock the texture read-only instead of write-only
* @param DestStride - output to retrieve the textures row stride (pitch)
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
* @return pointer to the CPU accessible resource data
*/
DEFINE_RHIMETHOD_GLOBALFLUSH_5(
	void*,LockTexture2D,
	FTexture2DRHIParamRef,Texture,
	uint32,MipIndex,
	EResourceLockMode,LockMode,
	uint32&,DestStride,
	bool,bLockWithinMiptail,
	return,DestStride = 0; return GetStaticBuffer()
	);

/**
* Unlocks a previously locked RHI texture resource
* @param Texture - the RHI texture resource to unlock, must not be 0
* @param MipIndex - index of the mip level to unlock
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
*/
DEFINE_RHIMETHOD_GLOBALFLUSH_3(
	void,UnlockTexture2D,
	FTexture2DRHIParamRef,Texture,
	uint32,MipIndex,
	bool,bLockWithinMiptail,
	,
	);

/**
* Locks an RHI texture's mip-map for read/write operations on the CPU
* @param Texture - the RHI texture resource to lock
* @param MipIndex - index of the mip level to lock
* @param LockMode - Whether to lock the texture read-only instead of write-only
* @param DestStride - output to retrieve the textures row stride (pitch)
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
* @return pointer to the CPU accessible resource data
*/
DEFINE_RHIMETHOD_GLOBALFLUSH_6(
	void*,LockTexture2DArray,
	FTexture2DArrayRHIParamRef,Texture,
	uint32,TextureIndex,
	uint32,MipIndex,
	EResourceLockMode,LockMode,
	uint32&,DestStride,
	bool,bLockWithinMiptail,
	return,DestStride = 0; return GetStaticBuffer()
	);

/**
* Unlocks a previously locked RHI texture resource
* @param Texture - the RHI texture resource to unlock
* @param MipIndex - index of the mip level to unlock
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
*/
DEFINE_RHIMETHOD_GLOBALFLUSH_4(
	void,UnlockTexture2DArray,
	FTexture2DArrayRHIParamRef,Texture,
	uint32,TextureIndex,
	uint32,MipIndex,
	bool,bLockWithinMiptail,
	,
	);

/**
* Updates a region of a 2D texture from system memory
* @param Texture - the RHI texture resource to update
* @param MipIndex - mip level index to be modified
* @param UpdateRegion - The rectangle to copy source image data from
* @param SourcePitch - size in bytes of each row of the source image
* @param SourceData - source image data, starting at the upper left corner of the source rectangle (in same pixel format as texture)
*/
DEFINE_RHIMETHOD_GLOBALFLUSH_5(
	void,UpdateTexture2D,
	FTexture2DRHIParamRef,Texture,
	uint32,MipIndex,
	const struct FUpdateTextureRegion2D&,UpdateRegion,
	uint32,SourcePitch,
	const uint8*,SourceData,
	,
	);

/**
* Updates a region of a 3D texture from system memory
* @param Texture - the RHI texture resource to update
* @param MipIndex - mip level index to be modified
* @param UpdateRegion - The rectangle to copy source image data from
* @param SourceRowPitch - size in bytes of each row of the source image, usually Bpp * SizeX
* @param SourceDepthPitch - size in bytes of each depth slice of the source image, usually Bpp * SizeX * SizeY
* @param SourceData - source image data, starting at the upper left corner of the source rectangle (in same pixel format as texture)
*/
DEFINE_RHIMETHOD_GLOBALFLUSH_6(
	void,UpdateTexture3D,
	FTexture3DRHIParamRef,Texture,
	uint32,MipIndex,
	const struct FUpdateTextureRegion3D&,UpdateRegion,
	uint32,SourceRowPitch,
	uint32,SourceDepthPitch,
	const uint8*,SourceData,
	,
	);

/**
* Creates a Cube RHI texture resource
* @param Size - width/height of the texture to create
* @param Format - EPixelFormat texture format
* @param NumMips - number of mips to generate or 0 for full mip pyramid
* @param Flags - ETextureCreateFlags creation flags
*/
DEFINE_RHIMETHOD_GLOBAL_5(
	FTextureCubeRHIRef,CreateTextureCube,
	uint32,Size,
	uint8,Format,
	uint32,NumMips,
	uint32,Flags,
	FRHIResourceCreateInfo&,CreateInfo,
	return,return new FRHITextureCube(Size,NumMips,(EPixelFormat)Format,Flags);
	);


	/**
	* Creates a Cube Array RHI texture resource
	* @param Size - width/height of the texture to create
	* @param ArraySize - number of array elements of the texture to create
	* @param Format - EPixelFormat texture format
	* @param NumMips - number of mips to generate or 0 for full mip pyramid
	* @param Flags - ETextureCreateFlags creation flags
	*/
DEFINE_RHIMETHOD_GLOBAL_6(
	FTextureCubeRHIRef,CreateTextureCubeArray,
		uint32,Size,
		uint32,ArraySize,
		uint8,Format,
		uint32,NumMips,
		uint32,Flags,
	FRHIResourceCreateInfo&,CreateInfo,
		return,return new FRHITextureCube(Size,NumMips,(EPixelFormat)Format,Flags);
	);


/**
* Locks an RHI texture's mip-map for read/write operations on the CPU
* @param Texture - the RHI texture resource to lock
* @param MipIndex - index of the mip level to lock
* @param LockMode - Whether to lock the texture read-only instead of write-only.
* @param DestStride - output to retrieve the textures row stride (pitch)
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
* @return pointer to the CPU accessible resource data
*/
DEFINE_RHIMETHOD_GLOBALFLUSH_7(
	void*,LockTextureCubeFace,
	FTextureCubeRHIParamRef,Texture,
	uint32,FaceIndex,
	uint32,ArrayIndex,
	uint32,MipIndex,
	EResourceLockMode,LockMode,
	uint32&,DestStride,
	bool,bLockWithinMiptail,
	return,DestStride = 0; return GetStaticBuffer();
	);

/**
* Unlocks a previously locked RHI texture resource
* @param Texture - the RHI texture resource to unlock
* @param MipIndex - index of the mip level to unlock
* @param bLockWithinMiptail - for platforms that support packed miptails allow locking of individual mip levels within the miptail
*/
DEFINE_RHIMETHOD_GLOBALFLUSH_5(
	void,UnlockTextureCubeFace,
	FTextureCubeRHIParamRef,Texture,
	uint32,FaceIndex,
	uint32,ArrayIndex,
	uint32,MipIndex,
	bool,bLockWithinMiptail,
	,
	);

/**
* Resolves from one texture to another.
* @param SourceTexture - texture to resolve from, 0 is silenty ignored
* @param DestTexture - texture to resolve to, 0 is silenty ignored
* @param bKeepOriginalSurface - true if the original surface will still be used after this function so must remain valid
* @param ResolveParams - optional resolve params
*/
DEFINE_RHIMETHOD_CMDLIST_4(
	void,CopyToResolveTarget,
	FTextureRHIParamRef,SourceTexture,
	FTextureRHIParamRef,DestTexture,
	bool,bKeepOriginalSurface,
	const FResolveParams&,ResolveParams,
	,
	);

DEFINE_RHIMETHOD_GLOBALTHREADSAFE_2(
	void,BindDebugLabelName,
	FTextureRHIParamRef, Texture,
	const TCHAR*, Name,
	,
	);

/**
 * Reads the contents of a texture to an output buffer (non MSAA and MSAA) and returns it as a FColor array.
 * If the format or texture type is unsupported the OutData array will be size 0
 */
DEFINE_RHIMETHOD_4(
	void,ReadSurfaceData,
	FTextureRHIParamRef,Texture,
	FIntRect,Rect,
	TArray<FColor>&,OutData,
	FReadSurfaceDataFlags,InFlags,
	, OutData.AddZeroed(Rect.Width() * Rect.Height());
	);

/** Watch out for OutData to be 0 (can happen on DXGI_ERROR_DEVICE_REMOVED), don't call RHIUnmapStagingSurface in that case. */
DEFINE_RHIMETHOD_4(
	void,MapStagingSurface,
	FTextureRHIParamRef,Texture,
	void*&,OutData,
	int32&,OutWidth,
	int32&,OutHeight,
	,
	);

/** call after a succesful RHIMapStagingSurface() call */
DEFINE_RHIMETHOD_1(
	void,UnmapStagingSurface,
	FTextureRHIParamRef,Texture,
	,
	);

DEFINE_RHIMETHOD_6(
	void,ReadSurfaceFloatData,
	FTextureRHIParamRef,Texture,
	FIntRect,Rect,
	TArray<FFloat16Color>&,OutData,
	ECubeFace,CubeFace,
	int32,ArrayIndex,
	int32,MipIndex,
	,
	);

DEFINE_RHIMETHOD_4(
	void,Read3DSurfaceFloatData,
	FTextureRHIParamRef,Texture,
	FIntRect,Rect,
	FIntPoint,ZMinMax,
	TArray<FFloat16Color>&,OutData,
	,
	);


// Occlusion/Timer queries.
DEFINE_RHIMETHOD_GLOBAL_1(
	FRenderQueryRHIRef,CreateRenderQuery,
	ERenderQueryType, QueryType,
	return,return new FRHIRenderQuery();
	);

DEFINE_RHIMETHOD_CMDLIST_1(
	void,BeginRenderQuery,
	FRenderQueryRHIParamRef,RenderQuery,
	,
	);
DEFINE_RHIMETHOD_CMDLIST_1(
	void,EndRenderQuery,
	FRenderQueryRHIParamRef,RenderQuery,
	,
	);

// CAUTION: Even though this is marked as threadsafe, it is only valid to call from the render thread. It is need not be threadsafe on platforms that do not support or aren't using an RHIThread
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_3(
	bool,GetRenderQueryResult,
	FRenderQueryRHIParamRef,RenderQuery,
	uint64&,OutResult,
	bool,bWait,
	return,return true;
);

DEFINE_RHIMETHOD_CMDLIST_0(
	void,BeginOcclusionQueryBatch,
	,
	);
DEFINE_RHIMETHOD_CMDLIST_0(
	void,EndOcclusionQueryBatch,
	,
	);

DEFINE_RHIMETHOD_CMDLIST_0(
	void,SubmitCommandsHint,
	,
	);

// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
DEFINE_RHIMETHOD_CMDLIST_2(
	void,BeginDrawingViewport,
	FViewportRHIParamRef,Viewport,
	FTextureRHIParamRef,RenderTargetRHI,
	,
	);
// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
DEFINE_RHIMETHOD_CMDLIST_3(
	void,EndDrawingViewport,
	FViewportRHIParamRef,Viewport,
	bool,bPresent,
	bool,bLockToVsync,
	,
	);
// With RHI thread, this is the current backbuffer from the perspecitve of the render thread.
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_1(
	FTexture2DRHIRef,GetViewportBackBuffer,
	FViewportRHIParamRef,Viewport,
	return,return new FRHITexture2D(1,1,1,1,PF_B8G8R8A8,TexCreate_RenderTargetable);
);
// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
DEFINE_RHIMETHOD_CMDLIST_0(
	void,BeginFrame,
	return,return;
);

// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
DEFINE_RHIMETHOD_CMDLIST_0(
	void,EndFrame,
	return,return;
);
/**
	* Signals the beginning of scene rendering. The RHI makes certain caching assumptions between
	* calls to BeginScene/EndScene. Currently the only restriction is that you can't update texture
	* references.
	*/
// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
DEFINE_RHIMETHOD_CMDLIST_0(
	void,BeginScene,
	return,return;
	);

/**
	* Signals the end of scene rendering. See RHIBeginScene.
	*/
// This method is queued with an RHIThread, otherwise it will flush after it is queued; without an RHI thread there is no benefit to queuing this frame advance commands
DEFINE_RHIMETHOD_CMDLIST_0(
	void,EndScene,
	return,return;
	);

// Only relevant with an RHI thread, this advances the backbuffer for the purpose of GetViewportBackBuffer
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_0(
	void,AdvanceFrameForGetViewportBackBuffer,
	return,return;
	);



/**
 * Determine if currently drawing the viewport
 * REMOVE THIS If you are updating the RHI for all platforms
 *
 * @return true if currently within a BeginDrawingViewport/EndDrawingViewport block
 */
DEFINE_RHIMETHOD_0(
	bool,IsDrawingViewport,
	return,return false;
	);
/*
 * Acquires or releases ownership of the platform-specific rendering context for the calling thread
 */
DEFINE_RHIMETHOD_GLOBALFLUSH_0(
	void,AcquireThreadOwnership,
	return,return;
	);
DEFINE_RHIMETHOD_GLOBALFLUSH_0(
	void,ReleaseThreadOwnership,
	return,return;
	);

// Flush driver resources. Typically called when switching contexts/threads
DEFINE_RHIMETHOD_GLOBALFLUSH_0(
	void,FlushResources,
	return,return;
	);
/*
 * Returns the total GPU time taken to render the last frame. Same metric as FPlatformTime::Cycles().
 */
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_0(
	uint32,GetGPUFrameCycles,
	return,return 0;
	);

/**
 * The following RHI functions must be called from the main thread.
 */
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_5(
	FViewportRHIRef,CreateViewport,
	void*,WindowHandle,
	uint32,SizeX,
	uint32,SizeY,
	bool,bIsFullscreen,
	EPixelFormat,PreferredPixelFormat,
	return,return new FRHIViewport();
	);
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_4(
	void,ResizeViewport,
	FViewportRHIParamRef,Viewport,
	uint32,SizeX,
	uint32,SizeY,
	bool,bIsFullscreen,
	,
	);
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_1(
	void,Tick,
	float,DeltaTime,
	,
	);

//
// RHI commands.
//

// Vertex state.
DEFINE_RHIMETHOD_CMDLIST_4(
	void,SetStreamSource,
	uint32,StreamIndex,
	FVertexBufferRHIParamRef,VertexBuffer,
	uint32,Stride,
	uint32,Offset,
	,
	);

/** Sets stream output targets, for use with a geometry shader created with RHICreateGeometryShaderWithStreamOutput. */
//@todo this should be a CMDLIST method
DEFINE_RHIMETHOD_3(
	void,SetStreamOutTargets,
	uint32,NumTargets,
	const FVertexBufferRHIParamRef*,VertexBuffers,
	const uint32*,Offsets,
	,
	);

// Rasterizer state.
DEFINE_RHIMETHOD_CMDLIST_1(
	void,SetRasterizerState,
	FRasterizerStateRHIParamRef,NewState,
	,
	);
// @param MinX including like Win32 RECT
// @param MinY including like Win32 RECT
// @param MaxX excluding like Win32 RECT
// @param MaxY excluding like Win32 RECT
DEFINE_RHIMETHOD_CMDLIST_6(
	void,SetViewport,
	uint32,MinX,
	uint32,MinY,
	float,MinZ,
	uint32,MaxX,
	uint32,MaxY,
	float,MaxZ,
	,
	);
// @param MinX including like Win32 RECT
// @param MinY including like Win32 RECT
// @param MaxX excluding like Win32 RECT
// @param MaxY excluding like Win32 RECT
DEFINE_RHIMETHOD_CMDLIST_5(
	void,SetScissorRect,
	bool,bEnable,
	uint32,MinX,
	uint32,MinY,
	uint32,MaxX,
	uint32,MaxY,
	,
	);

// Shader state.
/**
 * Set bound shader state. This will set the vertex decl/shader, and pixel shader
 * @param BoundShaderState - state resource
 */
DEFINE_RHIMETHOD_CMDLIST_1(
	void,SetBoundShaderState,
	FBoundShaderStateRHIParamRef,BoundShaderState,
	,
	);

/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
DEFINE_RHIMETHOD_CMDLIST_3(
	void,SetShaderTexture,
	FVertexShaderRHIParamRef,VertexShader,
	uint32,TextureIndex,
	FTextureRHIParamRef,NewTexture,
	,
	);

/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
DEFINE_RHIMETHOD_CMDLIST_3(
	void,SetShaderTexture,
	FHullShaderRHIParamRef,HullShader,
	uint32,TextureIndex,
	FTextureRHIParamRef,NewTexture,
	,
	);

/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
DEFINE_RHIMETHOD_CMDLIST_3(
	void,SetShaderTexture,
	FDomainShaderRHIParamRef,DomainShader,
	uint32,TextureIndex,
	FTextureRHIParamRef,NewTexture,
	,
	);

/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
DEFINE_RHIMETHOD_CMDLIST_3(
	void,SetShaderTexture,
	FGeometryShaderRHIParamRef,GeometryShader,
	uint32,TextureIndex,
	FTextureRHIParamRef,NewTexture,
	,
	);

/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
DEFINE_RHIMETHOD_CMDLIST_3(
	void,SetShaderTexture,
	FPixelShaderRHIParamRef,PixelShader,
	uint32,TextureIndex,
	FTextureRHIParamRef,NewTexture,
	,
	);

/** Set the shader resource view of a surface.  This is used for binding TextureMS parameter types that need a multi sampled view. */
DEFINE_RHIMETHOD_CMDLIST_3(
	void,SetShaderTexture,
	FComputeShaderRHIParamRef,PixelShader,
	uint32,TextureIndex,
	FTextureRHIParamRef,NewTexture,
	,
	);

/**
 * Sets sampler state.
 * @param ComputeShader		The compute shader to set the sampler for.
 * @param SamplerIndex		The index of the sampler.
 * @param NewState			The new sampler state.
 */
DEFINE_RHIMETHOD_CMDLIST_3(
	void,SetShaderSampler,
	FComputeShaderRHIParamRef,ComputeShader,
	uint32,SamplerIndex,
	FSamplerStateRHIParamRef,NewState,
	,
	);

/**
 * Sets sampler state.
 * @param VertexShader		The vertex shader to set the sampler for.
 * @param SamplerIndex		The index of the sampler.
 * @param NewState			The new sampler state.
 */
DEFINE_RHIMETHOD_CMDLIST_3(
	void,SetShaderSampler,
	FVertexShaderRHIParamRef,VertexShader,
	uint32,SamplerIndex,
	FSamplerStateRHIParamRef,NewState,
	,
	);

/**
 * Sets sampler state.
 * @param GeometryShader	The geometry shader to set the sampler for.
 * @param SamplerIndex		The index of the sampler.
 * @param NewState			The new sampler state.
 */
DEFINE_RHIMETHOD_CMDLIST_3(
	void,SetShaderSampler,
	FGeometryShaderRHIParamRef,GeometryShader,
	uint32,SamplerIndex,
	FSamplerStateRHIParamRef,NewState,
	,
	);

/**
 * Sets sampler state.
 * @param DomainShader		The domain shader to set the sampler for.
 * @param SamplerIndex		The index of the sampler.
 * @param NewState			The new sampler state.
 */
DEFINE_RHIMETHOD_CMDLIST_3(
	void,SetShaderSampler,
	FDomainShaderRHIParamRef,DomainShader,
	uint32,SamplerIndex,
	FSamplerStateRHIParamRef,NewState,
	,
	);

/**
 * Sets sampler state.
 * @param HullShader		The hull shader to set the sampler for.
 * @param SamplerIndex		The index of the sampler.
 * @param NewState			The new sampler state.
 */
DEFINE_RHIMETHOD_CMDLIST_3(
	void,SetShaderSampler,
	FHullShaderRHIParamRef,HullShader,
	uint32,SamplerIndex,
	FSamplerStateRHIParamRef,NewState,
	,
	);

/**
 * Sets sampler state.
 * @param PixelShader		The pixel shader to set the sampler for.
 * @param SamplerIndex		The index of the sampler.
 * @param NewState			The new sampler state.
 */
DEFINE_RHIMETHOD_CMDLIST_3(
	void,SetShaderSampler,
	FPixelShaderRHIParamRef,PixelShader,
	uint32,SamplerIndex,
	FSamplerStateRHIParamRef,NewState,
	,
	);

/** Sets a compute shader UAV parameter. */
DEFINE_RHIMETHOD_CMDLIST_3(
	 void,SetUAVParameter,
	 FComputeShaderRHIParamRef,ComputeShader,
	 uint32,UAVIndex,
	 FUnorderedAccessViewRHIParamRef,UAV,
	 ,
	 );

/** Sets a compute shader UAV parameter and initial count */
DEFINE_RHIMETHOD_CMDLIST_4(
	 void,SetUAVParameter,
	 FComputeShaderRHIParamRef,ComputeShader,
	 uint32,UAVIndex,
	 FUnorderedAccessViewRHIParamRef,UAV,
	 uint32,InitialCount,
	 , 
	 );

/** Sets a pixel shader resource view parameter. */
DEFINE_RHIMETHOD_CMDLIST_3(
	void,SetShaderResourceViewParameter,
	FPixelShaderRHIParamRef,PixelShader,
	uint32,SamplerIndex,
	FShaderResourceViewRHIParamRef,SRV,
	,
	);

/** Sets a vertex shader resource view parameter. */
DEFINE_RHIMETHOD_CMDLIST_3(
	void,SetShaderResourceViewParameter,
	FVertexShaderRHIParamRef,VertexShader,
	uint32,SamplerIndex,
	FShaderResourceViewRHIParamRef,SRV,
	,
	);

/** Sets a compute shader resource view parameter. */
DEFINE_RHIMETHOD_CMDLIST_3(
	void,SetShaderResourceViewParameter,
	FComputeShaderRHIParamRef,ComputeShader,
	uint32,SamplerIndex,
	FShaderResourceViewRHIParamRef,SRV,
	,
	);

/** Sets a hull shader resource view parameter. */
DEFINE_RHIMETHOD_CMDLIST_3(
	void,SetShaderResourceViewParameter,
	FHullShaderRHIParamRef,HullShader,
	uint32,SamplerIndex,
	FShaderResourceViewRHIParamRef,SRV,
	,
	);

/** Sets a domain shader resource view parameter. */
DEFINE_RHIMETHOD_CMDLIST_3(
	void,SetShaderResourceViewParameter,
	FDomainShaderRHIParamRef,DomainShader,
	uint32,SamplerIndex,
	FShaderResourceViewRHIParamRef,SRV,
	,
	);

/** Sets a geometry shader resource view parameter. */
DEFINE_RHIMETHOD_CMDLIST_3(
	void,SetShaderResourceViewParameter,
	FGeometryShaderRHIParamRef,GeometryShader,
	uint32,SamplerIndex,
	FShaderResourceViewRHIParamRef,SRV,
	,
	);

DEFINE_RHIMETHOD_CMDLIST_3(
	 void,SetShaderUniformBuffer,
	 FVertexShaderRHIParamRef,VertexShader,
	 uint32,BufferIndex,
	 FUniformBufferRHIParamRef,Buffer,
	 ,
	 );

DEFINE_RHIMETHOD_CMDLIST_3(
	 void,SetShaderUniformBuffer,
	 FHullShaderRHIParamRef,HullShader,
	 uint32,BufferIndex,
	 FUniformBufferRHIParamRef,Buffer,
	 ,
	 );

DEFINE_RHIMETHOD_CMDLIST_3(
	 void,SetShaderUniformBuffer,
	 FDomainShaderRHIParamRef,DomainShader,
	 uint32,BufferIndex,
	 FUniformBufferRHIParamRef,Buffer,
	 ,
	 );

DEFINE_RHIMETHOD_CMDLIST_3(
	 void,SetShaderUniformBuffer,
	 FGeometryShaderRHIParamRef,GeometryShader,
	 uint32,BufferIndex,
	 FUniformBufferRHIParamRef,Buffer,
	 ,
	 );

DEFINE_RHIMETHOD_CMDLIST_3(
	 void,SetShaderUniformBuffer,
	 FPixelShaderRHIParamRef,PixelShader,
	 uint32,BufferIndex,
	 FUniformBufferRHIParamRef,Buffer,
	 ,
	 );

DEFINE_RHIMETHOD_CMDLIST_3(
	void,SetShaderUniformBuffer,
	FComputeShaderRHIParamRef,ComputeShader,
	uint32,BufferIndex,
	FUniformBufferRHIParamRef,Buffer,
	,
	);

DEFINE_RHIMETHOD_CMDLIST_5(
	void,SetShaderParameter,
	FVertexShaderRHIParamRef,VertexShader,
	uint32,BufferIndex,
	uint32,BaseIndex,
	uint32,NumBytes,
	const void*,NewValue,
	,
	);
DEFINE_RHIMETHOD_CMDLIST_5(
	 void,SetShaderParameter,
	 FPixelShaderRHIParamRef,PixelShader,
	 uint32,BufferIndex,
	 uint32,BaseIndex,
	 uint32,NumBytes,
	 const void*,NewValue,
	 ,
	 );

DEFINE_RHIMETHOD_CMDLIST_5(
	 void,SetShaderParameter,
	 FHullShaderRHIParamRef,HullShader,
	 uint32,BufferIndex,
	 uint32,BaseIndex,
	 uint32,NumBytes,
	 const void*,NewValue,
	 ,
	 );
DEFINE_RHIMETHOD_CMDLIST_5(
	 void,SetShaderParameter,
	 FDomainShaderRHIParamRef,DomainShader,
	 uint32,BufferIndex,
	 uint32,BaseIndex,
	 uint32,NumBytes,
	 const void*,NewValue,
	 ,
	 );
DEFINE_RHIMETHOD_CMDLIST_5(
	 void,SetShaderParameter,
	 FGeometryShaderRHIParamRef,GeometryShader,
	 uint32,BufferIndex,
	 uint32,BaseIndex,
	 uint32,NumBytes,
	 const void*,NewValue,
	 ,
	 );
DEFINE_RHIMETHOD_CMDLIST_5(
	void,SetShaderParameter,
	FComputeShaderRHIParamRef,ComputeShader,
	uint32,BufferIndex,
	uint32,BaseIndex,
	uint32,NumBytes,
	const void*,NewValue,
	,
	);

// Output state.
DEFINE_RHIMETHOD_CMDLIST_2(
	void,SetDepthStencilState,
	FDepthStencilStateRHIParamRef,NewState,
	uint32,StencilRef,
	,
	);
// Allows to set the blend state, parameter can be created with RHICreateBlendState()
DEFINE_RHIMETHOD_CMDLIST_2(
	void,SetBlendState,
	FBlendStateRHIParamRef,NewState,
	const FLinearColor&,BlendFactor,
	,
	);
DEFINE_RHIMETHOD_CMDLIST_5(
	void,SetRenderTargets,
	uint32,NumSimultaneousRenderTargets,
	const FRHIRenderTargetView*,NewRenderTargets,
	const FRHIDepthRenderTargetView*, NewDepthStencilTarget,
	uint32,NumUAVs,
	const FUnorderedAccessViewRHIParamRef*,UAVs,
	,
	);

DEFINE_RHIMETHOD_3(
	void,DiscardRenderTargets,
	bool,Depth,
	bool,Stencil,
	uint32,ColorBitMask,
	,
	);
DEFINE_RHIMETHOD_CMDLIST_1(
	void,SetRenderTargetsAndClear,
	const FRHISetRenderTargetsInfo&, RenderTargetsInfo,
	,
	);

// Bind a clear values to currently bound rendertargets.  This is used by platforms which
// need the color when transitioning a target that supports hardware clears from a rendertarget to a shader resource.
// The explicit bind is needed to support parallel rendering.
DEFINE_RHIMETHOD_CMDLIST_7(
	void, BindClearMRTValues,
	bool, bClearColor,
	int32, NumClearColors,
	const FLinearColor*, ColorArray,
	bool, bClearDepth,
	float, Depth,
	bool, bClearStencil,
	uint32, Stencil,	
	,
	);

// Primitive drawing.
DEFINE_RHIMETHOD_CMDLIST_4(
	void,DrawPrimitive,
	uint32,PrimitiveType,
	uint32,BaseVertexIndex,
	uint32,NumPrimitives,
	uint32,NumInstances,
	,
	);
DEFINE_RHIMETHOD_CMDLIST_3(
	void,DrawPrimitiveIndirect,
	uint32,PrimitiveType,
	FVertexBufferRHIParamRef,ArgumentBuffer,
	uint32,ArgumentOffset,
	,
	);

DEFINE_RHIMETHOD_CMDLIST_5(
	void,DrawIndexedIndirect,
	FIndexBufferRHIParamRef,IndexBufferRHI,
	uint32,PrimitiveType,
	FStructuredBufferRHIParamRef,ArgumentsBufferRHI,
	int32,DrawArgumentsIndex,
	uint32,NumInstances,
	,
	);

// @param NumPrimitives need to be >0 
DEFINE_RHIMETHOD_CMDLIST_8(
	void,DrawIndexedPrimitive,
	FIndexBufferRHIParamRef,IndexBuffer,
	uint32,PrimitiveType,
	int32,BaseVertexIndex,
	uint32,FirstInstance,
	uint32,NumVertices,
	uint32,StartIndex,
	uint32,NumPrimitives,
	uint32,NumInstances,
	,
	);
DEFINE_RHIMETHOD_CMDLIST_4(
	void,DrawIndexedPrimitiveIndirect,
	uint32,PrimitiveType,
	FIndexBufferRHIParamRef,IndexBuffer,
	FVertexBufferRHIParamRef,ArgumentBuffer,
	uint32,ArgumentOffset,
	,
	);

// Immediate Primitive drawing
/**
 * Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawPrimitiveUP
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param NumPrimitives The number of primitives in the VertexData buffer
 * @param NumVertices The number of vertices to be written
 * @param VertexDataStride Size of each vertex 
 * @param OutVertexData Reference to the allocated vertex memory
 */
DEFINE_RHIMETHOD_CMDLIST_5(
	void,BeginDrawPrimitiveUP,
	uint32,PrimitiveType,
	uint32,NumPrimitives,
	uint32,NumVertices,
	uint32,VertexDataStride,
	void*&,OutVertexData,
	,OutVertexData = GetStaticBuffer();
	);

/**
 * Draw a primitive using the vertex data populated since RHIBeginDrawPrimitiveUP and clean up any memory as needed
 */
DEFINE_RHIMETHOD_CMDLIST_0(
	void,EndDrawPrimitiveUP,
	,
	);

/**
 * Preallocate memory or get a direct command stream pointer to fill up for immediate rendering . This avoids memcpys below in DrawIndexedPrimitiveUP
 * @param PrimitiveType The type (triangles, lineloop, etc) of primitive to draw
 * @param NumPrimitives The number of primitives in the VertexData buffer
 * @param NumVertices The number of vertices to be written
 * @param VertexDataStride Size of each vertex
 * @param OutVertexData Reference to the allocated vertex memory
 * @param MinVertexIndex The lowest vertex index used by the index buffer
 * @param NumIndices Number of indices to be written
 * @param IndexDataStride Size of each index (either 2 or 4 bytes)
 * @param OutIndexData Reference to the allocated index memory
 */
DEFINE_RHIMETHOD_CMDLIST_9(
	void,BeginDrawIndexedPrimitiveUP,
	uint32,PrimitiveType,
	uint32,NumPrimitives,
	uint32,NumVertices,
	uint32,VertexDataStride,
	void*&,OutVertexData,
	uint32,MinVertexIndex,
	uint32,NumIndices,
	uint32,IndexDataStride,
	void*&,OutIndexData,
	,OutVertexData = GetStaticBuffer(); OutIndexData = GetStaticBuffer();
	);

/**
 * Draw a primitive using the vertex and index data populated since RHIBeginDrawIndexedPrimitiveUP and clean up any memory as needed
 */
DEFINE_RHIMETHOD_CMDLIST_0(
	void,EndDrawIndexedPrimitiveUP,
	,
	);

/*
 * Raster operations.
 * This method clears all MRT's, but to only one color value
 * @param ExcludeRect within the viewport in pixels, is only a hint to optimize - if a fast clear can be done this is preferred
 */
DEFINE_RHIMETHOD_CMDLIST_7(
	void,Clear,
	bool,bClearColor,
	const FLinearColor&,Color,
	bool,bClearDepth,
	float,Depth,
	bool,bClearStencil,
	uint32,Stencil,
	FIntRect,ExcludeRect,
	,
	);

/*
 * This method clears all MRT's to potentially different color values
 * @param ExcludeRect within the viewport in pixels, is only a hint to optimize - if a fast clear can be done this is preferred
 */
DEFINE_RHIMETHOD_CMDLIST_8(
	void,ClearMRT,
	bool,bClearColor,
	int32,NumClearColors,
	const FLinearColor*,ColorArray,
	bool,bClearDepth,
	float,Depth,
	bool,bClearStencil,
	uint32,Stencil,
	FIntRect,ExcludeRect,
	,
	);

// Blocks the CPU until the GPU catches up and goes idle.
DEFINE_RHIMETHOD_0(
	void,BlockUntilGPUIdle,
	,
	);

// Operations to suspend title rendering and yield control to the system
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_0(
	void,SuspendRendering,
	,
	);
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_0(
	void,ResumeRendering,
	,
	);
DEFINE_RHIMETHOD_0(
	bool,IsRenderingSuspended,
	return,return false;
	);

/**
 *	Retrieve available screen resolutions.
 *
 *	@param	Resolutions			TArray<FScreenResolutionRHI> parameter that will be filled in.
 *	@param	bIgnoreRefreshRate	If true, ignore refresh rates.
 *
 *	@return	bool				true if successfully filled the array
 */
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_2(
	bool,GetAvailableResolutions,
	FScreenResolutionArray&,Resolutions,
	bool,bIgnoreRefreshRate,
	return,return false;
	);

/**
 * Returns a supported screen resolution that most closely matches input.
 * @param Width - Input: Desired resolution width in pixels. Output: A width that the platform supports.
 * @param Height - Input: Desired resolution height in pixels. Output: A height that the platform supports.
 */
DEFINE_RHIMETHOD_GLOBALTHREADSAFE_2(
	void,GetSupportedResolution,
	uint32&,Width,
	uint32&,Height,
	,
	);

/**
 * Function that is used to allocate / free space used for virtual texture mip levels.
 * Make sure you also update the visible mip levels.
 * @param Texture - the texture to update, must have been created with TexCreate_Virtual
 * @param FirstMip - the first mip that should be in memory
 */
DEFINE_RHIMETHOD_GLOBAL_2(
	void,VirtualTextureSetFirstMipInMemory,
	FTexture2DRHIParamRef,Texture,
	uint32,FirstMip,
	,
	);

/**
 * Function that can be used to update which is the first visible mip to the GPU.
 * @param Texture - the texture to update, must have been created with TexCreate_Virtual
 * @param FirstMip - the first mip that should be visible to the GPU
 */
DEFINE_RHIMETHOD_GLOBAL_2(
	void,VirtualTextureSetFirstMipVisible,
	FTexture2DRHIParamRef,Texture,
	uint32,FirstMip,
	,
	);

DEFINE_RHIMETHOD_GLOBAL_1(
	void,ExecuteCommandList,
	FRHICommandList*,CmdList,
	,
	);

/**
 * Enabled/Disables Depth Bounds Testing with the given min/max depth.
 * @param bEnable	Enable(non-zero)/disable(zero) the depth bounds test
 * @param MinDepth	The minimum depth for depth bounds test
 * @param MaxDepth	The maximum depth for depth bounds test.
 *					The valid values for fMinDepth and fMaxDepth are such that 0 <= fMinDepth <= fMaxDepth <= 1
 */
DEFINE_RHIMETHOD_CMDLIST_3(
	void,EnableDepthBoundsTest,
	bool,bEnable,
	float,MinDepth,
	float,MaxDepth,
	,
	);

/**
 * Provides access to the native device. Generally this should be avoided but is useful for third party plugins.
 */
DEFINE_RHIMETHOD_GLOBALFLUSH_0(
	void*,GetNativeDevice,
	return,return NULL
	);

DEFINE_RHIMETHOD_CMDLIST_1(
	void,PushEvent,
	const TCHAR*, Name,
	,
	);
DEFINE_RHIMETHOD_CMDLIST_0(
	void,PopEvent,
	,
	);

DEFINE_RHIMETHOD_GLOBALTHREADSAFE_0(
	class IRHICommandContext*,GetDefaultContext,
	return,return this
	);

DEFINE_RHIMETHOD_GLOBALTHREADSAFE_0(
	class IRHICommandContextContainer*,GetCommandContextContainer,
	return,return nullptr
	);



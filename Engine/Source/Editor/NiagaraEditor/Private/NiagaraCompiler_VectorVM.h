// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraCompiler.h"
#include "VectorVM.h"

/** A Niagara compiler that produces byte code to run on the VectorVM. */
class NIAGARAEDITOR_API FNiagaraCompiler_VectorVM : public FNiagaraCompiler
{
private:

	/** Creates an expression directly mapping to a VM operation. */
	TNiagaraExprPtr Expression_VMNative(EVectorVMOp Op, TArray<TNiagaraExprPtr>& InputExpressions);
	TNiagaraExprPtr Expression_VMNative(EVectorVMOp Op, TNiagaraExprPtr A);
	TNiagaraExprPtr Expression_VMNative(EVectorVMOp Op, TNiagaraExprPtr A, TNiagaraExprPtr B);
	TNiagaraExprPtr Expression_VMNative(EVectorVMOp Op, TNiagaraExprPtr A, TNiagaraExprPtr B, TNiagaraExprPtr C);
	TNiagaraExprPtr Expression_VMNative(EVectorVMOp Op, TNiagaraExprPtr A, TNiagaraExprPtr B, TNiagaraExprPtr C, TNiagaraExprPtr D);

	/** Array tracking which temp registers are free allowing their re-use and thus a smaller total number. */
	TArray<bool> TempRegisters;
public:

	/** Returns true if the expression is stored in the constant data. */
	bool ExpressionIsConstant(FNiagaraExpression*  Expression);
	bool ExpressionIsBufferConstant(FNiagaraExpression*  Expression);
	EVectorVMOperandLocation GetVMOperandType(FNiagaraExpression *Expression);

	/** Gets the final index used in the VM bytecode for the passed expression. */
	uint8 GetResultVMIndex(FNiagaraExpression*  Expression);

	/** Writes to the scripts bytecode. */
	void WriteCode(uint8 InCode);

	//Begin INiagaraCompiler Interface	
	virtual bool CompileScript(UNiagaraScript* InScript) override;

#define NiagaraOp(OpName) virtual bool OpName##_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions)override;
		NiagaraOpList;
#undef NiagaraOp

	virtual TNiagaraExprPtr Output(const FNiagaraVariableInfo& Attr, TNiagaraExprPtr& SourceExpression)override;
	virtual TNiagaraExprPtr SharedDataRead(const FNiagaraDataSetID& DataSet, const FNiagaraVariableInfo& Variable, TNiagaraExprPtr& IndexExpr) override;
	virtual TNiagaraExprPtr SharedDataWrite(const FNiagaraDataSetID& DataSet, const FNiagaraVariableInfo& Variable, TNiagaraExprPtr& IndexExpr, TNiagaraExprPtr& DataExpr) override;


	virtual TNiagaraExprPtr AcquireSharedDataIndex(const FNiagaraDataSetID& DataSet, bool bWrap, TNiagaraExprPtr& ValidExpr) override;
	virtual TNiagaraExprPtr ConsumeSharedDataIndex(const FNiagaraDataSetID& DataSet, bool bWrap) override;
	virtual TNiagaraExprPtr SharedDataIndexIsValid(const FNiagaraDataSetID& DataSet, TNiagaraExprPtr& IndexExpr) override;
	//End INiagaraCompiler;

	//Begin FNiagaraCompiler Interface
	virtual ENiagaraDataType GetConstantResultIndex(const FNiagaraVariableInfo& Constant, bool bInternal, int32& OutResultIndex)override;
	virtual int32 AquireTemporary()override;
	virtual void FreeTemporary(int32 TempIndex) override;
	//End INiagaraCompiler Interface
};

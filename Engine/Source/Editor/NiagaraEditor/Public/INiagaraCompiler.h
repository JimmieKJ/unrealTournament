// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraEditorCommon.h"

class UNiagaraNode;

//Interface for Niagara compilers.
class INiagaraCompiler
{
public:

	/** Compiles a script. */
	virtual bool CompileScript(class UNiagaraScript* InScript) = 0;

	/** Traverses the pins input, recursively compiling them and then adds any expressions needed for the owner node.*/
	virtual TNiagaraExprPtr CompilePin(UEdGraphPin* Pin) = 0;
	
	/** Creates an expression which fetches the named attribute. */
	virtual TNiagaraExprPtr GetAttribute(const FNiagaraVariableInfo& Attribute) = 0;
	/** Creates an expression which fetches the named constant. */
	virtual TNiagaraExprPtr GetExternalConstant(const FNiagaraVariableInfo& Constant, float Default) = 0;
	/** Creates an expression which fetches the named constant. */
	virtual TNiagaraExprPtr GetExternalConstant(const FNiagaraVariableInfo& Constant, const FVector4& Default) = 0;
	/** Creates an expression which fetches the named constant. */
	virtual TNiagaraExprPtr GetExternalConstant(const FNiagaraVariableInfo& Constant, const FMatrix& Default) = 0;

	/** Creates an expression which fetches the named constant. */
	virtual TNiagaraExprPtr GetInternalConstant(const FNiagaraVariableInfo& Constant, float Default) = 0;
	/** Creates an expression which fetches the named constant. */
	virtual TNiagaraExprPtr GetInternalConstant(const FNiagaraVariableInfo& Constant, const FVector4& Default) = 0;
	/** Creates an expression which fetches the named constant. */
	virtual TNiagaraExprPtr GetInternalConstant(const FNiagaraVariableInfo& Constant, const FMatrix& Default) = 0;

	/** Creates an expression which writes the result of SourceExpression to the named attribute. */
	virtual TNiagaraExprPtr Output(const FNiagaraVariableInfo& Attr, TNiagaraExprPtr& SourceExpression) = 0;

	/** Creates an expression which reads data from a shared data set. */
	virtual TNiagaraExprPtr SharedDataRead(const FNiagaraDataSetID& DataSet, const FNiagaraVariableInfo& Variable, TNiagaraExprPtr& IndexExpr) = 0;
	/** Creates an expression which writes data to an shared data set. */
	virtual TNiagaraExprPtr SharedDataWrite(const FNiagaraDataSetID& DataSet, const FNiagaraVariableInfo& Variable, TNiagaraExprPtr& IndexExpr, TNiagaraExprPtr& DataExpr) = 0;

	/** Creates an expression which gets the current counter for the shared data set and increments the counter. */
	virtual TNiagaraExprPtr AcquireSharedDataIndex(const FNiagaraDataSetID& DataSet, bool bWrap, TNiagaraExprPtr& ValidExpr) = 0;
	/** Creates an expression which gets the current counter for the shared data set and decrements the counter. */
	virtual TNiagaraExprPtr ConsumeSharedDataIndex(const FNiagaraDataSetID& DataSet, bool bWrap) = 0;
	/** Creates an expression with a mask of 0 or 0xFFFFFFFF marking the passed index as valid or not.*/
	virtual TNiagaraExprPtr SharedDataIndexIsValid(const FNiagaraDataSetID& DataSet, TNiagaraExprPtr& IndexExpr) = 0;

	/** Checks the validity of the input expressions against the informations about this operation in FNiagaraOpInfo. */
	virtual bool CheckInputs(FName OpName, TArray<TNiagaraExprPtr>& Inputs) = 0;
	/** Checks the validity of the output expressions against the informations about this operation in FNiagaraOpInfo. */
	virtual bool CheckOutputs(FName OpName, TArray<TNiagaraExprPtr>& Outputs) = 0;

	/** Adds an error to be reported to the user. Any error will lead to compilation failure. */
	virtual void Error(FText ErrorText, UNiagaraNode* Node, UEdGraphPin* Pin) = 0 ;

	/** Adds a warning to be reported to the user. Warnings will not cause a compilation failure. */
	virtual void Warning(FText WarningText, UNiagaraNode* Node, UEdGraphPin* Pin) = 0;

	//Static function named after each op that allows access to internals via FNiagaraOpInfo.Func.
#define NiagaraOp(OpName) static bool OpName(INiagaraCompiler* Compiler, TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions);
	NiagaraOpList;
#undef NiagaraOp

protected:
	//Internal virtual function for each op. 
#define NiagaraOp(OpName) virtual bool OpName##_Internal(TArray<TNiagaraExprPtr>& InputExpressions, TArray<TNiagaraExprPtr>& OutputExpressions) = 0;
	NiagaraOpList;
#undef NiagaraOp

};
// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "EdGraph/EdGraph.h"
#include "NiagaraGraph.generated.h"

UCLASS(MinimalAPI)
class UNiagaraGraph : public UEdGraph
{
	GENERATED_UCLASS_BODY()

	//~ Begin UObject Interface
	virtual void PostEditChangeProperty(struct FPropertyChangedEvent& PropertyChangedEvent)override;
	//~ End UObjet Interface
	
	/** Get the source that owns this graph */
	class UNiagaraScriptSource* GetSource() const;
		
	class UNiagaraNodeOutput* FindOutputNode() const;
	void FindInputNodes(TArray<class UNiagaraNodeInput*>& OutInputNodes) const;

	void FindReadDataSetNodes(TArray<class UNiagaraNodeReadDataSet*>& OutReadNodes) const;
	void FindWriteDataSetNodes(TArray<class UNiagaraNodeWriteDataSet*>& OutWriteNodes) const;

	/** Returns the index of this attribute in the output node of the graph. INDEX_NONE if this is not a valid attribute. */
	int32 GetAttributeIndex(const FNiagaraVariableInfo& Attr)const;
	void GetAttributes(TArray< FNiagaraVariableInfo >& OutAttributes)const;
};


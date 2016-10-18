// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraConstantSet.h"

#include "NiagaraScriptConstantData.generated.h"

USTRUCT()
struct FNiagaraScriptConstantData
{
	GENERATED_USTRUCT_BODY()

	/** The set of external constants for this script. */
	/** Constants driven by the system. Named New for BC reasons. Once all data is updated beyond VER_UE4_NIAGARA_DATA_OBJECT_DEV_UI_FIX. Get rid of the deprecated consts and rename the New. */
	UPROPERTY()
	FNiagaraConstants ExternalConstantsNew;

	/** The set of internal constants for this script. Named New for BC reasons. Once all data is updated beyond VER_UE4_NIAGARA_DATA_OBJECT_DEV_UI_FIX. Get rid of the deprecated consts and rename the New. */
	UPROPERTY()
	FNiagaraConstants InternalConstantsNew;

	void Empty()
	{
		ExternalConstantsNew.Empty();
		InternalConstantsNew.Empty();
	}

	FNiagaraConstants& GetExternalConstants(){ return ExternalConstantsNew; }

	void FillConstantTable(const FNiagaraConstants& ExternalConstants, TArray<FVector4>& OutConstantTable) const
	{
		//First up in the table comes the External constants in scalar->vector->matrix order.
		//Only fills the constants actually used by the script.
		OutConstantTable.Empty();
		ExternalConstantsNew.AppendToConstantsTable(OutConstantTable, ExternalConstants);
		//Next up add all the internal constants from the script.
		InternalConstantsNew.AppendToConstantsTable(OutConstantTable);
	}



	template<typename T>
	void SetOrAddInternal(const FNiagaraVariableInfo& Constant, const T& Value)
	{
		InternalConstantsNew.SetOrAdd(Constant, Value);
	}

	template<typename T>
	void SetOrAddExternal(const FNiagaraVariableInfo& Constant, const T& Value)
	{
		ExternalConstantsNew.SetOrAdd(Constant, Value);
	}

	/**
	Calculates the index into a a constant table created from this constant data for the given constant name.
	Obviously, assumes the constant data is complete. Any additions etc will invalidate previously calculated indexes.
	*/
	void GetTableIndex(const FNiagaraVariableInfo& Constant, bool bInternal, int32& OutConstantIdx, ENiagaraDataType& OutType)const
	{
		int32 Base = bInternal ? ExternalConstantsNew.GetTableSize() : 0;
		int32 ConstIdx = INDEX_NONE;
		ENiagaraDataType Type;
		const FNiagaraConstants& Consts = bInternal ? InternalConstantsNew : ExternalConstantsNew;

		ConstIdx = Consts.GetTableIndex_Scalar(Constant);
		Type = ENiagaraDataType::Scalar;
		if (ConstIdx == INDEX_NONE)
		{
			ConstIdx = Consts.GetTableIndex_Vector(Constant);
			Type = ENiagaraDataType::Vector;
			if (ConstIdx == INDEX_NONE)
			{
				ConstIdx = Consts.GetTableIndex_Matrix(Constant);
				Type = ENiagaraDataType::Matrix;
			}
		}

		OutConstantIdx = Base + ConstIdx;
		OutType = Type;
	}
};

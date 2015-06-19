// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "NiagaraConstantSet.h"

#include "NiagaraScriptConstantData.generated.h"

USTRUCT()
struct FNiagaraScriptConstantData
{
	GENERATED_USTRUCT_BODY()

	/** The set of external constants for this script. */
	UPROPERTY()
	FNiagaraConstants ExternalConstants;

	/** The set of internal constants for this script. */
	UPROPERTY()
	FNiagaraConstants InternalConstants;

	void Empty()
	{
		ExternalConstants.Empty();
		InternalConstants.Empty();
	}

	/** Fill a constants table ready for use in an update or spawn. */
	void FillConstantTable(const FNiagaraConstantMap& ExternalConstantsMap, TArray<FVector4>& OutConstantTable, TArray<FNiagaraDataObject *> &DataObjTable) const
	{
		//First up in the table comes the External constants in scalar->vector->matrix order.
		//Only fills the constants actually used by the script.
		OutConstantTable.Empty();
		ExternalConstants.AppendToConstantsTable(OutConstantTable, ExternalConstantsMap);
		ExternalConstants.AppendExternalBufferConstants(DataObjTable, ExternalConstantsMap);
		//Next up add all the internal constants from the script.
		InternalConstants.AppendToConstantsTable(OutConstantTable);
		InternalConstants.AppendBufferConstants(DataObjTable);
	}

	template<typename T>
	void SetOrAddInternal(const FNiagaraVariableInfo& Constant, const T& Value)
	{
		InternalConstants.SetOrAdd(Constant, Value);
	}

	template<typename T>
	void SetOrAddExternal(const FNiagaraVariableInfo& Constant, const T& Value)
	{
		ExternalConstants.SetOrAdd(Constant, Value);
	}

	/**
	Calculates the index into a a constant table created from this constant data for the given constant name.
	Obviously, assumes the constant data is complete. Any additions etc will invalidate previously calculated indexes.
	*/
	void GetTableIndex(const FNiagaraVariableInfo& Constant, bool bInternal, int32& OutConstantIdx, int32& OutComponentIndex, ENiagaraDataType& OutType)const
	{
		int32 Base = bInternal ? ExternalConstants.GetTableSize() : 0;
		int32 ConstIdx = INDEX_NONE;
		int32 CompIdx = INDEX_NONE;
		ENiagaraDataType Type;
		const FNiagaraConstants& Consts = bInternal ? InternalConstants : ExternalConstants;

		ConstIdx = Consts.GetAbsoluteIndex_Scalar(Constant);
		Type = ENiagaraDataType::Scalar;
		if (ConstIdx == INDEX_NONE)
		{
			ConstIdx = Consts.GetTableIndex_Vector(Constant);
			Type = ENiagaraDataType::Vector;
			if (ConstIdx == INDEX_NONE)
			{
				ConstIdx = Consts.GetTableIndex_Matrix(Constant);
				Type = ENiagaraDataType::Matrix;

				if (ConstIdx == INDEX_NONE)	// curves/buffers are in a separate table, so set base to 0
				{
					ConstIdx = Consts.GetTableIndex_DataObj(Constant);
					Type = ENiagaraDataType::Curve;
					Base = 0;
				}
			}
		}
		else
		{
			CompIdx = ConstIdx % 4;
			ConstIdx /= 4;
		}

		OutConstantIdx = Base + ConstIdx;
		OutComponentIndex = CompIdx;
		OutType = Type;
	}
};

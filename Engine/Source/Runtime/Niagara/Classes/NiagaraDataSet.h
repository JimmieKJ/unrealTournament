// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"

/** Storage for Niagara data. */
class FNiagaraDataSet
{
public:
	FNiagaraDataSet()
	{
		Reset();
	}

	FNiagaraDataSet(FNiagaraDataSetID InID)
		: ID(InID)
	{
		Reset();
	}

	~FNiagaraDataSet()
	{
		Reset();
	}

	void Allocate(uint32 NumExpectedInstances, bool bReset = true)
	{
		if (bReset)
		{
			DataBuffers[CurrentBuffer].Reset(NumExpectedInstances * VariableMap.Num());
			DataBuffers[CurrentBuffer].AddUninitialized(NumExpectedInstances * VariableMap.Num());
		}
		else
		{
			DataBuffers[CurrentBuffer].SetNumUninitialized(NumExpectedInstances * VariableMap.Num());
		}

		DataAllocation[CurrentBuffer] = NumExpectedInstances;
	}

	void Reset()
	{
		VariableMap.Empty();
		NumInstances[0] = 0;
		NumInstances[1] = 0;
		DataAllocation[0] = 0;
		DataAllocation[1] = 0;
		CurrentBuffer = 0;
		DataBuffers[0].Empty();
		DataBuffers[1].Empty();
	}

	FVector4* GetVariableData(const FNiagaraVariableInfo& VariableID, uint32 StartParticle = 0)
	{
		const uint32* Offset = VariableMap.Find(VariableID);
		return Offset ? DataBuffers[CurrentBuffer].GetData() + (*Offset * DataAllocation[CurrentBuffer]) + StartParticle : NULL;
	}

	const FVector4* GetVariableData(const FNiagaraVariableInfo& VariableID, uint32 StartParticle = 0)const
	{
		const uint32* Offset = VariableMap.Find(VariableID);
		return Offset ? DataBuffers[CurrentBuffer].GetData() + (*Offset * DataAllocation[CurrentBuffer]) + StartParticle : NULL;
	}

	FVector4* GetPrevVariableData(const FNiagaraVariableInfo& VariableID, uint32 StartParticle = 0)
	{
		const uint32* Offset = VariableMap.Find(VariableID);
		return Offset ? DataBuffers[CurrentBuffer ^ 0x1].GetData() + (*Offset * DataAllocation[CurrentBuffer ^ 0x1]) + StartParticle : NULL;
	}

	const FVector4* GetPrevVariableData(const FNiagaraVariableInfo& VariableID, uint32 StartParticle = 0)const
	{
		const uint32* Offset = VariableMap.Find(VariableID);
		return Offset ? DataBuffers[CurrentBuffer ^ 0x1].GetData() + (*Offset * DataAllocation[CurrentBuffer ^ 0x1]) + StartParticle : NULL;
	}

	void GetVariableData(const FNiagaraVariableInfo& VariableID, FVector4*& PrevBuffer, FVector4*& CurrBuffer, uint32 StartParticle)
	{
		const uint32* Offset = VariableMap.Find(VariableID);
		if (Offset)
		{
			PrevBuffer = DataBuffers[CurrentBuffer ^ 0x1].GetData() + (*Offset * DataAllocation[CurrentBuffer ^ 0x1]) + StartParticle;
			CurrBuffer = DataBuffers[CurrentBuffer].GetData() + (*Offset * DataAllocation[CurrentBuffer]) + StartParticle;
		}
		else
		{
			PrevBuffer = NULL;
			CurrBuffer = NULL;
		}
	}

	bool HasAttriubte(const FNiagaraVariableInfo& VariableID)
	{
		return VariableMap.Find(VariableID) != NULL;
	}

	void SetVariables(const TArray<FNiagaraVariableInfo>& VariableIDs)
	{
		Reset();
		for (const FNiagaraVariableInfo& Var : VariableIDs)
		{
			//TODO: Support attributes of any type. This may as well wait until the move of matrix ops etc over into UNiagaraScriptStructs.
			uint32 Idx = VariableMap.Num();
			VariableMap.Add(Var, Idx);
		}
	}

	void AddVariables(const TArray<FNiagaraVariableInfo>& Variables)
	{
		for (const FNiagaraVariableInfo& Variable : Variables)
		{
			AddVariable(Variable);
		}
	}

	void AddVariable(const FNiagaraVariableInfo& Variable)
	{
		//TODO: Support attributes of any type. This may as well wait until the move of matrix ops etc over into UNiagaraScriptStructs.
		uint32 Idx = VariableMap.Num();
		if (!VariableMap.Find(Variable))
		{
			VariableMap.Add(Variable, Idx);
		}
	}

	const TMap<FNiagaraVariableInfo, uint32>& GetVariables()const { return VariableMap; }
	int GetNumVariables()				{ return VariableMap.Num(); }

	uint32 GetNumInstances() const		{ return NumInstances[CurrentBuffer]; }
	uint32 GetPrevNumInstances() const		{ return NumInstances[CurrentBuffer ^ 0x1]; }
	uint32 GetDataAllocation() const { return DataAllocation[CurrentBuffer]; }
	uint32 GetPrevDataAllocation() const { return DataAllocation[CurrentBuffer ^ 0x1]; }
	void SetNumInstances(uint32 Num)	{ NumInstances[CurrentBuffer] = Num; }
	void SwapBuffers()					{ CurrentBuffer ^= 0x1; }

	FVector4* GetCurrentBuffer()		{ return DataBuffers[CurrentBuffer].GetData(); }
	FVector4* GetPreviousBuffer()		{ return DataBuffers[CurrentBuffer ^ 0x1].GetData(); }

	int GetBytesUsed()	{ return (DataBuffers[0].Num() + DataBuffers[1].Num()) * 16 + VariableMap.Num() * 4; }

	FNiagaraDataSetID GetID()const { return ID; }
	void SetID(FNiagaraDataSetID InID) { ID = InID; }

	void Tick()
	{
		SwapBuffers();

		if (ID.Type == ENiagaraDataSetType::Event)
		{
			SetNumInstances(0);
			Allocate(GetPrevDataAllocation());
			//Fancier allocation for events?
		}
		else if (ID.Type == ENiagaraDataSetType::ParticleData)
		{
			SetNumInstances(GetPrevNumInstances());
		}
	}

private:
	uint32 CurrentBuffer;
	uint32 NumInstances[2];
	uint32 DataAllocation[2];
	TArray<FVector4> DataBuffers[2];
	TMap<FNiagaraVariableInfo, uint32> VariableMap;
	FNiagaraDataSetID ID;
};


// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.
#pragma once

#include "NiagaraCommon.h"

const FName BUILTIN_VAR_PARTICLEAGE = FName(TEXT("Age"));

#define SEPARATE_PER_VARIABLE_DATA 1




/** Niagara Data Buffer
  * Holds a number of elements of data of a specific type; set Target to determine
  * whether CPU or GPU side
  */
class FNiagaraDataBuffer
{
public:
	FNiagaraDataBuffer()
		: DataType(ENiagaraDataType::Vector)
		, ElementType(ENiagaraType::Float)
		, Target(ENiagaraSimTarget::CPUSim)
	{
	}

	ENiagaraSimTarget GetTarget()
	{
		return Target;
	}

	// when changing sim target at runtime, make sure to Reset/Alloc
	// generally only happens when changing sim type on emitters, case components are usually re-registered and no further work is needed
	void SetTarget(ENiagaraSimTarget InTarget)
	{
		Target = InTarget;
	}

	void Allocate(int32 NumElements)
	{
		uint32 SizePerElement = GetNiagaraBytesPerElement(DataType, ElementType);
		uint32 SizeInBytes = NumElements * SizePerElement;

		if (Target == CPUSim)
		{
			CPUBuffer.AddUninitialized(SizeInBytes);
		}
		else if (Target == GPUComputeSim)
		{
			FRHIResourceCreateInfo CreateInfo;
			CreateInfo.ClearValueBinding = FClearValueBinding::None;
			CreateInfo.BulkData = nullptr;
			GPUBuffer = RHICreateStructuredBuffer(SizePerElement, SizeInBytes, BUF_ShaderResource, CreateInfo);
		}
	}

	void Reset(uint32 NumElements)
	{
		if (Target == CPUSim)
		{
			CPUBuffer.Reset(NumElements);
		}
		else if (Target == GPUComputeSim)
		{
			FRHIResourceCreateInfo CreateInfo;
			uint32 SizePerElement = GetNiagaraBytesPerElement(DataType, ElementType);
			uint32 SizeInBytes = NumElements * SizePerElement;
			GPUBuffer = RHICreateStructuredBuffer(SizePerElement, SizeInBytes, BUF_ShaderResource, CreateInfo);
		}
	}

	const TArray<FVector4>& GetCPUBuffer() const
	{
		return CPUBuffer;
	}

	TArray<FVector4>& GetCPUBuffer() 
	{
		return CPUBuffer;
	}


private:
	ENiagaraDataType DataType;
	ENiagaraType ElementType;
	FStructuredBufferRHIRef GPUBuffer;
	TArray<FVector4> CPUBuffer;
	ENiagaraSimTarget Target;
};



/** Storage for a single particle attribute for all particles of one emitter 
  * We double buffer at this level, so we can avoid copying or explicitly dealing
  * with unused attributes.
  */
class FNiagaraVariableData
{
public:
	FNiagaraVariableData()
		: CurrentBuffer(0)
		, bPassThroughEnabled(false)
	{}
	
	TArray<FVector4> &GetCurrentBuffer() { return DataBuffers[CurrentBuffer].GetCPUBuffer(); }
	TArray<FVector4> &GetPrevBuffer() { return DataBuffers[CurrentBuffer ^ 0x1].GetCPUBuffer(); }

	const TArray<FVector4> &GetCurrentBuffer() const { return DataBuffers[CurrentBuffer].GetCPUBuffer(); }
	const TArray<FVector4> &GetPrevBuffer() const { return DataBuffers[CurrentBuffer ^ 0x1].GetCPUBuffer(); }

	void Allocate(uint32 NumElements)
	{
		//DataBuffers[CurrentBuffer].AddUninitialized(NumElements);
		DataBuffers[CurrentBuffer].Allocate(NumElements);
	}

	void Reset(uint32 NumElements)
	{
		DataBuffers[CurrentBuffer].Reset(NumElements);
	}

	void SwapBuffers()
	{ 
		// Passthrough is usually enabled when an attribute is unused during VM execution; 
		// in this case, we don't swap buffers, to avoid needing to copy source to destination
		if (!bPassThroughEnabled)
		{
			CurrentBuffer ^= 0x1;
		}
	}

	// Passthrough enabled means no double buffering; to be used for unused attributes only
	void EnablePassThrough() { bPassThroughEnabled = true; }
	void DisablePassThrough() { bPassThroughEnabled = false; }


private:
	ENiagaraDataType Type;
	uint8 CurrentBuffer;
	FNiagaraDataBuffer DataBuffers[2];
	FStructuredBufferRHIRef GPUDataBuffers[2];

	bool bPassThroughEnabled;
};




/** Storage for Niagara data. 
  * Holds a set of FNiagaraVariableData along with mapping from FNiagaraVariableInfos
  * Generally, this is the entire set of particle attributes for one emitter
  */
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
			VariableDataArray.SetNumZeroed(VariableMap.Num());
			for (auto &Data : VariableDataArray)
			{
				Data.Reset(NumExpectedInstances);
				Data.Allocate(NumExpectedInstances);
			}
		}
		else
		{
			VariableDataArray.SetNumUninitialized(VariableMap.Num());
			for (auto &Data : VariableDataArray)
			{
				Data.Allocate(NumExpectedInstances);
			}
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
	}

	FVector4* GetVariableData(const FNiagaraVariableInfo& VariableID, uint32 StartParticle = 0)
	{
		const uint32* Offset = VariableMap.Find(VariableID);
		return Offset ? VariableDataArray[*Offset].GetCurrentBuffer().GetData() + StartParticle : NULL;
	}

	const FVector4* GetVariableData(const FNiagaraVariableInfo& VariableID, uint32 StartParticle = 0)const
	{
		const uint32* Offset = VariableMap.Find(VariableID);
		return Offset ? VariableDataArray[*Offset].GetCurrentBuffer().GetData() + StartParticle : NULL;
	}

	FVector4* GetPrevVariableData(const FNiagaraVariableInfo& VariableID, uint32 StartParticle = 0)
	{
		const uint32* Offset = VariableMap.Find(VariableID);
		return Offset ? VariableDataArray[*Offset].GetPrevBuffer().GetData() + StartParticle : NULL;
	}

	const FVector4* GetPrevVariableData(const FNiagaraVariableInfo& VariableID, uint32 StartParticle = 0)const
	{
		const uint32* Offset = VariableMap.Find(VariableID);
		return Offset ? VariableDataArray[*Offset].GetPrevBuffer().GetData() + StartParticle : NULL;
	}

	void GetVariableData(const FNiagaraVariableInfo& VariableID, FVector4*& PrevBuffer, FVector4*& CurrBuffer, uint32 StartParticle)
	{
		const uint32* Offset = VariableMap.Find(VariableID);
		if (Offset)
		{
			PrevBuffer = VariableDataArray[*Offset].GetPrevBuffer().GetData() + StartParticle;
			CurrBuffer = VariableDataArray[*Offset].GetCurrentBuffer().GetData() + StartParticle;
		}
		else
		{
			PrevBuffer = NULL;
			CurrBuffer = NULL;
		}
	}


	FNiagaraVariableData &GetVariableBuffer(const FNiagaraVariableInfo& VariableID)
	{
		const uint32* VarIdx = VariableMap.Find(VariableID);
		return VariableDataArray[*VarIdx];
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

	FVector4* GetCurrentBuffer(uint32 AttrIndex) { return VariableDataArray[AttrIndex].GetCurrentBuffer().GetData(); }
	FVector4* GetPreviousBuffer(uint32 AttrIndex) { return VariableDataArray[AttrIndex].GetPrevBuffer().GetData(); }

	int GetBytesUsed()	
	{ 
		if (VariableDataArray.Num() == 0)
		{
			return 0;
		}
		return (VariableDataArray[0].GetCurrentBuffer().Num() + VariableDataArray[0].GetPrevBuffer().Num()) * 16 + VariableMap.Num() * 4; 
	}

	FNiagaraDataSetID GetID()const { return ID; }
	void SetID(FNiagaraDataSetID InID) { ID = InID; }

	void Tick()
	{
		SwapBuffers();

		for (auto &VarData : VariableDataArray)
		{
			VarData.SwapBuffers();
		}

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
	uint32 DataAllocation[2];
	uint32 NumInstances[2];

	TMap<FNiagaraVariableInfo, uint32> VariableMap;
	FNiagaraDataSetID ID;
	TArray<FNiagaraVariableData> VariableDataArray;
};


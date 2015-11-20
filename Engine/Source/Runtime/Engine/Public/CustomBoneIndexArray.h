// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

template<typename ArrayType, typename BoneIndexType>
class TCustomBoneIndexArray : public TArray < ArrayType >
{
public:
	FORCEINLINE ArrayType& operator[](int32 BoneIndex)
	{ 
		return TArray<ArrayType>::operator [](BoneIndex); 
	}
	FORCEINLINE const ArrayType& operator[](int32 BoneIndex) const
	{ 
		return TArray<ArrayType>::operator [](BoneIndex); 
	}
	FORCEINLINE ArrayType& operator[](const BoneIndexType& BoneIndex) 
	{ 
		return TArray<ArrayType>::operator [] (BoneIndex.GetInt()); 
	}
	FORCEINLINE const ArrayType& operator[](const BoneIndexType& BoneIndex) const 
	{ 
		return TArray<ArrayType>::operator [] (BoneIndex.GetInt()); 
	}
};
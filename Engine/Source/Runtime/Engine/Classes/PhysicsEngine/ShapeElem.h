// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysxUserData.h"
#include "ShapeElem.generated.h"

namespace EAggCollisionShape
{
	enum Type
	{
		Sphere,
		Box,
		Sphyl,
		Convex,
		Unknown
	};
}

/** Sphere shape used for collision */
USTRUCT()
struct FKShapeElem
{
	GENERATED_USTRUCT_BODY()

	FKShapeElem()
	: ShapeType(EAggCollisionShape::Unknown)
#if WITH_PHYSX
	, UserData(this)
#endif
	
	{}

	FKShapeElem(EAggCollisionShape::Type InShapeType)
	: ShapeType(InShapeType)
#if WITH_PHYSX
	, UserData(this)
#endif
	{}

	FKShapeElem(const FKShapeElem& Copy)
	: ShapeType(Copy.ShapeType)
#if WITH_PHYSX
	, UserData(this)
#endif
	{
	}

	virtual ~FKShapeElem(){}

	template <typename T>
	T* GetShapeCheck()
	{
		check(T::StaticShapeType == ShapeType);
		return (T*)this;
	}

	const FPhysxUserData* GetUserData() const { FPhysxUserData::Set<FKShapeElem>((void*)&UserData, const_cast<FKShapeElem*>(this));  return &UserData; }

	ENGINE_API static EAggCollisionShape::Type StaticShapeType;

private:
	EAggCollisionShape::Type ShapeType;
	FPhysxUserData UserData;
};
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

#include "PhysicsPublic.h"

/*-----------------------------------------------------------------------------
   Hit Proxies
-----------------------------------------------------------------------------*/

struct HPhATEdBoneProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	int32							BodyIndex;
	EKCollisionPrimitiveType	PrimType;
	int32							PrimIndex;

	HPhATEdBoneProxy(int32 InBodyIndex, EKCollisionPrimitiveType InPrimType, int32 InPrimIndex):
	HHitProxy(HPP_UI),
		BodyIndex(InBodyIndex),
		PrimType(InPrimType),
		PrimIndex(InPrimIndex) {}
};

struct HPhATEdConstraintProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	int32							ConstraintIndex;

	HPhATEdConstraintProxy(int32 InConstraintIndex):
	HHitProxy(HPP_UI),
		ConstraintIndex(InConstraintIndex) {}
};

struct HPhATEdBoneNameProxy : public HHitProxy
{
	DECLARE_HIT_PROXY();

	int32			BoneIndex;

	HPhATEdBoneNameProxy(int32 InBoneIndex):
	HHitProxy(HPP_UI),
		BoneIndex(InBoneIndex) {}
};

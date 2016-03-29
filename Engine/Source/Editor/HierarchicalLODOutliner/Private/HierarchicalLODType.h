// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once


UENUM()	
enum class EHierarchicalLODActionType : uint8
{
	InvalidAction,
	CreateCluster,
	AddActorToCluster,
	MoveActorToCluster,
	RemoveActorFromCluster,
	MergeClusters,
	ChildCluster,
	Max
};
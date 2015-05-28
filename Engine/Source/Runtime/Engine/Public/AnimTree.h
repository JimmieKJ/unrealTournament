// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	AnimTree.h:	Animation tree element definitions and helper structs.
=============================================================================*/

#pragma once

/**
* Engine stats
*/
/** Skeletal stats */
DECLARE_CYCLE_STAT_EXTERN(TEXT("SkinnedMeshComp Tick"), STAT_SkinnedMeshCompTick, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("TickUpdateRate"), STAT_TickUpdateRate, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Anim Tick Time"), STAT_AnimTickTime, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("StateMachine Update"), STAT_AnimStateMachineUpdate, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("StateMachine Find Transition"), STAT_AnimStateMachineFindTransition, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("RefreshBoneTransforms"), STAT_RefreshBoneTransforms, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Evaluate Anim"), STAT_AnimBlendTime, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Perform Anim Evaluation"), STAT_PerformAnimEvaluation, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Post Anim Evaluation"), STAT_PostAnimEvaluation, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("GraphTime"), STAT_AnimGraphEvaluate, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Eval Pose"), STAT_AnimNativeEvaluatePoses, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Trigger Notifies"), STAT_AnimTriggerAnimNotifies, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Anim Decompression"), STAT_GetAnimationPose, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Blend Poses"), STAT_AnimNativeBlendPoses, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Copy Pose"), STAT_AnimNativeCopyPoses, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("StateMachine Eval"), STAT_AnimStateMachineEvaluate, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("FillSpaceBases"), STAT_SkelComposeTime, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("InterpolateSkippedFrames"), STAT_InterpolateSkippedFrames, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("UpdateKinematicBonesToAnim"), STAT_UpdateRBBones, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("UpdateRBJointsMotors"), STAT_UpdateRBJoints, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("UpdateLocalToWorldAndOverlaps"), STAT_UpdateLocalToWorldAndOverlaps, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("SkelComp UpdateTransform"), STAT_SkelCompUpdateTransform, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("MeshObject Update"), STAT_MeshObjectUpdate, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("Update SkelMesh Bounds"), STAT_UpdateSkelMeshBounds, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("BlendInPhysics"), STAT_BlendInPhysics, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("SkinPerPolyVertices);"), STAT_SkinPerPolyVertices, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("UpdateTriMeshVertices"), STAT_UpdateTriMeshVertices, STATGROUP_Anim, );
DECLARE_CYCLE_STAT_EXTERN(TEXT("AnimGameThreadTime"), STAT_AnimGameThreadTime, STATGROUP_Anim, );



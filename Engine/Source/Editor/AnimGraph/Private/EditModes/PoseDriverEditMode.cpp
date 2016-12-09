// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EditModes/PoseDriverEditMode.h"
#include "SceneManagement.h"
#include "AnimNodes/AnimNode_PoseDriver.h"
#include "AnimGraphNode_PoseDriver.h"
#include "IPersonaPreviewScene.h"
#include "Animation/DebugSkelMeshComponent.h"
#include "CanvasItem.h"
#include "CanvasTypes.h"

#define LOCTEXT_NAMESPACE "A3Nodes"

void FPoseDriverEditMode::EnterMode(class UAnimGraphNode_Base* InEditorNode, struct FAnimNode_Base* InRuntimeNode)
{
	RuntimeNode = static_cast<FAnimNode_PoseDriver*>(InRuntimeNode);
	GraphNode = CastChecked<UAnimGraphNode_PoseDriver>(InEditorNode);

	FAnimNodeEditMode::EnterMode(InEditorNode, InRuntimeNode);
}

void FPoseDriverEditMode::ExitMode()
{
	RuntimeNode = nullptr;
	GraphNode = nullptr;

	FAnimNodeEditMode::ExitMode();
}

static FLinearColor GetColorFromWeight(float InWeight)
{
	return FMath::Lerp(FLinearColor::White, FLinearColor::Red, InWeight);
}

void FPoseDriverEditMode::DrawHUD(FEditorViewportClient* ViewportClient, FViewport* Viewport, const FSceneView* View, FCanvas* Canvas)
{
	if (RuntimeNode == nullptr)
	{
		return;
	}

	// Get the node we are debugging
	USkeletalMeshComponent* SkelComp = GetAnimPreviewScene().GetPreviewMeshComponent();

	const int MarginX = 60;
	const int XDelta = 10;

	int PosY = 60;
	const int YDelta = 20;

	// Display bone we are watching
	FText InterpItemText = FText::Format(LOCTEXT("BoneFormat", "Bone: {0}"), FText::FromName(RuntimeNode->SourceBone.BoneName));
	FCanvasTextItem InterpItem(FVector2D(MarginX, PosY), InterpItemText, GEngine->GetSmallFont(), FLinearColor::White);
	Canvas->DrawItem(InterpItem);
	PosY += YDelta;

	int32 PoseIndex = 0;
	for (const FPoseDriverPoseInfo& PoseInfo : RuntimeNode->PoseInfos)
	{
		FString PoseItemString = FString::Printf(TEXT("%s : %f"), *PoseInfo.PoseName.ToString(), PoseInfo.PoseWeight);
		FCanvasTextItem PoseItem(FVector2D(MarginX + XDelta, PosY), FText::FromString(PoseItemString), GEngine->GetSmallFont(), GetColorFromWeight(PoseInfo.PoseWeight));
		Canvas->DrawItem(PoseItem);
		PosY += YDelta;

		PoseIndex++;
	}
}

void FPoseDriverEditMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	USkeletalMeshComponent* SkelComp = GetAnimPreviewScene().GetPreviewMeshComponent();

	static const float LineWidth = 0.1f;
	static const float DrawAxisLength = 20.f;
	static const float DrawPosSize = 1.f;

	int32 BoneIndex = SkelComp->GetBoneIndex(RuntimeNode->SourceBone.BoneName);
	if (BoneIndex != INDEX_NONE)
	{
		// Get transform of driven bone, used as basis for drawing
		FTransform BoneWorldTM = SkelComp->GetBoneTransform(BoneIndex);
		FVector BonePos = BoneWorldTM.GetLocation();

		// Get ref frame for drawing - rotation from parent, translation from driver bone
		FName ParentBoneName = SkelComp->GetParentBone(RuntimeNode->SourceBone.BoneName);
		int32 ParentBoneIndex = SkelComp->GetBoneIndex(ParentBoneName);
		FTransform ParentWorldTM = (ParentBoneIndex == INDEX_NONE) ? SkelComp->GetComponentToWorld() : SkelComp->GetBoneTransform(ParentBoneIndex);

		// Swing drawing
		if (RuntimeNode->Type == EPoseDriverType::SwingOnly)
		{
			FVector LocalVec = RuntimeNode->SourceBoneTM.TransformVectorNoScale(RuntimeNode->GetTwistAxisVector());
			FVector WorldVec = ParentWorldTM.TransformVectorNoScale(LocalVec);
			PDI->DrawLine(BonePos, BonePos + (WorldVec*DrawAxisLength), FLinearColor::Green, SDPG_Foreground, LineWidth);
		}
		// Swing & twist drawing
		else if (RuntimeNode->Type == EPoseDriverType::SwingAndTwist)
		{

		}
		// Translation drawing
		else if (RuntimeNode->Type == EPoseDriverType::Translation)
		{
			FVector LocalPos = RuntimeNode->SourceBoneTM.GetTranslation();
			FVector WorldPos = ParentWorldTM.TransformPosition(LocalPos);
			DrawWireDiamond(PDI, FTranslationMatrix(WorldPos), DrawPosSize, FLinearColor::Green, SDPG_Foreground);
		}

		for (FPoseDriverPoseInfo& PoseInfo : RuntimeNode->PoseInfos)
		{
			// Swing drawing
			if (RuntimeNode->Type == EPoseDriverType::SwingOnly)
			{
				FVector LocalVec = PoseInfo.PoseTM.TransformVectorNoScale(RuntimeNode->GetTwistAxisVector());
				FVector WorldVec = ParentWorldTM.TransformVectorNoScale(LocalVec);
				PDI->DrawLine(BonePos, BonePos + (WorldVec*DrawAxisLength), GetColorFromWeight(PoseInfo.PoseWeight), SDPG_Foreground, LineWidth);
			}
			// Swing & twist drawing
			else if (RuntimeNode->Type == EPoseDriverType::SwingAndTwist)
			{

			}
			// Translation drawing
			else if (RuntimeNode->Type == EPoseDriverType::Translation)
			{
				FVector LocalPos = PoseInfo.PoseTM.GetTranslation();
				FVector WorldPos = ParentWorldTM.TransformPosition(LocalPos);
				DrawWireDiamond(PDI, FTranslationMatrix(WorldPos), DrawPosSize, GetColorFromWeight(PoseInfo.PoseWeight), SDPG_Foreground);
			}
		}
	}
}

#undef LOCTEXT_NAMESPACE

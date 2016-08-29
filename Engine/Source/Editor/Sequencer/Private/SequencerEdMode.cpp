// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "SequencerPrivatePCH.h"
#include "SequencerEdMode.h"
#include "Sequencer.h"
#include "MovieSceneHitProxy.h"
#include "Tracks/MovieScene3DTransformTrack.h"
#include "Sections/MovieScene3DTransformSection.h"

const FEditorModeID FSequencerEdMode::EM_SequencerMode(TEXT("EM_SequencerMode"));

FSequencerEdMode::FSequencerEdMode() :
	Sequencer(nullptr)
{
	FSequencerEdModeTool* SequencerEdModeTool = new FSequencerEdModeTool(this);

	Tools.Add( SequencerEdModeTool );
	SetCurrentTool( SequencerEdModeTool );
}

FSequencerEdMode::~FSequencerEdMode()
{
}

void FSequencerEdMode::Enter()
{
	FEdMode::Enter();
}

void FSequencerEdMode::Exit()
{
	Sequencer = NULL;

	FEdMode::Exit();
}

bool FSequencerEdMode::IsCompatibleWith(FEditorModeID OtherModeID) const
{
	// Compatible with all modes so that we can take over with the sequencer hotkeys
	return true;
}

bool FSequencerEdMode::InputKey( FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event )
{
	if (Sequencer == nullptr)
	{
		return false;
	}

	if (Event != IE_Released)
	{
		FModifierKeysState KeyState = FSlateApplication::Get().GetModifierKeys();

		if (Sequencer->GetCommandBindings(ESequencerCommandBindings::Shared).Get()->ProcessCommandBindings(Key, KeyState, (Event == IE_Repeat) ))
		{
			return true;
		}
	}

	return FEdMode::InputKey(ViewportClient, Viewport, Key, Event);
}

void FSequencerEdMode::Render(const FSceneView* View, FViewport* Viewport, FPrimitiveDrawInterface* PDI)
{
	if (Sequencer == nullptr)
	{
		return;
	}

	FEdMode::Render(View, Viewport, PDI);

#if WITH_EDITORONLY_DATA
	if (View->Family->EngineShowFlags.Splines)
	{
		DrawTracks3D(View, PDI);
	}
#endif
}

void FSequencerEdMode::OnKeySelected(FViewport* Viewport, HMovieSceneKeyProxy* KeyProxy)
{
	bool bCtrlDown = Viewport->KeyState(EKeys::LeftControl) || Viewport->KeyState(EKeys::RightControl);
	bool bAltDown = Viewport->KeyState(EKeys::LeftAlt) || Viewport->KeyState(EKeys::RightAlt);
	bool bShiftDown = Viewport->KeyState(EKeys::LeftShift) || Viewport->KeyState(EKeys::RightShift);

	Sequencer->SetGlobalTimeDirectly(KeyProxy->Time);
	Sequencer->SelectTrackKeys(KeyProxy->MovieSceneSection, KeyProxy->Time, bShiftDown, bCtrlDown);
}

namespace SequencerEdMode_Draw3D
{
static const FColor	KeySelectedColor(255,128,0);
static const float	DrawTrackTimeRes = 0.1f;
static const float	CurveHandleScale = 0.5f;
}

FTransform GetRefFrame(const UObject* InObject)
{
	FTransform RefTM = FTransform::Identity;

	const AActor* Actor = Cast<AActor>(InObject);
	if (Actor != nullptr && Actor->GetRootComponent() != nullptr && Actor->GetRootComponent()->GetAttachParent() != nullptr)
	{
		RefTM = Actor->GetRootComponent()->GetAttachParent()->GetSocketTransform(Actor->GetRootComponent()->GetAttachSocketName());
	}

	return RefTM;
}

void GetLocationAtTime(UMovieScene3DTransformSection* TransformSection, float KeyTime, FVector& KeyPos, FRotator& KeyRot)
{
	TransformSection->EvalTranslation(KeyTime, KeyPos);
	TransformSection->EvalRotation(KeyTime, KeyRot);
}

void DrawTransformTrack(const FSceneView* View, FPrimitiveDrawInterface* PDI, UMovieScene3DTransformTrack* TransformTrack, const TArray<TWeakObjectPtr<UObject>>& BoundObjects, const bool& bIsSelected)
{
	const bool bHitTesting = PDI->IsHitTesting();
	FLinearColor TrackColor = FLinearColor::Yellow; //@todo - customizable per track

	for (int32 SectionIndex = 0; SectionIndex < TransformTrack->GetAllSections().Num(); ++SectionIndex)
	{
		UMovieSceneSection* Section = TransformTrack->GetAllSections()[SectionIndex];
		UMovieScene3DTransformSection* TransformSection = Cast<UMovieScene3DTransformSection>(Section);

		if (TransformSection->GetShow3DTrajectory() == EShow3DTrajectory::EST_Never)
		{
			continue;
		}

		if (TransformSection->GetShow3DTrajectory() == EShow3DTrajectory::EST_OnlyWhenSelected && !bIsSelected)
		{
			continue;
		}

		FRichCurve& TransXCurve = TransformSection->GetTranslationCurve(EAxis::X);
		FRichCurve& TransYCurve = TransformSection->GetTranslationCurve(EAxis::Y);
		FRichCurve& TransZCurve = TransformSection->GetTranslationCurve(EAxis::Z);

		TSet<float> KeyTimes;

		for (auto KeyIt(TransXCurve.GetKeyHandleIterator()); KeyIt; ++KeyIt)
		{
			FKeyHandle KeyHandle = KeyIt.Key();
			float KeyTime = TransXCurve.GetKeyTime(KeyHandle);
			KeyTimes.Add(KeyTime);
		}

		for (auto KeyIt(TransYCurve.GetKeyHandleIterator()); KeyIt; ++KeyIt)
		{
			FKeyHandle KeyHandle = KeyIt.Key();
			float KeyTime = TransYCurve.GetKeyTime(KeyHandle);
			KeyTimes.Add(KeyTime);
		}

		for (auto KeyIt(TransZCurve.GetKeyHandleIterator()); KeyIt; ++KeyIt)
		{
			FKeyHandle KeyHandle = KeyIt.Key();
			float KeyTime = TransZCurve.GetKeyTime(KeyHandle);
			KeyTimes.Add(KeyTime);
		}

		KeyTimes.Sort([](const float& A, const float& B ) { return A < B; });

		FVector OldKeyPos(0);
		float OldKeyTime = 0.f;
		int KeyTimeIndex = 0;

		for (auto NewKeyTime : KeyTimes)
		{
			FVector NewKeyPos(0);
			FRotator NewKeyRot(0,0,0);
			GetLocationAtTime(TransformSection, NewKeyTime, NewKeyPos, NewKeyRot);

			// If not the first keypoint, draw a line to the last keypoint.
			if(KeyTimeIndex > 0)
			{
				int32 NumSteps = FMath::CeilToInt( (NewKeyTime - OldKeyTime)/SequencerEdMode_Draw3D::DrawTrackTimeRes );
								// Limit the number of steps to prevent a rendering performance hit
				NumSteps = FMath::Min( 100, NumSteps );
				float DrawSubstep = (NewKeyTime - OldKeyTime)/NumSteps;

				// Find position on first keyframe.
				float OldTime = OldKeyTime;

				FVector OldPos(0);
				FRotator OldRot(0,0,0);
				GetLocationAtTime(TransformSection, OldKeyTime, OldPos, OldRot);

				FKeyHandle OldXKeyHandle = TransXCurve.FindKey(OldKeyTime);
				FKeyHandle OldYKeyHandle = TransYCurve.FindKey(OldKeyTime);
				FKeyHandle OldZKeyHandle = TransZCurve.FindKey(OldKeyTime);

				bool bIsConstantKey = false;
				if (TransXCurve.IsKeyHandleValid(OldXKeyHandle) && TransXCurve.GetKeyInterpMode(OldXKeyHandle) == ERichCurveInterpMode::RCIM_Constant &&
					TransYCurve.IsKeyHandleValid(OldYKeyHandle) && TransYCurve.GetKeyInterpMode(OldYKeyHandle) == ERichCurveInterpMode::RCIM_Constant &&
					TransZCurve.IsKeyHandleValid(OldZKeyHandle) && TransZCurve.GetKeyInterpMode(OldZKeyHandle) == ERichCurveInterpMode::RCIM_Constant)
				{
					bIsConstantKey = true;
				}

				for (auto BoundObject : BoundObjects)
				{
					FTransform RefTM = GetRefFrame(BoundObject.Get());

					FVector OldPos_G = RefTM.TransformPosition(OldPos);
					FVector NewKeyPos_G = RefTM.TransformPosition(NewKeyPos);

					// For constant interpolation - don't draw ticks - just draw dotted line.
					if (bIsConstantKey)
					{
						DrawDashedLine(PDI, OldPos_G, NewKeyPos_G, TrackColor, 20, SDPG_Foreground);
					}
					else
					{
						// Then draw a line for each substep.
						for (int32 j=1; j<NumSteps+1; j++)
						{
							float NewTime = OldKeyTime + j*DrawSubstep;

							FVector NewPos(0);
							FRotator NewRot(0,0,0);
							GetLocationAtTime(TransformSection, NewTime, NewPos, NewRot);

							FVector NewPos_G = RefTM.TransformPosition(NewPos);

							PDI->DrawLine(OldPos_G, NewPos_G, TrackColor, SDPG_Foreground);

							// Don't draw point for last one - its the keypoint drawn above.
							if (j != NumSteps)
							{
								PDI->DrawPoint(NewPos_G, TrackColor, 3.f, SDPG_Foreground);
							}
							OldTime = NewTime;
							OldPos_G = NewPos_G;
						}
					}
				}
			}
			
			OldKeyTime = NewKeyTime;			
			OldKeyPos = NewKeyPos;
			++KeyTimeIndex;			
		}

		// Draw keypoints on top of curve
		for (auto NewKeyTime : KeyTimes)
		{
			// Find if this key is one of the selected ones.
			bool bKeySelected = false;
			//@todo
			//for(int32 j=0; j<SelectedKeys.Num() && !bKeySelected; j++)
			//{
			//	if( SelectedKeys[j].Group == Group && 
			//		SelectedKeys[j].Track == this && 
			//		SelectedKeys[j].KeyIndex == i )
			//		bKeySelected = true;
			//}

			// Find the time, position and orientation of this Key.

			FVector NewKeyPos(0);
			FRotator NewKeyRot(0,0,0);
			GetLocationAtTime(TransformSection, NewKeyTime, NewKeyPos, NewKeyRot);

			for (auto BoundObject : BoundObjects)
			{
				FTransform RefTM = GetRefFrame(BoundObject.Get());

				FColor KeyColor = bKeySelected ? SequencerEdMode_Draw3D::KeySelectedColor : TrackColor.ToFColor(true);

				if (bHitTesting) 
				{
					PDI->SetHitProxy( new HMovieSceneKeyProxy(TransformTrack, TransformSection, NewKeyTime) );
				}

				FVector NewKeyPos_G = RefTM.TransformPosition(NewKeyPos);

				PDI->DrawPoint(NewKeyPos_G, KeyColor, 6.f, SDPG_Foreground);

				//@todo
				// If desired, draw directional arrow at each keyframe.
				//if(bShowArrowAtKeys)
				//{
				//	FRotationTranslationMatrix ArrowToWorld(NewKeyRot,NewKeyPos);
				//	DrawDirectionalArrow(PDI, FScaleMatrix(FVector(16.f,16.f,16.f)) * ArrowToWorld, KeyColor, 3.f, 1.f, SDPG_Foreground );
				//}
				if (bHitTesting) 
				{
					PDI->SetHitProxy( NULL );
				}

				// If a selected key, draw handles.
				/*
				if (bKeySelected)
				{
					FVector ArriveTangent = PosTrack.Points[i].ArriveTangent;
					FVector LeaveTangent = PosTrack.Points[i].LeaveTangent;

					EInterpCurveMode PrevMode = (i > 0)							? GetKeyInterpMode(i-1) : EInterpCurveMode(255);
					EInterpCurveMode NextMode = (i < PosTrack.Points.Num()-1)	? GetKeyInterpMode(i)	: EInterpCurveMode(255);

					// If not first point, and previous mode was a curve type.
					if(PrevMode == CIM_CurveAuto || PrevMode == CIM_CurveAutoClamped || PrevMode == CIM_CurveUser || PrevMode == CIM_CurveBreak)
					{
						FVector HandlePos = NewKeyPos - RefTM.TransformVector(ArriveTangent * SequencerEdMode_Draw3D::CurveHandleScale);
						PDI->DrawLine(NewKeyPos, HandlePos, FColor(128,255,0), SDPG_Foreground);

						if (bHitTesting) 
						{ 
							//@todo
							//PDI->SetHitProxy( new HInterpTrackKeyHandleProxy(Group, TrackIndex, i, true) );
						}
						PDI->DrawPoint(HandlePos, FColor(128,255,0), 5.f, SDPG_Foreground);
						if (bHitTesting) 
						{
							//@todo
							//PDI->SetHitProxy( NULL );
						}
					}

					// If next section is a curve, draw leaving handle.
					if(NextMode == CIM_CurveAuto || NextMode == CIM_CurveAutoClamped || NextMode == CIM_CurveUser || NextMode == CIM_CurveBreak)
					{
						FVector HandlePos = NewKeyPos + RefTM.TransformVector(LeaveTangent * SequencerEdMode_Draw3D::CurveHandleScale);
						PDI->DrawLine(NewKeyPos, HandlePos, FColor(128,255,0), SDPG_Foreground);

						if (bHitTesting) 
						{
							//@todo
							//PDI->SetHitProxy( new HInterpTrackKeyHandleProxy(Group, TrackIndex, i, false) );
						}
						PDI->DrawPoint(HandlePos, FColor(128,255,0), 5.f, SDPG_Foreground);
						if (bHitTesting) 
						{
							//@todo
							//PDI->SetHitProxy( NULL );
						}
					}
				}
				*/
			}
		}
	}
}


void FSequencerEdMode::DrawTracks3D(const FSceneView* View, FPrimitiveDrawInterface* PDI)
{
	TSet<TSharedRef<FSequencerDisplayNode> > ObjectBindingNodes;

	// Map between object binding nodes and selection
	TMap<TSharedRef<FSequencerDisplayNode>, bool > ObjectBindingNodesSelectionMap;

	for (auto ObjectBinding : Sequencer->GetNodeTree()->GetObjectBindingMap() )
	{
		if (ObjectBinding.Value.IsValid())
		{
			TSharedRef<FSequencerObjectBindingNode> ObjectBindingNode = ObjectBinding.Value.ToSharedRef();

			TSet<TSharedRef<FSequencerDisplayNode> > DescendantNodes;
			SequencerHelpers::GetDescendantNodes(ObjectBindingNode, DescendantNodes);

			bool bSelected = false;
			for (auto DescendantNode : DescendantNodes)
			{
				if (Sequencer->GetSelection().IsSelected(ObjectBindingNode) || 
					Sequencer->GetSelection().IsSelected(DescendantNode) || 
					Sequencer->GetSelection().NodeHasSelectedKeysOrSections(DescendantNode))
				{
					bSelected = true;
					break;
				}
			}

			ObjectBindingNodesSelectionMap.Add(ObjectBindingNode, bSelected);
		}
	}


	// Gather up the transform track nodes from the object binding nodes

	for (auto ObjectBindingNode : ObjectBindingNodesSelectionMap)
	{
		TSet<TSharedRef<FSequencerDisplayNode> > AllNodes;
		SequencerHelpers::GetDescendantNodes(ObjectBindingNode.Key, AllNodes);

		FGuid ObjectBinding = StaticCastSharedRef<FSequencerObjectBindingNode>(ObjectBindingNode.Key)->GetObjectBinding();

		TArray<TWeakObjectPtr<UObject>> BoundObjects;
		Sequencer->GetRuntimeObjects(Sequencer->GetFocusedMovieSceneSequenceInstance(), ObjectBinding, BoundObjects);

		for (auto DisplayNode : AllNodes)
		{
			if (DisplayNode->GetType() == ESequencerNode::Track)
			{
				TSharedRef<FSequencerTrackNode> TrackNode = StaticCastSharedRef<FSequencerTrackNode>(DisplayNode);
				UMovieSceneTrack* TrackNodeTrack = TrackNode->GetTrack();
				UMovieScene3DTransformTrack* TransformTrack = Cast<UMovieScene3DTransformTrack>(TrackNodeTrack);
				if (TransformTrack != nullptr)
				{
					DrawTransformTrack(View, PDI, TransformTrack, BoundObjects, ObjectBindingNode.Value);
				}
			}
		}
	}
}

FSequencerEdModeTool::FSequencerEdModeTool(FSequencerEdMode* InSequencerEdMode) :
	SequencerEdMode(InSequencerEdMode)
{
}

FSequencerEdModeTool::~FSequencerEdModeTool()
{
}

bool FSequencerEdModeTool::InputKey(FEditorViewportClient* ViewportClient, FViewport* Viewport, FKey Key, EInputEvent Event)
{
	if( Key == EKeys::LeftMouseButton )
	{
		if( Event == IE_Pressed)
		{
			int32 HitX = ViewportClient->Viewport->GetMouseX();
			int32 HitY = ViewportClient->Viewport->GetMouseY();
			HHitProxy*HitResult = ViewportClient->Viewport->GetHitProxy(HitX, HitY);

			if(HitResult)
			{
				if( HitResult->IsA(HMovieSceneKeyProxy::StaticGetType()) )
				{
					HMovieSceneKeyProxy* KeyProxy = (HMovieSceneKeyProxy*)HitResult;
					SequencerEdMode->OnKeySelected(ViewportClient->Viewport, KeyProxy);
				}
			}
		}
	}

	return FModeTool::InputKey(ViewportClient, Viewport, Key, Event);
}

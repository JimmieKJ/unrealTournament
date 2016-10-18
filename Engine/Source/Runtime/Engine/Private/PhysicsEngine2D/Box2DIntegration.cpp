// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "EnginePrivate.h"
#include "Box2DIntegration.h"
#include "PhysicsEngine/PhysXSupport.h"
#include "Collision/PhysicsFiltering.h"

#if WITH_BOX2D

//////////////////////////////////////////////////////////////////////////
// 

class FBox2DVisualizer : public b2Draw
{
public:
	// b2Draw interface
	virtual void DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override;
	virtual void DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color) override;
	virtual void DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color) override;
	virtual void DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color) override;
	virtual void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color) override;
	virtual void DrawTransform(const b2Transform& xf) override;
	// End of b2Draw interface

	FBox2DVisualizer()
		: PDI(NULL)
		, View(NULL)
	{
		//@TODO:

		SetFlags(b2Draw::e_shapeBit | b2Draw::e_jointBit | b2Draw::e_centerOfMassBit);
	}

	void SetParameters(FPrimitiveDrawInterface* InPDI, const FSceneView* InView)
	{
		PDI = InPDI;
		View = InView;
	}

public:
	FPrimitiveDrawInterface* PDI;
	const FSceneView* View;

	static FBox2DVisualizer GlobalVisualizer;
};

FBox2DVisualizer FBox2DVisualizer::GlobalVisualizer;

FLinearColor ConvertBoxColor(const b2Color& BoxColor)
{
	return FLinearColor(BoxColor.r, BoxColor.g, BoxColor.b, 1.0f);
}

void FBox2DVisualizer::DrawPolygon(const b2Vec2* Vertices, int32 VertexCount, const b2Color& BoxColor)
{
	// Draw a closed polygon provided in CCW order.
	const FLinearColor Color = ConvertBoxColor(BoxColor);
	
	for (int32 Index = 0; Index < VertexCount; ++Index)
	{
		const int32 NextIndex = (Index + 1) % VertexCount;

		const FVector Start(FPhysicsIntegration2D::ConvertBoxVectorToUnreal(Vertices[Index]));
		const FVector End(FPhysicsIntegration2D::ConvertBoxVectorToUnreal(Vertices[NextIndex]));

		PDI->DrawLine(Start, End, Color, 0);
	}
}

void FBox2DVisualizer::DrawSolidPolygon(const b2Vec2* Vertices, int32 VertexCount, const b2Color& BoxColor)
{
	//@TODO: Draw a solid closed polygon provided in CCW order.
	DrawPolygon(Vertices, VertexCount, BoxColor);
}

void FBox2DVisualizer::DrawCircle(const b2Vec2& Center, float32 Radius, const b2Color& BoxColor)
{
	// Draw a circle.
	const FLinearColor Color = ConvertBoxColor(BoxColor);
	PDI->DrawPoint(FPhysicsIntegration2D::ConvertBoxVectorToUnreal(Center), Color, Radius, 0);
}

void FBox2DVisualizer::DrawSolidCircle(const b2Vec2& Center, float32 Radius, const b2Vec2& Axis, const b2Color& BoxColor)
{
//	const FLinearColor Color = ConvertBoxColor(BoxColor);
	/// Draw a solid circle.
}

void FBox2DVisualizer::DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& BoxColor)
{
	//const FLinearColor Color = ConvertBoxColor(BoxColor);
	/// Draw a line segment.
}

void FBox2DVisualizer::DrawTransform(const b2Transform& xf)
{
	/// Draw a transform. Choose your own length scale.
	/// @param xf a transform.
}

//////////////////////////////////////////////////////////////////////////
// 

FPhysicsScene2D::FPhysicsScene2D(UWorld* AssociatedWorld)
	: UnrealWorld(AssociatedWorld)
{
	const FVector DefaultGravityUnreal(0.0f, 0.0f, AssociatedWorld->GetDefaultGravityZ());
	const b2Vec2 DefaultGravityBox = FPhysicsIntegration2D::ConvertUnrealVectorToBox(DefaultGravityUnreal);

	World = new b2World(DefaultGravityBox);
	World->SetDebugDraw(&FBox2DVisualizer::GlobalVisualizer);

	World->SetContactFilter(this);

	StartPhysicsTickFunction.bCanEverTick = true;
	StartPhysicsTickFunction.Target = this;
	StartPhysicsTickFunction.TickGroup = TG_StartPhysics;
	StartPhysicsTickFunction.RegisterTickFunction(AssociatedWorld->PersistentLevel);

	EndPhysicsTickFunction.bCanEverTick = true;
	EndPhysicsTickFunction.Target = this;
	EndPhysicsTickFunction.TickGroup = TG_EndPhysics;
	EndPhysicsTickFunction.RegisterTickFunction(AssociatedWorld->PersistentLevel);
	EndPhysicsTickFunction.AddPrerequisite(UnrealWorld, StartPhysicsTickFunction);
}

FPhysicsScene2D::~FPhysicsScene2D()
{
	StartPhysicsTickFunction.UnRegisterTickFunction();
	EndPhysicsTickFunction.UnRegisterTickFunction();

	delete World;
}

bool FPhysicsScene2D::ShouldCollide(b2Fixture* FixtureA, b2Fixture* FixtureB)
{
	//@TODO: BOX2D: May need to extend Box2D to be able to indicate that we don't want them to collide now but keep tracking the interaction, not sure!
	const bool KillInteraction = false;
	const bool SuppressInteraction = false;
	const bool NormalInteraction = true;

	const b2Filter& FilterDataA = FixtureA->GetFilterData();
	const b2Filter& FilterDataB = FixtureB->GetFilterData();

	const ECollisionChannel ChannelA = GetCollisionChannel(FilterDataA.ObjectTypeAndFlags);
	const ECollisionChannel ChannelB = GetCollisionChannel(FilterDataB.ObjectTypeAndFlags);
	const b2BodyType BodyTypeA = FixtureA->GetBody()->GetType();
	const b2BodyType BodyTypeB = FixtureB->GetBody()->GetType();

	// ignore kinematic-kinematic interactions which don't involve a destructible
	const bool bIsKinematicA = BodyTypeA == b2_kinematicBody;
	const bool bIsKinematicB = BodyTypeB == b2_kinematicBody;
	if (bIsKinematicA && bIsKinematicB && (ChannelA != ECC_Destructible) && (ChannelB != ECC_Destructible))
	{
		return KillInteraction;
	}

	// ignore static-kinematic interactions
	const bool bIsStaticA = BodyTypeA == b2_staticBody;
	const bool bIsStaticB = BodyTypeB == b2_staticBody;
	if ((bIsKinematicA || bIsKinematicB) && (bIsStaticA || bIsStaticB))
	{
		// should return eSUPPRESS here instead eKILL so that kinematics vs statics will still be considered once kinematics become dynamic (dying ragdoll case)
		return SuppressInteraction;
	}

	//@TODO: BOX2D: Skeletal collision filtering
#if 0
	// if these bodies are from the same skeletal mesh component, use the disable table to see if we should disable collision
	if ((FilterDataA.UniqueComponentID != 0) && (FilterDataA.UniqueComponentID == FilterDataB.UniqueComponentID))
	{
		FPhysScene* PhysScene = *magic*;
		check(PhysScene);

		const TMap<uint32, TMap<FRigidBodyIndexPair, bool> *> & CollisionDisableTableLookup = PhysScene->GetCollisionDisableTableLookup();
		TMap<FRigidBodyIndexPair, bool>* const * DisableTablePtrPtr = CollisionDisableTableLookup.FindChecked(FilterDataA.UniqueComponentID);
		TMap<FRigidBodyIndexPair, bool>* DisableTablePtr = *DisableTablePtrPtr;
		FRigidBodyIndexPair BodyPair(FilterDataA.BodyIndex, FilterDataB.BodyIndex);
		if (DisableTablePtr->Find(BodyPair))
		{
			return KillInteraction;
		}
	}
#endif

	// See if either one would like to block the other
	const uint32 BlockFlagToB = ECC_TO_BITFIELD(ChannelB) & FilterDataA.BlockingChannels;
	const uint32 BlockFlagToA = ECC_TO_BITFIELD(ChannelA) & FilterDataB.BlockingChannels;
	const bool bDoesWantToBlock = BlockFlagToB && BlockFlagToA;
	if (!bDoesWantToBlock)
	{
		return SuppressInteraction;
	}

	//@TODO: BOX2D: CCD / contact notification flags
#if 0
	const uint32 FilterFlags0 = FilterDataA.ObjectTypeAndFlags & 0xFFFFFF;
	const uint32 FilterFlags1 = FilterDataB.ObjectTypeAndFlags & 0xFFFFFF;

	bool bResult = NormalInteraction;

	//todo enabling CCD objects against everything else for now
	if (!(bIsKinematicA && bIsKinematicB) && ((FilterFlagsA & EPDF_CCD) || (FilterFlagsB & EPDF_CCD)))
	{
		pairFlags |= PxPairFlag::eCCD_LINEAR;
	}

	if ((FilterFlagsA & EPDF_ContactNotify) || (FilterFlagsB & EPDF_ContactNotify))
	{
		pairFlags |= (PxPairFlag::eNOTIFY_TOUCH_FOUND | PxPairFlag::eNOTIFY_TOUCH_PERSISTS | PxPairFlag::eNOTIFY_CONTACT_POINTS);
	}
#endif

	return NormalInteraction;
}

//////////////////////////////////////////////////////////////////////////
// FStartPhysics2DTickFunction

void FStartPhysics2DTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	QUICK_SCOPE_CYCLE_COUNTER(FStartPhysics2DTickFunction_ExecuteTick);

	if (Target->UnrealWorld->bShouldSimulatePhysics)
	{
		const int32 VelocityIterations = 8;
		const int32 PositionIterations = 3;
		
		// Advance the world
		b2World* PhysicsWorld = Target->World;
		PhysicsWorld->Step(DeltaTime, VelocityIterations, PositionIterations);
		PhysicsWorld->ClearForces();

		// Pull data back
		for (b2Body* Body = PhysicsWorld->GetBodyList(); Body; )
		{
			b2Body* NextBody = Body->GetNext();

			if (FBodyInstance* UnrealBodyInstance = static_cast<FBodyInstance*>(Body->GetUserData()))
			{
				if (UnrealBodyInstance->bSimulatePhysics)
				{
					// See if the transform is actually different, and if so, move the component to match physics
					const FTransform NewTransform = UnrealBodyInstance->GetUnrealWorldTransform();
					const FTransform& OldTransform = UnrealBodyInstance->OwnerComponent->ComponentToWorld;
					if (!NewTransform.Equals(OldTransform))
					{
						const FVector MoveBy = NewTransform.GetLocation() - OldTransform.GetLocation();
						const FRotator NewRotation = NewTransform.Rotator();

						UnrealBodyInstance->OwnerComponent->MoveComponent(MoveBy, NewRotation, /*bSweep=*/ false, /*OutHit=*/ nullptr, MOVECOMP_SkipPhysicsMove);
					}
				}
			}

			Body = NextBody;
		}
	}
}

FString FStartPhysics2DTickFunction::DiagnosticMessage()
{
	return TEXT("FStartPhysics2DTickFunction");
}

//////////////////////////////////////////////////////////////////////////
// FEndPhysics2DTickFunction

void FEndPhysics2DTickFunction::ExecuteTick(float DeltaTime, enum ELevelTick TickType, ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
{
	QUICK_SCOPE_CYCLE_COUNTER(FEndPhysics2DTickFunction_ExecuteTick);
}

FString FEndPhysics2DTickFunction::DiagnosticMessage()
{
	return TEXT("FEndPhysics2DTickFunction");
}


//////////////////////////////////////////////////////////////////////////
// FPhysicsIntegration2D

TMap< UWorld*, TSharedPtr<FPhysicsScene2D> > FPhysicsIntegration2D::WorldMappings;
FWorldDelegates::FWorldInitializationEvent::FDelegate FPhysicsIntegration2D::OnWorldCreatedDelegate;
FWorldDelegates::FWorldEvent::FDelegate FPhysicsIntegration2D::OnWorldDestroyedDelegate;
FDelegateHandle FPhysicsIntegration2D::OnWorldCreatedDelegateHandle;
FDelegateHandle FPhysicsIntegration2D::OnWorldDestroyedDelegateHandle;

void FPhysicsIntegration2D::OnWorldCreated(UWorld* UnrealWorld, const UWorld::InitializationValues IVS)
{
	if (IVS.bCreatePhysicsScene)
	{
		WorldMappings.Add(UnrealWorld, MakeShareable(new FPhysicsScene2D(UnrealWorld)));
	}
}

void FPhysicsIntegration2D::OnWorldDestroyed(UWorld* UnrealWorld)
{
	WorldMappings.Remove(UnrealWorld);
}

void FPhysicsIntegration2D::InitializePhysics()
{
	OnWorldCreatedDelegate   = FWorldDelegates::FWorldInitializationEvent::FDelegate::CreateStatic(&FPhysicsIntegration2D::OnWorldCreated);
	OnWorldDestroyedDelegate = FWorldDelegates::FWorldEvent::FDelegate::CreateStatic(&FPhysicsIntegration2D::OnWorldDestroyed);

	OnWorldCreatedDelegateHandle   = FWorldDelegates::OnPreWorldInitialization.Add(OnWorldCreatedDelegate);
	OnWorldDestroyedDelegateHandle = FWorldDelegates::OnPreWorldFinishDestroy .Add(OnWorldDestroyedDelegate);
}

void FPhysicsIntegration2D::ShutdownPhysics()
{
	FWorldDelegates::OnPreWorldInitialization.Remove(OnWorldCreatedDelegateHandle);
	FWorldDelegates::OnPreWorldFinishDestroy .Remove(OnWorldDestroyedDelegateHandle);

	//ensureMsgf(WorldMappings.Num() == 0, TEXT("You have some worlds hanging around. Is this a hard shutdown or did the world destroy delegate not fire?"));
}

TSharedPtr<FPhysicsScene2D> FPhysicsIntegration2D::FindAssociatedScene(UWorld* Source)
{
	return WorldMappings.FindRef(Source);
}

b2World* FPhysicsIntegration2D::FindAssociatedWorld(UWorld* Source)
{
	if (FPhysicsScene2D* Scene = FindAssociatedScene(Source).Get())
	{
		return Scene->World;
	}
	else
	{
		return NULL;
	}
}

void FPhysicsIntegration2D::DrawDebugPhysics(UWorld* World, FPrimitiveDrawInterface* PDI, const FSceneView* View)
{
	if (b2World* World2D = FindAssociatedWorld(World))
	{
		//@TODO: HORRIBLE CODE
		FBox2DVisualizer::GlobalVisualizer.SetParameters(PDI, View);
		World2D->DrawDebugData();
		FBox2DVisualizer::GlobalVisualizer.SetParameters(NULL, NULL);
	}
}

#else

void FPhysicsIntegration2D::InitializePhysics()
{
}

void FPhysicsIntegration2D::ShutdownPhysics()
{
}

void FPhysicsIntegration2D::DrawDebugPhysics(UWorld* World, FPrimitiveDrawInterface* PDI, const FSceneView* View)
{
}
#endif
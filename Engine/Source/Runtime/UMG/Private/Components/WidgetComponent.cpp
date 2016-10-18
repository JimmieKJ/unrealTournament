// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "WidgetComponent.h"
#include "HittestGrid.h"
#if !UE_SERVER
	#include "ISlateRHIRendererModule.h"
	#include "ISlate3DRenderer.h"
#endif // !UE_SERVER
#include "DynamicMeshBuilder.h"
#include "Scalability.h"
#include "WidgetLayoutLibrary.h"
#include "PhysicsEngine/BodySetup.h"
#include "SGameLayerManager.h"
#include "Slate/WidgetRenderer.h"
#include "Slate/SWorldWidgetScreenLayer.h"
#include "Widgets/LayerManager/STooltipPresenter.h"
#include "Widgets/Layout/SPopup.h"

DECLARE_CYCLE_STAT(TEXT("3DHitTesting"), STAT_Slate3DHitTesting, STATGROUP_Slate);

class FWorldWidgetScreenLayer : public IGameLayer
{
public:
	FWorldWidgetScreenLayer(const FLocalPlayerContext& PlayerContext)
	{
		OwningPlayer = PlayerContext;
	}

	virtual ~FWorldWidgetScreenLayer()
	{
		// empty virtual destructor to help clang warning
	}

	void AddComponent(UWidgetComponent* Component)
	{
		if ( Component )
		{
			Components.AddUnique(Component);
			if ( ScreenLayer.IsValid() )
			{
				if ( UUserWidget* UserWidget = Component->GetUserWidgetObject() )
				{
					ScreenLayer.Pin()->AddComponent(Component, UserWidget->TakeWidget());
				}
			}
		}
	}

	void RemoveComponent(UWidgetComponent* Component)
	{
		if ( Component )
		{
			Components.RemoveSwap(Component);

			if ( ScreenLayer.IsValid() )
			{
				ScreenLayer.Pin()->RemoveComponent(Component);
			}
		}
	}

	virtual TSharedRef<SWidget> AsWidget() override
	{
		if ( ScreenLayer.IsValid() )
		{
			return ScreenLayer.Pin().ToSharedRef();
		}

		TSharedRef<SWorldWidgetScreenLayer> NewScreenLayer = SNew(SWorldWidgetScreenLayer, OwningPlayer);
		ScreenLayer = NewScreenLayer;

		// Add all the pending user widgets to the surface
		for ( TWeakObjectPtr<UWidgetComponent>& WeakComponent : Components )
		{
			if ( UWidgetComponent* Component = WeakComponent.Get() )
			{
				if ( UUserWidget* UserWidget = Component->GetUserWidgetObject() )
				{
					NewScreenLayer->AddComponent(Component, UserWidget->TakeWidget());
				}
			}
		}

		return NewScreenLayer;
	}

private:
	FLocalPlayerContext OwningPlayer;
	TWeakPtr<SWorldWidgetScreenLayer> ScreenLayer;
	TArray<TWeakObjectPtr<UWidgetComponent>> Components;
};


/** Represents a billboard sprite to the scene manager. */
class FWidget3DSceneProxy : public FPrimitiveSceneProxy
{
public:
	/** Initialization constructor. */
	FWidget3DSceneProxy( UWidgetComponent* InComponent, ISlate3DRenderer& InRenderer )
		: FPrimitiveSceneProxy( InComponent )
		, Pivot( InComponent->GetPivot() )
		, Renderer( InRenderer )
		, RenderTarget( InComponent->GetRenderTarget() )
		, MaterialInstance( InComponent->GetMaterialInstance() )
		, BodySetup( InComponent->GetBodySetup() )
		, BlendMode( InComponent->GetBlendMode() )
		, bUseLegacyRotation( InComponent->IsUsingLegacyRotation() )
	{
		bWillEverBeLit = false;

		MaterialRelevance = MaterialInstance->GetRelevance(GetScene().GetFeatureLevel());
	}

	// FPrimitiveSceneProxy interface.
	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override
	{
#if WITH_EDITOR
		const bool bWireframe = AllowDebugViewmodes() && ViewFamily.EngineShowFlags.Wireframe;

		auto WireframeMaterialInstance = new FColoredMaterialRenderProxy(
			GEngine->WireframeMaterial ? GEngine->WireframeMaterial->GetRenderProxy(IsSelected()) : nullptr,
			FLinearColor(0, 0.5f, 1.f)
			);

		Collector.RegisterOneFrameMaterialProxy(WireframeMaterialInstance);

		FMaterialRenderProxy* MaterialProxy = nullptr;
		if ( bWireframe )
		{
			MaterialProxy = WireframeMaterialInstance;
		}
		else
		{
			MaterialProxy = MaterialInstance->GetRenderProxy(IsSelected());
		}
#else
		FMaterialRenderProxy* MaterialProxy = MaterialInstance->GetRenderProxy(IsSelected());
#endif

		const FMatrix& ViewportLocalToWorld = GetLocalToWorld();

		if( RenderTarget )
		{
			FTextureResource* TextureResource = RenderTarget->Resource;
			if ( TextureResource )
			{
				float U = -RenderTarget->SizeX * Pivot.X;
				float V = -RenderTarget->SizeY * Pivot.Y;
				float UL = RenderTarget->SizeX * ( 1.0f - Pivot.X );
				float VL = RenderTarget->SizeY * ( 1.0f - Pivot.Y );

				int32 VertexIndices[4];

				for ( int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++ )
				{
					FDynamicMeshBuilder MeshBuilder;

					if ( VisibilityMap & ( 1 << ViewIndex ) )
					{
						if( bUseLegacyRotation )
						{
							VertexIndices[0] = MeshBuilder.AddVertex(FVector(U, V, 0), FVector2D(0, 0), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);
							VertexIndices[1] = MeshBuilder.AddVertex(FVector(U, VL, 0), FVector2D(0, 1), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);
							VertexIndices[2] = MeshBuilder.AddVertex(FVector(UL, VL, 0), FVector2D(1, 1), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);
							VertexIndices[3] = MeshBuilder.AddVertex(FVector(UL, V, 0), FVector2D(1, 0), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);
						}
						else
						{
							VertexIndices[0] = MeshBuilder.AddVertex(-FVector(0, U, V), FVector2D(0, 0), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);
							VertexIndices[1] = MeshBuilder.AddVertex(-FVector(0, U, VL), FVector2D(0, 1), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);
							VertexIndices[2] = MeshBuilder.AddVertex(-FVector(0, UL, VL), FVector2D(1, 1), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);
							VertexIndices[3] = MeshBuilder.AddVertex(-FVector(0, UL, V), FVector2D(1, 0), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);
						}

						MeshBuilder.AddTriangle(VertexIndices[0], VertexIndices[1], VertexIndices[2]);
						MeshBuilder.AddTriangle(VertexIndices[0], VertexIndices[2], VertexIndices[3]);

						MeshBuilder.GetMesh(ViewportLocalToWorld, MaterialProxy, SDPG_World, false, true, ViewIndex, Collector);
					}
				}
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		for ( int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++ )
		{
			if ( VisibilityMap & ( 1 << ViewIndex ) )
			{
				RenderCollision(BodySetup, Collector, ViewIndex, ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
				RenderBounds(Collector.GetPDI(ViewIndex), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
			}
		}
#endif
	}

	void RenderCollision(UBodySetup* InBodySetup, FMeshElementCollector& Collector, int32 ViewIndex, const FEngineShowFlags& EngineShowFlags, const FBoxSphereBounds& InBounds, bool bRenderInEditor) const
	{
		if ( InBodySetup )
		{
			bool bDrawCollision = EngineShowFlags.Collision && IsCollisionEnabled();

			if ( bDrawCollision && AllowDebugViewmodes() )
			{
				// Draw simple collision as wireframe if 'show collision', collision is enabled, and we are not using the complex as the simple
				const bool bDrawSimpleWireframeCollision = InBodySetup->CollisionTraceFlag != ECollisionTraceFlag::CTF_UseComplexAsSimple;

				if ( FMath::Abs(GetLocalToWorld().Determinant()) < SMALL_NUMBER )
				{
					// Catch this here or otherwise GeomTransform below will assert
					// This spams so commented out
					//UE_LOG(LogStaticMesh, Log, TEXT("Zero scaling not supported (%s)"), *StaticMesh->GetPathName());
				}
				else
				{
					const bool bDrawSolid = !bDrawSimpleWireframeCollision;
					const bool bProxyIsSelected = IsSelected();

					if ( bDrawSolid )
					{
						// Make a material for drawing solid collision stuff
						auto SolidMaterialInstance = new FColoredMaterialRenderProxy(
							GEngine->ShadedLevelColorationUnlitMaterial->GetRenderProxy(IsSelected(), IsHovered()),
							WireframeColor
							);

						Collector.RegisterOneFrameMaterialProxy(SolidMaterialInstance);

						FTransform GeomTransform(GetLocalToWorld());
						InBodySetup->AggGeom.GetAggGeom(GeomTransform, WireframeColor.ToFColor(true), SolidMaterialInstance, false, true, UseEditorDepthTest(), ViewIndex, Collector);
					}
					// wireframe
					else
					{
						FColor CollisionColor = FColor(157, 149, 223, 255);
						FTransform GeomTransform(GetLocalToWorld());
						InBodySetup->AggGeom.GetAggGeom(GeomTransform, GetSelectionColor(CollisionColor, bProxyIsSelected, IsHovered()).ToFColor(true), nullptr, false, false, UseEditorDepthTest(), ViewIndex, Collector);
					}
				}
			}
		}
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		bool bVisible = true;

		FPrimitiveViewRelevance Result;

		MaterialRelevance.SetPrimitiveViewRelevance(Result);

		Result.bDrawRelevance = IsShown(View) && bVisible;
		Result.bDynamicRelevance = true;
		Result.bShadowRelevance = IsShadowCast(View);
		Result.bEditorPrimitiveRelevance = false;

		return Result;
	}

	virtual void GetLightRelevance(const FLightSceneProxy* LightSceneProxy, bool& bDynamic, bool& bRelevant, bool& bLightMapped, bool& bShadowMapped) const override
	{
		bDynamic = false;
		bRelevant = false;
		bLightMapped = false;
		bShadowMapped = false;
	}

	virtual void OnTransformChanged() override
	{
		Origin = GetLocalToWorld().GetOrigin();
	}

	virtual uint32 GetMemoryFootprint(void) const override { return(sizeof(*this) + GetAllocatedSize()); }

	uint32 GetAllocatedSize(void) const { return( FPrimitiveSceneProxy::GetAllocatedSize() ); }

private:
	FVector Origin;
	FVector2D Pivot;
	ISlate3DRenderer& Renderer;
	UTextureRenderTarget2D* RenderTarget;
	UMaterialInstanceDynamic* MaterialInstance;
	FMaterialRelevance MaterialRelevance;
	UBodySetup* BodySetup;
	EWidgetBlendMode BlendMode;
	bool bUseLegacyRotation;
};






UWidgetComponent::UWidgetComponent( const FObjectInitializer& PCIP )
	: Super( PCIP )
	, DrawSize( FIntPoint( 500, 500 ) )
	, bManuallyRedraw(false)
	, bRedrawRequested(true)
	, RedrawTime(0)
	, LastWidgetRenderTime(0)
	, bWindowFocusable(true)
	, BackgroundColor( FLinearColor::Transparent )
	, TintColorAndOpacity( FLinearColor::White )
	, OpacityFromTexture( 1.0f )
	, BlendMode( EWidgetBlendMode::Masked )
	, bIsOpaque_DEPRECATED( false )
	, bIsTwoSided( false )
	, ParabolaDistortion( 0 )
	, TickWhenOffscreen( false )
	, SharedLayerName(TEXT("WidgetComponentScreenLayer"))
	, LayerZOrder(-100)
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;

	RelativeRotation = FRotator::ZeroRotator;

	BodyInstance.SetCollisionProfileName(FName(TEXT("UI")));

	// Translucent material instances
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TranslucentMaterial_Finder( TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent") );
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TranslucentMaterial_OneSided_Finder(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent_OneSided"));
	TranslucentMaterial = TranslucentMaterial_Finder.Object;
	TranslucentMaterial_OneSided = TranslucentMaterial_OneSided_Finder.Object;

	// Opaque material instances
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OpaqueMaterial_Finder( TEXT( "/Engine/EngineMaterials/Widget3DPassThrough_Opaque" ) );
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OpaqueMaterial_OneSided_Finder(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Opaque_OneSided"));
	OpaqueMaterial = OpaqueMaterial_Finder.Object;
	OpaqueMaterial_OneSided = OpaqueMaterial_OneSided_Finder.Object;

	// Masked material instances
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaskedMaterial_Finder(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Masked"));
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> MaskedMaterial_OneSided_Finder(TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Masked_OneSided"));
	MaskedMaterial = MaskedMaterial_Finder.Object;
	MaskedMaterial_OneSided = MaskedMaterial_OneSided_Finder.Object;

	LastLocalHitLocation = FVector2D::ZeroVector;
	//bGenerateOverlapEvents = false;
	bUseEditorCompositing = false;

	bUseLegacyRotation = false;

	Space = EWidgetSpace::World;
	Pivot = FVector2D(0.5, 0.5);

	bAddedToScreen = false;

	// We want this because we want EndPlay to be called!
	bWantsBeginPlay = true;
}

void UWidgetComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	ReleaseResources();
	Super::EndPlay(EndPlayReason);
}

FPrimitiveSceneProxy* UWidgetComponent::CreateSceneProxy()
{
	// Always clear the material instance in case we're going from 3D to 2D.
	if ( MaterialInstance )
	{
		MaterialInstance = nullptr;
	}

	if ( Space != EWidgetSpace::Screen && WidgetRenderer.IsValid() )
	{
		// Create a new MID for the current base material
		{
			UMaterialInterface* BaseMaterial = GetMaterial(0);

			MaterialInstance = UMaterialInstanceDynamic::Create(BaseMaterial, this);

			UpdateMaterialInstanceParameters();
		}

		RequestRedraw();
		LastWidgetRenderTime = 0;

		return new FWidget3DSceneProxy(this, *WidgetRenderer->GetSlateRenderer());
	}
	
	return nullptr;
}

FBoxSphereBounds UWidgetComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	if ( Space != EWidgetSpace::Screen )
	{
		if( bUseLegacyRotation )
		{
			const FVector Origin = FVector(
			( DrawSize.X * 0.5f ) - ( DrawSize.X * Pivot.X ),
			( DrawSize.Y * 0.5f ) - ( DrawSize.Y * Pivot.Y ), .5f);
			const FVector BoxExtent = FVector(DrawSize.X / 2.0f, DrawSize.Y / 2.0f, 1.0f);

			return FBoxSphereBounds(Origin, BoxExtent, DrawSize.Size() / 2.0f).TransformBy(LocalToWorld);
		}
		else
		{
			const FVector Origin = FVector(.5f,
			-( DrawSize.X * 0.5f ) + ( DrawSize.X * Pivot.X ),
			-( DrawSize.Y * 0.5f ) + ( DrawSize.Y * Pivot.Y ));

			const FVector BoxExtent = FVector(1.f, DrawSize.X / 2.0f, DrawSize.Y / 2.0f);

			return FBoxSphereBounds(Origin, BoxExtent, DrawSize.Size() / 2.0f).TransformBy(LocalToWorld);
		}
	}
	else
	{
		return FBoxSphereBounds(ForceInit).TransformBy(LocalToWorld);
	}
}

UBodySetup* UWidgetComponent::GetBodySetup() 
{
	UpdateBodySetup();
	return BodySetup;
}

FCollisionShape UWidgetComponent::GetCollisionShape(float Inflation) const
{
	if ( Space != EWidgetSpace::Screen )
	{
		FVector BoxHalfExtent;

		if( bUseLegacyRotation )
		{
			BoxHalfExtent = ( FVector(DrawSize.X * 0.5f, DrawSize.Y * 0.5f, 0.01f) * ComponentToWorld.GetScale3D() ) + Inflation;
		}
		else
		{
			BoxHalfExtent = ( FVector(0.01f, DrawSize.X * 0.5f, DrawSize.Y * 0.5f) * ComponentToWorld.GetScale3D() ) + Inflation;
		}

		if ( Inflation < 0.0f )
		{
			// Don't shrink below zero size.
			BoxHalfExtent = BoxHalfExtent.ComponentMax(FVector::ZeroVector);
		}

		return FCollisionShape::MakeBox(BoxHalfExtent);
	}
	else
	{
		return FCollisionShape::MakeBox(FVector::ZeroVector);
	}
}

void UWidgetComponent::OnRegister()
{
	Super::OnRegister();

#if !UE_SERVER
	if ( !IsRunningDedicatedServer() )
	{
		if ( Space != EWidgetSpace::Screen )
		{
			if ( !WidgetRenderer.IsValid() && !GUsingNullRHI )
			{
				WidgetRenderer = MakeShareable(new FWidgetRenderer());
			}
		}

		BodySetup = nullptr;

		InitWidget();
	}
#endif // !UE_SERVER
}

void UWidgetComponent::OnUnregister()
{
#if WITH_EDITOR
	if (!GetWorld()->IsGameWorld())
	{
		ReleaseResources();
	}
#endif

	Super::OnUnregister();
}

void UWidgetComponent::DestroyComponent(bool bPromoteChildren/*= false*/)
{
	Super::DestroyComponent(bPromoteChildren);

	ReleaseResources();
}

void UWidgetComponent::ReleaseResources()
{
	if ( Widget )
	{
		RemoveWidgetFromScreen();
		Widget = nullptr;
	}

	WidgetRenderer.Reset();
	HitTestGrid.Reset();

	UnregisterWindow();
}

void UWidgetComponent::RegisterWindow()
{
	if ( SlateWindow.IsValid() )
	{
		if ( FSlateApplication::IsInitialized() )
		{
			FSlateApplication::Get().RegisterVirtualWindow(SlateWindow.ToSharedRef());
		}
	}
}

void UWidgetComponent::UnregisterWindow()
{
	if ( SlateWindow.IsValid() )
	{
		if ( FSlateApplication::IsInitialized() )
		{
			FSlateApplication::Get().UnregisterVirtualWindow(SlateWindow.ToSharedRef());
		}

		SlateWindow.Reset();
	}
}

void UWidgetComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		UpdateWidget();

		if ( Widget == nullptr && !SlateWidget.IsValid() )
		{
			return;
		}

	    if ( Space != EWidgetSpace::Screen )
	    {
			if ( ShouldDrawWidget() )
		    {
			    DrawWidgetToRenderTarget(DeltaTime);
		    }
	    }
	    else
	    {
			if ( ( Widget && !Widget->IsDesignTime() ) || SlateWidget.IsValid() )
		    {
				UWorld* ThisWorld = GetWorld();

				ULocalPlayer* TargetPlayer = GetOwnerPlayer();
				APlayerController* PlayerController = TargetPlayer ? TargetPlayer->PlayerController : nullptr;

				if ( TargetPlayer && PlayerController && IsVisible() )
				{
					if ( !bAddedToScreen )
					{
						if ( ThisWorld->IsGameWorld() )
						{
							if ( UGameViewportClient* ViewportClient = ThisWorld->GetGameViewport() )
							{
								TSharedPtr<IGameLayerManager> LayerManager = ViewportClient->GetGameLayerManager();
								if ( LayerManager.IsValid() )
								{
									TSharedPtr<FWorldWidgetScreenLayer> ScreenLayer;

									FLocalPlayerContext PlayerContext(TargetPlayer, ThisWorld);

									TSharedPtr<IGameLayer> Layer = LayerManager->FindLayerForPlayer(TargetPlayer, SharedLayerName);
									if ( !Layer.IsValid() )
									{
										TSharedRef<FWorldWidgetScreenLayer> NewScreenLayer = MakeShareable(new FWorldWidgetScreenLayer(PlayerContext));
										LayerManager->AddLayerForPlayer(TargetPlayer, SharedLayerName, NewScreenLayer, LayerZOrder);
										ScreenLayer = NewScreenLayer;
									}
									else
									{
										ScreenLayer = StaticCastSharedPtr<FWorldWidgetScreenLayer>(Layer);
									}
								
									bAddedToScreen = true;
								
									Widget->SetPlayerContext(PlayerContext);
									ScreenLayer->AddComponent(this);
								}
							}
						}
					}
				}
				else if ( bAddedToScreen )
				{
					RemoveWidgetFromScreen();
				}
			}
		}
	}
#endif // !UE_SERVER
}

bool UWidgetComponent::ShouldDrawWidget() const
{
	const float RenderTimeThreshold = .5f;
	if ( IsVisible() )
	{
		// If we don't tick when off-screen, don't bother ticking if it hasn't been rendered recently
		if ( TickWhenOffscreen || GetWorld()->TimeSince(LastRenderTime) <= RenderTimeThreshold )
		{
			if ( GetWorld()->TimeSince(LastWidgetRenderTime) >= RedrawTime )
			{
				return bManuallyRedraw ? bRedrawRequested : true;
			}
		}
	}

	return false;
}

void UWidgetComponent::DrawWidgetToRenderTarget(float DeltaTime)
{
	if ( GUsingNullRHI )
	{
		return;
	}

	if ( !SlateWindow.IsValid() )
	{
		return;
	}

	if ( DrawSize.X == 0 || DrawSize.Y == 0 )
	{
		return;
	}

	CurrentDrawSize = DrawSize;

	const float DrawScale = 1.0f;

	if ( bDrawAtDesiredSize )
	{
		SlateWindow->SlatePrepass(DrawScale);

		FVector2D DesiredSize = SlateWindow->GetDesiredSize();
		DesiredSize.X = FMath::RoundToInt(DesiredSize.X);
		DesiredSize.Y = FMath::RoundToInt(DesiredSize.Y);
		CurrentDrawSize = DesiredSize.IntPoint();

		WidgetRenderer->SetIsPrepassNeeded(false);
	}
	else
	{
		WidgetRenderer->SetIsPrepassNeeded(true);
	}

	if ( CurrentDrawSize != DrawSize )
	{
		DrawSize = CurrentDrawSize;
		UpdateBodySetup(true);
		RecreatePhysicsState();
	}

	UpdateRenderTarget(CurrentDrawSize);

	bRedrawRequested = false;

	WidgetRenderer->DrawWindow(
		RenderTarget,
		HitTestGrid.ToSharedRef(),
		SlateWindow.ToSharedRef(),
		DrawScale,
		CurrentDrawSize,
		DeltaTime);

	LastWidgetRenderTime = GetWorld()->TimeSeconds;
}

void UWidgetComponent::RemoveWidgetFromScreen()
{
#if !UE_SERVER
	if (!IsRunningDedicatedServer())
	{
		bAddedToScreen = false;

		if ( UGameViewportClient* ViewportClient = GetWorld()->GetGameViewport() )
		{
			TSharedPtr<IGameLayerManager> LayerManager = ViewportClient->GetGameLayerManager();
			if ( LayerManager.IsValid() )
			{
				ULocalPlayer* TargetPlayer = GetOwnerPlayer();

				TSharedPtr<IGameLayer> Layer = LayerManager->FindLayerForPlayer(TargetPlayer, SharedLayerName);
				if ( Layer.IsValid() )
				{
					TSharedPtr<FWorldWidgetScreenLayer> ScreenLayer = StaticCastSharedPtr<FWorldWidgetScreenLayer>(Layer);
					ScreenLayer->RemoveComponent(this);
				}
			}
		}
	}
#endif // !UE_SERVER
}

class FWidgetComponentInstanceData : public FSceneComponentInstanceData
{
public:
	FWidgetComponentInstanceData( const UWidgetComponent* SourceComponent )
		: FSceneComponentInstanceData(SourceComponent)
		, WidgetClass ( SourceComponent->GetWidgetClass() )
		, RenderTarget( SourceComponent->GetRenderTarget() )
	{}

	virtual void ApplyToComponent(UActorComponent* Component, const ECacheApplyPhase CacheApplyPhase) override
	{
		FSceneComponentInstanceData::ApplyToComponent(Component, CacheApplyPhase);
		CastChecked<UWidgetComponent>(Component)->ApplyComponentInstanceData(this);
	}

	virtual void AddReferencedObjects(FReferenceCollector& Collector) override
	{
		FSceneComponentInstanceData::AddReferencedObjects(Collector);

		UClass* WidgetUClass = *WidgetClass;
		Collector.AddReferencedObject(WidgetUClass);
		Collector.AddReferencedObject(RenderTarget);
	}

public:
	TSubclassOf<UUserWidget> WidgetClass;
	UTextureRenderTarget2D* RenderTarget;
};

FActorComponentInstanceData* UWidgetComponent::GetComponentInstanceData() const
{
	return new FWidgetComponentInstanceData( this );
}

void UWidgetComponent::ApplyComponentInstanceData(FWidgetComponentInstanceData* WidgetInstanceData)
{
	check(WidgetInstanceData);

	// Note: ApplyComponentInstanceData is called while the component is registered so the rendering thread is already using this component
	// That means all component state that is modified here must be mirrored on the scene proxy, which will be recreated to receive the changes later due to MarkRenderStateDirty.

	if (GetWidgetClass() != WidgetClass)
	{
		return;
	}

	RenderTarget = WidgetInstanceData->RenderTarget;
	if( MaterialInstance && RenderTarget )
	{
		MaterialInstance->SetTextureParameterValue("SlateUI", RenderTarget);
	}

	MarkRenderStateDirty();
}

#if WITH_EDITORONLY_DATA
void UWidgetComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	UProperty* Property = PropertyChangedEvent.MemberProperty;

	if( Property && PropertyChangedEvent.ChangeType != EPropertyChangeType::Interactive )
	{
		static FName DrawSizeName("DrawSize");
		static FName PivotName("Pivot");
		static FName WidgetClassName("WidgetClass");
		static FName IsOpaqueName("bIsOpaque");
		static FName IsTwoSidedName("bIsTwoSided");
		static FName BackgroundColorName("BackgroundColor");
		static FName TintColorAndOpacityName("TintColorAndOpacity");
		static FName OpacityFromTextureName("OpacityFromTexture");
		static FName ParabolaDistortionName(TEXT("ParabolaDistortion"));
		static FName BlendModeName( TEXT( "BlendMode" ) );

		auto PropertyName = Property->GetFName();

		if( PropertyName == WidgetClassName )
		{
			Widget = nullptr;

			UpdateWidget();
			MarkRenderStateDirty();
		}
		else if ( PropertyName == DrawSizeName || PropertyName == PivotName )
		{
			MarkRenderStateDirty();
			UpdateBodySetup(true);
			RecreatePhysicsState();
		}
		else if ( PropertyName == IsOpaqueName || PropertyName == IsTwoSidedName || PropertyName == BlendModeName )
		{
			MarkRenderStateDirty();
		}
		else if( PropertyName == BackgroundColorName || PropertyName == ParabolaDistortionName )
		{
			MarkRenderStateDirty();
		}
		else if( PropertyName == TintColorAndOpacityName || PropertyName == OpacityFromTextureName )
		{
			MarkRenderStateDirty();
		}
	}

	Super::PostEditChangeProperty(PropertyChangedEvent);
}
#endif

void UWidgetComponent::InitWidget()
{
	// Don't do any work if Slate is not initialized
	if ( FSlateApplication::IsInitialized() )
	{
		if ( WidgetClass && Widget == nullptr && GetWorld() )
		{
			Widget = CreateWidget<UUserWidget>(GetWorld(), WidgetClass);
		}
		
#if WITH_EDITOR
		if ( Widget && !GetWorld()->IsGameWorld() && !bEditTimeUsable )
		{
			if( !GEnableVREditorHacks )
			{
				// Prevent native ticking of editor component previews
				Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);
			}
		}
#endif
	}
}

void UWidgetComponent::SetOwnerPlayer(ULocalPlayer* LocalPlayer)
{
	if ( OwnerPlayer != LocalPlayer )
	{
		RemoveWidgetFromScreen();
		OwnerPlayer = LocalPlayer;
	}
}

ULocalPlayer* UWidgetComponent::GetOwnerPlayer() const
{
	return OwnerPlayer ? OwnerPlayer : GEngine->GetLocalPlayerFromControllerId(GetWorld(), 0);
}

void UWidgetComponent::SetWidget(UUserWidget* InWidget)
{
	if( InWidget != nullptr )
	{
		SetSlateWidget( nullptr );
	}

	if ( Widget )
	{
		RemoveWidgetFromScreen();
	}

	Widget = InWidget;

	UpdateWidget();
}

void UWidgetComponent::SetSlateWidget( const TSharedPtr<SWidget>& InSlateWidget )
{
	if( Widget != nullptr )
	{
		SetWidget( nullptr );
	}

	if( SlateWidget.IsValid() )
	{
		RemoveWidgetFromScreen();
		SlateWidget.Reset();
	}

	SlateWidget = InSlateWidget;

	UpdateWidget();
}

void UWidgetComponent::UpdateWidget()
{
	// Don't do any work if Slate is not initialized
	if ( FSlateApplication::IsInitialized() )
	{
		if ( Space != EWidgetSpace::Screen )
		{
			TSharedPtr<SWidget> NewSlateWidget;
			if (Widget)
			{
				NewSlateWidget = Widget->TakeWidget();
			}

			if ( !SlateWindow.IsValid() )
			{
				SlateWindow = SNew(SVirtualWindow).Size(DrawSize);
				SlateWindow->SetIsFocusable(bWindowFocusable);
				RegisterWindow();
			}

			if ( !HitTestGrid.IsValid() )
			{
				HitTestGrid = MakeShareable(new FHittestGrid);
			}

			SlateWindow->Resize(DrawSize);

			if ( NewSlateWidget.IsValid() )
			{
				if ( NewSlateWidget != CurrentSlateWidget )
				{
					CurrentSlateWidget = NewSlateWidget;
					SlateWindow->SetContent(NewSlateWidget.ToSharedRef());
				}
			}
			else if( SlateWidget.IsValid() )
			{
				if ( SlateWidget != CurrentSlateWidget )
				{
					CurrentSlateWidget = SlateWidget;
					SlateWindow->SetContent(SlateWidget.ToSharedRef());
				}
			}
			else
			{
				CurrentSlateWidget = SNullWidget::NullWidget;
				SlateWindow->SetContent( SNullWidget::NullWidget );
			}
		}
		else
		{
			UnregisterWindow();
		}
	}
}

void UWidgetComponent::UpdateRenderTarget(FIntPoint DesiredRenderTargetSize)
{
	bool bWidgetRenderStateDirty = false;
	bool bClearColorChanged = false;

	FLinearColor ActualBackgroundColor = BackgroundColor;
	switch ( BlendMode )
	{
	case EWidgetBlendMode::Opaque:
		ActualBackgroundColor.A = 1.0f;
	case EWidgetBlendMode::Masked:
		ActualBackgroundColor.A = 0.0f;
	}

	if ( DesiredRenderTargetSize.X != 0 && DesiredRenderTargetSize.Y != 0 )
	{
		if ( RenderTarget == nullptr )
		{
			RenderTarget = NewObject<UTextureRenderTarget2D>(this);
			RenderTarget->ClearColor = ActualBackgroundColor;

			bClearColorChanged = bWidgetRenderStateDirty = true;

			RenderTarget->InitCustomFormat(DesiredRenderTargetSize.X, DesiredRenderTargetSize.Y, PF_B8G8R8A8, false);

			MaterialInstance->SetTextureParameterValue("SlateUI", RenderTarget);
		}
		else
		{
			// Update the format
			if ( RenderTarget->SizeX != DesiredRenderTargetSize.X || RenderTarget->SizeY != DesiredRenderTargetSize.Y )
			{
				RenderTarget->InitCustomFormat(DesiredRenderTargetSize.X, DesiredRenderTargetSize.Y, PF_B8G8R8A8, false);
				RenderTarget->UpdateResourceImmediate(false);
				bWidgetRenderStateDirty = true;
			}

			// Update the clear color
			if ( RenderTarget->ClearColor != ActualBackgroundColor )
			{
				RenderTarget->ClearColor = ActualBackgroundColor;
				bClearColorChanged = bWidgetRenderStateDirty = true;
			}

			if ( bWidgetRenderStateDirty )
			{
				RenderTarget->UpdateResource();
			}
		}
	}

	if ( RenderTarget )
	{
		// If the clear color of the render target changed, update the BackColor of the material to match
		if ( bClearColorChanged )
		{
			MaterialInstance->SetVectorParameterValue("BackColor", RenderTarget->ClearColor);
		}

		static FName ParabolaDistortionName(TEXT("ParabolaDistortion"));

		float CurrentParabolaValue;
		if ( MaterialInstance->GetScalarParameterValue(ParabolaDistortionName, CurrentParabolaValue) && CurrentParabolaValue != ParabolaDistortion )
		{
			MaterialInstance->SetScalarParameterValue(ParabolaDistortionName, ParabolaDistortion);
		}

		if ( bWidgetRenderStateDirty )
		{
			MarkRenderStateDirty();
		}
	}
}

void UWidgetComponent::UpdateBodySetup( bool bDrawSizeChanged )
{
	if (Space == EWidgetSpace::Screen)
	{
		// We do not have a bodysetup in screen space
		BodySetup = nullptr;
	}
	else if ( !BodySetup || bDrawSizeChanged )
	{
		BodySetup = NewObject<UBodySetup>(this);
		BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		BodySetup->AggGeom.BoxElems.Add(FKBoxElem());

		FKBoxElem* BoxElem = BodySetup->AggGeom.BoxElems.GetData();

		FVector Origin;
		if( bUseLegacyRotation )
		{
			Origin = FVector(
				( DrawSize.X * 0.5f ) - ( DrawSize.X * Pivot.X ),
				( DrawSize.Y * 0.5f ) - ( DrawSize.Y * Pivot.Y ), .5f);

			BoxElem->X = DrawSize.X;
			BoxElem->Y = DrawSize.Y;
			BoxElem->Z = 0.01f;
		}
		else
		{
			Origin = FVector(.5f,
				-( DrawSize.X * 0.5f ) + ( DrawSize.X * Pivot.X ),
				-( DrawSize.Y * 0.5f ) + ( DrawSize.Y * Pivot.Y ));
			
			BoxElem->X = 0.01f;
			BoxElem->Y = DrawSize.X;
			BoxElem->Z = DrawSize.Y;
		}

		BoxElem->SetTransform(FTransform::Identity);
		BoxElem->Center = Origin;
	}
}

void UWidgetComponent::GetLocalHitLocation(FVector WorldHitLocation, FVector2D& OutLocalWidgetHitLocation) const
{
	// Find the hit location on the component
	FVector ComponentHitLocation = ComponentToWorld.InverseTransformPosition(WorldHitLocation);

	// Convert the 3D position of component space, into the 2D equivalent
	if ( bUseLegacyRotation )
	{
		OutLocalWidgetHitLocation = FVector2D(ComponentHitLocation.X, ComponentHitLocation.Y);
	}
	else
	{
		OutLocalWidgetHitLocation = FVector2D(-ComponentHitLocation.Y, -ComponentHitLocation.Z);
	}

	// Offset the position by the pivot to get the position in widget space.
	OutLocalWidgetHitLocation.X += CurrentDrawSize.X * Pivot.X;
	OutLocalWidgetHitLocation.Y += CurrentDrawSize.Y * Pivot.Y;

	// Apply the parabola distortion
	FVector2D NormalizedLocation = OutLocalWidgetHitLocation / CurrentDrawSize;
	NormalizedLocation.Y += ParabolaDistortion * ( -2.0f * NormalizedLocation.Y + 1.0f ) * NormalizedLocation.X * ( NormalizedLocation.X - 1.0f );

	OutLocalWidgetHitLocation.Y = CurrentDrawSize.Y * NormalizedLocation.Y;
}

UUserWidget* UWidgetComponent::GetUserWidgetObject() const
{
	return Widget;
}

UTextureRenderTarget2D* UWidgetComponent::GetRenderTarget() const
{
	return RenderTarget;
}

UMaterialInstanceDynamic* UWidgetComponent::GetMaterialInstance() const
{
	return MaterialInstance;
}

const TSharedPtr<SWidget>& UWidgetComponent::GetSlateWidget() const
{
	return SlateWidget;
}

TArray<FWidgetAndPointer> UWidgetComponent::GetHitWidgetPath(FVector WorldHitLocation, bool bIgnoreEnabledStatus, float CursorRadius)
{
	FVector2D LocalHitLocation;
	GetLocalHitLocation(WorldHitLocation, LocalHitLocation);

	TSharedRef<FVirtualPointerPosition> VirtualMouseCoordinate = MakeShareable( new FVirtualPointerPosition );

	VirtualMouseCoordinate->CurrentCursorPosition = LocalHitLocation;
	VirtualMouseCoordinate->LastCursorPosition = LastLocalHitLocation;

	// Cache the location of the hit
	LastLocalHitLocation = LocalHitLocation;

	TArray<FWidgetAndPointer> ArrangedWidgets;
	if ( HitTestGrid.IsValid() )
	{
		ArrangedWidgets = HitTestGrid->GetBubblePath( LocalHitLocation, CursorRadius, bIgnoreEnabledStatus );

		for( FWidgetAndPointer& ArrangedWidget : ArrangedWidgets )
		{
			ArrangedWidget.PointerPosition = VirtualMouseCoordinate;
		}
	}

	return ArrangedWidgets;
}

TSharedPtr<SWindow> UWidgetComponent::GetSlateWindow() const
{
	return SlateWindow;
}

FVector2D UWidgetComponent::GetDrawSize() const
{
	return DrawSize;
}

void UWidgetComponent::SetDrawSize(FVector2D Size)
{
	FIntPoint NewDrawSize((int32)Size.X, (int32)Size.Y);

	if ( NewDrawSize != DrawSize )
	{
		DrawSize = NewDrawSize;
		MarkRenderStateDirty();
		UpdateBodySetup( true );
		RecreatePhysicsState();
	}
}

void UWidgetComponent::RequestRedraw()
{
	bRedrawRequested = true;
}

void UWidgetComponent::SetBlendMode( const EWidgetBlendMode NewBlendMode )
{
	if( NewBlendMode != this->BlendMode )
	{
		this->BlendMode = NewBlendMode;
		if( IsRegistered() )
		{
			MarkRenderStateDirty();
		}
	}
}

void UWidgetComponent::SetTwoSided( const bool bWantTwoSided )
{
	if( bWantTwoSided != this->bIsTwoSided )
	{
		this->bIsTwoSided = bWantTwoSided;
		if( IsRegistered() )
		{
			MarkRenderStateDirty();
		}
	}
}

void UWidgetComponent::SetBackgroundColor( const FLinearColor NewBackgroundColor )
{
	if( NewBackgroundColor != this->BackgroundColor)
	{
		this->BackgroundColor = NewBackgroundColor;
		MarkRenderStateDirty();
	}
}

void UWidgetComponent::SetTintColorAndOpacity( const FLinearColor NewTintColorAndOpacity )
{
	if( NewTintColorAndOpacity != this->TintColorAndOpacity )
	{
		this->TintColorAndOpacity = NewTintColorAndOpacity;
		UpdateMaterialInstanceParameters();
	}
}

void UWidgetComponent::SetOpacityFromTexture( const float NewOpacityFromTexture )
{
	if( NewOpacityFromTexture != this->OpacityFromTexture )
	{
		this->OpacityFromTexture = NewOpacityFromTexture;
		UpdateMaterialInstanceParameters();
	}
}

TSharedPtr< SWindow > UWidgetComponent::GetVirtualWindow() const
{
	return StaticCastSharedPtr<SWindow>(SlateWindow);
}

void UWidgetComponent::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_ADD_PIVOT_TO_WIDGET_COMPONENT )
	{
		Pivot = FVector2D(0, 0);
	}

	if ( GetLinkerUE4Version() < VER_UE4_ADD_BLEND_MODE_TO_WIDGET_COMPONENT )
	{
		BlendMode = bIsOpaque_DEPRECATED ? EWidgetBlendMode::Opaque : EWidgetBlendMode::Transparent;
	}

	if( GetLinkerUE4Version() < VER_UE4_FIXED_DEFAULT_ORIENTATION_OF_WIDGET_COMPONENT )
	{	
		// This indicates the value does not differ from the default.  In some rare cases this could cause incorrect rotation for anyone who directly set a value of 0,0,0 for rotation
		// However due to delta serialization we have no way to know if this value is actually different from the default so assume it is not.
		if( RelativeRotation == FRotator::ZeroRotator )
		{
			RelativeRotation = FRotator(0.f, 0.f, 90.f);
		}
		bUseLegacyRotation = true;
	}
}

UMaterialInterface* UWidgetComponent::GetMaterial(int32 MaterialIndex) const
{
	if ( OverrideMaterials.IsValidIndex(MaterialIndex) && ( OverrideMaterials[MaterialIndex] != nullptr ) )
	{
		return OverrideMaterials[MaterialIndex];
	}
	else
	{
		switch ( BlendMode )
		{
		case EWidgetBlendMode::Opaque:
			return bIsTwoSided ? OpaqueMaterial : OpaqueMaterial_OneSided;
			break;
		case EWidgetBlendMode::Masked:
			return bIsTwoSided ? MaskedMaterial : MaskedMaterial_OneSided;
			break;
		case EWidgetBlendMode::Transparent:
			return bIsTwoSided ? TranslucentMaterial : TranslucentMaterial_OneSided;
			break;
		}
	}

	return nullptr;
}

int32 UWidgetComponent::GetNumMaterials() const
{
	return FMath::Max<int32>(OverrideMaterials.Num(), 1);
}

void UWidgetComponent::UpdateMaterialInstanceParameters()
{
	if ( MaterialInstance )
	{
		MaterialInstance->SetTextureParameterValue("SlateUI", RenderTarget);
		MaterialInstance->SetVectorParameterValue("TintColorAndOpacity", TintColorAndOpacity);
		MaterialInstance->SetScalarParameterValue("OpacityFromTexture", OpacityFromTexture);
	}
}

void UWidgetComponent::SetWidgetClass(TSubclassOf<UUserWidget> InWidgetClass)
{
	WidgetClass = InWidgetClass;
}

// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

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

DECLARE_CYCLE_STAT(TEXT("3DHitTesting"), STAT_Slate3DHitTesting, STATGROUP_Slate);

static const FName SharedLayerName(TEXT("WidgetComponentScreenLayer"));

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

		const FMatrix& LocalToWorld = GetLocalToWorld();

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

						MeshBuilder.GetMesh(LocalToWorld, MaterialProxy, SDPG_World, false, true, ViewIndex, Collector);
					}
				}
			}
		}

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		RenderBounds(Collector.GetPDI(0), ViewFamily.EngineShowFlags, GetBounds(), IsSelected());
#endif
	}

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) const override
	{
		bool bVisible = true;

		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View) && bVisible;
		Result.bOpaqueRelevance = BlendMode == EWidgetBlendMode::Opaque;
		Result.bMaskedRelevance = BlendMode == EWidgetBlendMode::Masked;
		// ideally the TranslucencyRelevance should be filled out by the material, here we do it conservative
		Result.bSeparateTranslucencyRelevance = Result.bSeparateTranslucencyRelevance = (BlendMode == EWidgetBlendMode::Transparent);
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
	UBodySetup* BodySetup;
	EWidgetBlendMode BlendMode;
	bool bUseLegacyRotation;
};

/**
* The hit tester used by all Widget Component objects.
*/
class FWidget3DHitTester : public ICustomHitTestPath
{
public:
	FWidget3DHitTester( UWorld* InWorld )
		: World( InWorld )
		, CachedFrame(-1)
	{}

	// ICustomHitTestPath implementation
	virtual TArray<FWidgetAndPointer> GetBubblePathAndVirtualCursors(const FGeometry& InGeometry, FVector2D DesktopSpaceCoordinate, bool bIgnoreEnabledStatus) const override
	{
		SCOPE_CYCLE_COUNTER(STAT_Slate3DHitTesting);

		if( World.IsValid() && ensure( World->IsGameWorld() ) )
		{
			UWorld* SafeWorld = World.Get();
			if ( SafeWorld )
			{
				ULocalPlayer* const TargetPlayer = GEngine->GetLocalPlayerFromControllerId(SafeWorld, 0);

				if( TargetPlayer && TargetPlayer->PlayerController )
				{
					if ( UPrimitiveComponent* HitComponent = GetHitResultAtScreenPositionAndCache(TargetPlayer->PlayerController, InGeometry.AbsoluteToLocal(DesktopSpaceCoordinate)) )
					{
						if ( UWidgetComponent* WidgetComponent = Cast<UWidgetComponent>(HitComponent) )
						{
							// Get the "forward" vector based on the current rotation system.
							const FVector ForwardVector = WidgetComponent->IsUsingLegacyRotation() ? WidgetComponent->GetUpVector() : WidgetComponent->GetForwardVector();

							// Make sure the player is interacting with the front of the widget
							if ( FVector::DotProduct(ForwardVector, CachedHitResult.ImpactPoint - CachedHitResult.TraceStart) < 0.f )
							{
								// Make sure the player is close enough to the widget to interact with it
								if ( FVector::DistSquared(CachedHitResult.TraceStart, WidgetComponent->GetComponentLocation()) <= FMath::Square(WidgetComponent->GetMaxInteractionDistance()) )
								{
									return WidgetComponent->GetHitWidgetPath(CachedHitResult.Location, bIgnoreEnabledStatus);
								}
							}
						}
					}
				}
			}
		}

		return TArray<FWidgetAndPointer>();
	}

	virtual void ArrangeChildren( FArrangedChildren& ArrangedChildren ) const override
	{
		for( TWeakObjectPtr<UWidgetComponent> Component : RegisteredComponents )
		{
			UWidgetComponent* WidgetComponent = Component.Get();
			// Check if visible;
			if ( WidgetComponent && WidgetComponent->GetSlateWidget().IsValid() )
			{
				FGeometry WidgetGeom;

				ArrangedChildren.AddWidget( FArrangedWidget( WidgetComponent->GetSlateWidget().ToSharedRef(), WidgetGeom.MakeChild( WidgetComponent->GetDrawSize(), FSlateLayoutTransform() ) ) );
			}
		}
	}

	virtual TSharedPtr<struct FVirtualPointerPosition> TranslateMouseCoordinateFor3DChild( const TSharedRef<SWidget>& ChildWidget, const FGeometry& ViewportGeometry, const FVector2D& ScreenSpaceMouseCoordinate, const FVector2D& LastScreenSpaceMouseCoordinate ) const override
	{
		if ( World.IsValid() && ensure(World->IsGameWorld()) )
		{
			ULocalPlayer* const TargetPlayer = GEngine->GetLocalPlayerFromControllerId(World.Get(), 0);
			if ( TargetPlayer && TargetPlayer->PlayerController )
			{
				FVector2D LocalMouseCoordinate = ViewportGeometry.AbsoluteToLocal(ScreenSpaceMouseCoordinate);

				// Check for a hit against any widget components in the world
				for ( TWeakObjectPtr<UWidgetComponent> Component : RegisteredComponents )
				{
					UWidgetComponent* WidgetComponent = Component.Get();
					// Check if visible;
					if ( WidgetComponent && WidgetComponent->GetSlateWidget() == ChildWidget )
					{
						if ( UPrimitiveComponent* HitComponent = GetHitResultAtScreenPositionAndCache(TargetPlayer->PlayerController, LocalMouseCoordinate) )
						{
							if ( WidgetComponent == HitComponent )
							{
								TSharedPtr<FVirtualPointerPosition> VirtualCursorPos = MakeShareable(new FVirtualPointerPosition);

								FVector2D LocalHitLocation;
								WidgetComponent->GetLocalHitLocation(CachedHitResult.Location, LocalHitLocation);

								VirtualCursorPos->CurrentCursorPosition = LocalHitLocation;
								VirtualCursorPos->LastCursorPosition = LocalHitLocation;

								return VirtualCursorPos;
							}
						}
					}
				}
			}
		}

		return nullptr;
	}
	// End ICustomHitTestPath

	UPrimitiveComponent* GetHitResultAtScreenPositionAndCache(APlayerController* PlayerController, FVector2D ScreenPosition) const
	{
		UPrimitiveComponent* HitComponent = nullptr;

		if ( GFrameNumber != CachedFrame || CachedScreenPosition != ScreenPosition )
		{
			CachedFrame = GFrameNumber;
			CachedScreenPosition = ScreenPosition;

			if ( PlayerController )
			{
				if ( PlayerController->GetHitResultAtScreenPosition(ScreenPosition, ECC_Visibility, true, CachedHitResult) )
				{
					return CachedHitResult.Component.Get();
				}
			}
		}
		else
		{
			return CachedHitResult.Component.Get();
		}

		return nullptr;
	}

	void RegisterWidgetComponent( UWidgetComponent* InComponent )
	{
		RegisteredComponents.AddUnique( InComponent );
	}

	void UnregisterWidgetComponent( UWidgetComponent* InComponent )
	{
		RegisteredComponents.RemoveSingleSwap( InComponent );
	}

	uint32 GetNumRegisteredComponents() const { return RegisteredComponents.Num(); }
	
	UWorld* GetWorld() const { return World.Get(); }

private:
	TArray< TWeakObjectPtr<UWidgetComponent> > RegisteredComponents;
	TWeakObjectPtr<UWorld> World;

	mutable int64 CachedFrame;
	mutable FVector2D CachedScreenPosition;
	mutable FHitResult CachedHitResult;
};

UWidgetComponent::UWidgetComponent( const FObjectInitializer& PCIP )
	: Super( PCIP )
	, DrawSize( FIntPoint( 500, 500 ) )
	, MaxInteractionDistance( 1000.f )
	, BackgroundColor( FLinearColor::Transparent )
	, BlendMode( EWidgetBlendMode::Masked )
	, bIsOpaque_DEPRECATED( false )
	, bIsTwoSided( false )
	, ParabolaDistortion( 0 )
	, TickWhenOffscreen( false )
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
	if ( Space != EWidgetSpace::Screen && WidgetRenderer.IsValid() )
	{
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
			( DrawSize.X * 0.5f ) - ( DrawSize.X * Pivot.X ),
			( DrawSize.Y * 0.5f ) - ( DrawSize.Y * Pivot.Y ));

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
			BoxHalfExtent = ( FVector(DrawSize.X * 0.5f, DrawSize.Y * 0.5f, 1.0f) * ComponentToWorld.GetScale3D() ) + Inflation;
		}
		else
		{
			BoxHalfExtent = ( FVector(1.0f, DrawSize.X * 0.5f, DrawSize.Y * 0.5f) * ComponentToWorld.GetScale3D() ) + Inflation;
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
			if ( GetWorld()->IsGameWorld() )
			{
				TSharedPtr<SViewport> GameViewportWidget = GEngine->GetGameViewportWidget();

				if ( GameViewportWidget.IsValid() )
				{
					TSharedPtr<ICustomHitTestPath> CustomHitTestPath = GameViewportWidget->GetCustomHitTestPath();
					if ( !CustomHitTestPath.IsValid() )
					{
						CustomHitTestPath = MakeShareable(new FWidget3DHitTester(GetWorld()));
						GameViewportWidget->SetCustomHitTestPath(CustomHitTestPath);
					}

					TSharedPtr<FWidget3DHitTester> Widget3DHitTester = StaticCastSharedPtr<FWidget3DHitTester>(CustomHitTestPath);
					if ( Widget3DHitTester->GetWorld() == GetWorld() )
					{
						Widget3DHitTester->RegisterWidgetComponent(this);
					}
				}
			}

			if( !MaterialInstance )
			{
				UpdateMaterialInstance();
			}
		}

		InitWidget();

		if ( Space != EWidgetSpace::Screen )
		{
			if ( !WidgetRenderer.IsValid() && !GUsingNullRHI )
			{
				WidgetRenderer = MakeShareable(new FWidgetRenderer());
			}
		}
	}
#endif // !UE_SERVER
}

void UWidgetComponent::OnUnregister()
{
	if( GetWorld()->IsGameWorld() )
	{
		TSharedPtr<SViewport> GameViewportWidget = GEngine->GetGameViewportWidget();
		if( GameViewportWidget.IsValid() )
		{
			TSharedPtr<ICustomHitTestPath> CustomHitTestPath = GameViewportWidget->GetCustomHitTestPath();
			if( CustomHitTestPath.IsValid() )
			{
				TSharedPtr<FWidget3DHitTester> WidgetHitTestPath = StaticCastSharedPtr<FWidget3DHitTester>(CustomHitTestPath);

				WidgetHitTestPath->UnregisterWidgetComponent( this );

				if ( WidgetHitTestPath->GetNumRegisteredComponents() == 0 )
				{
					GameViewportWidget->SetCustomHitTestPath( nullptr );
				}
			}
		}
	}

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
		Widget->MarkPendingKill();
		Widget = nullptr;
	}

	WidgetRenderer.Reset();
	SlateWidget.Reset();
	HitTestGrid.Reset();
}

void UWidgetComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
#if !UE_SERVER

	static const int32 LayerZOrder = -100;

	UpdateWidget();

	if ( Widget == nullptr )
	{
		return;
	}

	if ( Space != EWidgetSpace::Screen )
	{
		if ( GUsingNullRHI )
		{
			return;
		}

		if ( !SlateWidget.IsValid() )
		{
			return;
		}

		if ( DrawSize.X == 0 || DrawSize.Y == 0 )
		{
			return;
		}

		const float RenderTimeThreshold = .5f;
		if ( IsVisible() )
		{
			// If we don't tick when off-screen, don't bother ticking if it hasn't been rendered recently
			if ( TickWhenOffscreen || GetWorld()->TimeSince(LastRenderTime) <= RenderTimeThreshold )
			{
				UpdateRenderTarget();
				
				const float DrawScale = 1.0f;

				WidgetRenderer->DrawWindow(
					RenderTarget,
					HitTestGrid.ToSharedRef(),
					SlateWidget.ToSharedRef(),
					DrawScale,
					DrawSize,
					DeltaTime);
			}
		}
	}
	else
	{
		if ( Widget && !Widget->IsDesignTime() )
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
						if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
						{
							TSharedPtr<IGameLayerManager> LayerManager = ViewportClient->GetGameLayerManager();
							if ( LayerManager.IsValid() )
							{
								TSharedPtr<FWorldWidgetScreenLayer> ScreenLayer;

								FLocalPlayerContext PlayerContext(TargetPlayer, GetWorld());

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

#endif // !UE_SERVER
}

void UWidgetComponent::RemoveWidgetFromScreen()
{
#if !UE_SERVER
	bAddedToScreen = false;

	if ( UGameViewportClient* ViewportClient = World->GetGameViewport() )
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
	if( MaterialInstance )
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
		static FName ParabolaDistortionName(TEXT("ParabolaDistortion"));

		auto PropertyName = Property->GetFName();

		if( PropertyName == DrawSizeName || PropertyName == WidgetClassName )
		{
			UpdateWidget();

			if ( PropertyName == DrawSizeName || PropertyName == PivotName )
			{
				UpdateBodySetup(true);
			}

			MarkRenderStateDirty();
		}
		else if ( PropertyName == IsOpaqueName || PropertyName == IsTwoSidedName )
		{
			UpdateMaterialInstance();
			MarkRenderStateDirty();
		}
		else if( PropertyName == BackgroundColorName || PropertyName == ParabolaDistortionName )
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
		if ( Widget && !GetWorld()->IsGameWorld() )
		{
			// Prevent native ticking of editor component previews
			Widget->SetDesignerFlags(EWidgetDesignFlags::Designing);
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
	if ( Widget )
	{
		RemoveWidgetFromScreen();
		Widget->MarkPendingKill();
		Widget = nullptr;
	}

	Widget = InWidget;

	UpdateWidget();
}

void UWidgetComponent::UpdateWidget()
{
	// Don't do any work if Slate is not initialized
	if ( FSlateApplication::IsInitialized() )
	{
		if ( Space != EWidgetSpace::Screen )
		{
			if ( !SlateWidget.IsValid() )
			{
				SlateWidget = SNew(SVirtualWindow).Size(DrawSize);
			}

			if ( !HitTestGrid.IsValid() )
			{
				HitTestGrid = MakeShareable(new FHittestGrid);
			}

			SlateWidget->Resize(DrawSize);

			if ( Widget )
			{
				SlateWidget->SetContent(Widget->TakeWidget());
			}
			else
			{
				SlateWidget->SetContent(SNullWidget::NullWidget);
			}
		}
		else
		{
			SlateWidget.Reset();
		}
	}
}

void UWidgetComponent::UpdateRenderTarget()
{
	bool bRenderStateDirty = false;
	bool bClearColorChanged = false;

	FLinearColor ActualBackgroundColor = BackgroundColor;
	switch ( BlendMode )
	{
	case EWidgetBlendMode::Opaque:
		ActualBackgroundColor.A = 1.0f;
	case EWidgetBlendMode::Masked:
		ActualBackgroundColor.A = 0.0f;
	}

	if ( DrawSize.X != 0 && DrawSize.Y != 0 )
	{
		if ( RenderTarget == nullptr )
		{
			RenderTarget = NewObject<UTextureRenderTarget2D>(this);
			RenderTarget->ClearColor = ActualBackgroundColor;

			bClearColorChanged = bRenderStateDirty = true;

			RenderTarget->InitCustomFormat(DrawSize.X, DrawSize.Y, PF_B8G8R8A8, false);

			MaterialInstance->SetTextureParameterValue("SlateUI", RenderTarget);
		}
		else
		{
			// Update the format
			if ( RenderTarget->SizeX != DrawSize.X || RenderTarget->SizeY != DrawSize.Y )
			{
				RenderTarget->InitCustomFormat(DrawSize.X, DrawSize.Y, PF_B8G8R8A8, false);
				bRenderStateDirty = true;
			}

			// Update the clear color
			if ( RenderTarget->ClearColor != ActualBackgroundColor )
			{
				RenderTarget->ClearColor = ActualBackgroundColor;
				bClearColorChanged = bRenderStateDirty = true;
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
			bRenderStateDirty = true;
		}

		if ( bRenderStateDirty )
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
	else if( !BodySetup || bDrawSizeChanged )
	{
		BodySetup = NewObject<UBodySetup>(this);
		BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		BodySetup->AggGeom.BoxElems.Add(FKBoxElem());

		FKBoxElem* BoxElem = BodySetup->AggGeom.BoxElems.GetData();

		FVector Origin;
		if( bUseLegacyRotation )
		{
			Origin = FVector(
				(DrawSize.X * 0.5f) - ( DrawSize.X * Pivot.X ),
				(DrawSize.Y * 0.5f) - ( DrawSize.Y * Pivot.Y ), .5f);

					
			BoxElem->X = DrawSize.X;
			BoxElem->Y = DrawSize.Y;
			BoxElem->Z = 1.0f;
		}
		else
		{
			Origin = FVector(.5f,
				(DrawSize.X * 0.5f) - ( DrawSize.X * Pivot.X ),
				(DrawSize.Y * 0.5f) - ( DrawSize.Y * Pivot.Y ) );
					
			BoxElem->X = 1.0f;
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
		OutLocalWidgetHitLocation = FVector2D(ComponentHitLocation.Y, ComponentHitLocation.Z);
	}

	// Offset the position by the pivot to get the position in widget space.
	OutLocalWidgetHitLocation.X += DrawSize.X * Pivot.X;
	OutLocalWidgetHitLocation.Y += DrawSize.Y * Pivot.Y;

	// Apply the parabola distortion
	FVector2D NormalizedLocation = OutLocalWidgetHitLocation / DrawSize;
	NormalizedLocation.Y += ParabolaDistortion * ( -2.0f * NormalizedLocation.Y + 1.0f ) * NormalizedLocation.X * ( NormalizedLocation.X - 1.0f );

	OutLocalWidgetHitLocation.Y = DrawSize.Y * NormalizedLocation.Y;
}

UUserWidget* UWidgetComponent::GetUserWidgetObject() const
{
	return Widget;
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

TSharedPtr<SWidget> UWidgetComponent::GetSlateWidget() const
{
	return SlateWidget;
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
		RecreatePhysicsState();
	}
}

float UWidgetComponent::GetMaxInteractionDistance() const
{
	return MaxInteractionDistance;
}

void UWidgetComponent::SetMaxInteractionDistance(float Distance)
{
	MaxInteractionDistance = Distance;
}

TSharedPtr< SWindow > UWidgetComponent::GetVirtualWindow() const
{
	return StaticCastSharedPtr<SWindow>(SlateWidget);
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

void UWidgetComponent::UpdateMaterialInstance()
{
	UMaterialInterface* Parent = nullptr;
	switch ( BlendMode )
	{
	case EWidgetBlendMode::Opaque:
		Parent = bIsTwoSided ? OpaqueMaterial : OpaqueMaterial_OneSided;
		break;
	case EWidgetBlendMode::Masked:
		Parent = bIsTwoSided ? MaskedMaterial : MaskedMaterial_OneSided;
		break;
	case EWidgetBlendMode::Transparent:
		Parent = bIsTwoSided ? TranslucentMaterial : TranslucentMaterial_OneSided;
		break;
	}

	if( MaterialInstance )
	{
		MaterialInstance->MarkPendingKill();
		MaterialInstance = nullptr;
	}

	MaterialInstance = UMaterialInstanceDynamic::Create(Parent, this);

	if( MaterialInstance )
	{
		MaterialInstance->SetTextureParameterValue("SlateUI", RenderTarget);
	}

}

void UWidgetComponent::SetWidgetClass(TSubclassOf<UUserWidget> InWidgetClass)
{
	WidgetClass = InWidgetClass;
}

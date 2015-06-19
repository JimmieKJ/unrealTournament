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

DECLARE_CYCLE_STAT(TEXT("3DHitTesting"), STAT_Slate3DHitTesting, STATGROUP_Slate);

class SVirtualWindow : public SWindow
{
	SLATE_BEGIN_ARGS( SVirtualWindow )
		: _Size( FVector2D(100,100) )
	{}

		SLATE_ARGUMENT( FVector2D, Size )
	SLATE_END_ARGS()

public:
	void Construct( const FArguments& InArgs )
	{
		bIsPopupWindow = true;
		SetCachedSize( InArgs._Size );
		SetNativeWindow( MakeShareable( new FGenericWindow() ) );
		SetContent( SNullWidget::NullWidget );
	}
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
		, bIsOpaque( InComponent->IsOpaque() )
	{
		bWillEverBeLit = false;	
	}

	~FWidget3DSceneProxy()
	{
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
						VertexIndices[0] = MeshBuilder.AddVertex(FVector(U, V, 0), FVector2D(0, 0), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);
						VertexIndices[1] = MeshBuilder.AddVertex(FVector(U, VL, 0), FVector2D(0, 1), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);
						VertexIndices[2] = MeshBuilder.AddVertex(FVector(UL, VL, 0), FVector2D(1, 1), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);
						VertexIndices[3] = MeshBuilder.AddVertex(FVector(UL, V, 0), FVector2D(1, 0), FVector(1, 0, 0), FVector(0, 1, 0), FVector(0, 0, 1), FColor::White);

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

	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) override
	{
		bool bVisible = true;// View->Family->EngineShowFlags.BillboardSprites;

		FPrimitiveViewRelevance Result;
		Result.bDrawRelevance = IsShown(View) && bVisible;
		Result.bOpaqueRelevance = bIsOpaque;
		Result.bNormalTranslucencyRelevance = !bIsOpaque;
		Result.bSeparateTranslucencyRelevance = !bIsOpaque;
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
	bool bIsOpaque;
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
							// Make sure the player is interacting with the front of the widget
							// For widget components, the "front" faces the Z (or Up vector) direction
							if ( FVector::DotProduct(WidgetComponent->GetUpVector(), CachedHitResult.ImpactPoint - CachedHitResult.TraceStart) < 0.f )
							{
								// Make sure the player is close enough to the widget to interact with it
								if ( FVector::DistSquared(CachedHitResult.TraceStart, CachedHitResult.ImpactPoint) <= FMath::Square(WidgetComponent->GetMaxInteractionDistance()) )
								{
									return WidgetComponent->GetHitWidgetPath(CachedHitResult, bIgnoreEnabledStatus);
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
								WidgetComponent->GetLocalHitLocation(CachedHitResult, LocalHitLocation);

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
	, bIsOpaque( false )
	, bIsTwoSided( false )
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	
	RelativeRotation = FRotator(0.f, 0.f, 90.f);
	
	BodyInstance.SetCollisionProfileName(FName(TEXT("UI")));

	// Translucent material instances
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TranslucentMaterial_Finder( TEXT("/Engine/EngineMaterials/Widget3DPassThrough_Translucent") );
	TranslucentMaterial = TranslucentMaterial_Finder.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> TranslucentMaterial_OneSided_Finder( TEXT( "/Engine/EngineMaterials/Widget3DPassThrough_Translucent_OneSided" ) );
	TranslucentMaterial_OneSided = TranslucentMaterial_OneSided_Finder.Object;

	// Opaque material instances
	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OpaqueMaterial_Finder( TEXT( "/Engine/EngineMaterials/Widget3DPassThrough_Opaque" ) );
	OpaqueMaterial = OpaqueMaterial_Finder.Object;

	static ConstructorHelpers::FObjectFinder<UMaterialInterface> OpaqueMaterial_OneSided_Finder( TEXT( "/Engine/EngineMaterials/Widget3DPassThrough_Opaque_OneSided" ) );
	OpaqueMaterial_OneSided = OpaqueMaterial_OneSided_Finder.Object;

	LastLocalHitLocation = FVector2D::ZeroVector;
	//bGenerateOverlapEvents = false;
	bUseEditorCompositing = false;

	Space = EWidgetSpace::World;
	Pivot = FVector2D(0.5, 0.5);
	ZOrder = -100;

	PrimaryComponentTick.TickGroup = TG_PostUpdateWork;
}

FPrimitiveSceneProxy* UWidgetComponent::CreateSceneProxy()
{
	if ( Space != EWidgetSpace::Screen )
	{
		return new FWidget3DSceneProxy(this, *Renderer);
	}
	else
	{
		return nullptr;
	}
}

FBoxSphereBounds UWidgetComponent::CalcBounds(const FTransform & LocalToWorld) const
{
	if ( Space != EWidgetSpace::Screen )
	{
		const FVector Origin = FVector(
			( DrawSize.X * 0.5f ) - ( DrawSize.X * Pivot.X ),
			( DrawSize.Y * 0.5f ) - ( DrawSize.Y * Pivot.Y ), .5f);
		const FVector BoxExtent = FVector(DrawSize.X / 2.0f, DrawSize.Y / 2.0f, 1.0f);

		return FBoxSphereBounds(Origin, BoxExtent, DrawSize.Size() / 2.0f).TransformBy(LocalToWorld);
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
		FVector	BoxHalfExtent = ( FVector(DrawSize.X * 0.5f, DrawSize.Y * 0.5f, 1.0f) * ComponentToWorld.GetScale3D() ) + Inflation;
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

			if ( !MaterialInstance )
			{
				UMaterialInterface* Parent = nullptr;
				if ( bIsOpaque )
				{
					Parent = bIsTwoSided ? OpaqueMaterial : OpaqueMaterial_OneSided;
				}
				else
				{
					Parent = bIsTwoSided ? TranslucentMaterial : TranslucentMaterial_OneSided;
				}

				MaterialInstance = UMaterialInstanceDynamic::Create(Parent, this);
			}
		}

		InitWidget();

		if ( Space != EWidgetSpace::Screen )
		{
			if ( !Renderer.IsValid() )
			{
				Renderer = FModuleManager::Get().LoadModuleChecked<ISlateRHIRendererModule>("SlateRHIRenderer").CreateSlate3DRenderer();
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

	if ( Widget )
	{
		Widget->RemoveFromParent();
		Widget->MarkPendingKill();
		Widget = nullptr;
	}

	SlateWidget.Reset();
	Super::OnUnregister();
}

void UWidgetComponent::DestroyComponent(bool bPromoteChildren/*= false*/)
{
	Super::DestroyComponent(bPromoteChildren);

	Renderer.Reset();

	if ( Widget )
	{
		Widget->RemoveFromParent();
		Widget->MarkPendingKill();
		Widget = nullptr;
	}

	SlateWidget.Reset();
}

void UWidgetComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
#if !UE_SERVER

	UpdateWidget();

	if ( Widget == nullptr )
	{
		return;
	}

	if ( Space != EWidgetSpace::Screen )
	{
		if ( !SlateWidget.IsValid() )
		{
			return;
		}

		const float RenderTimeThreshold = .5f;
		if ( IsVisible() && GetWorld()->TimeSince(LastRenderTime) <= RenderTimeThreshold ) // Don't bother ticking if it hasn't been rendered recently
		{
			SlateWidget->SlatePrepass();

			FGeometry WindowGeometry = FGeometry::MakeRoot(DrawSize, FSlateLayoutTransform());

			SlateWidget->TickWidgetsRecursively(WindowGeometry, FApp::GetCurrentTime(), DeltaTime);

			// Ticking can cause geometry changes.  Recompute
			SlateWidget->SlatePrepass();

			// Get the free buffer & add our virtual window
			FSlateDrawBuffer& DrawBuffer = Renderer->GetDrawBuffer();
			FSlateWindowElementList& WindowElementList = DrawBuffer.AddWindowElementList(SlateWidget.ToSharedRef());

			int32 MaxLayerId = 0;
			{
				// Prepare the test grid 
				HitTestGrid->ClearGridForNewFrame(WindowGeometry.GetClippingRect());

				// Paint the window
				MaxLayerId = SlateWidget->Paint(
					FPaintArgs(SlateWidget.ToSharedRef(), *HitTestGrid, FVector2D::ZeroVector, FSlateApplication::Get().GetCurrentTime(), FSlateApplication::Get().GetDeltaTime()),
					WindowGeometry, WindowGeometry.GetClippingRect(),
					WindowElementList,
					0,
					FWidgetStyle(),
					SlateWidget->IsEnabled());
			}

			Renderer->DrawWindow_GameThread(DrawBuffer);

			UpdateRenderTarget();

			// Enqueue a command to unlock the draw buffer after all windows have been drawn
			ENQUEUE_UNIQUE_RENDER_COMMAND_THREEPARAMETER(UWidgetComponentRenderToTexture,
				FSlateDrawBuffer&, InDrawBuffer, DrawBuffer,
				UTextureRenderTarget2D*, InRenderTarget, RenderTarget,
				ISlate3DRenderer*, InRenderer, Renderer.Get(),
				{
					InRenderer->DrawWindowToTarget_RenderThread(RHICmdList, InRenderTarget, InDrawBuffer);
				});

		}
	}
	else
	{
		if ( Widget && !Widget->IsDesignTime() )
		{
			ULocalPlayer* TargetPlayer = OwnerPlayer ? OwnerPlayer : GEngine->GetLocalPlayerFromControllerId(GetWorld(), 0);
			APlayerController* PlayerController = TargetPlayer ? TargetPlayer->PlayerController : nullptr;

			if ( TargetPlayer && PlayerController && IsVisible() )
			{
				FVector WorldLocation = GetComponentLocation();

				FVector2D ScreenPosition;
				const bool bProjected = UWidgetLayoutLibrary::ProjectWorldLocationToWidgetPosition(
					PlayerController, WorldLocation, ScreenPosition);

				if ( bProjected )
				{
					Widget->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
					
					Widget->SetDesiredSizeInViewport(DrawSize);
					Widget->SetPositionInViewport(ScreenPosition, false);
					Widget->SetAlignmentInViewport(Pivot);
				}
				else
				{
					Widget->SetVisibility(ESlateVisibility::Hidden);
				}

				if ( !Widget->IsInViewport() )
				{
					Widget->SetPlayerContext(TargetPlayer);
					Widget->AddToPlayerScreen(ZOrder);
				}
			}
			else if ( Widget->IsInViewport() )
			{
				// If the component isn't visible, and the widget is on the viewport, remove it.
				Widget->RemoveFromParent();
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

FName UWidgetComponent::GetComponentInstanceDataType() const
{
	static const FName InstanceDataName(TEXT("WidgetInstanceData"));
	return InstanceDataName;
}

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
		else if ( PropertyName == IsOpaqueName  || PropertyName == BackgroundColorName || PropertyName == IsTwoSidedName)
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
		else if ( !WidgetClass && Widget )
		{
			Widget = nullptr;
		}

		if ( Widget && !GetWorld()->IsGameWorld() )
		{
			// Prevent native ticking of editor component previews
			Widget->SetIsDesignTime(true);
		}
	}
}

void UWidgetComponent::SetOwnerPlayer(ULocalPlayer* LocalPlayer)
{
	OwnerPlayer = LocalPlayer;
}

ULocalPlayer* UWidgetComponent::GetOwnerPlayer() const
{
	return OwnerPlayer;
}

void UWidgetComponent::SetWidget(UUserWidget* InWidget)
{
	if ( Widget )
	{
		Widget->RemoveFromParent();
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
	}
}

void UWidgetComponent::UpdateRenderTarget()
{
	bool bClearColorChanged = false;

	if(!RenderTarget && DrawSize != FIntPoint::ZeroValue)
	{
		RenderTarget = NewObject<UTextureRenderTarget2D>(this);

		RenderTarget->ClearColor = BackgroundColor;

		if (bIsOpaque)
		{
			RenderTarget->ClearColor.A = 1.0f;
		}

		bClearColorChanged = true;

		RenderTarget->InitCustomFormat(DrawSize.X, DrawSize.Y, PF_B8G8R8A8, false);

		MaterialInstance->SetTextureParameterValue( "SlateUI", RenderTarget );
		MarkRenderStateDirty();
	}
	else if ( DrawSize != FIntPoint::ZeroValue )
	{
		// Update the format
		if ( RenderTarget->SizeX != DrawSize.X || RenderTarget->SizeY != DrawSize.Y )
		{
			RenderTarget->InitCustomFormat( DrawSize.X, DrawSize.Y, PF_B8G8R8A8, false );
			MarkRenderStateDirty();
		}

		// Update the clear color
		if (!bIsOpaque && RenderTarget->ClearColor != BackgroundColor)
		{
			RenderTarget->ClearColor = BackgroundColor;
			bClearColorChanged = true;
		}
		else if ( bIsOpaque )
		{
			// If opaque, make sure the alpha channel is set to 1.0
			FLinearColor ClearColor( BackgroundColor.R, BackgroundColor.G, BackgroundColor.B, 1.0f );
			if ( RenderTarget->ClearColor != ClearColor )
			{
				RenderTarget->ClearColor = ClearColor;
				bClearColorChanged = true;
			}
		}
	}

	// If the clear color of the render target changed, update the BackColor of the material to match
	if ( bClearColorChanged )
	{
		MaterialInstance->SetVectorParameterValue( "BackColor", RenderTarget->ClearColor );
		MarkRenderStateDirty();
	}
}

void UWidgetComponent::UpdateBodySetup( bool bDrawSizeChanged )
{
	if( !BodySetup || bDrawSizeChanged )
	{
		BodySetup = NewObject<UBodySetup>(this);
		BodySetup->CollisionTraceFlag = CTF_UseSimpleAsComplex;
		BodySetup->AggGeom.BoxElems.Add(FKBoxElem());

		FKBoxElem* BoxElem = BodySetup->AggGeom.BoxElems.GetData();

		const FVector Origin = FVector(
			(DrawSize.X * 0.5f) - ( DrawSize.X * Pivot.X ),
			(DrawSize.Y * 0.5f) - ( DrawSize.Y * Pivot.Y ), .5f);

		BoxElem->SetTransform(FTransform::Identity);
		BoxElem->Center = Origin;
		BoxElem->X = DrawSize.X;
		BoxElem->Y = DrawSize.Y;
		BoxElem->Z = 1.0f;
	}	
}

void UWidgetComponent::GetLocalHitLocation(const FHitResult& HitResult, FVector2D& OutLocalHitLocation) const
{
	// Find the hit location on the component
	OutLocalHitLocation = FVector2D(ComponentToWorld.InverseTransformPosition(HitResult.Location));

	// Offset the position by the pivot to get the position in widget space.
	OutLocalHitLocation.X += DrawSize.X * Pivot.X;
	OutLocalHitLocation.Y += DrawSize.Y * Pivot.Y;
}

UUserWidget* UWidgetComponent::GetUserWidgetObject() const
{
	return Widget;
}

TArray<FWidgetAndPointer> UWidgetComponent::GetHitWidgetPath(const FHitResult& HitResult, bool bIgnoreEnabledStatus, float CursorRadius )
{
	FVector2D LocalHitLocation;
	GetLocalHitLocation(HitResult, LocalHitLocation);

	TSharedRef<FVirtualPointerPosition> VirtualMouseCoordinate = MakeShareable( new FVirtualPointerPosition );

	VirtualMouseCoordinate->CurrentCursorPosition = LocalHitLocation;
	VirtualMouseCoordinate->LastCursorPosition = LastLocalHitLocation;

	// Cache the location of the hit
	LastLocalHitLocation = LocalHitLocation;

	TArray<FWidgetAndPointer> ArrangedWidgets = HitTestGrid->GetBubblePath(LocalHitLocation, CursorRadius, bIgnoreEnabledStatus);

	for( FWidgetAndPointer& ArrangedWidget : ArrangedWidgets )
	{
		ArrangedWidget.PointerPosition = VirtualMouseCoordinate;
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
	DrawSize = FIntPoint((int32)Size.X, (int32)Size.Y);
}

float UWidgetComponent::GetMaxInteractionDistance() const
{
	return MaxInteractionDistance;
}

void UWidgetComponent::SetMaxInteractionDistance(float Distance)
{
	MaxInteractionDistance = Distance;
}

void UWidgetComponent::PostLoad()
{
	Super::PostLoad();

	if ( GetLinkerUE4Version() < VER_UE4_ADD_PIVOT_TO_WIDGET_COMPONENT )
	{
		Pivot = FVector2D(0, 0);
	}
}

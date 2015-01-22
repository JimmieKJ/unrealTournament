// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "UMGPrivatePCH.h"
#include "Slate/SlateBrushAsset.h"
#include "PreviewScene.h"
#include "EngineModule.h"
#include "SceneViewport.h"

#define LOCTEXT_NAMESPACE "UMG"

namespace FocusConstants
{
	const float TransitionTime = 0.25f;
}

FUMGViewportCameraTransform::FUMGViewportCameraTransform()
	: TransitionCurve(new FCurveSequence(0.0f, FocusConstants::TransitionTime, ECurveEaseFunction::CubicOut))
	, ViewLocation(FVector::ZeroVector)
	, ViewRotation(FRotator::ZeroRotator)
	, DesiredLocation(FVector::ZeroVector)
	, LookAt(FVector::ZeroVector)
	, StartLocation(FVector::ZeroVector)
	, OrthoZoom(DEFAULT_ORTHOZOOM)
{}

void FUMGViewportCameraTransform::SetLocation(const FVector& Position)
{
	ViewLocation = Position;
	DesiredLocation = ViewLocation;
}

void FUMGViewportCameraTransform::TransitionToLocation(const FVector& InDesiredLocation, bool bInstant)
{
	if ( bInstant )
	{
		SetLocation(InDesiredLocation);
		TransitionCurve->JumpToEnd();
	}
	else
	{
		DesiredLocation = InDesiredLocation;
		StartLocation = ViewLocation;

		TransitionCurve->Play();
	}
}

bool FUMGViewportCameraTransform::UpdateTransition()
{
	bool bIsAnimating = false;
	if ( TransitionCurve->IsPlaying() || ViewLocation != DesiredLocation )
	{
		float LerpWeight = TransitionCurve->GetLerp();

		if ( LerpWeight == 1.0f )
		{
			// Failsafe for the value not being exact on lerps
			ViewLocation = DesiredLocation;
		}
		else
		{
			ViewLocation = FMath::Lerp(StartLocation, DesiredLocation, LerpWeight);
		}


		bIsAnimating = true;
	}

	return bIsAnimating;
}

FMatrix FUMGViewportCameraTransform::ComputeOrbitMatrix() const
{
	FTransform Transform =
		FTransform(-LookAt) *
		FTransform(FRotator(0, ViewRotation.Yaw, 0)) *
		FTransform(FRotator(0, 0, ViewRotation.Pitch)) *
		FTransform(FVector(0, ( ViewLocation - LookAt ).Size(), 0));

	return Transform.ToMatrixNoScale() * FInverseRotationMatrix(FRotator(0, 90.f, 0));
}

FUMGViewportClient::FUMGViewportClient(FPreviewScene* InPreviewScene)
	: PreviewScene(InPreviewScene)
	, Viewport(nullptr)
	, ViewFOV(90.0f)
	, FOVAngle(90.0f)
	, AspectRatio(1.777777f)
	, EngineShowFlags(ESFIM_Game)
{
	ViewState.Allocate();

	BackgroundColor = FColor(55, 55, 55);
}

FUMGViewportClient::~FUMGViewportClient()
{

}

void FUMGViewportClient::AddReferencedObjects(FReferenceCollector & Collector)
{

}

void FUMGViewportClient::Tick(float InDeltaTime)
{
	if ( !GIntraFrameDebuggingGameThread )
	{
		// Begin Play
		if ( !PreviewScene->GetWorld()->bBegunPlay )
		{
			for ( FActorIterator It(PreviewScene->GetWorld()); It; ++It )
			{
				It->BeginPlay();
			}
			PreviewScene->GetWorld()->bBegunPlay = true;
		}

		// Tick
		PreviewScene->GetWorld()->Tick(LEVELTICK_All, InDeltaTime);
	}
}

void FUMGViewportClient::Draw(FViewport* InViewport, FCanvas* Canvas)
{
	FViewport* ViewportBackup = Viewport;
	Viewport = InViewport ? InViewport : Viewport;

	// Determine whether we should use world time or real time based on the scene.
	float TimeSeconds;
	float RealTimeSeconds;
	float DeltaTimeSeconds;

	const bool bIsRealTime = true;

	UWorld* World = GWorld;
	if ( ( GetScene() != World->Scene ) || ( bIsRealTime == true ) )
	{
		// Use time relative to start time to avoid issues with float vs double
		TimeSeconds = FApp::GetCurrentTime() - GStartTime;
		RealTimeSeconds = FApp::GetCurrentTime() - GStartTime;
		DeltaTimeSeconds = FApp::GetDeltaTime();
	}
	else
	{
		TimeSeconds = World->GetTimeSeconds();
		RealTimeSeconds = World->GetRealTimeSeconds();
		DeltaTimeSeconds = World->GetDeltaSeconds();
	}

	// Setup a FSceneViewFamily/FSceneView for the viewport.
	FSceneViewFamilyContext ViewFamily(FSceneViewFamily::ConstructionValues(
		Canvas->GetRenderTarget(),
		GetScene(),
		EngineShowFlags)
		.SetWorldTimes(TimeSeconds, DeltaTimeSeconds, RealTimeSeconds)
		.SetRealtimeUpdate(bIsRealTime));

	ViewFamily.EngineShowFlags = EngineShowFlags;

	//UpdateLightingShowFlags(ViewFamily.EngineShowFlags);

	//ViewFamily.ExposureSettings = ExposureSettings;

	//ViewFamily.LandscapeLODOverride = LandscapeLODOverride;

	FSceneView* View = CalcSceneView(&ViewFamily);

	//SetupViewForRendering(ViewFamily, *View);

	FSlateRect SafeFrame;
	View->CameraConstrainedViewRect = View->UnscaledViewRect;
	//if ( CalculateEditorConstrainedViewRect(SafeFrame, Viewport) )
	//{
	//	View->CameraConstrainedViewRect = FIntRect(SafeFrame.Left, SafeFrame.Top, SafeFrame.Right, SafeFrame.Bottom);
	//}

	if ( IsAspectRatioConstrained() )
	{
		// Clear the background to black if the aspect ratio is constrained, as the scene view won't write to all pixels.
		Canvas->Clear(FLinearColor::Black);
	}

	Canvas->Clear(BackgroundColor);

	GetRendererModule().BeginRenderingViewFamily(Canvas, &ViewFamily);

	// Remove temporary debug lines.
	// Possibly a hack. Lines may get added without the scene being rendered etc.
	if ( World->LineBatcher != NULL && ( World->LineBatcher->BatchedLines.Num() || World->LineBatcher->BatchedPoints.Num() ) )
	{
		World->LineBatcher->Flush();
	}

	if ( World->ForegroundLineBatcher != NULL && ( World->ForegroundLineBatcher->BatchedLines.Num() || World->ForegroundLineBatcher->BatchedPoints.Num() ) )
	{
		World->ForegroundLineBatcher->Flush();
	}
	
	//FCanvas* DebugCanvas = Viewport->GetDebugCanvas();

	//UDebugDrawService::Draw(ViewFamily.EngineShowFlags, Viewport, View, DebugCanvas);

	//
	//FlushRenderingCommands();

	Viewport = ViewportBackup;
}

FSceneInterface* FUMGViewportClient::GetScene() const
{
	UWorld* World = GetWorld();
	if ( World )
	{
		return World->Scene;
	}

	return NULL;
}

UWorld* FUMGViewportClient::GetWorld() const
{
	UWorld* OutWorldPtr = NULL;
	// If we have a valid scene get its world
	if ( PreviewScene )
	{
		OutWorldPtr = PreviewScene->GetWorld();
	}
	if ( OutWorldPtr == NULL )
	{
		OutWorldPtr = GWorld;
	}
	return OutWorldPtr;
}

bool FUMGViewportClient::IsOrtho() const
{
	return ViewInfo.ProjectionMode == ECameraProjectionMode::Orthographic;
}

bool FUMGViewportClient::IsPerspective() const
{
	return ViewInfo.ProjectionMode == ECameraProjectionMode::Perspective;
}

bool FUMGViewportClient::IsAspectRatioConstrained() const
{
	return ViewInfo.bConstrainAspectRatio;
}

ECameraProjectionMode::Type FUMGViewportClient::GetProjectionType() const
{
	return ViewInfo.ProjectionMode;
}

void FUMGViewportClient::SetBackgroundColor(FLinearColor InBackgroundColor)
{
	BackgroundColor = InBackgroundColor;
}

FLinearColor FUMGViewportClient::GetBackgroundColor() const
{
	return BackgroundColor;
}

float FUMGViewportClient::GetNearClipPlane() const
{
	return ( NearPlane < 0.0f ) ? GNearClippingPlane : NearPlane;
}

void FUMGViewportClient::OverrideNearClipPlane(float InNearPlane)
{
	NearPlane = InNearPlane;
}

float FUMGViewportClient::GetFarClipPlaneOverride() const
{
	return FarPlane;
}

void FUMGViewportClient::OverrideFarClipPlane(const float InFarPlane)
{
	FarPlane = InFarPlane;
}

float FUMGViewportClient::GetOrthoUnitsPerPixel(const FViewport* InViewport) const
{
	const float SizeX = InViewport->GetSizeXY().X;

	// 15.0f was coming from the CAMERA_ZOOM_DIV marco, seems it was chosen arbitrarily
	return ( GetOrthoZoom() / ( SizeX * 15.f ) )/* * ComputeOrthoZoomFactor(SizeX)*/;
}


FSceneView* FUMGViewportClient::CalcSceneView(FSceneViewFamily* ViewFamily)
{
	FSceneViewInitOptions ViewInitOptions;

	const FVector& ViewLocation = GetViewLocation();
	const FRotator& ViewRotation = GetViewRotation();

	const FIntPoint ViewportSizeXY = Viewport->GetSizeXY();

	FIntRect ViewRect = FIntRect(0, 0, ViewportSizeXY.X, ViewportSizeXY.Y);
	ViewInitOptions.SetViewRectangle(ViewRect);

	const ECameraProjectionMode::Type ProjectionType = GetProjectionType();
	const bool bConstrainAspectRatio = ViewInfo.bConstrainAspectRatio;

	ViewInitOptions.ViewMatrix = FTranslationMatrix(-GetViewLocation());
	if ( ProjectionType == ECameraProjectionMode::Perspective )
	{
		ViewInitOptions.ViewMatrix = ViewInitOptions.ViewMatrix * FInverseRotationMatrix(ViewRotation);

		ViewInitOptions.ViewMatrix = ViewInitOptions.ViewMatrix * FMatrix(
			FPlane(0, 0, 1, 0),
			FPlane(1, 0, 0, 0),
			FPlane(0, 1, 0, 0),
			FPlane(0, 0, 0, 1));

		float MinZ = GetNearClipPlane();
		float MaxZ = MinZ;
		float MatrixFOV = ViewFOV * (float)PI / 360.0f;

		if ( bConstrainAspectRatio )
		{
			ViewInitOptions.ProjectionMatrix = FReversedZPerspectiveMatrix(
				MatrixFOV,
				MatrixFOV,
				1.0f,
				AspectRatio,
				MinZ,
				MaxZ
				);

			ViewInitOptions.SetConstrainedViewRectangle(Viewport->CalculateViewExtents(AspectRatio, ViewRect));
		}
		else
		{
			float XAxisMultiplier;
			float YAxisMultiplier;

			if ( ViewportSizeXY.X > ViewportSizeXY.Y )
			{
				//if the viewport is wider than it is tall
				XAxisMultiplier = 1.0f;
				YAxisMultiplier = ViewportSizeXY.X / (float)ViewportSizeXY.Y;
			}
			else
			{
				//if the viewport is taller than it is wide
				XAxisMultiplier = ViewportSizeXY.Y / (float)ViewportSizeXY.X;
				YAxisMultiplier = 1.0f;
			}

			ViewInitOptions.ProjectionMatrix = FReversedZPerspectiveMatrix(
				MatrixFOV,
				MatrixFOV,
				XAxisMultiplier,
				YAxisMultiplier,
				MinZ,
				MaxZ
				);
		}
	}
	else if ( ProjectionType == ECameraProjectionMode::Orthographic )
	{
		float ZScale = 0.5f / HALF_WORLD_MAX;
		float ZOffset = HALF_WORLD_MAX;

		//The divisor for the matrix needs to match the translation code.
		const float Zoom = GetOrthoUnitsPerPixel(Viewport);

		float OrthoWidth = Zoom * ViewportSizeXY.X / 2.0f;
		float OrthoHeight = Zoom * ViewportSizeXY.Y / 2.0f;

		ViewInitOptions.ViewMatrix = ViewInitOptions.ViewMatrix * FInverseRotationMatrix(ViewRotation);
		ViewInitOptions.ViewMatrix = ViewInitOptions.ViewMatrix * FMatrix(
			FPlane(0, 0, 1, 0),
			FPlane(1, 0, 0, 0),
			FPlane(0, 1, 0, 0),
			FPlane(0, 0, 0, 1));

		const float EffectiveAspectRatio = bConstrainAspectRatio ? AspectRatio : ( ViewportSizeXY.X / (float)ViewportSizeXY.Y );
		const float YScale = 1.0f / EffectiveAspectRatio;
		OrthoWidth = ViewInfo.OrthoWidth / 2.0f;
		OrthoHeight = ( ViewInfo.OrthoWidth / 2.0f ) * YScale;

		const float NearViewPlane = ViewInitOptions.ViewMatrix.GetOrigin().Z;
		const float FarViewPlane = NearViewPlane - 2.0f*WORLD_MAX;

		const float InverseRange = 1.0f / ( FarViewPlane - NearViewPlane );
		ZScale = -2.0f * InverseRange;
		ZOffset = -( FarViewPlane + NearViewPlane ) * InverseRange;

		ViewInitOptions.ProjectionMatrix = FReversedZOrthoMatrix(
			OrthoWidth,
			OrthoHeight,
			ZScale,
			ZOffset
			);

		if ( bConstrainAspectRatio )
		{
			ViewInitOptions.SetConstrainedViewRectangle(Viewport->CalculateViewExtents(AspectRatio, ViewRect));
		}
	}
	else
	{
		check(false);
	}

	ViewInitOptions.ViewFamily = ViewFamily;
	ViewInitOptions.SceneViewStateInterface = ViewState.GetReference();
	ViewInitOptions.ViewElementDrawer = this;

	ViewInitOptions.BackgroundColor = GetBackgroundColor();

	//ViewInitOptions.EditorViewBitflag = 0, // send the bit for this view - each actor will check it's visibility bits against this

	// for ortho views to steal perspective view origin
	//ViewInitOptions.OverrideLODViewOrigin = FVector::ZeroVector;
	//ViewInitOptions.bUseFauxOrthoViewPos = true;

	ViewInitOptions.OverrideFarClippingPlaneDistance = FarPlane;
	//ViewInitOptions.CursorPos = CurrentMousePos;

	FSceneView* View = new FSceneView(ViewInitOptions);

	ViewFamily->Views.Add(View);

	View->StartFinalPostprocessSettings(ViewLocation);

	//OverridePostProcessSettings(*View);

	View->EndFinalPostprocessSettings(ViewInitOptions);

	return View;
}

/////////////////////////////////////////////////////
// SAutoRefreshViewport

class SAutoRefreshViewport : public SViewport
{
	SLATE_BEGIN_ARGS(SAutoRefreshViewport)
	{
	}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs)
	{
		SViewport::FArguments ParentArgs;
		ParentArgs.IgnoreTextureAlpha(false);
		ParentArgs.EnableBlending(false);
		//ParentArgs.RenderDirectlyToWindow(true);
		SViewport::Construct(ParentArgs);

		ViewportClient = MakeShareable(new FUMGViewportClient(&PreviewScene));
		Viewport = MakeShareable(new FSceneViewport(ViewportClient.Get(), SharedThis(this)));

		// The viewport widget needs an interface so it knows what should render
		SetViewportInterface(Viewport.ToSharedRef());
	}

	void Tick(const FGeometry& AllottedGeometry, const double InCurrentTime, const float InDeltaTime) override
	{
		Viewport->Invalidate();
		Viewport->InvalidateDisplay();

		Viewport->Tick(AllottedGeometry, InCurrentTime, InDeltaTime);

		ViewportClient->Tick(InDeltaTime);
	}

public:
	TSharedPtr<FUMGViewportClient> ViewportClient;
	
	TSharedPtr<FSceneViewport> Viewport;

	/** preview scene */
	FPreviewScene PreviewScene;
};

/////////////////////////////////////////////////////
// UViewport

UViewport::UViewport(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ShowFlags(ESFIM_Game)
{
	bIsVariable = true;

	BackgroundColor = FLinearColor::Black;
	ShowFlags.DisableAdvancedFeatures();
	//ParentArgs.IgnoreTextureAlpha(false);
	//ParentArgs.EnableBlending(true);
	////ParentArgs.RenderDirectlyToWindow(true);
}

void UViewport::ReleaseSlateResources(bool bReleaseChildren)
{
	Super::ReleaseSlateResources(bReleaseChildren);

	ViewportWidget.Reset();
}

TSharedRef<SWidget> UViewport::RebuildWidget()
{
	if ( IsDesignTime() )
	{
		return BuildDesignTimeWidget(SNew(SBox)
			.HAlign(HAlign_Center)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Text(LOCTEXT("Viewport", "Viewport"))
			]);
	}
	else
	{
		ViewportWidget = SNew(SAutoRefreshViewport);

		if ( GetChildrenCount() > 0 )
		{
			ViewportWidget->SetContent(GetContentSlot()->Content ? GetContentSlot()->Content->TakeWidget() : SNullWidget::NullWidget);
		}

		return BuildDesignTimeWidget(ViewportWidget.ToSharedRef());
	}
}

void UViewport::SynchronizeProperties()
{
	Super::SynchronizeProperties();

	if ( ViewportWidget.IsValid() )
	{
		ViewportWidget->ViewportClient->SetBackgroundColor(BackgroundColor);
		ViewportWidget->ViewportClient->SetEngineShowFlags(ShowFlags);
	}
}

void UViewport::OnSlotAdded(UPanelSlot* Slot)
{
	// Add the child to the live canvas if it already exists
	if ( ViewportWidget.IsValid() )
	{
		ViewportWidget->SetContent(Slot->Content ? Slot->Content->TakeWidget() : SNullWidget::NullWidget);
	}
}

void UViewport::OnSlotRemoved(UPanelSlot* Slot)
{
	// Remove the widget from the live slot if it exists.
	if ( ViewportWidget.IsValid() )
	{
		ViewportWidget->SetContent(SNullWidget::NullWidget);
	}
}

UWorld* UViewport::GetViewportWorld() const
{
	if ( ViewportWidget.IsValid() )
	{
		return ViewportWidget->PreviewScene.GetWorld();
	}

	return NULL;
}

FVector UViewport::GetViewLocation() const
{
	if ( ViewportWidget.IsValid() )
	{
		return ViewportWidget->ViewportClient->GetViewLocation();
	}

	return FVector();
}

void UViewport::SetViewLocation(FVector Vector)
{
	if ( ViewportWidget.IsValid() )
	{
		ViewportWidget->ViewportClient->SetViewLocation(Vector);
	}
}

FRotator UViewport::GetViewRotation() const
{
	if ( ViewportWidget.IsValid() )
	{
		return ViewportWidget->ViewportClient->GetViewRotation();
	}

	return FRotator();
}

void UViewport::SetViewRotation(FRotator Rotator)
{
	if ( ViewportWidget.IsValid() )
	{
		ViewportWidget->ViewportClient->SetViewRotation(Rotator);
	}
}

AActor* UViewport::Spawn(TSubclassOf<AActor> ActorClass)
{
	if ( ViewportWidget.IsValid() )
	{
		UWorld* World = GetViewportWorld();
		return World->SpawnActor<AActor>(ActorClass, FVector(0, 0, 0), FRotator());
	}

	// TODO UMG Report spawning actor error before the world is ready.

	return NULL;
}

#if WITH_EDITOR

const FSlateBrush* UViewport::GetEditorIcon()
{
	return FUMGStyle::Get().GetBrush("Widget.Viewport");
}

const FText UViewport::GetPaletteCategory()
{
	return LOCTEXT("Primitive", "Primitive");
}

#endif

/////////////////////////////////////////////////////

#undef LOCTEXT_NAMESPACE

// Fill out your copyright notice in the Description page of Project Settings.

#include "LeapMotionControllerPrivatePCH.h"
#include "LeapMotionImageComponent.h"

#include "LeapMotionDevice.h"

#include "Leap_NoPI.h"

#include "Classes/Kismet/KismetMaterialLibrary.h"
#include "Runtime/HeadMountedDisplay/Public/IHeadMountedDisplay.h"


// Sets default values for this component's properties
ULeapMotionImageComponent::ULeapMotionImageComponent(const class FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	// Set this component to be initialized when the game starts, and to be ticked every frame.  You can turn these features
	// off to improve performance if you don't need them.
	bWantsInitializeComponent = true;
	PrimaryComponentTick.bCanEverTick = true;

	// ...
	bIsVisible = true;
	bIsPaused = false;

	DisplaySurfaceDistance = 100000.0f;

//#	define LM_ASSETS_FOLDER TEXT("/Game") // used by in-project plugin
#	define LM_ASSETS_FOLDER TEXT("/LeapMotionController") // used by in-engine plugin

	PassthroughMaterial = ConstructorHelpers::FObjectFinder<UMaterialInterface>(LM_ASSETS_FOLDER TEXT("/LM_PassthroughMaterial")).Object;

	UStaticMesh* DefaultCubeMesh = ConstructorHelpers::FObjectFinder<UStaticMesh>(TEXT("/Engine/BasicShapes/Cube")).Object;

	DisplaySurfaceComponent = ObjectInitializer.CreateDefaultSubobject<UStaticMeshComponent>(this, TEXT("DisplaySurfaceComponent"));
	DisplaySurfaceComponent->SetStaticMesh(DefaultCubeMesh);
	DisplaySurfaceComponent->SetCollisionEnabled(ECollisionEnabled::NoCollision);
	DisplaySurfaceComponent->SetMobility(EComponentMobility::Movable);

	DisplaySurfaceComponent->SetVisibility(bIsVisible);
}


// Called when the game starts
void ULeapMotionImageComponent::InitializeComponent()
{
	Super::InitializeComponent();

	// ...

	DynamicPassthroughMaterial = UKismetMaterialLibrary::CreateDynamicMaterialInstance(this, PassthroughMaterial);
	DisplaySurfaceComponent->SetMaterial(0, DynamicPassthroughMaterial);


	FLeapMotionDevice* Device = FLeapMotionControllerPlugin::GetLeapDeviceSafe();
	if (Device && Device->IsConnected())
	{
		Device->SetImagePolicy(true);
	}
}


// Called every frame
void ULeapMotionImageComponent::TickComponent( float DeltaTime, ELevelTick TickType, FActorComponentTickFunction* ThisTickFunction )
{
	Super::TickComponent( DeltaTime, TickType, ThisTickFunction );

	// ...
	if (!bIsPaused)
	{
		UpdateImageTexture();
	}
}

void ULeapMotionImageComponent::UpdateImageTexture()
{
	FLeapMotionDevice* Device = FLeapMotionControllerPlugin::GetLeapDeviceSafe();

	if (Device && Device->IsConnected())
	{
		Device->SetReferenceFrameOncePerTick();
	}
	
	if (Device && Device->IsConnected() && Device->Frame().images().count() > 1)
	{
		Leap::ImageList ImageList = Device->Frame().images();
		for (int eye = 0; eye < 2; eye++)
		{
			Leap::Image Image = ImageList[eye];

			UTexture2D*& Texture = eye ? ImagePassthroughRight : ImagePassthroughLeft;
			UTexture2D*& Distortion = eye ? DistortionTextureRight : DistortionTextureLeft;

			// Recreate the image & distortion textures.
			if (!Texture || Image.width() != Texture->PlatformData->SizeX || Image.height() != Texture->PlatformData->SizeY)
			{
				EPixelFormat PixelFormat = (Image.format() == Leap::Image::INFRARED) ? PF_G8 : PF_Unknown;
				Texture = UTexture2D::CreateTransient(Image.width(), Image.height(), PixelFormat);
				Texture->UpdateResource();

				Distortion = BuildDistortionTextures(Image);

				DynamicPassthroughMaterial->SetTextureParameterValue(eye ? TEXT("BaseTextureRight") : TEXT("BaseTexture"), Texture);
				DynamicPassthroughMaterial->SetTextureParameterValue(eye ? TEXT("DistortionTextureRight") : TEXT("DistortionTexture"), Distortion);

				if (GEngine && GEngine->GameViewport && GEngine->GameViewport->Viewport)
				{
					FIntPoint Resolution = GEngine->GameViewport->Viewport->GetSizeXY();
					FLinearColor ScreenRes = FLinearColor(Resolution.X, Resolution.Y, 0.f, 0.f);
					DynamicPassthroughMaterial->SetVectorParameterValue(TEXT("ScreenResolution"), ScreenRes);
				}
				
				AttachDisplaySurface();
			}

			// Extract the image 
			{
				UTexture2D*& Texture = eye ? ImagePassthroughRight : ImagePassthroughLeft;

				// Lock the texture so it can be modified
				uint8* MipData = static_cast<uint8*>(Texture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

				const uint8* LeapData = Image.data();
				const int LeapDataSize = Image.width() * Image.height();
				// This has no effect (!?): FMemory::Memcmp(MipData, LeapData, LeapDataSize);
				for (int32 i = 0; i < LeapDataSize; i++)
				{
					MipData[1 * i + 0] = LeapData[i];
				}

				// Unlock the texture
				Texture->PlatformData->Mips[0].BulkData.Unlock();
				Texture->UpdateResource();
			}

			// Hack: need to update distortion texture every frame to handle device flipping.
			UpdateDistortionTextures(Image, Distortion);
		}

		const bool bUsingHmd = GEngine->HMDDevice.IsValid() && GEngine->HMDDevice->IsHeadTrackingAllowed();
		DynamicPassthroughMaterial->SetScalarParameterValue("", bUsingHmd ? 1.4f : 1.0f);
	}
	else
	{
		// We can't find two images, that might be due to device being detached & maybe exchanged later -- trigger reset.
		ImagePassthroughLeft = nullptr;
		ImagePassthroughRight = nullptr;
		DistortionTextureLeft = nullptr;
		DistortionTextureRight = nullptr;
	}
}

UTexture2D* ULeapMotionImageComponent::BuildDistortionTextures( const Leap::Image& Image )
{
	UTexture2D* DistortionTexture = UTexture2D::CreateTransient(Image.distortionWidth() / 2, Image.distortionHeight(), PF_B8G8R8A8);
	DistortionTexture->SRGB = 0;
	DistortionTexture->UpdateResource();

	UpdateDistortionTextures(Image, DistortionTexture);

	return DistortionTexture;
}

void ULeapMotionImageComponent::UpdateDistortionTextures(const Leap::Image& Image, UTexture2D* DistortionTexture)
{
	uint8* MipData = static_cast<uint8*>(DistortionTexture->PlatformData->Mips[0].BulkData.Lock(LOCK_READ_WRITE));

	FColor* Colors = reinterpret_cast<FColor*>(MipData);

	const float* DistortionData = Image.distortion();
	int DistortionDataSize = /*2 * */Image.distortionWidth() * Image.distortionHeight();

	const int TexWidth = Image.distortionWidth() / 2;
	const int TexHeight = Image.distortionHeight();

	// Move distortion data to distortion x textures.
	for (int i = 0; i < DistortionDataSize; i += 2)
	{
		// The distortion range is -0.6 to +1.7. Normalize to range [0..1).
		float dval = (DistortionData[i] + 0.6f) / 2.3f;

		float enc_y = FMath::Frac(dval * 255.0f);
		float enc_x = FMath::Frac(dval - enc_y / 255.0f);

		int index = i >> 1;
		index = index % TexWidth + (TexHeight - 1 - index / TexWidth) * TexWidth;

		Colors[index].R = (uint8)(256 * enc_x);
		Colors[index].G = (uint8)(256 * enc_y);
	}

	// Move distortion data to distortion y textures.
	for (int i = 1; i < DistortionDataSize; i += 2)
	{
		// The distortion range is -0.6 to +1.7. Normalize to range [0..1).
		float dval = (DistortionData[i] + 0.6f) / 2.3f;

		float enc_y = FMath::Frac(dval * 255.0f);
		float enc_x = FMath::Frac(dval - enc_y / 255.0f);

		// Divide by 2, to get index for this one texture's pixel.
		int index = i >> 1;
		// we have to write out by rows from the bottom actually. Texture mapping is different in Unreal.
		index = index % TexWidth + (TexHeight - 1 - index / TexWidth) * TexWidth;

		Colors[index].B = (uint8)(256 * enc_x);
		Colors[index].A = (uint8)(256 * enc_y);
	}

	// Unlock the texture
	DistortionTexture->PlatformData->Mips[0].BulkData.Unlock();
	DistortionTexture->UpdateResource();
}

void ULeapMotionImageComponent::AttachDisplaySurface_Implementation()
{
	AActor* CameraManager = UGameplayStatics::GetPlayerCameraManager(this, 0);

	//@todo:  Fix this once we have CameraComponents that update with the HMD's position and orientation
	if (CameraManager)
	{
		DisplaySurfaceComponent->AttachTo(CameraManager->GetRootComponent());
		DisplaySurfaceComponent->SetRelativeLocation(CameraManager->GetTransform().Rotator().UnrotateVector(CameraManager->GetActorForwardVector()) * DisplaySurfaceDistance);
		DisplaySurfaceComponent->SetRelativeRotation(FRotator::ZeroRotator);
		DisplaySurfaceComponent->SetWorldScale3D(FVector(0.0f, 0.08f * DisplaySurfaceDistance, 0.08f * DisplaySurfaceDistance));
	}
}

#if WITH_EDITOR
void ULeapMotionImageComponent::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent)
{
	Super::PostEditChangeProperty(PropertyChangedEvent);

	DisplaySurfaceComponent->SetVisibility(bIsVisible);

	AttachDisplaySurface();
}
#endif

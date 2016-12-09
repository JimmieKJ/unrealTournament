#include "OculusRiftBoundaryComponent.h"
#include "Engine/Engine.h"
#include "EngineGlobals.h"

#if OCULUS_RIFT_SUPPORTED_PLATFORMS


//-------------------------------------------------------------------------------------------------
// Static Type Conversion Helpers for OculusRiftBoundaryComponent
//-------------------------------------------------------------------------------------------------

/** Helper that converts ovrVector3f type to FVector type (without modifying scale or coordinate space) */
static FVector ToFVector(ovrVector3f Vector)
{
	FVector NewVector = FVector(Vector.x, Vector.y, Vector.z);
	return NewVector;
}

/** Helper that converts FVector type to ovrVector3f type (without modifying scale or coordinate space)  */
static ovrVector3f ToOvrVector3f(FVector Vector)
{
	ovrVector3f NewVector = { Vector.X, Vector.Y, Vector.Z };
	return NewVector;
}

/** Helper that converts ovrBoundaryType to EBoundaryType */
static EBoundaryType ToEBoundaryType(ovrBoundaryType Source)
{
	EBoundaryType Destination = EBoundaryType::Boundary_Outer; // Best attempt at initialization

	switch (Source)
	{
		case ovrBoundary_Outer:
			Destination = EBoundaryType::Boundary_Outer;
			break;
		case ovrBoundary_PlayArea:
			Destination = EBoundaryType::Boundary_PlayArea;
			break;
		default:
			break;
	}
	return Destination;
}

/** Helper that converts EBoundaryType to ovrBoundaryType */
static ovrBoundaryType ToOvrBoundaryType(EBoundaryType Source)
{
	ovrBoundaryType Destination = ovrBoundary_Outer; // Best attempt at initialization

	switch (Source)
	{
		case EBoundaryType::Boundary_Outer:
			Destination = ovrBoundary_Outer;
			break;
		case EBoundaryType::Boundary_PlayArea:
			Destination = ovrBoundary_PlayArea;
			break;
		default:
			break;
	}
	return Destination;
}

/** Helper that converts ovrTrackedDeviceType to ETrackedDeviceType */
static ETrackedDeviceType ToETrackedDeviceType(ovrTrackedDeviceType Source)
{
	ETrackedDeviceType Destination = ETrackedDeviceType::All; // Best attempt at initialization

	switch (Source)
	{
		case ovrTrackedDevice_HMD:
			Destination = ETrackedDeviceType::HMD;
			break;
		case ovrTrackedDevice_LTouch:
			Destination = ETrackedDeviceType::LTouch;
			break;
		case ovrTrackedDevice_RTouch:
			Destination = ETrackedDeviceType::RTouch;
			break;
		case ovrTrackedDevice_Touch:
			Destination = ETrackedDeviceType::Touch;
			break;
		case ovrTrackedDevice_All:
			Destination = ETrackedDeviceType::All;
			break;
		default:
			break;
	}
	return Destination;
}

/** Helper that converts ETrackedDeviceType to ovrTrackedDeviceType */
static ovrTrackedDeviceType ToOVRTrackedDeviceType(ETrackedDeviceType Source)
{
	ovrTrackedDeviceType Destination = ovrTrackedDevice_All; // Best attempt at initialization

	switch (Source)
	{
		case ETrackedDeviceType::HMD:
			Destination = ovrTrackedDevice_HMD;
			break;
		case ETrackedDeviceType::LTouch:
			Destination = ovrTrackedDevice_LTouch;
			break;
		case ETrackedDeviceType::RTouch:
			Destination = ovrTrackedDevice_RTouch;
			break;
		case ETrackedDeviceType::Touch:
			Destination = ovrTrackedDevice_Touch;
			break;
		case ETrackedDeviceType::All:
			Destination = ovrTrackedDevice_All;
			break;
		default:
			break;
		}
	return Destination;
}


//-------------------------------------------------------------------------------------------------
// Other Static Helpers (decomposition) for OculusRiftBoundaryComponent
//-------------------------------------------------------------------------------------------------

static FVector PointToWorldSpace(ovrVector3f ovrPoint)
{
	FOculusRiftHMD* OculusHMD = (FOculusRiftHMD*)(GEngine->HMDDevice.Get());
	return OculusHMD->ScaleAndMovePointWithPlayer(ovrPoint);
}

static FVector DimensionsToWorldSpace(ovrVector3f Dimensions)
{
	FOculusRiftHMD* OculusHMD = (FOculusRiftHMD*)(GEngine->HMDDevice.Get());
	FVector WorldLength = OculusHMD->ConvertVector_M2U(Dimensions);
	WorldLength.X = -WorldLength.X;
	return WorldLength;
}

static FVector NormalToWorldSpace(ovrVector3f Normal)
{
	FVector WorldNormal = ToFVector(Normal);
	float temp = WorldNormal.X;
	WorldNormal.X = -WorldNormal.Z;
	WorldNormal.Z = WorldNormal.Y;
	WorldNormal.Y = temp;
	return WorldNormal;
}

static float DistanceToWorldSpace(float ovrDistance)
{
	FOculusRiftHMD* OculusHMD = (FOculusRiftHMD*)(GEngine->HMDDevice.Get());
	return OculusHMD->ConvertFloat_M2U(ovrDistance);
}

/**
 * Helper that checks if DeviceType triggers BoundaryType. If so, stores details about interaction and adds to ResultList!
 * @param OculusSession Specifies current ovrSession
 * @param ResultList Out-parameter -- a list to add to may or may not be specified. Stores each FBoundaryTestResult corresponding to a device that has triggered boundary system
 * @param DeviceType Specifies device type to test
 * @param BoundaryType Specifies EBoundaryType::Boundary_Outer or EBoundaryType::Boundary_PlayArea to test
 * @return true if DeviceType triggers BoundaryType
 */
static bool AddInteractionPairsToList(ovrSession OculusSession, TArray<FBoundaryTestResult>* ResultList, ovrTrackedDeviceType DeviceType, ovrBoundaryType BoundaryType)
{
	ovrBoundaryTestResult TestResult;
	ovrResult ovrRes = ovr_TestBoundary(OculusSession, DeviceType, BoundaryType, &TestResult);

	FBoundaryTestResult InteractionInfo;
	memset(&InteractionInfo, 0, sizeof(FBoundaryTestResult));
	bool IsTriggering = (TestResult.IsTriggering != 0);

	if (OVR_SUCCESS(ovrRes))
	{
		if (IsTriggering && ResultList != NULL)
		{
			InteractionInfo.IsTriggering = IsTriggering;
			InteractionInfo.DeviceType = ToETrackedDeviceType(DeviceType);
			InteractionInfo.ClosestDistance = DistanceToWorldSpace(TestResult.ClosestDistance);
			InteractionInfo.ClosestPoint = PointToWorldSpace(TestResult.ClosestPoint);
			InteractionInfo.ClosestPointNormal = NormalToWorldSpace(TestResult.ClosestPointNormal);

			ResultList->Add(InteractionInfo);
		}
	}

	return IsTriggering;
}

/**
 * Helper that gets geometry (3D points) of outer boundaries or play area (specified by BoundaryType)
 * @param OculusSession Specifies current ovrSession
 * @param BoundaryPoints Empty array of 3D points preallocated to size MaxNumBoundaryPoints
 * @param BoundaryType Must be ovrBoundary_Outer or ovrBoundary_PlayArea, specifies the type of boundary geometry to retrieve
 * @return Array of 3D points in Unreal world coordinate space corresponding to boundary geometry.
 */
static TArray<FVector> GetBoundaryPoints(ovrSession OculusSession, ovrVector3f* BoundaryPoints, ovrBoundaryType BoundaryType)
{
	TArray<FVector> BoundaryPointList;
	int NumPoints;
	ovrResult ovrRes = ovr_GetBoundaryGeometry(OculusSession, BoundaryType, BoundaryPoints, &NumPoints);

	if (OVR_SUCCESS(ovrRes))
	{
		for (int i = 0; i < NumPoints; i++)
		{
			FVector point = PointToWorldSpace(BoundaryPoints[i]);
			BoundaryPointList.Add(point);
		}
	}

	return BoundaryPointList;
}

/**
 * @param OculusSession Specifies current ovrSession
 * @param BoundaryType must be EBoundaryType::Boundary_Outer or EBoundaryType::Boundary_PlayArea
 * @param Point 3D point in Unreal coordinate space to be tested
 * @return Information about distance from specified boundary, whether the boundary is triggered, etc.
 */
static FBoundaryTestResult CheckPointInBounds(ovrSession OculusSession, EBoundaryType BoundaryType, const FVector Point)
{
	const ovrVector3f ovrPoint = ToOvrVector3f(Point);
	ovrBoundaryType ovrBounds = ToOvrBoundaryType(BoundaryType);
	ovrBoundaryTestResult InteractionResult;

	FBoundaryTestResult InteractionInfo;
	memset(&InteractionInfo, 0, sizeof(FBoundaryTestResult));
	ovrResult ovrRes = ovr_TestBoundaryPoint(OculusSession, &ovrPoint, ovrBounds, &InteractionResult);

	if (OVR_SUCCESS(ovrRes))
	{
		InteractionInfo.IsTriggering = (InteractionResult.IsTriggering != 0);
		InteractionInfo.ClosestDistance = DistanceToWorldSpace(InteractionResult.ClosestDistance);
		InteractionInfo.ClosestPoint = PointToWorldSpace(InteractionResult.ClosestPoint);
		InteractionInfo.ClosestPointNormal = NormalToWorldSpace(InteractionResult.ClosestPointNormal);
		InteractionInfo.DeviceType = ETrackedDeviceType::None;
	}

	return InteractionInfo;
}

#endif // OCULUS_RIFT_SUPPORTED_PLATFORMS

//-------------------------------------------------------------------------------------------------
// OculusRiftBoundaryComponent Member Functions
//-------------------------------------------------------------------------------------------------

UOculusRiftBoundaryComponent::UOculusRiftBoundaryComponent(const FObjectInitializer& ObjectInitializer)
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	: Super(ObjectInitializer)
{
	PrimaryComponentTick.bCanEverTick = true;
	bTickInEditor = true;
	bAutoActivate = true;
	bIsOuterBoundaryTriggered = false;
}
#else
{}
#endif

void UOculusRiftBoundaryComponent::BeginPlay()
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	Super::BeginPlay();
	Session = *FOvrSessionShared::AutoSession(FOculusRiftPlugin::Get().GetSession());
	bIsOuterBoundaryTriggered = AddInteractionPairsToList(Session, NULL, ovrTrackedDevice_HMD, ovrBoundary_Outer) ||
								AddInteractionPairsToList(Session, NULL, ovrTrackedDevice_LTouch, ovrBoundary_Outer) ||
								AddInteractionPairsToList(Session, NULL, ovrTrackedDevice_RTouch, ovrBoundary_Outer);
#endif
}

void UOculusRiftBoundaryComponent::TickComponent(float DeltaTime, enum ELevelTick TickType, FActorComponentTickFunction *ThisTickFunction)
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
	FOculusRiftHMD* OculusHMD = (FOculusRiftHMD*)(GEngine->HMDDevice.Get());
	if (OculusHMD && OculusHMD->IsHMDActive())
	{
		OuterBoundsInteractionList.Empty(); // Reset list of FBoundaryTestResults

		// Test each device with outer boundary, adding results to OuterBoundsInteractionList
		bool HMDTriggered = AddInteractionPairsToList(Session, &OuterBoundsInteractionList, ovrTrackedDevice_HMD, ovrBoundary_Outer);
		bool LTouchTriggered = AddInteractionPairsToList(Session, &OuterBoundsInteractionList, ovrTrackedDevice_LTouch, ovrBoundary_Outer);
		bool RTouchTriggered = AddInteractionPairsToList(Session, &OuterBoundsInteractionList, ovrTrackedDevice_RTouch, ovrBoundary_Outer);
		bool OuterBoundsTriggered = HMDTriggered || LTouchTriggered || RTouchTriggered;


		if (OuterBoundsTriggered != bIsOuterBoundaryTriggered) // outer boundary triggered status has changed
		{
			if (OuterBoundsTriggered)
			{
				OnOuterBoundaryTriggered.Broadcast(OuterBoundsInteractionList);
			}
			else
			{
				OnOuterBoundaryReturned.Broadcast();
			}
		}

		bIsOuterBoundaryTriggered = OuterBoundsTriggered;
	}
#endif
}

bool UOculusRiftBoundaryComponent::IsOuterBoundaryDisplayed()
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	ovrBool IsVisible;
	ovr_GetBoundaryVisible(Session, &IsVisible);
	return (IsVisible != 0);
#else
	return false;
#endif
}

bool UOculusRiftBoundaryComponent::IsOuterBoundaryTriggered()
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	return bIsOuterBoundaryTriggered;
#else
	return false;
#endif
}

bool UOculusRiftBoundaryComponent::SetOuterBoundaryColor(const FColor InBoundaryColor)
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	ovrColorf NewColor = { InBoundaryColor.R, InBoundaryColor.G, InBoundaryColor.B, InBoundaryColor.A };
	ovrBoundaryLookAndFeel OuterBoundaryProperties;
	OuterBoundaryProperties.Color = NewColor;

	ovrResult ovrRes = ovr_SetBoundaryLookAndFeel(Session, &OuterBoundaryProperties);
	if (!OVR_SUCCESS(ovrRes))
	{
		return false;
	}
#endif

	return true;
}

bool UOculusRiftBoundaryComponent::ResetOuterBoundaryColor()
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	ovrResult ovrRes = ovr_ResetBoundaryLookAndFeel(Session);
	if (!OVR_SUCCESS(ovrRes))
	{
		return false;
	}
#endif

	return true;
}

TArray<FVector> UOculusRiftBoundaryComponent::GetPlayAreaPoints()
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	return GetBoundaryPoints(Session, BoundaryPoints, ovrBoundary_PlayArea);
#else
	TArray<FVector> ReturnValue;
	return ReturnValue;
#endif
}

TArray<FVector> UOculusRiftBoundaryComponent::GetOuterBoundaryPoints()
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	return GetBoundaryPoints(Session, BoundaryPoints, ovrBoundary_Outer);
#else
	TArray<FVector> ReturnValue;
	return ReturnValue;
#endif
}

FVector UOculusRiftBoundaryComponent::GetPlayAreaDimensions()
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	ovrVector3f Dimensions = { 0.f, 0.f, 0.f };
	ovr_GetBoundaryDimensions(Session, ovrBoundary_PlayArea, &Dimensions);

	return DimensionsToWorldSpace(Dimensions);
#else
	return FVector::ZeroVector;
#endif
}

FVector UOculusRiftBoundaryComponent::GetOuterBoundaryDimensions()
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	ovrVector3f Dimensions = { 0.f, 0.f, 0.f };
	ovr_GetBoundaryDimensions(Session, ovrBoundary_Outer, &Dimensions);

	return DimensionsToWorldSpace(Dimensions);
#else
	return FVector::ZeroVector;
#endif
}

FBoundaryTestResult UOculusRiftBoundaryComponent::CheckIfPointWithinPlayArea(const FVector Point)
{
	FBoundaryTestResult PlayAreaInteractionInfo;
	memset(&PlayAreaInteractionInfo, 0, sizeof(FBoundaryTestResult));
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	PlayAreaInteractionInfo = CheckPointInBounds(Session, EBoundaryType::Boundary_PlayArea, Point);
#endif
	return PlayAreaInteractionInfo;
}

FBoundaryTestResult UOculusRiftBoundaryComponent::CheckIfPointWithinOuterBounds(const FVector Point)
{
	FBoundaryTestResult BoundaryInteractionInfo;
	memset(&BoundaryInteractionInfo, 0, sizeof(FBoundaryTestResult));
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	BoundaryInteractionInfo = CheckPointInBounds(Session, EBoundaryType::Boundary_Outer, Point);
#endif
	return BoundaryInteractionInfo;
}

bool UOculusRiftBoundaryComponent::RequestOuterBoundaryVisible(bool BoundaryVisible)
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	ovrBool visible = BoundaryVisible ? 1 : 0;
	ovrResult ovrRes = ovr_RequestBoundaryVisible(Session, visible);
	if (OVR_SUCCESS(ovrRes))
	{
		return true;
	}
#endif

	return false;
}

FBoundaryTestResult UOculusRiftBoundaryComponent::GetTriggeredPlayAreaInfo(ETrackedDeviceType DeviceType)
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	ovrTrackedDeviceType ovrDevice = ToOVRTrackedDeviceType(DeviceType);
	ovrBoundaryTestResult TestResult;
	ovrResult ovrRes = ovr_TestBoundary(Session, ovrDevice, ovrBoundary_PlayArea, &TestResult);
#endif

	FBoundaryTestResult InteractionInfo;
	memset(&InteractionInfo, 0, sizeof(FBoundaryTestResult));
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	bool IsTriggering = (TestResult.IsTriggering != 0);

	if (OVR_SUCCESS(ovrRes))
	{
		if (IsTriggering)
		{
			InteractionInfo.IsTriggering = IsTriggering;
			InteractionInfo.DeviceType = ToETrackedDeviceType(ovrDevice);
			InteractionInfo.ClosestDistance = DistanceToWorldSpace(TestResult.ClosestDistance);
			InteractionInfo.ClosestPoint = PointToWorldSpace(TestResult.ClosestPoint);
			InteractionInfo.ClosestPointNormal = NormalToWorldSpace(TestResult.ClosestPointNormal);

		}
	}
#endif

	return InteractionInfo;
}

TArray<FBoundaryTestResult> UOculusRiftBoundaryComponent::GetTriggeredOuterBoundaryInfo()
{
#if OCULUS_RIFT_SUPPORTED_PLATFORMS
	return OuterBoundsInteractionList;
#else
	TArray<FBoundaryTestResult> ReturnValue;
	return ReturnValue;
#endif
}

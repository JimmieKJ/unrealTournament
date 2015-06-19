// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	InstancedStaticMesh.cpp: Static mesh rendering code.
=============================================================================*/

#include "EnginePrivate.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/HierarchicalInstancedStaticMeshComponent.h"
#include "InstancedStaticMesh.h"
#include "NavigationSystemHelpers.h"
#include "AI/Navigation/NavCollision.h"

static TAutoConsoleVariable<int32> CVarFoliageSplitFactor(
	TEXT("foliage.SplitFactor"),
	16,
	TEXT("This controls the branching factor of the foliage tree."));

static TAutoConsoleVariable<int32> CVarForceLOD(
	TEXT("foliage.ForceLOD"),
	-1,
	TEXT("If greater than or equal to zero, forces the foliage LOD to that level."));

static TAutoConsoleVariable<int32> CVarOnlyLOD(
	TEXT("foliage.OnlyLOD"),
	-1,
	TEXT("If greater than or equal to zero, only renders the foliage LOD at that level."));

static TAutoConsoleVariable<int32> CVarDisableCull(
	TEXT("foliage.DisableCull"),
	0,
	TEXT("If greater than zero, no culling occurs based on frustum."));

static TAutoConsoleVariable<int32> CVarCullAll(
	TEXT("foliage.CullAll"),
	0,
	TEXT("If greater than zero, everything is conside3red culled."));

static TAutoConsoleVariable<int32> CVarDitheredLOD(
	TEXT("foliage.DitheredLOD"),
	1,
	TEXT("If greater than zero, dithered LOD is used, otherwise popping LOD is used."));

static TAutoConsoleVariable<int32> CVarOverestimateLOD(
	TEXT("foliage.OverestimateLOD"),
	0,
	TEXT("If greater than zero and dithered LOD is not used, then we use an overestimate of LOD instead of an underestimate."));

static TAutoConsoleVariable<int32> CVarMaxTrianglesToRender(
	TEXT("foliage.MaxTrianglesToRender"),
	100000000,
	TEXT("This is an absolute limit on the number of foliage triangles to render in one traversal. This is used to prevent a silly LOD parameter mistake from causing the OS to kill the GPU."));

TAutoConsoleVariable<float> CVarFoliageMinimumScreenSize(
	TEXT("foliage.MinimumScreenSize"),
	0.000005f,
	TEXT("This controls the screen size at which we cull foliage instances entirely."));

TAutoConsoleVariable<float> CVarFoliageLODDistanceScale(
	TEXT("foliage.LODDistanceScale"),
	1.0f,
	TEXT("Scale factor for the distance used in computing LOD for foliage."));

TAutoConsoleVariable<float> CVarRandomLODRange(
	TEXT("foliage.RandomLODRange"),
	0.0f,
	TEXT("Random distance added to each instance distance to compute LOD."));

static TAutoConsoleVariable<int32> CVarASyncInstaneBufferConversion(
	TEXT("foliage.ASyncInstaneBufferConversion"),
	1,
	TEXT("If this is > 0, then build game instance buffering async during streaming. This is not thought to be a long term solution to this problem."));

static TAutoConsoleVariable<int32> CVarMinVertsToSplitNode(
	TEXT("foliage.MinVertsToSplitNode"),
	16384,
	TEXT("Controls the accuracy between culling and LOD accuracy and culling and CPU performance."));

static TAutoConsoleVariable<int32> CVarMaxOcclusionQueriesPerComponent(
	TEXT("foliage.MaxOcclusionQueriesPerComponent"),
	16,
	TEXT("Controls the granualrity of occlusion culling. 16-128 is a reasonable range."));

static TAutoConsoleVariable<int32> CVarMinInstancesPerOcclusionQuery(
	TEXT("foliage.MinInstancesPerOcclusionQuery"),
	256,
	TEXT("Controls the granualrity of occlusion culling. 1024 to 65536 is a reasonable range. This is not exact, actual minimum might be off by a factor of two."));

DECLARE_CYCLE_STAT(TEXT("Traversal Time"),STAT_FoliageTraversalTime,STATGROUP_Foliage);
DECLARE_CYCLE_STAT(TEXT("Build Time"), STAT_FoliageBuildTime, STATGROUP_Foliage);
DECLARE_CYCLE_STAT(TEXT("Batch Time"),STAT_FoliageBatchTime,STATGROUP_Foliage);
DECLARE_DWORD_COUNTER_STAT(TEXT("Runs"), STAT_FoliageRuns, STATGROUP_Foliage);
DECLARE_DWORD_COUNTER_STAT(TEXT("Mesh Batches"), STAT_FoliageMeshBatches, STATGROUP_Foliage);
DECLARE_DWORD_COUNTER_STAT(TEXT("Triangles"), STAT_FoliageTriangles, STATGROUP_Foliage);
DECLARE_DWORD_COUNTER_STAT(TEXT("Instances"), STAT_FoliageInstances, STATGROUP_Foliage);
DECLARE_DWORD_COUNTER_STAT(TEXT("Occlusion Culled Instances"), STAT_OcclusionCulledFoliageInstances, STATGROUP_Foliage);
DECLARE_DWORD_COUNTER_STAT(TEXT("Traversals"),STAT_FoliageTraversals,STATGROUP_Foliage);
DECLARE_MEMORY_STAT(TEXT("Instance Buffers"),STAT_FoliageInstanceBuffers,STATGROUP_Foliage);

struct FClusterTree
{
	TArray<FClusterNode> Nodes;
	TArray<int32> SortedInstances;
	TArray<int32> InstanceReorderTable;
	int32 OutOcclusionLayerNum;
};


class FClusterBuilder
{
	int32 Num;
	FBox InstBox;
	int32 BranchingFactor;
	int32 InternalNodeBranchingFactor;
	int32 OcclusionLayerTarget;
	int32 MaxInstancesPerLeaf;
	int32 NumRoots;
	TArray<int32> SortIndex;
	TArray<FVector> SortPoints;
	TArray<FMatrix> Transforms;

	struct FRunPair
	{
		int32 Start;
		int32 Num;

		FRunPair(int32 InStart, int32 InNum)
			: Start(InStart)
			, Num(InNum)
		{
		}

		bool operator< (const FRunPair& Other) const
		{
			return Start < Other.Start;
		}
	};
	TArray<FRunPair> Clusters;

	struct FSortPair
	{
		float d;
		int32 Index;

		bool operator< (const FSortPair& Other) const
		{
			return d < Other.d;
		}
	};
	TArray<FSortPair> SortPairs;

	void Split(int32 InNum)
	{
		check(InNum);
		Clusters.Reset();
		Split(0, InNum - 1);
		Clusters.Sort();
		check(Clusters.Num());
		int32 At = 0;
		for (auto& Cluster : Clusters)
		{
			check(At == Cluster.Start);
			At += Cluster.Num;
		}
		check(At == InNum);
	}

	void Split(int32 Start, int32 End)
	{
		int32 NumRange = 1 + End - Start;
		FBox ClusterBounds(ForceInit);
		for (int32 Index = Start; Index <= End; Index++)
		{
			ClusterBounds += SortPoints[SortIndex[Index]];
		}
		if (NumRange <= BranchingFactor)
		{
			Clusters.Add(FRunPair(Start, NumRange));
			return;
		}
		check(NumRange >= 2);
		SortPairs.Reset();
		int32 BestAxis = -1;
		float BestAxisValue = -1.0f;
		for (int32 Axis = 0; Axis < 3; Axis++)
		{
			float ThisAxisValue = ClusterBounds.Max[Axis] - ClusterBounds.Min[Axis];
			if (!Axis || ThisAxisValue > BestAxisValue)
			{
				BestAxis = Axis;
				BestAxisValue = ThisAxisValue;
			}
		}
		for (int32 Index = Start; Index <= End; Index++)
		{
			FSortPair Pair;

			Pair.Index = SortIndex[Index];
			Pair.d = SortPoints[Pair.Index][BestAxis];
			SortPairs.Add(Pair);
		}
		SortPairs.Sort();
		for (int32 Index = Start; Index <= End; Index++)
		{
			SortIndex[Index] = SortPairs[Index - Start].Index;
		}

		int32 Half = NumRange / 2;

		int32 EndLeft = Start + Half - 1;
		int32 StartRight = 1 + End - Half;

		if (NumRange & 1)
		{
			if (SortPairs[Half].d - SortPairs[Half - 1].d < SortPairs[Half + 1].d - SortPairs[Half].d)
			{
				EndLeft++;
			}
			else
			{
				StartRight--;
			}
		}
		check(EndLeft + 1 == StartRight);
		check(EndLeft >= Start);
		check(End >= StartRight);

		Split(Start, EndLeft);
		Split(StartRight, End);

	}
public:
	FClusterTree* Result;

	FClusterBuilder(TArray<FMatrix>& InTransforms, const FBox& InInstBox, int32 InMaxInstancesPerLeaf)
		: Num(InTransforms.Num())
		, InstBox(InInstBox)
		, Result(nullptr)
	{
		OcclusionLayerTarget = CVarMaxOcclusionQueriesPerComponent.GetValueOnAnyThread();
		int32 MinInstancesPerOcclusionQuery = CVarMinInstancesPerOcclusionQuery.GetValueOnAnyThread();

		if (Num / MinInstancesPerOcclusionQuery < OcclusionLayerTarget)
		{
			OcclusionLayerTarget = Num / MinInstancesPerOcclusionQuery;
			if (OcclusionLayerTarget < 6)
			{
				OcclusionLayerTarget = 0;
			}
		}
		InternalNodeBranchingFactor = CVarFoliageSplitFactor.GetValueOnAnyThread();
		MaxInstancesPerLeaf = InMaxInstancesPerLeaf;

		Exchange(Transforms, InTransforms);
		SortIndex.AddUninitialized(Num);
		SortPoints.AddUninitialized(Num);
		for (int32 Index = 0; Index < Num; Index++)
		{
			SortIndex[Index] = Index;
			SortPoints[Index] = Transforms[Index].GetOrigin();
		}
	}

	void BuildAsync(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent)
	{
		Build();
		//FPlatformProcess::Sleep(5);
	}

	void Build()
	{
		Result = new FClusterTree;
		bool bIsOcclusionLayer = false;
		BranchingFactor = MaxInstancesPerLeaf;
		if (BranchingFactor > 2 && OcclusionLayerTarget && Num / BranchingFactor <= OcclusionLayerTarget)
		{
			BranchingFactor = FMath::Max<int32>(2, (Num + OcclusionLayerTarget - 1) / OcclusionLayerTarget);
			OcclusionLayerTarget = 0;
			bIsOcclusionLayer = true;
		}
		Split(Num);
		if (bIsOcclusionLayer)
		{
			Result->OutOcclusionLayerNum = Clusters.Num();
			bIsOcclusionLayer = false;
		}

		TArray<int32>& SortedInstances = Result->SortedInstances;
		SortedInstances.AddUninitialized(Num);
		for (int32 Index = 0; Index < Num; Index++)
		{
			SortedInstances[Index] = SortIndex[Index];
		}
		NumRoots = Clusters.Num();
		Result->Nodes.Reserve(Clusters.Num());
		for (int32 Index = 0; Index < NumRoots; Index++)
		{
			Result->Nodes.Add(FClusterNode());
		}

		for (int32 Index = 0; Index < NumRoots; Index++)
		{
			FClusterNode& Node = Result->Nodes[Index];
			Node.FirstInstance = Clusters[Index].Start;
			Node.LastInstance = Clusters[Index].Start + Clusters[Index].Num - 1;
			FBox NodeBox(ForceInit);
			for (int32 InstanceIndex = Node.FirstInstance; InstanceIndex <= Node.LastInstance; InstanceIndex++)
			{
				const FMatrix& ThisInstTrans = Transforms[SortedInstances[InstanceIndex]];
				FBox ThisInstBox = InstBox.TransformBy(ThisInstTrans);
				NodeBox += ThisInstBox;
			}
			Node.BoundMin = NodeBox.Min;
			Node.BoundMax = NodeBox.Max;
		}
		TArray<int32> NodesPerLevel;
		NodesPerLevel.Add(NumRoots);
		int32 LOD = 0;

		TArray<int32> InverseSortIndex;
		TArray<int32> RemapSortIndex;
		TArray<int32> InverseInstanceIndex;
		TArray<int32> OldInstanceIndex;
		TArray<int32> LevelStarts;
		TArray<int32> InverseChildIndex;
		TArray<FClusterNode> OldNodes;

		while (NumRoots > 1)
		{
			SortIndex.Reset();
			SortPoints.Reset();
			SortIndex.AddUninitialized(NumRoots);
			SortPoints.AddUninitialized(NumRoots);
			for (int32 Index = 0; Index < NumRoots; Index++)
			{
				SortIndex[Index] = Index;
				FClusterNode& Node = Result->Nodes[Index];
				SortPoints[Index] = (Node.BoundMin + Node.BoundMax) * 0.5f;
			}
			BranchingFactor = InternalNodeBranchingFactor;
			if (BranchingFactor > 2 && OcclusionLayerTarget && NumRoots / BranchingFactor <= OcclusionLayerTarget)
			{
				BranchingFactor = FMath::Max<int32>(2, (NumRoots + OcclusionLayerTarget - 1) / OcclusionLayerTarget);
				OcclusionLayerTarget = 0;
				bIsOcclusionLayer = true;
			}
			Split(NumRoots);
			if (bIsOcclusionLayer)
			{
				Result->OutOcclusionLayerNum = Clusters.Num();
				bIsOcclusionLayer = false;
			}

			InverseSortIndex.Reset();
			InverseSortIndex.AddUninitialized(NumRoots);
			for (int32 Index = 0; Index < NumRoots; Index++)
			{
				InverseSortIndex[SortIndex[Index]] = Index;
			}

			{
				// rearrange the instances to match the new order of the old roots
				RemapSortIndex.Reset();
				RemapSortIndex.AddUninitialized(Num);
				int32 OutIndex = 0;
				for (int32 Index = 0; Index < NumRoots; Index++)
				{
					FClusterNode& Node = Result->Nodes[SortIndex[Index]];
					for (int32 InstanceIndex = Node.FirstInstance; InstanceIndex <= Node.LastInstance; InstanceIndex++)
					{
						RemapSortIndex[OutIndex++] = InstanceIndex;
					}
				}
				InverseInstanceIndex.Reset();
				InverseInstanceIndex.AddUninitialized(Num);
				for (int32 Index = 0; Index < Num; Index++)
				{
					InverseInstanceIndex[RemapSortIndex[Index]] = Index;
				}
				for (int32 Index = 0; Index < Result->Nodes.Num(); Index++)
				{
					FClusterNode& Node = Result->Nodes[Index];
					Node.FirstInstance = InverseInstanceIndex[Node.FirstInstance];
					Node.LastInstance = InverseInstanceIndex[Node.LastInstance];
				}
				OldInstanceIndex.Reset();
				Exchange(OldInstanceIndex, SortedInstances);
				SortedInstances.AddUninitialized(Num);
				for (int32 Index = 0; Index < Num; Index++)
				{
					SortedInstances[Index] = OldInstanceIndex[RemapSortIndex[Index]];
				}
			}
			{
				// rearrange the nodes to match the new order of the old roots
				RemapSortIndex.Reset();
				int32 NewNum = Result->Nodes.Num() + Clusters.Num();
				// RemapSortIndex[new index] == old index
				RemapSortIndex.AddUninitialized(NewNum);
				LevelStarts.Reset();
				LevelStarts.Add(Clusters.Num());
				for (int32 Index = 0; Index < NodesPerLevel.Num() - 1; Index++)
				{
					LevelStarts.Add(LevelStarts[Index] + NodesPerLevel[Index]);
				}

				for (int32 Index = 0; Index < NumRoots; Index++)
				{
					FClusterNode& Node = Result->Nodes[SortIndex[Index]];
					RemapSortIndex[LevelStarts[0]++] = SortIndex[Index];

					int32 LeftIndex = Node.FirstChild;
					int32 RightIndex = Node.LastChild;
					int32 LevelIndex = 1;
					while (RightIndex >= 0)
					{
						int32 NextLeftIndex = MAX_int32;
						int32 NextRightIndex = -1;
						for (int32 ChildIndex = LeftIndex; ChildIndex <= RightIndex; ChildIndex++)
						{
							RemapSortIndex[LevelStarts[LevelIndex]++] = ChildIndex;
							int32 LeftChild = Result->Nodes[ChildIndex].FirstChild;
							int32 RightChild = Result->Nodes[ChildIndex].LastChild;
							if (LeftChild >= 0 && LeftChild <  NextLeftIndex)
							{
								NextLeftIndex = LeftChild;
							}
							if (RightChild >= 0 && RightChild >  NextRightIndex)
							{
								NextRightIndex = RightChild;
							}
						}
						LeftIndex = NextLeftIndex;
						RightIndex = NextRightIndex;
						LevelIndex++;
					}
				}
				check(LevelStarts[LevelStarts.Num() - 1] == NewNum);
				InverseChildIndex.Reset();
				// InverseChildIndex[old index] == new index
				InverseChildIndex.AddUninitialized(NewNum);
				for (int32 Index = Clusters.Num(); Index < NewNum; Index++)
				{
					InverseChildIndex[RemapSortIndex[Index]] = Index;
				}
				for (int32 Index = 0; Index < Result->Nodes.Num(); Index++)
				{
					FClusterNode& Node = Result->Nodes[Index];
					if (Node.FirstChild >= 0)
					{
						Node.FirstChild = InverseChildIndex[Node.FirstChild];
						Node.LastChild = InverseChildIndex[Node.LastChild];
					}
				}
				{
					Exchange(OldNodes, Result->Nodes);
					Result->Nodes.Empty(NewNum);
					for (int32 Index = 0; Index < Clusters.Num(); Index++)
					{
						Result->Nodes.Add(FClusterNode());
					}
					Result->Nodes.AddUninitialized(OldNodes.Num());
					for (int32 Index = 0; Index < OldNodes.Num(); Index++)
					{
						Result->Nodes[InverseChildIndex[Index]] = OldNodes[Index];
					}
				}
				int32 OldIndex = Clusters.Num();
				int32 InstanceTracker = 0;
				for (int32 Index = 0; Index < Clusters.Num(); Index++)
				{
					FClusterNode& Node = Result->Nodes[Index];
					Node.FirstChild = OldIndex;
					OldIndex += Clusters[Index].Num;
					Node.LastChild = OldIndex - 1;
					Node.FirstInstance = Result->Nodes[Node.FirstChild].FirstInstance;
					check(Node.FirstInstance == InstanceTracker);
					Node.LastInstance = Result->Nodes[Node.LastChild].LastInstance;
					InstanceTracker = Node.LastInstance + 1;
					check(InstanceTracker <= Num);
					FBox NodeBox(ForceInit);
					for (int32 ChildIndex = Node.FirstChild; ChildIndex <= Node.LastChild; ChildIndex++)
					{
						FClusterNode& ChildNode = Result->Nodes[ChildIndex];
						NodeBox += ChildNode.BoundMin;
						NodeBox += ChildNode.BoundMax;
					}
					Node.BoundMin = NodeBox.Min;
					Node.BoundMax = NodeBox.Max;
				}
				NumRoots = Clusters.Num();
				NodesPerLevel.Insert(NumRoots, 0);
			}
		}

		// Save inverse map
		Result->InstanceReorderTable.AddUninitialized(Num);
		for (int32 Index = 0; Index < Num; Index++)
		{
			Result->InstanceReorderTable[SortedInstances[Index]] = Index;
		}
	}

};


static bool PrintLevel(const FClusterTree& Tree, int32 NodeIndex, int32 Level, int32 CurrentLevel, int32 Parent)
{
	const FClusterNode& Node = Tree.Nodes[NodeIndex];
	if (Level == CurrentLevel)
	{
		UE_LOG(LogConsoleResponse, Display, TEXT("Level %2d  Parent %3d"),
			Level,
			Parent
			);
		FVector Extent = Node.BoundMax - Node.BoundMin;
		UE_LOG(LogConsoleResponse, Display, TEXT("    Bound (%5.1f, %5.1f, %5.1f) [(%5.1f, %5.1f, %5.1f) - (%5.1f, %5.1f, %5.1f)]"),
			Extent.X, Extent.Y, Extent.Z, 
			Node.BoundMin.X, Node.BoundMin.Y, Node.BoundMin.Z, 
			Node.BoundMax.X, Node.BoundMax.Y, Node.BoundMax.Z
			);
		UE_LOG(LogConsoleResponse, Display, TEXT("    children %3d [%3d,%3d]   instances %3d [%3d,%3d]"),
			(Node.FirstChild < 0) ? 0 : 1 + Node.LastChild - Node.FirstChild, Node.FirstChild, Node.LastChild,
			1 + Node.LastInstance - Node.FirstInstance, Node.FirstInstance, Node.LastInstance
			);
		return true;
	}
	else if (Node.FirstChild < 0)
	{
		return false;
	}
	bool Ret = false;
	for (int32 Child = Node.FirstChild; Child <= Node.LastChild; Child++)
	{
		Ret = PrintLevel(Tree, Child, Level, CurrentLevel + 1, NodeIndex) || Ret;
	}
	return Ret;
}

static void TestFoliage(const TArray<FString>& Args)
{
	UE_LOG(LogConsoleResponse, Display, TEXT("Running Foliage test."));
	TArray<FInstancedStaticMeshInstanceData> Instances;

	FMatrix Temp;
	Temp.SetIdentity();
	FRandomStream RandomStream(0x238946);
	for (int32 i = 0; i < 1000; i++)
	{
		Instances.Add(FInstancedStaticMeshInstanceData());
		Temp.SetOrigin(FVector(RandomStream.FRandRange(0.0f, 1.0f), RandomStream.FRandRange(0.0f, 1.0f), 0.0f) * 10000.0f);
		Instances[i].Transform = Temp;
	}

	FBox TempBox(ForceInit);
	TempBox += FVector(-100.0f, -100.0f, -100.0f);
	TempBox += FVector(100.0f, 100.0f, 100.0f);

	TArray<FMatrix> InstanceTransforms;
	InstanceTransforms.AddUninitialized(Instances.Num());
	for (int32 Index = 0; Index < Instances.Num(); Index++)
	{
		InstanceTransforms[Index] = Instances[Index].Transform;
	}
	FClusterBuilder Builder(InstanceTransforms, TempBox, 16);
	Builder.Build();

	int32 Level = 0;

	UE_LOG(LogConsoleResponse, Display, TEXT("-----"));

	while(PrintLevel(*Builder.Result, 0, Level++, 0, -1))
	{
	}

	delete Builder.Result;
}

static FAutoConsoleCommand TestFoliageCmd(
	TEXT("foliage.Test"),
	TEXT("Useful for debugging."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&TestFoliage)
	);

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	static uint32 GDebugTag = 1;
	static uint32 GCaptureDebugRuns = 0;
#endif

static void FreezeFoliageCulling(const TArray<FString>& Args)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UE_LOG(LogConsoleResponse, Display, TEXT("Freezing Foliage Culling."));
	GDebugTag++;
	GCaptureDebugRuns = GDebugTag;
#endif
}

static FAutoConsoleCommand FreezeFoliageCullingCmd(
	TEXT("foliage.Freeze"),
	TEXT("Useful for debugging. Freezes the foliage culling and LOD."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&FreezeFoliageCulling)
	);

static void UnFreezeFoliageCulling(const TArray<FString>& Args)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UE_LOG(LogConsoleResponse, Display, TEXT("Unfreezing Foliage Culling."));
	GDebugTag++;
	GCaptureDebugRuns = 0;
#endif
}

static FAutoConsoleCommand UnFreezeFoliageCullingCmd(
	TEXT("foliage.UnFreeze"),
	TEXT("Useful for debugging. Freezes the foliage culling and LOD."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&UnFreezeFoliageCulling)
	);


struct FFoliageOcclusionResults
{
	const bool* Results;
	int32 NumResults;
	uint32 FrameNumberRenderThread;

	FFoliageOcclusionResults(const bool* InResults, int32 InNumResults)
		: Results(InResults)
		, NumResults(InNumResults)
		, FrameNumberRenderThread(GFrameNumberRenderThread)
	{

	}
};

struct FFoliageElementParams;
struct FFoliageRenderInstanceParams;
struct FFoliageCullInstanceParams;

class FHierarchicalStaticMeshSceneProxy : public FInstancedStaticMeshSceneProxy
{
	TSharedPtr<TArray<FClusterNode>, ESPMode::ThreadSafe> ClusterTreePtr;
	const TArray<FClusterNode>& ClusterTree;

	int32 FirstUnbuiltIndex;
	int32 LastUnbuiltIndex;

	int32 FirstOcclusionNode;
	int32 LastOcclusionNode;
	TArray<FBoxSphereBounds> OcclusionBounds;
	TMap<uint32, FFoliageOcclusionResults> OcclusionResults;
	bool bIsGrass;

#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	mutable TArray<uint32> SingleDebugRuns[MAX_STATIC_MESH_LODS];
	mutable int32 SingleDebugTotalInstances[MAX_STATIC_MESH_LODS];
	mutable TArray<uint32> MultipleDebugRuns[MAX_STATIC_MESH_LODS];
	mutable int32 MultipleDebugTotalInstances[MAX_STATIC_MESH_LODS];
	mutable int32 CaptureTag;
#endif

public:

	FHierarchicalStaticMeshSceneProxy(bool bInIsGrass, UHierarchicalInstancedStaticMeshComponent* InComponent, ERHIFeatureLevel::Type InFeatureLevel)
		: FInstancedStaticMeshSceneProxy(InComponent, InFeatureLevel)
		, ClusterTreePtr(InComponent->ClusterTreePtr)
		, ClusterTree(*InComponent->ClusterTreePtr)
		, FirstUnbuiltIndex(InComponent->NumBuiltInstances)
		, LastUnbuiltIndex(InComponent->PerInstanceSMData.Num()+InComponent->RemovedInstances.Num()-1)
		, bIsGrass(bInIsGrass)
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		, CaptureTag(0)
#endif
	{
		check(InComponent->InstanceReorderTable.Num() == InComponent->PerInstanceSMData.Num());
		SetupOcclusion(InComponent);
	}

	FHierarchicalStaticMeshSceneProxy(bool bInIsGrass, UHierarchicalInstancedStaticMeshComponent* InComponent, ERHIFeatureLevel::Type InFeatureLevel, FStaticMeshInstanceData& Other)
		: FInstancedStaticMeshSceneProxy(InComponent, InFeatureLevel, Other)
		, ClusterTreePtr(InComponent->ClusterTreePtr)
		, ClusterTree(*InComponent->ClusterTreePtr)
		, FirstUnbuiltIndex(InComponent->NumBuiltInstances)
		, LastUnbuiltIndex(InstancedRenderData.NumInstances+InComponent->RemovedInstances.Num()-1)
		, bIsGrass(bInIsGrass)
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
		, CaptureTag(0)
#endif
	{
		check(!bInIsGrass || (!InComponent->InstanceReorderTable.Num() && !InComponent->PerInstanceSMData.Num()));
		SetupOcclusion(InComponent);
	}

	void SetupOcclusion(UHierarchicalInstancedStaticMeshComponent* InComponent)
	{
		FirstOcclusionNode = 0;
		LastOcclusionNode = 0;
		if (ClusterTree.Num() && InComponent->OcclusionLayerNumNodes)
		{
			while (true)
			{
				int32 NextFirstOcclusionNode = ClusterTree[FirstOcclusionNode].FirstChild;
				int32 NextLastOcclusionNode = ClusterTree[LastOcclusionNode].LastChild;

				if (NextFirstOcclusionNode < 0 || NextLastOcclusionNode < 0)
				{
					break;
				}
				int32 NumNodes = 1 + NextLastOcclusionNode - NextFirstOcclusionNode;
				if (NumNodes > InComponent->OcclusionLayerNumNodes)
				{
					break;
				}
				FirstOcclusionNode = NextFirstOcclusionNode;
				LastOcclusionNode = NextLastOcclusionNode;
			}
		}
		int32 NumNodes = 1 + LastOcclusionNode - FirstOcclusionNode;
		if (NumNodes < 2)
		{
			FirstOcclusionNode = -1;
			LastOcclusionNode = -1;
			NumNodes = 0;
			if (ClusterTree.Num())
			{
				//UE_LOG(LogTemp, Display, TEXT("No SubOcclusion %d inst"), 1 + ClusterTree[0].LastInstance - ClusterTree[0].FirstInstance);
			}
		}
		else
		{
			//int32 NumPerNode = (1 + ClusterTree[0].LastInstance - ClusterTree[0].FirstInstance) / NumNodes;
			//UE_LOG(LogTemp, Display, TEXT("Occlusion level %d   %d inst / node"), NumNodes, NumPerNode);
			OcclusionBounds.Reserve(NumNodes);
			FMatrix XForm = InComponent->ComponentToWorld.ToMatrixWithScale();
			for (int32 Index = FirstOcclusionNode; Index <= LastOcclusionNode; Index++)
			{
				OcclusionBounds.Add(FBoxSphereBounds(FBox(ClusterTree[Index].BoundMin, ClusterTree[Index].BoundMax).TransformBy(XForm)));
			}
		}
	}

	// FPrimitiveSceneProxy interface.
	
	virtual FPrimitiveViewRelevance GetViewRelevance(const FSceneView* View) override
	{
		FPrimitiveViewRelevance Result;
		if (bIsGrass ? View->Family->EngineShowFlags.InstancedGrass : View->Family->EngineShowFlags.InstancedFoliage)
		{
			Result = FStaticMeshSceneProxy::GetViewRelevance(View);
			Result.bDynamicRelevance = true;
			Result.bStaticRelevance = false;
		}
		return Result;
	}

	virtual void GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const override;

	virtual const TArray<FBoxSphereBounds>* GetOcclusionQueries(const FSceneView* View) const override;
	virtual void AcceptOcclusionResults(const FSceneView* View, const bool* Results, int32 NumResults) override;
	virtual bool HasSubprimitiveOcclusionQueries() const override
	{
		return FirstOcclusionNode > 0;
	}


	virtual void DrawStaticElements(FStaticPrimitiveDrawInterface* PDI) override
	{
	}

	void FillDynamicMeshElements(FMeshElementCollector& Collector, const FFoliageElementParams& ElementParams, const FFoliageRenderInstanceParams& Instances) const;

	template<bool TUseVector>
	void Traverse(const FFoliageCullInstanceParams& Params, int32 Index, int32 MinLOD, int32 MaxLOD, bool bFullyContained = false) const;
};

struct FFoliageRenderInstanceParams
{
	bool bNeedsSingleLODRuns;
	bool bNeedsMultipleLODRuns;
	bool bOverestimate;
	mutable TArray<uint32, SceneRenderingAllocator> MultipleLODRuns[MAX_STATIC_MESH_LODS];
	mutable TArray<uint32, SceneRenderingAllocator> SingleLODRuns[MAX_STATIC_MESH_LODS];
	mutable int32 TotalSingleLODInstances[MAX_STATIC_MESH_LODS];
	mutable int32 TotalMultipleLODInstances[MAX_STATIC_MESH_LODS];

	FFoliageRenderInstanceParams(bool InbNeedsSingleLODRuns, bool InbNeedsMultipleLODRuns, bool InbOverestimate)
		: bNeedsSingleLODRuns(InbNeedsSingleLODRuns)
		, bNeedsMultipleLODRuns(InbNeedsMultipleLODRuns)
		, bOverestimate(InbOverestimate)
	{
		for (int32 Index = 0; Index < MAX_STATIC_MESH_LODS; Index++)
		{
			TotalSingleLODInstances[Index] = 0;
			TotalMultipleLODInstances[Index] = 0;
		}
	}
	static FORCEINLINE_DEBUGGABLE void AddRun(TArray<uint32, SceneRenderingAllocator>& Array, int32 FirstInstance, int32 LastInstance)
	{
		if (Array.Num() && Array.Last() + 1 == FirstInstance)
		{
			Array.Last() = (uint32)LastInstance;
		}
		else
		{
			Array.Add((uint32)FirstInstance);
			Array.Add((uint32)LastInstance);
		}
	}
	FORCEINLINE_DEBUGGABLE void AddRun(int32 MinLod, int32 MaxLod, int32 FirstInstance, int32 LastInstance) const
	{
		if (bNeedsSingleLODRuns)
		{
			AddRun(SingleLODRuns[bOverestimate ? MaxLod : MinLod], FirstInstance, LastInstance);
			TotalSingleLODInstances[bOverestimate ? MaxLod : MinLod] += 1 + LastInstance - FirstInstance;
		}
		if (bNeedsMultipleLODRuns)
		{
			for (int32 Lod = MinLod; Lod <= MaxLod; Lod++)
			{
				TotalMultipleLODInstances[Lod] += 1 + LastInstance - FirstInstance;
				AddRun(MultipleLODRuns[Lod], FirstInstance, LastInstance);
			}
		}
	}

	FORCEINLINE_DEBUGGABLE void AddRun(int32 MinLod, int32 MaxLod, const FClusterNode& Node) const
	{
		AddRun(MinLod, MaxLod, Node.FirstInstance, Node.LastInstance);
	}
};

struct FFoliageCullInstanceParams : public FFoliageRenderInstanceParams
{
	FConvexVolume ViewFrustumLocal;
	int32 MinInstancesToSplit[MAX_STATIC_MESH_LODS];
	const TArray<FClusterNode>& Tree;
	const FSceneView* View;
	FVector ViewOriginInLocalZero;
	FVector ViewOriginInLocalOne;
	int32 LODs;
	float LODPlanesMax[MAX_STATIC_MESH_LODS];
	float LODPlanesMin[MAX_STATIC_MESH_LODS];
	int32 FirstOcclusionNode;
	int32 LastOcclusionNode;
	const bool* OcclusionResults;



	FFoliageCullInstanceParams(bool InbNeedsSingleLODRuns, bool InbNeedsMultipleLODRuns, bool InbOverestimate, const TArray<FClusterNode>& InTree)
	:	FFoliageRenderInstanceParams(InbNeedsSingleLODRuns, InbNeedsMultipleLODRuns, InbOverestimate)
	,	Tree(InTree)
	,	FirstOcclusionNode(-1)
	,	LastOcclusionNode(-1)
	,	OcclusionResults(nullptr)
	{
	}
};

static bool GUseVectorCull = true;

static void ToggleUseVectorCull(const TArray<FString>& Args)
{
	GUseVectorCull = !GUseVectorCull;
}

static FAutoConsoleCommand ToggleUseVectorCullCmd(
	TEXT("foliage.ToggleVectorCull"),
	TEXT("Useful for debugging. Toggles the optimized cull."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&ToggleUseVectorCull)
	);

static uint32 GFrameNumberRenderThread_CaptureFoliageRuns = MAX_uint32;

static void LogFoliageFrame(const TArray<FString>& Args)
{
	GFrameNumberRenderThread_CaptureFoliageRuns = GFrameNumberRenderThread + 2;
}

static FAutoConsoleCommand LogFoliageFrameCmd(
	TEXT("foliage.LogFoliageFrame"),
	TEXT("Useful for debugging. Logs all foliage rendered in a frame."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&LogFoliageFrame)
	);

const VectorRegister		VECTOR_HALF_HALF_HALF_ZERO				= DECLARE_VECTOR_REGISTER(0.5f, 0.5f, 0.5f, 0.0f);

template<bool TUseVector>
static FORCEINLINE_DEBUGGABLE bool CullNode(const FFoliageCullInstanceParams& Params, const FClusterNode& Node, bool& bOutFullyContained)
{
	if (TUseVector)
	{
		checkSlow(Params.ViewFrustumLocal.PermutedPlanes.Num() == 4);

		//@todo, once we have more than one mesh per tree, these should be aligned
		VectorRegister BoxMin = VectorLoadFloat3(&Node.BoundMin);
		VectorRegister BoxMax = VectorLoadFloat3(&Node.BoundMax);

		VectorRegister BoxDiff = VectorSubtract(BoxMax,BoxMin);
		VectorRegister BoxSum = VectorAdd(BoxMax,BoxMin);

		// Load the origin & extent
		VectorRegister Orig = VectorMultiply(VECTOR_HALF_HALF_HALF_ZERO, BoxSum);
		VectorRegister Ext = VectorMultiply(VECTOR_HALF_HALF_HALF_ZERO, BoxDiff);
		// Splat origin into 3 vectors
		VectorRegister OrigX = VectorReplicate(Orig, 0);
		VectorRegister OrigY = VectorReplicate(Orig, 1);
		VectorRegister OrigZ = VectorReplicate(Orig, 2);
		// Splat the abs for the pushout calculation
		VectorRegister AbsExtentX = VectorReplicate(Ext, 0);
		VectorRegister AbsExtentY = VectorReplicate(Ext, 1);
		VectorRegister AbsExtentZ = VectorReplicate(Ext, 2);
		// Since we are moving straight through get a pointer to the data
		const FPlane* RESTRICT PermutedPlanePtr = (FPlane*)Params.ViewFrustumLocal.PermutedPlanes.GetData();
		// Process four planes at a time until we have < 4 left
		// Load 4 planes that are already all Xs, Ys, ...
		VectorRegister PlanesX = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesY = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesZ = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		VectorRegister PlanesW = VectorLoadAligned(PermutedPlanePtr);
		PermutedPlanePtr++;
		// Calculate the distance (x * x) + (y * y) + (z * z) - w
		VectorRegister DistX = VectorMultiply(OrigX,PlanesX);
		VectorRegister DistY = VectorMultiplyAdd(OrigY,PlanesY,DistX);
		VectorRegister DistZ = VectorMultiplyAdd(OrigZ,PlanesZ,DistY);
		VectorRegister Distance = VectorSubtract(DistZ,PlanesW);
		// Now do the push out FMath::Abs(x * x) + FMath::Abs(y * y) + FMath::Abs(z * z)
		VectorRegister PushX = VectorMultiply(AbsExtentX,VectorAbs(PlanesX));
		VectorRegister PushY = VectorMultiplyAdd(AbsExtentY,VectorAbs(PlanesY),PushX);
		VectorRegister PushOut = VectorMultiplyAdd(AbsExtentZ,VectorAbs(PlanesZ),PushY);
		VectorRegister PushOutNegative = VectorNegate(PushOut);

		bOutFullyContained = !VectorAnyGreaterThan(Distance,PushOutNegative);
		// Check for completely outside
		return !!VectorAnyGreaterThan(Distance,PushOut);
	}
	FVector Center = (Node.BoundMin + Node.BoundMax) * 0.5f;
	FVector Extent = (Node.BoundMax - Node.BoundMin) * 0.5f;
	if (!Params.ViewFrustumLocal.IntersectBox(Center, Extent, bOutFullyContained)) 
	{
		return true;
	}
	return false;
}

template<bool TUseVector>
void FHierarchicalStaticMeshSceneProxy::Traverse(const FFoliageCullInstanceParams& Params, int32 Index, int32 MinLOD, int32 MaxLOD, bool bFullyContained) const
{
	const FClusterNode& Node = Params.Tree[Index];
	if (!bFullyContained)
	{
		if (CullNode<TUseVector>(Params, Node, bFullyContained))
		{
			return;
		}
	}

	if (MinLOD != MaxLOD)
	{
		FVector Center = (Node.BoundMax + Node.BoundMin) * 0.5f;
		float DistCenterZero = FVector::Dist(Center, Params.ViewOriginInLocalZero);
		float DistCenterOne = FVector::Dist(Center, Params.ViewOriginInLocalOne);
		float HalfWidth = FVector::Dist(Node.BoundMax, Node.BoundMin) * 0.5f;
		float NearDot = FMath::Min(DistCenterZero, DistCenterOne) - HalfWidth;
		float FarDot = FMath::Max(DistCenterZero, DistCenterOne) + HalfWidth;

		while (MaxLOD > MinLOD &&  NearDot > Params.LODPlanesMax[MinLOD])
		{
			MinLOD++;
		}
		if (MinLOD >= Params.LODs)
		{
			return;
		}
		while (MaxLOD > MinLOD && FarDot < Params.LODPlanesMin[MaxLOD - 1])
		{
			MaxLOD--;
		}
	}
	if (Index >= Params.FirstOcclusionNode && Index <= Params.LastOcclusionNode)
	{
		if (Params.OcclusionResults[Index - Params.FirstOcclusionNode])
		{
			INC_DWORD_STAT_BY(STAT_OcclusionCulledFoliageInstances, 1 + Node.LastInstance - Node.FirstInstance);
			return;
		}
	}

	bool bSplit = (!bFullyContained || MinLOD < MaxLOD || Index < Params.FirstOcclusionNode) 
		&& Node.FirstChild >= 0 
		&& (1 + Node.LastInstance - Node.FirstInstance) >= Params.MinInstancesToSplit[MinLOD];

	if (!bSplit)
	{
		MaxLOD = FMath::Min(MaxLOD, Params.LODs - 1);
		Params.AddRun(MinLOD, MaxLOD, Node);
		return;
	}
	for (int32 ChildIndex = Node.FirstChild; ChildIndex <= Node.LastChild; ChildIndex++)
	{
		Traverse<TUseVector>(Params, ChildIndex, MinLOD, MaxLOD, bFullyContained);
	}
}

struct FFoliageElementParams
{
	const FInstancingUserData* PassUserData[2];
	int32 NumSelectionGroups;
	const FSceneView* View;
	int32 ViewIndex;
	bool bSelectionRenderEnabled;
	bool BatchRenderSelection[2];
	bool bIsWireframe;
	bool bUseHoveredMaterial;
	bool bInstanced;
	bool bBlendLODs;
	ERHIFeatureLevel::Type FeatureLevel;
	bool ShadowFrustum;
	float FinalCullDistance;
};

void FHierarchicalStaticMeshSceneProxy::FillDynamicMeshElements(FMeshElementCollector& Collector, const FFoliageElementParams& ElementParams, const FFoliageRenderInstanceParams& Params) const
{
	SCOPE_CYCLE_COUNTER(STAT_FoliageBatchTime);
	int64 TotalTriangles = 0;

	int32 OnlyLOD = FMath::Min<int32>(CVarOnlyLOD.GetValueOnRenderThread(),InstancedRenderData.VertexFactories.Num() - 1);
	int32 FirstLOD = (OnlyLOD < 0) ? 0 : OnlyLOD;
	int32 LastLODPlusOne = (OnlyLOD < 0) ? InstancedRenderData.VertexFactories.Num() : (OnlyLOD+1);

	for (int32 LODIndex = FirstLOD; LODIndex < LastLODPlusOne; LODIndex++)
	{
		const FStaticMeshLODResources& LODModel = StaticMesh->RenderData->LODResources[LODIndex];

		for (int32 SelectionGroupIndex = 0; SelectionGroupIndex < ElementParams.NumSelectionGroups; SelectionGroupIndex++)
		{
			for (int32 SectionIndex = 0; SectionIndex < LODModel.Sections.Num(); SectionIndex++)
			{
				const FLODInfo& ProxyLODInfo = LODs[LODIndex];
				UMaterialInterface* Material = ProxyLODInfo.Sections[SectionIndex].Material;
				const bool bDitherLODEnabled = ElementParams.bBlendLODs;

				TArray<uint32, SceneRenderingAllocator>& RunArray = bDitherLODEnabled ? Params.MultipleLODRuns[LODIndex] : Params.SingleLODRuns[LODIndex];

				if (!RunArray.Num())
				{
					continue;
				}

				int32 NumBatches = 1;
				int32 CurrentRun = 0;
				int32 CurrentInstance = 0;
				int32 RemainingInstances = bDitherLODEnabled ? Params.TotalMultipleLODInstances[LODIndex] : Params.TotalSingleLODInstances[LODIndex];

				if (!ElementParams.bInstanced)
				{
					NumBatches = FMath::DivideAndRoundUp(RemainingInstances, (int32)FInstancedStaticMeshVertexFactory::NumBitsForVisibilityMask());
					if (NumBatches)
					{
						check(RunArray.Num());
						CurrentInstance = RunArray[CurrentRun];
					}
				}

#if STATS
				INC_DWORD_STAT_BY(STAT_FoliageInstances, RemainingInstances);
				if (!ElementParams.bInstanced)
				{
					INC_DWORD_STAT_BY(STAT_FoliageRuns, NumBatches);
				}
#endif
				bool bDidStats = false;
				for (int32 BatchIndex = 0; BatchIndex < NumBatches; BatchIndex++)
				{
					FMeshBatch& MeshElement = Collector.AllocateMesh();
					INC_DWORD_STAT(STAT_FoliageMeshBatches);

					if (!FStaticMeshSceneProxy::GetMeshElement(LODIndex, 0, SectionIndex, GetDepthPriorityGroup(ElementParams.View), ElementParams.BatchRenderSelection[SelectionGroupIndex], ElementParams.bUseHoveredMaterial, MeshElement))
					{
						continue;
					}
					checkSlow(MeshElement.GetNumPrimitives() > 0);

					MeshElement.VertexFactory = &InstancedRenderData.VertexFactories[LODIndex];
					FMeshBatchElement& BatchElement0 = MeshElement.Elements[0];

					BatchElement0.UserData = ElementParams.PassUserData[SelectionGroupIndex];
					BatchElement0.MaxScreenSize = 1.0;
					BatchElement0.MinScreenSize = 0.0;
					BatchElement0.InstancedLODIndex = LODIndex;
					BatchElement0.InstancedLODRange = bDitherLODEnabled ? 1 : 0;
					MeshElement.bCanApplyViewModeOverrides = true;
					MeshElement.bUseSelectionOutline = ElementParams.BatchRenderSelection[SelectionGroupIndex];
					MeshElement.bUseWireframeSelectionColoring = ElementParams.BatchRenderSelection[SelectionGroupIndex];
					MeshElement.bUseAsOccluder = false;

					if (!bDidStats)
					{
						bDidStats = true;
						int64 Tris = int64(RemainingInstances) * int64(BatchElement0.NumPrimitives);
						TotalTriangles += Tris;
#if STATS
						if (GFrameNumberRenderThread_CaptureFoliageRuns == GFrameNumberRenderThread)
						{
							if (ElementParams.FinalCullDistance > 9.9E8)
							{
								UE_LOG(LogStaticMesh, Display, TEXT("lod:%1d/%1d   sel:%1d   section:%1d/%1d   runs:%4d   inst:%8d   tris:%9lld   cast shadow:%1d   cull:-NONE!!-   shadow:%1d     %s %s"), 
									LODIndex, InstancedRenderData.VertexFactories.Num(), SelectionGroupIndex, SectionIndex, LODModel.Sections.Num(), RunArray.Num() / 2, 
									RemainingInstances, Tris, (int)MeshElement.CastShadow, ElementParams.ShadowFrustum,
									*StaticMesh->GetPathName(),
									*MeshElement.MaterialRenderProxy->GetMaterial(ElementParams.FeatureLevel)->GetFriendlyName());
							}
							else
							{
								UE_LOG(LogStaticMesh, Display, TEXT("lod:%1d/%1d   sel:%1d   section:%1d/%1d   runs:%4d   inst:%8d   tris:%9lld   cast shadow:%1d   cull:%8.0f   shadow:%1d     %s %s"), 
									LODIndex, InstancedRenderData.VertexFactories.Num(), SelectionGroupIndex, SectionIndex, LODModel.Sections.Num(), RunArray.Num() / 2, 
									RemainingInstances, Tris, (int)MeshElement.CastShadow, ElementParams.FinalCullDistance, ElementParams.ShadowFrustum,
									*StaticMesh->GetPathName(),
									*MeshElement.MaterialRenderProxy->GetMaterial(ElementParams.FeatureLevel)->GetFriendlyName());
							}
						}
#endif
					}
					if (ElementParams.bInstanced)
					{
						BatchElement0.NumInstances = RunArray.Num() / 2;
						BatchElement0.InstanceRuns = &RunArray[0];
#if STATS
						INC_DWORD_STAT_BY(STAT_FoliageRuns, BatchElement0.NumInstances);
#endif
					}
					else
					{
						uint32 NumInstancesThisBatch = FMath::Min(RemainingInstances, (int32)FInstancedStaticMeshVertexFactory::NumBitsForVisibilityMask());

						MeshElement.Elements.Reserve(NumInstancesThisBatch);
						check(NumInstancesThisBatch);

						for (uint32 Instance = 0; Instance < NumInstancesThisBatch; ++Instance)
						{
							FMeshBatchElement* NewBatchElement; 

							if (Instance == 0)
							{
								NewBatchElement = &MeshElement.Elements[0];
							}
							else
							{
								NewBatchElement = new(MeshElement.Elements) FMeshBatchElement();
								*NewBatchElement = MeshElement.Elements[0];
							}

							NewBatchElement->UserIndex = CurrentInstance;
							if (--RemainingInstances)
							{
								if ((uint32)CurrentInstance >= RunArray[CurrentRun + 1])
								{
									CurrentRun += 2;
									check(CurrentRun + 1 < RunArray.Num());
									CurrentInstance = RunArray[CurrentRun];
								}
								else
								{
									CurrentInstance++;
								}
							}
						}
					}
					if (TotalTriangles < (int64)CVarMaxTrianglesToRender.GetValueOnRenderThread())
					{
						Collector.AddMesh(ElementParams.ViewIndex, MeshElement);
					}
				}
			}
		}
	}
#if STATS
	TotalTriangles = FMath::Min<int64>(TotalTriangles, MAX_int32);
	INC_DWORD_STAT_BY(STAT_FoliageTriangles, (uint32)TotalTriangles);
	INC_DWORD_STAT_BY(STAT_StaticMeshTriangles, (uint32)TotalTriangles);
#endif
}



void FHierarchicalStaticMeshSceneProxy::GetDynamicMeshElements(const TArray<const FSceneView*>& Views, const FSceneViewFamily& ViewFamily, uint32 VisibilityMap, FMeshElementCollector& Collector) const
{
	if (Views[0]->bRenderFirstInstanceOnly)
	{
		FInstancedStaticMeshSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
		return;
	}

	QUICK_SCOPE_CYCLE_COUNTER(STAT_HierarchicalInstancedStaticMeshSceneProxy_GetMeshElements);

	bool bMultipleSections = ALLOW_DITHERED_LOD_FOR_INSTANCED_STATIC_MESHES && CVarDitheredLOD.GetValueOnRenderThread() > 0;
	bool bSingleSections = !bMultipleSections;
	bool bOverestimate = CVarOverestimateLOD.GetValueOnRenderThread() > 0;

	int32 MinVertsToSplitNode = CVarMinVertsToSplitNode.GetValueOnRenderThread();

	for (int32 ViewIndex = 0; ViewIndex < Views.Num(); ViewIndex++)
	{
		if (VisibilityMap & (1 << ViewIndex))
		{
			const FSceneView* View = Views[ViewIndex];

			FFoliageElementParams ElementParams;
			ElementParams.bSelectionRenderEnabled = GIsEditor && ViewFamily.EngineShowFlags.Selection;
			ElementParams.NumSelectionGroups = (ElementParams.bSelectionRenderEnabled && bHasSelectedInstances) ? 2 : 1;
			ElementParams.PassUserData[0] = bHasSelectedInstances && ElementParams.bSelectionRenderEnabled ? &UserData_SelectedInstances : &UserData_AllInstances;
			ElementParams.PassUserData[1] = &UserData_DeselectedInstances;
			ElementParams.BatchRenderSelection[0] = ElementParams.bSelectionRenderEnabled && IsSelected();
			ElementParams.BatchRenderSelection[1] = false;
			ElementParams.bIsWireframe = ViewFamily.EngineShowFlags.Wireframe;
			ElementParams.bUseHoveredMaterial = IsHovered();
			ElementParams.bInstanced = RHISupportsInstancing(GetFeatureLevelShaderPlatform(InstancedRenderData.FeatureLevel));
			ElementParams.FeatureLevel = InstancedRenderData.FeatureLevel;
			ElementParams.ViewIndex = ViewIndex;
			ElementParams.View = View;

			// Render built instances
			if (ClusterTree.Num())
			{

				FFoliageCullInstanceParams InstanceParams(bSingleSections, bMultipleSections, bOverestimate, ClusterTree);
				InstanceParams.LODs = RenderData->LODResources.Num();

				InstanceParams.View = View;

				FMatrix WorldToLocal = GetLocalToWorld().Inverse();
				bool bUseVectorCull = GUseVectorCull;
				bool bIsOrtho = false;

				bool bDisableCull = !!CVarDisableCull.GetValueOnRenderThread();
				ElementParams.ShadowFrustum = !!View->ViewMatrices.GetDynamicMeshElementsShadowCullFrustum;
				if (View->ViewMatrices.GetDynamicMeshElementsShadowCullFrustum)
				{
					for (int32 Index = 0; Index < View->ViewMatrices.GetDynamicMeshElementsShadowCullFrustum->Planes.Num(); Index++)
					{
						FPlane Src = View->ViewMatrices.GetDynamicMeshElementsShadowCullFrustum->Planes[Index];
						FPlane Norm = Src / Src.Size();
						// remove world space preview translation
						Norm.W -= (FVector(Norm) | View->ViewMatrices.PreShadowTranslation);
						FPlane Local = Norm.TransformBy(WorldToLocal);
						FPlane LocalNorm = Local / Local.Size();
						InstanceParams.ViewFrustumLocal.Planes.Add(LocalNorm);
					}
					bUseVectorCull = InstanceParams.ViewFrustumLocal.Planes.Num() == 4;
				}
				else
				{
					FMatrix LocalViewProjForCulling = GetLocalToWorld() * View->ViewMatrices.GetViewProjMatrix();

					GetViewFrustumBounds(InstanceParams.ViewFrustumLocal, LocalViewProjForCulling, false);


					if (View->ViewMatrices.IsPerspectiveProjection())
					{
						InstanceParams.ViewFrustumLocal.Planes.Pop(false); // we don't want the far plane either
						FMatrix ThreePlanes;
						ThreePlanes.SetIdentity();
						ThreePlanes.SetAxes(&InstanceParams.ViewFrustumLocal.Planes[0], &InstanceParams.ViewFrustumLocal.Planes[1], &InstanceParams.ViewFrustumLocal.Planes[2]);
						FVector ProjectionOrigin = ThreePlanes.Inverse().GetTransposed().TransformVector(FVector(InstanceParams.ViewFrustumLocal.Planes[0].W, InstanceParams.ViewFrustumLocal.Planes[1].W, InstanceParams.ViewFrustumLocal.Planes[2].W));

						FVector Forward = LocalViewProjForCulling.GetColumn(3).GetSafeNormal();
						for (int32 Index = 0; Index < InstanceParams.ViewFrustumLocal.Planes.Num(); Index++)
						{
							FPlane Src = InstanceParams.ViewFrustumLocal.Planes[Index];
							FVector Normal = Src.GetSafeNormal();
							InstanceParams.ViewFrustumLocal.Planes[Index] = FPlane(Normal, Normal | ProjectionOrigin);
						}
					}
					else
					{
						bIsOrtho = true;
						bUseVectorCull = false;
					}
				}
				if (!InstanceParams.ViewFrustumLocal.Planes.Num())
				{
					bDisableCull = true;
				}
				else
				{
					InstanceParams.ViewFrustumLocal.Init();
				}

				ElementParams.bBlendLODs = bMultipleSections;

				InstanceParams.ViewOriginInLocalZero = WorldToLocal.TransformPosition(View->GetTemporalLODOrigin(0, bMultipleSections));
				InstanceParams.ViewOriginInLocalOne = WorldToLocal.TransformPosition(View->GetTemporalLODOrigin(1, bMultipleSections));

				float MinSize = bIsOrtho ? 0.0f : CVarFoliageMinimumScreenSize.GetValueOnRenderThread();
				float LODScale = CVarFoliageLODDistanceScale.GetValueOnRenderThread();
				float LODRandom = CVarRandomLODRange.GetValueOnRenderThread();
				float MaxDrawDistanceScale = GetCachedScalabilityCVars().ViewDistanceScale;
				float SphereRadius = RenderData->Bounds.SphereRadius;

				for (int32 LODIndex = 0; LODIndex < InstanceParams.LODs; LODIndex++)
				{
					InstanceParams.LODPlanesMax[LODIndex] = MIN_flt;
					InstanceParams.LODPlanesMin[LODIndex] = MAX_flt;
				}
				ElementParams.FinalCullDistance = MIN_flt;
				for (int32 SampleIndex = 0; SampleIndex < 2; SampleIndex++)
				{
					// we aren't going to accurately figure out LOD planes for both samples and try all 4 possibilities, rather we just consider the worst case LOD range; the shaders will render correctly
					float Fac = View->GetTemporalLODDistanceFactor(SampleIndex, bMultipleSections) * SphereRadius * SphereRadius * LODScale * LODScale;

					float FinalCull = MAX_flt;

					if (MinSize > 0.0)
					{
						FinalCull = FMath::Sqrt(Fac / MinSize);
					}
					if (UserData_AllInstances.EndCullDistance > 0.0f && UserData_AllInstances.EndCullDistance < FinalCull)
					{
						FinalCull = UserData_AllInstances.EndCullDistance;
					}
					FinalCull *= MaxDrawDistanceScale;
					ElementParams.FinalCullDistance = FMath::Max(ElementParams.FinalCullDistance, FinalCull);

					for (int32 LODIndex = 1; LODIndex < InstanceParams.LODs; LODIndex++)
					{
						float Val = FMath::Min(FMath::Sqrt(Fac / RenderData->ScreenSize[LODIndex]), FinalCull);
						InstanceParams.LODPlanesMin[LODIndex - 1] = FMath::Min(InstanceParams.LODPlanesMin[LODIndex - 1], Val - LODRandom);
						InstanceParams.LODPlanesMax[LODIndex - 1] = FMath::Max(InstanceParams.LODPlanesMax[LODIndex - 1], Val);
					}
					InstanceParams.LODPlanesMin[InstanceParams.LODs - 1] = FMath::Min(InstanceParams.LODPlanesMin[InstanceParams.LODs - 1], FinalCull - LODRandom);
					InstanceParams.LODPlanesMax[InstanceParams.LODs - 1] = FMath::Max(InstanceParams.LODPlanesMax[InstanceParams.LODs - 1], FinalCull);
				}

				for (int32 LODIndex = 0; LODIndex < InstanceParams.LODs; LODIndex++)
				{
					InstanceParams.MinInstancesToSplit[LODIndex] = 2;
					int32 NumVerts = RenderData->LODResources[LODIndex].VertexBuffer.GetNumVertices();
					if (NumVerts)
					{
						InstanceParams.MinInstancesToSplit[LODIndex] = MinVertsToSplitNode / NumVerts;
					}
				}

				if (FirstOcclusionNode >= 0 && LastOcclusionNode >= 0 && FirstOcclusionNode <= LastOcclusionNode)
				{
					uint32 ViewId = View->GetViewKey();
					const FFoliageOcclusionResults* OldResults = OcclusionResults.Find(ViewId);
					if (OldResults && 
						OldResults->FrameNumberRenderThread == GFrameNumberRenderThread &&
						1 + LastOcclusionNode - FirstOcclusionNode == OldResults->NumResults
						)
					{
						InstanceParams.FirstOcclusionNode = FirstOcclusionNode;
						InstanceParams.LastOcclusionNode = LastOcclusionNode;
						InstanceParams.OcclusionResults = OldResults->Results;
					}
				}

				INC_DWORD_STAT(STAT_FoliageTraversals);
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
				if (GCaptureDebugRuns == GDebugTag && CaptureTag == GDebugTag)
				{
					for (int32 LODIndex = 0; LODIndex < InstanceParams.LODs; LODIndex++)
					{
						for (int32 Run = 0; Run < SingleDebugRuns[LODIndex].Num(); Run++)
						{
							InstanceParams.SingleLODRuns[LODIndex].Add(SingleDebugRuns[LODIndex][Run]);
						}
						InstanceParams.TotalSingleLODInstances[LODIndex] = SingleDebugTotalInstances[LODIndex];
						for (int32 Run = 0; Run < MultipleDebugRuns[LODIndex].Num(); Run++)
						{
							InstanceParams.MultipleLODRuns[LODIndex].Add(MultipleDebugRuns[LODIndex][Run]);
						}
						InstanceParams.TotalMultipleLODInstances[LODIndex] = MultipleDebugTotalInstances[LODIndex];
					}
				}
				else
#endif
				{
					SCOPE_CYCLE_COUNTER(STAT_FoliageTraversalTime);

					// validate that the bounding box is layed out correctly in memory
					check(&((const FVector4*)&ClusterTree[0].BoundMin)[1] == (const FVector4*)&ClusterTree[0].BoundMax);
					//check(UPTRINT(&ClusterTree[0].BoundMin) % 16 == 0);
					//check(UPTRINT(&ClusterTree[0].BoundMax) % 16 == 0);

					int32 UseMinLOD = ClampedMinLOD;

					int32 DebugMin = FMath::Min(CVarMinLOD.GetValueOnRenderThread(), InstanceParams.LODs - 1);
					if (DebugMin >= 0)
					{
						UseMinLOD = FMath::Max(UseMinLOD, DebugMin);
					}
					int32 UseMaxLOD = InstanceParams.LODs;

					int32 Force = CVarForceLOD.GetValueOnRenderThread();
					if (Force >= 0)
					{
						UseMinLOD = FMath::Clamp(Force, 0, InstanceParams.LODs - 1);
						UseMaxLOD = FMath::Clamp(Force, 0, InstanceParams.LODs - 1);
					}

					if (CVarCullAll.GetValueOnRenderThread() < 1)
					{
						if (bUseVectorCull)
						{
							Traverse<true>(InstanceParams, 0, UseMinLOD, UseMaxLOD, bDisableCull);
						}
						else
						{
							Traverse<false>(InstanceParams, 0, UseMinLOD, UseMaxLOD, bDisableCull);
						}
					}
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
					if (GCaptureDebugRuns == GDebugTag && CaptureTag != GDebugTag)
					{
						CaptureTag = GDebugTag;
						for (int32 LODIndex = 0; LODIndex < InstanceParams.LODs; LODIndex++)
						{
							SingleDebugRuns[LODIndex].Empty();
							SingleDebugTotalInstances[LODIndex] = InstanceParams.TotalSingleLODInstances[LODIndex];
							for (int32 Run = 0; Run < InstanceParams.SingleLODRuns[LODIndex].Num(); Run++)
							{
								SingleDebugRuns[LODIndex].Add(InstanceParams.SingleLODRuns[LODIndex][Run]);
							}
							MultipleDebugRuns[LODIndex].Empty();
							MultipleDebugTotalInstances[LODIndex] = InstanceParams.TotalMultipleLODInstances[LODIndex];
							for (int32 Run = 0; Run < InstanceParams.MultipleLODRuns[LODIndex].Num(); Run++)
							{
								MultipleDebugRuns[LODIndex].Add(InstanceParams.MultipleLODRuns[LODIndex][Run]);
							}
						}
					}
#endif
				}

				FillDynamicMeshElements(Collector, ElementParams, InstanceParams);
			}

			// Render unbuilt instances
			if (FirstUnbuiltIndex <= LastUnbuiltIndex)
			{
				FFoliageRenderInstanceParams InstanceParams(true, false, false);

				// disable LOD blending for unbuilt instances as we haven't calculated the correct LOD.
				ElementParams.bBlendLODs = false;

				if (LastUnbuiltIndex - FirstUnbuiltIndex < 1000)
				{
					// less than 1000 instances, just add them all at LOD 0
					InstanceParams.AddRun(0, 0, FirstUnbuiltIndex, LastUnbuiltIndex);
				}
				else
				{
					// more than 1000, render them all at Max LOD
					InstanceParams.AddRun(RenderData->LODResources.Num() - 1, RenderData->LODResources.Num() - 1, FirstUnbuiltIndex, LastUnbuiltIndex);
				}
				FillDynamicMeshElements(Collector, ElementParams, InstanceParams);
			}
		}
	}
}

void FHierarchicalStaticMeshSceneProxy::AcceptOcclusionResults(const FSceneView* View, const bool* Results, int32 NumResults)
{
	if (OcclusionBounds.Num() == NumResults)
	{
		uint32 ViewId = View->GetViewKey();
		FFoliageOcclusionResults* OldResults = OcclusionResults.Find(ViewId);
		if (OldResults)
		{
			OldResults->FrameNumberRenderThread = GFrameNumberRenderThread;
			OldResults->Results = Results;
			OldResults->NumResults = NumResults;
		}
		else
		{
			// now is a good time to clean up any stale entries
			for (auto Iter = OcclusionResults.CreateIterator(); Iter; ++Iter)
			{
				if (Iter.Value().FrameNumberRenderThread != GFrameNumberRenderThread)
				{
					Iter.RemoveCurrent();
				}
			}
			OcclusionResults.Add(ViewId, FFoliageOcclusionResults(Results, NumResults));
		}
	}
}

const TArray<FBoxSphereBounds>* FHierarchicalStaticMeshSceneProxy::GetOcclusionQueries(const FSceneView* View) const 
{
	return &OcclusionBounds;
}

FBoxSphereBounds UHierarchicalInstancedStaticMeshComponent::CalcBounds(const FTransform& BoundTransform) const
{
	const TArray<FClusterNode>& ClusterTree = *ClusterTreePtr;
	if (ClusterTree.Num() || UnbuiltInstanceBounds.IsValid)
	{
		FBoxSphereBounds Result;
		if (ClusterTree.Num())
		{
			Result = FBox(ClusterTree[0].BoundMin, ClusterTree[0].BoundMax);
			// Include any unbuilt instances
			if (UnbuiltInstanceBounds.IsValid)
			{
				Result = Result + UnbuiltInstanceBounds;
			}
		}
		else
		{
			Result = UnbuiltInstanceBounds;
		}

		return Result.TransformBy(BoundTransform);
	}
	else
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UHierarchicalInstancedStaticMeshComponent_CalcBounds_SlowPath);
		return Super::CalcBounds(BoundTransform);
	}
}

UHierarchicalInstancedStaticMeshComponent::UHierarchicalInstancedStaticMeshComponent(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
	, ClusterTreePtr(MakeShareable(new TArray<FClusterNode>))
	, NumBuiltInstances(0)
	, UnbuiltInstanceBounds(0)
	, bIsAsyncBuilding(false)
	, bConcurrentRemoval(false)
	, AccumulatedNavigationDirtyArea(0)
{
	bCanEverAffectNavigation = true;
}

UHierarchicalInstancedStaticMeshComponent::~UHierarchicalInstancedStaticMeshComponent()
{
	if (ProxySize)
	{
		DEC_DWORD_STAT_BY(STAT_FoliageInstanceBuffers, ProxySize);
	}
	ProxySize = 0;
	FlushAsyncBuildInstanceBufferTask();
}

void UHierarchicalInstancedStaticMeshComponent::FlushAsyncBuildInstanceBufferTask()
{
	if (AsyncBuildInstanceBufferTask)
	{
		AsyncBuildInstanceBufferTask->EnsureCompletion();
		delete AsyncBuildInstanceBufferTask;
		AsyncBuildInstanceBufferTask = nullptr;
	}
}



void UHierarchicalInstancedStaticMeshComponent::Serialize(FArchive& Ar)
{
	Super::Serialize(Ar);
	if (Ar.IsLoading())
	{
		ClusterTreePtr = MakeShareable(new TArray<FClusterNode>);
	}
	TArray<FClusterNode>& ClusterTree = *ClusterTreePtr;
	ClusterTree.BulkSerialize(Ar);
}

SIZE_T UHierarchicalInstancedStaticMeshComponent::GetResourceSize( EResourceSizeMode::Type Mode )
{
	SIZE_T ResSize = Super::GetResourceSize(Mode);

	ResSize += SortedInstances.GetAllocatedSize();

	return ResSize;
}

bool UHierarchicalInstancedStaticMeshComponent::RemoveInstance(int32 InstanceIndex)
{
	if (!PerInstanceSMData.IsValidIndex(InstanceIndex))
	{
		return false;
	}

	PartialNavigationUpdate(InstanceIndex);

	// Save the render index
	RemovedInstances.Add(InstanceReorderTable[InstanceIndex]);
	
	// Remove the instance
	PerInstanceSMData.RemoveAtSwap(InstanceIndex);
	InstanceReorderTable.RemoveAtSwap(InstanceIndex);
#if WITH_EDITOR
	if (SelectedInstances.Num())
	{
		SelectedInstances.RemoveAtSwap(InstanceIndex);
	}
#endif

	if (IsAsyncBuilding())
	{
		// invalidate the results of the current async build as it's too slow to fix up deletes
		bConcurrentRemoval = true;
	}
	else
	{
		BuildTreeAsync();
	}


	// update the physics state
	if (bPhysicsStateCreated)
	{
		int32 OldLastIndex = PerInstanceSMData.Num();

		// always shut down physics for the last instance
		InstanceBodies[OldLastIndex]->TermBody();

		// recreate physics for the instance we swapped in the removed item's place
		if (InstanceIndex != PerInstanceSMData.Num() && PerInstanceSMData.Num())
		{
			InstanceBodies[InstanceIndex]->TermBody();
			InitInstanceBody(InstanceIndex, InstanceBodies[InstanceIndex]);
		}
	}

	ReleasePerInstanceRenderData();
	MarkRenderStateDirty();
	
	return true;
}

bool UHierarchicalInstancedStaticMeshComponent::UpdateInstanceTransform(int32 InstanceIndex, const FTransform& NewInstanceTransform, bool bWorldSpace, bool bMarkRenderStateDirty)
{
	if (!PerInstanceSMData.IsValidIndex(InstanceIndex))
	{
		return false;
	}

	if (IsAsyncBuilding())
	{
		// invalidate the results of the current async build we need to modify the tree
		bConcurrentRemoval = true;
	}

	// Treat the old instance render data like a removal.
	RemovedInstances.Add(InstanceReorderTable[InstanceIndex]);

	// Allocate a new instance render order ID, rendered last
	InstanceReorderTable[InstanceIndex] = PerInstanceSMData.Num()-1 + RemovedInstances.Num();

	bool Result = Super::UpdateInstanceTransform(InstanceIndex, NewInstanceTransform, bWorldSpace, bMarkRenderStateDirty);

	if (StaticMesh)
	{
		UnbuiltInstanceBounds += StaticMesh->GetBounds().GetBox().TransformBy(NewInstanceTransform);
	}

	if (!IsAsyncBuilding())
	{
		BuildTreeAsync();
	}

	return Result;
}

int32 UHierarchicalInstancedStaticMeshComponent::AddInstance(const FTransform& InstanceTransform)
{
	int32 InstanceIndex = UInstancedStaticMeshComponent::AddInstance(InstanceTransform);

	// Need to offset the newly added instance's RenderIndex by the amount that will be adjusted at the end of the frame
	InstanceReorderTable.Add(InstanceIndex + RemovedInstances.Num());

	if (PerInstanceSMData.Num() == 1)
	{
		BuildTree();
	}
	else
	if (!IsAsyncBuilding())
	{
		BuildTreeAsync();
	}

	if (StaticMesh)
	{
		UnbuiltInstanceBounds += StaticMesh->GetBounds().GetBox().TransformBy(InstanceTransform);
	}

	return InstanceIndex;
}

void UHierarchicalInstancedStaticMeshComponent::ClearInstances()
{
	if (IsAsyncBuilding())
	{
		bConcurrentRemoval = true;
	}

	ClusterTreePtr = MakeShareable(new TArray<FClusterNode>);
	NumBuiltInstances = 0;
	SortedInstances.Empty();
	UnbuiltInstanceBounds.Init();

	if (ProxySize)
	{
		DEC_DWORD_STAT_BY(STAT_FoliageInstanceBuffers, ProxySize);
	}

	Super::ClearInstances();
}

bool UHierarchicalInstancedStaticMeshComponent::ShouldCreatePhysicsState() const
{
	if (bDisableCollision)
	{
		return false;
	}
	return Super::ShouldCreatePhysicsState();
}

int32 UHierarchicalInstancedStaticMeshComponent::GetVertsForLOD(int32 LODIndex)
{
	if (StaticMesh && StaticMesh->HasValidRenderData())
	{
		return StaticMesh->GetNumVertices(LODIndex);
	}
	return 0;
}

int32 UHierarchicalInstancedStaticMeshComponent::DesiredInstancesPerLeaf()
{
	int32 LOD0Verts = GetVertsForLOD(0);
	int32 VertsToSplit = CVarMinVertsToSplitNode.GetValueOnAnyThread();
	if (LOD0Verts)
	{
		return FMath::Clamp(VertsToSplit / LOD0Verts, 1, 1024);
	}
	return 16;
}

float UHierarchicalInstancedStaticMeshComponent::ActualInstancesPerLeaf()
{
	const TArray<FClusterNode>& ClusterTree = *ClusterTreePtr;
	if (ClusterTree.Num())
	{
		int32 NumLeaves = 0;
		int32 NumInstances = 0;
		for (int32 Index = ClusterTree.Num() - 1; Index >= 0; Index--)
		{
			if (ClusterTree[Index].FirstChild >= 0)
			{
				break;
			}
			NumLeaves++;
			NumInstances += 1 + ClusterTree[Index].LastInstance - ClusterTree[Index].FirstInstance;
		}
		if (NumLeaves)
		{
			return float(NumInstances) / float(NumLeaves);
		}
	}
	return 0.0f;
}

void UHierarchicalInstancedStaticMeshComponent::PostBuildStats()
{
#if 0
	const TArray<FClusterNode>& ClusterTree = *ClusterTreePtr;
	int32 NumInst = WriteOncePrebuiltInstanceBuffer.Num() ? WriteOncePrebuiltInstanceBuffer.Num() : PerInstanceSMData.Num();
	UE_LOG(LogStaticMesh, Display, TEXT("Built a foliage hierarchy with %d instances, %d nodes, %f instances / leaf (desired %d) and %d verts in LOD0. Grass? %d "), NumInst, ClusterTree.Num(), ActualInstancesPerLeaf(), DesiredInstancesPerLeaf(), GetVertsForLOD(0), !!WriteOncePrebuiltInstanceBuffer.Num());
#endif
}


void UHierarchicalInstancedStaticMeshComponent::BuildTree()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UHierarchicalInstancedStaticMeshComponent_BuildTree);
	// Verify that the mesh is valid before using it.
	const bool bMeshIsValid = 
		// make sure we have instances
		PerInstanceSMData.Num() > 0 &&
		// make sure we have an actual staticmesh
		StaticMesh &&
		StaticMesh->HasValidRenderData() &&
		// You really can't use hardware instancing on the consoles with multiple elements because they share the same index buffer. 
		// @todo: Level error or something to let LDs know this
		1;//StaticMesh->LODModels(0).Elements.Num() == 1;

	if(bMeshIsValid)
	{
		//double StartTime = FPlatformTime::Seconds();
		// If we don't have a random seed for this instanced static mesh component yet, then go ahead and
		// generate one now.  This will be saved with the static mesh component and used for future generation
		// of random numbers for this component's instances. (Used by the PerInstanceRandom material expression)
		while( InstancingRandomSeed == 0 )
		{
			InstancingRandomSeed = FMath::Rand();
		}

		TArray<FMatrix> InstanceTransforms;
		InstanceTransforms.AddUninitialized(PerInstanceSMData.Num());
		for (int32 Index = 0; Index < PerInstanceSMData.Num(); Index++)
		{
			InstanceTransforms[Index] = PerInstanceSMData[Index].Transform;
		}

		TUniquePtr<FClusterBuilder> Builder(new FClusterBuilder(InstanceTransforms, StaticMesh->GetBounds().GetBox(), DesiredInstancesPerLeaf()));
		Builder->Build();

		NumBuiltInstances = Builder->Result->InstanceReorderTable.Num();
		OcclusionLayerNumNodes = Builder->Result->OutOcclusionLayerNum;
		UnbuiltInstanceBounds.Init();
		RemovedInstances.Empty();

		ClusterTreePtr = MakeShareable(new TArray<FClusterNode>);
		Exchange(*ClusterTreePtr, Builder->Result->Nodes);
		Exchange(InstanceReorderTable, Builder->Result->InstanceReorderTable);
		Exchange(SortedInstances, Builder->Result->SortedInstances);
		
		FlushAccumulatedNavigationUpdates();
				
		PostBuildStats();
	}
}

void UHierarchicalInstancedStaticMeshComponent::BuildTreeAnyThread(
	TArray<FMatrix>& InstanceTransforms, 
	const FBox& MeshBox,
	TArray<FClusterNode>& OutClusterTree,
	TArray<int32>& OutSortedInstances,
	TArray<int32>& OutInstanceReorderTable,
	int32& OutOcclusionLayerNum,
	int32 MaxInstancesPerLeaf
	)
{
	check(MaxInstancesPerLeaf > 0);

	TUniquePtr<FClusterBuilder> Builder(new FClusterBuilder(InstanceTransforms, MeshBox, MaxInstancesPerLeaf));
	Builder->Build();
	OutOcclusionLayerNum = Builder->Result->OutOcclusionLayerNum;

	Exchange(OutClusterTree, Builder->Result->Nodes);
	Exchange(OutInstanceReorderTable, Builder->Result->InstanceReorderTable);
	Exchange(OutSortedInstances, Builder->Result->SortedInstances);

}

void UHierarchicalInstancedStaticMeshComponent::AcceptPrebuiltTree(TArray<FClusterNode>& InClusterTree, int InOcclusionLayerNumNodes)
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UHierarchicalInstancedStaticMeshComponent_AcceptPrebuiltTree);
	// this is only for prebuild data, already in the correct order
	check(!PerInstanceSMData.Num());
	NumBuiltInstances = WriteOncePrebuiltInstanceBuffer.Num();
	check(NumBuiltInstances);
	UnbuiltInstanceBounds.Init();
	RemovedInstances.Empty();
	ClusterTreePtr = MakeShareable(new TArray<FClusterNode>);
	InstanceReorderTable.Empty();
	SortedInstances.Empty();
	OcclusionLayerNumNodes = InOcclusionLayerNumNodes;

	// Verify that the mesh is valid before using it.
	const bool bMeshIsValid = 
		// make sure we have instances
		NumBuiltInstances > 0 &&
		// make sure we have an actual staticmesh
		StaticMesh &&
		StaticMesh->HasValidRenderData() &&
		// You really can't use hardware instancing on the consoles with multiple elements because they share the same index buffer. 
		// @todo: Level error or something to let LDs know this
		1;//StaticMesh->LODModels(0).Elements.Num() == 1;

	if(bMeshIsValid)
	{
		//double StartTime = FPlatformTime::Seconds();
		// If we don't have a random seed for this instanced static mesh component yet, then go ahead and
		// generate one now.  This will be saved with the static mesh component and used for future generation
		// of random numbers for this component's instances. (Used by the PerInstanceRandom material expression)
		while( InstancingRandomSeed == 0 )
		{
			InstancingRandomSeed = FMath::Rand();
		}
		Exchange(*ClusterTreePtr, InClusterTree);
		PostBuildStats();

	}
	QUICK_SCOPE_CYCLE_COUNTER(STAT_UHierarchicalInstancedStaticMeshComponent_AcceptPrebuiltTree_Mark);

	MarkRenderStateDirty();
}

void UHierarchicalInstancedStaticMeshComponent::BuildFlatTree(const TArray<int32>& LeafInstanceCounts)
{
	// Verify that the mesh is valid before using it.
	const bool bMeshIsValid = 
		// make sure we have instances
		PerInstanceSMData.Num() > 0 &&
		// make sure we have an actual staticmesh
		StaticMesh &&
		StaticMesh->HasValidRenderData() &&
		// You really can't use hardware instancing on the consoles with multiple elements because they share the same index buffer. 
		// @todo: Level error or something to let LDs know this
		1;//StaticMesh->LODModels(0).Elements.Num() == 1;

	if(bMeshIsValid)
	{
		//double StartTime = FPlatformTime::Seconds();
		// If we don't have a random seed for this instanced static mesh component yet, then go ahead and
		// generate one now.  This will be saved with the static mesh component and used for future generation
		// of random numbers for this component's instances. (Used by the PerInstanceRandom material expression)
		while( InstancingRandomSeed == 0 )
		{
			InstancingRandomSeed = FMath::Rand();
		}

		NumBuiltInstances = PerInstanceSMData.Num();
		UnbuiltInstanceBounds.Init();
		RemovedInstances.Empty();

		const FBox InstBox = StaticMesh->GetBounds().GetBox();

		ClusterTreePtr = MakeShareable(new TArray<FClusterNode>);
		TArray<FClusterNode>& ClusterTree = *ClusterTreePtr;

		ClusterTree.Empty(LeafInstanceCounts.Num() + 1);
		FClusterNode Root;
		Root.FirstChild = 1;
		Root.LastChild = LeafInstanceCounts.Num();
		Root.FirstInstance = 0;
		Root.LastInstance = PerInstanceSMData.Num() - 1;
		ClusterTree.Add(Root);
		FBox RootBox(ForceInit);

		int32 InstIndex = 0;
		for (auto Count : LeafInstanceCounts)
		{
			FClusterNode Leaf;
			Leaf.FirstChild = -1;
			Leaf.LastChild = -1;
			Leaf.FirstInstance = InstIndex;
			Leaf.LastInstance = InstIndex + Count - 1;
			check(Count);


			FBox LeafBox(ForceInit);
			for (int32 SubIndex = 0; SubIndex < Count; SubIndex++)
			{
				const FMatrix& ThisInstTrans = PerInstanceSMData[InstIndex + SubIndex].Transform;
				const FBox ThisInstBox = InstBox.TransformBy(ThisInstTrans);
				LeafBox += ThisInstBox;
			}
			Leaf.BoundMin = LeafBox.Min;
			Leaf.BoundMax = LeafBox.Max;
			ClusterTree.Add(Leaf);
			RootBox += LeafBox;
			InstIndex += Count;
		}
		check(InstIndex == PerInstanceSMData.Num()); // else you botched your counts
		ClusterTree[0].BoundMin = RootBox.Min;
		ClusterTree[0].BoundMax = RootBox.Max;

		if (ClusterTree.Num() == 2)
		{
			// no point in having two levels if the second level only has one node
			ClusterTree.RemoveAt(1);
			ClusterTree[0].FirstChild = -1;
			ClusterTree[0].LastChild = -1;
		}

		InstanceReorderTable.Empty(PerInstanceSMData.Num());
		SortedInstances.Empty(PerInstanceSMData.Num());

		for (int32 Index = 0; Index < PerInstanceSMData.Num(); Index++)
		{
			InstanceReorderTable.Add(Index);
			SortedInstances.Add(Index);
		}
		OcclusionLayerNumNodes = 0;

		FlushAccumulatedNavigationUpdates();

		PostBuildStats();
	}
}


void UHierarchicalInstancedStaticMeshComponent::ApplyBuildTreeAsync(ENamedThreads::Type CurrentThread, const FGraphEventRef& MyCompletionGraphEvent, TSharedRef<FClusterBuilder, ESPMode::ThreadSafe> Builder, double StartTime)
{
	bIsAsyncBuilding = false;

	if (bConcurrentRemoval)
	{
		bConcurrentRemoval = false;

		UE_LOG(LogStaticMesh, Display, TEXT("Discarded foliage hierarchy of %d elements build due to concurrent removal (%.1fs)"), Builder->Result->InstanceReorderTable.Num(), float(FPlatformTime::Seconds() - StartTime));

		// There were removes while we were building, it's too slow to fix up the result now, so build async again.
		BuildTreeAsync();
	}
	else
	{
		NumBuiltInstances = Builder->Result->InstanceReorderTable.Num();

	    if (NumBuiltInstances < PerInstanceSMData.Num())
	    {
		    // Add remap entries for unbuilt instances
			Builder->Result->InstanceReorderTable.AddUninitialized(PerInstanceSMData.Num() - NumBuiltInstances);
		    for (int32 Index = NumBuiltInstances; Index < PerInstanceSMData.Num(); Index++)
		    {
				Builder->Result->InstanceReorderTable[Index] = Index;
		    }
	    }
	
		ClusterTreePtr = MakeShareable(new TArray<FClusterNode>);
		TArray<FClusterNode>& ClusterTree = *ClusterTreePtr;
	    Exchange(ClusterTree, Builder->Result->Nodes);
		Exchange(InstanceReorderTable, Builder->Result->InstanceReorderTable);
		Exchange(SortedInstances, Builder->Result->SortedInstances);
		RemovedInstances.Empty();
		OcclusionLayerNumNodes = Builder->Result->OutOcclusionLayerNum;


	    //UE_LOG(LogStaticMesh, Display, TEXT("Built a foliage hierarchy with %d of %d elements in %.1fs."), NumBuiltInstances, PerInstanceSMData.Num(), float(FPlatformTime::Seconds() - StartTime));
	
	    if (NumBuiltInstances < PerInstanceSMData.Num())
	    {
		    // There are new outstanding instances, build again!
		    BuildTreeAsync();
	    }
		else
		{
			UnbuiltInstanceBounds.Init();
			FlushAccumulatedNavigationUpdates();
		}

		ReleasePerInstanceRenderData();
	    MarkRenderStateDirty();

		PostBuildStats();
	}
}

void UHierarchicalInstancedStaticMeshComponent::BuildTreeAsync()
{
	check(!bIsAsyncBuilding);

	// Verify that the mesh is valid before using it.
	const bool bMeshIsValid =
		// make sure we have instances
		PerInstanceSMData.Num() > 0 &&
		// make sure we have an actual staticmesh
		StaticMesh &&
		StaticMesh->HasValidRenderData() &&
		// You really can't use hardware instancing on the consoles with multiple elements because they share the same index buffer. 
		// @todo: Level error or something to let LDs know this
		1;//StaticMesh->LODModels(0).Elements.Num() == 1;

	if (bMeshIsValid)
	{
		double StartTime = FPlatformTime::Seconds();
		// If we don't have a random seed for this instanced static mesh component yet, then go ahead and
		// generate one now.  This will be saved with the static mesh component and used for future generation
		// of random numbers for this component's instances. (Used by the PerInstanceRandom material expression)
		while (InstancingRandomSeed == 0)
		{
			InstancingRandomSeed = FMath::Rand();
		}

		int32 Num = PerInstanceSMData.Num();
		TArray<FMatrix> InstanceTransforms;
		InstanceTransforms.AddUninitialized(Num);
		for (int32 Index = 0; Index < Num; Index++)
		{
			InstanceTransforms[Index] = PerInstanceSMData[Index].Transform;
		}

		//UE_LOG(LogStaticMesh, Display, TEXT("Copied %d transforms in %.3fs."), Num, float(FPlatformTime::Seconds() - StartTime));

		TSharedRef<FClusterBuilder, ESPMode::ThreadSafe> Builder(new FClusterBuilder(InstanceTransforms, StaticMesh->GetBounds().GetBox(), DesiredInstancesPerLeaf()));

		bIsAsyncBuilding = true;

		FGraphEventRef BuildTreeAsyncResult(
			FDelegateGraphTask::CreateAndDispatchWhenReady(FDelegateGraphTask::FDelegate::CreateRaw(&Builder.Get(), &FClusterBuilder::BuildAsync), GET_STATID(STAT_FoliageBuildTime), NULL, ENamedThreads::GameThread, ENamedThreads::AnyThread));

		// add a dependent task to run on the main thread when build is complete
		FGraphEventRef UnusedAsyncResult(
			FDelegateGraphTask::CreateAndDispatchWhenReady(
			FDelegateGraphTask::FDelegate::CreateUObject(this, &UHierarchicalInstancedStaticMeshComponent::ApplyBuildTreeAsync, Builder, StartTime), GET_STATID(STAT_FoliageBuildTime),
			BuildTreeAsyncResult, ENamedThreads::GameThread, ENamedThreads::GameThread
			)
			);
	}
}

FPrimitiveSceneProxy* UHierarchicalInstancedStaticMeshComponent::CreateSceneProxy()
{
	QUICK_SCOPE_CYCLE_COUNTER(STAT_HierarchicalInstancedStaticMeshComponent_CreateSceneProxy);

	if (ProxySize)
	{
		DEC_DWORD_STAT_BY(STAT_FoliageInstanceBuffers, ProxySize);
	}
	ProxySize = 0;

	if (PerInstanceSMData.Num() != InstanceReorderTable.Num())
	{
		// this is a bizzaro case where folks are editing the PerInstanceSMData in a template of a blueprint component
		// not a great solution, but we can fix it now.
		BuildTree();
	}

	FlushAsyncBuildInstanceBufferTask();

	// Verify that the mesh is valid before using it.
	const bool bMeshIsValid = 
		// make sure we have instances
		(PerInstanceSMData.Num() > 0 || WriteOncePrebuiltInstanceBuffer.Num() > 0 || bPerInstanceRenderDataWasPrebuilt) &&
		// make sure we have an actual staticmesh
		StaticMesh &&
		StaticMesh->HasValidRenderData() &&
		// You really can't use hardware instancing on the consoles with multiple elements because they share the same index buffer. 
		// @todo: Level error or something to let LDs know this
		1;//StaticMesh->LODModels(0).Elements.Num() == 1;

	if(bMeshIsValid)
	{
		if (bHasPerInstanceHitProxies && GetWorld() && GetWorld()->IsGameWorld())
		{
			// hit proxies take forever to build, don't do it if this is a game world.
			bHasPerInstanceHitProxies = false;
		}
		// If we don't have a random seed for this instanced static mesh component yet, then go ahead and
		// generate one now.  This will be saved with the static mesh component and used for future generation
		// of random numbers for this component's instances. (Used by the PerInstanceRandom material expression)
		while( InstancingRandomSeed == 0 )
		{
			InstancingRandomSeed = FMath::Rand();
		}

		if (WriteOncePrebuiltInstanceBuffer.Num() > 0 || bPerInstanceRenderDataWasPrebuilt)
		{
			ProxySize = FStaticMeshInstanceData::GetResourceSize(WriteOncePrebuiltInstanceBuffer.Num());
			INC_DWORD_STAT_BY(STAT_FoliageInstanceBuffers, ProxySize);

			bool bIsGrass = !PerInstanceSMData.Num();

			check(bIsGrass || PerInstanceSMData.Num() == WriteOncePrebuiltInstanceBuffer.Num());
			return ::new FHierarchicalStaticMeshSceneProxy(bIsGrass, this, GetWorld()->FeatureLevel, WriteOncePrebuiltInstanceBuffer);
		}
		ProxySize = FStaticMeshInstanceData::GetResourceSize(PerInstanceSMData.Num());
		INC_DWORD_STAT_BY(STAT_FoliageInstanceBuffers, ProxySize);
		return ::new FHierarchicalStaticMeshSceneProxy(false, this, GetWorld()->FeatureLevel);
	}
	return NULL;
}

void FAsyncBuildInstanceBuffer::DoWork()
{
	QUICK_SCOPE_CYCLE_COUNTER(FAsyncBuildInstanceBuffer_DoWork);

	int32 Num = Component->PerInstanceSMData.Num();
	Component->WriteOncePrebuiltInstanceBuffer.AllocateInstances(Num);
	FRandomStream RandomStream(Component->InstancingRandomSeed);
	bool bUseRemapTable = Component->PerInstanceSMData.Num() == Component->InstanceReorderTable.Num();

	for (int32 InstanceIndex = 0; InstanceIndex < Num; InstanceIndex++)
	{
		FInstanceStream* InstanceRenderData = Component->WriteOncePrebuiltInstanceBuffer.GetInstanceWriteAddress(bUseRemapTable ? Component->InstanceReorderTable[InstanceIndex] : InstanceIndex);
		const FInstancedStaticMeshInstanceData& Instance = Component->PerInstanceSMData[InstanceIndex];

		InstanceRenderData->SetInstance(Instance.Transform, RandomStream.GetFraction(), Instance.LightmapUVBias, Instance.ShadowmapUVBias);
	}
	if (Component->RemovedInstances.Num())
	{
		check(bUseRemapTable);
		for (int32 InstanceIndex = 0; InstanceIndex < Component->RemovedInstances.Num(); InstanceIndex++)
		{
			FInstanceStream* InstanceRenderData = Component->WriteOncePrebuiltInstanceBuffer.GetInstanceWriteAddress(Component->RemovedInstances[InstanceIndex]);
			InstanceRenderData->NullifyInstance();
		}
	}


	check(World);
	check(World->AsyncPreRegisterLevelStreamingTasks.GetValue() > 0);
	FPlatformMisc::MemoryBarrier();
	World->AsyncPreRegisterLevelStreamingTasks.Decrement();
}

void UHierarchicalInstancedStaticMeshComponent::PostLoad()
{
	Super::PostLoad();
	if (CVarASyncInstaneBufferConversion.GetValueOnGameThread() > 0)
	{
		UWorld* World = GetWorld();
		if (World && World->IsGameWorld() && PerInstanceSMData.Num())
		{
			World->AsyncPreRegisterLevelStreamingTasks.Increment();
			while( InstancingRandomSeed == 0 )
			{
				InstancingRandomSeed = FMath::Rand();
			}
			AsyncBuildInstanceBufferTask = new FAsyncTask<FAsyncBuildInstanceBuffer>(this, World);
			AsyncBuildInstanceBufferTask->StartBackgroundTask();
		}
	}
	// For some reason we don't have a tree, or it is out of date. Build one now!
	if (StaticMesh && PerInstanceSMData.Num() > 0 && (!ClusterTreePtr.IsValid() || ClusterTreePtr->Num() == 0 || GetLinkerUE4Version() < VER_UE4_REBUILD_HIERARCHICAL_INSTANCE_TREES))
	{
		UE_LOG(LogStaticMesh, Warning, TEXT("Rebuilding foliage, please resave map %s."), *GetFullName());
		check(!IsAsyncBuilding());
		StaticMesh->ConditionalPostLoad();
		BuildTree();
	}
}

static void GatherInstanceTransformsInArea(const UHierarchicalInstancedStaticMeshComponent& Component, const FBox& AreaBox, int32 Child, TArray<FTransform>& InstanceData)
{
	const TArray<FClusterNode>& ClusterTree = *Component.ClusterTreePtr;
	if (ClusterTree.Num())
	{
	const FClusterNode& ChildNode = ClusterTree[Child];
	const FBox WorldNodeBox = FBox(ChildNode.BoundMin, ChildNode.BoundMax).TransformBy(Component.ComponentToWorld);
	
	if (AreaBox.Intersect(WorldNodeBox))
	{
		if (ChildNode.FirstChild < 0 || AreaBox.IsInside(WorldNodeBox))
		{
			// Unfortunately ordering of PerInstanceSMData does not match ordering of cluster tree, so we have to use remaping
			const bool bUseRemaping = Component.SortedInstances.Num() > 0;
			
			// In case there no more subdivision or node is completely encapsulated by a area box
			// add all instances to the result
			for (int32 i = ChildNode.FirstInstance; i <= ChildNode.LastInstance; ++i)
			{
				int32 SortedIdx = bUseRemaping ? Component.SortedInstances[i] : i;

					FTransform InstanceToComponent;
					if (Component.PerInstanceSMData.IsValidIndex(SortedIdx))
					{
						InstanceToComponent = FTransform(Component.PerInstanceSMData[SortedIdx].Transform);
					}
					else if (Component.PerInstanceRenderData.IsValid())
					{
						// if there's no PerInstanceSMData (e.g. for grass), we'll go ge the transform from the render buffer
						FInstanceStream const* Inst = Component.PerInstanceRenderData->InstanceBuffer.GetInstance(i);
						FMatrix XformMat;
						Inst->GetInstanceTransform(XformMat);
						InstanceToComponent = FTransform(XformMat);
					}
				if (!InstanceToComponent.GetScale3D().IsZero())
				{
					InstanceData.Add(InstanceToComponent*Component.ComponentToWorld);
				}
			}
		}
		else
		{
			for (int32 i = ChildNode.FirstChild; i <= ChildNode.LastChild; ++i)
			{
				GatherInstanceTransformsInArea(Component, AreaBox, i, InstanceData);
			}
		}
	}
}
}

int32 UHierarchicalInstancedStaticMeshComponent::GetOverlappingSphereCount(const FSphere& Sphere) const
{
	int32 Count = 0;
	TArray<FTransform> Transforms;
	const FBox AABB(Sphere.Center - FVector(Sphere.W), Sphere.Center + FVector(Sphere.W));
	GatherInstanceTransformsInArea(*this, AABB, 0, Transforms);
	const FBoxSphereBounds Bounds = StaticMesh->GetBounds();

	for (const FTransform& TM : Transforms)
	{
		const FVector Center = TM.GetLocation();
		const FSphere InstanceSphere(Center, Bounds.SphereRadius);
		
		if (Sphere.Intersects(InstanceSphere))
		{
			++Count;
		}
	}

	return Count;
}

int32 UHierarchicalInstancedStaticMeshComponent::GetOverlappingBoxCount(const FBox& Box) const
{
	TArray<FTransform> Transforms;
	GatherInstanceTransformsInArea(*this, Box, 0, Transforms);
	
	int32 Count = 0;
	const FBoxSphereBounds Bounds = StaticMesh->GetBounds();
	for(FTransform& T : Transforms)
	{
		const FVector Centre = T.GetLocation();
		const FBox OtherBox(FVector(Centre - Bounds.BoxExtent), FVector(Centre + Bounds.BoxExtent));

		if(Box.Intersect(OtherBox))
		{
			Count++;
		}
	}

	return Count;
}

void UHierarchicalInstancedStaticMeshComponent::GetOverlappingBoxTransforms(const FBox& Box, TArray<FTransform>& OutTransforms) const
{
	GatherInstanceTransformsInArea(*this, Box, 0, OutTransforms);

	const FBoxSphereBounds Bounds = StaticMesh->GetBounds();
	int32 NumTransforms = OutTransforms.Num();
	for(int32 Idx = NumTransforms - 1 ; Idx >= 0 ; --Idx)
	{
		FTransform& TM = OutTransforms[Idx];
		const FVector Centre = TM.GetLocation();
		const FBox OtherBox(FVector(Centre - Bounds.BoxExtent), FVector(Centre + Bounds.BoxExtent));

		if(!Box.Intersect(OtherBox))
		{
			OutTransforms.RemoveAt(Idx);
		}
	}
}

void UHierarchicalInstancedStaticMeshComponent::GetNavigationPerInstanceTransforms(const FBox& AreaBox, TArray<FTransform>& InstanceData) const
{
	if (IsTreeFullyBuilt())
	{
		const TArray<FClusterNode>& ClusterTree = *ClusterTreePtr;
		if (ClusterTree.Num())
		{
			GatherInstanceTransformsInArea(*this, AreaBox, 0, InstanceData);
		}
	}
	else
	{
		// This area should be processed again by navigation system when cluster tree is available
		// Store smaller tile box in accumulated dirty area, so we will not unintentionally mark as dirty neighbor tiles 
		const FBox SmallTileBox = AreaBox.ExpandBy(-AreaBox.GetExtent()/2.f);
		AccumulatedNavigationDirtyArea+= SmallTileBox;
	}
}

void UHierarchicalInstancedStaticMeshComponent::PartialNavigationUpdate(int32 InstanceIdx)
{
	if (InstanceIdx == INDEX_NONE)
	{
		AccumulatedNavigationDirtyArea.Init();
		UNavigationSystem::UpdateNavOctree(this);
	}
	else if (StaticMesh)
	{
		// Accumulate dirty areas and send them to navigation system once cluster tree is rebuilt
		UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
		// Check if this component is registered in navigation system
		if (NavSys && NavSys->GetObjectsNavOctreeId(this))
		{
			FTransform InstanceTransform(PerInstanceSMData[InstanceIdx].Transform);
			FBox InstanceBox = StaticMesh->GetBounds().TransformBy(InstanceTransform*ComponentToWorld).GetBox(); // in world space
			AccumulatedNavigationDirtyArea+= InstanceBox;
		}
	}
}

void UHierarchicalInstancedStaticMeshComponent::FlushAccumulatedNavigationUpdates()
{
	if (AccumulatedNavigationDirtyArea.IsValid)
	{
		QUICK_SCOPE_CYCLE_COUNTER(STAT_UHierarchicalInstancedStaticMeshComponent_FlushAccumulatedNavigationUpdates);

		const TArray<FClusterNode>& ClusterTree = *ClusterTreePtr;
		UNavigationSystem* NavSys = UNavigationSystem::GetCurrent(GetWorld());
		// Check if this component is registered in navigation system
		if (ClusterTree.Num() && NavSys && NavSys->GetObjectsNavOctreeId(this))
		{
			FBox NewBounds = FBox(ClusterTree[0].BoundMin, ClusterTree[0].BoundMax).TransformBy(ComponentToWorld);
			NavSys->UpdateNavOctreeElementBounds(this, NewBounds, AccumulatedNavigationDirtyArea);
		}
			
		AccumulatedNavigationDirtyArea.Init();
	}
}

static void RebuildFoliageTrees(const TArray<FString>& Args)
{
#if !(UE_BUILD_SHIPPING || UE_BUILD_TEST)
	UE_LOG(LogConsoleResponse, Display, TEXT("Rebuild Foliage Trees"));
#endif
	for (TObjectIterator<UHierarchicalInstancedStaticMeshComponent> It; It; ++It)
	{
		UHierarchicalInstancedStaticMeshComponent* Comp = *It;
		if (Comp && !Comp->IsTemplate() && !Comp->IsPendingKill() && !Comp->IsAsyncBuilding())
		{
			Comp->BuildTree();
			Comp->ReleasePerInstanceRenderData();
			Comp->MarkRenderStateDirty();
		}
	}
}

static FAutoConsoleCommand RebuildFoliageTreesCmd(
	TEXT("foliage.RebuildFoliageTrees"),
	TEXT("Rebuild the trees for non-grass foliage."),
	FConsoleCommandWithArgsDelegate::CreateStatic(&RebuildFoliageTrees)
	);


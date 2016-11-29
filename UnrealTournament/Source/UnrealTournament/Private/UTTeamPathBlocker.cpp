#include "UnrealTournament.h"
#include "UTTeamPathBlocker.h"
#include "UTRecastNavMesh.h"
#include "UTReachSpec_Team.h"
#include "UObjectToken.h"
#include "AI/NavigationSystemHelpers.h"
#include "AI/NavigationOctree.h"
#include "UTNavArea_Default.h"

void AUTTeamPathBlocker::AddSpecialPaths(class UUTPathNode* MyNode, class AUTRecastNavMesh* NavData)
{
	// if there are multiple team blockers, then one must win, we can't make two copies of the paths
	for (TWeakObjectPtr<AActor> It : MyNode->POIs)
	{
		if (Cast<AUTTeamPathBlocker>(It.Get()))
		{
			if (It.Get() == this)
			{
				break;
			}
			else
			{
				FMessageLog(TEXT("MapCheck")).Warning()->AddToken(FUObjectToken::Create(this))->AddToken(FTextToken::Create(NSLOCTEXT("UTRecastNavMesh", "MultipleTeamBlockers", "Multiple team-based path blockers in the same location; only one will be effective")));
				return;
			}
		}
	}

	// add reverse paths to the normal walking paths that use our special ReachSpec for team checking
	for (const UUTPathNode* OtherNode : NavData->GetAllNodes())
	{
		if (OtherNode != MyNode)
		{
			for (const FUTPathLink& OtherLink : OtherNode->Paths)
			{
				if (OtherLink.End == MyNode && OtherLink.ReachFlags == 0)
				{
					UUTReachSpec_Team* NewSpec = NewObject<UUTReachSpec_Team>(MyNode);
					NewSpec->Arbiter = this;
					FUTPathLink* NewLink = new(MyNode->Paths) FUTPathLink(MyNode, OtherLink.EndPoly, OtherNode, OtherLink.StartEdgePoly, NewSpec, OtherLink.CollisionRadius, OtherLink.CollisionHeight, OtherLink.ReachFlags);
					for (NavNodeRef PolyRef : MyNode->Polys)
					{
						NewLink->Distances.Add(NavData->CalcPolyDistance(PolyRef, NewLink->EndPoly));
					}
				}
				else if (OtherLink.Spec != nullptr)
				{
					// walk paths are always first so none of the following paths can be useful to us
					break;
				}
			}
		}
	}
}

bool AUTTeamPathBlocker::IsAllowedThrough_Implementation(uint8 TeamNum)
{
	FFrame::KismetExecutionMessage(*FString::Printf(TEXT("%s needs an IsAllowedThrough() implementation for AI!"), *GetName()), ELogVerbosity::Warning);
	return false;
}

USceneComponent* AUTTeamPathBlocker::GetBlockingComponent_Implementation() const
{
	return nullptr;
}

void AUTTeamPathBlocker::GetNavigationData(FNavigationRelevantData& Data) const
{
	// the modifier doesn't actually affect costs or reachability but simply causes the poly generation to split around the jump pad's bounds, so it has its own nav mesh region we can put a PathNode on without affecting adjacent areas
	FTransform ModTransform = ActorToWorld();
	ModTransform.RemoveScaling();
	Data.Modifiers.Add(FAreaNavModifier(GetNavigationBounds().GetExtent(), ModTransform, UUTNavArea_Default::StaticClass()));
}
// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "ReferenceViewerPrivatePCH.h"
#include "AssetThumbnail.h"
#include "AssetRegistryModule.h"

#define LOCTEXT_NAMESPACE "ReferenceViewer"

//////////////////////////////////////////////////////////////////////////
// UEdGraphNode_Reference

UEdGraphNode_Reference::UEdGraphNode_Reference(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	DependencyPin = NULL;
	ReferencerPin = NULL;
}

void UEdGraphNode_Reference::SetupReferenceNode(const FIntPoint& NodeLoc, const TArray<FName>& NewPackageNames, const FAssetData& InAssetData)
{
	check(NewPackageNames.Num() > 0);

	NodePosX = NodeLoc.X;
	NodePosY = NodeLoc.Y;

	PackageNames = NewPackageNames;
	FString ShortPackageName = FPackageName::GetLongPackageAssetName(NewPackageNames[0].ToString());
	if ( NewPackageNames.Num() == 1 )
	{
		NodeComment = NewPackageNames[0].ToString();
		NodeTitle = FText::FromString(ShortPackageName);
	}
	else
	{
		NodeComment = FText::Format(LOCTEXT("ReferenceNodeMultiplePackagesTitle", "{0} nodes"), FText::AsNumber(NewPackageNames.Num())).ToString();
		NodeTitle = FText::Format(LOCTEXT("ReferenceNodeMultiplePackagesComment", "{0} and {1} others"), FText::FromString(ShortPackageName), FText::AsNumber(NewPackageNames.Num()));
	}
	
	CacheAssetData(InAssetData);
	AllocateDefaultPins();
}

void UEdGraphNode_Reference::SetReferenceNodeCollapsed(const FIntPoint& NodeLoc, int32 InNumReferencesExceedingMax)
{
	NodePosX = NodeLoc.X;
	NodePosY = NodeLoc.Y;

	PackageNames.Empty();
	NodeComment = FText::Format(LOCTEXT("ReferenceNodeCollapsedMessage", "{0} other nodes"), FText::AsNumber(InNumReferencesExceedingMax)).ToString();

	NodeTitle = LOCTEXT("ReferenceNodeCollapsedTitle", "Collapsed nodes");
	CacheAssetData(FAssetData());
	AllocateDefaultPins();
}

void UEdGraphNode_Reference::AddReferencer(UEdGraphNode_Reference* ReferencerNode)
{
	UEdGraphPin* ReferencerDependencyPin = ReferencerNode->GetDependencyPin();

	if ( ensure(ReferencerDependencyPin) )
	{
		ReferencerDependencyPin->bHidden = false;
		ReferencerPin->bHidden = false;
		ReferencerPin->MakeLinkTo(ReferencerDependencyPin);
	}
}

FName UEdGraphNode_Reference::GetPackageName() const
{
	if ( PackageNames.Num() > 0 )
	{
		return PackageNames[0];
	}
	
	return NAME_None;
}

UEdGraph_ReferenceViewer* UEdGraphNode_Reference::GetReferenceViewerGraph() const
{
	return Cast<UEdGraph_ReferenceViewer>( GetGraph() );
}

FText UEdGraphNode_Reference::GetNodeTitle(ENodeTitleType::Type TitleType) const
{
	return NodeTitle;
}

void UEdGraphNode_Reference::AllocateDefaultPins()
{
	ReferencerPin = CreatePin( EEdGraphPinDirection::EGPD_Input, TEXT(""), TEXT(""), NULL, false, false, TEXT("") );
	DependencyPin = CreatePin( EEdGraphPinDirection::EGPD_Output, TEXT(""), TEXT(""), NULL, false, false, TEXT("") );

	ReferencerPin->bHidden = true;
	DependencyPin->bHidden = true;
}

UObject* UEdGraphNode_Reference::GetJumpTargetForDoubleClick() const
{
	if ( PackageNames.Num() > 0 )
	{
		GetReferenceGraph()->SetGraphRoot(PackageNames, FIntPoint(NodePosX, NodePosY));
		GetReferenceGraph()->RebuildGraph();
	}
	return NULL;
}

UEdGraphPin* UEdGraphNode_Reference::GetDependencyPin()
{
	return DependencyPin;
}

UEdGraphPin* UEdGraphNode_Reference::GetReferencerPin()
{
	return ReferencerPin;
}

void UEdGraphNode_Reference::CacheAssetData(const FAssetData& AssetData)
{
	if ( AssetData.IsValid() )
	{
		bUsesThumbnail = true;
		CachedAssetData = AssetData;
	}
	else
	{
		CachedAssetData = FAssetData();
		bUsesThumbnail = false;

		if ( PackageNames.Num() == 1 )
		{
			const FString PackageNameStr = PackageNames[0].ToString();
			if ( FPackageName::IsValidLongPackageName(PackageNameStr, true) )
			{
				if ( PackageNameStr.StartsWith(TEXT("/Script")) )
				{
					CachedAssetData.AssetClass = FName(TEXT("Code"));
				}
				else
				{
					const FString PotentiallyMapFilename = FPackageName::LongPackageNameToFilename(PackageNameStr, FPackageName::GetMapPackageExtension());
					const bool bIsMapPackage = FPlatformFileManager::Get().GetPlatformFile().FileExists(*PotentiallyMapFilename);
					if ( bIsMapPackage )
					{
						CachedAssetData.AssetClass = FName(TEXT("World"));
					}
				}
			}
		}
		else
		{
			CachedAssetData.AssetClass = FName(TEXT("Multiple Nodes"));
		}
	}

}

FAssetData UEdGraphNode_Reference::GetAssetData() const
{
	return CachedAssetData;
}

bool UEdGraphNode_Reference::UsesThumbnail() const
{
	return bUsesThumbnail;
}
#undef LOCTEXT_NAMESPACE
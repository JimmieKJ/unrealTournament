// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetRegistryPCH.h"

FDependsNode::FDependsNode(FName InPackageName)
{
	PackageName = InPackageName;
}

void FDependsNode::PrintNode() const
{
	UE_LOG(LogAssetRegistry, Log, TEXT("*** Printing DependsNode: %s ***"), *PackageName.ToString());
	UE_LOG(LogAssetRegistry, Log, TEXT("*** Dependencies:"));
	PrintDependencies();
	UE_LOG(LogAssetRegistry, Log, TEXT("*** Referencers:"));
	PrintReferencers();
}

void FDependsNode::PrintDependencies() const
{
	TSet<const FDependsNode*> VisitedNodes;

	PrintDependenciesRecursive(TEXT(""), VisitedNodes);
}

void FDependsNode::PrintReferencers() const
{
	TSet<const FDependsNode*> VisitedNodes;

	PrintReferencersRecursive(TEXT(""), VisitedNodes);
}

void FDependsNode::GetDependencies(TArray<FDependsNode*>& OutDependencies, EAssetRegistryDependencyType::Type InDependencyType) const
{
	IterateOverDependencies([&](FDependsNode* InDependency, EAssetRegistryDependencyType::Type /*InDependencyType*/)
	{
		OutDependencies.Add(InDependency);
	}, 
	InDependencyType);
}

void FDependsNode::GetDependencies(TArray<FName>& OutDependencies, EAssetRegistryDependencyType::Type InDependencyType) const
{
	IterateOverDependencies([&](FDependsNode* InDependency, EAssetRegistryDependencyType::Type /*InDependencyType*/)
	{
		OutDependencies.Add(InDependency->GetPackageName());
	},
	InDependencyType);
}

void FDependsNode::GetReferencers(TArray<FDependsNode*>& OutReferencers) const
{
	for (auto ReferencerIt = Referencers.CreateConstIterator(); ReferencerIt; ++ReferencerIt)
	{
		OutReferencers.Add(*ReferencerIt);
	}
}

void FDependsNode::PrintDependenciesRecursive(const FString& Indent, TSet<const FDependsNode*>& VisitedNodes) const
{
	if ( this == NULL )
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("%sNULL"), *Indent);
	}
	else if ( VisitedNodes.Contains(this) )
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("%s[CircularReferenceTo]%s"), *Indent, *PackageName.ToString());
	}
	else
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("%s%s"), *Indent, *PackageName.ToString());
		VisitedNodes.Add(this);

		IterateOverDependencies([&](FDependsNode* InDependency, EAssetRegistryDependencyType::Type /*InDependencyType*/)
		{
			InDependency->PrintDependenciesRecursive(Indent + TEXT("  "), VisitedNodes);
		},
		EAssetRegistryDependencyType::All
		);
	}
}

void FDependsNode::PrintReferencersRecursive(const FString& Indent, TSet<const FDependsNode*>& VisitedNodes) const
{
	if ( this == NULL )
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("%sNULL"), *Indent);
	}
	else if ( VisitedNodes.Contains(this) )
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("%s[CircularReferenceTo]%s"), *Indent, *PackageName.ToString());
	}
	else
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("%s%s"), *Indent, *PackageName.ToString());
		VisitedNodes.Add(this);

		for (auto ReferencerIt = Referencers.CreateConstIterator(); ReferencerIt; ++ReferencerIt)
		{
			(*ReferencerIt)->PrintReferencersRecursive(Indent + TEXT("  "), VisitedNodes);
		}
	}
}

// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#include "DependsNode.h"
#include "AssetRegistryPrivate.h"

void FDependsNode::PrintNode() const
{
	UE_LOG(LogAssetRegistry, Log, TEXT("*** Printing DependsNode: %s ***"), *Identifier.ToString());
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

void FDependsNode::GetDependencies(TArray<FAssetIdentifier>& OutDependencies, EAssetRegistryDependencyType::Type InDependencyType) const
{
	IterateOverDependencies([&](FDependsNode* InDependency, EAssetRegistryDependencyType::Type /*InDependencyType*/)
	{
		OutDependencies.Add(InDependency->GetIdentifier());
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
		UE_LOG(LogAssetRegistry, Log, TEXT("%s[CircularReferenceTo]%s"), *Indent, *Identifier.ToString());
	}
	else
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("%s%s"), *Indent, *Identifier.ToString());
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
		UE_LOG(LogAssetRegistry, Log, TEXT("%s[CircularReferenceTo]%s"), *Indent, *Identifier.ToString());
	}
	else
	{
		UE_LOG(LogAssetRegistry, Log, TEXT("%s%s"), *Indent, *Identifier.ToString());
		VisitedNodes.Add(this);

		for (auto ReferencerIt = Referencers.CreateConstIterator(); ReferencerIt; ++ReferencerIt)
		{
			(*ReferencerIt)->PrintReferencersRecursive(Indent + TEXT("  "), VisitedNodes);
		}
	}
}

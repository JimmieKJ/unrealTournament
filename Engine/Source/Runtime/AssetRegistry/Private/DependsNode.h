// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

/** Implementation of IDependsNode */
class FDependsNode
{
public:
	FDependsNode(FName InPackageName);

	/** Prints the dependencies and referencers for this node to the log */
	void PrintNode() const;
	/** Prints the dependencies for this node to the log */
	void PrintDependencies() const;
	/** Prints the referencers to this node to the log */
	void PrintReferencers() const;
	/** Gets the list of dependencies for this node */
	void GetDependencies(TArray<FDependsNode*>& OutDependencies) const;
	/** Gets the list of referencers to this node */
	void GetReferencers(TArray<FDependsNode*>& OutReferencers) const;

private:
	/** Recursively prints dependencies of the node starting with the specified indent. VisitedNodes should be an empty set at first which is populated recursively. */
	void PrintDependenciesRecursive(const FString& Indent, TSet<const FDependsNode*>& VisitedNodes) const;
	/** Recursively prints referencers to the node starting with the specified indent. VisitedNodes should be an empty set at first which is populated recursively. */
	void PrintReferencersRecursive(const FString& Indent, TSet<const FDependsNode*>& VisitedNodes) const;

public:
	/** The name of the package this node represents */
	FName PackageName;
	/** The list of dependencies for this node */
	TSet<FDependsNode*> Dependencies;
	/** The list of referencers to this node */
	TSet<FDependsNode*> Referencers;
};
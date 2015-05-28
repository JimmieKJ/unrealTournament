// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#pragma once

class FPathTreeNode
{
public:
	FPathTreeNode(const FString& InFolderName);
	FPathTreeNode(FName InFolderName);
	~FPathTreeNode();

	/** Adds the path to the tree relative to this node, creating nodes as needed. Returns true if the specified path was actually added (as opposed to already existed) */
	bool CachePath(const FString& Path);

	/** Removes the specified path in the tree relative to this node. Returns true if the folder was found and removed. */
	bool RemoveFolder(const FString& Path);

	/** Recursively gathers all child paths from the specified base path relative to this node */
	bool GetSubPaths(const FString& BasePath, TSet<FName>& OutPaths, bool bRecurse = true) const;

private:
	/** Helper function for CachePath. PathElements is the tokenized path delimited by "/" */
	bool CachePath_Recursive(TArray<FString>& PathElements);

	/** Helper function for RemoveFolder. PathElements is the tokenized path delimited by "/" */
	bool RemoveFolder_Recursive(TArray<FString>& PathElements);

	/** Finds a node specified by the supplied tokenized PathElements list */
	const FPathTreeNode* FindNode_Recursive(TArray<FString>& PathElements) const;

	/** Helper function for recursive all subpath gathering. Currentpath is the string version of the parent node. */
	void GetSubPaths_Recursive(const FString& CurrentPath, TSet<FName>& OutPaths, bool bRecurse = true) const;

private:
	// Experimental FName version of PathTreeNode functions. Only used when launching with -PathTreeFNames
	bool CachePath_Recursive(TArray<FName>& PathElements);
	bool RemoveFolder_Recursive(TArray<FName>& PathElements);
	const FPathTreeNode* FindNode_Recursive(TArray<FName>& PathElements) const;

private:
	/** The short folder name in the tree (no path) */
	FString FolderName;
	/** Experimental FName folder */
	FName FolderFName;
	/** The child node list for this node */
	TArray<FPathTreeNode*> Children;
};
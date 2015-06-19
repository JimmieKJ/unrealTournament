// Copyright 1998-2015 Epic Games, Inc. All Rights Reserved.

#include "AssetRegistryPCH.h"

bool IsUsingExperimentalFNameTreeNodes()
{
	static bool bUsingExperimentalFNameTreeNodes = FParse::Param(FCommandLine::Get(), TEXT("PathTreeFNames"));
	return bUsingExperimentalFNameTreeNodes;
}

FPathTreeNode::FPathTreeNode(const FString& InFolderName)
	: FolderName(InFolderName)
	, FolderFName(NAME_None)
{}

FPathTreeNode::FPathTreeNode(FName InFolderName)
	: FolderFName(InFolderName)
{}

FPathTreeNode::~FPathTreeNode()
{
	// Every node is responsible for deleting its children
	for (int32 ChildIdx = 0; ChildIdx < Children.Num(); ++ChildIdx)
	{
		delete Children[ChildIdx];
	}

	Children.Empty();
}

bool FPathTreeNode::CachePath(const FString& Path)
{
	if (IsUsingExperimentalFNameTreeNodes())
	{
		TArray<FString> PathElementStrings;
		Path.ParseIntoArray(PathElementStrings, TEXT("/"), /*InCullEmpty=*/true);

		TArray<FName> PathElements;
		PathElements.Reserve(PathElementStrings.Num());
		for (const auto& Element : PathElementStrings)
		{
			PathElements.Add(FName(*Element));
		}

		return CachePath_Recursive(PathElements);
	}
	else
	{
		TArray<FString> PathElements;
		Path.ParseIntoArray(PathElements, TEXT("/"), /*InCullEmpty=*/true);

		return CachePath_Recursive(PathElements);
	}
}

bool FPathTreeNode::RemoveFolder(const FString& Path)
{
	if (IsUsingExperimentalFNameTreeNodes())
	{
		TArray<FString> PathElementStrings;
		Path.ParseIntoArray(PathElementStrings, TEXT("/"), /*InCullEmpty=*/true);

		TArray<FName> PathElements;
		PathElements.Reserve(PathElementStrings.Num());
		for (const auto& Element : PathElementStrings)
		{
			PathElements.Add(FName(*Element));
		}

		return RemoveFolder_Recursive(PathElements);
	}
	else
	{
		TArray<FString> PathElements;
		Path.ParseIntoArray(PathElements, TEXT("/"), /*InCullEmpty=*/true);

		return RemoveFolder_Recursive(PathElements);
	}
}

bool FPathTreeNode::GetSubPaths(const FString& BasePath, TSet<FName>& OutPaths, bool bRecurse) const
{
	if (IsUsingExperimentalFNameTreeNodes())
	{
		TArray<FString> PathElementStrings;
		BasePath.ParseIntoArray(PathElementStrings, TEXT("/"), /*InCullEmpty=*/true);

		TArray<FName> PathElements;
		PathElements.Reserve(PathElementStrings.Num());
		for (const auto& Element : PathElementStrings)
		{
			PathElements.Add(FName(*Element));
		}

		const FPathTreeNode* BasePathNode = FindNode_Recursive(PathElements);

		if (BasePathNode)
		{
			// Found the base path, get the paths of all its children
			for (int32 ChildIdx = 0; ChildIdx < BasePathNode->Children.Num(); ++ChildIdx)
			{
				BasePathNode->Children[ChildIdx]->GetSubPaths_Recursive(BasePath, OutPaths, bRecurse);
			}

			return true;
		}
		else
		{
			// Failed to find the base path
			return false;
		}
	}
	else
	{
		TArray<FString> PathElements;
		BasePath.ParseIntoArray(PathElements, TEXT("/"), /*InCullEmpty=*/true);

		const FPathTreeNode* BasePathNode = FindNode_Recursive(PathElements);

		if ( BasePathNode )
		{
			// Found the base path, get the paths of all its children
			for (int32 ChildIdx = 0; ChildIdx < BasePathNode->Children.Num(); ++ChildIdx)
			{
				BasePathNode->Children[ChildIdx]->GetSubPaths_Recursive(BasePath, OutPaths, bRecurse);
			}
		
			return true;
		}
		else
		{
			// Failed to find the base path
			return false;
		}
	}
}

bool FPathTreeNode::CachePath_Recursive(TArray<FString>& PathElements)
{
	if ( PathElements.Num() > 0 )
	{
		// Pop the bottom element
		FString ChildFolderName = PathElements[0];
		PathElements.RemoveAt(0);

		// Try to find a child which uses this folder name
		FPathTreeNode* Child = NULL;
		for (int32 ChildIdx = 0; ChildIdx < Children.Num(); ++ChildIdx)
		{
			if ( Children[ChildIdx]->FolderName == ChildFolderName )
			{
				Child = Children[ChildIdx];
				break;
			}
		}

		// If one was not found, create it
		if ( !Child )
		{
			int32 ChildIdx = Children.Add(new FPathTreeNode(ChildFolderName));
			Child = Children[ChildIdx];

			if ( PathElements.Num() == 0 )
			{
				// We added final element to the tree, return true;
				return true;
			}
		}

		if ( ensure(Child) )
		{
			return Child->CachePath_Recursive(PathElements);
		}
	}

	return false;
}

bool FPathTreeNode::RemoveFolder_Recursive(TArray<FString>& PathElements)
{
	if ( PathElements.Num() > 0 )
	{
		// Pop the bottom element
		FString ChildFolderName = PathElements[0];
		PathElements.RemoveAt(0);

		// Try to find a child which uses this folder name
		for (int32 ChildIdx = 0; ChildIdx < Children.Num(); ++ChildIdx)
		{
			if ( Children[ChildIdx]->FolderName == ChildFolderName )
			{
				FPathTreeNode* Child = Children[ChildIdx];

				if ( PathElements.Num() == 0 )
				{
					// This is the final child in the path, thus it is the folder to remove.
					// Remove it now and return true;
					Children.RemoveAt(ChildIdx);
					delete Child;
					return true;
				}
				else
				{
					// This is not the last child, so recurse below
					return Child->RemoveFolder_Recursive(PathElements);
				}
			}
		}
	}

	// The child was not found at some point while recursing, fail the remove.
	return false;
}

const FPathTreeNode* FPathTreeNode::FindNode_Recursive(TArray<FString>& PathElements) const
{
	if (PathElements.Num() == 0)
	{
		// Found the path node
		return this;
	}
	else
	{
		// Must drill down further
		// Pop the bottom element
		FString ChildFolderName = PathElements[0];
		PathElements.RemoveAt(0);

		for (int32 ChildIdx = 0; ChildIdx < Children.Num(); ++ChildIdx)
		{
			if ( Children[ChildIdx]->FolderName == ChildFolderName )
			{
				return Children[ChildIdx]->FindNode_Recursive(PathElements);
			}
		}

		// Could not find the path
		return NULL;
	}
}

bool FPathTreeNode::CachePath_Recursive(TArray<FName>& PathElements)
{
	if ( PathElements.Num() > 0 )
	{
		// Pop the bottom element
		FName ChildFolderName = PathElements[0];
		PathElements.RemoveAt(0);

		// Try to find a child which uses this folder name
		FPathTreeNode* Child = NULL;
		for (int32 ChildIdx = 0; ChildIdx < Children.Num(); ++ChildIdx)
		{
			if ( Children[ChildIdx]->FolderFName == ChildFolderName )
			{
				Child = Children[ChildIdx];
				break;
			}
		}

		// If one was not found, create it
		if ( !Child )
		{
			int32 ChildIdx = Children.Add(new FPathTreeNode(ChildFolderName));
			Child = Children[ChildIdx];

			if ( PathElements.Num() == 0 )
			{
				// We added final element to the tree, return true;
				return true;
			}
		}

		if ( ensure(Child) )
		{
			return Child->CachePath_Recursive(PathElements);
		}
	}

	return false;
}

bool FPathTreeNode::RemoveFolder_Recursive(TArray<FName>& PathElements)
{
	if ( PathElements.Num() > 0 )
	{
		// Pop the bottom element
		FName ChildFolderName = PathElements[0];
		PathElements.RemoveAt(0);

		// Try to find a child which uses this folder name
		for (int32 ChildIdx = 0; ChildIdx < Children.Num(); ++ChildIdx)
		{
			if ( Children[ChildIdx]->FolderFName == ChildFolderName )
			{
				FPathTreeNode* Child = Children[ChildIdx];

				if ( PathElements.Num() == 0 )
				{
					// This is the final child in the path, thus it is the folder to remove.
					// Remove it now and return true;
					Children.RemoveAt(ChildIdx);
					delete Child;
					return true;
				}
				else
				{
					// This is not the last child, so recurse below
					return Child->RemoveFolder_Recursive(PathElements);
				}
			}
		}
	}

	// The child was not found at some point while recursing, fail the remove.
	return false;
}

const FPathTreeNode* FPathTreeNode::FindNode_Recursive(TArray<FName>& PathElements) const
{
	if (PathElements.Num() == 0)
	{
		// Found the path node
		return this;
	}
	else
	{
		// Must drill down further
		// Pop the bottom element
		FName ChildFolderName = PathElements[0];
		PathElements.RemoveAt(0);

		for (int32 ChildIdx = 0; ChildIdx < Children.Num(); ++ChildIdx)
		{
			if ( Children[ChildIdx]->FolderFName == ChildFolderName )
			{
				return Children[ChildIdx]->FindNode_Recursive(PathElements);
			}
		}

		// Could not find the path
		return NULL;
	}
}


void FPathTreeNode::GetSubPaths_Recursive(const FString& CurrentPath, TSet<FName>& OutPaths, bool bRecurse) const
{
	FString NewPath = IsUsingExperimentalFNameTreeNodes() ? (CurrentPath / FolderFName.ToString()) : (CurrentPath / FolderName);
	OutPaths.Add(FName(*NewPath));

	if(bRecurse)
	{
		for (int32 ChildIdx = 0; ChildIdx < Children.Num(); ++ChildIdx)
		{
			Children[ChildIdx]->GetSubPaths_Recursive(NewPath, OutPaths, bRecurse);
		}
	}
}

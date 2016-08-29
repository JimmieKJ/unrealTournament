// Copyright 1998-2016 Epic Games, Inc. All Rights Reserved.

#pragma once
#include "K2Node.h"
#include "K2Node_TunnelBoundary.generated.h"

UCLASS()
class BLUEPRINTGRAPH_API UK2Node_TunnelBoundary : public UK2Node
{
	GENERATED_UCLASS_BODY()

	/** Tunnel Graph Name */
	UPROPERTY()
	FName TunnelGraphName;

	/** Final exit site */
	UPROPERTY()
	FEdGraphPinReference FinalExitSite;

	/** Wether or not this is an exit point */
	bool bEntryNode;

public:

	//~ Begin of UK2Node Interface
	virtual FText GetNodeTitle(ENodeTitleType::Type TitleType) const override;
	virtual class FNodeHandlingFunctor* CreateNodeHandler(class FKismetCompilerContext& CompilerContext) const override;
	//~ End UK2Node Interface

	/** Creates Boundary nodes for a tunnel graph */
	static void CreateBoundaryNodesForGraph(UEdGraph* TunnelGraph, class FCompilerResultsLog& MessageLog);

	/** Creates Boundary nodes for a tunnel instance */
	static void CreateBoundaryNodesForTunnelInstance(UK2Node_Tunnel* TunnelInstance, UEdGraph* TunnelGraph, class FCompilerResultsLog& MessageLog);

	/** Checks if the tunnel node is a pure tunnel rather than a tunnel instance */
	static bool IsPureTunnel(UK2Node_Tunnel* Tunnel);

protected:

	/** Wires up the tunnel entry boundries */
	void WireUpTunnelEntry(UK2Node_Tunnel* TunnelInstance, UEdGraphPin* TunnelPin, FCompilerResultsLog& MessageLog);

	/** Wires up the tunnel exit boundries */
	void WireUpTunnelExit(UK2Node_Tunnel* TunnelInstance, UEdGraphPin* TunnelPin, FCompilerResultsLog& MessageLog);

	/** Determines the tunnel graph name */
	void DetermineTunnelGraphName(UK2Node_Tunnel* TunnelInstance);

	/** Build a guid map for nodes from the original graph */
	static void BuildSourceNodeMap(UEdGraphNode* Tunnel, TMap<FGuid, const UEdGraphNode*>& SourceNodeMap);

	/** Determines the true source tunnel instance */
	static UEdGraphNode* FindTrueSourceTunnelInstance(UEdGraphNode* Tunnel, UEdGraphNode* SourceTunnelInstance);
};

#include "ComboGraph/RZCombatComboGraph.h"

#include "ComboGraph/RZCombatComboGraphNodes.h"
#include "ComboGraph/RZCombatComboGraphSchema.h"
#include "RZCombatComboTableAsset.h"

void URZCombatComboGraph::InitializeGraph(URZCombatComboTableAsset* InComboTable)
{
	(void)InComboTable;

	if (Schema != URZCombatComboGraphSchema::StaticClass())
	{
		Modify();
		Schema = URZCombatComboGraphSchema::StaticClass();
	}
}

URZCombatComboTableAsset* URZCombatComboGraph::GetComboTable() const
{
	return GetTypedOuter<URZCombatComboTableAsset>();
}

URZCombatComboGraphNode_Entry* URZCombatComboGraph::FindEntryNode() const
{
	for (UEdGraphNode* Node : Nodes)
	{
		if (URZCombatComboGraphNode_Entry* EntryNode = Cast<URZCombatComboGraphNode_Entry>(Node))
		{
			return EntryNode;
		}
	}

	return nullptr;
}

URZCombatComboGraphNode_Entry* URZCombatComboGraph::EnsureEntryNode()
{
	if (URZCombatComboGraphNode_Entry* ExistingEntryNode = FindEntryNode())
	{
		return ExistingEntryNode;
	}

	Modify();

	URZCombatComboGraphNode_Entry* EntryNode = NewObject<URZCombatComboGraphNode_Entry>(this, NAME_None, RF_Transactional);
	EntryNode->CreateNewGuid();
	EntryNode->NodePosX = 0;
	EntryNode->NodePosY = 0;
	EntryNode->AllocateDefaultPins();
	AddNode(EntryNode, false, false);

	return EntryNode;
}

void URZCombatComboGraph::SetRuntimeDebugSnapshot(const FRZCombatComboGraphRuntimeDebugSnapshot& InRuntimeDebugSnapshot)
{
	RuntimeDebugSnapshot = InRuntimeDebugSnapshot;
}

void URZCombatComboGraph::ClearRuntimeDebugSnapshot()
{
	RuntimeDebugSnapshot = FRZCombatComboGraphRuntimeDebugSnapshot();
}

const FRZCombatComboGraphRuntimeDebugSnapshot& URZCombatComboGraph::GetRuntimeDebugSnapshot() const
{
	return RuntimeDebugSnapshot;
}

ERZCombatComboGraphRuntimeNodeState URZCombatComboGraph::ResolveRuntimeDebugNodeState(const FRZCombatComboNodeId& NodeId) const
{
	return RZCombatComboGraphRuntimeDebug::ResolveComboNodeState(GetComboTable(), RuntimeDebugSnapshot, NodeId);
}

ERZCombatComboGraphRuntimeNodeState URZCombatComboGraph::ResolveRuntimeDebugTransitionState(
	const FRZCombatComboNodeId& SourceNodeId,
	bool bSourceIsEntry,
	const FRZCombatComboNodeId& TargetNodeId,
	const FGameplayTag& TriggerTag
) const
{
	return RZCombatComboGraphRuntimeDebug::ResolveTransitionState(
		GetComboTable(),
		RuntimeDebugSnapshot,
		SourceNodeId,
		bSourceIsEntry,
		TargetNodeId,
		TriggerTag
	);
}

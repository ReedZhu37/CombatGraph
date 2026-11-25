#pragma once

#include "CoreMinimal.h"
#include "ComboGraph/RZCombatComboGraphRuntimeDebug.h"
#include "EdGraph/EdGraph.h"
#include "RZCombatComboGraph.generated.h"

class URZCombatComboGraphNode_Entry;
class URZCombatComboTableAsset;

UCLASS()
class RZCOMBATEDITOR_API URZCombatComboGraph : public UEdGraph
{
	GENERATED_BODY()

public:
	void InitializeGraph(URZCombatComboTableAsset* InComboTable);
	URZCombatComboTableAsset* GetComboTable() const;

	URZCombatComboGraphNode_Entry* FindEntryNode() const;
	URZCombatComboGraphNode_Entry* EnsureEntryNode();

	void SetRuntimeDebugSnapshot(const FRZCombatComboGraphRuntimeDebugSnapshot& InRuntimeDebugSnapshot);
	void ClearRuntimeDebugSnapshot();
	const FRZCombatComboGraphRuntimeDebugSnapshot& GetRuntimeDebugSnapshot() const;
	ERZCombatComboGraphRuntimeNodeState ResolveRuntimeDebugNodeState(const FRZCombatComboNodeId& NodeId) const;
	ERZCombatComboGraphRuntimeNodeState ResolveRuntimeDebugTransitionState(
		const FRZCombatComboNodeId& SourceNodeId,
		bool bSourceIsEntry,
		const FRZCombatComboNodeId& TargetNodeId,
		const FGameplayTag& TriggerTag
	) const;

private:
	FRZCombatComboGraphRuntimeDebugSnapshot RuntimeDebugSnapshot;
};

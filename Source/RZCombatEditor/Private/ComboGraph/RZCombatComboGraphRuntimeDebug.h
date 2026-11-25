#pragma once

#include "CoreMinimal.h"
#include "RZCombatComboTableTypes.h"

class URZCombatComboComponent;
class URZCombatComboTableAsset;

enum class ERZCombatComboGraphRuntimeNodeState : uint8
{
	None,
	Available,
	Current,
	PendingActivation,
	Activated,
	Committed,
	Failed,
	Ended,
	Cancelled,
	ComboCommitted
};

struct RZCOMBATEDITOR_API FRZCombatComboGraphRuntimeDebugSnapshot
{
	bool bHasDebugTarget = false;
	TWeakObjectPtr<URZCombatComboComponent> ComboComponent;
	TWeakObjectPtr<URZCombatComboTableAsset> ActiveTable;
	FString DebugTargetLabel;
	FRZCombatComboNodeId CurrentNodeId;
	FRZCombatComboNodeId PendingSourceNodeId;
	FRZCombatComboNodeId PendingTargetNodeId;
	FRZCombatComboNodeId LastSourceNodeId;
	FRZCombatComboNodeId LastTargetNodeId;
	TArray<FRZCombatComboNodeId> AvailableTargetNodeIds;
	FGameplayTag LastTriggerTag;
	ERZCombatComboRequestResultType LastResult = ERZCombatComboRequestResultType::NoActiveTable;
	ERZCombatComboExecutionState ExecutionState = ERZCombatComboExecutionState::None;
	bool bObservedAbilityWasCancelled = false;
};

namespace RZCombatComboGraphRuntimeDebug
{
	RZCOMBATEDITOR_API ERZCombatComboGraphRuntimeNodeState ResolveComboNodeState(
		const URZCombatComboTableAsset* EditedTable,
		const FRZCombatComboGraphRuntimeDebugSnapshot& RuntimeDebug,
		const FRZCombatComboNodeId& NodeId
	);

	RZCOMBATEDITOR_API ERZCombatComboGraphRuntimeNodeState ResolveTransitionState(
		const URZCombatComboTableAsset* EditedTable,
		const FRZCombatComboGraphRuntimeDebugSnapshot& RuntimeDebug,
		const FRZCombatComboNodeId& SourceNodeId,
		bool bSourceIsEntry,
		const FRZCombatComboNodeId& TargetNodeId,
		const FGameplayTag& TriggerTag
	);

	RZCOMBATEDITOR_API FLinearColor GetNodeBorderColor(ERZCombatComboGraphRuntimeNodeState NodeState, bool bHovered);
	RZCOMBATEDITOR_API FLinearColor GetNodeBodyColor(
		ERZCombatComboGraphRuntimeNodeState NodeState,
		const FLinearColor& DefaultBodyColor,
		bool bHovered
	);
	RZCOMBATEDITOR_API FText GetExecutionStateText(ERZCombatComboExecutionState ExecutionState);
	RZCOMBATEDITOR_API FString MakeDebugSignature(const FRZCombatComboGraphRuntimeDebugSnapshot& RuntimeDebug);
}

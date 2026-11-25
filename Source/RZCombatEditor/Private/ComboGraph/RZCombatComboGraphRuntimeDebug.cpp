#include "ComboGraph/RZCombatComboGraphRuntimeDebug.h"

#include "RZCombatComboComponent.h"
#include "RZCombatComboTableAsset.h"

#define LOCTEXT_NAMESPACE "RZCombatComboGraphRuntimeDebug"

namespace
{
bool DoesNodeIdMatch(const FRZCombatComboNodeId& A, const FRZCombatComboNodeId& B)
{
	return A.IsValid() && B.IsValid() && A.Matches(B);
}

bool ContainsNodeId(const TArray<FRZCombatComboNodeId>& NodeIds, const FRZCombatComboNodeId& Query)
{
	for (const FRZCombatComboNodeId& NodeId : NodeIds)
	{
		if (DoesNodeIdMatch(NodeId, Query))
		{
			return true;
		}
	}

	return false;
}

bool DoesSourceMatch(
	const FRZCombatComboNodeId& ExpectedSourceNodeId,
	bool bExpectedSourceIsEntry,
	const FRZCombatComboNodeId& SourceNodeId,
	bool bSourceIsEntry
)
{
	if (ExpectedSourceNodeId.IsValid())
	{
		return !bSourceIsEntry && DoesNodeIdMatch(ExpectedSourceNodeId, SourceNodeId);
	}

	return bExpectedSourceIsEntry && bSourceIsEntry;
}

bool DoesTriggerMatch(const FGameplayTag& ExpectedTriggerTag, const FGameplayTag& TriggerTag)
{
	return !ExpectedTriggerTag.IsValid() || TriggerTag.MatchesTagExact(ExpectedTriggerTag);
}

bool TryGetExecutionTransitionPath(
	const FRZCombatComboGraphRuntimeDebugSnapshot& RuntimeDebug,
	FRZCombatComboNodeId& OutSourceNodeId,
	FRZCombatComboNodeId& OutTargetNodeId,
	ERZCombatComboGraphRuntimeNodeState& OutNodeState
)
{
	switch (RuntimeDebug.ExecutionState)
	{
	case ERZCombatComboExecutionState::AbilityFailed:
		OutSourceNodeId = RuntimeDebug.LastSourceNodeId;
		OutTargetNodeId = RuntimeDebug.LastTargetNodeId;
		OutNodeState = ERZCombatComboGraphRuntimeNodeState::Failed;
		return OutTargetNodeId.IsValid();

	case ERZCombatComboExecutionState::AbilityCancelled:
		OutSourceNodeId = RuntimeDebug.LastSourceNodeId;
		OutTargetNodeId = RuntimeDebug.LastTargetNodeId;
		OutNodeState = ERZCombatComboGraphRuntimeNodeState::Cancelled;
		return OutTargetNodeId.IsValid();

	case ERZCombatComboExecutionState::AbilityEnded:
		OutSourceNodeId = RuntimeDebug.LastSourceNodeId;
		OutTargetNodeId = RuntimeDebug.LastTargetNodeId;
		OutNodeState = ERZCombatComboGraphRuntimeNodeState::Ended;
		return OutTargetNodeId.IsValid();

	case ERZCombatComboExecutionState::ResolvedPendingActivation:
		OutSourceNodeId = RuntimeDebug.PendingSourceNodeId.IsValid()
			? RuntimeDebug.PendingSourceNodeId
			: RuntimeDebug.LastSourceNodeId;
		OutTargetNodeId = RuntimeDebug.PendingTargetNodeId.IsValid()
			? RuntimeDebug.PendingTargetNodeId
			: RuntimeDebug.LastTargetNodeId;
		OutNodeState = ERZCombatComboGraphRuntimeNodeState::PendingActivation;
		return OutTargetNodeId.IsValid();

	case ERZCombatComboExecutionState::AbilityActivated:
		OutSourceNodeId = RuntimeDebug.PendingSourceNodeId.IsValid()
			? RuntimeDebug.PendingSourceNodeId
			: RuntimeDebug.LastSourceNodeId;
		OutTargetNodeId = RuntimeDebug.PendingTargetNodeId.IsValid()
			? RuntimeDebug.PendingTargetNodeId
			: RuntimeDebug.LastTargetNodeId;
		OutNodeState = ERZCombatComboGraphRuntimeNodeState::Activated;
		return OutTargetNodeId.IsValid();

	case ERZCombatComboExecutionState::ComboCommitted:
	case ERZCombatComboExecutionState::AbilityCommitted:
		OutSourceNodeId = RuntimeDebug.LastSourceNodeId;
		OutTargetNodeId = RuntimeDebug.LastTargetNodeId.IsValid()
			? RuntimeDebug.LastTargetNodeId
			: RuntimeDebug.CurrentNodeId;
		OutNodeState = RuntimeDebug.ExecutionState == ERZCombatComboExecutionState::ComboCommitted
			? ERZCombatComboGraphRuntimeNodeState::ComboCommitted : ERZCombatComboGraphRuntimeNodeState::Committed;
		return OutTargetNodeId.IsValid();

	case ERZCombatComboExecutionState::None:
	default:
		return false;
	}
}

FString NodeIdToDebugString(const FRZCombatComboNodeId& NodeId)
{
	return FString::Printf(
		TEXT("%s:%s"),
		*NodeId.Guid.ToString(EGuidFormats::Digits),
		*NodeId.Name.ToString()
	);
}
}

ERZCombatComboGraphRuntimeNodeState RZCombatComboGraphRuntimeDebug::ResolveComboNodeState(
	const URZCombatComboTableAsset* EditedTable,
	const FRZCombatComboGraphRuntimeDebugSnapshot& RuntimeDebug,
	const FRZCombatComboNodeId& NodeId
)
{
	if (!RuntimeDebug.bHasDebugTarget || !EditedTable || RuntimeDebug.ActiveTable.Get() != EditedTable || !NodeId.IsValid())
	{
		return ERZCombatComboGraphRuntimeNodeState::None;
	}

	switch (RuntimeDebug.ExecutionState)
	{
	case ERZCombatComboExecutionState::AbilityFailed:
		if (DoesNodeIdMatch(RuntimeDebug.LastTargetNodeId, NodeId))
		{
			return ERZCombatComboGraphRuntimeNodeState::Failed;
		}
		break;

	case ERZCombatComboExecutionState::AbilityCancelled:
		if (DoesNodeIdMatch(RuntimeDebug.LastTargetNodeId, NodeId))
		{
			return ERZCombatComboGraphRuntimeNodeState::Cancelled;
		}
		break;

	case ERZCombatComboExecutionState::AbilityEnded:
		if (DoesNodeIdMatch(RuntimeDebug.LastTargetNodeId, NodeId))
		{
			return ERZCombatComboGraphRuntimeNodeState::Ended;
		}
		break;

	case ERZCombatComboExecutionState::ResolvedPendingActivation:
		if (DoesNodeIdMatch(RuntimeDebug.PendingTargetNodeId, NodeId) || DoesNodeIdMatch(RuntimeDebug.LastTargetNodeId, NodeId))
		{
			return ERZCombatComboGraphRuntimeNodeState::PendingActivation;
		}
		break;

	case ERZCombatComboExecutionState::AbilityActivated:
		if (DoesNodeIdMatch(RuntimeDebug.PendingTargetNodeId, NodeId) || DoesNodeIdMatch(RuntimeDebug.LastTargetNodeId, NodeId))
		{
			return ERZCombatComboGraphRuntimeNodeState::Activated;
		}
		break;

	case ERZCombatComboExecutionState::ComboCommitted:
	case ERZCombatComboExecutionState::AbilityCommitted:
		if (DoesNodeIdMatch(RuntimeDebug.LastTargetNodeId.IsValid() ? RuntimeDebug.LastTargetNodeId : RuntimeDebug.CurrentNodeId, NodeId))
		{
			return RuntimeDebug.ExecutionState == ERZCombatComboExecutionState::ComboCommitted
				? ERZCombatComboGraphRuntimeNodeState::ComboCommitted : ERZCombatComboGraphRuntimeNodeState::Committed;
		}
		break;

	case ERZCombatComboExecutionState::None:
	default:
		break;
	}

	if (DoesNodeIdMatch(RuntimeDebug.CurrentNodeId, NodeId))
	{
		return ERZCombatComboGraphRuntimeNodeState::Current;
	}

	if (ContainsNodeId(RuntimeDebug.AvailableTargetNodeIds, NodeId))
	{
		return ERZCombatComboGraphRuntimeNodeState::Available;
	}

	return ERZCombatComboGraphRuntimeNodeState::None;
}

ERZCombatComboGraphRuntimeNodeState RZCombatComboGraphRuntimeDebug::ResolveTransitionState(
	const URZCombatComboTableAsset* EditedTable,
	const FRZCombatComboGraphRuntimeDebugSnapshot& RuntimeDebug,
	const FRZCombatComboNodeId& SourceNodeId,
	bool bSourceIsEntry,
	const FRZCombatComboNodeId& TargetNodeId,
	const FGameplayTag& TriggerTag
)
{
	if (!RuntimeDebug.bHasDebugTarget
		|| !EditedTable
		|| RuntimeDebug.ActiveTable.Get() != EditedTable
		|| !TargetNodeId.IsValid())
	{
		return ERZCombatComboGraphRuntimeNodeState::None;
	}

	FRZCombatComboNodeId ExecutionSourceNodeId;
	FRZCombatComboNodeId ExecutionTargetNodeId;
	ERZCombatComboGraphRuntimeNodeState ExecutionNodeState = ERZCombatComboGraphRuntimeNodeState::None;
	if (TryGetExecutionTransitionPath(RuntimeDebug, ExecutionSourceNodeId, ExecutionTargetNodeId, ExecutionNodeState)
		&& DoesNodeIdMatch(ExecutionTargetNodeId, TargetNodeId)
		&& DoesSourceMatch(ExecutionSourceNodeId, !ExecutionSourceNodeId.IsValid(), SourceNodeId, bSourceIsEntry)
		&& DoesTriggerMatch(RuntimeDebug.LastTriggerTag, TriggerTag))
	{
		return ExecutionNodeState;
	}

	const bool bAvailableSourceMatches = RuntimeDebug.CurrentNodeId.IsValid()
		? DoesSourceMatch(RuntimeDebug.CurrentNodeId, false, SourceNodeId, bSourceIsEntry)
		: bSourceIsEntry;
	if (bAvailableSourceMatches && ContainsNodeId(RuntimeDebug.AvailableTargetNodeIds, TargetNodeId))
	{
		return ERZCombatComboGraphRuntimeNodeState::Available;
	}

	return ERZCombatComboGraphRuntimeNodeState::None;
}

FLinearColor RZCombatComboGraphRuntimeDebug::GetNodeBorderColor(ERZCombatComboGraphRuntimeNodeState NodeState, bool bHovered)
{
	switch (NodeState)
	{
	case ERZCombatComboGraphRuntimeNodeState::PendingActivation:
		return FLinearColor(1.0f, 0.72f, 0.18f, 1.0f);
	case ERZCombatComboGraphRuntimeNodeState::Activated:
		return FLinearColor(0.35f, 0.78f, 1.0f, 1.0f);
	case ERZCombatComboGraphRuntimeNodeState::Committed:
	case ERZCombatComboGraphRuntimeNodeState::ComboCommitted:
	case ERZCombatComboGraphRuntimeNodeState::Current:
		return FLinearColor(0.20f, 0.95f, 0.42f, 1.0f);
	case ERZCombatComboGraphRuntimeNodeState::Failed:
		return FLinearColor(1.0f, 0.18f, 0.15f, 1.0f);
	case ERZCombatComboGraphRuntimeNodeState::Cancelled:
		return FLinearColor(0.95f, 0.25f, 0.78f, 1.0f);
	case ERZCombatComboGraphRuntimeNodeState::Ended:
		return FLinearColor(0.45f, 0.66f, 1.0f, 1.0f);
	case ERZCombatComboGraphRuntimeNodeState::Available:
		return FLinearColor(0.35f, 0.48f, 0.65f, 1.0f);
	case ERZCombatComboGraphRuntimeNodeState::None:
	default:
		return bHovered ? FLinearColor(0.34f, 0.37f, 0.42f) : FLinearColor(0.18f, 0.19f, 0.22f);
	}
}

FLinearColor RZCombatComboGraphRuntimeDebug::GetNodeBodyColor(
	ERZCombatComboGraphRuntimeNodeState NodeState,
	const FLinearColor& DefaultBodyColor,
	bool bHovered
)
{
	switch (NodeState)
	{
	case ERZCombatComboGraphRuntimeNodeState::PendingActivation:
		return FLinearColor(0.36f, 0.24f, 0.08f, 1.0f);
	case ERZCombatComboGraphRuntimeNodeState::Activated:
		return FLinearColor(0.08f, 0.22f, 0.32f, 1.0f);
	case ERZCombatComboGraphRuntimeNodeState::Committed:
	case ERZCombatComboGraphRuntimeNodeState::ComboCommitted:
	case ERZCombatComboGraphRuntimeNodeState::Current:
		return FLinearColor(0.07f, 0.26f, 0.13f, 1.0f);
	case ERZCombatComboGraphRuntimeNodeState::Failed:
		return FLinearColor(0.34f, 0.07f, 0.07f, 1.0f);
	case ERZCombatComboGraphRuntimeNodeState::Cancelled:
		return FLinearColor(0.28f, 0.08f, 0.24f, 1.0f);
	case ERZCombatComboGraphRuntimeNodeState::Ended:
		return FLinearColor(0.13f, 0.18f, 0.32f, 1.0f);
	case ERZCombatComboGraphRuntimeNodeState::Available:
		return FLinearColor(0.18f, 0.24f, 0.32f, 1.0f);
	case ERZCombatComboGraphRuntimeNodeState::None:
	default:
		return bHovered ? DefaultBodyColor + FLinearColor(0.04f, 0.04f, 0.04f, 0.0f) : DefaultBodyColor;
	}
}

FText RZCombatComboGraphRuntimeDebug::GetExecutionStateText(ERZCombatComboExecutionState ExecutionState)
{
	switch (ExecutionState)
	{
	case ERZCombatComboExecutionState::ResolvedPendingActivation:
		return LOCTEXT("ExecutionStatePending", "Pending");
	case ERZCombatComboExecutionState::AbilityActivated:
		return LOCTEXT("ExecutionStateActivated", "Activated");
	case ERZCombatComboExecutionState::AbilityCommitted:
		return LOCTEXT("ExecutionStateCommitted", "GA Commit (observed)");
	case ERZCombatComboExecutionState::ComboCommitted:
		return LOCTEXT("ExecutionStateComboCommitted", "Combo Commit (state)");
	case ERZCombatComboExecutionState::AbilityFailed:
		return LOCTEXT("ExecutionStateFailed", "Failed");
	case ERZCombatComboExecutionState::AbilityEnded:
		return LOCTEXT("ExecutionStateEnded", "Ended");
	case ERZCombatComboExecutionState::AbilityCancelled:
		return LOCTEXT("ExecutionStateCancelled", "Cancelled");
	case ERZCombatComboExecutionState::None:
	default:
		return LOCTEXT("ExecutionStateIdle", "Idle");
	}
}

FString RZCombatComboGraphRuntimeDebug::MakeDebugSignature(const FRZCombatComboGraphRuntimeDebugSnapshot& RuntimeDebug)
{
	FString AvailableNodeSignature;
	for (const FRZCombatComboNodeId& NodeId : RuntimeDebug.AvailableTargetNodeIds)
	{
		AvailableNodeSignature += TEXT("|");
		AvailableNodeSignature += NodeIdToDebugString(NodeId);
	}

	return FString::Printf(
		TEXT("Has:%d;Component:%s;Table:%s;Label:%s;Current:%s;PendingSource:%s;Pending:%s;LastSource:%s;LastTarget:%s;LastTrigger:%s;LastResult:%d;State:%d;Cancelled:%d;Available:%s"),
		RuntimeDebug.bHasDebugTarget ? 1 : 0,
		RuntimeDebug.ComboComponent.IsValid() ? *RuntimeDebug.ComboComponent->GetPathName() : TEXT("None"),
		RuntimeDebug.ActiveTable.IsValid() ? *RuntimeDebug.ActiveTable->GetPathName() : TEXT("None"),
		*RuntimeDebug.DebugTargetLabel,
		*NodeIdToDebugString(RuntimeDebug.CurrentNodeId),
		*NodeIdToDebugString(RuntimeDebug.PendingSourceNodeId),
		*NodeIdToDebugString(RuntimeDebug.PendingTargetNodeId),
		*NodeIdToDebugString(RuntimeDebug.LastSourceNodeId),
		*NodeIdToDebugString(RuntimeDebug.LastTargetNodeId),
		*RuntimeDebug.LastTriggerTag.ToString(),
		static_cast<int32>(RuntimeDebug.LastResult),
		static_cast<int32>(RuntimeDebug.ExecutionState),
		RuntimeDebug.bObservedAbilityWasCancelled ? 1 : 0,
		*AvailableNodeSignature
	);
}

#undef LOCTEXT_NAMESPACE

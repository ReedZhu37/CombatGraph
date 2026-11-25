#pragma once

#include "RZCombatComboTableTypes.h"

/** Diagnostic events are values only: retaining history must not keep actors, abilities or assets alive. */
enum class ERZCombatComboDebugEventType : uint8
{
	Resolve,
	ComboCommit,
	AbilityActivated,
	AbilityCommitted,
	AbilityFailed,
	AbilityEnded,
	AbilityCancelled,
	Reset,
	TableChanged
};

struct RZCOMBATRUNTIME_API FRZCombatComboDebugEvent
{
	uint64 Sequence = 0;
	uint64 Frame = 0;
	double WorldTime = 0.0;
	ERZCombatComboDebugEventType Type = ERZCombatComboDebugEventType::Resolve;
	FString TablePath;
	FString PreviousTablePath;
	FRZCombatComboNodeId SourceNodeId;
	FRZCombatComboNodeId TargetNodeId;
	FRZCombatComboNodeId CurrentNodeId;
	FGameplayTag TriggerTag;
	ERZCombatComboRequestResultType Result = ERZCombatComboRequestResultType::NoActiveTable;
	ERZCombatComboExecutionState ExecutionState = ERZCombatComboExecutionState::None;
	FString AbilityClassPath;
	FGameplayAbilitySpecHandle AbilitySpecHandle;
	FGameplayTagContainer FailureTags;
};

#pragma once

#include "CoreMinimal.h"
#include "RZCombatComboDebugEvent.h"

class URZCombatComboComponent;
class URZCombatComboTableAsset;
class UWorld;

enum class ERZCombatComboDebuggerState : uint8
{
	Inactive,
	Waiting,
	Live,
	Paused,
	TargetLost,
	TableMismatch,
	Disabled
};

struct FRZCombatComboDebugCandidate
{
	TWeakObjectPtr<URZCombatComboComponent> Component;
	FString Label;
	FString SortKey;
	// Exact component selection precedes actor selection; unrelated/unmapped selections never participate.
	int32 SelectionRank = MAX_int32;
	bool bLocalPlayer = false;
};

namespace RZCombatComboDebugger
{
	TArray<UWorld*> GetPlayWorlds();
	TArray<UObject*> GetEditorSelection();
	bool IsRuntimeComponent(const URZCombatComboComponent* Component, TConstArrayView<UWorld*> PlayWorlds);
	FString DescribeComponent(const URZCombatComboComponent* Component);
	TArray<FRZCombatComboDebugCandidate> FindCandidates(
		const URZCombatComboTableAsset* Table, TConstArrayView<UWorld*> PlayWorlds, TConstArrayView<UObject*> Selection);
	URZCombatComboComponent* ChooseAutomaticTarget(TArray<FRZCombatComboDebugCandidate> Candidates);
}

/** Per asset-editor observation session. No strong runtime references, gameplay delegate binding or policy changes. */
class FRZCombatComboDebuggerSession
{
public:
	void Update(const URZCombatComboTableAsset* Table, TConstArrayView<UWorld*> PlayWorlds,
		TConstArrayView<UObject*> Selection, bool bEnabled = true);
	bool SelectTarget(URZCombatComboComponent* Component, const URZCombatComboTableAsset* Table,
		TConstArrayView<UWorld*> PlayWorlds);
	void ReturnToAuto();
	void EndSession();

	URZCombatComboComponent* GetTarget() const { return Target.Get(); }
	ERZCombatComboDebuggerState GetState() const { return State; }
	bool IsManual() const { return bManual; }
	bool HasChosenTarget() const { return bTargetChosen; }
	const FString& GetTargetLabel() const { return TargetLabel; }
	const TArray<FRZCombatComboDebugEvent>& GetRecentEvents() const { return RecentEvents; }
	FText GetStateText() const;

private:
	void SetTarget(URZCombatComboComponent* Component, bool bInManual);
	TWeakObjectPtr<URZCombatComboComponent> Target;
	FString TargetLabel;
	TArray<FRZCombatComboDebugEvent> RecentEvents;
	ERZCombatComboDebuggerState State = ERZCombatComboDebuggerState::Inactive;
	bool bManual = false;
	bool bTargetChosen = false;
};

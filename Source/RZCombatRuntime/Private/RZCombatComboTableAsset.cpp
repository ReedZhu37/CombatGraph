#include "RZCombatComboTableAsset.h"

bool URZCombatComboTableAsset::HasNode(const FRZCombatComboNodeId& NodeId) const
{
	return FindNode(NodeId) != nullptr;
}

const FRZCombatComboNode* URZCombatComboTableAsset::FindNode(const FRZCombatComboNodeId& NodeId) const
{
	if (!NodeId.IsValid())
	{
		return nullptr;
	}

	for (const FRZCombatComboNode& Node : Nodes)
	{
		if (Node.bEnabled && Node.NodeId.Matches(NodeId))
		{
			return &Node;
		}
	}

	return nullptr;
}

const FRZCombatComboTransition* URZCombatComboTableAsset::ResolveBestTransition(
	const TArray<FRZCombatComboTransition>& Transitions,
	FGameplayTag TriggerTag,
	bool& bOutAmbiguous
) const
{
	bOutAmbiguous = false;
	const FRZCombatComboTransition* BestTransition = nullptr;

	for (const FRZCombatComboTransition& Transition : Transitions)
	{
		if (!Transition.bEnabled || !Transition.TriggerTag.MatchesTagExact(TriggerTag))
		{
			continue;
		}

		if (!BestTransition || Transition.Priority > BestTransition->Priority)
		{
			BestTransition = &Transition;
			bOutAmbiguous = false;
			continue;
		}

		if (Transition.Priority == BestTransition->Priority)
		{
			bOutAmbiguous = true;
		}
	}

	return BestTransition;
}

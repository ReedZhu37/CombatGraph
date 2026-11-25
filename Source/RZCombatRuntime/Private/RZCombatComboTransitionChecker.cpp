#include "RZCombatComboTransitionChecker.h"

bool URZCombatComboTransitionChecker::CanUseTransition_Implementation(
	const FRZCombatComboTransitionCheckContext& Context
) const
{
	(void)Context;
	return true;
}

bool URZCombatComboAndChecker::CanUseTransition_Implementation(
	const FRZCombatComboTransitionCheckContext& Context
) const
{
	bool bHasChecker = false;
	for (const TObjectPtr<URZCombatComboTransitionChecker>& Checker : Checkers)
	{
		const URZCombatComboTransitionChecker* CheckerObject = Checker.Get();
		if (!CheckerObject)
		{
			continue;
		}

		bHasChecker = true;
		if (!CheckerObject->CanUseTransition(Context))
		{
			return false;
		}
	}

	return bHasChecker;
}

bool URZCombatComboOrChecker::CanUseTransition_Implementation(
	const FRZCombatComboTransitionCheckContext& Context
) const
{
	for (const TObjectPtr<URZCombatComboTransitionChecker>& Checker : Checkers)
	{
		const URZCombatComboTransitionChecker* CheckerObject = Checker.Get();
		if (CheckerObject && CheckerObject->CanUseTransition(Context))
		{
			return true;
		}
	}

	return false;
}

bool URZCombatComboNotChecker::CanUseTransition_Implementation(
	const FRZCombatComboTransitionCheckContext& Context
) const
{
	const URZCombatComboTransitionChecker* CheckerObject = Checker.Get();
	return CheckerObject && !CheckerObject->CanUseTransition(Context);
}

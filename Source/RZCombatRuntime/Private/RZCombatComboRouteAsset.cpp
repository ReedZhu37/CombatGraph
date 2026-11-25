#include "RZCombatComboRouteAsset.h"

namespace
{
	bool DoesContextPass(const FGameplayTagQuery& RequiredContext, const FGameplayTagQuery& BlockedContext, const FGameplayTagContainer& ContextTags)
	{
		if (!RequiredContext.IsEmpty() && !RequiredContext.Matches(ContextTags))
		{
			return false;
		}

		if (!BlockedContext.IsEmpty() && BlockedContext.Matches(ContextTags))
		{
			return false;
		}

		return true;
	}
}

const FRZCombatComboEntry* URZCombatComboRouteAsset::FindEntryByInputTag(FGameplayTag InputTag, const FGameplayTagContainer& ContextTags) const
{
	if (!InputTag.IsValid())
	{
		return nullptr;
	}

	for (const FRZCombatComboEntry& Entry : Entries)
	{
		if (!Entry.bEnabled)
		{
			continue;
		}

		if (!Entry.InputTag.MatchesTagExact(InputTag))
		{
			continue;
		}

		if (!DoesContextPass(Entry.RequiredContext, Entry.BlockedContext, ContextTags))
		{
			continue;
		}

		return &Entry;
	}

	return nullptr;
}

const FRZCombatComboAbilityRoute* URZCombatComboRouteAsset::FindRouteBySourceAbilityTag(FGameplayTag SourceAbilityTag) const
{
	if (!SourceAbilityTag.IsValid())
	{
		return nullptr;
	}

	for (const FRZCombatComboAbilityRoute& AbilityRoute : AbilityRoutes)
	{
		if (!AbilityRoute.bEnabled)
		{
			continue;
		}

		if (AbilityRoute.SourceAbilityTag.MatchesTagExact(SourceAbilityTag))
		{
			return &AbilityRoute;
		}
	}

	return nullptr;
}

const FRZCombatComboBranch* URZCombatComboRouteAsset::ResolveBranch(
	FGameplayTag SourceAbilityTag,
	FGameplayTag InputTag,
	const FGameplayTagContainer& ActiveWindowTags,
	const FGameplayTagContainer& ContextTags,
	bool bHasHitConfirm
) const
{
	const FRZCombatComboAbilityRoute* AbilityRoute = FindRouteBySourceAbilityTag(SourceAbilityTag);
	if (!AbilityRoute || !InputTag.IsValid())
	{
		return nullptr;
	}

	const FRZCombatComboBranch* BestBranch = nullptr;

	for (const FRZCombatComboBranch& Branch : AbilityRoute->Branches)
	{
		if (!Branch.bEnabled)
		{
			continue;
		}

		if (!Branch.RequiredInputTag.MatchesTagExact(InputTag))
		{
			continue;
		}

		if (!Branch.RequiredWindowTag.IsValid() || !ActiveWindowTags.HasTagExact(Branch.RequiredWindowTag))
		{
			continue;
		}

		if (Branch.bRequireHitConfirm && !bHasHitConfirm)
		{
			continue;
		}

		if (!DoesContextPass(Branch.RequiredContext, Branch.BlockedContext, ContextTags))
		{
			continue;
		}

		if (!BestBranch || Branch.Priority > BestBranch->Priority)
		{
			BestBranch = &Branch;
		}
	}

	return BestBranch;
}

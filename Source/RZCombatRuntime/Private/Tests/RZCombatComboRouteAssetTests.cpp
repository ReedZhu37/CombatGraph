#if WITH_DEV_AUTOMATION_TESTS

#include "RZCombatComboRouteAsset.h"

#include "Misc/AutomationTest.h"

namespace
{
	FGameplayTag MakeRouteAssetTestTag(const TCHAR* TagName)
	{
		return FGameplayTag::RequestGameplayTag(FName(TagName), false);
	}

	URZCombatComboRouteAsset* MakeRouteAsset()
	{
		return NewObject<URZCombatComboRouteAsset>();
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboRouteEntryTest,
	"RZCombat.Combo.DataLayer.EntryResolvesByInputTag",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboRouteEntryTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FGameplayTag LightInputTag = MakeRouteAssetTestTag(TEXT("Input.Action.Attack.Light"));
	const FGameplayTag TargetAbilityTag = MakeRouteAssetTestTag(TEXT("Ability.Combo.Test.A"));

	URZCombatComboRouteAsset* RouteAsset = MakeRouteAsset();
	TestNotNull(TEXT("Route asset should be created."), RouteAsset);

	FRZCombatComboEntry& Entry = RouteAsset->Entries.AddDefaulted_GetRef();
	Entry.InputTag = LightInputTag;
	Entry.TargetAbilityTag = TargetAbilityTag;

	const FRZCombatComboEntry* FoundEntry = RouteAsset->FindEntryByInputTag(LightInputTag, FGameplayTagContainer());

	TestNotNull(TEXT("Entry should resolve from exact input tag."), FoundEntry);
	TestEqual(TEXT("Resolved entry should point at the target ability tag."), FoundEntry ? FoundEntry->TargetAbilityTag : FGameplayTag(), TargetAbilityTag);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboRouteBranchTest,
	"RZCombat.Combo.DataLayer.BranchResolvesBySourceInputAndWindow",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboRouteBranchTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FGameplayTag SourceAbilityTag = MakeRouteAssetTestTag(TEXT("Ability.Combo.Test.A"));
	const FGameplayTag InputTag = MakeRouteAssetTestTag(TEXT("Input.Action.Attack.Light"));
	const FGameplayTag WindowTag = MakeRouteAssetTestTag(TEXT("Combo.Window.Branch"));
	const FGameplayTag LowPriorityTargetTag = MakeRouteAssetTestTag(TEXT("Ability.Combo.Test.LowPriority"));
	const FGameplayTag HighPriorityTargetTag = MakeRouteAssetTestTag(TEXT("Ability.Combo.Test.HighPriority"));

	URZCombatComboRouteAsset* RouteAsset = MakeRouteAsset();

	FRZCombatComboAbilityRoute& AbilityRoute = RouteAsset->AbilityRoutes.AddDefaulted_GetRef();
	AbilityRoute.SourceAbilityTag = SourceAbilityTag;

	FRZCombatComboBranch& LowPriorityBranch = AbilityRoute.Branches.AddDefaulted_GetRef();
	LowPriorityBranch.RequiredInputTag = InputTag;
	LowPriorityBranch.RequiredWindowTag = WindowTag;
	LowPriorityBranch.TargetAbilityTag = LowPriorityTargetTag;
	LowPriorityBranch.Priority = 1;

	FRZCombatComboBranch& HighPriorityBranch = AbilityRoute.Branches.AddDefaulted_GetRef();
	HighPriorityBranch.RequiredInputTag = InputTag;
	HighPriorityBranch.RequiredWindowTag = WindowTag;
	HighPriorityBranch.TargetAbilityTag = HighPriorityTargetTag;
	HighPriorityBranch.Priority = 10;

	FGameplayTagContainer ActiveWindows;
	ActiveWindows.AddTag(WindowTag);

	const FRZCombatComboBranch* FoundBranch = RouteAsset->ResolveBranch(SourceAbilityTag, InputTag, ActiveWindows, FGameplayTagContainer(), false);

	TestNotNull(TEXT("Branch should resolve from exact source, input, and active window."), FoundBranch);
	TestEqual(TEXT("Highest priority matching branch should win."), FoundBranch ? FoundBranch->TargetAbilityTag : FGameplayTag(), HighPriorityTargetTag);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboRouteDisabledAndContextTest,
	"RZCombat.Combo.DataLayer.DisabledAndContextRulesFilterRoutes",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboRouteDisabledAndContextTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FGameplayTag InputTag = MakeRouteAssetTestTag(TEXT("Input.Action.Attack.Heavy"));
	const FGameplayTag TargetAbilityTag = MakeRouteAssetTestTag(TEXT("Ability.Combo.Test.Heavy"));
	const FGameplayTag RequiredContextTag = MakeRouteAssetTestTag(TEXT("State.Combat.Ready"));
	const FGameplayTag BlockedContextTag = MakeRouteAssetTestTag(TEXT("State.Stunned"));

	URZCombatComboRouteAsset* RouteAsset = MakeRouteAsset();

	FRZCombatComboEntry& DisabledEntry = RouteAsset->Entries.AddDefaulted_GetRef();
	DisabledEntry.bEnabled = false;
	DisabledEntry.InputTag = InputTag;
	DisabledEntry.TargetAbilityTag = TargetAbilityTag;

	TestNull(TEXT("Disabled entry should not resolve."), RouteAsset->FindEntryByInputTag(InputTag, FGameplayTagContainer()));

	FRZCombatComboEntry& ContextEntry = RouteAsset->Entries.AddDefaulted_GetRef();
	ContextEntry.InputTag = InputTag;
	ContextEntry.TargetAbilityTag = TargetAbilityTag;
	ContextEntry.RequiredContext = FGameplayTagQuery::MakeQuery_MatchTag(RequiredContextTag);
	ContextEntry.BlockedContext = FGameplayTagQuery::MakeQuery_MatchTag(BlockedContextTag);

	TestNull(TEXT("Entry should not resolve when required context is missing."), RouteAsset->FindEntryByInputTag(InputTag, FGameplayTagContainer()));

	FGameplayTagContainer ValidContext;
	ValidContext.AddTag(RequiredContextTag);
	TestNotNull(TEXT("Entry should resolve when required context is present."), RouteAsset->FindEntryByInputTag(InputTag, ValidContext));

	ValidContext.AddTag(BlockedContextTag);
	TestNull(TEXT("Entry should not resolve when blocked context is present."), RouteAsset->FindEntryByInputTag(InputTag, ValidContext));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboRouteExactAbilityTagTest,
	"RZCombat.Combo.DataLayer.SourceAbilityUsesExactTagMatching",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboRouteExactAbilityTagTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	const FGameplayTag AbilityATag = MakeRouteAssetTestTag(TEXT("Ability.Combo.Test.A"));
	const FGameplayTag AbilityABTag = MakeRouteAssetTestTag(TEXT("Ability.Combo.Test.AB"));
	const FGameplayTag InputTag = MakeRouteAssetTestTag(TEXT("Input.Action.Attack.Light"));
	const FGameplayTag WindowTag = MakeRouteAssetTestTag(TEXT("Combo.Window.Branch"));
	const FGameplayTag TargetAbilityTag = MakeRouteAssetTestTag(TEXT("Ability.Combo.Test.ABC"));

	URZCombatComboRouteAsset* RouteAsset = MakeRouteAsset();

	FRZCombatComboAbilityRoute& AbilityRoute = RouteAsset->AbilityRoutes.AddDefaulted_GetRef();
	AbilityRoute.SourceAbilityTag = AbilityATag;

	FRZCombatComboBranch& Branch = AbilityRoute.Branches.AddDefaulted_GetRef();
	Branch.RequiredInputTag = InputTag;
	Branch.RequiredWindowTag = WindowTag;
	Branch.TargetAbilityTag = TargetAbilityTag;

	FGameplayTagContainer ActiveWindows;
	ActiveWindows.AddTag(WindowTag);

	TestNull(TEXT("Ability.Combo.Test.A should not match Ability.Combo.Test.AB."), RouteAsset->ResolveBranch(AbilityABTag, InputTag, ActiveWindows, FGameplayTagContainer(), false));

	return true;
}

#endif

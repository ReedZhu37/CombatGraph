#if WITH_DEV_AUTOMATION_TESTS

#include "AbilitySystemComponent.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "RZCombatComboComponent.h"
#include "RZCombatComboAbilityTargetData.h"
#include "RZCombatComboComponentTestTypes.h"
#include "RZCombatComboPayload.h"
#include "RZCombatComboTableAsset.h"

#include "Abilities/GameplayAbility.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Misc/AutomationTest.h"
#include "NativeGameplayTags.h"

UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_RZCombatComboTestFailure, "RZCombat.Test.Ability.Fail");

bool URZCombatComboTransitionCheckerTestDouble::bHasObservedContext = false;
TWeakObjectPtr<URZCombatComboComponent> URZCombatComboTransitionCheckerTestDouble::ObservedComboComponent = nullptr;
TWeakObjectPtr<APawn> URZCombatComboTransitionCheckerTestDouble::ObservedPawn = nullptr;
FGameplayTag URZCombatComboTransitionCheckerTestDouble::ObservedTriggerTag;
FName URZCombatComboTransitionCheckerTestDouble::ObservedSourceNodeName = NAME_None;
FName URZCombatComboTransitionCheckerTestDouble::ObservedTargetNodeName = NAME_None;

void URZCombatComboTransitionCheckerTestDouble::ResetObservedContext()
{
	bHasObservedContext = false;
	ObservedComboComponent.Reset();
	ObservedPawn.Reset();
	ObservedTriggerTag = FGameplayTag();
	ObservedSourceNodeName = NAME_None;
	ObservedTargetNodeName = NAME_None;
}

bool URZCombatComboTransitionCheckerTestDouble::HasObservedContext()
{
	return bHasObservedContext;
}

URZCombatComboComponent* URZCombatComboTransitionCheckerTestDouble::GetObservedComboComponent()
{
	return ObservedComboComponent.Get();
}

APawn* URZCombatComboTransitionCheckerTestDouble::GetObservedPawn()
{
	return ObservedPawn.Get();
}

FGameplayTag URZCombatComboTransitionCheckerTestDouble::GetObservedTriggerTag()
{
	return ObservedTriggerTag;
}

FName URZCombatComboTransitionCheckerTestDouble::GetObservedSourceNodeName()
{
	return ObservedSourceNodeName;
}

FName URZCombatComboTransitionCheckerTestDouble::GetObservedTargetNodeName()
{
	return ObservedTargetNodeName;
}

bool URZCombatComboTransitionCheckerTestDouble::CanUseTransition_Implementation(
	const FRZCombatComboTransitionCheckContext& Context
) const
{
	bHasObservedContext = true;
	ObservedComboComponent = Context.ComboComponent;
	ObservedPawn = Context.Pawn;
	ObservedTriggerTag = Context.Request.TriggerTag;
	ObservedSourceNodeName = Context.SourceNode.NodeId.Name;
	ObservedTargetNodeName = Context.TargetNode.NodeId.Name;
	return bAllowTransition;
}

URZCombatComboGASLifecycleTestAbility::URZCombatComboGASLifecycleTestAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void URZCombatComboGASLifecycleTestAbility::SetTestSpecHandle(FGameplayAbilitySpecHandle InHandle)
{
	CurrentSpecHandle = InHandle;
}

URZCombatComboGASLifecycleOtherAbility::URZCombatComboGASLifecycleOtherAbility()
{
	InstancingPolicy = EGameplayAbilityInstancingPolicy::InstancedPerActor;
}

void URZCombatComboGASLifecycleOtherAbility::SetTestSpecHandle(FGameplayAbilitySpecHandle InHandle)
{
	CurrentSpecHandle = InHandle;
}

namespace
{
	FGameplayTag MakeTestTag(const TCHAR* TagName)
	{
		return FGameplayTag::RequestGameplayTag(FName(TagName), false);
	}

	URZCombatComboTableAsset* MakeTable()
	{
		return NewObject<URZCombatComboTableAsset>();
	}

	FRZCombatComboNode& AddNode(
		URZCombatComboTableAsset* Table,
		const TCHAR* NodeName,
		TSubclassOf<UGameplayAbility> AbilityClass = UGameplayAbility::StaticClass()
	)
	{
		FRZCombatComboNode& Node = Table->Nodes.AddDefaulted_GetRef();
		Node.NodeId.Name = NodeName;
		Node.AbilityClass = AbilityClass;
		return Node;
	}

	FRZCombatComboNode& AddNodeWithoutAbility(URZCombatComboTableAsset* Table, const TCHAR* NodeName)
	{
		return AddNode(Table, NodeName, nullptr);
	}

	FRZCombatComboTransition& AddTransition(
		TArray<FRZCombatComboTransition>& Transitions,
		FGameplayTag TriggerTag,
		const FRZCombatComboNodeId& TargetNodeId,
		int32 Priority = 0
	)
	{
		FRZCombatComboTransition& Transition = Transitions.AddDefaulted_GetRef();
		Transition.TriggerTag = TriggerTag;
		Transition.TargetNodeId = TargetNodeId;
		Transition.Priority = Priority;
		return Transition;
	}

	URZCombatComboTransitionCheckerTestDouble* MakeChecker(UObject* Outer, bool bAllowTransition)
	{
		URZCombatComboTransitionCheckerTestDouble* Checker = NewObject<URZCombatComboTransitionCheckerTestDouble>(Outer);
		Checker->bAllowTransition = bAllowTransition;
		return Checker;
	}

	URZCombatComboTransitionCheckerTestDouble* AddChecker(
		FRZCombatComboTransition& Transition,
		UObject* Outer,
		bool bAllowTransition
	)
	{
		URZCombatComboTransitionCheckerTestDouble* Checker = MakeChecker(Outer, bAllowTransition);
		Transition.Checker = Checker;
		return Checker;
	}

	URZCombatComboComponent* MakeComponentWithActiveTable(URZCombatComboTableAsset* Table)
	{
		URZCombatComboComponent* Component = NewObject<URZCombatComboComponent>();
		Component->AddComboTable(Table);
		Component->SetActiveComboTable(Table);
		return Component;
	}

	APawn* MakeTestPawn()
	{
		if (UWorld* World = GWorld)
		{
			return World->SpawnActor<APawn>();
		}

		return NewObject<APawn>();
	}

	URZCombatComboComponent* MakeOwnedComponentWithActiveTable(APawn* Pawn, URZCombatComboTableAsset* Table)
	{
		URZCombatComboComponent* Component = NewObject<URZCombatComboComponent>(Pawn);
		if (Pawn)
		{
			Pawn->AddOwnedComponent(Component);
			if (Pawn->GetWorld())
			{
				Component->RegisterComponent();
			}
		}

		Component->AddComboTable(Table);
		Component->SetActiveComboTable(Table);
		return Component;
	}

	UAbilitySystemComponent* MakeAbilitySystemComponent(APawn* Pawn)
	{
		UAbilitySystemComponent* AbilitySystemComponent = NewObject<UAbilitySystemComponent>(Pawn);
		if (Pawn)
		{
			Pawn->AddOwnedComponent(AbilitySystemComponent);
			if (Pawn->GetWorld())
			{
				AbilitySystemComponent->RegisterComponent();
			}

			AbilitySystemComponent->InitAbilityActorInfo(Pawn, Pawn);
		}

		return AbilitySystemComponent;
	}

	UAbilitySystemComponent* MakePlayerStateAbilitySystemComponent(APawn* Pawn)
	{
		APlayerState* PlayerState = nullptr;
		if (UWorld* World = Pawn ? Pawn->GetWorld() : GWorld)
		{
			PlayerState = World->SpawnActor<APlayerState>();
		}

		if (!PlayerState)
		{
			PlayerState = NewObject<APlayerState>();
		}

		if (Pawn)
		{
			Pawn->SetPlayerState(PlayerState);
		}

		UAbilitySystemComponent* AbilitySystemComponent = NewObject<UAbilitySystemComponent>(PlayerState);
		PlayerState->AddOwnedComponent(AbilitySystemComponent);
		if (PlayerState->GetWorld())
		{
			AbilitySystemComponent->RegisterComponent();
		}

		AbilitySystemComponent->InitAbilityActorInfo(PlayerState, Pawn);
		return AbilitySystemComponent;
	}

	void EnableAbilitySystemLifecycle(
		URZCombatComboComponent* ComboComponent,
		UAbilitySystemComponent* AbilitySystemComponent
	)
	{
		ComboComponent->AbilitySystemComponentOverride = AbilitySystemComponent;
		ComboComponent->bObserveAbilitySystemLifecycle = true;
		ComboComponent->BindToAbilitySystemLifecycle();
	}

	FGameplayAbilitySpecHandle GrantAbility(UAbilitySystemComponent* AbilitySystemComponent, TSubclassOf<UGameplayAbility> AbilityClass)
	{
		if (!AbilitySystemComponent)
		{
			return FGameplayAbilitySpecHandle();
		}

		FGameplayAbilitySpec Spec(AbilityClass, 1, INDEX_NONE);
		return AbilitySystemComponent->GiveAbility(Spec);
	}

	URZCombatComboGASLifecycleTestAbility* MakeObservedAbility(
		UAbilitySystemComponent* AbilitySystemComponent,
		FGameplayAbilitySpecHandle SpecHandle
	)
	{
		URZCombatComboGASLifecycleTestAbility* Ability = NewObject<URZCombatComboGASLifecycleTestAbility>(AbilitySystemComponent);
		Ability->SetTestSpecHandle(SpecHandle);
		return Ability;
	}

	URZCombatComboGASLifecycleOtherAbility* MakeObservedOtherAbility(
		UAbilitySystemComponent* AbilitySystemComponent,
		FGameplayAbilitySpecHandle SpecHandle
	)
	{
		URZCombatComboGASLifecycleOtherAbility* Ability = NewObject<URZCombatComboGASLifecycleOtherAbility>(AbilitySystemComponent);
		Ability->SetTestSpecHandle(SpecHandle);
		return Ability;
	}

	FRZCombatComboRequestResult MakeSuccessfulCommitResult(
		URZCombatComboTableAsset* Table,
		const FRZCombatComboNode& TargetNode
	)
	{
		FRZCombatComboRequestResult Result;
		Result.Result = ERZCombatComboRequestResultType::Success;
		Result.SelectedTable = Table;
		Result.TargetNodeId = TargetNode.NodeId;
		Result.AbilityClass = TargetNode.AbilityClass;
		Result.bResolved = true;
		return Result;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentTableManagementTest,
	"RZCombat.Combo.Component.TableManagementAndReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentTableManagementTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* FirstTable = MakeTable();
	URZCombatComboTableAsset* SecondTable = MakeTable();
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	FRZCombatComboNode& FirstNode = AddNode(FirstTable, TEXT("Light_1"));
	AddTransition(FirstTable->EntryTransitions, TriggerTag, FirstNode.NodeId);

	URZCombatComboComponent* Component = NewObject<URZCombatComboComponent>();
	Component->AddComboTable(FirstTable);

	TestEqual(TEXT("Adding the first table should make it active."), Component->GetActiveComboTable(), FirstTable);

	const FRZCombatComboRequestResult Result = Component->RequestCombo(TriggerTag);
	TestEqual(TEXT("Request should resolve before reset."), Result.Result, ERZCombatComboRequestResultType::Success);
	TestFalse(TEXT("Request should not set current node before commit."), Component->GetCurrentNodeId().IsValid());

	Component->CommitResolvedCombo(Result);
	TestTrue(TEXT("Commit should set current node before reset."), Component->GetCurrentNodeId().IsValid());

	Component->ResetCombo();
	TestFalse(TEXT("Reset should clear current node."), Component->GetCurrentNodeId().IsValid());

	Component->SetActiveComboTable(SecondTable);
	TestEqual(TEXT("SetActiveComboTable should switch active table."), Component->GetActiveComboTable(), SecondTable);

	Component->RemoveComboTable(SecondTable);
	TestNull(TEXT("Removing the active table should clear active table."), Component->GetActiveComboTable());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentPendingExecutionDebugTest,
	"RZCombat.Combo.Component.RequestSuccessRecordsPendingExecutionDebugState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentPendingExecutionDebugTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& Node = AddNode(Table, TEXT("Light_1"), URZCombatComboGASLifecycleTestAbility::StaticClass());
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	AddTransition(Table->EntryTransitions, TriggerTag, Node.NodeId);

	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);
	const FRZCombatComboRequestResult Result = Component->RequestCombo(TriggerTag);
	const FRZCombatComboDebugSnapshot Snapshot = Component->GetComboDebugSnapshot();

	TestEqual(TEXT("Request should resolve successfully."), Result.Result, ERZCombatComboRequestResultType::Success);
	TestEqual(TEXT("Successful request should mark execution as pending activation."), Snapshot.ExecutionState, ERZCombatComboExecutionState::ResolvedPendingActivation);
	TestEqual(TEXT("Pending execution debug should expose observed ability class."), Snapshot.ObservedAbilityClass.Get(), Node.AbilityClass.Get());
	TestTrue(TEXT("Pending debug snapshot should expose the last resolved target node."), Snapshot.LastTargetNodeId.Matches(Node.NodeId));
	TestTrue(TEXT("Pending debug snapshot should expose the pending target node."), Snapshot.PendingTargetNodeId.Matches(Node.NodeId));
	TestFalse(TEXT("Entry-source request should expose an empty last source node."), Snapshot.LastSourceNodeId.IsValid());
	TestFalse(TEXT("Entry-source pending request should expose an empty pending source node."), Snapshot.PendingSourceNodeId.IsValid());
	TestFalse(TEXT("Pending activation should not advance current node."), Snapshot.CurrentNodeId.IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboDebugEventOrderTest,
	"RZCombat.Combo.DebugEvents.SameFrameOrderAndHonestCommit",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboDebugEventOrderTest::RunTest(const FString& Parameters)
{
	URZCombatComboTableAsset* Table = MakeTable();
	const FRZCombatComboNodeId NodeId = AddNode(Table, TEXT("Light_1")).NodeId;
	const FGameplayTag Trigger = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	AddTransition(Table->EntryTransitions, Trigger, NodeId);
	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);
	const int32 First = Component->GetRecentComboDebugEvents().Num();
	const FRZCombatComboRequestResult Result = Component->RequestCombo(Trigger);
	const FGameplayAbilitySpec GrantedSpec(UGameplayAbility::StaticClass(), 1, INDEX_NONE);
	const FGameplayAbilitySpecHandle Spec = GrantedSpec.Handle;
	TestTrue(TEXT("Record matching activation."), Component->RecordResolvedAbilityActivated(Result, Spec, UGameplayAbility::StaticClass()));
	TestTrue(TEXT("Commit the combo state."), Component->CommitResolvedCombo(Result));
	TestEqual(TEXT("Combo advancement is not a GAS commit."), Component->GetComboDebugSnapshot().ExecutionState, ERZCombatComboExecutionState::ComboCommitted);
	TestFalse(TEXT("Committed combo is no longer pending activation."), Component->GetComboDebugSnapshot().PendingTargetNodeId.IsValid());
	TestTrue(TEXT("Record actual GAS commit separately."), Component->RecordResolvedAbilityCommitted(Result, Spec, UGameplayAbility::StaticClass()));
	TestTrue(TEXT("Record normal end."), Component->RecordResolvedAbilityEnded(Result, Spec, UGameplayAbility::StaticClass(), false, false));
	Component->ResetCombo();
	URZCombatComboTableAsset* OtherTable = MakeTable();
	Component->SetActiveComboTable(OtherTable);

	const ERZCombatComboDebugEventType Expected[] = {
		ERZCombatComboDebugEventType::Resolve, ERZCombatComboDebugEventType::AbilityActivated,
		ERZCombatComboDebugEventType::ComboCommit, ERZCombatComboDebugEventType::AbilityCommitted,
		ERZCombatComboDebugEventType::AbilityEnded, ERZCombatComboDebugEventType::Reset,
		ERZCombatComboDebugEventType::TableChanged, ERZCombatComboDebugEventType::Reset };
	const TArray<FRZCombatComboDebugEvent>& Events = Component->GetRecentComboDebugEvents();
	TestEqual(TEXT("Every same-frame event is retained without UI polling."), Events.Num() - First, static_cast<int32>(UE_ARRAY_COUNT(Expected)));
	if (Events.Num() - First != UE_ARRAY_COUNT(Expected))
	{
		return false;
	}
	for (int32 Index = 0; Index < UE_ARRAY_COUNT(Expected); ++Index)
	{
		const FRZCombatComboDebugEvent& Event = Events[First + Index];
		TestEqual(TEXT("Event order follows actual mutation order."), Event.Type, Expected[Index]);
		TestEqual(TEXT("Events occurred in the same frame."), Event.Frame, Events[First].Frame);
		if (Index > 0)
		{
			TestEqual(TEXT("Sequence increases even within one frame."), Event.Sequence, Events[First + Index - 1].Sequence + 1);
		}
	}
	TestTrue(TEXT("Resolve event preserves target identity."), Events[First].TargetNodeId.Matches(NodeId));
	TestEqual(TEXT("Combo commit event preserves spec."), Events[First + 2].AbilitySpecHandle, Spec);
	TestEqual(TEXT("Table switch preserves previous identity."), Events[First + 6].PreviousTablePath, Table->GetPathName());
	TestEqual(TEXT("Table switch preserves new identity."), Events[First + 6].TablePath, OtherTable->GetPathName());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboDebugResetNotificationTest,
	"RZCombat.Combo.DebugEvents.ResetBroadcastsClearedState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboDebugResetNotificationTest::RunTest(const FString& Parameters)
{
	URZCombatComboTableAsset* Table = MakeTable();
	const FRZCombatComboNodeId NodeId = AddNode(Table, TEXT("Light_1")).NodeId;
	const FGameplayTag Trigger = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	AddTransition(Table->EntryTransitions, Trigger, NodeId);
	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);
	URZCombatComboStateChangedTestListener* Listener = NewObject<URZCombatComboStateChangedTestListener>();
	Component->OnComboStateChanged.AddDynamic(Listener, &URZCombatComboStateChangedTestListener::HandleStateChanged);
	Component->CommitResolvedCombo(Component->RequestCombo(Trigger));
	const int32 BeforeReset = Listener->NotificationCount;
	Component->ResetCombo();
	TestEqual(TEXT("Reset broadcasts exactly one complete state notification."), Listener->NotificationCount, BeforeReset + 1);
	TestFalse(TEXT("Reset notification clears current."), Listener->LastSnapshot.CurrentNodeId.IsValid());
	TestFalse(TEXT("Reset notification clears pending."), Listener->LastSnapshot.PendingTargetNodeId.IsValid());
	TestEqual(TEXT("Reset notification clears execution state."), Listener->LastSnapshot.ExecutionState, ERZCombatComboExecutionState::None);
	TestEqual(TEXT("Reset has an explicit event, not an inferred missing highlight."), Component->GetRecentComboDebugEvents().Last().Type, ERZCombatComboDebugEventType::Reset);
	Component->OnComboStateChanged.RemoveAll(Listener);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboDebugSameFrameTransitionsTest,
	"RZCombat.Combo.DebugEvents.SameFrameTransitionsAndCancellationReset",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboDebugSameFrameTransitionsTest::RunTest(const FString& Parameters)
{
	URZCombatComboTableAsset* Table = MakeTable();
	const FRZCombatComboNodeId FirstId = AddNode(Table, TEXT("Light_1")).NodeId;
	const FRZCombatComboNodeId SecondId = AddNode(Table, TEXT("Light_2")).NodeId;
	const FGameplayTag Trigger = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	AddTransition(Table->EntryTransitions, Trigger, FirstId);
	AddTransition(Table->Nodes[0].Transitions, Trigger, SecondId);
	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);
	const int32 FirstEventIndex = Component->GetRecentComboDebugEvents().Num();
	TestTrue(TEXT("First resolve can commit immediately."), Component->CommitResolvedCombo(Component->RequestCombo(Trigger)));
	const FRZCombatComboRequestResult SecondResult = Component->RequestCombo(Trigger);
	TestTrue(TEXT("The next resolve starts at the committed node in the same frame."), SecondResult.SourceNodeId.Matches(FirstId));
	TestTrue(TEXT("The second transition can commit in the same frame."), Component->CommitResolvedCombo(SecondResult));
	TestTrue(TEXT("Second commit advances to the second target."), Component->GetCurrentNodeId().Matches(SecondId));
	TestTrue(TEXT("Cancellation records the configured reset."), Component->RecordResolvedAbilityEnded(
		SecondResult, FGameplayAbilitySpecHandle(), UGameplayAbility::StaticClass(), true, true));
	const TArray<FRZCombatComboDebugEvent>& Events = Component->GetRecentComboDebugEvents();
	TestEqual(TEXT("Both resolves and commits plus cancellation and reset survive polling."), Events.Num() - FirstEventIndex, 6);
	TestTrue(TEXT("Second resolve preserves the actual transition source."), Events[FirstEventIndex + 2].SourceNodeId.Matches(FirstId));
	TestTrue(TEXT("Second commit preserves the actual transition target."), Events[FirstEventIndex + 3].TargetNodeId.Matches(SecondId));
	TestEqual(TEXT("Cancellation precedes its reset event."), Events.Last(1).Type, ERZCombatComboDebugEventType::AbilityCancelled);
	TestEqual(TEXT("Reset is explicit even when combined with a GAS lifecycle record."), Events.Last().Type, ERZCombatComboDebugEventType::Reset);
	TestEqual(TEXT("All transitions and reset occurred in one frame."), Events[FirstEventIndex].Frame, Events.Last().Frame);
	TestFalse(TEXT("Cancellation actually clears current state."), Component->GetCurrentNodeId().IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboDebugHistoryCapacityTest,
	"RZCombat.Combo.DebugEvents.CapacityExpiresOldestWithoutResettingSequence",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboDebugHistoryCapacityTest::RunTest(const FString& Parameters)
{
	URZCombatComboComponent* Component = NewObject<URZCombatComboComponent>();
	for (int32 Index = 0; Index < Component->MaxRecentComboDebugEvents + 17; ++Index)
	{
		Component->ResetCombo();
	}
	const TArray<FRZCombatComboDebugEvent>& Events = Component->GetRecentComboDebugEvents();
	TestEqual(TEXT("History has a hard capacity."), Events.Num(), Component->MaxRecentComboDebugEvents);
	TestEqual(TEXT("Oldest events expire first."), Events[0].Sequence, uint64(18));
	TestEqual(TEXT("Sequence is monotonic across reset and eviction."), Events.Last().Sequence, uint64(Component->MaxRecentComboDebugEvents + 17));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboDebugObservationReadOnlyTest,
	"RZCombat.Combo.DebugEvents.ObservationDoesNotEvaluateCheckerOrChangePolicies",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboDebugObservationReadOnlyTest::RunTest(const FString& Parameters)
{
	URZCombatComboTableAsset* Table = MakeTable();
	const FRZCombatComboNodeId NodeId = AddNode(Table, TEXT("Light_1")).NodeId;
	FRZCombatComboTransition& Transition = AddTransition(Table->EntryTransitions, MakeTestTag(TEXT("Input.Action.Attack.Light")), NodeId);
	AddChecker(Transition, Table, false);
	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);
	URZCombatComboTransitionCheckerTestDouble::ResetObservedContext();
	const int32 Count = Component->GetRecentComboDebugEvents().Num();
	const FRZCombatComboDebugSnapshot Snapshot = Component->GetComboDebugSnapshot();
	Component->GetRecentComboDebugEvents();
	TestFalse(TEXT("Reading structural candidates must not run authored Checkers."), URZCombatComboTransitionCheckerTestDouble::HasObservedContext());
	TestEqual(TEXT("Structural candidate is visible even though the Checker would fail."), Snapshot.AvailableTargetNodeIds.Num(), 1);
	TestEqual(TEXT("Observation emits no gameplay or debug mutations."), Component->GetRecentComboDebugEvents().Num(), Count);
	TestFalse(TEXT("Observation does not enable GAS lifecycle."), Component->bObserveAbilitySystemLifecycle);
	TestTrue(TEXT("Observation leaves commit policy untouched."), Component->bCommitComboOnAbilityActivated);
	TestTrue(TEXT("Observation leaves cancellation reset policy untouched."), Component->bResetComboOnAbilityCancelled);
	TestFalse(TEXT("Observation leaves normal-end reset policy untouched."), Component->bResetComboOnAbilityEnded);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboLateAbilitySystemTest,
	"RZCombat.Combo.GASLifecycle.LatePlayerStateASCBindsOnRequest",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboLateAbilitySystemTest::RunTest(const FString& Parameters)
{
	APawn* Pawn = MakeTestPawn();
	URZCombatComboTableAsset* Table = MakeTable();
	const FRZCombatComboNodeId NodeId = AddNode(Table, TEXT("Light_1"), URZCombatComboGASLifecycleTestAbility::StaticClass()).NodeId;
	const FGameplayTag Trigger = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	AddTransition(Table->EntryTransitions, Trigger, NodeId);
	URZCombatComboComponent* Component = MakeOwnedComponentWithActiveTable(Pawn, Table);
	Component->bObserveAbilitySystemLifecycle = true;
	TestFalse(TEXT("Initial binding attempt before PlayerState / ASC exists fails safely."), Component->BindToAbilitySystemLifecycle());
	UAbilitySystemComponent* ASC = MakePlayerStateAbilitySystemComponent(Pawn);
	const FGameplayAbilitySpecHandle Spec = GrantAbility(ASC, URZCombatComboGASLifecycleTestAbility::StaticClass());
	Component->RequestCombo(Trigger);
	TestEqual(TEXT("The request boundary lazily binds the late PlayerState ASC."), Component->GetBoundAbilitySystemComponent(), ASC);
	ASC->AbilityActivatedCallbacks.Broadcast(MakeObservedAbility(ASC, Spec));
	TestTrue(TEXT("First request after late initialization can commit."), Component->GetCurrentNodeId().Matches(NodeId));
	TestFalse(TEXT("No permanent component Tick is introduced."), Component->PrimaryComponentTick.bCanEverTick);

	Component->ResetCombo();
	Component->UnbindFromAbilitySystemLifecycle();
	Component->RequestCombo(Trigger);
	TestFalse(TEXT("Explicit Unbind is not silently undone by lazy binding."), Component->IsBoundToAbilitySystemLifecycle());
	URZCombatComboComponent* ManualOnly = MakeOwnedComponentWithActiveTable(Pawn, Table);
	ManualOnly->bObserveAbilitySystemLifecycle = true;
	ManualOnly->bAutoBindAbilitySystemOnBeginPlay = false;
	ManualOnly->RequestCombo(Trigger);
	TestFalse(TEXT("Auto-bind opt-out is respected without an explicit Unbind."), ManualOnly->IsBoundToAbilitySystemLifecycle());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGASLifecycleActivatedAbilityCommitsComboTest,
	"RZCombat.Combo.GASLifecycle.MatchingAbilityActivatedCommitsCombo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGASLifecycleActivatedAbilityCommitsComboTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	APawn* Pawn = MakeTestPawn();
	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& Node = AddNode(Table, TEXT("Light_1"), URZCombatComboGASLifecycleTestAbility::StaticClass());
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	AddTransition(Table->EntryTransitions, TriggerTag, Node.NodeId);

	URZCombatComboComponent* ComboComponent = MakeOwnedComponentWithActiveTable(Pawn, Table);
	UAbilitySystemComponent* AbilitySystemComponent = MakeAbilitySystemComponent(Pawn);
	EnableAbilitySystemLifecycle(ComboComponent, AbilitySystemComponent);
	const FGameplayAbilitySpecHandle SpecHandle = GrantAbility(AbilitySystemComponent, URZCombatComboGASLifecycleTestAbility::StaticClass());
	URZCombatComboGASLifecycleTestAbility* Ability = MakeObservedAbility(AbilitySystemComponent, SpecHandle);

	const FRZCombatComboRequestResult Result = ComboComponent->RequestCombo(TriggerTag);
	AbilitySystemComponent->AbilityActivatedCallbacks.Broadcast(Ability);

	const FRZCombatComboDebugSnapshot Snapshot = ComboComponent->GetComboDebugSnapshot();
	TestEqual(TEXT("Test setup should resolve combo."), Result.Result, ERZCombatComboRequestResultType::Success);
	TestTrue(TEXT("Matching ability activation should commit current node."), ComboComponent->GetCurrentNodeId().Matches(Node.NodeId));
	TestEqual(TEXT("GAS lifecycle should record the observed spec handle."), Snapshot.ObservedAbilitySpecHandle, SpecHandle);
	TestEqual(TEXT("Activation advances combo state, but does not claim a GAS commit."), Snapshot.ExecutionState, ERZCombatComboExecutionState::ComboCommitted);
	TestTrue(TEXT("Combo component should remain bound for the activation path."), ComboComponent->IsBoundToAbilitySystemLifecycle());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGASLifecycleOwnerPawnPlayerStateAbilitySystemCommitsComboTest,
	"RZCombat.Combo.GASLifecycle.OwnerPawnPlayerStateAbilitySystemCommitsCombo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGASLifecycleOwnerPawnPlayerStateAbilitySystemCommitsComboTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	APawn* Pawn = MakeTestPawn();
	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& Node = AddNode(Table, TEXT("Light_1"), URZCombatComboGASLifecycleTestAbility::StaticClass());
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	AddTransition(Table->EntryTransitions, TriggerTag, Node.NodeId);

	URZCombatComboComponent* ComboComponent = MakeOwnedComponentWithActiveTable(Pawn, Table);
	UAbilitySystemComponent* AbilitySystemComponent = MakePlayerStateAbilitySystemComponent(Pawn);
	ComboComponent->bObserveAbilitySystemLifecycle = true;
	const bool bBound = ComboComponent->BindToAbilitySystemLifecycle();
	const FGameplayAbilitySpecHandle SpecHandle = GrantAbility(AbilitySystemComponent, URZCombatComboGASLifecycleTestAbility::StaticClass());
	URZCombatComboGASLifecycleTestAbility* Ability = MakeObservedAbility(AbilitySystemComponent, SpecHandle);

	const FRZCombatComboRequestResult Result = ComboComponent->RequestCombo(TriggerTag);
	AbilitySystemComponent->AbilityActivatedCallbacks.Broadcast(Ability);

	const FRZCombatComboDebugSnapshot Snapshot = ComboComponent->GetComboDebugSnapshot();
	TestTrue(TEXT("Combo component should auto-resolve the owner's PlayerState ASC."), bBound);
	TestEqual(TEXT("Test setup should resolve combo."), Result.Result, ERZCombatComboRequestResultType::Success);
	TestTrue(TEXT("Matching PlayerState ASC ability activation should commit current node."), ComboComponent->GetCurrentNodeId().Matches(Node.NodeId));
	TestEqual(TEXT("PlayerState ASC lifecycle should record the observed spec handle."), Snapshot.ObservedAbilitySpecHandle, SpecHandle);
	TestEqual(TEXT("PlayerState ASC activation commits combo state, not GAS cost/cooldown."), Snapshot.ExecutionState, ERZCombatComboExecutionState::ComboCommitted);
	TestEqual(TEXT("Combo component should bind to the PlayerState ASC."), ComboComponent->GetBoundAbilitySystemComponent(), AbilitySystemComponent);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGASLifecycleNonMatchingAbilityIgnoredTest,
	"RZCombat.Combo.GASLifecycle.NonMatchingAbilityActivatedDoesNotCommitCombo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGASLifecycleNonMatchingAbilityIgnoredTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	APawn* Pawn = MakeTestPawn();
	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& Node = AddNode(Table, TEXT("Light_1"), URZCombatComboGASLifecycleTestAbility::StaticClass());
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	AddTransition(Table->EntryTransitions, TriggerTag, Node.NodeId);

	URZCombatComboComponent* ComboComponent = MakeOwnedComponentWithActiveTable(Pawn, Table);
	UAbilitySystemComponent* AbilitySystemComponent = MakeAbilitySystemComponent(Pawn);
	EnableAbilitySystemLifecycle(ComboComponent, AbilitySystemComponent);
	const FGameplayAbilitySpecHandle OtherSpecHandle = GrantAbility(AbilitySystemComponent, URZCombatComboGASLifecycleOtherAbility::StaticClass());
	URZCombatComboGASLifecycleOtherAbility* OtherAbility = MakeObservedOtherAbility(AbilitySystemComponent, OtherSpecHandle);

	ComboComponent->RequestCombo(TriggerTag);
	AbilitySystemComponent->AbilityActivatedCallbacks.Broadcast(OtherAbility);

	const FRZCombatComboDebugSnapshot Snapshot = ComboComponent->GetComboDebugSnapshot();
	TestFalse(TEXT("Non-matching ability activation should not commit current node."), ComboComponent->GetCurrentNodeId().IsValid());
	TestEqual(TEXT("Non-matching ability should leave pending debug state untouched."), Snapshot.ExecutionState, ERZCombatComboExecutionState::ResolvedPendingActivation);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGASLifecycleAbilityCommittedUpdatesDebugOnlyTest,
	"RZCombat.Combo.GASLifecycle.MatchingAbilityCommittedUpdatesDebugWithoutAdvancing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGASLifecycleAbilityCommittedUpdatesDebugOnlyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	APawn* Pawn = MakeTestPawn();
	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& Node = AddNode(Table, TEXT("Light_1"), URZCombatComboGASLifecycleTestAbility::StaticClass());
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	AddTransition(Table->EntryTransitions, TriggerTag, Node.NodeId);

	URZCombatComboComponent* ComboComponent = MakeOwnedComponentWithActiveTable(Pawn, Table);
	UAbilitySystemComponent* AbilitySystemComponent = MakeAbilitySystemComponent(Pawn);
	EnableAbilitySystemLifecycle(ComboComponent, AbilitySystemComponent);
	ComboComponent->bCommitComboOnAbilityActivated = false;
	const FGameplayAbilitySpecHandle SpecHandle = GrantAbility(AbilitySystemComponent, URZCombatComboGASLifecycleTestAbility::StaticClass());
	URZCombatComboGASLifecycleTestAbility* Ability = MakeObservedAbility(AbilitySystemComponent, SpecHandle);

	ComboComponent->RequestCombo(TriggerTag);
	AbilitySystemComponent->AbilityCommittedCallbacks.Broadcast(Ability);

	const FRZCombatComboDebugSnapshot Snapshot = ComboComponent->GetComboDebugSnapshot();
	TestFalse(TEXT("Ability committed callback should not advance combo by itself."), ComboComponent->GetCurrentNodeId().IsValid());
	TestEqual(TEXT("Ability committed callback should update execution state."), Snapshot.ExecutionState, ERZCombatComboExecutionState::AbilityCommitted);
	TestEqual(TEXT("Ability committed callback should keep observed spec handle."), Snapshot.ObservedAbilitySpecHandle, SpecHandle);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGASLifecycleAbilityFailedClearsPendingTest,
	"RZCombat.Combo.GASLifecycle.MatchingAbilityFailedClearsPendingWithoutAdvancing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGASLifecycleAbilityFailedClearsPendingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	APawn* Pawn = MakeTestPawn();
	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& Node = AddNode(Table, TEXT("Light_1"), URZCombatComboGASLifecycleTestAbility::StaticClass());
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	const FGameplayTag FailureTag = TAG_RZCombatComboTestFailure.GetTag();
	AddTransition(Table->EntryTransitions, TriggerTag, Node.NodeId);

	URZCombatComboComponent* ComboComponent = MakeOwnedComponentWithActiveTable(Pawn, Table);
	UAbilitySystemComponent* AbilitySystemComponent = MakeAbilitySystemComponent(Pawn);
	EnableAbilitySystemLifecycle(ComboComponent, AbilitySystemComponent);
	const FGameplayAbilitySpecHandle SpecHandle = GrantAbility(AbilitySystemComponent, URZCombatComboGASLifecycleTestAbility::StaticClass());
	URZCombatComboGASLifecycleTestAbility* Ability = MakeObservedAbility(AbilitySystemComponent, SpecHandle);

	const FRZCombatComboRequestResult Result = ComboComponent->RequestCombo(TriggerTag);
	FGameplayTagContainer FailureTags;
	FailureTags.AddTag(FailureTag);
	AbilitySystemComponent->AbilityFailedCallbacks.Broadcast(Ability, FailureTags);

	const FRZCombatComboDebugSnapshot Snapshot = ComboComponent->GetComboDebugSnapshot();
	TestFalse(TEXT("Ability failure should not advance combo."), ComboComponent->GetCurrentNodeId().IsValid());
	TestEqual(TEXT("Ability failure should update execution state."), Snapshot.ExecutionState, ERZCombatComboExecutionState::AbilityFailed);
	TestTrue(TEXT("Ability failure should keep the last resolved target for graph debug."), Snapshot.LastTargetNodeId.Matches(Node.NodeId));
	TestTrue(TEXT("Ability failure should keep failure tags in debug snapshot."), Snapshot.ObservedFailureTags.HasTagExact(FailureTag));
	TestEqual(TEXT("The failure is also captured as an explicit event."), ComboComponent->GetRecentComboDebugEvents().Last().Type, ERZCombatComboDebugEventType::AbilityFailed);
	TestTrue(TEXT("Failure tags survive event capture."), ComboComponent->GetRecentComboDebugEvents().Last().FailureTags.HasTagExact(FailureTag));
	TestFalse(TEXT("Ability failure should clear pending commit result."), ComboComponent->CommitResolvedCombo(Result));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGASLifecycleAbilityCancelledResetsComboTest,
	"RZCombat.Combo.GASLifecycle.MatchingAbilityCancelledResetsCombo",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGASLifecycleAbilityCancelledResetsComboTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	APawn* Pawn = MakeTestPawn();
	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& Node = AddNode(Table, TEXT("Light_1"), URZCombatComboGASLifecycleTestAbility::StaticClass());
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	AddTransition(Table->EntryTransitions, TriggerTag, Node.NodeId);

	URZCombatComboComponent* ComboComponent = MakeOwnedComponentWithActiveTable(Pawn, Table);
	UAbilitySystemComponent* AbilitySystemComponent = MakeAbilitySystemComponent(Pawn);
	EnableAbilitySystemLifecycle(ComboComponent, AbilitySystemComponent);
	const FGameplayAbilitySpecHandle SpecHandle = GrantAbility(AbilitySystemComponent, URZCombatComboGASLifecycleTestAbility::StaticClass());
	URZCombatComboGASLifecycleTestAbility* Ability = MakeObservedAbility(AbilitySystemComponent, SpecHandle);

	ComboComponent->RequestCombo(TriggerTag);
	AbilitySystemComponent->AbilityActivatedCallbacks.Broadcast(Ability);
	AbilitySystemComponent->OnAbilityEnded.Broadcast(FAbilityEndedData(Ability, SpecHandle, false, true));

	const FRZCombatComboDebugSnapshot Snapshot = ComboComponent->GetComboDebugSnapshot();
	TestFalse(TEXT("Cancelled ability should reset current combo node by default."), ComboComponent->GetCurrentNodeId().IsValid());
	TestEqual(TEXT("Cancelled ability should record cancelled execution state."), Snapshot.ExecutionState, ERZCombatComboExecutionState::AbilityCancelled);
	TestTrue(TEXT("Cancelled ability should keep the last resolved target for graph debug."), Snapshot.LastTargetNodeId.Matches(Node.NodeId));
	TestTrue(TEXT("Cancelled ability should expose cancellation flag."), Snapshot.bObservedAbilityWasCancelled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGASLifecycleAbilityEndedRecordsDebugWithoutResetTest,
	"RZCombat.Combo.GASLifecycle.MatchingAbilityEndedRecordsDebugWithoutResetByDefault",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGASLifecycleAbilityEndedRecordsDebugWithoutResetTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	APawn* Pawn = MakeTestPawn();
	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& Node = AddNode(Table, TEXT("Light_1"), URZCombatComboGASLifecycleTestAbility::StaticClass());
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	AddTransition(Table->EntryTransitions, TriggerTag, Node.NodeId);

	URZCombatComboComponent* ComboComponent = MakeOwnedComponentWithActiveTable(Pawn, Table);
	UAbilitySystemComponent* AbilitySystemComponent = MakeAbilitySystemComponent(Pawn);
	EnableAbilitySystemLifecycle(ComboComponent, AbilitySystemComponent);
	const FGameplayAbilitySpecHandle SpecHandle = GrantAbility(AbilitySystemComponent, URZCombatComboGASLifecycleTestAbility::StaticClass());
	URZCombatComboGASLifecycleTestAbility* Ability = MakeObservedAbility(AbilitySystemComponent, SpecHandle);

	ComboComponent->RequestCombo(TriggerTag);
	AbilitySystemComponent->AbilityActivatedCallbacks.Broadcast(Ability);
	AbilitySystemComponent->OnAbilityEnded.Broadcast(FAbilityEndedData(Ability, SpecHandle, false, false));

	const FRZCombatComboDebugSnapshot Snapshot = ComboComponent->GetComboDebugSnapshot();
	TestTrue(TEXT("Normal ability end should keep committed node by default."), ComboComponent->GetCurrentNodeId().Matches(Node.NodeId));
	TestEqual(TEXT("Normal ability end should record ended execution state."), Snapshot.ExecutionState, ERZCombatComboExecutionState::AbilityEnded);
	TestFalse(TEXT("Normal ability end should expose non-cancel flag."), Snapshot.bObservedAbilityWasCancelled);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboGASLifecycleUnbindStopsCallbacksTest,
	"RZCombat.Combo.GASLifecycle.UnbindStopsCallbacks",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboGASLifecycleUnbindStopsCallbacksTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	APawn* Pawn = MakeTestPawn();
	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& Node = AddNode(Table, TEXT("Light_1"), URZCombatComboGASLifecycleTestAbility::StaticClass());
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	AddTransition(Table->EntryTransitions, TriggerTag, Node.NodeId);

	URZCombatComboComponent* ComboComponent = MakeOwnedComponentWithActiveTable(Pawn, Table);
	UAbilitySystemComponent* AbilitySystemComponent = MakeAbilitySystemComponent(Pawn);
	EnableAbilitySystemLifecycle(ComboComponent, AbilitySystemComponent);
	const FGameplayAbilitySpecHandle SpecHandle = GrantAbility(AbilitySystemComponent, URZCombatComboGASLifecycleTestAbility::StaticClass());
	URZCombatComboGASLifecycleTestAbility* Ability = MakeObservedAbility(AbilitySystemComponent, SpecHandle);

	ComboComponent->RequestCombo(TriggerTag);
	ComboComponent->UnbindFromAbilitySystemLifecycle();
	AbilitySystemComponent->AbilityActivatedCallbacks.Broadcast(Ability);

	const FRZCombatComboDebugSnapshot Snapshot = ComboComponent->GetComboDebugSnapshot();
	TestFalse(TEXT("Unbound combo component should not commit combo from later callbacks."), ComboComponent->GetCurrentNodeId().IsValid());
	TestEqual(TEXT("Unbound combo component should leave pending debug state untouched."), Snapshot.ExecutionState, ERZCombatComboExecutionState::ResolvedPendingActivation);
	TestFalse(TEXT("Combo component should report unbound lifecycle state."), ComboComponent->IsBoundToAbilitySystemLifecycle());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentNoActiveTableTest,
	"RZCombat.Combo.Component.NoActiveTable",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentNoActiveTableTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboComponent* Component = NewObject<URZCombatComboComponent>();

	const FRZCombatComboRequestResult Result = Component->RequestCombo(MakeTestTag(TEXT("Input.Action.Attack.Light")));

	TestEqual(TEXT("Request without active table should fail."), Result.Result, ERZCombatComboRequestResultType::NoActiveTable);
	TestEqual(TEXT("Debug snapshot should keep last result."), Component->GetComboDebugSnapshot().LastResult, ERZCombatComboRequestResultType::NoActiveTable);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentNoEntryTransitionTest,
	"RZCombat.Combo.Component.NoEntryTransition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentNoEntryTransitionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);

	const FRZCombatComboRequestResult Result = Component->RequestCombo(MakeTestTag(TEXT("Input.Action.Attack.Light")));

	TestEqual(TEXT("Empty entry transition list should fail before matching."), Result.Result, ERZCombatComboRequestResultType::NoEntryTransition);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentNoMatchingTriggerTest,
	"RZCombat.Combo.Component.NoMatchingTrigger",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentNoMatchingTriggerTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& Node = AddNode(Table, TEXT("Light_1"));
	AddTransition(Table->EntryTransitions, MakeTestTag(TEXT("Input.Action.Attack.Heavy")), Node.NodeId);

	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);

	const FRZCombatComboRequestResult Result = Component->RequestCombo(MakeTestTag(TEXT("Input.Action.Attack.Light")));

	TestEqual(TEXT("Different trigger should not match."), Result.Result, ERZCombatComboRequestResultType::NoMatchingTrigger);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentEntryResolutionTest,
	"RZCombat.Combo.Component.EntryTriggerResolvesTargetWithoutAdvancing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentEntryResolutionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& Node = AddNode(Table, TEXT("Light_1"));
	FRZCombatComboTransition& Entry = AddTransition(Table->EntryTransitions, MakeTestTag(TEXT("Input.Action.Attack.Light")), Node.NodeId);

	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);

	const FRZCombatComboRequestResult Result = Component->RequestCombo(Entry.TriggerTag);

	TestEqual(TEXT("Request should resolve successfully."), Result.Result, ERZCombatComboRequestResultType::Success);
	TestTrue(TEXT("Request should report that a transition was resolved."), Result.bResolved);
	TestEqual(TEXT("Resolved result should return target ability class."), Result.AbilityClass.Get(), Node.AbilityClass.Get());
	TestTrue(TEXT("Resolved result should return target node id."), Result.TargetNodeId.Matches(Node.NodeId));
	TestFalse(TEXT("Current node should remain invalid until commit."), Component->GetCurrentNodeId().IsValid());
	TestFalse(TEXT("Debug snapshot should not expose an active ability before commit."), Component->GetComboDebugSnapshot().CurrentAbilityClass != nullptr);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentResolvedTransitionPayloadTest,
	"RZCombat.Combo.Component.ResolvedTransitionExposesPayload",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentResolvedTransitionPayloadTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& Node = AddNode(Table, TEXT("Light_1"));
	FRZCombatComboTransition& Entry = AddTransition(Table->EntryTransitions, MakeTestTag(TEXT("Input.Action.Attack.Light")), Node.NodeId);
	URZCombatComboPayload* Payload = NewObject<URZCombatComboPayload>(Table);
	Entry.Payload = Payload;

	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);
	const FRZCombatComboRequestResult Result = Component->RequestCombo(Entry.TriggerTag);

	TestEqual(TEXT("Request should resolve successfully."), Result.Result, ERZCombatComboRequestResultType::Success);
	TestTrue(TEXT("Resolved result should expose the authored transition payload."), Result.Payload.Get() == Payload);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboTargetDataCarriesActivationContextTest,
	"RZCombat.Combo.TargetData.CarriesActivationContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboTargetDataCarriesActivationContextTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboPayload* Payload = NewObject<URZCombatComboPayload>();
	FRZCombatComboActivationContext Context;
	Context.RequestResult.Result = ERZCombatComboRequestResultType::Success;
	Context.RequestResult.TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	Context.RequestResult.Payload = Payload;
	Context.Payload = Payload;
	Context.ContextTags.AddTag(MakeTestTag(TEXT("Input.Action.Attack.Heavy")));

	const FGameplayAbilityTargetDataHandle Handle = RZCombatComboTargetData::MakeTargetDataHandle(Context);
	const FRZCombatComboActivationContext* ExtractedContext = RZCombatComboTargetData::FindActivationContext(Handle);

	TestNotNull(TEXT("Combo target data should be discoverable from a target data handle."), ExtractedContext);
	if (!ExtractedContext)
	{
		return false;
	}

	TestEqual(TEXT("Extracted context should keep the trigger tag."), ExtractedContext->RequestResult.TriggerTag, Context.RequestResult.TriggerTag);
	TestTrue(TEXT("Extracted context should keep the payload object."), ExtractedContext->Payload.Get() == Payload);
	TestTrue(TEXT("Extracted context should keep request context tags."), ExtractedContext->ContextTags.HasTagExact(MakeTestTag(TEXT("Input.Action.Attack.Heavy"))));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentCommitResolvedComboTest,
	"RZCombat.Combo.Component.CommitResolvedComboAdvancesState",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentCommitResolvedComboTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& Node = AddNode(Table, TEXT("Light_1"));
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	AddTransition(Table->EntryTransitions, TriggerTag, Node.NodeId);

	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);

	const FRZCombatComboRequestResult Result = Component->RequestCombo(TriggerTag);
	Component->CommitResolvedCombo(Result);

	const FRZCombatComboDebugSnapshot Snapshot = Component->GetComboDebugSnapshot();
	TestTrue(TEXT("Commit should advance current node."), Component->GetCurrentNodeId().Matches(Node.NodeId));
	TestTrue(TEXT("Debug snapshot should expose committed current node."), Snapshot.CurrentNodeId.Matches(Node.NodeId));
	TestEqual(TEXT("Debug snapshot should expose committed ability class."), Snapshot.CurrentAbilityClass.Get(), Node.AbilityClass.Get());
	TestEqual(TEXT("Debug snapshot should keep success as the last result."), Snapshot.LastResult, ERZCombatComboRequestResultType::Success);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentCommitRejectsInvalidResultsTest,
	"RZCombat.Combo.Component.CommitResolvedComboRejectsInvalidResults",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentCommitRejectsInvalidResultsTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& CurrentNode = AddNode(Table, TEXT("Light_1"));
	AddTransition(Table->EntryTransitions, MakeTestTag(TEXT("Input.Action.Attack.Light")), CurrentNode.NodeId);

	URZCombatComboComponent* FreshComponent = MakeComponentWithActiveTable(Table);
	FRZCombatComboRequestResult FabricatedResult = MakeSuccessfulCommitResult(Table, CurrentNode);
	TestFalse(
		TEXT("Commit should reject a fabricated success result that was not produced by the latest resolve."),
		FreshComponent->CommitResolvedCombo(FabricatedResult)
	);
	TestFalse(TEXT("Rejected fabricated commit should not change fresh component state."), FreshComponent->GetCurrentNodeId().IsValid());

	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);
	Component->CommitResolvedCombo(Component->RequestCombo(MakeTestTag(TEXT("Input.Action.Attack.Light"))));
	const FRZCombatComboDebugSnapshot OriginalSnapshot = Component->GetComboDebugSnapshot();

	FRZCombatComboNode& OtherNode = AddNode(Table, TEXT("Light_2"));
	URZCombatComboTableAsset* OtherTable = MakeTable();
	FRZCombatComboNode& OtherTableNode = AddNode(OtherTable, TEXT("Other_Light_1"));

	FRZCombatComboRequestResult NonSuccessResult = MakeSuccessfulCommitResult(Table, OtherNode);
	NonSuccessResult.SourceNodeId = CurrentNode.NodeId;
	NonSuccessResult.Result = ERZCombatComboRequestResultType::NoMatchingTrigger;

	FRZCombatComboRequestResult NoTableResult = MakeSuccessfulCommitResult(Table, OtherNode);
	NoTableResult.SourceNodeId = CurrentNode.NodeId;
	NoTableResult.SelectedTable = nullptr;

	FRZCombatComboRequestResult StaleTableResult = MakeSuccessfulCommitResult(OtherTable, OtherTableNode);
	StaleTableResult.SourceNodeId = CurrentNode.NodeId;

	FRZCombatComboRequestResult MissingTargetResult = MakeSuccessfulCommitResult(Table, OtherNode);
	MissingTargetResult.SourceNodeId = CurrentNode.NodeId;
	MissingTargetResult.TargetNodeId.Name = TEXT("Missing_Target");

	FRZCombatComboRequestResult IllegalTransitionResult = MakeSuccessfulCommitResult(Table, OtherNode);
	IllegalTransitionResult.SourceNodeId = CurrentNode.NodeId;

	TestFalse(TEXT("Commit should reject non-success results."), Component->CommitResolvedCombo(NonSuccessResult));
	TestFalse(TEXT("Commit should reject results without a selected table."), Component->CommitResolvedCombo(NoTableResult));
	TestFalse(TEXT("Commit should reject stale table results."), Component->CommitResolvedCombo(StaleTableResult));
	TestFalse(TEXT("Commit should reject missing target results."), Component->CommitResolvedCombo(MissingTargetResult));
	TestFalse(
		TEXT("Commit should reject fabricated results that do not match a legal resolved transition."),
		Component->CommitResolvedCombo(IllegalTransitionResult)
	);

	const FRZCombatComboDebugSnapshot FinalSnapshot = Component->GetComboDebugSnapshot();
	TestTrue(TEXT("Rejected commits should not change current node."), Component->GetCurrentNodeId().Matches(CurrentNode.NodeId));
	TestTrue(TEXT("Rejected commits should not change snapshot current node."), FinalSnapshot.CurrentNodeId.Matches(OriginalSnapshot.CurrentNodeId));
	TestEqual(TEXT("Rejected commits should not change current ability class."), FinalSnapshot.CurrentAbilityClass.Get(), OriginalSnapshot.CurrentAbilityClass.Get());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentCommittedCurrentNodeTransitionTest,
	"RZCombat.Combo.Component.SecondRequestUsesCommittedCurrentNodeTransitions",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentCommittedCurrentNodeTransitionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	Table->Nodes.Reserve(2);
	FRZCombatComboNode& FirstNode = AddNode(Table, TEXT("Light_1"));
	FRZCombatComboNode& SecondNode = AddNode(Table, TEXT("Light_2"));
	const FGameplayTag EntryTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	const FGameplayTag FollowupTag = MakeTestTag(TEXT("Input.Action.Attack.Heavy"));
	AddTransition(Table->EntryTransitions, EntryTag, FirstNode.NodeId);
	AddTransition(FirstNode.Transitions, FollowupTag, SecondNode.NodeId);

	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);

	const FRZCombatComboRequestResult EntryResult = Component->RequestCombo(EntryTag);
	const FRZCombatComboRequestResult PreCommitFollowupResult = Component->RequestCombo(FollowupTag);
	TestEqual(TEXT("Follow-up trigger should not use first node transitions before commit."), PreCommitFollowupResult.Result, ERZCombatComboRequestResultType::NoMatchingTrigger);

	TestFalse(TEXT("An intervening request invalidates the old entry result."), Component->CommitResolvedCombo(EntryResult));
	const FRZCombatComboRequestResult FreshEntryResult = Component->RequestCombo(EntryTag);
	TestTrue(TEXT("A fresh entry result can be committed."), Component->CommitResolvedCombo(FreshEntryResult));
	const FRZCombatComboRequestResult FollowupResult = Component->RequestCombo(FollowupTag);

	TestEqual(TEXT("Committed current-node transition should resolve."), FollowupResult.Result, ERZCombatComboRequestResultType::Success);
	TestTrue(TEXT("Follow-up result should target the next node."), FollowupResult.TargetNodeId.Matches(SecondNode.NodeId));
	TestTrue(TEXT("Current node should remain on the committed source until follow-up commit."), Component->GetCurrentNodeId().Matches(FirstNode.NodeId));

	Component->CommitResolvedCombo(FollowupResult);
	TestTrue(TEXT("Follow-up commit should advance to the transition target."), Component->GetCurrentNodeId().Matches(SecondNode.NodeId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentNoCurrentNodeTest,
	"RZCombat.Combo.Component.NoCurrentNode",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentNoCurrentNodeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& Node = AddNode(Table, TEXT("Light_1"));
	AddTransition(Table->EntryTransitions, MakeTestTag(TEXT("Input.Action.Attack.Light")), Node.NodeId);

	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);
	Component->CommitResolvedCombo(Component->RequestCombo(MakeTestTag(TEXT("Input.Action.Attack.Light"))));
	Table->Nodes.Empty();

	const FRZCombatComboRequestResult Result = Component->RequestCombo(MakeTestTag(TEXT("Input.Action.Attack.Heavy")));

	TestEqual(TEXT("Missing current node should fail before transition matching."), Result.Result, ERZCombatComboRequestResultType::NoCurrentNode);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentAmbiguousTransitionTest,
	"RZCombat.Combo.Component.AmbiguousTransition",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentAmbiguousTransitionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& FirstNode = AddNode(Table, TEXT("Light_1"));
	FRZCombatComboNode& SecondNode = AddNode(Table, TEXT("Light_2"));
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	AddTransition(Table->EntryTransitions, TriggerTag, FirstNode.NodeId, 10);
	AddTransition(Table->EntryTransitions, TriggerTag, SecondNode.NodeId, 10);

	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);

	const FRZCombatComboRequestResult Result = Component->RequestCombo(TriggerTag);

	TestEqual(TEXT("Highest-priority tie should be ambiguous."), Result.Result, ERZCombatComboRequestResultType::AmbiguousTransition);
	TestFalse(TEXT("Ambiguous request should not advance current node."), Component->GetCurrentNodeId().IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentHighestPriorityTransitionTest,
	"RZCombat.Combo.Component.HighestPriorityTransitionWins",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentHighestPriorityTransitionTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& LowPriorityNode = AddNode(Table, TEXT("Light_1"));
	FRZCombatComboNode& HighPriorityNode = AddNode(Table, TEXT("Heavy_1"));
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	AddTransition(Table->EntryTransitions, TriggerTag, LowPriorityNode.NodeId, 1);
	AddTransition(Table->EntryTransitions, TriggerTag, HighPriorityNode.NodeId, 10);

	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);

	const FRZCombatComboRequestResult Result = Component->RequestCombo(TriggerTag);

	TestEqual(TEXT("Request should resolve."), Result.Result, ERZCombatComboRequestResultType::Success);
	TestTrue(TEXT("Highest-priority target should be selected."), Result.TargetNodeId.Matches(HighPriorityNode.NodeId));
	TestFalse(TEXT("Priority resolution should not advance current node before commit."), Component->GetCurrentNodeId().IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentInvalidTieCandidateDoesNotAmbiguateTest,
	"RZCombat.Combo.Component.InvalidTieCandidateDoesNotAmbiguate",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentInvalidTieCandidateDoesNotAmbiguateTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& ValidNode = AddNode(Table, TEXT("Light_1"));
	FRZCombatComboNodeId MissingNodeId;
	MissingNodeId.Name = TEXT("Missing");
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	AddTransition(Table->EntryTransitions, TriggerTag, MissingNodeId, 10);
	AddTransition(Table->EntryTransitions, TriggerTag, ValidNode.NodeId, 10);

	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);

	const FRZCombatComboRequestResult Result = Component->RequestCombo(TriggerTag);

	TestEqual(TEXT("Only valid same-priority candidate should be selected."), Result.Result, ERZCombatComboRequestResultType::Success);
	TestTrue(TEXT("Valid target should be selected."), Result.TargetNodeId.Matches(ValidNode.NodeId));
	TestFalse(TEXT("Valid resolution should not advance current node before commit."), Component->GetCurrentNodeId().IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentTargetNodeMissingTest,
	"RZCombat.Combo.Component.TargetNodeMissing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentTargetNodeMissingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNodeId MissingNodeId;
	MissingNodeId.Name = TEXT("Missing");
	AddTransition(Table->EntryTransitions, MakeTestTag(TEXT("Input.Action.Attack.Light")), MissingNodeId);

	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);

	const FRZCombatComboRequestResult Result = Component->RequestCombo(MakeTestTag(TEXT("Input.Action.Attack.Light")));

	TestEqual(TEXT("Transition to a missing target should fail."), Result.Result, ERZCombatComboRequestResultType::TargetNodeMissing);
	TestFalse(TEXT("Missing target should not advance current node."), Component->GetCurrentNodeId().IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentTargetAbilityMissingTest,
	"RZCombat.Combo.Component.TargetAbilityMissing",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentTargetAbilityMissingTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& Node = AddNodeWithoutAbility(Table, TEXT("Light_1"));
	AddTransition(Table->EntryTransitions, MakeTestTag(TEXT("Input.Action.Attack.Light")), Node.NodeId);

	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);

	const FRZCombatComboRequestResult Result = Component->RequestCombo(MakeTestTag(TEXT("Input.Action.Attack.Light")));

	TestEqual(TEXT("Target without ability should fail."), Result.Result, ERZCombatComboRequestResultType::TargetAbilityMissing);
	TestFalse(TEXT("Missing ability should not advance current node."), Component->GetCurrentNodeId().IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentCheckerFallsBackToNextValidChildTest,
	"RZCombat.Combo.Component.CheckerFailureFallsBackToNextValidChild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentCheckerFallsBackToNextValidChildTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& BlockedNode = AddNode(Table, TEXT("Blocked_Light"));
	FRZCombatComboNode& FallbackNode = AddNode(Table, TEXT("Fallback_Light"));
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	FRZCombatComboTransition& BlockedTransition = AddTransition(Table->EntryTransitions, TriggerTag, BlockedNode.NodeId, 20);
	AddChecker(BlockedTransition, Table, false);
	AddTransition(Table->EntryTransitions, TriggerTag, FallbackNode.NodeId, 10);

	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);
	const FRZCombatComboRequestResult Result = Component->RequestCombo(TriggerTag);

	TestEqual(TEXT("Request should continue past a failed checker and resolve a lower-priority child."), Result.Result, ERZCombatComboRequestResultType::Success);
	TestTrue(TEXT("Fallback target should be selected after the higher-priority checker fails."), Result.TargetNodeId.Matches(FallbackNode.NodeId));
	TestFalse(TEXT("Resolution should still not advance current node before commit."), Component->GetCurrentNodeId().IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentCheckerFailureWithoutFallbackTest,
	"RZCombat.Combo.Component.CheckerFailureWithoutFallbackReturnsConditionFailed",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentCheckerFailureWithoutFallbackTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& BlockedNode = AddNode(Table, TEXT("Blocked_Light"));
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	FRZCombatComboTransition& BlockedTransition = AddTransition(Table->EntryTransitions, TriggerTag, BlockedNode.NodeId, 20);
	AddChecker(BlockedTransition, Table, false);

	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);
	const FRZCombatComboRequestResult Result = Component->RequestCombo(TriggerTag);

	TestEqual(TEXT("A matching transition blocked only by checker should report a condition failure."), Result.Result, ERZCombatComboRequestResultType::ConditionFailed);
	TestFalse(TEXT("Failed checker should not advance current node."), Component->GetCurrentNodeId().IsValid());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentCheckerReceivesPawnContextTest,
	"RZCombat.Combo.Component.CheckerReceivesPawnContext",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentCheckerReceivesPawnContextTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTransitionCheckerTestDouble::ResetObservedContext();

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& TargetNode = AddNode(Table, TEXT("Light_1"));
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	FRZCombatComboTransition& Transition = AddTransition(Table->EntryTransitions, TriggerTag, TargetNode.NodeId);
	AddChecker(Transition, Table, true);

	APawn* Pawn = MakeTestPawn();
	URZCombatComboComponent* Component = MakeOwnedComponentWithActiveTable(Pawn, Table);
	const FRZCombatComboRequestResult Result = Component->RequestCombo(TriggerTag);

	TestEqual(TEXT("Checker should allow the request."), Result.Result, ERZCombatComboRequestResultType::Success);
	TestTrue(TEXT("Checker should observe a context."), URZCombatComboTransitionCheckerTestDouble::HasObservedContext());
	TestEqual(TEXT("Checker context should expose the combo component."), URZCombatComboTransitionCheckerTestDouble::GetObservedComboComponent(), Component);
	TestEqual(TEXT("Checker context should expose the owner pawn."), URZCombatComboTransitionCheckerTestDouble::GetObservedPawn(), Pawn);
	TestTrue(TEXT("Checker context should expose the request trigger."), URZCombatComboTransitionCheckerTestDouble::GetObservedTriggerTag().MatchesTagExact(TriggerTag));
	TestEqual(TEXT("Entry transition checker should receive an empty source node."), URZCombatComboTransitionCheckerTestDouble::GetObservedSourceNodeName(), NAME_None);
	TestEqual(TEXT("Checker context should expose the target node."), URZCombatComboTransitionCheckerTestDouble::GetObservedTargetNodeName(), TargetNode.NodeId.Name);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentAndCheckerRequiresAllChildrenTest,
	"RZCombat.Combo.Component.AndCheckerRequiresAllChildren",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentAndCheckerRequiresAllChildrenTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& BlockedNode = AddNode(Table, TEXT("Blocked_Light"));
	FRZCombatComboNode& FallbackNode = AddNode(Table, TEXT("Fallback_Light"));
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	FRZCombatComboTransition& BlockedTransition = AddTransition(Table->EntryTransitions, TriggerTag, BlockedNode.NodeId, 20);

	URZCombatComboAndChecker* AndChecker = NewObject<URZCombatComboAndChecker>(Table);
	AndChecker->Checkers.Add(MakeChecker(AndChecker, true));
	AndChecker->Checkers.Add(MakeChecker(AndChecker, false));
	BlockedTransition.Checker = AndChecker;
	AddTransition(Table->EntryTransitions, TriggerTag, FallbackNode.NodeId, 10);

	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);
	const FRZCombatComboRequestResult BlockedResult = Component->RequestCombo(TriggerTag);
	TestEqual(TEXT("AND should fail when any child checker fails."), BlockedResult.Result, ERZCombatComboRequestResultType::Success);
	TestTrue(TEXT("Failed AND checker should allow the next valid child to resolve."), BlockedResult.TargetNodeId.Matches(FallbackNode.NodeId));

	AndChecker->Checkers[1] = MakeChecker(AndChecker, true);
	const FRZCombatComboRequestResult AllowedResult = Component->RequestCombo(TriggerTag);
	TestEqual(TEXT("AND should pass when all child checkers pass."), AllowedResult.Result, ERZCombatComboRequestResultType::Success);
	TestTrue(TEXT("Passing AND checker should keep the higher-priority target."), AllowedResult.TargetNodeId.Matches(BlockedNode.NodeId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentOrCheckerPassesWhenAnyChildPassesTest,
	"RZCombat.Combo.Component.OrCheckerPassesWhenAnyChildPasses",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentOrCheckerPassesWhenAnyChildPassesTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& TargetNode = AddNode(Table, TEXT("Light_1"));
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	FRZCombatComboTransition& Transition = AddTransition(Table->EntryTransitions, TriggerTag, TargetNode.NodeId);

	URZCombatComboOrChecker* OrChecker = NewObject<URZCombatComboOrChecker>(Table);
	OrChecker->Checkers.Add(MakeChecker(OrChecker, false));
	OrChecker->Checkers.Add(MakeChecker(OrChecker, true));
	Transition.Checker = OrChecker;

	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);
	const FRZCombatComboRequestResult Result = Component->RequestCombo(TriggerTag);

	TestEqual(TEXT("OR should pass when any child checker passes."), Result.Result, ERZCombatComboRequestResultType::Success);
	TestTrue(TEXT("Passing OR checker should select the transition target."), Result.TargetNodeId.Matches(TargetNode.NodeId));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboComponentNotCheckerInvertsChildTest,
	"RZCombat.Combo.Component.NotCheckerInvertsChild",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboComponentNotCheckerInvertsChildTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = MakeTable();
	FRZCombatComboNode& BlockedNode = AddNode(Table, TEXT("Blocked_Light"));
	FRZCombatComboNode& FallbackNode = AddNode(Table, TEXT("Fallback_Light"));
	const FGameplayTag TriggerTag = MakeTestTag(TEXT("Input.Action.Attack.Light"));
	FRZCombatComboTransition& BlockedTransition = AddTransition(Table->EntryTransitions, TriggerTag, BlockedNode.NodeId, 20);

	URZCombatComboNotChecker* NotChecker = NewObject<URZCombatComboNotChecker>(Table);
	NotChecker->Checker = MakeChecker(NotChecker, true);
	BlockedTransition.Checker = NotChecker;
	AddTransition(Table->EntryTransitions, TriggerTag, FallbackNode.NodeId, 10);

	URZCombatComboComponent* Component = MakeComponentWithActiveTable(Table);
	const FRZCombatComboRequestResult BlockedResult = Component->RequestCombo(TriggerTag);
	TestEqual(TEXT("NOT should fail when its child checker passes."), BlockedResult.Result, ERZCombatComboRequestResultType::Success);
	TestTrue(TEXT("Failed NOT checker should allow the fallback transition."), BlockedResult.TargetNodeId.Matches(FallbackNode.NodeId));

	NotChecker->Checker = MakeChecker(NotChecker, false);
	const FRZCombatComboRequestResult AllowedResult = Component->RequestCombo(TriggerTag);
	TestEqual(TEXT("NOT should pass when its child checker fails."), AllowedResult.Result, ERZCombatComboRequestResultType::Success);
	TestTrue(TEXT("Passing NOT checker should keep the higher-priority target."), AllowedResult.TargetNodeId.Matches(BlockedNode.NodeId));
	return true;
}

#endif

#if WITH_DEV_AUTOMATION_TESTS

#include "ComboGraph/RZCombatComboDebugger.h"
#include "RZCombatComboComponent.h"
#include "RZCombatComboTableAsset.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "Misc/AutomationTest.h"
#include "Algo/Reverse.h"

namespace RZCombatComboDebuggerTests
{
	struct FWorldFixture
	{
		TArray<UWorld*> Worlds;
		~FWorldFixture()
		{
			for (UWorld* World : Worlds)
			{
				World->DestroyWorld(false);
			}
		}
		UWorld* AddWorld(EWorldType::Type Type = EWorldType::PIE)
		{
			const UWorld::InitializationValues Values = UWorld::InitializationValues()
				.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false)
				.CreateAISystem(false).ShouldSimulatePhysics(false);
			UWorld* World = UWorld::CreateWorld(Type, false, NAME_None, nullptr, true, ERHIFeatureLevel::Num, &Values);
			Worlds.Add(World);
			return World;
		}
	};

	AActor* AddActor(UWorld* World, FName Name)
	{
		FActorSpawnParameters Params;
		Params.Name = Name;
		return World->SpawnActor<AActor>(Params);
	}

	URZCombatComboComponent* AddComponent(AActor* Owner, URZCombatComboTableAsset* Table, FName Name = TEXT("Combo"))
	{
		URZCombatComboComponent* Component = NewObject<URZCombatComboComponent>(Owner, Name);
		Owner->AddInstanceComponent(Component);
		Component->SetActiveComboTable(Table);
		Component->RegisterComponent();
		return Component;
	}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboDebuggerRuntimeWorldFilterTest,
	"RZCombat.Combo.Debugger.FiltersWorldTemplatesTablesAndDestroyedComponents",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboDebuggerRuntimeWorldFilterTest::RunTest(const FString& Parameters)
{
	using namespace RZCombatComboDebuggerTests;
	FWorldFixture Fixture;
	UWorld* PIE = Fixture.AddWorld();
	UWorld* OtherPIE = Fixture.AddWorld();
	UWorld* Editor = Fixture.AddWorld(EWorldType::Editor);
	UWorld* Preview = Fixture.AddWorld(EWorldType::EditorPreview);
	UWorld* Game = Fixture.AddWorld(EWorldType::Game);
	URZCombatComboTableAsset* Table = NewObject<URZCombatComboTableAsset>();
	URZCombatComboComponent* Valid = AddComponent(AddActor(PIE, TEXT("Runtime")), Table);
	URZCombatComboComponent* ExcludedPIE = AddComponent(AddActor(OtherPIE, TEXT("OtherSession")), Table);
	URZCombatComboComponent* EditorComponent = AddComponent(AddActor(Editor, TEXT("EditorOnly")), Table);
	AddComponent(AddActor(Preview, TEXT("PreviewOnly")), Table);
	AddComponent(AddActor(Game, TEXT("NonPIEGame")), Table);
	AddComponent(AddActor(PIE, TEXT("WrongTable")), NewObject<URZCombatComboTableAsset>());
	URZCombatComboComponent* Destroyed = AddComponent(AddActor(PIE, TEXT("Destroyed")), Table);
	Destroyed->DestroyComponent();
	const TArray<UWorld*> ActiveWorlds = { PIE, Editor, Preview, Game };
	const TArray<UObject*> Selection = { EditorComponent, EditorComponent->GetOwner() };
	const TArray<FRZCombatComboDebugCandidate> Candidates = RZCombatComboDebugger::FindCandidates(Table, ActiveWorlds, Selection);
	TestEqual(TEXT("Only registered, live, matching PIE components qualify."), Candidates.Num(), 1);
	TestEqual(TEXT("An unmapped editor selection never falls back to an editor-world object."), RZCombatComboDebugger::ChooseAutomaticTarget(Candidates), Valid);
	TestFalse(TEXT("PIE worlds outside the active session are excluded."), RZCombatComboDebugger::IsRuntimeComponent(ExcludedPIE, ActiveWorlds));
	TestFalse(TEXT("Class templates are excluded."), RZCombatComboDebugger::IsRuntimeComponent(GetDefault<URZCombatComboComponent>(), ActiveWorlds));
	PIE->bIsTearingDown = true;
	TestFalse(TEXT("Tearing-down worlds are excluded."), RZCombatComboDebugger::IsRuntimeComponent(Valid, ActiveWorlds));
	PIE->bIsTearingDown = false;
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboDebuggerPriorityTest,
	"RZCombat.Combo.Debugger.SelectedThenLocalPlayerThenStableFirst",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboDebuggerPriorityTest::RunTest(const FString& Parameters)
{
	using namespace RZCombatComboDebuggerTests;
	FWorldFixture Fixture;
	UWorld* PIE = Fixture.AddWorld();
	URZCombatComboTableAsset* Table = NewObject<URZCombatComboTableAsset>();
	URZCombatComboComponent* First = AddComponent(AddActor(PIE, TEXT("A_First")), Table);
	URZCombatComboComponent* Second = AddComponent(First->GetOwner(), Table, TEXT("SecondCombo"));
	APawn* LocalPawn = PIE->SpawnActor<APawn>();
	APlayerController* Controller = PIE->SpawnActor<APlayerController>();
	// Minimal local-player topology without initializing viewport/input or an online session.
	Controller->Player = NewObject<ULocalPlayer>(GEngine);
	Controller->SetAsLocalPlayerController();
	Controller->Possess(LocalPawn);
	URZCombatComboComponent* Local = AddComponent(LocalPawn, Table);
	const TArray<UWorld*> ActiveWorlds = { PIE };
	auto Candidates = RZCombatComboDebugger::FindCandidates(Table, ActiveWorlds, {});
	TestEqual(TEXT("Local player beats the stable first non-player."), RZCombatComboDebugger::ChooseAutomaticTarget(Candidates), Local);
	const TArray<UObject*> ActorSelection = { First->GetOwner() };
	Candidates = RZCombatComboDebugger::FindCandidates(Table, ActiveWorlds, ActorSelection);
	TestEqual(TEXT("Matching selected actor beats local player."), RZCombatComboDebugger::ChooseAutomaticTarget(Candidates), First);
	const TArray<UObject*> ComponentSelection = { First->GetOwner(), Second };
	Candidates = RZCombatComboDebugger::FindCandidates(Table, ActiveWorlds, ComponentSelection);
	TestEqual(TEXT("Exact component selection disambiguates multiple components."), RZCombatComboDebugger::ChooseAutomaticTarget(Candidates), Second);
	Controller->UnPossess();
	Candidates = RZCombatComboDebugger::FindCandidates(Table, ActiveWorlds, {});
	TestEqual(TEXT("Without a selected or local player, choose the stable first."), RZCombatComboDebugger::ChooseAutomaticTarget(Candidates), First);
	Algo::Reverse(Candidates);
	TestEqual(TEXT("Enumeration order cannot change the automatic choice."), RZCombatComboDebugger::ChooseAutomaticTarget(Candidates), First);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboDebuggerVerifiedCounterpartTest,
	"RZCombat.Combo.Debugger.EditorSelectionRequiresVerifiedPIECounterpart",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboDebuggerVerifiedCounterpartTest::RunTest(const FString& Parameters)
{
	using namespace RZCombatComboDebuggerTests;
	if (!GEditor)
	{
		AddError(TEXT("This editor-context test requires GEditor."));
		return false;
	}
	FWorldFixture Fixture;
	UWorld* PIE = Fixture.AddWorld();
	UWorld* Editor = Fixture.AddWorld(EWorldType::Editor);
	URZCombatComboTableAsset* Table = NewObject<URZCombatComboTableAsset>();
	URZCombatComboComponent* First = AddComponent(AddActor(PIE, TEXT("A_First")), Table);
	URZCombatComboComponent* Selected = AddComponent(AddActor(PIE, TEXT("Z_Selected")), Table);
	URZCombatComboComponent* EditorComponent = AddComponent(AddActor(Editor, TEXT("Z_Selected")), Table);
	const TArray<UWorld*> ActiveWorlds = { PIE };
	const TArray<UObject*> Selection = { EditorComponent };
	// Editor-Cmd does not necessarily populate EditorWorld. Supply and restore the fixture world explicitly.
	TGuardValue<TObjectPtr<UWorld>> EditorWorldGuard(GEditor->EditorWorld, Editor);
	TGuardValue<TObjectPtr<UWorld>> PlayWorldGuard(GEditor->PlayWorld, PIE);
	TestEqual(TEXT("Matching names alone are not sufficient to map a runtime target."),
		RZCombatComboDebugger::ChooseAutomaticTarget(RZCombatComboDebugger::FindCandidates(Table, ActiveWorlds, Selection)), First);
	GEditor->ObjectsThatExistInEditorWorld.Set(Selected->GetOwner());
	TestEqual(TEXT("The engine-verified actor/component counterpart participates in selection priority."),
		RZCombatComboDebugger::ChooseAutomaticTarget(RZCombatComboDebugger::FindCandidates(Table, ActiveWorlds, Selection)), Selected);
	GEditor->ObjectsThatExistInEditorWorld.Clear(Selected->GetOwner());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboDebuggerLockedTargetTest,
	"RZCombat.Combo.Debugger.LockedTargetManualLossMismatchAndCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboDebuggerLockedTargetTest::RunTest(const FString& Parameters)
{
	using namespace RZCombatComboDebuggerTests;
	FWorldFixture Fixture;
	UWorld* PIE = Fixture.AddWorld();
	URZCombatComboTableAsset* Table = NewObject<URZCombatComboTableAsset>();
	URZCombatComboComponent* First = AddComponent(AddActor(PIE, TEXT("A_First")), Table);
	URZCombatComboComponent* Second = AddComponent(AddActor(PIE, TEXT("B_Second")), Table);
	const TArray<UWorld*> ActiveWorlds = { PIE };
	const TArray<UObject*> Selection = { Second };
	FRZCombatComboDebuggerSession Session;
	Session.Update(Table, ActiveWorlds, {});
	TestEqual(TEXT("Auto selects the first instance."), Session.GetTarget(), First);
	Session.Update(Table, ActiveWorlds, Selection);
	TestEqual(TEXT("External selection never steals the locked target."), Session.GetTarget(), First);
	Session.ReturnToAuto();
	Session.Update(Table, ActiveWorlds, Selection);
	TestEqual(TEXT("Returning to Auto explicitly re-evaluates selection priority."), Session.GetTarget(), Second);
	TestTrue(TEXT("Debug Object can manually switch instances."), Session.SelectTarget(First, Table, ActiveWorlds));
	TestTrue(TEXT("Manual mode is explicit."), Session.IsManual());
	First->SetActiveComboTable(NewObject<URZCombatComboTableAsset>());
	Session.Update(Table, ActiveWorlds, Selection);
	TestEqual(TEXT("A changed table produces a visible mismatch."), Session.GetState(), ERZCombatComboDebuggerState::TableMismatch);
	TestEqual(TEXT("Mismatch never silently changes targets."), Session.GetTarget(), First);
	TestEqual(TEXT("Table switch remains visible in event history."), Session.GetRecentEvents().Last(1).Type, ERZCombatComboDebugEventType::TableChanged);
	First->SetActiveComboTable(Table);
	Session.Update(Table, ActiveWorlds, {});
	TestEqual(TEXT("The same target can return to the edited table."), Session.GetState(), ERZCombatComboDebuggerState::Live);
	PIE->bDebugPauseExecution = true;
	Session.Update(Table, ActiveWorlds, {});
	TestEqual(TEXT("Native PIE pause is reflected without detaching."), Session.GetState(), ERZCombatComboDebuggerState::Paused);
	PIE->bDebugPauseExecution = false;
	Session.Update(Table, ActiveWorlds, {});
	TestEqual(TEXT("Resume continues the locked session."), Session.GetState(), ERZCombatComboDebuggerState::Live);
	const FString LastIdentity = Session.GetTargetLabel();
	First->DestroyComponent();
	Session.Update(Table, ActiveWorlds, Selection);
	TestEqual(TEXT("Manual target loss is visible."), Session.GetState(), ERZCombatComboDebuggerState::TargetLost);
	TestNull(TEXT("Lost targets are not replaced by another instance."), Session.GetTarget());
	TestEqual(TEXT("Lost target identity is retained for diagnosis."), Session.GetTargetLabel(), LastIdentity);
	Session.EndSession();
	TestFalse(TEXT("EndPIE clears target choice."), Session.HasChosenTarget());
	TestFalse(TEXT("EndPIE clears manual mode."), Session.IsManual());
	TestTrue(TEXT("EndPIE clears history."), Session.GetRecentEvents().IsEmpty());
	TestTrue(TEXT("EndPIE clears identity."), Session.GetTargetLabel().IsEmpty());
	Session.Update(Table, {}, {});
	TestEqual(TEXT("No active play worlds leaves an inactive session."), Session.GetState(), ERZCombatComboDebuggerState::Inactive);
	Session.Update(Table, ActiveWorlds, {});
	TestEqual(TEXT("Next session selects a fresh valid instance."), Session.GetTarget(), Second);
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboDebuggerLateSpawnAndObservationTest,
	"RZCombat.Combo.Debugger.LateSpawnOpenDuringPlayMultiWorldAndReadOnlyObservation",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboDebuggerLateSpawnAndObservationTest::RunTest(const FString& Parameters)
{
	using namespace RZCombatComboDebuggerTests;
	FWorldFixture Fixture;
	UWorld* FirstWorld = Fixture.AddWorld();
	UWorld* SecondWorld = Fixture.AddWorld();
	const TArray<UWorld*> ActiveWorlds = { FirstWorld, SecondWorld };
	URZCombatComboTableAsset* Table = NewObject<URZCombatComboTableAsset>();
	FRZCombatComboDebuggerSession Session;
	Session.Update(Table, ActiveWorlds, {});
	TestEqual(TEXT("PIE without a matching instance waits."), Session.GetState(), ERZCombatComboDebuggerState::Waiting);
	URZCombatComboComponent* Late = AddComponent(AddActor(SecondWorld, TEXT("SpawnedLater")), Table);
	Late->ResetCombo();
	const int32 BeforeObserve = Late->GetRecentComboDebugEvents().Num();
	Session.Update(Table, ActiveWorlds, {});
	TestEqual(TEXT("A late-generated instance is automatically discovered."), Session.GetTarget(), Late);
	TestEqual(TEXT("Opening a graph during PIE includes pre-attachment events."), Session.GetRecentEvents().Num(), BeforeObserve);
	URZCombatComboComponent* Other = AddComponent(AddActor(FirstWorld, TEXT("SpawnedLater")), Table);
	const auto Candidates = RZCombatComboDebugger::FindCandidates(Table, ActiveWorlds, {});
	TestEqual(TEXT("All active PIE worlds can offer matching components."), Candidates.Num(), 2);
	TestNotEqual(TEXT("Same actor names across worlds have distinct displayed identities."),
		RZCombatComboDebugger::DescribeComponent(Late), RZCombatComboDebugger::DescribeComponent(Other));
	Session.Update(Table, ActiveWorlds, {}, false);
	TestEqual(TEXT("Debug toggle disables observation, not gameplay."), Session.GetState(), ERZCombatComboDebuggerState::Disabled);
	Session.Update(Table, ActiveWorlds, {});
	TestEqual(TEXT("Re-enabling preserves the locked instance."), Session.GetTarget(), Late);
	TestEqual(TEXT("Reading the session does not append runtime events."), Late->GetRecentComboDebugEvents().Num(), BeforeObserve);
	TestFalse(TEXT("Debugger never enables GAS observation."), Late->bObserveAbilitySystemLifecycle);
	TestFalse(TEXT("Debugger never binds the ASC."), Late->IsBoundToAbilitySystemLifecycle());
	TestTrue(TEXT("Debugger does not change commit policy."), Late->bCommitComboOnAbilityActivated);
	TestFalse(TEXT("Debugger does not change end-reset policy."), Late->bResetComboOnAbilityEnded);
	Session.Update(Table, {}, {});
	TestNull(TEXT("World cleanup clears weak target reference."), Session.GetTarget());
	return true;
}

#endif

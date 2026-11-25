#include "ComboGraph/RZCombatComboDebugger.h"

#include "RZCombatComboComponent.h"
#include "RZCombatComboTableAsset.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"
#include "Selection.h"
#include "UObject/UObjectIterator.h"

#define LOCTEXT_NAMESPACE "RZCombatComboDebugger"

namespace
{
	AActor* MapSelectedActor(AActor* Actor, TConstArrayView<UWorld*> PlayWorlds)
	{
		if (!IsValid(Actor) || Actor->IsTemplate())
		{
			return nullptr;
		}
		if (PlayWorlds.Contains(Actor->GetWorld()))
		{
			return Actor;
		}
		// Only the engine's verified counterpart mapping may promote an editor actor to a runtime target.
		if (Actor->GetWorld() && Actor->GetWorld()->WorldType == EWorldType::Editor)
		{
			AActor* Counterpart = EditorUtilities::GetSimWorldCounterpartActor(Actor);
			return IsValid(Counterpart) && PlayWorlds.Contains(Counterpart->GetWorld()) ? Counterpart : nullptr;
		}
		return nullptr;
	}

	bool IsLocalPlayerOwner(const AActor* Actor)
	{
		const APlayerController* PlayerController = Cast<APlayerController>(Actor);
		if (const APawn* Pawn = Cast<APawn>(Actor))
		{
			PlayerController = Cast<APlayerController>(Pawn->GetController());
		}
		else if (const APlayerState* PlayerState = Cast<APlayerState>(Actor))
		{
			PlayerController = Cast<APlayerController>(PlayerState->GetOwner());
		}
		return PlayerController && PlayerController->IsLocalPlayerController() && PlayerController->GetLocalPlayer();
	}

	const TCHAR* NetModeName(ENetMode NetMode)
	{
		switch (NetMode)
		{
		case NM_Client: return TEXT("Client");
		case NM_ListenServer: return TEXT("Listen Server");
		case NM_DedicatedServer: return TEXT("Server");
		default: return TEXT("Standalone");
		}
	}
}

TArray<UWorld*> RZCombatComboDebugger::GetPlayWorlds()
{
	TArray<UWorld*> Worlds;
	if (GEngine)
	{
		for (const FWorldContext& Context : GEngine->GetWorldContexts())
		{
			UWorld* World = Context.World();
			if (Context.WorldType == EWorldType::PIE && IsValid(World) && !World->bIsTearingDown)
			{
				Worlds.AddUnique(World);
			}
		}
	}
	return Worlds;
}

TArray<UObject*> RZCombatComboDebugger::GetEditorSelection()
{
	TArray<UObject*> Selection;
	if (GEditor)
	{
		if (USelection* Components = GEditor->GetSelectedComponents())
		{
			Components->GetSelectedObjects(Selection);
		}
		for (FSelectionIterator It(GEditor->GetSelectedActorIterator()); It; ++It)
		{
			Selection.AddUnique(*It);
		}
	}
	return Selection;
}

bool RZCombatComboDebugger::IsRuntimeComponent(const URZCombatComboComponent* Component, TConstArrayView<UWorld*> PlayWorlds)
{
	if (!IsValid(Component) || Component->IsTemplate() || !Component->IsRegistered())
	{
		return false;
	}
	const AActor* Owner = Component->GetOwner();
	const UWorld* World = Component->GetWorld();
	return IsValid(Owner) && !Owner->IsTemplate() && !Owner->IsActorBeingDestroyed()
		&& IsValid(World) && World->WorldType == EWorldType::PIE && !World->bIsTearingDown
		&& PlayWorlds.Contains(World);
}

FString RZCombatComboDebugger::DescribeComponent(const URZCombatComboComponent* Component)
{
	if (!IsValid(Component) || !Component->GetOwner() || !Component->GetWorld())
	{
		return TEXT("None");
	}
	const AActor* Owner = Component->GetOwner();
	const UWorld* World = Component->GetWorld();
	return FString::Printf(TEXT("%s / %s [PIE %d | %s | %s | Actor: %s]"),
		*Owner->GetActorLabel(), *Component->GetName(), World->GetOutermost()->GetPIEInstanceID(),
		NetModeName(World->GetNetMode()), *World->GetPathName(), *Owner->GetName());
}

TArray<FRZCombatComboDebugCandidate> RZCombatComboDebugger::FindCandidates(
	const URZCombatComboTableAsset* Table, TConstArrayView<UWorld*> PlayWorlds, TConstArrayView<UObject*> Selection)
{
	TArray<FRZCombatComboDebugCandidate> Candidates;
	if (!IsValid(Table) || PlayWorlds.IsEmpty())
	{
		return Candidates;
	}

	TSet<AActor*> SelectedActors;
	TSet<URZCombatComboComponent*> SelectedComponents;
	for (UObject* SelectedObject : Selection)
	{
		if (UActorComponent* Component = Cast<UActorComponent>(SelectedObject))
		{
			AActor* RuntimeActor = MapSelectedActor(Component->GetOwner(), PlayWorlds);
			if (!RuntimeActor)
			{
				continue;
			}
			if (URZCombatComboComponent* ComboComponent = Cast<URZCombatComboComponent>(Component))
			{
				UActorComponent* RuntimeComponent = RuntimeActor == Component->GetOwner() ? Component
					: EditorUtilities::FindMatchingComponentInstance(Component, RuntimeActor);
				if (URZCombatComboComponent* RuntimeCombo = Cast<URZCombatComboComponent>(RuntimeComponent))
				{
					SelectedComponents.Add(RuntimeCombo);
				}
			}
			else
			{
				SelectedActors.Add(RuntimeActor);
			}
		}
		else if (AActor* RuntimeActor = MapSelectedActor(Cast<AActor>(SelectedObject), PlayWorlds))
		{
			SelectedActors.Add(RuntimeActor);
		}
	}

	for (TObjectIterator<URZCombatComboComponent> It; It; ++It)
	{
		URZCombatComboComponent* Component = *It;
		if (!IsRuntimeComponent(Component, PlayWorlds) || Component->GetActiveComboTable() != Table)
		{
			continue;
		}
		FRZCombatComboDebugCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Component = Component;
		Candidate.Label = DescribeComponent(Component);
		Candidate.SortKey = FString::Printf(TEXT("%010d|%s"),
			Component->GetWorld()->GetOutermost()->GetPIEInstanceID() + 1, *Component->GetPathName());
		Candidate.SelectionRank = SelectedComponents.Contains(Component) ? 0
			: SelectedActors.Contains(Component->GetOwner()) ? 1 : MAX_int32;
		Candidate.bLocalPlayer = IsLocalPlayerOwner(Component->GetOwner());
	}
	Candidates.Sort([](const FRZCombatComboDebugCandidate& A, const FRZCombatComboDebugCandidate& B)
	{
		return A.SortKey < B.SortKey;
	});
	return Candidates;
}

URZCombatComboComponent* RZCombatComboDebugger::ChooseAutomaticTarget(TArray<FRZCombatComboDebugCandidate> Candidates)
{
	Candidates.Sort([](const FRZCombatComboDebugCandidate& A, const FRZCombatComboDebugCandidate& B)
	{
		if (A.SelectionRank != B.SelectionRank)
		{
			return A.SelectionRank < B.SelectionRank;
		}
		if (A.bLocalPlayer != B.bLocalPlayer)
		{
			return A.bLocalPlayer;
		}
		return A.SortKey < B.SortKey;
	});
	for (const FRZCombatComboDebugCandidate& Candidate : Candidates)
	{
		if (Candidate.Component.IsValid())
		{
			return Candidate.Component.Get();
		}
	}
	return nullptr;
}

void FRZCombatComboDebuggerSession::Update(const URZCombatComboTableAsset* Table, TConstArrayView<UWorld*> PlayWorlds,
	TConstArrayView<UObject*> Selection, bool bEnabled)
{
	if (PlayWorlds.IsEmpty())
	{
		EndSession();
		State = bEnabled ? ERZCombatComboDebuggerState::Inactive : ERZCombatComboDebuggerState::Disabled;
		return;
	}
	if (!bEnabled)
	{
		State = ERZCombatComboDebuggerState::Disabled;
		return;
	}
	if (!bTargetChosen)
	{
		if (URZCombatComboComponent* Component = RZCombatComboDebugger::ChooseAutomaticTarget(
			RZCombatComboDebugger::FindCandidates(Table, PlayWorlds, Selection)))
		{
			SetTarget(Component, false);
		}
	}
	if (!bTargetChosen)
	{
		State = ERZCombatComboDebuggerState::Waiting;
		return;
	}
	if (!RZCombatComboDebugger::IsRuntimeComponent(Target.Get(), PlayWorlds))
	{
		// Both auto and manual choices stay locked. Losing a target must never silently switch actors.
		Target.Reset();
		State = ERZCombatComboDebuggerState::TargetLost;
		return;
	}
	TargetLabel = RZCombatComboDebugger::DescribeComponent(Target.Get());
	RecentEvents = Target->GetRecentComboDebugEvents();
	if (Target->GetActiveComboTable() != Table)
	{
		State = ERZCombatComboDebuggerState::TableMismatch;
	}
	else
	{
		const UWorld* World = Target->GetWorld();
		State = World->bDebugPauseExecution || World->IsPaused()
			? ERZCombatComboDebuggerState::Paused : ERZCombatComboDebuggerState::Live;
	}
}

bool FRZCombatComboDebuggerSession::SelectTarget(URZCombatComboComponent* Component, const URZCombatComboTableAsset* Table,
	TConstArrayView<UWorld*> PlayWorlds)
{
	if (!RZCombatComboDebugger::IsRuntimeComponent(Component, PlayWorlds) || Component->GetActiveComboTable() != Table)
	{
		return false;
	}
	SetTarget(Component, true);
	Update(Table, PlayWorlds, {});
	return true;
}

void FRZCombatComboDebuggerSession::SetTarget(URZCombatComboComponent* Component, bool bInManual)
{
	Target = Component;
	bTargetChosen = true;
	bManual = bInManual;
	TargetLabel = RZCombatComboDebugger::DescribeComponent(Component);
	RecentEvents = Component->GetRecentComboDebugEvents();
}

void FRZCombatComboDebuggerSession::ReturnToAuto()
{
	EndSession();
	State = ERZCombatComboDebuggerState::Waiting;
}

void FRZCombatComboDebuggerSession::EndSession()
{
	Target.Reset();
	TargetLabel.Reset();
	RecentEvents.Reset();
	bTargetChosen = false;
	bManual = false;
	State = ERZCombatComboDebuggerState::Inactive;
}

FText FRZCombatComboDebuggerSession::GetStateText() const
{
	switch (State)
	{
	case ERZCombatComboDebuggerState::Waiting:
		return LOCTEXT("Waiting", "Waiting for an instance using this Combo Table (PIE / Simulate).");
	case ERZCombatComboDebuggerState::Live:
		return LOCTEXT("Live", "Live — observing the locked runtime instance.");
	case ERZCombatComboDebuggerState::Paused:
		return LOCTEXT("Paused", "Paused — runtime state retained. Use Resume to continue.");
	case ERZCombatComboDebuggerState::TargetLost:
		return LOCTEXT("Lost", "Target lost — select another Debug Object or return to Auto. No automatic replacement.");
	case ERZCombatComboDebuggerState::TableMismatch:
		return LOCTEXT("Mismatch", "Table mismatch — the locked target changed its active table. Select another Debug Object or return to Auto.");
	case ERZCombatComboDebuggerState::Disabled:
		return LOCTEXT("Disabled", "PIE Debug disabled — gameplay policies are unchanged.");
	default:
		return LOCTEXT("Inactive", "Not running — use Play or Simulate to debug the current level.");
	}
}

#undef LOCTEXT_NAMESPACE

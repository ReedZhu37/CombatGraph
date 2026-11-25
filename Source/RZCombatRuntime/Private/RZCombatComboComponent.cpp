#include "RZCombatComboComponent.h"

#include "AbilitySystemComponent.h"
#include "AbilitySystemGlobals.h"
#include "Abilities/GameplayAbility.h"
#include "Abilities/GameplayAbilityTypes.h"
#include "RZCombatComboTableAsset.h"
#include "RZCombatComboTransitionChecker.h"

#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Engine/World.h"

namespace
{
	void AddComboAbilitySystemActorCandidate(TArray<AActor*, TInlineAllocator<8>>& Actors, AActor* Actor)
	{
		if (IsValid(Actor))
		{
			Actors.AddUnique(Actor);
		}
	}

	UAbilitySystemComponent* FindComboAbilitySystemComponentOnActor(AActor* Actor)
	{
		if (!IsValid(Actor))
		{
			return nullptr;
		}

		if (UAbilitySystemComponent* AbilitySystemComponent = UAbilitySystemGlobals::GetAbilitySystemComponentFromActor(Actor))
		{
			return AbilitySystemComponent;
		}

		return Actor->FindComponentByClass<UAbilitySystemComponent>();
	}

	APawn* ResolveComboContextPawn(AActor* OwnerActor)
	{
		if (!OwnerActor)
		{
			return nullptr;
		}

		if (APawn* OwnerPawn = Cast<APawn>(OwnerActor))
		{
			return OwnerPawn;
		}

		if (AController* OwnerController = Cast<AController>(OwnerActor))
		{
			return OwnerController->GetPawn();
		}

		return OwnerActor->GetInstigator();
	}
}

URZCombatComboComponent::URZCombatComboComponent()
{
	PrimaryComponentTick.bCanEverTick = false;
}

void URZCombatComboComponent::BeginPlay()
{
	Super::BeginPlay();

	if (bObserveAbilitySystemLifecycle && bAutoBindAbilitySystemOnBeginPlay)
	{
		BindToAbilitySystemLifecycle();
	}
}

void URZCombatComboComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
	UnbindFromAbilitySystemLifecycle();
	Super::EndPlay(EndPlayReason);
}

void URZCombatComboComponent::AddComboTable(URZCombatComboTableAsset* Table)
{
	if (!IsValid(Table))
	{
		return;
	}

	ComboTables.AddUnique(Table);

	if (!ActiveComboTable)
	{
		SetActiveComboTable(Table);
	}
}

void URZCombatComboComponent::RemoveComboTable(URZCombatComboTableAsset* Table)
{
	if (!IsValid(Table))
	{
		return;
	}

	ComboTables.Remove(Table);

	if (ActiveComboTable == Table)
	{
		SetActiveComboTable(nullptr);
	}
}

void URZCombatComboComponent::SetActiveComboTable(URZCombatComboTableAsset* Table)
{
	if (ActiveComboTable == Table)
	{
		return;
	}

	if (IsValid(Table))
	{
		ComboTables.AddUnique(Table);
	}

	const FString PreviousTablePath = GetPathNameSafe(ActiveComboTable);
	ActiveComboTable = Table;
	ResetComboState(true);
	RecordDebugEvent(ERZCombatComboDebugEventType::TableChanged, PreviousTablePath);
	RecordDebugEvent(ERZCombatComboDebugEventType::Reset);
	OnComboStateChanged.Broadcast(GetComboDebugSnapshot());
}

URZCombatComboTableAsset* URZCombatComboComponent::GetActiveComboTable() const
{
	return ActiveComboTable;
}

void URZCombatComboComponent::ResetCombo()
{
	ResetComboState(true);
	RecordDebugEvent(ERZCombatComboDebugEventType::Reset);
	OnComboStateChanged.Broadcast(GetComboDebugSnapshot());
}

void URZCombatComboComponent::ResetComboState(bool bResetExecutionDebug)
{
	CurrentNodeId = FRZCombatComboNodeId();
	CurrentAbilityClass = nullptr;
	PendingResolvedResult = FRZCombatComboRequestResult();
	bPendingComboCommitted = false;
	ClearPendingAbilitySystemObservation();

	if (bResetExecutionDebug)
	{
		ClearObservedExecutionDebug();
	}
}

void URZCombatComboComponent::ClearObservedExecutionDebug()
{
	ExecutionState = ERZCombatComboExecutionState::None;
	ObservedAbilityClass = nullptr;
	ObservedAbilitySpecHandle = FGameplayAbilitySpecHandle();
	ObservedFailureTags.Reset();
	bObservedAbilityWasCancelled = false;
}

FRZCombatComboRequestResult URZCombatComboComponent::RequestCombo(FGameplayTag TriggerTag)
{
	FRZCombatComboRequest Request;
	Request.TriggerTag = TriggerTag;

	return RequestComboWithContext(Request);
}

FRZCombatComboRequestResult URZCombatComboComponent::RequestComboWithContext(const FRZCombatComboRequest& Request)
{
	// BeginPlay can precede PlayerState/ASC initialization. Retry at the gameplay request boundary,
	// never from the debugger or a permanent Tick. Explicit Unbind and the auto-bind opt-out are respected.
	if (bObserveAbilitySystemLifecycle && bAutoBindAbilitySystemOnBeginPlay
		&& !bAutomaticAbilitySystemBindingSuppressed && !IsBoundToAbilitySystemLifecycle())
	{
		BindToAbilitySystemLifecycle();
	}

	FRZCombatComboRequestResult Result;
	Result.SelectedTable = ActiveComboTable;
	Result.SourceNodeId = CurrentNodeId;
	Result.TriggerTag = Request.TriggerTag;

	const auto ResolveRequest = [this](const FRZCombatComboRequestResult& ResolvedResult)
	{
		LastRequestResult = ResolvedResult;
		bPendingComboCommitted = false;
		if (ResolvedResult.Result == ERZCombatComboRequestResultType::Success && ResolvedResult.bResolved)
		{
			PendingResolvedResult = ResolvedResult;
			CachePendingAbilitySystemSpecForResult(ResolvedResult);
			RecordObservedExecution(
				ERZCombatComboExecutionState::ResolvedPendingActivation,
				FGameplayAbilitySpecHandle(),
				ResolvedResult.AbilityClass,
				nullptr,
				false
			);
		}
		else
		{
			PendingResolvedResult = FRZCombatComboRequestResult();
			ClearPendingAbilitySystemObservation();
			ClearObservedExecutionDebug();
		}

		RecordDebugEvent(ERZCombatComboDebugEventType::Resolve);
		OnComboRequestResolved.Broadcast(LastRequestResult);
		return LastRequestResult;
	};

	if (!IsValid(ActiveComboTable))
	{
		Result.Result = ERZCombatComboRequestResultType::NoActiveTable;
		return ResolveRequest(Result);
	}

	const FRZCombatComboNode* SourceNode = nullptr;
	const TArray<FRZCombatComboTransition>* Transitions = nullptr;

	if (CurrentNodeId.IsValid())
	{
		SourceNode = ActiveComboTable->FindNode(CurrentNodeId);
		if (!SourceNode)
		{
			Result.Result = ERZCombatComboRequestResultType::NoCurrentNode;
			return ResolveRequest(Result);
		}

		Transitions = &SourceNode->Transitions;
	}
	else
	{
		if (ActiveComboTable->EntryTransitions.IsEmpty())
		{
			Result.Result = ERZCombatComboRequestResultType::NoEntryTransition;
			return ResolveRequest(Result);
		}

		Transitions = &ActiveComboTable->EntryTransitions;
	}

	FRZCombatComboNode EmptySourceNode;
	const FRZCombatComboNode& SourceNodeForCondition = SourceNode ? *SourceNode : EmptySourceNode;

	struct FResolvedTransitionCandidate
	{
		const FRZCombatComboTransition* Transition = nullptr;
		const FRZCombatComboNode* TargetNode = nullptr;
	};

	TArray<FResolvedTransitionCandidate> Candidates;
	bool bSawMatchingTrigger = false;
	bool bSawTargetNodeMissing = false;
	bool bSawTargetAbilityMissing = false;
	bool bSawConditionFailed = false;
	FRZCombatComboNodeId FirstMissingTargetNodeId;
	FRZCombatComboNodeId FirstMissingAbilityNodeId;

	for (const FRZCombatComboTransition& CandidateTransition : *Transitions)
	{
		if (!CandidateTransition.bEnabled || !CandidateTransition.TriggerTag.MatchesTagExact(Request.TriggerTag))
		{
			continue;
		}

		bSawMatchingTrigger = true;

		const FRZCombatComboNode* CandidateTargetNode = ActiveComboTable->FindNode(CandidateTransition.TargetNodeId);
		if (!CandidateTargetNode)
		{
			if (!bSawTargetNodeMissing)
			{
				FirstMissingTargetNodeId = CandidateTransition.TargetNodeId;
			}

			bSawTargetNodeMissing = true;
			continue;
		}

		if (!CandidateTargetNode->AbilityClass)
		{
			if (!bSawTargetAbilityMissing)
			{
				FirstMissingAbilityNodeId = CandidateTargetNode->NodeId;
			}

			bSawTargetAbilityMissing = true;
			continue;
		}

		if (!CanUseComboTransition(Request, SourceNodeForCondition, CandidateTransition, *CandidateTargetNode))
		{
			bSawConditionFailed = true;
			continue;
		}

		if (CandidateTransition.Checker)
		{
			FRZCombatComboTransitionCheckContext CheckContext;
			CheckContext.ComboComponent = this;
			CheckContext.OwnerActor = GetOwner();
			CheckContext.Pawn = ResolveComboContextPawn(CheckContext.OwnerActor);
			CheckContext.ActiveTable = ActiveComboTable;
			CheckContext.Request = Request;
			CheckContext.SourceNode = SourceNodeForCondition;
			CheckContext.Transition = CandidateTransition;
			CheckContext.TargetNode = *CandidateTargetNode;

			if (!CandidateTransition.Checker->CanUseTransition(CheckContext))
			{
				bSawConditionFailed = true;
				continue;
			}
		}

		FResolvedTransitionCandidate& Candidate = Candidates.AddDefaulted_GetRef();
		Candidate.Transition = &CandidateTransition;
		Candidate.TargetNode = CandidateTargetNode;
	}

	if (Candidates.IsEmpty())
	{
		if (!bSawMatchingTrigger)
		{
			Result.Result = ERZCombatComboRequestResultType::NoMatchingTrigger;
			return ResolveRequest(Result);
		}

		if (bSawTargetNodeMissing)
		{
			Result.TargetNodeId = FirstMissingTargetNodeId;
			Result.Result = ERZCombatComboRequestResultType::TargetNodeMissing;
			return ResolveRequest(Result);
		}

		if (bSawTargetAbilityMissing)
		{
			Result.TargetNodeId = FirstMissingAbilityNodeId;
			Result.Result = ERZCombatComboRequestResultType::TargetAbilityMissing;
			return ResolveRequest(Result);
		}

		if (bSawConditionFailed)
		{
			Result.Result = ERZCombatComboRequestResultType::ConditionFailed;
			return ResolveRequest(Result);
		}

		Result.Result = ERZCombatComboRequestResultType::NoMatchingTrigger;
		return ResolveRequest(Result);
	}

	const FResolvedTransitionCandidate* BestCandidate = nullptr;
	bool bAmbiguous = false;
	for (const FResolvedTransitionCandidate& Candidate : Candidates)
	{
		if (!BestCandidate || Candidate.Transition->Priority > BestCandidate->Transition->Priority)
		{
			BestCandidate = &Candidate;
			bAmbiguous = false;
			continue;
		}

		if (Candidate.Transition->Priority == BestCandidate->Transition->Priority)
		{
			bAmbiguous = true;
		}
	}

	if (bAmbiguous)
	{
		Result.Result = ERZCombatComboRequestResultType::AmbiguousTransition;
		return ResolveRequest(Result);
	}

	const FRZCombatComboTransition* Transition = BestCandidate ? BestCandidate->Transition : nullptr;
	const FRZCombatComboNode* TargetNode = BestCandidate ? BestCandidate->TargetNode : nullptr;
	if (!Transition || !TargetNode)
	{
		Result.Result = ERZCombatComboRequestResultType::NoMatchingTrigger;
		return ResolveRequest(Result);
	}

	Result.TargetNodeId = Transition->TargetNodeId;
	Result.AbilityClass = TargetNode->AbilityClass;
	Result.Payload = Transition->Payload;
	Result.Checker = Transition->Checker;
	Result.SourceNodeId = SourceNodeForCondition.NodeId;
	Result.TargetNodeId = TargetNode->NodeId;
	Result.bResolved = true;
	Result.Result = ERZCombatComboRequestResultType::Success;
	return ResolveRequest(Result);
}

bool URZCombatComboComponent::BindToAbilitySystemLifecycle()
{
	UnbindFromAbilitySystemLifecycle();
	bAutomaticAbilitySystemBindingSuppressed = false;

	if (!bObserveAbilitySystemLifecycle)
	{
		return false;
	}

	UAbilitySystemComponent* AbilitySystemComponent = ResolveAbilitySystemComponent();
	if (!IsValid(AbilitySystemComponent))
	{
		return false;
	}

	BoundAbilitySystemComponent = AbilitySystemComponent;
	AbilityActivatedDelegateHandle = AbilitySystemComponent->AbilityActivatedCallbacks.AddUObject(
		this,
		&URZCombatComboComponent::HandleAbilityActivated
	);
	AbilityCommittedDelegateHandle = AbilitySystemComponent->AbilityCommittedCallbacks.AddUObject(
		this,
		&URZCombatComboComponent::HandleAbilityCommitted
	);
	AbilityFailedDelegateHandle = AbilitySystemComponent->AbilityFailedCallbacks.AddUObject(
		this,
		&URZCombatComboComponent::HandleAbilityFailed
	);
	AbilityEndedDelegateHandle = AbilitySystemComponent->OnAbilityEnded.AddUObject(
		this,
		&URZCombatComboComponent::HandleAbilityEnded
	);

	CachePendingAbilitySystemSpecForResult(PendingResolvedResult);
	return true;
}

void URZCombatComboComponent::UnbindFromAbilitySystemLifecycle()
{
	bAutomaticAbilitySystemBindingSuppressed = true;
	if (UAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get())
	{
		if (AbilityActivatedDelegateHandle.IsValid())
		{
			AbilitySystemComponent->AbilityActivatedCallbacks.Remove(AbilityActivatedDelegateHandle);
		}
		if (AbilityCommittedDelegateHandle.IsValid())
		{
			AbilitySystemComponent->AbilityCommittedCallbacks.Remove(AbilityCommittedDelegateHandle);
		}
		if (AbilityFailedDelegateHandle.IsValid())
		{
			AbilitySystemComponent->AbilityFailedCallbacks.Remove(AbilityFailedDelegateHandle);
		}
		if (AbilityEndedDelegateHandle.IsValid())
		{
			AbilitySystemComponent->OnAbilityEnded.Remove(AbilityEndedDelegateHandle);
		}
	}

	AbilityActivatedDelegateHandle.Reset();
	AbilityCommittedDelegateHandle.Reset();
	AbilityFailedDelegateHandle.Reset();
	AbilityEndedDelegateHandle.Reset();
	BoundAbilitySystemComponent.Reset();
	ClearPendingAbilitySystemObservation();
}

bool URZCombatComboComponent::IsBoundToAbilitySystemLifecycle() const
{
	return BoundAbilitySystemComponent.IsValid()
		&& AbilityActivatedDelegateHandle.IsValid()
		&& AbilityCommittedDelegateHandle.IsValid()
		&& AbilityFailedDelegateHandle.IsValid()
		&& AbilityEndedDelegateHandle.IsValid();
}

UAbilitySystemComponent* URZCombatComboComponent::GetBoundAbilitySystemComponent() const
{
	return BoundAbilitySystemComponent.Get();
}

UAbilitySystemComponent* URZCombatComboComponent::ResolveAbilitySystemComponent() const
{
	if (IsValid(AbilitySystemComponentOverride))
	{
		return AbilitySystemComponentOverride;
	}

	AActor* Owner = GetOwner();
	if (!IsValid(Owner))
	{
		return nullptr;
	}

	TArray<AActor*, TInlineAllocator<8>> CandidateActors;
	AddComboAbilitySystemActorCandidate(CandidateActors, Owner);

	if (APawn* OwnerPawn = Cast<APawn>(Owner))
	{
		AddComboAbilitySystemActorCandidate(CandidateActors, OwnerPawn->GetController());
		AddComboAbilitySystemActorCandidate(CandidateActors, OwnerPawn->GetPlayerState());
	}

	if (AController* OwnerController = Cast<AController>(Owner))
	{
		AddComboAbilitySystemActorCandidate(CandidateActors, OwnerController->GetPawn());
		AddComboAbilitySystemActorCandidate(CandidateActors, OwnerController->PlayerState);
	}

	if (APawn* ContextPawn = ResolveComboContextPawn(Owner))
	{
		AddComboAbilitySystemActorCandidate(CandidateActors, ContextPawn);
		AddComboAbilitySystemActorCandidate(CandidateActors, ContextPawn->GetController());
		AddComboAbilitySystemActorCandidate(CandidateActors, ContextPawn->GetPlayerState());
	}

	for (AActor* CandidateActor : CandidateActors)
	{
		if (UAbilitySystemComponent* AbilitySystemComponent = FindComboAbilitySystemComponentOnActor(CandidateActor))
		{
			return AbilitySystemComponent;
		}
	}

	return nullptr;
}

bool URZCombatComboComponent::CachePendingAbilitySystemSpecForResult(const FRZCombatComboRequestResult& Result)
{
	ClearPendingAbilitySystemObservation();
	if (!bObserveAbilitySystemLifecycle
		|| Result.Result != ERZCombatComboRequestResultType::Success
		|| !Result.bResolved
		|| !Result.AbilityClass)
	{
		return false;
	}

	PendingAbilitySpecHandle = FindSpecHandleForAbilityClass(Result.AbilityClass);
	return PendingAbilitySpecHandle.IsValid();
}

FGameplayAbilitySpecHandle URZCombatComboComponent::FindSpecHandleForAbilityClass(TSubclassOf<UGameplayAbility> AbilityClass) const
{
	const UAbilitySystemComponent* AbilitySystemComponent = BoundAbilitySystemComponent.Get();
	if (!AbilitySystemComponent || !AbilityClass)
	{
		return FGameplayAbilitySpecHandle();
	}

	const FGameplayAbilitySpec* AbilitySpec = AbilitySystemComponent->FindAbilitySpecFromClass(AbilityClass);
	return AbilitySpec ? AbilitySpec->Handle : FGameplayAbilitySpecHandle();
}

bool URZCombatComboComponent::DoesLifecycleAbilityMatchPending(
	const UGameplayAbility* Ability,
	FGameplayAbilitySpecHandle AbilitySpecHandle,
	bool bRequireSpecHandle
) const
{
	if (!Ability
		|| PendingResolvedResult.Result != ERZCombatComboRequestResultType::Success
		|| !PendingResolvedResult.bResolved
		|| !PendingResolvedResult.AbilityClass)
	{
		return false;
	}

	if (!Ability->IsA(PendingResolvedResult.AbilityClass))
	{
		return false;
	}

	if (PendingAbilitySpecHandle.IsValid() && AbilitySpecHandle.IsValid())
	{
		return PendingAbilitySpecHandle == AbilitySpecHandle;
	}

	return !bRequireSpecHandle;
}

TSubclassOf<UGameplayAbility> URZCombatComboComponent::ResolveObservedAbilityClass(const UGameplayAbility* Ability) const
{
	return Ability ? Ability->GetClass() : nullptr;
}

FGameplayAbilitySpecHandle URZCombatComboComponent::ResolveObservedSpecHandle(const UGameplayAbility* Ability) const
{
	return Ability ? Ability->GetCurrentAbilitySpecHandle() : FGameplayAbilitySpecHandle();
}

void URZCombatComboComponent::ClearPendingAbilitySystemObservation()
{
	PendingAbilitySpecHandle = FGameplayAbilitySpecHandle();
}

void URZCombatComboComponent::HandleAbilityActivated(UGameplayAbility* Ability)
{
	const FGameplayAbilitySpecHandle AbilitySpecHandle = ResolveObservedSpecHandle(Ability);
	if (!DoesLifecycleAbilityMatchPending(Ability, AbilitySpecHandle, true))
	{
		return;
	}

	const TSubclassOf<UGameplayAbility> AbilityClass = ResolveObservedAbilityClass(Ability);
	if (!RecordResolvedAbilityActivated(PendingResolvedResult, AbilitySpecHandle, AbilityClass))
	{
		return;
	}

	if (bCommitComboOnAbilityActivated)
	{
		CommitResolvedCombo(PendingResolvedResult);
	}
}

void URZCombatComboComponent::HandleAbilityCommitted(UGameplayAbility* Ability)
{
	const FGameplayAbilitySpecHandle AbilitySpecHandle = ResolveObservedSpecHandle(Ability);
	if (!DoesLifecycleAbilityMatchPending(Ability, AbilitySpecHandle, true))
	{
		return;
	}

	RecordResolvedAbilityCommitted(PendingResolvedResult, AbilitySpecHandle, ResolveObservedAbilityClass(Ability));
}

void URZCombatComboComponent::HandleAbilityFailed(const UGameplayAbility* Ability, const FGameplayTagContainer& FailureTags)
{
	const FGameplayAbilitySpecHandle AbilitySpecHandle = ResolveObservedSpecHandle(Ability);
	if (!DoesLifecycleAbilityMatchPending(Ability, AbilitySpecHandle, false))
	{
		return;
	}

	RecordResolvedAbilityFailed(PendingResolvedResult, AbilitySpecHandle, ResolveObservedAbilityClass(Ability), FailureTags);
	ClearPendingAbilitySystemObservation();
}

void URZCombatComboComponent::HandleAbilityEnded(const FAbilityEndedData& AbilityEndedData)
{
	const UGameplayAbility* Ability = AbilityEndedData.AbilityThatEnded;
	if (!DoesLifecycleAbilityMatchPending(Ability, AbilityEndedData.AbilitySpecHandle, true))
	{
		return;
	}

	RecordResolvedAbilityEnded(
		PendingResolvedResult,
		AbilityEndedData.AbilitySpecHandle,
		ResolveObservedAbilityClass(Ability),
		AbilityEndedData.bWasCancelled,
		AbilityEndedData.bWasCancelled ? bResetComboOnAbilityCancelled : bResetComboOnAbilityEnded
	);
	ClearPendingAbilitySystemObservation();
}

FRZCombatComboRequestResult URZCombatComboComponent::GetPendingResolvedCombo() const
{
	return PendingResolvedResult;
}

bool URZCombatComboComponent::DoesResolvedResultMatchPending(const FRZCombatComboRequestResult& ResolvedResult) const
{
	if (ResolvedResult.Result != ERZCombatComboRequestResultType::Success || !ResolvedResult.bResolved || !ResolvedResult.AbilityClass)
	{
		return false;
	}

	if (PendingResolvedResult.Result != ERZCombatComboRequestResultType::Success || !PendingResolvedResult.bResolved)
	{
		return false;
	}

	if (PendingResolvedResult.SelectedTable != ResolvedResult.SelectedTable
		|| PendingResolvedResult.AbilityClass != ResolvedResult.AbilityClass
		|| PendingResolvedResult.Payload.Get() != ResolvedResult.Payload.Get()
		|| PendingResolvedResult.Checker.Get() != ResolvedResult.Checker.Get()
		|| !PendingResolvedResult.TriggerTag.MatchesTagExact(ResolvedResult.TriggerTag))
	{
		return false;
	}

	if (PendingResolvedResult.SourceNodeId.IsValid() != ResolvedResult.SourceNodeId.IsValid()
		|| PendingResolvedResult.TargetNodeId.IsValid() != ResolvedResult.TargetNodeId.IsValid())
	{
		return false;
	}

	if (PendingResolvedResult.SourceNodeId.IsValid() && !PendingResolvedResult.SourceNodeId.Matches(ResolvedResult.SourceNodeId))
	{
		return false;
	}

	if (PendingResolvedResult.TargetNodeId.IsValid() && !PendingResolvedResult.TargetNodeId.Matches(ResolvedResult.TargetNodeId))
	{
		return false;
	}

	return true;
}

bool URZCombatComboComponent::DoesObservedAbilityMatchPending(
	const FRZCombatComboRequestResult& ResolvedResult,
	FGameplayAbilitySpecHandle AbilitySpecHandle,
	TSubclassOf<UGameplayAbility> AbilityClass
) const
{
	if (!DoesResolvedResultMatchPending(ResolvedResult) || !AbilityClass)
	{
		return false;
	}

	UClass* ObservedClass = AbilityClass.Get();
	UClass* ExpectedClass = ResolvedResult.AbilityClass.Get();
	if (!ObservedClass || !ExpectedClass || !ObservedClass->IsChildOf(ExpectedClass))
	{
		return false;
	}

	if (ObservedAbilitySpecHandle.IsValid()
		&& AbilitySpecHandle.IsValid()
		&& ObservedAbilitySpecHandle != AbilitySpecHandle)
	{
		return false;
	}

	return true;
}

void URZCombatComboComponent::RecordObservedExecution(
	ERZCombatComboExecutionState NewExecutionState,
	FGameplayAbilitySpecHandle AbilitySpecHandle,
	TSubclassOf<UGameplayAbility> AbilityClass,
	const FGameplayTagContainer* FailureTags,
	bool bWasCancelled
)
{
	ExecutionState = NewExecutionState;
	ObservedAbilityClass = AbilityClass;
	ObservedAbilitySpecHandle = AbilitySpecHandle;
	ObservedFailureTags.Reset();
	if (FailureTags)
	{
		ObservedFailureTags = *FailureTags;
	}
	bObservedAbilityWasCancelled = bWasCancelled;
}

bool URZCombatComboComponent::RecordResolvedAbilityActivated(
	const FRZCombatComboRequestResult& ResolvedResult,
	FGameplayAbilitySpecHandle AbilitySpecHandle,
	TSubclassOf<UGameplayAbility> AbilityClass
)
{
	if (!DoesObservedAbilityMatchPending(ResolvedResult, AbilitySpecHandle, AbilityClass))
	{
		return false;
	}

	RecordObservedExecution(ERZCombatComboExecutionState::AbilityActivated, AbilitySpecHandle, AbilityClass, nullptr, false);
	RecordDebugEvent(ERZCombatComboDebugEventType::AbilityActivated);
	OnComboStateChanged.Broadcast(GetComboDebugSnapshot());
	return true;
}

bool URZCombatComboComponent::RecordResolvedAbilityCommitted(
	const FRZCombatComboRequestResult& ResolvedResult,
	FGameplayAbilitySpecHandle AbilitySpecHandle,
	TSubclassOf<UGameplayAbility> AbilityClass
)
{
	if (!DoesObservedAbilityMatchPending(ResolvedResult, AbilitySpecHandle, AbilityClass))
	{
		return false;
	}

	RecordObservedExecution(ERZCombatComboExecutionState::AbilityCommitted, AbilitySpecHandle, AbilityClass, nullptr, false);
	RecordDebugEvent(ERZCombatComboDebugEventType::AbilityCommitted);
	OnComboStateChanged.Broadcast(GetComboDebugSnapshot());
	return true;
}

bool URZCombatComboComponent::RecordResolvedAbilityFailed(
	const FRZCombatComboRequestResult& ResolvedResult,
	FGameplayAbilitySpecHandle AbilitySpecHandle,
	TSubclassOf<UGameplayAbility> AbilityClass,
	const FGameplayTagContainer& FailureTags
)
{
	if (!DoesObservedAbilityMatchPending(ResolvedResult, AbilitySpecHandle, AbilityClass))
	{
		return false;
	}

	PendingResolvedResult = FRZCombatComboRequestResult();
	bPendingComboCommitted = false;
	RecordObservedExecution(ERZCombatComboExecutionState::AbilityFailed, AbilitySpecHandle, AbilityClass, &FailureTags, false);
	RecordDebugEvent(ERZCombatComboDebugEventType::AbilityFailed);
	OnComboStateChanged.Broadcast(GetComboDebugSnapshot());
	return true;
}

bool URZCombatComboComponent::RecordResolvedAbilityEnded(
	const FRZCombatComboRequestResult& ResolvedResult,
	FGameplayAbilitySpecHandle AbilitySpecHandle,
	TSubclassOf<UGameplayAbility> AbilityClass,
	bool bWasCancelled,
	bool bResetComboState
)
{
	if (!DoesObservedAbilityMatchPending(ResolvedResult, AbilitySpecHandle, AbilityClass))
	{
		return false;
	}

	if (bResetComboState)
	{
		ResetComboState(false);
	}
	else
	{
		PendingResolvedResult = FRZCombatComboRequestResult();
		bPendingComboCommitted = false;
	}

	RecordObservedExecution(
		bWasCancelled ? ERZCombatComboExecutionState::AbilityCancelled : ERZCombatComboExecutionState::AbilityEnded,
		AbilitySpecHandle,
		AbilityClass,
		nullptr,
		bWasCancelled
	);
	RecordDebugEvent(bWasCancelled ? ERZCombatComboDebugEventType::AbilityCancelled : ERZCombatComboDebugEventType::AbilityEnded);
	if (bResetComboState)
	{
		RecordDebugEvent(ERZCombatComboDebugEventType::Reset);
	}
	OnComboStateChanged.Broadcast(GetComboDebugSnapshot());
	return true;
}

bool URZCombatComboComponent::CommitResolvedCombo(const FRZCombatComboRequestResult& ResolvedResult)
{
	if (ResolvedResult.Result != ERZCombatComboRequestResultType::Success || !ResolvedResult.bResolved || !ResolvedResult.AbilityClass)
	{
		return false;
	}

	if (PendingResolvedResult.Result != ERZCombatComboRequestResultType::Success || !PendingResolvedResult.bResolved)
	{
		return false;
	}

	if (PendingResolvedResult.SelectedTable != ResolvedResult.SelectedTable
		|| PendingResolvedResult.AbilityClass != ResolvedResult.AbilityClass
		|| PendingResolvedResult.Payload.Get() != ResolvedResult.Payload.Get()
		|| PendingResolvedResult.Checker.Get() != ResolvedResult.Checker.Get()
		|| !PendingResolvedResult.TriggerTag.MatchesTagExact(ResolvedResult.TriggerTag))
	{
		return false;
	}

	if (PendingResolvedResult.SourceNodeId.IsValid() != ResolvedResult.SourceNodeId.IsValid()
		|| PendingResolvedResult.TargetNodeId.IsValid() != ResolvedResult.TargetNodeId.IsValid())
	{
		return false;
	}

	if (PendingResolvedResult.SourceNodeId.IsValid() && !PendingResolvedResult.SourceNodeId.Matches(ResolvedResult.SourceNodeId))
	{
		return false;
	}

	if (PendingResolvedResult.TargetNodeId.IsValid() && !PendingResolvedResult.TargetNodeId.Matches(ResolvedResult.TargetNodeId))
	{
		return false;
	}

	if (!IsValid(ActiveComboTable) || ResolvedResult.SelectedTable != ActiveComboTable)
	{
		return false;
	}

	if (ResolvedResult.SourceNodeId.IsValid() != CurrentNodeId.IsValid())
	{
		return false;
	}

	if (ResolvedResult.SourceNodeId.IsValid() && !ResolvedResult.SourceNodeId.Matches(CurrentNodeId))
	{
		return false;
	}

	const TArray<FRZCombatComboTransition>* CommitTransitions = nullptr;
	if (ResolvedResult.SourceNodeId.IsValid())
	{
		const FRZCombatComboNode* SourceNode = ActiveComboTable->FindNode(ResolvedResult.SourceNodeId);
		if (!SourceNode)
		{
			return false;
		}

		CommitTransitions = &SourceNode->Transitions;
	}
	else
	{
		CommitTransitions = &ActiveComboTable->EntryTransitions;
	}

	bool bFoundResolvedTransition = false;
	for (const FRZCombatComboTransition& Transition : *CommitTransitions)
	{
		if (Transition.bEnabled
			&& Transition.TriggerTag.MatchesTagExact(ResolvedResult.TriggerTag)
			&& Transition.TargetNodeId.Matches(ResolvedResult.TargetNodeId)
			&& Transition.Payload.Get() == ResolvedResult.Payload.Get()
			&& Transition.Checker.Get() == ResolvedResult.Checker.Get())
		{
			bFoundResolvedTransition = true;
			break;
		}
	}

	if (!bFoundResolvedTransition)
	{
		return false;
	}

	const FRZCombatComboNode* TargetNode = ActiveComboTable->FindNode(ResolvedResult.TargetNodeId);
	if (!TargetNode || TargetNode->AbilityClass != ResolvedResult.AbilityClass)
	{
		return false;
	}

	CurrentNodeId = TargetNode->NodeId;
	CurrentAbilityClass = TargetNode->AbilityClass;
	LastRequestResult = ResolvedResult;
	PendingResolvedResult = ResolvedResult;
	bPendingComboCommitted = true;
	RecordObservedExecution(
		ERZCombatComboExecutionState::ComboCommitted,
		ObservedAbilitySpecHandle,
		ResolvedResult.AbilityClass,
		nullptr,
		false
	);
	RecordDebugEvent(ERZCombatComboDebugEventType::ComboCommit);
	OnComboStateChanged.Broadcast(GetComboDebugSnapshot());
	return true;
}

FRZCombatComboNodeId URZCombatComboComponent::GetCurrentNodeId() const
{
	return CurrentNodeId;
}

bool URZCombatComboComponent::CanUseComboTransition_Implementation(
	const FRZCombatComboRequest& Request,
	const FRZCombatComboNode& SourceNode,
	const FRZCombatComboTransition& Transition,
	const FRZCombatComboNode& TargetNode
) const
{
	(void)Request;
	(void)SourceNode;
	(void)Transition;
	(void)TargetNode;

	return true;
}

FRZCombatComboDebugSnapshot URZCombatComboComponent::GetComboDebugSnapshot() const
{
	FRZCombatComboDebugSnapshot Snapshot;
	Snapshot.ActiveTable = ActiveComboTable;
	Snapshot.CurrentNodeId = CurrentNodeId;
	Snapshot.CurrentAbilityClass = CurrentAbilityClass;
	Snapshot.LastTriggerTag = LastRequestResult.TriggerTag;
	Snapshot.LastResult = LastRequestResult.Result;
	Snapshot.LastSourceNodeId = LastRequestResult.SourceNodeId;
	Snapshot.LastTargetNodeId = LastRequestResult.TargetNodeId;
	if (!bPendingComboCommitted && PendingResolvedResult.Result == ERZCombatComboRequestResultType::Success && PendingResolvedResult.bResolved)
	{
		Snapshot.PendingSourceNodeId = PendingResolvedResult.SourceNodeId;
		Snapshot.PendingTargetNodeId = PendingResolvedResult.TargetNodeId;
	}
	Snapshot.ExecutionState = ExecutionState;
	Snapshot.ObservedAbilityClass = ObservedAbilityClass;
	Snapshot.ObservedAbilitySpecHandle = ObservedAbilitySpecHandle;
	Snapshot.ObservedFailureTags = ObservedFailureTags;
	Snapshot.bObservedAbilityWasCancelled = bObservedAbilityWasCancelled;

	if (!IsValid(ActiveComboTable))
	{
		return Snapshot;
	}

	const TArray<FRZCombatComboTransition>* Transitions = nullptr;
	if (CurrentNodeId.IsValid())
	{
		if (const FRZCombatComboNode* SourceNode = ActiveComboTable->FindNode(CurrentNodeId))
		{
			Transitions = &SourceNode->Transitions;
		}
	}
	else
	{
		Transitions = &ActiveComboTable->EntryTransitions;
	}

	if (Transitions)
	{
		for (const FRZCombatComboTransition& Transition : *Transitions)
		{
			if (Transition.bEnabled && Transition.TargetNodeId.IsValid())
			{
				Snapshot.AvailableTargetNodeIds.Add(Transition.TargetNodeId);
			}
		}
	}

	return Snapshot;
}

void URZCombatComboComponent::RecordDebugEvent(ERZCombatComboDebugEventType Type, const FString& PreviousTablePath)
{
#if !UE_BUILD_SHIPPING
	if (IsTemplate())
	{
		return;
	}
	if (RecentDebugEvents.Num() == MaxRecentComboDebugEvents)
	{
		RecentDebugEvents.RemoveAt(0, 1, EAllowShrinking::No);
	}
	FRZCombatComboDebugEvent& Event = RecentDebugEvents.AddDefaulted_GetRef();
	Event.Sequence = ++DebugEventSequence;
	Event.Frame = GFrameCounter;
	Event.WorldTime = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.0;
	Event.Type = Type;
	Event.TablePath = GetPathNameSafe(ActiveComboTable);
	Event.PreviousTablePath = PreviousTablePath;
	Event.SourceNodeId = LastRequestResult.SourceNodeId;
	Event.TargetNodeId = LastRequestResult.TargetNodeId;
	Event.CurrentNodeId = CurrentNodeId;
	Event.TriggerTag = LastRequestResult.TriggerTag;
	Event.Result = LastRequestResult.Result;
	Event.ExecutionState = ExecutionState;
	Event.AbilityClassPath = GetPathNameSafe(ObservedAbilityClass.Get());
	Event.AbilitySpecHandle = ObservedAbilitySpecHandle;
	Event.FailureTags = ObservedFailureTags;
#endif
}

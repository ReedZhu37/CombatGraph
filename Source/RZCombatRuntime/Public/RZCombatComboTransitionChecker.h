#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "RZCombatComboTableTypes.h"
#include "RZCombatComboTransitionChecker.generated.h"

class AActor;
class APawn;
class URZCombatComboComponent;
class URZCombatComboTableAsset;

USTRUCT(BlueprintType)
struct RZCOMBATRUNTIME_API FRZCombatComboTransitionCheckContext
{
	GENERATED_BODY()

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	TObjectPtr<URZCombatComboComponent> ComboComponent = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	TObjectPtr<AActor> OwnerActor = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	TObjectPtr<APawn> Pawn = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	TObjectPtr<URZCombatComboTableAsset> ActiveTable = nullptr;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	FRZCombatComboRequest Request;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	FRZCombatComboNode SourceNode;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	FRZCombatComboTransition Transition;

	UPROPERTY(BlueprintReadOnly, Category = "RZCombat|Combo")
	FRZCombatComboNode TargetNode;
};

UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class RZCOMBATRUNTIME_API URZCombatComboTransitionChecker : public UObject
{
	GENERATED_BODY()

public:
	UFUNCTION(BlueprintCallable, BlueprintNativeEvent, Category = "RZCombat|Combo")
	bool CanUseTransition(const FRZCombatComboTransitionCheckContext& Context) const;
	virtual bool CanUseTransition_Implementation(const FRZCombatComboTransitionCheckContext& Context) const;
};

UCLASS(Abstract, BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories)
class RZCOMBATRUNTIME_API URZCombatComboCompositeChecker : public URZCombatComboTransitionChecker
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "RZCombat|Combo")
	TArray<TObjectPtr<URZCombatComboTransitionChecker>> Checkers;
};

UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories, meta = (DisplayName = "Combo And Checker"))
class RZCOMBATRUNTIME_API URZCombatComboAndChecker : public URZCombatComboCompositeChecker
{
	GENERATED_BODY()

public:
	virtual bool CanUseTransition_Implementation(const FRZCombatComboTransitionCheckContext& Context) const override;
};

UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories, meta = (DisplayName = "Combo Or Checker"))
class RZCOMBATRUNTIME_API URZCombatComboOrChecker : public URZCombatComboCompositeChecker
{
	GENERATED_BODY()

public:
	virtual bool CanUseTransition_Implementation(const FRZCombatComboTransitionCheckContext& Context) const override;
};

UCLASS(BlueprintType, Blueprintable, EditInlineNew, DefaultToInstanced, CollapseCategories, meta = (DisplayName = "Combo Not Checker"))
class RZCOMBATRUNTIME_API URZCombatComboNotChecker : public URZCombatComboTransitionChecker
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Instanced, Category = "RZCombat|Combo")
	TObjectPtr<URZCombatComboTransitionChecker> Checker = nullptr;

	virtual bool CanUseTransition_Implementation(const FRZCombatComboTransitionCheckContext& Context) const override;
};

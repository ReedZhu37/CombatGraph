#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "GameplayTagContainer.h"
#include "RZCombatComboMoveAsset.generated.h"

class UAnimMontage;

UCLASS(BlueprintType)
class RZCOMBATRUNTIME_API URZCombatComboMoveAsset : public UPrimaryDataAsset
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FGameplayTag MoveTag;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	TObjectPtr<UAnimMontage> Montage;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FName SectionName;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo", meta = (ClampMin = "0.0"))
	float PlayRate = 1.0f;

#if WITH_EDITORONLY_DATA
	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "RZCombat|Combo")
	FText DesignerNote;
#endif
};

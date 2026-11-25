#pragma once

#include "CoreMinimal.h"

class URZCombatComboGraph;
class URZCombatComboTableAsset;

struct RZCOMBATEDITOR_API FRZCombatComboGraphCompileResult
{
	bool bSucceeded = false;
	TArray<FText> Errors;
	TArray<FText> Warnings;

	bool HasErrors() const
	{
		return Errors.Num() > 0;
	}
};

struct RZCOMBATEDITOR_API FRZCombatComboGraphCompiler
{
	static URZCombatComboGraph* EnsureGraph(URZCombatComboTableAsset* ComboTable);
	static FRZCombatComboGraphCompileResult Compile(URZCombatComboTableAsset* ComboTable);
};

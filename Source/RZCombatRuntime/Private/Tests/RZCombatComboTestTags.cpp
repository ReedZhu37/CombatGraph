#include "CoreMinimal.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "NativeGameplayTags.h"

// Keep the automation suite runnable without a host project's Gameplay Tag config.
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ComboTestAbilityA, "Ability.Combo.Test.A");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ComboTestAbilityAB, "Ability.Combo.Test.AB");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ComboTestAbilityABC, "Ability.Combo.Test.ABC");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ComboTestAbilityHeavy, "Ability.Combo.Test.Heavy");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ComboTestAbilityHighPriority, "Ability.Combo.Test.HighPriority");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ComboTestAbilityLowPriority, "Ability.Combo.Test.LowPriority");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ComboTestBranchWindow, "Combo.Window.Branch");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ComboTestInputHeavy, "Input.Action.Attack.Heavy");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ComboTestInputLight, "Input.Action.Attack.Light");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ComboTestStateReady, "State.Combat.Ready");
UE_DEFINE_GAMEPLAY_TAG_STATIC(TAG_ComboTestStateStunned, "State.Stunned");

#endif // WITH_DEV_AUTOMATION_TESTS

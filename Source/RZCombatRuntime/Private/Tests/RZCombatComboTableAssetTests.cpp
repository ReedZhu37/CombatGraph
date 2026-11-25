#if WITH_DEV_AUTOMATION_TESTS

#include "RZCombatComboTableAsset.h"

#include "Misc/AutomationTest.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboNodeIdGuidIsEditorReadOnlyTest,
	"RZCombat.Combo.Table.NodeIdGuidIsEditorReadOnly",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboNodeIdGuidIsEditorReadOnlyTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FProperty* GuidProperty = FRZCombatComboNodeId::StaticStruct()->FindPropertyByName(GET_MEMBER_NAME_CHECKED(FRZCombatComboNodeId, Guid));
	TestNotNull(TEXT("NodeId should expose a Guid property."), GuidProperty);
	if (!GuidProperty)
	{
		return false;
	}

	TestTrue(TEXT("NodeId Guid should be visible but not user editable."), GuidProperty->HasAnyPropertyFlags(CPF_EditConst));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboNodeIdMatchesByNameTest,
	"RZCombat.Combo.Table.NodeIdMatchesByName",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboNodeIdMatchesByNameTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	FRZCombatComboNodeId A;
	A.Name = TEXT("Light_1");

	FRZCombatComboNodeId B;
	B.Name = TEXT("Light_1");

	TestTrue(TEXT("Node ids with matching names should match when GUIDs are unset."), A.Matches(B));
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FRZCombatComboTableFindsNodeTest,
	"RZCombat.Combo.Table.FindsNodeById",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FRZCombatComboTableFindsNodeTest::RunTest(const FString& Parameters)
{
	(void)Parameters;

	URZCombatComboTableAsset* Table = NewObject<URZCombatComboTableAsset>();
	FRZCombatComboNode& Node = Table->Nodes.AddDefaulted_GetRef();
	Node.NodeId.Name = TEXT("Light_1");

	FRZCombatComboNodeId Query;
	Query.Name = TEXT("Light_1");

	TestNotNull(TEXT("Table should find a node by id."), Table->FindNode(Query));
	return true;
}

#endif

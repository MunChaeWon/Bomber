// Copyright (c) Yevhenii Selivanov.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Structures/BmrCell.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBmrCellValidityTest,
	"Bomber.Unit.Cell.Validity",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBmrCellValidityTest::RunTest(const FString& Parameters)
{
	const FBmrCell ValidCell(200.f, 400.f, 0.f);

	TestTrue(TEXT("A regular cell is valid"), ValidCell.IsValid());
	TestFalse(TEXT("A regular cell is not the invalid sentinel"), ValidCell.IsInvalidCell());
	TestFalse(TEXT("The invalid sentinel is not valid"), FBmrCell::InvalidCell.IsValid());
	TestTrue(TEXT("The invalid sentinel identifies itself as invalid"), FBmrCell::InvalidCell.IsInvalidCell());

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

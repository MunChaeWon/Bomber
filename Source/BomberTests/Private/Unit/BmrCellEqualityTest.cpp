// Copyright (c) Yevhenii Selivanov.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Structures/BmrCell.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBmrCellEqualityTest,
	"Bomber.Unit.Cell.Equality",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBmrCellEqualityTest::RunTest(const FString& Parameters)
{
	const FBmrCell ReferenceCell(200.f, 400.f, 0.f);
	const FBmrCell EqualCell(200.f, 400.f, 0.f);
	const FBmrCell DifferentXCell(201.f, 400.f, 0.f);
	const FBmrCell DifferentYCell(200.f, 401.f, 0.f);
	const FBmrCell DifferentZCell(200.f, 400.f, 1.f);

	TestTrue(TEXT("Cells with equal coordinates compare equal"), ReferenceCell == EqualCell);
	TestFalse(TEXT("Cells with equal coordinates do not compare unequal"), ReferenceCell != EqualCell);
	TestFalse(TEXT("Cells with different X coordinates do not compare equal"), ReferenceCell == DifferentXCell);
	TestFalse(TEXT("Cells with different Y coordinates do not compare equal"), ReferenceCell == DifferentYCell);
	TestFalse(TEXT("Cells with different Z coordinates do not compare equal"), ReferenceCell == DifferentZCell);
	TestTrue(TEXT("Cells with different coordinates compare unequal"), ReferenceCell != DifferentXCell);
	TestTrue(TEXT("The invalid sentinel compares equal to itself"), FBmrCell::InvalidCell == FBmrCell::InvalidCell);
	TestTrue(TEXT("A regular cell compares unequal to the invalid sentinel"), ReferenceCell != FBmrCell::InvalidCell);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

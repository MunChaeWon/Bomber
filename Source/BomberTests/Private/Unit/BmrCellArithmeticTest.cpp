// Copyright (c) Yevhenii Selivanov.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Structures/BmrCell.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBmrCellArithmeticTest,
	"Bomber.Unit.Cell.Arithmetic",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBmrCellArithmeticTest::RunTest(const FString& Parameters)
{
	const FBmrCell LeftCell(200.f, -400.f, 2.f);
	const FBmrCell RightCell(-50.f, 100.f, 3.f);
	const FBmrCell ExpectedSum(150.f, -300.f, 5.f);
	const FBmrCell ExpectedDifference(250.f, -500.f, -1.f);

	TestTrue(TEXT("Addition combines each coordinate"), LeftCell + RightCell == ExpectedSum);
	TestTrue(TEXT("Subtraction subtracts each coordinate"), LeftCell - RightCell == ExpectedDifference);

	FBmrCell CompoundCell = LeftCell;
	CompoundCell += RightCell;
	TestTrue(TEXT("Compound addition updates the target cell"), CompoundCell == ExpectedSum);
	CompoundCell -= RightCell;
	TestTrue(TEXT("Compound subtraction restores the original cell"), CompoundCell == LeftCell);

	TestTrue(TEXT("Integer scaling multiplies every coordinate"), LeftCell * 2 == FBmrCell(400.f, -800.f, 4.f));
	TestTrue(TEXT("Fractional scaling multiplies every coordinate"), LeftCell * 0.5f == FBmrCell(100.f, -200.f, 1.f));
	TestTrue(TEXT("Negative scaling reverses every coordinate"), LeftCell * -1 == FBmrCell(-200.f, 400.f, -2.f));
	TestTrue(TEXT("Non-mutating operators preserve the original cell"), LeftCell == FBmrCell(200.f, -400.f, 2.f));

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

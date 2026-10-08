// Copyright (c) Yevhenii Selivanov.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Structures/BmrCell.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBmrCellConstructionTest,
	"Bomber.Unit.Cell.Construction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBmrCellConstructionTest::RunTest(const FString& Parameters)
{
	const FBmrCell VectorCell(FVector(199.6f, 400.4f, -199.6f));

	TestEqual(TEXT("Vector constructor rounds X to the nearest whole unit"), VectorCell.X(), 200.f);
	TestEqual(TEXT("Vector constructor rounds Y to the nearest whole unit"), VectorCell.Y(), 400.f);
	TestEqual(TEXT("Vector constructor rounds negative Z to the nearest whole unit"), VectorCell.Z(), -200.f);

	const FBmrCell ScalarCell(199.4f, -400.4f, 0.49f);

	TestEqual(TEXT("Scalar constructor rounds X to the nearest whole unit"), ScalarCell.X(), 199.f);
	TestEqual(TEXT("Scalar constructor rounds negative Y to the nearest whole unit"), ScalarCell.Y(), -400.f);
	TestEqual(TEXT("Scalar constructor rounds Z to zero when below one half"), ScalarCell.Z(), 0.f);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

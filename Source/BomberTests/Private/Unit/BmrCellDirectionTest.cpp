// Copyright (c) Yevhenii Selivanov.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"
#include "Structures/BmrCell.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBmrCellDirectionTest,
	"Bomber.Unit.Cell.Direction",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBmrCellDirectionTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Forward enum maps to the forward cell"), FBmrCell::GetCellDirection(ECD::Forward) == FBmrCell::ForwardCell);
	TestTrue(TEXT("Backward enum maps to the backward cell"), FBmrCell::GetCellDirection(ECD::Backward) == FBmrCell::BackwardCell);
	TestTrue(TEXT("Right enum maps to the right cell"), FBmrCell::GetCellDirection(ECD::Right) == FBmrCell::RightCell);
	TestTrue(TEXT("Left enum maps to the left cell"), FBmrCell::GetCellDirection(ECD::Left) == FBmrCell::LeftCell);
	TestTrue(TEXT("None maps to the invalid sentinel"), FBmrCell::GetCellDirection(ECD::None) == FBmrCell::InvalidCell);
	TestTrue(TEXT("Combined directions map to the invalid sentinel"), FBmrCell::GetCellDirection(ECD::All) == FBmrCell::InvalidCell);

	TestTrue(TEXT("Forward cell maps to the forward enum"), FBmrCell::GetCellDirection(FBmrCell::ForwardCell) == ECD::Forward);
	TestTrue(TEXT("Backward cell maps to the backward enum"), FBmrCell::GetCellDirection(FBmrCell::BackwardCell) == ECD::Backward);
	TestTrue(TEXT("Right cell maps to the right enum"), FBmrCell::GetCellDirection(FBmrCell::RightCell) == ECD::Right);
	TestTrue(TEXT("Left cell maps to the left enum"), FBmrCell::GetCellDirection(FBmrCell::LeftCell) == ECD::Left);

	const FBmrCell UnknownDirection(2.f, 2.f, 0.f);
	TestTrue(TEXT("An unknown direction cell maps to None"), FBmrCell::GetCellDirection(UnknownDirection) == ECD::None);

	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

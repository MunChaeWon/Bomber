// Copyright (c) Yevhenii Selivanov.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBomberSmokeTest,
	"Bomber.Unit.Smoke",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::SmokeFilter)

bool FBomberSmokeTest::RunTest(const FString& Parameters)
{
	TestTrue(TEXT("Bomber test module is loaded and can execute tests"), true);
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

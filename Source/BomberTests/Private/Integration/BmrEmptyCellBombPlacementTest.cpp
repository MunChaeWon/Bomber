// Copyright (c) Yevhenii Selivanov.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

// Bomber
#include "AbilitySystem/Abilities/BmrBombPlaceAbility.h"
#include "Actors/BmrPawn.h"
#include "Bomber.h"
#include "Components/BmrMapComponent.h"
#include "Components/BmrMoverComponent.h"
#include "Structures/BmrCell.h"
#include "Structures/BmrGameplayTags.h"
#include "Subsystems/GlobalMessageSubsystem.h"
#include "UtilityLibraries/BmrActorUtilsLibrary.h"
#include "UtilityLibraries/BmrCellUtilsLibrary.h"

// UE
#include "AbilitySystemComponent.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "Tests/AutomationCommon.h"
#include "Tests/AutomationEditorCommon.h"

namespace BmrEmptyCellBombPlacementTest
{
constexpr double TimeoutSeconds = 30.0;

struct FTestState
{
	double SetupDeadline = 0.0;
	double PlacementDeadline = 0.0;
	FBmrCell TargetCell = FBmrCell::InvalidCell;
	int32 InitialBombCount = 0;
	bool bPlacementRequested = false;
};

UWorld* GetPIEWorld()
{
	if (!GEngine)
	{
		return nullptr;
	}

	for (const FWorldContext& WorldContext : GEngine->GetWorldContexts())
	{
		if (WorldContext.WorldType == EWorldType::PIE && WorldContext.World())
		{
			return WorldContext.World();
		}
	}

	return nullptr;
}

int32 CountBombsAtCell(const FBmrCell& Cell)
{
	TSet<UBmrMapComponent*> BombComponents;
	UBmrActorUtilsLibrary::GetLevelActors(BombComponents, TO_FLAG(EBmrActorType::Bomb));

	int32 Count = 0;
	for (const UBmrMapComponent* BombComponent : BombComponents)
	{
		if (BombComponent && BombComponent->GetCell() == Cell)
		{
			++Count;
		}
	}

	return Count;
}
} // namespace BmrEmptyCellBombPlacementTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBmrEmptyCellBombPlacementTest,
	"Bomber.Integration.Bomb.EmptyCellPlacement",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBmrEmptyCellBombPlacementTest::RunTest(const FString& Parameters)
{
	using namespace BmrEmptyCellBombPlacementTest;

	if (!AutomationOpenMap(TEXT("/Game/Bomber/Maps/Main")))
	{
		AddError(TEXT("INT-BOMB-001: Failed to open /Game/Bomber/Maps/Main."));
		return false;
	}

	const TSharedRef<FTestState> State = MakeShared<FTestState>();
	State->SetupDeadline = FPlatformTime::Seconds() + TimeoutSeconds;

	AddCommand(new FStartPIECommand(false));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		UWorld* PIEWorld = GetPIEWorld();
		if (PIEWorld)
		{
			for (TActorIterator<ABmrPawn> PawnIt(PIEWorld); PawnIt; ++PawnIt)
			{
				ABmrPawn* Pawn = *PawnIt;
				UAbilitySystemComponent* AbilitySystemComponent = Pawn ? Pawn->GetAbilitySystemComponent() : nullptr;
				UBmrMapComponent* MapComponent = UBmrMapComponent::GetMapComponent(Pawn);
				UBmrMoverComponent* MoverComponent = Pawn ? Pawn->GetMoverComponent() : nullptr;
				if (!Pawn || !AbilitySystemComponent || !MapComponent || !MoverComponent || !MapComponent->GetCell().IsValid())
				{
					continue;
				}

				bool bHasBombPlacementAbility = false;
				for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent->GetActivatableAbilities())
				{
					if (AbilitySpec.Ability && AbilitySpec.Ability->IsA<UBmrBombPlaceAbility>())
					{
						bHasBombPlacementAbility = true;
						break;
					}
				}

				if (!bHasBombPlacementAbility)
				{
					continue;
				}

				State->TargetCell = MapComponent->GetCell();
				const bool bHasBlockingActor = UBmrCellUtilsLibrary::IsCellHasAnyMatchingActor(
					State->TargetCell,
					TO_FLAG(~EBmrActorType::Player));
				if (bHasBlockingActor)
				{
					continue;
				}

				State->InitialBombCount = CountBombsAtCell(State->TargetCell);
				if (State->InitialBombCount != 0)
				{
					continue;
				}

				// Main opens in its menu state under command-line PIE. Establish the test case
				// precondition that the pawn can use gameplay abilities before requesting placement.
				MoverComponent->SetBlockMovement(false);
				if (AbilitySystemComponent->HasMatchingGameplayTag(BmrGameplayTags::GameplayEffect::Block::Movement))
				{
					continue;
				}

				Pawn->SpawnBomb();
				State->bPlacementRequested = true;
				State->PlacementDeadline = FPlatformTime::Seconds() + TimeoutSeconds;
				return true;
			}
		}

		if (FPlatformTime::Seconds() >= State->SetupDeadline)
		{
			AddError(TEXT("INT-BOMB-001: Timed out waiting for a pawn with the bomb ability on a valid empty cell."));
			return true;
		}

		return false;
	}));

	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (!State->bPlacementRequested)
		{
			return true;
		}

		const int32 BombCount = CountBombsAtCell(State->TargetCell);
		if (BombCount == State->InitialBombCount + 1)
		{
			TestEqual(TEXT("INT-BOMB-001: Empty cell contains exactly one newly placed bomb"), BombCount, 1);
			return true;
		}

		if (FPlatformTime::Seconds() >= State->PlacementDeadline)
		{
			AddError(FString::Printf(
				TEXT("INT-BOMB-001: Timed out waiting for one bomb at cell %s; found %d."),
				*State->TargetCell.ToString(),
				BombCount));
			return true;
		}

		return false;
	}));

	// Keep PIE visible long enough to observe the placed bomb and capture video evidence.
	AddCommand(new FWaitLatentCommand(5.0));
	AddCommand(new FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBmrOccupiedCellBombPlacementTest,
	"Bomber.Integration.Bomb.RejectOccupiedCell",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBmrOccupiedCellBombPlacementTest::RunTest(const FString& Parameters)
{
	using namespace BmrEmptyCellBombPlacementTest;

	if (!AutomationOpenMap(TEXT("/Game/Bomber/Maps/Main")))
	{
		AddError(TEXT("INT-BOMB-002: Failed to open /Game/Bomber/Maps/Main."));
		return false;
	}

	const TSharedRef<FTestState> State = MakeShared<FTestState>();
	State->SetupDeadline = FPlatformTime::Seconds() + TimeoutSeconds;

	AddCommand(new FStartPIECommand(false));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		UWorld* PIEWorld = GetPIEWorld();
		if (PIEWorld)
		{
			for (TActorIterator<ABmrPawn> PawnIt(PIEWorld); PawnIt; ++PawnIt)
			{
				ABmrPawn* Pawn = *PawnIt;
				UAbilitySystemComponent* AbilitySystemComponent = Pawn ? Pawn->GetAbilitySystemComponent() : nullptr;
				UBmrMoverComponent* MoverComponent = Pawn ? Pawn->GetMoverComponent() : nullptr;
				if (!Pawn || !AbilitySystemComponent || !MoverComponent)
				{
					continue;
				}

				bool bHasBombPlacementAbility = false;
				for (const FGameplayAbilitySpec& AbilitySpec : AbilitySystemComponent->GetActivatableAbilities())
				{
					if (AbilitySpec.Ability && AbilitySpec.Ability->IsA<UBmrBombPlaceAbility>())
					{
						bHasBombPlacementAbility = true;
						break;
					}
				}

				if (!bHasBombPlacementAbility)
				{
					continue;
				}

				const FBmrCells OccupiedCells = UBmrCellUtilsLibrary::GetAllCellsWithActors(
					TO_FLAG(EBmrActorType::Wall | EBmrActorType::Box));
				if (OccupiedCells.IsEmpty())
				{
					continue;
				}

				State->TargetCell = *OccupiedCells.CreateConstIterator();
				State->InitialBombCount = CountBombsAtCell(State->TargetCell);
				if (!State->TargetCell.IsValid() || State->InitialBombCount != 0)
				{
					continue;
				}

				// Establish the precondition that occupancy is the only reason for rejection.
				MoverComponent->SetBlockMovement(false);
				if (AbilitySystemComponent->HasMatchingGameplayTag(BmrGameplayTags::GameplayEffect::Block::Movement))
				{
					continue;
				}

				FGameplayEventData EventData;
				EventData.EventTag = BmrGameplayTags::Event::Bomb_Placed;
				EventData.Instigator = Pawn;
				EventData.EventMagnitude = UBmrCellUtilsLibrary::GetIndexByCellOnLevel(State->TargetCell);
				UGlobalMessageSubsystem::BroadcastGlobalMessage(EventData, Pawn);

				State->bPlacementRequested = true;
				State->PlacementDeadline = FPlatformTime::Seconds() + 1.0;
				return true;
			}
		}

		if (FPlatformTime::Seconds() >= State->SetupDeadline)
		{
			AddError(TEXT("INT-BOMB-002: Timed out waiting for a pawn with the bomb ability and an occupied cell."));
			return true;
		}

		return false;
	}));

	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (!State->bPlacementRequested)
		{
			return true;
		}

		const int32 BombCount = CountBombsAtCell(State->TargetCell);
		if (BombCount != State->InitialBombCount)
		{
			AddError(FString::Printf(
				TEXT("INT-BOMB-002: Occupied cell accepted bomb placement; found %d bomb(s)."),
				BombCount));
			return true;
		}

		if (FPlatformTime::Seconds() >= State->PlacementDeadline)
		{
			TestEqual(TEXT("INT-BOMB-002: Occupied cell contains no bomb after placement request"), BombCount, 0);
			return true;
		}

		return false;
	}));

	// Keep PIE visible long enough to capture the rejected placement as video evidence.
	AddCommand(new FWaitLatentCommand(5.0));
	AddCommand(new FEndPlayMapCommand());
	return true;
}
#endif // WITH_DEV_AUTOMATION_TESTS

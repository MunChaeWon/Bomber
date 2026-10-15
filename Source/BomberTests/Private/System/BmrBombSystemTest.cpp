// Copyright (c) Yevhenii Selivanov.

#if WITH_DEV_AUTOMATION_TESTS

#include "Misc/AutomationTest.h"

// Bomber
#include "AbilitySystem/Abilities/BmrBombPlaceAbility.h"
#include "Actors/BmrBombAbilityActor.h"
#include "Actors/BmrGeneratedMap.h"
#include "Actors/BmrPawn.h"
#include "Bomber.h"
#include "Components/BmrMapComponent.h"
#include "Components/BmrMoverComponent.h"
#include "Controllers/BmrAIController.h"
#include "DataAssets/BmrBombDataAsset.h"
#include "GameFramework/BmrGameState.h"
#include "Structures/BmrCell.h"
#include "Structures/BmrGameStateTag.h"
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

namespace BmrBombSystemTest
{
constexpr double SetupTimeoutSeconds = 30.0;
constexpr double ActionTimeoutSeconds = 10.0;
constexpr int32 LoadBombLimit = 32;

struct FBaseState
{
	double Deadline = 0.0;
	TWeakObjectPtr<ABmrPawn> Pawn;
	FBmrCell TargetCell = FBmrCell::InvalidCell;
	bool bSetupComplete = false;
};

struct FBombLifecycleState : FBaseState
{
	double ExplosionDeadline = 0.0;
	bool bBombObserved = false;
};

struct FBoxDestructionState : FBombLifecycleState
{
	FBmrCell BoxCell = FBmrCell::InvalidCell;
};

struct FLoadState : FBaseState
{
	FBmrCells LoadCells;
	int32 RequestedBombCount = 0;
	int32 SpawnedBombCount = 0;
	bool bCleanupRequested = false;
	bool bNormalPlacementRequested = false;
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

void DisableAIBombPlacement(UWorld* World)
{
	if (!World)
	{
		return;
	}

	for (TActorIterator<ABmrAIController> ControllerIt(World); ControllerIt; ++ControllerIt)
	{
		ControllerIt->SetAICanSpawnBomb(false);
	}
}

bool SetInGameState(UWorld* World)
{
	ABmrGameState* GameState = World ? World->GetGameState<ABmrGameState>() : nullptr;
	UAbilitySystemComponent* WorldASC = GameState ? GameState->GetAbilitySystemComponent() : nullptr;
	if (!WorldASC)
	{
		return false;
	}

	if (!WorldASC->HasMatchingGameplayTag(FBmrGameStateTag::InGame))
	{
		WorldASC->AddLooseGameplayTag(FBmrGameStateTag::InGame);
	}
	return true;
}

ABmrPawn* FindReadyPawn(UWorld* World)
{
	if (!World)
	{
		return nullptr;
	}

	for (TActorIterator<ABmrPawn> PawnIt(World); PawnIt; ++PawnIt)
	{
		ABmrPawn* Pawn = *PawnIt;
		UAbilitySystemComponent* ASC = Pawn ? Pawn->GetAbilitySystemComponent() : nullptr;
		UBmrMapComponent* MapComponent = UBmrMapComponent::GetMapComponent(Pawn);
		UBmrMoverComponent* MoverComponent = Pawn ? Pawn->GetMoverComponent() : nullptr;
		if (!Pawn || !ASC || !MapComponent || !MoverComponent || !MapComponent->GetCell().IsValid())
		{
			continue;
		}

		bool bHasBombPlacementAbility = false;
		for (const FGameplayAbilitySpec& AbilitySpec : ASC->GetActivatableAbilities())
		{
			if (AbilitySpec.Ability && AbilitySpec.Ability->IsA<UBmrBombPlaceAbility>())
			{
				bHasBombPlacementAbility = true;
				break;
			}
		}

		if (bHasBombPlacementAbility)
		{
			MoverComponent->SetBlockMovement(false);
			return Pawn;
		}
	}

	return nullptr;
}

int32 CountActors(int32 ActorTypes)
{
	TSet<UBmrMapComponent*> Components;
	UBmrActorUtilsLibrary::GetLevelActors(Components, ActorTypes);
	return Components.Num();
}

int32 CountActorsAtCell(const FBmrCell& Cell, int32 ActorTypes)
{
	TSet<UBmrMapComponent*> Components;
	UBmrActorUtilsLibrary::GetLevelActors(Components, ActorTypes);

	int32 Count = 0;
	for (const UBmrMapComponent* Component : Components)
	{
		if (Component && Component->GetCell() == Cell)
		{
			++Count;
		}
	}
	return Count;
}

int32 CountBombsOnCells(const FBmrCells& Cells)
{
	TSet<UBmrMapComponent*> BombComponents;
	UBmrActorUtilsLibrary::GetLevelActors(BombComponents, TO_FLAG(EAT::Bomb));

	int32 Count = 0;
	for (const UBmrMapComponent* BombComponent : BombComponents)
	{
		if (BombComponent && Cells.Contains(BombComponent->GetCell()))
		{
			++Count;
		}
	}
	return Count;
}

bool RequestBombAtCell(ABmrPawn* Pawn, const FBmrCell& Cell)
{
	if (!Pawn || !Cell.IsValid())
	{
		return false;
	}

	FGameplayEventData EventData;
	EventData.EventTag = BmrGameplayTags::Event::Bomb_Placed;
	EventData.Instigator = Pawn;
	EventData.EventMagnitude = UBmrCellUtilsLibrary::GetIndexByCellOnLevel(Cell);
	UGlobalMessageSubsystem::BroadcastGlobalMessage(EventData, Pawn);
	return true;
}

bool FindBoxAndAdjacentEmptyCell(FBmrCell& OutBoxCell, FBmrCell& OutBombCell)
{
	const FBmrCells BoxCells = UBmrCellUtilsLibrary::GetAllCellsWithActors(TO_FLAG(EAT::Box));
	for (const FBmrCell& BoxCell : BoxCells)
	{
		const FBmrCells EmptyNeighbors = UBmrCellUtilsLibrary::GetEmptyCellsAroundWithoutActors(BoxCell, EPathType::Explosion, 1);
		if (!EmptyNeighbors.IsEmpty())
		{
			OutBoxCell = BoxCell;
			OutBombCell = *EmptyNeighbors.CreateConstIterator();
			return true;
		}
	}
	return false;
}

bool PrepareGameplay(FBaseState& State)
{
	UWorld* World = GetPIEWorld();
	ABmrGeneratedMap* GeneratedMap = World ? ABmrGeneratedMap::GetGeneratedMap(World) : nullptr;
	ABmrPawn* Pawn = FindReadyPawn(World);
	if (!World || !GeneratedMap || !Pawn || UBmrCellUtilsLibrary::GetAllCellsOnLevel().IsEmpty())
	{
		return false;
	}

	DisableAIBombPlacement(World);
	if (!SetInGameState(World))
	{
		return false;
	}

	State.Pawn = Pawn;
	State.bSetupComplete = true;
	return true;
}

void SpawnLoadBombs(const TSharedRef<FLoadState>& State)
{
	ABmrPawn* Pawn = State->Pawn.Get();
	UAbilitySystemComponent* ASC = Pawn ? Pawn->GetAbilitySystemComponent() : nullptr;
	ABmrGeneratedMap* GeneratedMap = ABmrGeneratedMap::GetGeneratedMap(Pawn);
	if (!ASC || !GeneratedMap)
	{
		return;
	}

	for (const FBmrCell& Cell : State->LoadCells)
	{
		GeneratedMap->SpawnActorByType(EAT::Bomb, Cell, [State, WeakASC = TWeakObjectPtr<UAbilitySystemComponent>(ASC)](UBmrMapComponent& MapComponent)
		{
			if (ABmrBombAbilityActor* Bomb = Cast<ABmrBombAbilityActor>(MapComponent.GetOwner()))
			{
				Bomb->InitBomb(WeakASC.Get());
				++State->SpawnedBombCount;
			}
		});
	}
}

bool SelectLoadCells(FLoadState& State)
{
	const FBmrCells EmptyCells = UBmrCellUtilsLibrary::GetAllEmptyCellsWithoutActors();
	for (const FBmrCell& Cell : EmptyCells)
	{
		State.LoadCells.Add(Cell);
		if (State.LoadCells.Num() >= LoadBombLimit)
		{
			break;
		}
	}

	State.RequestedBombCount = State.LoadCells.Num();
	return State.RequestedBombCount > 0;
}
} // namespace BmrBombSystemTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBmrGameStartupSystemTest,
	"Bomber.System.Game.StartupAndMapReady",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBmrGameStartupSystemTest::RunTest(const FString& Parameters)
{
	using namespace BmrBombSystemTest;

	if (!AutomationOpenMap(TEXT("/Game/Bomber/Maps/Main")))
	{
		AddError(TEXT("SYS-GAME-001: Failed to open /Game/Bomber/Maps/Main."));
		return false;
	}

	const TSharedRef<FBaseState> State = MakeShared<FBaseState>();
	State->Deadline = FPlatformTime::Seconds() + SetupTimeoutSeconds;

	AddCommand(new FStartPIECommand(false));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (PrepareGameplay(*State))
		{
			TestTrue(TEXT("SYS-GAME-001: Generated map contains playable cells"), !UBmrCellUtilsLibrary::GetAllCellsOnLevel().IsEmpty());
			TestTrue(TEXT("SYS-GAME-001: At least one player is ready"), CountActors(TO_FLAG(EAT::Player)) > 0);
			TestNotNull(TEXT("SYS-GAME-001: Generated map is available"), ABmrGeneratedMap::GetGeneratedMap(GetPIEWorld()));
			return true;
		}

		if (FPlatformTime::Seconds() >= State->Deadline)
		{
			AddError(TEXT("SYS-GAME-001: Timed out waiting for the game, generated map, and a playable pawn."));
			return true;
		}
		return false;
	}));
	AddCommand(new FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBmrBombLifecycleSystemTest,
	"Bomber.System.Bomb.FullLifecycle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBmrBombLifecycleSystemTest::RunTest(const FString& Parameters)
{
	using namespace BmrBombSystemTest;

	if (!AutomationOpenMap(TEXT("/Game/Bomber/Maps/Main")))
	{
		AddError(TEXT("SYS-BOMB-001: Failed to open /Game/Bomber/Maps/Main."));
		return false;
	}

	const TSharedRef<FBombLifecycleState> State = MakeShared<FBombLifecycleState>();
	State->Deadline = FPlatformTime::Seconds() + SetupTimeoutSeconds;

	AddCommand(new FStartPIECommand(false));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (PrepareGameplay(*State))
		{
			UBmrMapComponent* PawnMapComponent = UBmrMapComponent::GetMapComponent(State->Pawn.Get());
			State->TargetCell = PawnMapComponent ? PawnMapComponent->GetCell() : FBmrCell::InvalidCell;
			if (!State->TargetCell.IsValid()
			    || UBmrCellUtilsLibrary::IsCellHasAnyMatchingActor(State->TargetCell, TO_FLAG(~EAT::Player)))
			{
				AddError(TEXT("SYS-BOMB-001: The ready pawn is not standing on a valid empty placement cell."));
				return true;
			}

			State->Pawn->SpawnBomb();
			State->Deadline = FPlatformTime::Seconds() + ActionTimeoutSeconds;
			return true;
		}

		if (FPlatformTime::Seconds() >= State->Deadline)
		{
			AddError(TEXT("SYS-BOMB-001: Timed out preparing the gameplay state."));
			return true;
		}
		return false;
	}));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (!State->bSetupComplete || !State->TargetCell.IsValid())
		{
			return true;
		}

		if (CountActorsAtCell(State->TargetCell, TO_FLAG(EAT::Bomb)) == 1)
		{
			State->bBombObserved = true;
			State->ExplosionDeadline = FPlatformTime::Seconds() + UBmrBombDataAsset::Get().GetDurationTime() + ActionTimeoutSeconds;
			TestTrue(TEXT("SYS-BOMB-001: Bomb is created on the player's empty cell"), true);
			return true;
		}

		if (FPlatformTime::Seconds() >= State->Deadline)
		{
			AddError(TEXT("SYS-BOMB-001: Timed out waiting for the placed bomb."));
			return true;
		}
		return false;
	}));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (!State->bBombObserved)
		{
			return true;
		}

		if (CountActorsAtCell(State->TargetCell, TO_FLAG(EAT::Bomb)) == 0)
		{
			TestFalse(
				TEXT("SYS-BOMB-001: Bomb is removed and the cell is reusable after detonation"),
				UBmrCellUtilsLibrary::IsCellHasAnyMatchingActor(State->TargetCell, TO_FLAG(EAT::Bomb)));
			return true;
		}

		if (FPlatformTime::Seconds() >= State->ExplosionDeadline)
		{
			AddError(TEXT("SYS-BOMB-001: Bomb did not finish its lifecycle before the timeout."));
			return true;
		}
		return false;
	}));
	AddCommand(new FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBmrBoxDestructionSystemTest,
	"Bomber.System.World.DestroyBoxByExplosion",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBmrBoxDestructionSystemTest::RunTest(const FString& Parameters)
{
	using namespace BmrBombSystemTest;

	if (!AutomationOpenMap(TEXT("/Game/Bomber/Maps/Main")))
	{
		AddError(TEXT("SYS-WORLD-001: Failed to open /Game/Bomber/Maps/Main."));
		return false;
	}

	const TSharedRef<FBoxDestructionState> State = MakeShared<FBoxDestructionState>();
	State->Deadline = FPlatformTime::Seconds() + SetupTimeoutSeconds;

	AddCommand(new FStartPIECommand(false));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (PrepareGameplay(*State) && FindBoxAndAdjacentEmptyCell(State->BoxCell, State->TargetCell))
		{
			if (!RequestBombAtCell(State->Pawn.Get(), State->TargetCell))
			{
				AddError(TEXT("SYS-WORLD-001: Failed to request bomb placement next to a box."));
				return true;
			}
			State->Deadline = FPlatformTime::Seconds() + ActionTimeoutSeconds;
			return true;
		}

		if (FPlatformTime::Seconds() >= State->Deadline)
		{
			AddError(TEXT("SYS-WORLD-001: Timed out finding a box with an adjacent empty cell."));
			return true;
		}
		return false;
	}));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (!State->bSetupComplete || !State->TargetCell.IsValid())
		{
			return true;
		}

		if (CountActorsAtCell(State->TargetCell, TO_FLAG(EAT::Bomb)) == 1)
		{
			State->bBombObserved = true;
			State->ExplosionDeadline = FPlatformTime::Seconds() + UBmrBombDataAsset::Get().GetDurationTime() + ActionTimeoutSeconds;
			return true;
		}

		if (FPlatformTime::Seconds() >= State->Deadline)
		{
			AddError(TEXT("SYS-WORLD-001: Timed out waiting for the bomb next to the box."));
			return true;
		}
		return false;
	}));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (!State->bBombObserved)
		{
			return true;
		}

		if (CountActorsAtCell(State->BoxCell, TO_FLAG(EAT::Box)) == 0)
		{
			TestEqual(TEXT("SYS-WORLD-001: Explosion removes the adjacent destructible box"), CountActorsAtCell(State->BoxCell, TO_FLAG(EAT::Box)), 0);
			return true;
		}

		if (FPlatformTime::Seconds() >= State->ExplosionDeadline)
		{
			AddError(TEXT("SYS-WORLD-001: Adjacent box remained after the bomb explosion timeout."));
			return true;
		}
		return false;
	}));
	AddCommand(new FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBmrBombLoadSystemTest,
	"Bomber.System.Load.MassBombSpawnAndCleanup",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBmrBombLoadSystemTest::RunTest(const FString& Parameters)
{
	using namespace BmrBombSystemTest;

	if (!AutomationOpenMap(TEXT("/Game/Bomber/Maps/Main")))
	{
		AddError(TEXT("SYS-LOAD-001: Failed to open /Game/Bomber/Maps/Main."));
		return false;
	}

	const TSharedRef<FLoadState> State = MakeShared<FLoadState>();
	State->Deadline = FPlatformTime::Seconds() + SetupTimeoutSeconds;

	AddCommand(new FStartPIECommand(false));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (PrepareGameplay(*State))
		{
			if (!SelectLoadCells(*State))
			{
				AddError(TEXT("SYS-LOAD-001: No empty cells are available for the load test."));
				return true;
			}
			SpawnLoadBombs(State);
			State->Deadline = FPlatformTime::Seconds() + ActionTimeoutSeconds;
			return true;
		}

		if (FPlatformTime::Seconds() >= State->Deadline)
		{
			AddError(TEXT("SYS-LOAD-001: Timed out preparing the gameplay state."));
			return true;
		}
		return false;
	}));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (!State->bSetupComplete || State->RequestedBombCount == 0)
		{
			return true;
		}

		const int32 LiveBombCount = CountBombsOnCells(State->LoadCells);
		if (State->SpawnedBombCount == State->RequestedBombCount && LiveBombCount == State->RequestedBombCount)
		{
			TestEqual(TEXT("SYS-LOAD-001: All requested load-test bombs are registered"), LiveBombCount, State->RequestedBombCount);
			if (ABmrGeneratedMap* GeneratedMap = ABmrGeneratedMap::GetGeneratedMap(State->Pawn.Get()))
			{
				GeneratedMap->DestroyLevelActorsOnCells(State->LoadCells, State->Pawn.Get());
				State->bCleanupRequested = true;
				State->Deadline = FPlatformTime::Seconds() + ActionTimeoutSeconds;
			}
			return true;
		}

		if (FPlatformTime::Seconds() >= State->Deadline)
		{
			AddError(FString::Printf(
				TEXT("SYS-LOAD-001: Timed out spawning load-test bombs. Requested=%d, callbacks=%d, live=%d."),
				State->RequestedBombCount,
				State->SpawnedBombCount,
				LiveBombCount));
			return true;
		}
		return false;
	}));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (!State->bCleanupRequested)
		{
			return true;
		}

		const int32 RemainingBombCount = CountBombsOnCells(State->LoadCells);
		if (RemainingBombCount == 0)
		{
			TestEqual(TEXT("SYS-LOAD-001: Every load-test bomb is removed without a crash"), RemainingBombCount, 0);
			AddInfo(FString::Printf(TEXT("SYS-LOAD-001: Spawned and cleaned up %d bombs."), State->RequestedBombCount));
			return true;
		}

		if (FPlatformTime::Seconds() >= State->Deadline)
		{
			AddError(FString::Printf(TEXT("SYS-LOAD-001: Cleanup timed out with %d bomb(s) remaining."), RemainingBombCount));
			return true;
		}
		return false;
	}));
	AddCommand(new FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBmrPostLoadRecoverySystemTest,
	"Bomber.System.Load.PostLoadRecovery",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBmrPostLoadRecoverySystemTest::RunTest(const FString& Parameters)
{
	using namespace BmrBombSystemTest;

	if (!AutomationOpenMap(TEXT("/Game/Bomber/Maps/Main")))
	{
		AddError(TEXT("SYS-RECOVERY-001: Failed to open /Game/Bomber/Maps/Main."));
		return false;
	}

	const TSharedRef<FLoadState> State = MakeShared<FLoadState>();
	State->Deadline = FPlatformTime::Seconds() + SetupTimeoutSeconds;

	AddCommand(new FStartPIECommand(false));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (PrepareGameplay(*State))
		{
			if (!SelectLoadCells(*State))
			{
				AddError(TEXT("SYS-RECOVERY-001: No empty cells are available for the load phase."));
				return true;
			}
			SpawnLoadBombs(State);
			State->Deadline = FPlatformTime::Seconds() + ActionTimeoutSeconds;
			return true;
		}

		if (FPlatformTime::Seconds() >= State->Deadline)
		{
			AddError(TEXT("SYS-RECOVERY-001: Timed out preparing the gameplay state."));
			return true;
		}
		return false;
	}));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (!State->bSetupComplete || State->RequestedBombCount == 0)
		{
			return true;
		}

		if (State->SpawnedBombCount == State->RequestedBombCount
		    && CountBombsOnCells(State->LoadCells) == State->RequestedBombCount)
		{
			if (ABmrGeneratedMap* GeneratedMap = ABmrGeneratedMap::GetGeneratedMap(State->Pawn.Get()))
			{
				GeneratedMap->DestroyLevelActorsOnCells(State->LoadCells, State->Pawn.Get());
				State->bCleanupRequested = true;
				State->Deadline = FPlatformTime::Seconds() + ActionTimeoutSeconds;
			}
			return true;
		}

		if (FPlatformTime::Seconds() >= State->Deadline)
		{
			AddError(TEXT("SYS-RECOVERY-001: Load phase did not finish before the timeout."));
			return true;
		}
		return false;
	}));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (!State->bCleanupRequested)
		{
			return true;
		}

		if (CountBombsOnCells(State->LoadCells) == 0)
		{
			UBmrMapComponent* PawnMapComponent = UBmrMapComponent::GetMapComponent(State->Pawn.Get());
			State->TargetCell = PawnMapComponent ? PawnMapComponent->GetCell() : FBmrCell::InvalidCell;
			if (!State->TargetCell.IsValid()
			    || UBmrCellUtilsLibrary::IsCellHasAnyMatchingActor(State->TargetCell, TO_FLAG(~EAT::Player)))
			{
				AddError(TEXT("SYS-RECOVERY-001: Player cell is unavailable after load cleanup."));
				return true;
			}

			State->Pawn->SpawnBomb();
			State->bNormalPlacementRequested = true;
			State->Deadline = FPlatformTime::Seconds() + ActionTimeoutSeconds;
			return true;
		}

		if (FPlatformTime::Seconds() >= State->Deadline)
		{
			AddError(TEXT("SYS-RECOVERY-001: Load cleanup did not finish before the timeout."));
			return true;
		}
		return false;
	}));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (!State->bNormalPlacementRequested)
		{
			return true;
		}

		if (CountActorsAtCell(State->TargetCell, TO_FLAG(EAT::Bomb)) == 1)
		{
			TestEqual(
				TEXT("SYS-RECOVERY-001: A normal bomb can be placed after the load phase"),
				CountActorsAtCell(State->TargetCell, TO_FLAG(EAT::Bomb)),
				1);
			return true;
		}

		if (FPlatformTime::Seconds() >= State->Deadline)
		{
			AddError(TEXT("SYS-RECOVERY-001: Normal bomb placement failed after the load phase."));
			return true;
		}
		return false;
	}));
	AddCommand(new FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBmrJiraIntentionalFailureSystemTest,
	"Bomber.SystemDemo.Jira.IntentionalFailure",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBmrJiraIntentionalFailureSystemTest::RunTest(const FString& Parameters)
{
	AddError(TEXT("[INTENTIONAL FAILURE] SYS-JIRA-FAIL-001: Jira automatic issue creation pipeline verification."));
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

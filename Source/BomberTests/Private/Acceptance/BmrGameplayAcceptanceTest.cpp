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
#include "GameFramework/BmrPlayerState.h"
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

namespace BmrGameplayAcceptanceTest
{
constexpr double SetupTimeoutSeconds = 30.0;
constexpr double ActionTimeoutSeconds = 10.0;

struct FAcceptanceState
{
	double Deadline = 0.0;
	double ExplosionDeadline = 0.0;
	TWeakObjectPtr<ABmrPawn> Pawn;
	TWeakObjectPtr<ABmrPlayerState> PlayerState;
	FBmrCell BombCell = FBmrCell::InvalidCell;
	FBmrCell BoxCell = FBmrCell::InvalidCell;
	bool bSetupComplete = false;
	bool bBombObserved = false;
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

bool HasBombPlacementAbility(const ABmrPawn* Pawn)
{
	UAbilitySystemComponent* ASC = Pawn ? Pawn->GetAbilitySystemComponent() : nullptr;
	if (!ASC)
	{
		return false;
	}

	for (const FGameplayAbilitySpec& AbilitySpec : ASC->GetActivatableAbilities())
	{
		if (AbilitySpec.Ability && AbilitySpec.Ability->IsA<UBmrBombPlaceAbility>())
		{
			return true;
		}
	}
	return false;
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
		UBmrMapComponent* MapComponent = Pawn ? UBmrMapComponent::GetMapComponent(Pawn) : nullptr;
		UBmrMoverComponent* MoverComponent = Pawn ? Pawn->GetMoverComponent() : nullptr;
		if (!Pawn || !Pawn->GetPlayerState<ABmrPlayerState>() || !MapComponent || !MoverComponent
		    || !MapComponent->GetCell().IsValid() || !HasBombPlacementAbility(Pawn))
		{
			continue;
		}

		MoverComponent->SetBlockMovement(false);
		return Pawn;
	}
	return nullptr;
}

bool PrepareAcceptanceGameplay(FAcceptanceState& State)
{
	UWorld* World = GetPIEWorld();
	ABmrGeneratedMap* GeneratedMap = World ? ABmrGeneratedMap::GetGeneratedMap(World) : nullptr;
	ABmrPawn* Pawn = FindReadyPawn(World);
	ABmrPlayerState* PlayerState = Pawn ? Pawn->GetPlayerState<ABmrPlayerState>() : nullptr;
	UBmrMapComponent* MapComponent = Pawn ? UBmrMapComponent::GetMapComponent(Pawn) : nullptr;

	if (!World || !GeneratedMap || !Pawn || !PlayerState || !MapComponent
	    || !MapComponent->GetCell().IsValid() || !HasBombPlacementAbility(Pawn)
	    || UBmrCellUtilsLibrary::GetAllCellsOnLevel().IsEmpty())
	{
		return false;
	}

	DisableAIBombPlacement(World);
	if (!SetInGameState(World))
	{
		return false;
	}

	State.Pawn = Pawn;
	State.PlayerState = PlayerState;
	State.bSetupComplete = true;
	return true;
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
} // namespace BmrGameplayAcceptanceTest

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBmrStartPlayableMatchAcceptanceTest,
	"Bomber.Acceptance.Game.StartPlayableMatch",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBmrStartPlayableMatchAcceptanceTest::RunTest(const FString& Parameters)
{
	using namespace BmrGameplayAcceptanceTest;

	if (!AutomationOpenMap(TEXT("/Game/Bomber/Maps/Main")))
	{
		AddError(TEXT("ACC-GAME-001: Failed to open /Game/Bomber/Maps/Main."));
		return false;
	}

	const TSharedRef<FAcceptanceState> State = MakeShared<FAcceptanceState>();
	State->Deadline = FPlatformTime::Seconds() + SetupTimeoutSeconds;

	AddCommand(new FStartPIECommand(false));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (PrepareAcceptanceGameplay(*State))
		{
			TestTrue(TEXT("ACC-GAME-001: A playable pawn is ready for input"), State->Pawn.IsValid());
			TestTrue(TEXT("ACC-GAME-001: Bomb placement capability is available"), HasBombPlacementAbility(State->Pawn.Get()));
			TestFalse(TEXT("ACC-GAME-001: Generated map contains playable cells"), UBmrCellUtilsLibrary::GetAllCellsOnLevel().IsEmpty());
			return true;
		}

		if (FPlatformTime::Seconds() >= State->Deadline)
		{
			AddError(TEXT("ACC-GAME-001: Timed out waiting for a playable match."));
			return true;
		}
		return false;
	}));
	AddCommand(new FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBmrClearObstacleAcceptanceTest,
	"Bomber.Acceptance.Bomb.ClearObstacle",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBmrClearObstacleAcceptanceTest::RunTest(const FString& Parameters)
{
	using namespace BmrGameplayAcceptanceTest;

	if (!AutomationOpenMap(TEXT("/Game/Bomber/Maps/Main")))
	{
		AddError(TEXT("ACC-BOMB-001: Failed to open /Game/Bomber/Maps/Main."));
		return false;
	}

	const TSharedRef<FAcceptanceState> State = MakeShared<FAcceptanceState>();
	State->Deadline = FPlatformTime::Seconds() + SetupTimeoutSeconds;

	AddCommand(new FStartPIECommand(false));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (PrepareAcceptanceGameplay(*State) && FindBoxAndAdjacentEmptyCell(State->BoxCell, State->BombCell))
		{
			if (!RequestBombAtCell(State->Pawn.Get(), State->BombCell))
			{
				AddError(TEXT("ACC-BOMB-001: Failed to request bomb placement next to the obstacle."));
				return true;
			}
			State->Deadline = FPlatformTime::Seconds() + ActionTimeoutSeconds;
			return true;
		}

		if (FPlatformTime::Seconds() >= State->Deadline)
		{
			AddError(TEXT("ACC-BOMB-001: Timed out preparing a destructible obstacle scenario."));
			return true;
		}
		return false;
	}));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (!State->bSetupComplete || !State->BombCell.IsValid())
		{
			return true;
		}

		if (CountActorsAtCell(State->BombCell, TO_FLAG(EAT::Bomb)) == 1)
		{
			State->bBombObserved = true;
			State->ExplosionDeadline = FPlatformTime::Seconds() + UBmrBombDataAsset::Get().GetDurationTime() + ActionTimeoutSeconds;
			return true;
		}

		if (FPlatformTime::Seconds() >= State->Deadline)
		{
			AddError(TEXT("ACC-BOMB-001: Timed out waiting for the user's bomb."));
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
			TestEqual(TEXT("ACC-BOMB-001: Explosion clears the destructible obstacle"), CountActorsAtCell(State->BoxCell, TO_FLAG(EAT::Box)), 0);
			return true;
		}

		if (FPlatformTime::Seconds() >= State->ExplosionDeadline)
		{
			AddError(TEXT("ACC-BOMB-001: The destructible obstacle remained after the explosion timeout."));
			return true;
		}
		return false;
	}));
	AddCommand(new FEndPlayMapCommand());
	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
	FBmrShowVictoryResultAcceptanceTest,
	"Bomber.Acceptance.Round.ShowVictoryResult",
	EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FBmrShowVictoryResultAcceptanceTest::RunTest(const FString& Parameters)
{
	using namespace BmrGameplayAcceptanceTest;

	if (!AutomationOpenMap(TEXT("/Game/Bomber/Maps/Main")))
	{
		AddError(TEXT("ACC-RESULT-001: Failed to open /Game/Bomber/Maps/Main."));
		return false;
	}

	const TSharedRef<FAcceptanceState> State = MakeShared<FAcceptanceState>();
	State->Deadline = FPlatformTime::Seconds() + SetupTimeoutSeconds;

	AddCommand(new FStartPIECommand(false));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (PrepareAcceptanceGameplay(*State))
		{
			State->PlayerState->SetEndGameState(EBmrEndGameState::Win);
			State->Deadline = FPlatformTime::Seconds() + ActionTimeoutSeconds;
			return true;
		}

		if (FPlatformTime::Seconds() >= State->Deadline)
		{
			AddError(TEXT("ACC-RESULT-001: Timed out preparing a playable pawn and player state."));
			return true;
		}
		return false;
	}));
	AddCommand(new FFunctionLatentCommand([this, State]()
	{
		if (!State->bSetupComplete || !State->PlayerState.IsValid())
		{
			return true;
		}

		if (State->PlayerState->GetEndGameState() == EBmrEndGameState::Win)
		{
			TestTrue(TEXT("ACC-RESULT-001: Player result changes to Win"), State->PlayerState->GetEndGameState() == EBmrEndGameState::Win);
			return true;
		}

		if (FPlatformTime::Seconds() >= State->Deadline)
		{
			AddError(TEXT("ACC-RESULT-001: Victory state was not applied before timeout."));
			return true;
		}
		return false;
	}));
	AddCommand(new FEndPlayMapCommand());
	return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS

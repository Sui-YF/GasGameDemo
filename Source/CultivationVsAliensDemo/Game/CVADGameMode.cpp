#include "Game/CVADGameMode.h"
#include "Character/CVADCharacter.h"
#include "Player/CVADPlayerState.h"
#include "Player/CVADPlayerController.h"
#include "Battle/CVADBattleDirector.h"
#include "EngineUtils.h"

ACVADGameMode::ACVADGameMode()
{
    DefaultPawnClass = ACVADCharacter::StaticClass();
    PlayerStateClass = ACVADPlayerState::StaticClass();
    PlayerControllerClass = ACVADPlayerController::StaticClass();
}

void ACVADGameMode::BeginPlay()
{
    Super::BeginPlay();
    if (GetWorld() && GetWorld()->GetMapName().Contains(TEXT("L_MainMenu"))) return;
    for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It)
    {
        It->StartBattle();
        UE_LOG(LogTemp, Log, TEXT("GameMode automatically started battle via %s"), *GetNameSafe(*It));
        break;
    }
}

UClass* ACVADGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    if (GetWorld() && GetWorld()->GetMapName().Contains(TEXT("L_MainMenu"))) return nullptr;
    return Super::GetDefaultPawnClassForController_Implementation(InController);
}

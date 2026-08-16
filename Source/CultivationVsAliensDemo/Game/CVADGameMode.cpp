#include "Game/CVADGameMode.h"
#include "Character/CVADCharacter.h"
#include "Player/CVADPlayerState.h"
#include "Player/CVADPlayerController.h"
#include "Battle/CVADBattleDirector.h"
#include "EngineUtils.h"
#include "GameFramework/GameStateBase.h"
#include "Kismet/GameplayStatics.h"
#include "GenericPlatform/GenericPlatformHttp.h"

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

void ACVADGameMode::PreLogin(const FString& Options,const FString& Address,const FUniqueNetIdRepl& UniqueId,FString& ErrorMessage)
{
    Super::PreLogin(Options,Address,UniqueId,ErrorMessage);
    if(!ErrorMessage.IsEmpty() || !GetWorld() || !GetWorld()->URL.HasOption(TEXT("Lobby"))) return;
    const AGameStateBase* GS=GetWorld()->GetGameState();
    if(GS && GS->PlayerArray.Num()>=2) ErrorMessage=TEXT("LobbyFull");
}

FString ACVADGameMode::InitNewPlayer(APlayerController* NewPlayerController,const FUniqueNetIdRepl& UniqueId,
    const FString& Options,const FString& Portal)
{
    const FString Error=Super::InitNewPlayer(NewPlayerController,UniqueId,Options,Portal);
    if(!Error.IsEmpty() || !NewPlayerController) return Error;
    FString RequestedName=FGenericPlatformHttp::UrlDecode(UGameplayStatics::ParseOption(Options,TEXT("PlayerName"))).TrimStartAndEnd().Left(20);
    bool bValid=RequestedName.Len()>=1;
    for(const TCHAR C : RequestedName) if(C<32 || C==TEXT('/') || C==TEXT('\\')) { bValid=false; break; }
    if(!bValid) RequestedName=FString::Printf(TEXT("Player%d"),GetWorld()->GetGameState()?GetWorld()->GetGameState()->PlayerArray.Num():1);
    NewPlayerController->ServerChangeName(RequestedName);
    UE_LOG(LogTemp,Log,TEXT("Initialized network player Name=%s Address=%s"),*RequestedName,*NewPlayerController->GetName());
    return Error;
}

UClass* ACVADGameMode::GetDefaultPawnClassForController_Implementation(AController* InController)
{
    if (GetWorld() && GetWorld()->GetMapName().Contains(TEXT("L_MainMenu"))) return nullptr;
    return Super::GetDefaultPawnClassForController_Implementation(InController);
}

#pragma once

#include "GameFramework/GameModeBase.h"
#include "CVADGameMode.generated.h"

UCLASS()
class CULTIVATIONVSALIENSDEMO_API ACVADGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ACVADGameMode();
    virtual void BeginPlay() override;
    virtual void PreLogin(const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage) override;
    virtual FString InitNewPlayer(APlayerController* NewPlayerController, const FUniqueNetIdRepl& UniqueId,
        const FString& Options, const FString& Portal = TEXT("")) override;
    virtual UClass* GetDefaultPawnClassForController_Implementation(AController* InController) override;
};

#pragma once

#include "GameFramework/SaveGame.h"
#include "Inventory/CVADInventoryTypes.h"
#include "CVADSaveGame.generated.h"

UCLASS()
class CULTIVATIONVSALIENSDEMO_API UCVADSaveGame : public USaveGame
{
    GENERATED_BODY()

public:
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Profile")
    int32 SaveVersion = 1;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Profile")
    FString PlayerDisplayName = TEXT("Player");

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Progress")
    bool bHasCompletedDemo = false;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Progress")
    float BestCompletionTimeSeconds = 0.f;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Progress")
    int32 BestTeamDetonationCount = 0;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Inventory")
    TArray<FName> UnlockedItemIds;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Inventory")
    FCVADEquipmentLoadout EquipmentLoadout;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Skills")
    TArray<FName> EquippedSkillRows;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Skills") TArray<FName> UnlockedSkillRows;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Progress") int32 PlayerLevel = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Progress") int32 Experience = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, SaveGame, Category="Progress") int32 SkillPoints = 0;
};

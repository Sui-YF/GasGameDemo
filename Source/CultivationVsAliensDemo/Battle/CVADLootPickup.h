#pragma once

#include "GameFramework/Actor.h"
#include "CVADLootPickup.generated.h"

class USphereComponent;
class UStaticMeshComponent;
class UPointLightComponent;

/**
 * Boss loot that drops at the death location. Walking over it grants the
 * collecting player experience and skill points, then the pickup disappears.
 * Fully C++ so it works without any new Blueprint assets.
 */
UCLASS(Blueprintable)
class CULTIVATIONVSALIENSDEMO_API ACVADLootPickup : public AActor
{
    GENERATED_BODY()

public:
    ACVADLootPickup();

    UFUNCTION(BlueprintCallable, Category="Loot")
    void InitializeLoot(int32 InExperience, int32 InSkillPoints, const FString& InDisplayName);

protected:
    virtual void BeginPlay() override;
    virtual void Tick(float DeltaSeconds) override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Loot")
    TObjectPtr<USphereComponent> PickupSphere;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Loot")
    TObjectPtr<UStaticMeshComponent> PickupMesh;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Loot")
    TObjectPtr<UPointLightComponent> PickupLight;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot")
    int32 ExperienceReward = 500;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot")
    int32 SkillPointReward = 1;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot")
    FString LootDisplayName = TEXT("天穹三使的传承");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta=(ClampMin="0.0"))
    float BobSpeed = 2.2f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta=(ClampMin="0.0"))
    float BobHeight = 14.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Loot", meta=(ClampMin="0.1"))
    float LifetimeSeconds = 30.f;

private:
    void CheckForCollectors();
    void Collect(class ACVADCharacter* Player);
    FVector InitialLocation = FVector::ZeroVector;
    bool bCollected = false;
};

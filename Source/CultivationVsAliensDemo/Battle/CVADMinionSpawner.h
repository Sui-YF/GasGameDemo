#pragma once

#include "GameFramework/Actor.h"
#include "CVADMinionSpawner.generated.h"

class UBoxComponent;
class ACVADEnemyCharacter;

UCLASS(Blueprintable)
class CULTIVATIONVSALIENSDEMO_API ACVADMinionSpawner : public AActor
{
    GENERATED_BODY()

public:
    ACVADMinionSpawner();
    virtual void BeginPlay() override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    void NotifySpawnedMinionDefeated(ACVADEnemyCharacter* DefeatedMinion);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Spawner") void StartSpawning();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Spawner") void StopSpawning();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Spawner") void ResetSpawner();
    UFUNCTION(Exec, Category="Spawner|Debug") void DebugSpawnBoss();

    UPROPERTY(BlueprintReadOnly, Replicated, Category="Spawner") int32 AliveCount = 0;
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Spawner") int32 DefeatedCount = 0;
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_Completed, Category="Spawner") bool bCompleted = false;
    UFUNCTION(BlueprintPure, Category="Spawner") int32 GetKillQuota() const { return KillQuota; }

protected:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawner|Balance") FName ProfileRowName = TEXT("Frontline");
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Spawner") TObjectPtr<UBoxComponent> ActivationBox;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawner") TSubclassOf<ACVADEnemyCharacter> MinionClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawner|Boss") TSubclassOf<ACVADEnemyCharacter> BossClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawner|Boss") FVector BossSpawnOffset = FVector(900.f, 0.f, 100.f);
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawner", meta=(ClampMin="0.1")) float SpawnInterval = 1.25f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawner", meta=(ClampMin="1")) int32 MaxAlive = 12;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawner", meta=(ClampMin="0")) int32 KillQuota = 30;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawner") bool bRequirePlayerInside = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawner") bool bStartActive = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Spawner") bool bResumeWhenPlayerReturns = true;

    UFUNCTION() void HandleBeginOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
    UFUNCTION() void HandleEndOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex);
    UFUNCTION() void OnRep_Completed();
    UFUNCTION(BlueprintImplementableEvent, Category="Spawner") void OnSpawnerCompleted();

private:
    TSet<TWeakObjectPtr<AActor>> PlayersInside;
    TSet<TWeakObjectPtr<ACVADEnemyCharacter>> SpawnedMinions;
    FTimerHandle SpawnTimer;

    void TrySpawnMinion();
    void RefreshSpawningState();
    FVector FindSpawnLocation() const;
    void PruneInvalidEntries();
    void ApplyProfile();
    void ScanPlayersAndRefresh();
    void CompleteSpawner();
};

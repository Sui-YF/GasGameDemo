#pragma once

#include "GameFramework/Actor.h"
#include "Battle/CVADBattleTypes.h"
#include "CVADBattleDirector.generated.h"

DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCVADBattlePhaseChanged, ECVADBattlePhase, PreviousPhase, ECVADBattlePhase, NewPhase);
DECLARE_DYNAMIC_MULTICAST_DELEGATE_TwoParams(FCVADDefeatCountChanged, int32, CurrentCount, int32, TargetCount);

UCLASS(Blueprintable)
class CULTIVATIONVSALIENSDEMO_API ACVADBattleDirector : public AActor
{
    GENERATED_BODY()

public:
    ACVADBattleDirector();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Battle")
    void StartBattle();

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Battle")
    void RegisterMinionDefeated(int32 Amount = 1);

    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Battle")
    void AdvancePhase();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Battle") void RegisterBoss(class ACVADEnemyCharacter* Boss);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Battle") void UpdateBossHealth(float Current, float Maximum);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Battle") void CompleteBossBattle(class ACVADEnemyCharacter* DefeatedBoss);
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Battle") void RegisterPlayerDown();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Battle") void RegisterPlayerDownTimeout();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Battle") void RegisterPlayerRevived();
    UFUNCTION(BlueprintCallable, BlueprintAuthorityOnly, Category="Battle") void RegisterExperienceReward(int32 Amount);
    UFUNCTION(BlueprintPure, Category="Battle") bool IsBossStageReady() const { return BattlePhase == ECVADBattlePhase::Boss; }

    UPROPERTY(BlueprintAssignable, Category="Battle")
    FCVADBattlePhaseChanged OnBattlePhaseChanged;

    UPROPERTY(BlueprintAssignable, Category="Battle")
    FCVADDefeatCountChanged OnDefeatCountChanged;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_BattlePhase, Category="Battle")
    ECVADBattlePhase BattlePhase = ECVADBattlePhase::Rally;

    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_DefeatCount, Category="Battle")
    int32 DefeatCount = 0;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Battle", meta=(ClampMin="1"))
    int32 FrontlineDefeatTarget = 60;
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Battle|Boss") float BossHealth = 0.f;
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Battle|Boss") float BossMaxHealth = 0.f;
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Battle") bool bVictory = false;
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Battle") bool bDefeat = false;
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Battle") int32 DownedPlayerCount = 0;
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Battle|Results") float CompletionTimeSeconds = 0.f;
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Battle|Results") int32 ExperienceEarned = 0;
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Battle|Boss") TObjectPtr<class ACVADEnemyCharacter> RegisteredBoss;
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Battle|Boss") TArray<TObjectPtr<class ACVADEnemyCharacter>> RegisteredBosses;
    UPROPERTY(BlueprintReadOnly, Replicated, Category="Battle|Boss") int32 BossesRemaining = 0;

protected:
    float BattleStartTimeSeconds = 0.f;
    UFUNCTION()
    void OnRep_BattlePhase(ECVADBattlePhase PreviousPhase);

    UFUNCTION()
    void OnRep_DefeatCount(int32 PreviousCount);

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

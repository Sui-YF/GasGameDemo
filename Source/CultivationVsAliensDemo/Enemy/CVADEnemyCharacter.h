#pragma once

#include "AbilitySystemInterface.h"
#include "GameFramework/Character.h"
#include "CVADEnemyCharacter.generated.h"

class UAbilitySystemComponent;
class UCVADAttributeSet;
class ACVADMinionSpawner;
struct FOnAttributeChangeData;

UCLASS(Blueprintable)
class CULTIVATIONVSALIENSDEMO_API ACVADEnemyCharacter : public ACharacter, public IAbilitySystemInterface
{
    GENERATED_BODY()

public:
    ACVADEnemyCharacter();
    virtual UAbilitySystemComponent* GetAbilitySystemComponent() const override;
    virtual void BeginPlay() override;
    void SetSpawnSource(ACVADMinionSpawner* InSpawnSource);

    UFUNCTION(BlueprintPure, Category="Boss") int32 GetBossPhase() const { return BossPhase; }
    UFUNCTION(BlueprintPure, Category="Boss") bool IsBoss() const { return bIsBoss; }
    UFUNCTION(BlueprintImplementableEvent, Category="Combat") void OnEnemyDamaged(float DamageAmount);

protected:
    /** Rendering/network distance only. The actor is never hidden or destroyed by this setting. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Optimization", meta=(ClampMin="1000.0"))
    float VisualCullDistance = 30000.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Optimization", meta=(ClampMin="1000.0"))
    float NetworkCullDistance = 50000.f;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Combat") float HitReactionImpulse = 420.f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Balance") FName BalanceRowName = TEXT("Minion");
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Boss") bool bIsBoss = false;
    UPROPERTY(BlueprintReadOnly, ReplicatedUsing=OnRep_BossPhase, Category="Boss") int32 BossPhase = 1;
    UFUNCTION() void OnRep_BossPhase();
    UFUNCTION(BlueprintImplementableEvent, Category="Boss") void OnBossPhaseChanged(int32 NewPhase);
    void EvaluateBossPhase(float CurrentHealth);
    void HandleHealthChanged(const FOnAttributeChangeData& ChangeData);
    bool bDeathHandled = false;
    TWeakObjectPtr<ACVADMinionSpawner> SpawnSource;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS") TObjectPtr<UAbilitySystemComponent> AbilitySystemComponent;
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="GAS") TObjectPtr<UCVADAttributeSet> AttributeSet;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

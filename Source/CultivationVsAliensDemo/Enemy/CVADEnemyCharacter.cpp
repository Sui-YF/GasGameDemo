#include "Enemy/CVADEnemyCharacter.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/CVADAttributeSet.h"
#include "Battle/CVADBattleDirector.h"
#include "EngineUtils.h"
#include "Enemy/CVADEnemyAIController.h"
#include "Battle/CVADMinionSpawner.h"
#include "Data/CVADBalanceRows.h"
#include "Engine/DataTable.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "Player/CVADPlayerState.h"
#include "GameFramework/PlayerController.h"
#include "Character/CVADCharacter.h"

ACVADEnemyCharacter::ACVADEnemyCharacter()
{
    bReplicates = true;
    SetReplicateMovement(true);
    AIControllerClass = ACVADEnemyAIController::StaticClass();
    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
    AbilitySystemComponent = CreateDefaultSubobject<UAbilitySystemComponent>(TEXT("AbilitySystemComponent"));
    AbilitySystemComponent->SetIsReplicated(true);
    AbilitySystemComponent->SetReplicationMode(EGameplayEffectReplicationMode::Minimal);
    AttributeSet = CreateDefaultSubobject<UCVADAttributeSet>(TEXT("AttributeSet"));
    // 300 m render range and 500 m network relevancy range. No HiddenInGame is used.
    GetMesh()->SetCullDistance(30000.f);
    GetMesh()->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    NetCullDistanceSquared = FMath::Square(50000.f);
}

UAbilitySystemComponent* ACVADEnemyCharacter::GetAbilitySystemComponent() const
{
    return AbilitySystemComponent;
}

void ACVADEnemyCharacter::SetSpawnSource(ACVADMinionSpawner* InSpawnSource)
{
    SpawnSource = InSpawnSource;
}

void ACVADEnemyCharacter::BeginPlay()
{
    Super::BeginPlay();
    bIsBoss = bIsBoss || BalanceRowName == TEXT("Boss");
    GetMesh()->SetCullDistance(VisualCullDistance);
    NetCullDistanceSquared = FMath::Square(NetworkCullDistance);
    AbilitySystemComponent->InitAbilityActorInfo(this, this);
    AbilitySystemComponent->GetGameplayAttributeValueChangeDelegate(UCVADAttributeSet::GetHealthAttribute())
        .AddUObject(this, &ThisClass::HandleHealthChanged);

    UDataTable* BalanceTable = LoadObject<UDataTable>(nullptr, TEXT("/Game/CVAD/Data/DT_EnemyBalance.DT_EnemyBalance"));
    if (const FCVADEnemyBalanceRow* Row = BalanceTable ? BalanceTable->FindRow<FCVADEnemyBalanceRow>(BalanceRowName, TEXT("EnemyBeginPlay")) : nullptr)
    {
        AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetMaxHealthAttribute(), Row->MaxHealth);
        AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetHealthAttribute(), Row->MaxHealth);
        GetCharacterMovement()->MaxWalkSpeed = Row->MoveSpeed;
        if (ACVADEnemyAIController* EnemyController = Cast<ACVADEnemyAIController>(GetController()))
        {
            EnemyController->ConfigureCombat(Row->AttackDamage, Row->AttackInterval);
        }
    }
    UE_LOG(LogTemp, Log, TEXT("Enemy %s initialized Row=%s Boss=%s Health=%.0f"), *GetName(),
        *BalanceRowName.ToString(), bIsBoss ? TEXT("true") : TEXT("false"), AttributeSet->GetHealth());
    if (HasAuthority() && bIsBoss)
    {
        for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It)
        { It->RegisterBoss(this); It->UpdateBossHealth(AttributeSet->GetHealth(), AttributeSet->GetMaxHealth()); break; }
    }
}

void ACVADEnemyCharacter::HandleHealthChanged(const FOnAttributeChangeData& ChangeData)
{
    if (HasAuthority() && bIsBoss)
    {
        for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It)
        {
            if (!It->IsBossStageReady() && ChangeData.NewValue < ChangeData.OldValue)
            {
                AbilitySystemComponent->SetNumericAttributeBase(UCVADAttributeSet::GetHealthAttribute(), AttributeSet->GetMaxHealth());
                return;
            }
            break;
        }
    }
    if (ChangeData.NewValue < ChangeData.OldValue)
    {
        OnEnemyDamaged(ChangeData.OldValue - ChangeData.NewValue);
        if (HasAuthority() && !bIsBoss)
        {
            AActor* NearestPlayer = nullptr;
            float BestSq = TNumericLimits<float>::Max();
            for (TActorIterator<ACVADCharacter> It(GetWorld()); It; ++It)
            {
                const float Sq = FVector::DistSquared(It->GetActorLocation(), GetActorLocation());
                if (Sq < BestSq) { BestSq = Sq; NearestPlayer = *It; }
            }
            if (NearestPlayer)
            {
                const FVector Away = (GetActorLocation() - NearestPlayer->GetActorLocation()).GetSafeNormal2D();
                LaunchCharacter(Away * HitReactionImpulse + FVector(0.f, 0.f, 110.f), true, true);
            }
        }
    }
    if (HasAuthority() && bIsBoss && ChangeData.NewValue > 0.f) EvaluateBossPhase(ChangeData.NewValue);
    if (HasAuthority() && bIsBoss)
        for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It)
        { It->UpdateBossHealth(ChangeData.NewValue, AttributeSet->GetMaxHealth()); break; }
    if (!HasAuthority() || bDeathHandled || ChangeData.NewValue > 0.f) return;
    bDeathHandled = true;
    if (bIsBoss) for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It) { It->CompleteBossBattle(); break; }
    if (SpawnSource.IsValid()) SpawnSource->NotifySpawnedMinionDefeated(this);
    for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It)
    {
        It->RegisterMinionDefeated();
        break;
    }
    const int32 ExperienceReward = bIsBoss ? 500 : (BalanceRowName == TEXT("Captain") ? 60 : 25);
    for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It) { It->RegisterExperienceReward(ExperienceReward); break; }
    for (FConstPlayerControllerIterator It = GetWorld()->GetPlayerControllerIterator(); It; ++It)
    {
        if (ACVADPlayerState* RewardPlayerState = It->Get() ? It->Get()->GetPlayerState<ACVADPlayerState>() : nullptr)
            RewardPlayerState->AddExperience(ExperienceReward);
    }
    SetLifeSpan(0.05f);
}

void ACVADEnemyCharacter::EvaluateBossPhase(float CurrentHealth)
{
    const float MaxHealth = FMath::Max(1.f, AttributeSet->GetMaxHealth());
    const float Ratio = CurrentHealth / MaxHealth;
    const int32 NewPhase = Ratio <= 0.35f ? 3 : (Ratio <= 0.70f ? 2 : 1);
    if (NewPhase == BossPhase) return;
    BossPhase = NewPhase;
    if (ACVADEnemyAIController* AI = Cast<ACVADEnemyAIController>(GetController())) AI->ApplyBossPhase(BossPhase);
    OnBossPhaseChanged(BossPhase);
    ForceNetUpdate();
}

void ACVADEnemyCharacter::OnRep_BossPhase() { OnBossPhaseChanged(BossPhase); }

void ACVADEnemyCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACVADEnemyCharacter, BossPhase);
}

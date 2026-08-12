#include "Battle/CVADBattleDirector.h"
#include "Net/UnrealNetwork.h"
#include "GameFramework/GameStateBase.h"
#include "Enemy/CVADEnemyCharacter.h"
#include "EngineUtils.h"
#include "Battle/CVADMinionSpawner.h"

ACVADBattleDirector::ACVADBattleDirector()
{
    bReplicates = true;
    bAlwaysRelevant = true;
}

void ACVADBattleDirector::RegisterBoss(ACVADEnemyCharacter* Boss)
{
    if (!HasAuthority()) return;
    RegisteredBoss = Boss;
    // A boss may already be placed in the map. Registration must not skip the frontline objective.
    if (BattlePhase == ECVADBattlePhase::Boss)
    {
        bVictory = false; bDefeat = false;
    }
    ForceNetUpdate();
}

void ACVADBattleDirector::UpdateBossHealth(float Current, float Maximum)
{
    if (!HasAuthority()) return;
    BossHealth = FMath::Max(0.f, Current); BossMaxHealth = FMath::Max(1.f, Maximum); ForceNetUpdate();
}

void ACVADBattleDirector::CompleteBossBattle()
{
    if (!HasAuthority()) return;
    const ECVADBattlePhase Previous = BattlePhase;
    BattlePhase = ECVADBattlePhase::Results; bVictory = true; BossHealth = 0.f;
    CompletionTimeSeconds = FMath::Max(0.f, GetWorld()->GetTimeSeconds() - BattleStartTimeSeconds);
    ForceNetUpdate(); OnBattlePhaseChanged.Broadcast(Previous, BattlePhase);
}

void ACVADBattleDirector::RegisterPlayerDown()
{
    if (!HasAuthority() || bVictory || bDefeat) return;
    ++DownedPlayerCount;
    const int32 Players = GetWorld()->GetGameState() ? GetWorld()->GetGameState()->PlayerArray.Num() : 1;
    if (DownedPlayerCount >= FMath::Max(1, Players))
    {
        bDefeat = true; BattlePhase = ECVADBattlePhase::Results;
        CompletionTimeSeconds = FMath::Max(0.f, GetWorld()->GetTimeSeconds() - BattleStartTimeSeconds);
    }
    ForceNetUpdate();
}

void ACVADBattleDirector::RegisterPlayerRevived()
{
    if (!HasAuthority()) return;
    DownedPlayerCount = FMath::Max(0, DownedPlayerCount - 1);
    if (!bVictory) { bDefeat = false; if (BattlePhase == ECVADBattlePhase::Results) BattlePhase = ECVADBattlePhase::Frontline; }
    ForceNetUpdate();
}

void ACVADBattleDirector::StartBattle()
{
    if (!HasAuthority()) return;

    DefeatCount = 0; DownedPlayerCount = 0; bVictory = false; bDefeat = false;
    ExperienceEarned = 0; CompletionTimeSeconds = 0.f; BattleStartTimeSeconds = GetWorld()->GetTimeSeconds();
    int32 CombinedQuota = 0;
    for (TActorIterator<ACVADMinionSpawner> It(GetWorld()); It; ++It)
        CombinedQuota += FMath::Max(0, It->GetKillQuota());
    if (CombinedQuota > 0) FrontlineDefeatTarget = CombinedQuota;
    const ECVADBattlePhase PreviousPhase = BattlePhase;
    BattlePhase = ECVADBattlePhase::Frontline;
    ForceNetUpdate();
    OnBattlePhaseChanged.Broadcast(PreviousPhase, BattlePhase);
    OnDefeatCountChanged.Broadcast(DefeatCount, FrontlineDefeatTarget);
    UE_LOG(LogTemp, Log, TEXT("Battle started with %d total frontline defeats required"), FrontlineDefeatTarget);
}

void ACVADBattleDirector::RegisterExperienceReward(int32 Amount)
{
    if (!HasAuthority() || Amount <= 0) return;
    ExperienceEarned = FMath::Max(0, ExperienceEarned + Amount);
    ForceNetUpdate();
}

void ACVADBattleDirector::RegisterMinionDefeated(int32 Amount)
{
    if (!HasAuthority() || BattlePhase == ECVADBattlePhase::Results) return;

    DefeatCount = FMath::Max(0, DefeatCount + Amount);
    ForceNetUpdate();
    OnDefeatCountChanged.Broadcast(DefeatCount, FrontlineDefeatTarget);

    if (BattlePhase == ECVADBattlePhase::Frontline && DefeatCount >= FrontlineDefeatTarget)
    {
        AdvancePhase();
    }
}

void ACVADBattleDirector::AdvancePhase()
{
    if (!HasAuthority()) return;

    const ECVADBattlePhase PreviousPhase = BattlePhase;
    // The current demo intentionally has one army phase and one boss phase.
    switch (BattlePhase)
    {
    case ECVADBattlePhase::Rally: BattlePhase = ECVADBattlePhase::Frontline; break;
    case ECVADBattlePhase::Frontline: BattlePhase = ECVADBattlePhase::Boss; break;
    case ECVADBattlePhase::Boss: BattlePhase = ECVADBattlePhase::Results; break;
    default: BattlePhase = ECVADBattlePhase::Results; break;
    }
    if (BattlePhase == ECVADBattlePhase::Boss)
    {
        for (TActorIterator<ACVADEnemyCharacter> It(GetWorld()); It; ++It)
            if (!It->IsBoss()) It->Destroy();
        UE_LOG(LogTemp, Log, TEXT("Frontline complete: residual minions cleared, boss stage activated"));
    }
    ForceNetUpdate();
    OnBattlePhaseChanged.Broadcast(PreviousPhase, BattlePhase);
}

void ACVADBattleDirector::OnRep_BattlePhase(ECVADBattlePhase PreviousPhase)
{
    OnBattlePhaseChanged.Broadcast(PreviousPhase, BattlePhase);
}

void ACVADBattleDirector::OnRep_DefeatCount(int32 PreviousCount)
{
    OnDefeatCountChanged.Broadcast(DefeatCount, FrontlineDefeatTarget);
}

void ACVADBattleDirector::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACVADBattleDirector, BattlePhase);
    DOREPLIFETIME(ACVADBattleDirector, DefeatCount);
    DOREPLIFETIME(ACVADBattleDirector, BossHealth);
    DOREPLIFETIME(ACVADBattleDirector, BossMaxHealth);
    DOREPLIFETIME(ACVADBattleDirector, bVictory);
    DOREPLIFETIME(ACVADBattleDirector, bDefeat);
    DOREPLIFETIME(ACVADBattleDirector, DownedPlayerCount);
    DOREPLIFETIME(ACVADBattleDirector, RegisteredBoss);
    DOREPLIFETIME(ACVADBattleDirector, CompletionTimeSeconds);
    DOREPLIFETIME(ACVADBattleDirector, ExperienceEarned);
}

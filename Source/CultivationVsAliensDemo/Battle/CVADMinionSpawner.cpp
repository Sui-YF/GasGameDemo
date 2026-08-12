#include "Battle/CVADMinionSpawner.h"
#include "Character/CVADCharacter.h"
#include "Enemy/CVADEnemyCharacter.h"
#include "Components/BoxComponent.h"
#include "NavigationSystem.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"
#include "Data/CVADBalanceRows.h"
#include "Engine/DataTable.h"
#include "Battle/CVADBattleDirector.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogCVADSpawner, Log, All);

ACVADMinionSpawner::ACVADMinionSpawner()
{
    bReplicates = true;
    ActivationBox = CreateDefaultSubobject<UBoxComponent>(TEXT("ActivationBox"));
    SetRootComponent(ActivationBox);
    ActivationBox->SetBoxExtent(FVector(1200.f, 1200.f, 400.f));
    ActivationBox->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    ActivationBox->SetCollisionResponseToAllChannels(ECR_Ignore);
    ActivationBox->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    ActivationBox->SetHiddenInGame(true);
}

void ACVADMinionSpawner::BeginPlay()
{
    Super::BeginPlay();
    ActivationBox->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleBeginOverlap);
    ActivationBox->OnComponentEndOverlap.AddDynamic(this, &ThisClass::HandleEndOverlap);
    ApplyProfile();
    if (HasAuthority())
    {
        // A player may already be inside when BeginPlay binds overlap delegates (common on map load).
        // Seed the set explicitly so the first demo wave cannot miss its activation event.
        TArray<AActor*> InitialOverlaps;
        ActivationBox->GetOverlappingActors(InitialOverlaps, ACVADCharacter::StaticClass());
        for (AActor* Actor : InitialOverlaps) PlayersInside.Add(Actor);
        UE_LOG(LogCVADSpawner, Log, TEXT("Spawner %s initialized. PlayersInside=%d MinionClass=%s"),
            *GetName(), PlayersInside.Num(), *GetNameSafe(MinionClass));
        if (bStartActive) StartSpawning();
        else RefreshSpawningState();
        FTimerHandle InitialPlayerScanTimer;
        GetWorldTimerManager().SetTimer(InitialPlayerScanTimer, this, &ThisClass::ScanPlayersAndRefresh, 0.5f, false);
    }
}

void ACVADMinionSpawner::ScanPlayersAndRefresh()
{
    if (!HasAuthority()) return;
    TArray<AActor*> Overlaps;
    ActivationBox->GetOverlappingActors(Overlaps, ACVADCharacter::StaticClass());
    for (AActor* Actor : Overlaps) PlayersInside.Add(Actor);
    UE_LOG(LogCVADSpawner, Log, TEXT("Spawner %s delayed player scan. PlayersInside=%d"), *GetName(), PlayersInside.Num());
    RefreshSpawningState();
}

void ACVADMinionSpawner::ApplyProfile()
{
    UDataTable* Profiles = LoadObject<UDataTable>(nullptr, TEXT("/Game/CVAD/Data/DT_SpawnerProfiles.DT_SpawnerProfiles"));
    const FCVADSpawnerProfileRow* Row = Profiles ? Profiles->FindRow<FCVADSpawnerProfileRow>(ProfileRowName, TEXT("SpawnerBeginPlay")) : nullptr;
    if (!Row) return;
    SpawnInterval = Row->SpawnInterval;
    MaxAlive = Row->MaxAlive;
    KillQuota = Row->KillQuota;
    bRequirePlayerInside = Row->bRequirePlayerInside;
}

void ACVADMinionSpawner::StartSpawning()
{
    if (!HasAuthority() || bCompleted || !MinionClass) return;
    if (!GetWorldTimerManager().IsTimerActive(SpawnTimer))
    {
        UE_LOG(LogCVADSpawner, Log, TEXT("Spawner %s started. MaxAlive=%d KillQuota=%d Interval=%.2f"),
            *GetName(), MaxAlive, KillQuota, SpawnInterval);
        GetWorldTimerManager().SetTimer(SpawnTimer, this, &ThisClass::TrySpawnMinion, SpawnInterval, true, 0.f);
    }
}

void ACVADMinionSpawner::StopSpawning()
{
    if (!HasAuthority()) return;
    GetWorldTimerManager().ClearTimer(SpawnTimer);
    UE_LOG(LogCVADSpawner, Log, TEXT("Spawner %s paused. Alive=%d Defeated=%d"), *GetName(), AliveCount, DefeatedCount);
}

void ACVADMinionSpawner::ResetSpawner()
{
    if (!HasAuthority()) return;
    StopSpawning();
    DefeatedCount = 0;
    bCompleted = false;
    PruneInvalidEntries();
    AliveCount = SpawnedMinions.Num();
    RefreshSpawningState();
    ForceNetUpdate();
}

void ACVADMinionSpawner::DebugSpawnBoss()
{
#if !UE_BUILD_SHIPPING
    CompleteSpawner();
#endif
}

void ACVADMinionSpawner::HandleBeginOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!HasAuthority() || !OtherActor || !OtherActor->IsA<ACVADCharacter>()) return;
    PlayersInside.Add(OtherActor);
    UE_LOG(LogCVADSpawner, Log, TEXT("Player %s entered spawner %s. PlayersInside=%d"), *GetNameSafe(OtherActor), *GetName(), PlayersInside.Num());
    RefreshSpawningState();
}

void ACVADMinionSpawner::HandleEndOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32)
{
    if (!HasAuthority() || !OtherActor || !OtherActor->IsA<ACVADCharacter>()) return;
    PlayersInside.Remove(OtherActor);
    UE_LOG(LogCVADSpawner, Log, TEXT("Player %s left spawner %s. PlayersInside=%d"), *GetNameSafe(OtherActor), *GetName(), PlayersInside.Num());
    RefreshSpawningState();
}

void ACVADMinionSpawner::RefreshSpawningState()
{
    PruneInvalidEntries();
    if (bCompleted)
    {
        StopSpawning();
        return;
    }
    for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It)
    {
        if (It->BattlePhase != ECVADBattlePhase::Frontline) { StopSpawning(); return; }
        break;
    }
    const bool bMaySpawn = !bRequirePlayerInside || PlayersInside.Num() > 0;
    if (bMaySpawn && (bResumeWhenPlayerReturns || DefeatedCount == 0)) StartSpawning();
    else StopSpawning();
}

void ACVADMinionSpawner::TrySpawnMinion()
{
    if (!HasAuthority() || bCompleted || !MinionClass) return;
    for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It)
    {
        if (It->BattlePhase != ECVADBattlePhase::Frontline) { StopSpawning(); return; }
        break;
    }
    PruneInvalidEntries();
    if (bRequirePlayerInside && PlayersInside.Num() == 0) { StopSpawning(); return; }
    if (KillQuota > 0 && DefeatedCount >= KillQuota)
    {
        CompleteSpawner();
        return;
    }
    if (AliveCount >= MaxAlive) return;

    FActorSpawnParameters Params;
    Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
    ACVADEnemyCharacter* Minion = GetWorld()->SpawnActor<ACVADEnemyCharacter>(MinionClass, FindSpawnLocation(), GetActorRotation(), Params);
    if (Minion)
    {
        Minion->SetSpawnSource(this);
        SpawnedMinions.Add(Minion);
        AliveCount = SpawnedMinions.Num();
        ForceNetUpdate();
        UE_LOG(LogCVADSpawner, Log, TEXT("Spawner %s created %s Alive=%d"), *GetName(), *GetNameSafe(Minion), AliveCount);
    }
}

void ACVADMinionSpawner::NotifySpawnedMinionDefeated(ACVADEnemyCharacter* DefeatedMinion)
{
    if (!HasAuthority()) return;
    SpawnedMinions.Remove(DefeatedMinion);
    AliveCount = SpawnedMinions.Num();
    ++DefeatedCount;
    UE_LOG(LogCVADSpawner, Log, TEXT("Spawner %s defeat progress %d/%s Alive=%d"), *GetName(), DefeatedCount,
        KillQuota > 0 ? *FString::FromInt(KillQuota) : TEXT("Infinite"), AliveCount);
    if (KillQuota > 0 && DefeatedCount >= KillQuota)
    {
        CompleteSpawner();
    }
    ForceNetUpdate();
}

void ACVADMinionSpawner::CompleteSpawner()
{
    if (!HasAuthority() || bCompleted) return;
    bCompleted = true;
    StopSpawning();
    ACVADBattleDirector* Director = nullptr;
    for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It) { Director = *It; break; }
    if (BossClass && (!Director || !IsValid(Director->RegisteredBoss)))
    {
        FActorSpawnParameters Params;
        Params.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;
        ACVADEnemyCharacter* Boss = GetWorld()->SpawnActor<ACVADEnemyCharacter>(BossClass,
            GetActorTransform().TransformPosition(BossSpawnOffset), GetActorRotation(), Params);
        if (Director && Boss) Director->RegisterBoss(Boss);
        UE_LOG(LogCVADSpawner, Log, TEXT("Spawner %s completed and spawned Boss=%s"), *GetName(), *GetNameSafe(Boss));
    }
    OnSpawnerCompleted();
    ForceNetUpdate();
}

FVector ACVADMinionSpawner::FindSpawnLocation() const
{
    const FVector Extent = ActivationBox->GetScaledBoxExtent();
    const FVector Origin = ActivationBox->GetComponentLocation();
    const FVector RandomPoint = Origin + FVector(
        FMath::FRandRange(-Extent.X, Extent.X),
        FMath::FRandRange(-Extent.Y, Extent.Y),
        0.f);
    FNavLocation NavLocation;
    if (const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(GetWorld()))
    {
        if (Nav->ProjectPointToNavigation(RandomPoint, NavLocation, FVector(300.f, 300.f, 600.f))) return NavLocation.Location;
    }
    return RandomPoint;
}

void ACVADMinionSpawner::PruneInvalidEntries()
{
    for (auto It = PlayersInside.CreateIterator(); It; ++It) if (!It->IsValid()) It.RemoveCurrent();
    for (auto It = SpawnedMinions.CreateIterator(); It; ++It) if (!It->IsValid()) It.RemoveCurrent();
    AliveCount = SpawnedMinions.Num();
}

void ACVADMinionSpawner::OnRep_Completed()
{
    if (bCompleted) OnSpawnerCompleted();
}

void ACVADMinionSpawner::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACVADMinionSpawner, AliveCount);
    DOREPLIFETIME(ACVADMinionSpawner, DefeatedCount);
    DOREPLIFETIME(ACVADMinionSpawner, bCompleted);
}

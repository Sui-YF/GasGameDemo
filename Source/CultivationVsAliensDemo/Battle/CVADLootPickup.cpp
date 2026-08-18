#include "Battle/CVADLootPickup.h"

#include "AbilitySystemInterface.h"
#include "Character/CVADCharacter.h"
#include "Components/PointLightComponent.h"
#include "Components/SphereComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "EngineUtils.h"
#include "Player/CVADPlayerState.h"
#include "TimerManager.h"
#include "UObject/ConstructorHelpers.h"

ACVADLootPickup::ACVADLootPickup()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    bAlwaysRelevant = true;

    PickupSphere = CreateDefaultSubobject<USphereComponent>(TEXT("PickupSphere"));
    RootComponent = PickupSphere;
    PickupSphere->SetSphereRadius(160.f);
    PickupSphere->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    PickupSphere->SetCollisionResponseToAllChannels(ECR_Ignore);
    PickupSphere->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    PickupSphere->SetGenerateOverlapEvents(true);

    PickupMesh = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("PickupMesh"));
    PickupMesh->SetupAttachment(PickupSphere);
    PickupMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<UStaticMesh> SphereMesh(
        TEXT("/Engine/BasicShapes/Sphere.Sphere"));
    if (SphereMesh.Succeeded()) PickupMesh->SetStaticMesh(SphereMesh.Object);
    PickupMesh->SetRelativeScale3D(FVector(0.75f));

    PickupLight = CreateDefaultSubobject<UPointLightComponent>(TEXT("PickupLight"));
    PickupLight->SetupAttachment(PickupSphere);
    PickupLight->SetLightColor(FLinearColor(1.f, 0.72f, 0.2f));
    PickupLight->SetIntensity(6000.f);
    PickupLight->SetAttenuationRadius(650.f);
    PickupLight->SetSourceRadius(30.f);
}

void ACVADLootPickup::InitializeLoot(int32 InExperience, int32 InSkillPoints, const FString& InDisplayName)
{
    if (!HasAuthority()) return;
    ExperienceReward = FMath::Max(0, InExperience);
    SkillPointReward = FMath::Max(0, InSkillPoints);
    if (!InDisplayName.IsEmpty()) LootDisplayName = InDisplayName;
    SetLifeSpan(LifetimeSeconds);
    UE_LOG(LogTemp, Log, TEXT("Loot %s spawned XP=%d SP=%d"), *GetName(), ExperienceReward, SkillPointReward);
}

void ACVADLootPickup::BeginPlay()
{
    Super::BeginPlay();
    InitialLocation = GetActorLocation();
    SetLifeSpan(LifetimeSeconds);
}

void ACVADLootPickup::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    if (IsValid(PickupMesh))
    {
        const float Time = GetWorld() ? GetWorld()->GetTimeSeconds() : 0.f;
        PickupMesh->SetRelativeLocation(FVector(0.f, 0.f, FMath::Sin(Time * BobSpeed) * BobHeight));
        PickupMesh->AddLocalRotation(FRotator(0.f, 60.f * DeltaSeconds, 0.f));
    }
    if (HasAuthority()) CheckForCollectors();
}

void ACVADLootPickup::CheckForCollectors()
{
    if (bCollected) return;
    const float PickupRadius = PickupSphere ? PickupSphere->GetScaledSphereRadius() : 160.f;
    for (TActorIterator<ACVADCharacter> It(GetWorld()); It; ++It)
    {
        ACVADCharacter* Player = *It;
        if (!Player || Player->IsPlayerDown()) continue;
        if (FVector::DistSquared2D(GetActorLocation(), Player->GetActorLocation())
            > FMath::Square(PickupRadius)) continue;
        Collect(Player);
        return;
    }
}

void ACVADLootPickup::Collect(ACVADCharacter* Player)
{
    if (!HasAuthority() || bCollected || !Player) return;
    bCollected = true;
    ACVADPlayerState* PS = Player->GetPlayerState<ACVADPlayerState>();
    if (PS)
    {
        if (ExperienceReward > 0) PS->AddExperience(ExperienceReward);
        if (SkillPointReward > 0)
        {
            PS->SkillPoints += SkillPointReward;
            PS->ForceNetUpdate();
        }
    }
    Player->ClientNotifyLootCollected(LootDisplayName, ExperienceReward, SkillPointReward);
    UE_LOG(LogTemp, Log, TEXT("Player %s collected loot %s XP=%d SP=%d"),
        *Player->GetName(), *LootDisplayName, ExperienceReward, SkillPointReward);
    Destroy();
}

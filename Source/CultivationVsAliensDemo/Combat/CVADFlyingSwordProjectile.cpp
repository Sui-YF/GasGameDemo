#include "Combat/CVADFlyingSwordProjectile.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystem/Effects/CVADDamageEffect.h"
#include "Components/SphereComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Enemy/CVADEnemyCharacter.h"
#include "GameplayTagsManager.h"
#include "Net/UnrealNetwork.h"
#include "UObject/ConstructorHelpers.h"

ACVADFlyingSwordProjectile::ACVADFlyingSwordProjectile()
{
    PrimaryActorTick.bCanEverTick = true;
    bReplicates = true;
    SetReplicateMovement(true);
    NetUpdateFrequency = 30.f;

    Collision = CreateDefaultSubobject<USphereComponent>(TEXT("Collision"));
    SetRootComponent(Collision);
    Collision->SetSphereRadius(45.f);
    Collision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);
    Collision->SetCollisionResponseToAllChannels(ECR_Ignore);
    Collision->SetCollisionResponseToChannel(ECC_Pawn, ECR_Overlap);
    Collision->OnComponentBeginOverlap.AddDynamic(this, &ThisClass::HandleOverlap);

    SwordMesh = CreateDefaultSubobject<USkeletalMeshComponent>(TEXT("SwordMesh"));
    SwordMesh->SetupAttachment(Collision);
    SwordMesh->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    static ConstructorHelpers::FObjectFinder<USkeletalMesh> SwordAsset(TEXT("/Game/LanFang/Meshes/Weapons/SK_Sword.SK_Sword"));
    if (SwordAsset.Succeeded()) SwordMesh->SetSkeletalMesh(SwordAsset.Object);
    SwordMesh->SetRelativeRotation(FRotator(0.f, 90.f, 0.f));
    InitialLifeSpan = MaxLifetime;
}

void ACVADFlyingSwordProjectile::InitializeProjectile(AActor* InOwner, AActor* InTarget, float InDamage)
{
    if (!HasAuthority()) return;
    SetOwner(InOwner);
    TargetActor = InTarget;
    Damage = FMath::Max(0.f, InDamage);
    ForceNetUpdate();
}

void ACVADFlyingSwordProjectile::Tick(float DeltaSeconds)
{
    Super::Tick(DeltaSeconds);
    // The server owns homing simulation. Clients receive replicated movement and must not
    // independently integrate the projectile or they will fight network correction.
    if (!HasAuthority()) return;
    if (!bReturning && (!IsValid(TargetActor.Get()) || TargetActor->IsActorBeingDestroyed()))
    {
        bReturning = true;
        TargetActor = nullptr;
        Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        SetLifeSpan(2.f);
        ForceNetUpdate();
    }
    AActor* DestinationActor = bReturning ? GetOwner() : TargetActor.Get();
    if (!DestinationActor)
    {
        if (HasAuthority()) Destroy();
        return;
    }
    const FVector ToDestination = DestinationActor->GetActorLocation() + FVector(0.f, 0.f, 90.f) - GetActorLocation();
    if (HasAuthority() && bReturning && ToDestination.SizeSquared() <= FMath::Square(ReturnDistance))
    {
        Destroy();
        return;
    }
    const FVector Direction = ToDestination.GetSafeNormal();
    const FRotator DesiredRotation = Direction.Rotation();
    SetActorRotation(FMath::RInterpTo(GetActorRotation(), DesiredRotation, DeltaSeconds, TurnSpeed));
    AddActorWorldOffset(GetActorForwardVector() * FlightSpeed * DeltaSeconds, true);
}

void ACVADFlyingSwordProjectile::HandleOverlap(UPrimitiveComponent*, AActor* OtherActor, UPrimitiveComponent*, int32, bool, const FHitResult&)
{
    if (!HasAuthority() || bReturning || bDamageApplied || !OtherActor || OtherActor == GetOwner()) return;
    ACVADEnemyCharacter* Enemy = Cast<ACVADEnemyCharacter>(OtherActor);
    IAbilitySystemInterface* SourceInterface = Cast<IAbilitySystemInterface>(GetOwner());
    UAbilitySystemComponent* SourceASC = SourceInterface ? SourceInterface->GetAbilitySystemComponent() : nullptr;
    UAbilitySystemComponent* TargetASC = Enemy ? Enemy->GetAbilitySystemComponent() : nullptr;
    if (!SourceASC || !TargetASC) return;
    FGameplayEffectSpecHandle Spec = SourceASC->MakeOutgoingSpec(UCVADDamageEffect::StaticClass(), 1.f, SourceASC->MakeEffectContext());
    if (Spec.IsValid())
    {
        Spec.Data->SetSetByCallerMagnitude(UGameplayTagsManager::Get().RequestGameplayTag(TEXT("Data.Damage")), -Damage);
        SourceASC->ApplyGameplayEffectSpecToTarget(*Spec.Data.Get(), TargetASC);
        bDamageApplied = true;
        bReturning = true;
        TargetActor = nullptr;
        Collision->SetCollisionEnabled(ECollisionEnabled::NoCollision);
        SetLifeSpan(2.f);
        ForceNetUpdate();
        UE_LOG(LogTemp, Log, TEXT("Flying sword projectile hit %s Damage=%.1f and is returning"), *GetNameSafe(Enemy), Damage);
    }
}

void ACVADFlyingSwordProjectile::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(ACVADFlyingSwordProjectile, TargetActor);
    DOREPLIFETIME(ACVADFlyingSwordProjectile, bReturning);
}

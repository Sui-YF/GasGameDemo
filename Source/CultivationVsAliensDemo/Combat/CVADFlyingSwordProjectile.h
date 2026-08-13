#pragma once

#include "GameFramework/Actor.h"
#include "CVADFlyingSwordProjectile.generated.h"

class USkeletalMeshComponent;
class USphereComponent;

UCLASS()
class CULTIVATIONVSALIENSDEMO_API ACVADFlyingSwordProjectile : public AActor
{
    GENERATED_BODY()

public:
    ACVADFlyingSwordProjectile();
    virtual void Tick(float DeltaSeconds) override;
    void InitializeProjectile(AActor* InOwner, AActor* InTarget, float InDamage);

protected:
    UPROPERTY(VisibleAnywhere) TObjectPtr<USphereComponent> Collision;
    UPROPERTY(VisibleAnywhere) TObjectPtr<USkeletalMeshComponent> SwordMesh;
    UPROPERTY(Replicated) TObjectPtr<AActor> TargetActor;
    UPROPERTY(Replicated) bool bReturning = false;
    UPROPERTY(EditDefaultsOnly, Category="Flying Sword") float FlightSpeed = 1800.f;
    UPROPERTY(EditDefaultsOnly, Category="Flying Sword") float TurnSpeed = 8.f;
    UPROPERTY(EditDefaultsOnly, Category="Flying Sword") float ReturnDistance = 110.f;
    UPROPERTY(EditDefaultsOnly, Category="Flying Sword") float MaxLifetime = 4.f;

private:
    UFUNCTION() void HandleOverlap(UPrimitiveComponent* OverlappedComponent, AActor* OtherActor,
        UPrimitiveComponent* OtherComponent, int32 OtherBodyIndex, bool bFromSweep, const FHitResult& SweepResult);
    float Damage = 0.f;
    bool bDamageApplied = false;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
};

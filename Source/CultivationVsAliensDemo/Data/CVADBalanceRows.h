#pragma once

#include "Engine/DataTable.h"
#include "CVADBalanceRows.generated.h"

USTRUCT(BlueprintType)
struct FCVADEnemyBalanceRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float MaxHealth = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float AttackDamage = 8.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float MoveSpeed = 420.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float AttackInterval = 1.5f;
};

USTRUCT(BlueprintType)
struct FCVADSpawnerProfileRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float SpawnInterval = 1.25f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 MaxAlive = 12;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) int32 KillQuota = 30;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bRequirePlayerInside = true;
};

USTRUCT(BlueprintType)
struct FCVADPlayerBalanceRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float MaxHealth = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float MaxStamina = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float MaxSpirit = 100.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float MoveSpeed = 650.f;
};

#pragma once

#include "Engine/DataTable.h"
#include "AbilitySystem/Abilities/CVADCombatAbility.h"
#include "CVADSkillRows.generated.h"

class UGameplayAbility;
class UTexture2D;
class UAnimSequenceBase;

USTRUCT(BlueprintType)
struct FCVADSkillRow : public FTableRowBase
{
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FText DisplayName;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(MultiLine=true)) FText Description;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) ECVADAbilityInput SkillSlot = ECVADAbilityInput::LightAttack;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftClassPtr<UGameplayAbility> AbilityClass;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) TSoftObjectPtr<UAnimSequenceBase> ActionAnimation;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName ResourceType = TEXT("None");
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float ResourceCost = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float CooldownSeconds = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bIsAreaOfEffect = false;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) float AreaRadius = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) bool bUnlockedByDefault = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="1")) int32 RequiredLevel = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, meta=(ClampMin="0")) int32 SkillPointCost = 1;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName PrerequisiteSkill;
};

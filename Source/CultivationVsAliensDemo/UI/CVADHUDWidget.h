#pragma once

#include "UI/CVADUserWidget.h"
#include "CVADHUDWidget.generated.h"

class UProgressBar;
class UTextBlock;
class ACVADBattleDirector;

UCLASS(Blueprintable)
class CULTIVATIONVSALIENSDEMO_API UCVADHUDWidget : public UCVADUserWidget
{
    GENERATED_BODY()

protected:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;

private:
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UProgressBar> HealthBar;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UProgressBar> StaminaBar;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UProgressBar> SpiritBar;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> DefeatText;
    UPROPERTY(meta=(BindWidget)) TObjectPtr<UTextBlock> ObjectiveText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> ExperienceBar;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UProgressBar> BossHealthBar;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> LevelText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ExperienceText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SkillPointsText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> BossNameText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> ResultStateText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> DownedHintText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> PlayerNameText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> HealthValueText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> StaminaValueText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SpiritValueText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> StanceText;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SkillSlot1Text;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SkillSlot2Text;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SkillSlot3Text;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SkillSlot4Text;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> SkillSlot5Text;
    TWeakObjectPtr<ACVADBattleDirector> BattleDirector;
    UPROPERTY(EditDefaultsOnly, Category="HUD|Boss", meta=(ClampMin="500.0")) float BossHUDDisplayRadius = 5000.f;
};

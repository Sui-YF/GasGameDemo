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
    TWeakObjectPtr<ACVADBattleDirector> BattleDirector;
};

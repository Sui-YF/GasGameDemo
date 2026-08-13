#include "UI/CVADHUDWidget.h"
#include "AbilitySystem/CVADAttributeSet.h"
#include "Battle/CVADBattleDirector.h"
#include "Player/CVADPlayerState.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "Character/CVADCharacter.h"

void UCVADHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    InitializeFromOwningPlayer();
}

void UCVADHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    ACVADPlayerState* PS = GetCVADPlayerState();
    UCVADAttributeSet* A = PS ? PS->GetAttributeSet() : nullptr;
    if (A)
    {
        if (HealthBar) HealthBar->SetPercent(A->GetMaxHealth() > 0.f ? A->GetHealth()/A->GetMaxHealth() : 0.f);
        if (StaminaBar) StaminaBar->SetPercent(A->GetMaxStamina() > 0.f ? A->GetStamina()/A->GetMaxStamina() : 0.f);
        if (SpiritBar) SpiritBar->SetPercent(A->GetMaxSpirit() > 0.f ? A->GetSpirit()/A->GetMaxSpirit() : 0.f);
    }
    if (PS)
    {
        if (LevelText) LevelText->SetText(FText::Format(NSLOCTEXT("CVAD","Level","等级 {0}"), PS->PlayerLevel));
        if (ExperienceText) ExperienceText->SetText(FText::Format(NSLOCTEXT("CVAD","XP","经验 {0}/{1}"), PS->Experience, PS->GetExperienceToNextLevel()));
        if (ExperienceBar) ExperienceBar->SetPercent(PS->GetExperienceToNextLevel()>0 ? static_cast<float>(PS->Experience)/PS->GetExperienceToNextLevel() : 0.f);
        if (SkillPointsText) SkillPointsText->SetText(FText::Format(NSLOCTEXT("CVAD","SP","技能点 {0}"), PS->SkillPoints));
    }
    if (DownedHintText)
    {
        const ACVADCharacter* LocalCharacter = GetOwningPlayerPawn<ACVADCharacter>();
        bool bTeammateDown = false;
        if (GetWorld())
            for (TActorIterator<ACVADCharacter> It(GetWorld()); It; ++It)
                if (*It != LocalCharacter && It->IsPlayerDown()) { bTeammateDown = true; break; }
        if (LocalCharacter && LocalCharacter->IsPlayerDown())
        {
            DownedHintText->SetText(NSLOCTEXT("CVAD", "WaitingForRevive", "你已倒地，等待队友救援"));
            DownedHintText->SetVisibility(ESlateVisibility::Visible);
        }
        else if (bTeammateDown)
        {
            DownedHintText->SetText(NSLOCTEXT("CVAD", "ReviveTeammate", "队友已倒地，靠近后按交互键救援"));
            DownedHintText->SetVisibility(ESlateVisibility::Visible);
        }
        else DownedHintText->SetVisibility(ESlateVisibility::Collapsed);
    }
    if (!BattleDirector.IsValid() && GetWorld())
        for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It) { BattleDirector=*It; break; }
    if (BattleDirector.IsValid())
    {
        if (DefeatText) DefeatText->SetText(FText::Format(NSLOCTEXT("CVAD","Defeats","击破 {0}/{1}"), BattleDirector->DefeatCount, BattleDirector->FrontlineDefeatTarget));
        FText PhaseText = NSLOCTEXT("CVAD", "RallyPhase", "集结准备");
        switch (BattleDirector->BattlePhase)
        {
        case ECVADBattlePhase::Frontline: PhaseText = NSLOCTEXT("CVAD", "FrontlinePhase", "清剿外星机械军团"); break;
        case ECVADBattlePhase::Boss: PhaseText = FText::Format(NSLOCTEXT("CVAD", "BossPhase", "击败天穹三使（剩余 {0}）"), BattleDirector->BossesRemaining); break;
        case ECVADBattlePhase::Results: PhaseText = NSLOCTEXT("CVAD", "ResultsPhase", "战斗结束"); break;
        default: break;
        }
        if (ObjectiveText) ObjectiveText->SetText(PhaseText);
        const bool bBoss = BattleDirector->BattlePhase == ECVADBattlePhase::Boss;
        if (BossHealthBar) { BossHealthBar->SetPercent(BattleDirector->BossMaxHealth>0.f ? BattleDirector->BossHealth/BattleDirector->BossMaxHealth : 0.f); BossHealthBar->SetVisibility(bBoss?ESlateVisibility::Visible:ESlateVisibility::Collapsed); }
        if (BossNameText) { BossNameText->SetText(FText::Format(NSLOCTEXT("CVAD","BossName","天穹三使 · 剑士 / 翼卫 / 天术师  [{0}/3]"),BattleDirector->BossesRemaining)); BossNameText->SetVisibility(bBoss?ESlateVisibility::Visible:ESlateVisibility::Collapsed); }
        if (ResultStateText) ResultStateText->SetText(BattleDirector->bVictory?NSLOCTEXT("CVAD","Victory","胜利"):(BattleDirector->bDefeat?NSLOCTEXT("CVAD","Defeat","失败"):FText::GetEmpty()));
    }
}

#include "UI/CVADHUDWidget.h"
#include "AbilitySystem/CVADAttributeSet.h"
#include "Battle/CVADBattleDirector.h"
#include "Player/CVADPlayerState.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "EngineUtils.h"
#include "Character/CVADCharacter.h"
#include "Enemy/CVADEnemyCharacter.h"
#include "Data/CVADSkillRows.h"
#include "Engine/DataTable.h"

void UCVADHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();
    InitializeFromOwningPlayer();
    if(BossHealthBar) BossHealthBar->SetVisibility(ESlateVisibility::Collapsed);
    if(BossNameText) BossNameText->SetVisibility(ESlateVisibility::Collapsed);
}

void UCVADHUDWidget::NativeTick(const FGeometry& MyGeometry, float InDeltaTime)
{
    Super::NativeTick(MyGeometry, InDeltaTime);
    ACVADPlayerState* PS=GetCVADPlayerState();
    UCVADAttributeSet* A=PS?PS->GetAttributeSet():nullptr;
    if(A)
    {
        if(HealthBar) HealthBar->SetPercent(A->GetMaxHealth()>0.f?A->GetHealth()/A->GetMaxHealth():0.f);
        if(StaminaBar) StaminaBar->SetPercent(A->GetMaxStamina()>0.f?A->GetStamina()/A->GetMaxStamina():0.f);
        if(SpiritBar) SpiritBar->SetPercent(A->GetMaxSpirit()>0.f?A->GetSpirit()/A->GetMaxSpirit():0.f);
        if(HealthValueText) HealthValueText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"),A->GetHealth(),A->GetMaxHealth())));
        if(StaminaValueText) StaminaValueText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"),A->GetStamina(),A->GetMaxStamina())));
        if(SpiritValueText) SpiritValueText->SetText(FText::FromString(FString::Printf(TEXT("%.0f / %.0f"),A->GetSpirit(),A->GetMaxSpirit())));
    }
    if(PS)
    {
        if(SkillPointsText) SkillPointsText->SetVisibility(ESlateVisibility::Collapsed);
        if(PlayerNameText) PlayerNameText->SetText(FText::FromString(PS->GetPlayerName()));
        if(LevelText) LevelText->SetText(FText::Format(NSLOCTEXT("CVAD","HUDLevel","等级 {0}"),PS->PlayerLevel));
        if(ExperienceText) ExperienceText->SetText(FText::Format(NSLOCTEXT("CVAD","HUDXP","经验 {0}/{1}"),PS->Experience,PS->GetExperienceToNextLevel()));
        if(ExperienceBar) ExperienceBar->SetPercent(PS->GetExperienceToNextLevel()>0?static_cast<float>(PS->Experience)/PS->GetExperienceToNextLevel():0.f);
        if(SkillPointsText) SkillPointsText->SetText(FText::Format(NSLOCTEXT("CVAD","HUDSP","技能点 {0}"),PS->SkillPoints));
        UDataTable* SkillTable=LoadObject<UDataTable>(nullptr,TEXT("/Game/CVAD/Data/DT_Skills.DT_Skills"));
        UTextBlock* Slots[]={SkillSlot1Text,SkillSlot2Text,SkillSlot3Text,SkillSlot4Text,SkillSlot5Text};
        for(int32 Index=0;Index<5;++Index) if(Slots[Index])
        {
            FText SkillName=NSLOCTEXT("CVAD","HUDDefaultAction","基础招式");
            if(PS->EquippedSkillRows.IsValidIndex(Index) && SkillTable)
                if(const FCVADSkillRow* Row=SkillTable->FindRow<FCVADSkillRow>(PS->EquippedSkillRows[Index],TEXT("HUD"))) SkillName=Row->DisplayName;
            Slots[Index]->SetText(FText::Format(NSLOCTEXT("CVAD","HUDSkillSlot","{0}  {1}"),Index+1,SkillName));
        }
    }
    const ACVADCharacter* LocalCharacter=GetOwningPlayerPawn<ACVADCharacter>();
    if(LocalCharacter && StanceText) StanceText->SetText(LocalCharacter->IsFlyingSwordMode()?NSLOCTEXT("CVAD","HUDFlying","御剑模式 · 消耗灵力"):NSLOCTEXT("CVAD","HUDSword","持剑模式 · 消耗体力"));
    if(DownedHintText)
    {
        bool bTeammateDown=false;
        if(GetWorld()) for(TActorIterator<ACVADCharacter> It(GetWorld());It;++It) if(*It!=LocalCharacter&&It->IsPlayerDown()){bTeammateDown=true;break;}
        if(LocalCharacter&&LocalCharacter->IsPlayerDown()){DownedHintText->SetText(NSLOCTEXT("CVAD","HUDDown","你已倒地，等待队友救援"));DownedHintText->SetVisibility(ESlateVisibility::Visible);}
        else if(bTeammateDown){DownedHintText->SetText(NSLOCTEXT("CVAD","HUDRevive","队友已倒地，靠近后按 E 救援"));DownedHintText->SetVisibility(ESlateVisibility::Visible);}
        else DownedHintText->SetVisibility(ESlateVisibility::Collapsed);
    }
    if(!BattleDirector.IsValid()&&GetWorld()) for(TActorIterator<ACVADBattleDirector> It(GetWorld());It;++It){BattleDirector=*It;break;}
    if(!BattleDirector.IsValid()) return;
    if(DefeatText) DefeatText->SetText(FText::Format(NSLOCTEXT("CVAD","HUDKills","击破 {0}/{1}"),BattleDirector->DefeatCount,BattleDirector->FrontlineDefeatTarget));
    FText Phase=NSLOCTEXT("CVAD","HUDRally","集结准备");
    if(BattleDirector->BattlePhase==ECVADBattlePhase::Frontline) Phase=NSLOCTEXT("CVAD","HUDFrontline","肃清骷髅军团");
    else if(BattleDirector->BattlePhase==ECVADBattlePhase::Boss) Phase=FText::Format(NSLOCTEXT("CVAD","HUDBossObjective","击败天穹三使（剩余 {0}）"),BattleDirector->BossesRemaining);
    else if(BattleDirector->BattlePhase==ECVADBattlePhase::Results) Phase=NSLOCTEXT("CVAD","HUDResults","战斗结束");
    if(ObjectiveText) ObjectiveText->SetText(Phase);
    bool bNearBoss=false;
    if(LocalCharacter && BattleDirector->BattlePhase==ECVADBattlePhase::Boss)
        for(const ACVADEnemyCharacter* Boss : BattleDirector->RegisteredBosses)
            if(IsValid(Boss) && FVector::DistSquared(LocalCharacter->GetActorLocation(),Boss->GetActorLocation())<=FMath::Square(3500.f)){bNearBoss=true;break;}
    const bool bBoss=bNearBoss && BattleDirector->BossesRemaining>0;
    if(BossHealthBar){BossHealthBar->SetPercent(BattleDirector->BossMaxHealth>0.f?BattleDirector->BossHealth/BattleDirector->BossMaxHealth:0.f);BossHealthBar->SetVisibility(bBoss?ESlateVisibility::Visible:ESlateVisibility::Collapsed);}
    if(BossNameText){BossNameText->SetText(FText::Format(NSLOCTEXT("CVAD","HUDBossName","天穹三使 · 剑使 / 翼卫 / 天术师  [{0}/3]"),BattleDirector->BossesRemaining));BossNameText->SetVisibility(bBoss?ESlateVisibility::Visible:ESlateVisibility::Collapsed);}
    if(ResultStateText) ResultStateText->SetText(BattleDirector->bVictory?NSLOCTEXT("CVAD","HUDVictory","胜利"):(BattleDirector->bDefeat?NSLOCTEXT("CVAD","HUDDefeat","失败"):FText::GetEmpty()));
}

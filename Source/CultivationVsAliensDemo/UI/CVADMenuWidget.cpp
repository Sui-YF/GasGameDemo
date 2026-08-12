#include "UI/CVADMenuWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Player/CVADPlayerController.h"
#include "Battle/CVADBattleDirector.h"
#include "Save/CVADSaveGame.h"
#include "EngineUtils.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"

void UCVADMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (Button_SinglePlayer) Button_SinglePlayer->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSinglePlayerClicked);
    if (Button_HostListenServer) Button_HostListenServer->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleHostClicked);
    if (Button_JoinGame) Button_JoinGame->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleJoinClicked);
    if (Button_LoadGame) Button_LoadGame->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLoadClicked);
    if (Button_Quit) Button_Quit->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleQuitClicked);
    if (Button_Retry) Button_Retry->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRetryClicked);
    if (Button_ReturnLobby) Button_ReturnLobby->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReturnClicked);
    if (Button_ReturnMainMenu) Button_ReturnMainMenu->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReturnClicked);
    bool bWon = false; float Time = 0.f; int32 Defeats = 0; int32 XP = 0;
    if (GetBattleResultData(bWon, Time, Defeats, XP))
    {
        if (Text_ResultTitle) Text_ResultTitle->SetText(bWon ? NSLOCTEXT("CVAD", "ResultVictory", "修仙者胜利") : NSLOCTEXT("CVAD", "ResultDefeat", "战斗失败"));
        const int32 Minutes = FMath::FloorToInt(Time / 60.f);
        const int32 Seconds = FMath::FloorToInt(FMath::Fmod(Time, 60.f));
        if (Text_ClearTime) Text_ClearTime->SetText(FText::FromString(FString::Printf(TEXT("战斗时间 %02d:%02d"), Minutes, Seconds)));
        if (Text_Defeats) Text_Defeats->SetText(FText::Format(NSLOCTEXT("CVAD", "ResultDefeats", "击破敌军 {0}"), Defeats));
        if (Text_BossResult) Text_BossResult->SetText(bWon ? NSLOCTEXT("CVAD", "BossDestroyed", "外星机械统领已摧毁") : NSLOCTEXT("CVAD", "BossSurvived", "外星机械统领仍在运作"));
        if (Text_ExperienceEarned) Text_ExperienceEarned->SetText(FText::Format(NSLOCTEXT("CVAD", "ResultXP", "本局经验 +{0}"), XP));
        if (bWon) SaveBattleResult();
    }
}

void UCVADMenuWidget::HandleSinglePlayerClicked() { StartSinglePlayer(); }
void UCVADMenuWidget::HandleHostClicked() { HostListenServer(); }
void UCVADMenuWidget::HandleJoinClicked()
{
    const FString Address = Input_ServerAddress ? Input_ServerAddress->GetText().ToString().TrimStartAndEnd() : TEXT("127.0.0.1");
    if (Text_Status) Text_Status->SetText(FText::Format(NSLOCTEXT("CVAD", "Connecting", "正在连接 {0}…"), FText::FromString(Address)));
    JoinServer(Address);
}
void UCVADMenuWidget::HandleLoadClicked()
{
    if (!UGameplayStatics::DoesSaveGameExist(TEXT("CVAD_Profile_0"), 0))
    {
        if (Text_Status) Text_Status->SetText(NSLOCTEXT("CVAD", "LoadFailed", "没有可读取的存档"));
        return;
    }
    if (Text_Status) Text_Status->SetText(NSLOCTEXT("CVAD", "LoadSuccess", "正在读取存档…"));
    UGameplayStatics::OpenLevel(this, TEXT("L_BattlePrototype"), true, TEXT("LoadProfile=1"));
}
void UCVADMenuWidget::HandleQuitClicked() { QuitGame(); }
void UCVADMenuWidget::HandleRetryClicked() { RetryBattle(); }
void UCVADMenuWidget::HandleReturnClicked() { ReturnToMainMenu(); }

void UCVADMenuWidget::StartSinglePlayer()
{
    UGameplayStatics::OpenLevel(this, TEXT("L_BattlePrototype"));
}

void UCVADMenuWidget::HostListenServer()
{
    UGameplayStatics::OpenLevel(this, TEXT("L_BattlePrototype"), true, TEXT("listen"));
}

void UCVADMenuWidget::JoinServer(const FString& Address)
{
    if (APlayerController* PC = GetOwningPlayer()) PC->ClientTravel(Address.IsEmpty() ? TEXT("127.0.0.1") : Address, TRAVEL_Absolute);
}

void UCVADMenuWidget::QuitGame()
{
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UCVADMenuWidget::RetryBattle()
{
    if (ACVADPlayerController* PC = Cast<ACVADPlayerController>(GetOwningPlayer())) PC->RequestRestartBattle();
    else UGameplayStatics::OpenLevel(this, TEXT("L_BattlePrototype"));
}
void UCVADMenuWidget::ReturnToMainMenu()
{
    if (ACVADPlayerController* PC = Cast<ACVADPlayerController>(GetOwningPlayer())) PC->RequestReturnToMainMenu();
    else UGameplayStatics::OpenLevel(this, TEXT("L_MainMenu"));
}

bool UCVADMenuWidget::GetBattleResultData(bool& bOutVictory, float& CompletionTime,
    int32& Defeats, int32& ExperienceReward) const
{
    if (!GetWorld()) return false;
    for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It)
    {
        bOutVictory = It->bVictory; CompletionTime = It->CompletionTimeSeconds;
        Defeats = It->DefeatCount; ExperienceReward = It->ExperienceEarned;
        return It->BattlePhase == ECVADBattlePhase::Results;
    }
    return false;
}

bool UCVADMenuWidget::SaveBattleResult()
{
    bool bWon = false; float Time = 0.f; int32 Defeats = 0; int32 XP = 0;
    if (!GetBattleResultData(bWon, Time, Defeats, XP) || !bWon) return false;
    UCVADSaveGame* Save = Cast<UCVADSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("CVAD_Profile_0"), 0));
    if (!Save) Save = Cast<UCVADSaveGame>(UGameplayStatics::CreateSaveGameObject(UCVADSaveGame::StaticClass()));
    if (!Save) return false;
    Save->bHasCompletedDemo = true;
    if (Save->BestCompletionTimeSeconds <= 0.f || Time < Save->BestCompletionTimeSeconds) Save->BestCompletionTimeSeconds = Time;
    Save->BestTeamDetonationCount = FMath::Max(Save->BestTeamDetonationCount, Defeats);
    return UGameplayStatics::SaveGameToSlot(Save, TEXT("CVAD_Profile_0"), 0);
}

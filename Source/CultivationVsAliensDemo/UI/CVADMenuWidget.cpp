#include "UI/CVADMenuWidget.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"
#include "GameFramework/PlayerController.h"
#include "Player/CVADPlayerController.h"
#include "Character/CVADCharacter.h"
#include "Battle/CVADBattleDirector.h"
#include "Save/CVADSaveGame.h"
#include "EngineUtils.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Player/CVADPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "Misc/ConfigCacheIni.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "HAL/PlatformApplicationMisc.h"
#include "SocketSubsystem.h"
#include "IPAddress.h"

void UCVADMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();
    if (Button_SinglePlayer) Button_SinglePlayer->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSinglePlayerClicked);
    if (Button_HostListenServer) Button_HostListenServer->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleHostClicked);
    if (Button_JoinGame) Button_JoinGame->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleJoinClicked);
    if (Button_LoadGame) Button_LoadGame->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLoadClicked);
    if (Button_Quit) Button_Quit->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleQuitClicked);
    if (Button_Settings) Button_Settings->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSettingsClicked);
    if (Button_CustomKeybindings) Button_CustomKeybindings->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCustomKeybindingsClicked);
    if (Button_ChangeName) Button_ChangeName->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleChangeNameClicked);
    if (Button_Skills) Button_Skills->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSkillsClicked);
    if (Button_Outfit) Button_Outfit->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOutfitClicked);
    if (Button_Multiplayer) Button_Multiplayer->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleMultiplayerClicked);
    if (Button_Ready) Button_Ready->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleReadyClicked);
    if (Button_StartGame) Button_StartGame->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleStartGameClicked);
    if (Button_LeaveLobby) Button_LeaveLobby->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleLeaveLobbyClicked);
    if (Button_CopyAddress) Button_CopyAddress->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleCopyAddressClicked);
    if (Button_Retry) Button_Retry->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRetryClicked);
    if (Button_ReturnLobby) Button_ReturnLobby->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReturnClicked);
    if (Button_ReturnMainMenu) Button_ReturnMainMenu->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleReturnClicked);
    bool bWon = false; float Time = 0.f; int32 Defeats = 0; int32 XP = 0;
    const ACVADCharacter* Character=Cast<ACVADCharacter>(GetOwningPlayerPawn());
    const bool bPlayerCurrentlyDown=Character && Character->IsPlayerDown();
    if (bPlayerCurrentlyDown)
    {
        if(Text_ResultTitle) Text_ResultTitle->SetText(NSLOCTEXT("CVAD","PlayerDead","你已死亡"));
        if(Text_ClearTime) Text_ClearTime->SetText(NSLOCTEXT("CVAD","DeathChoice","选择复活将重新开始本次战斗"));
        if(Text_Defeats) Text_Defeats->SetText(FText::GetEmpty());
        if(Text_BossResult) Text_BossResult->SetText(FText::GetEmpty());
        if(Text_ExperienceEarned) Text_ExperienceEarned->SetText(FText::GetEmpty());
        if(UTextBlock* RetryLabel=Cast<UTextBlock>(GetWidgetFromName(TEXT("Label_Button_Retry")))) RetryLabel->SetText(NSLOCTEXT("CVAD","Respawn","复活"));
    }
    else if (GetBattleResultData(bWon, Time, Defeats, XP))
    {
        if (Text_ResultTitle) Text_ResultTitle->SetText(bWon ? NSLOCTEXT("CVAD", "ResultVictory", "修仙者胜利") : NSLOCTEXT("CVAD", "ResultDefeat", "战斗失败"));
        const int32 Minutes = FMath::FloorToInt(Time / 60.f);
        const int32 Seconds = FMath::FloorToInt(FMath::Fmod(Time, 60.f));
        if (Text_ClearTime) Text_ClearTime->SetText(FText::FromString(FString::Printf(TEXT("战斗时间 %02d:%02d"), Minutes, Seconds)));
        if (Text_Defeats) Text_Defeats->SetText(FText::Format(NSLOCTEXT("CVAD", "ResultDefeats", "击破敌军 {0}"), Defeats));
        if (Text_BossResult) Text_BossResult->SetText(bWon ? NSLOCTEXT("CVAD", "BossDestroyed", "天穹三使已全部击败") : NSLOCTEXT("CVAD", "BossSurvived", "天穹三使仍镇守战场"));
        if (Text_ExperienceEarned) Text_ExperienceEarned->SetText(FText::Format(NSLOCTEXT("CVAD", "ResultXP", "本局经验 +{0}"), XP));
        if (bWon) SaveBattleResult();
    }
    if(Text_Status && GConfig)
    {
        FString LastError;
        if(GConfig->GetString(TEXT("CVAD.Network"),TEXT("LastError"),LastError,GGameUserSettingsIni)&&!LastError.IsEmpty())
        {Text_Status->SetText(FText::FromString(LastError));GConfig->RemoveKey(TEXT("CVAD.Network"),TEXT("LastError"),GGameUserSettingsIni);GConfig->Flush(false,GGameUserSettingsIni);}
    }
    if (Text_PlayerName)
        Text_PlayerName->SetText(FText::FromString(FString::Printf(TEXT("玩家：%s"), *GetConfiguredPlayerName())));
}

void UCVADMenuWidget::NativeTick(const FGeometry& MyGeometry,float InDeltaTime)
{
    Super::NativeTick(MyGeometry,InDeltaTime);
    if (Text_PlayerName)
    {
        const FString CurrentName = GetConfiguredPlayerName();
        if (CurrentName != DisplayedPlayerName)
        {
            Text_PlayerName->SetText(FText::FromString(FString::Printf(TEXT("玩家：%s"), *CurrentName)));
            DisplayedPlayerName = CurrentName;
        }
    }
    LobbyRefreshAccumulator+=InDeltaTime;
    if(LobbyRefreshAccumulator>=0.25f){LobbyRefreshAccumulator=0.f;RefreshLobbyDisplay();}
}

void UCVADMenuWidget::HandleSinglePlayerClicked() { if(EnsureConfiguredPlayerName(1)) StartSinglePlayer(); }
void UCVADMenuWidget::HandleHostClicked() { if(EnsureConfiguredPlayerName(2)) HostListenServer(); }
void UCVADMenuWidget::HandleJoinClicked()
{
    const FString Address = Input_ServerAddress ? Input_ServerAddress->GetText().ToString().TrimStartAndEnd() : TEXT("127.0.0.1");
    if(!EnsureConfiguredPlayerName(3,Address)) return;
    if (Text_Status) Text_Status->SetText(FText::Format(NSLOCTEXT("CVAD", "Connecting", "正在连接 {0}…"), FText::FromString(Address)));
    JoinServer(Address);
}
void UCVADMenuWidget::HandleLoadClicked()
{
    if (ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer())) PC->ShowSaveSlotsScreen();
}
void UCVADMenuWidget::HandleQuitClicked() { QuitGame(); }
void UCVADMenuWidget::HandleSettingsClicked() { if (ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer())) PC->ShowSettingsScreen(); }
void UCVADMenuWidget::HandleCustomKeybindingsClicked() { if (ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer())) PC->ShowCustomKeybindingsScreen(); }
void UCVADMenuWidget::HandleChangeNameClicked() { if (ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer())) PC->ShowNameEntryScreen(); }
void UCVADMenuWidget::HandleSkillsClicked() { if (ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer())) PC->ShowSkillTreeScreen(); }
void UCVADMenuWidget::HandleOutfitClicked() { if (ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer())) PC->ShowOutfitScreen(); }
void UCVADMenuWidget::HandleMultiplayerClicked() { if (ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer())) PC->ShowMultiplayerScreen(); }
void UCVADMenuWidget::HandleReadyClicked(){if(ACVADPlayerState* PS=GetOwningPlayerState<ACVADPlayerState>()) PS->SetLobbyReady(!PS->bLobbyReady);}
void UCVADMenuWidget::HandleStartGameClicked(){if(ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer())) PC->RequestStartLobbyGame();}
void UCVADMenuWidget::HandleLeaveLobbyClicked(){UGameplayStatics::OpenLevel(this,TEXT("L_MainMenu"));}
void UCVADMenuWidget::HandleCopyAddressClicked()
{
    FString HostIp=TEXT("127.0.0.1");
    if(ISocketSubsystem* Sockets=ISocketSubsystem::Get(PLATFORM_SOCKETSUBSYSTEM))
    {
        TArray<TSharedPtr<FInternetAddr>> Addresses;
        if(Sockets->GetLocalAdapterAddresses(Addresses))
            for(const TSharedPtr<FInternetAddr>& Candidate : Addresses)
            {
                const FString Ip=Candidate.IsValid()?Candidate->ToString(false):TEXT("");
                if(!Ip.IsEmpty() && !Ip.StartsWith(TEXT("127.")) && !Ip.StartsWith(TEXT("169.254.")) && Ip.Contains(TEXT(".")))
                {HostIp=Ip;break;}
            }
    }
    const FString Address=FString::Printf(TEXT("%s:%d"),*HostIp,GetWorld()?GetWorld()->URL.Port:7777);
    FPlatformApplicationMisc::ClipboardCopy(*Address);
    if(UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_ConnectionStatus")))) T->SetText(FText::FromString(TEXT("已复制地址：")+Address));
    UE_LOG(LogTemp,Log,TEXT("Copied listen server address %s"),*Address);
}

void UCVADMenuWidget::RefreshLobbyDisplay()
{
    AGameStateBase* GS=GetWorld()?GetWorld()->GetGameState():nullptr; if(!GS) return;
    const TArray<TObjectPtr<APlayerState>>& Players=GS->PlayerArray;
    auto FormatPlayer=[](const APlayerState* Base,int32 Index)->FText
    { const ACVADPlayerState* PS=Cast<ACVADPlayerState>(Base); return PS?FText::FromString(FString::Printf(TEXT("%s%s  %s"),Index==0?TEXT("[主机] "):TEXT(""),*PS->GetPlayerName(),PS->bLobbyReady?TEXT("已准备"):TEXT("未准备"))):FText::FromString(TEXT("等待玩家…")); };
    if(UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Player1")))) T->SetText(FormatPlayer(Players.IsValidIndex(0)?Players[0]:nullptr,0));
    if(UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Player2")))) T->SetText(FormatPlayer(Players.IsValidIndex(1)?Players[1]:nullptr,1));
    if(UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_ConnectionStatus")))) T->SetText(FText::FromString(FString::Printf(TEXT("已连接 %d/2"),Players.Num())));
    if(UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_HostName"))))
        T->SetText(FText::FromString(FString::Printf(TEXT("主机地址：127.0.0.1:%d"),GetWorld()?GetWorld()->URL.Port:7777)));
    if(ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer())) if(Button_StartGame) Button_StartGame->SetIsEnabled(PC->IsLobbyHost());
}

FString UCVADMenuWidget::GetConfiguredPlayerName() const
{
    FString Name=TEXT("Player");
    if(GConfig) GConfig->GetString(TEXT("CVAD.Profile"),TEXT("PlayerName"),Name,GGameUserSettingsIni);
    Name=Name.TrimStartAndEnd().Left(20);
    return Name.Len()>=1?Name:TEXT("Player");
}

bool UCVADMenuWidget::EnsureConfiguredPlayerName(int32 PendingAction,const FString& Address)
{
    FString Name;
    if(GConfig) GConfig->GetString(TEXT("CVAD.Profile"),TEXT("PlayerName"),Name,GGameUserSettingsIni);
    FText Failure;
    if(ValidatePlayerName(Name,Failure)) return true;
    if(Text_Status) Text_Status->SetText(FText::FromString(TEXT("请先设置角色名称，再次点击即可开始")));
    if(ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer())) PC->SetPendingMenuAction(PendingAction,Address);
    return false;
}

FString UCVADMenuWidget::BuildPlayerTravelOptions(const FString& BaseOptions) const
{
    FString Result=BaseOptions;
    if(!Result.IsEmpty() && !Result.EndsWith(TEXT("?"))) Result+=TEXT("?");
    Result+=TEXT("PlayerName=")+FGenericPlatformHttp::UrlEncode(GetConfiguredPlayerName());
    return Result;
}
void UCVADMenuWidget::HandleRetryClicked() { RetryBattle(); }
void UCVADMenuWidget::HandleReturnClicked() { ReturnToMainMenu(); }

void UCVADMenuWidget::StartSinglePlayer()
{
    UGameplayStatics::OpenLevel(this, TEXT("L_CastleBattle"),true,BuildPlayerTravelOptions());
}

void UCVADMenuWidget::HostListenServer()
{
    UGameplayStatics::OpenLevel(this, TEXT("L_MainMenu"), true, BuildPlayerTravelOptions(TEXT("listen?Lobby=1")));
}

void UCVADMenuWidget::JoinServer(const FString& Address)
{
    const FString Target=(Address.IsEmpty()?TEXT("127.0.0.1"):Address)+TEXT("?")+BuildPlayerTravelOptions();
    if (APlayerController* PC = GetOwningPlayer()) PC->ClientTravel(Target, TRAVEL_Absolute);
}

void UCVADMenuWidget::QuitGame()
{
    UKismetSystemLibrary::QuitGame(this, GetOwningPlayer(), EQuitPreference::Quit, false);
}

void UCVADMenuWidget::RetryBattle()
{
    if (ACVADPlayerController* PC = Cast<ACVADPlayerController>(GetOwningPlayer())) PC->RequestRestartBattle();
    else UGameplayStatics::OpenLevel(this, TEXT("L_CastleBattle"));
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
    // Persist the authoritative local player's level, skill loadout and equipment
    // before adding run statistics to the same profile slot.
    SaveProfileToSlot(GetLastUsedProfileSlot());
    const FString SlotName = GetProfileSlotName(GetLastUsedProfileSlot());
    UCVADSaveGame* Save = nullptr;
    if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
    {
        Save = Cast<UCVADSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
    }
    if (!Save) Save = Cast<UCVADSaveGame>(UGameplayStatics::CreateSaveGameObject(UCVADSaveGame::StaticClass()));
    if (!Save) return false;
    Save->bHasCompletedDemo = true;
    if (Save->BestCompletionTimeSeconds <= 0.f || Time < Save->BestCompletionTimeSeconds) Save->BestCompletionTimeSeconds = Time;
    Save->BestTeamDetonationCount = FMath::Max(Save->BestTeamDetonationCount, Defeats);
    return UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
}

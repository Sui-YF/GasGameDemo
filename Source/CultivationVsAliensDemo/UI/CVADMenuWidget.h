#pragma once

#include "UI/CVADUserWidget.h"
#include "CVADMenuWidget.generated.h"

class UButton;
class UEditableTextBox;
class UTextBlock;

UCLASS(Blueprintable)
class CULTIVATIONVSALIENSDEMO_API UCVADMenuWidget : public UCVADUserWidget
{
    GENERATED_BODY()
public:
    virtual void NativeConstruct() override;
    virtual void NativeTick(const FGeometry& MyGeometry, float InDeltaTime) override;
    UFUNCTION(BlueprintCallable, Category="Menu") void StartSinglePlayer();
    UFUNCTION(BlueprintCallable, Category="Menu") void HostListenServer();
    UFUNCTION(BlueprintCallable, Category="Menu") void JoinServer(const FString& Address);
    UFUNCTION(BlueprintCallable, Category="Menu") void QuitGame();
    UFUNCTION(BlueprintCallable, Category="Menu") void RetryBattle();
    UFUNCTION(BlueprintCallable, Category="Menu") void ReturnToMainMenu();
    UFUNCTION(BlueprintPure, Category="Menu|Results") bool GetBattleResultData(bool& bVictory, float& CompletionTime,
        int32& Defeats, int32& ExperienceReward) const;
    UFUNCTION(BlueprintCallable, Category="Menu|Results") bool SaveBattleResult();

protected:
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_SinglePlayer;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_HostListenServer;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_JoinGame;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_LoadGame;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_Quit;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_Settings;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_CustomKeybindings;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_ChangeName;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_Skills;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_Outfit;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_Multiplayer;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_Ready;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_StartGame;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_LeaveLobby;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_CopyAddress;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_Retry;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_ReturnLobby;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UButton> Button_ReturnMainMenu;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UEditableTextBox> Input_ServerAddress;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Status;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_PlayerName;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ResultTitle;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ClearTime;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_Defeats;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_BossResult;
    UPROPERTY(meta=(BindWidgetOptional)) TObjectPtr<UTextBlock> Text_ExperienceEarned;

private:
    UFUNCTION() void HandleSinglePlayerClicked();
    UFUNCTION() void HandleHostClicked();
    UFUNCTION() void HandleJoinClicked();
    UFUNCTION() void HandleLoadClicked();
    UFUNCTION() void HandleQuitClicked();
    UFUNCTION() void HandleSettingsClicked();
    UFUNCTION() void HandleCustomKeybindingsClicked();
    UFUNCTION() void HandleChangeNameClicked();
    UFUNCTION() void HandleSkillsClicked();
    UFUNCTION() void HandleOutfitClicked();
    UFUNCTION() void HandleMultiplayerClicked();
    UFUNCTION() void HandleReadyClicked();
    UFUNCTION() void HandleStartGameClicked();
    UFUNCTION() void HandleLeaveLobbyClicked();
    UFUNCTION() void HandleCopyAddressClicked();
    void RefreshLobbyDisplay();
    FString GetConfiguredPlayerName() const;
    FString BuildPlayerTravelOptions(const FString& BaseOptions = TEXT("")) const;
    bool EnsureConfiguredPlayerName(int32 PendingAction, const FString& Address = FString());
    float LobbyRefreshAccumulator = 0.f;
    FString DisplayedPlayerName;
    UFUNCTION() void HandleRetryClicked();
    UFUNCTION() void HandleReturnClicked();
};

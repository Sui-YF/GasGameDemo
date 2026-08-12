#include "UI/CVADUserWidget.h"
#include "Player/CVADPlayerState.h"
#include "Inventory/CVADInventoryComponent.h"
#include "GameFramework/GameUserSettings.h"
#include "Kismet/GameplayStatics.h"
#include "GameFramework/PlayerController.h"
#include "Save/CVADSaveGame.h"
#include "AbilitySystem/Abilities/CVADCombatAbility.h"
#include "Data/CVADSkillRows.h"
#include "Engine/DataTable.h"
#include "GameplayTagsManager.h"
#include "AbilitySystemComponent.h"
#include "GameplayEffect.h"
#include "AudioDevice.h"
#include "Misc/ConfigCacheIni.h"
#include "Components/Button.h"
#include "Player/CVADPlayerController.h"

static const TCHAR* CVADAudioConfigSection = TEXT("CVAD.AudioSettings");

void UCVADUserWidget::NativeConstruct()
{
    Super::NativeConstruct();
    InitializeFromOwningPlayer();
    if (!GetClass()->GetName().Contains(TEXT("WBP_Pause"))) return;
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_Resume")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePauseResumeClicked);
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_SaveGame")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePauseSaveClicked);
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_LoadGame")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePauseLoadClicked);
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_ReturnMainMenu")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePauseReturnMenuClicked);
}

void UCVADUserWidget::HandlePauseResumeClicked() { ResumeGame(); }
void UCVADUserWidget::HandlePauseSaveClicked() { SaveProfile(); }
void UCVADUserWidget::HandlePauseLoadClicked() { LoadProfile(); }
void UCVADUserWidget::HandlePauseReturnMenuClicked()
{
    if (ACVADPlayerController* PC = Cast<ACVADPlayerController>(GetOwningPlayer())) PC->RequestReturnToMainMenu();
}

void UCVADUserWidget::InitializeFromOwningPlayer()
{
    CachedPlayerState = GetOwningPlayerState<ACVADPlayerState>();
    if (CachedPlayerState) OnPlayerDataReady();
}

void UCVADUserWidget::CloseScreen()
{
    RemoveFromParent();
    if (APlayerController* PC = GetOwningPlayer())
    {
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
    }
}

void UCVADUserWidget::ResumeGame()
{
    if (APlayerController* PC = GetOwningPlayer())
    {
        if (GetWorld() && GetWorld()->GetNetMode() == NM_Standalone) UGameplayStatics::SetGamePaused(this, false);
        PC->SetInputMode(FInputModeGameOnly());
        PC->bShowMouseCursor = false;
    }
    RemoveFromParent();
}

void UCVADUserWidget::ApplyVideoSettings(int32 ResolutionScale, int32 Quality, bool bFullscreen, bool bVSync)
{
    if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
    {
        Settings->SetResolutionScaleValueEx(FMath::Clamp(ResolutionScale, 50, 100));
        Settings->SetOverallScalabilityLevel(FMath::Clamp(Quality, 0, 4));
        Settings->SetFullscreenMode(bFullscreen ? EWindowMode::Fullscreen : EWindowMode::Windowed);
        Settings->SetVSyncEnabled(bVSync);
        Settings->ApplySettings(false);
        Settings->SaveSettings();
    }
}

void UCVADUserWidget::ApplyAudioSettings(float MasterVolume, float MusicVolume, float SFXVolume)
{
    MasterVolume = FMath::Clamp(MasterVolume, 0.f, 1.f);
    MusicVolume = FMath::Clamp(MusicVolume, 0.f, 1.f);
    SFXVolume = FMath::Clamp(SFXVolume, 0.f, 1.f);
    if (GEngine)
        if (FAudioDevice* AudioDevice = GEngine->GetMainAudioDeviceRaw()) AudioDevice->SetTransientPrimaryVolume(MasterVolume);
    if (GConfig)
    {
        GConfig->SetFloat(CVADAudioConfigSection, TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
        GConfig->SetFloat(CVADAudioConfigSection, TEXT("MusicVolume"), MusicVolume, GGameUserSettingsIni);
        GConfig->SetFloat(CVADAudioConfigSection, TEXT("SFXVolume"), SFXVolume, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }
    UE_LOG(LogTemp, Log, TEXT("Audio settings Master=%.2f Music=%.2f SFX=%.2f"), MasterVolume, MusicVolume, SFXVolume);
}

void UCVADUserWidget::GetSavedAudioSettings(float& MasterVolume, float& MusicVolume, float& SFXVolume) const
{
    MasterVolume = MusicVolume = SFXVolume = 1.f;
    if (!GConfig) return;
    GConfig->GetFloat(CVADAudioConfigSection, TEXT("MasterVolume"), MasterVolume, GGameUserSettingsIni);
    GConfig->GetFloat(CVADAudioConfigSection, TEXT("MusicVolume"), MusicVolume, GGameUserSettingsIni);
    GConfig->GetFloat(CVADAudioConfigSection, TEXT("SFXVolume"), SFXVolume, GGameUserSettingsIni);
}

void UCVADUserWidget::SubmitPlayerName(const FString& NewName)
{
    if (APlayerController* PC = GetOwningPlayer()) PC->ServerChangeName(NewName.Left(20));
}

void UCVADUserWidget::SpendSkillPoint(FName SkillRowName)
{
    if (!CachedPlayerState) InitializeFromOwningPlayer();
    if (CachedPlayerState) CachedPlayerState->RequestSpendSkillPoint(SkillRowName);
}

bool UCVADUserWidget::GetSkillNodeInfo(FName SkillRowName, FText& DisplayName, FText& Description,
    int32& RequiredLevel, int32& SkillPointCost, FName& PrerequisiteSkill, bool& bUnlocked,
    bool& bCanUnlock, FText& FailureReason) const
{
    UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/CVAD/Data/DT_Skills.DT_Skills"));
    const FCVADSkillRow* Row = Table ? Table->FindRow<FCVADSkillRow>(SkillRowName, TEXT("SkillWidget")) : nullptr;
    if (!Row) return false;
    DisplayName=Row->DisplayName; Description=Row->Description; RequiredLevel=Row->RequiredLevel;
    SkillPointCost=Row->SkillPointCost; PrerequisiteSkill=Row->PrerequisiteSkill;
    bUnlocked = CachedPlayerState && CachedPlayerState->IsSkillUnlocked(SkillRowName);
    bCanUnlock = CachedPlayerState && CachedPlayerState->CanUnlockSkill(SkillRowName, FailureReason);
    return true;
}

float UCVADUserWidget::GetCooldownRemainingForSlot(int32 AbilitySlotIndex) const
{
    if (!CachedPlayerState || !CachedPlayerState->GetAbilitySystemComponent()) return 0.f;
    static const TCHAR* Tags[] = {TEXT("Cooldown.Attack.Light"),TEXT("Cooldown.Attack.Heavy"),TEXT("Cooldown.Dodge"),TEXT("Cooldown.FlyingSword"),TEXT("Cooldown.Stance")};
    if (AbilitySlotIndex < 0 || AbilitySlotIndex >= UE_ARRAY_COUNT(Tags)) return 0.f;
    FGameplayTagContainer QueryTags; QueryTags.AddTag(UGameplayTagsManager::Get().RequestGameplayTag(Tags[AbilitySlotIndex]));
    const FGameplayEffectQuery Query = FGameplayEffectQuery::MakeQuery_MatchAnyOwningTags(QueryTags);
    const TArray<TPair<float, float>> Cooldowns =
        CachedPlayerState->GetAbilitySystemComponent()->GetActiveEffectsTimeRemainingAndDuration(Query);
    float LongestRemaining = 0.f;
    for (const TPair<float, float>& Cooldown : Cooldowns)
    {
        LongestRemaining = FMath::Max(LongestRemaining, Cooldown.Key);
    }
    return LongestRemaining;
}

bool UCVADUserWidget::SaveProfile()
{
    if (!CachedPlayerState) InitializeFromOwningPlayer();
    if (!CachedPlayerState) return false;
    UCVADSaveGame* Save = Cast<UCVADSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("CVAD_Profile_0"), 0));
    if (!Save) Save = Cast<UCVADSaveGame>(UGameplayStatics::CreateSaveGameObject(UCVADSaveGame::StaticClass()));
    if (!Save) return false;
    Save->PlayerDisplayName = CachedPlayerState->GetPlayerName();
    Save->PlayerLevel = CachedPlayerState->PlayerLevel;
    Save->Experience = CachedPlayerState->Experience;
    Save->SkillPoints = CachedPlayerState->SkillPoints;
    Save->EquippedSkillRows = CachedPlayerState->EquippedSkillRows;
    Save->UnlockedSkillRows = CachedPlayerState->UnlockedSkillRows;
    if (UCVADInventoryComponent* Inventory = GetInventory())
    {
        Save->EquipmentLoadout = Inventory->GetEquipmentLoadout();
        Save->UnlockedItemIds = Inventory->GetOwnedItemIds();
    }
    return UGameplayStatics::SaveGameToSlot(Save, TEXT("CVAD_Profile_0"), 0);
}

bool UCVADUserWidget::LoadProfile()
{
    UCVADSaveGame* Save = Cast<UCVADSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("CVAD_Profile_0"), 0));
    if (!Save) return false;
    if (APlayerController* PC = GetOwningPlayer()) PC->ServerChangeName(Save->PlayerDisplayName);
    if (!CachedPlayerState) InitializeFromOwningPlayer();
    if (CachedPlayerState)
    {
        CachedPlayerState->RequestRestoreProfile(Save->PlayerLevel, Save->Experience, Save->SkillPoints,
            Save->UnlockedSkillRows, Save->EquippedSkillRows, Save->UnlockedItemIds, Save->EquipmentLoadout);
    }
    return true;
}

UCVADInventoryComponent* UCVADUserWidget::GetInventory() const
{
    return CachedPlayerState ? CachedPlayerState->GetInventoryComponent() : nullptr;
}

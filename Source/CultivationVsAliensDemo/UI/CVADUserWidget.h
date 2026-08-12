#pragma once

#include "Blueprint/UserWidget.h"
#include "CVADUserWidget.generated.h"

class ACVADPlayerState;
class UCVADInventoryComponent;
class UButton;

UCLASS(Abstract, Blueprintable)
class CULTIVATIONVSALIENSDEMO_API UCVADUserWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    UFUNCTION(BlueprintCallable, Category="UI") void InitializeFromOwningPlayer();
    UFUNCTION(BlueprintPure, Category="UI") ACVADPlayerState* GetCVADPlayerState() const { return CachedPlayerState; }
    UFUNCTION(BlueprintPure, Category="UI") UCVADInventoryComponent* GetInventory() const;
    UFUNCTION(BlueprintCallable, Category="UI|Navigation") void CloseScreen();
    UFUNCTION(BlueprintCallable, Category="UI|Navigation") void ResumeGame();
    UFUNCTION(BlueprintCallable, Category="UI|Settings") void ApplyVideoSettings(int32 ResolutionScale, int32 Quality, bool bFullscreen, bool bVSync);
    UFUNCTION(BlueprintCallable, Category="UI|Settings") void ApplyAudioSettings(float MasterVolume, float MusicVolume, float SFXVolume);
    UFUNCTION(BlueprintPure, Category="UI|Settings") void GetSavedAudioSettings(float& MasterVolume, float& MusicVolume, float& SFXVolume) const;
    UFUNCTION(BlueprintCallable, Category="UI|Profile") void SubmitPlayerName(const FString& NewName);
    UFUNCTION(BlueprintCallable, Category="UI|Skills") void SpendSkillPoint(FName SkillRowName);
    UFUNCTION(BlueprintPure, Category="UI|Skills") bool GetSkillNodeInfo(FName SkillRowName, FText& DisplayName,
        FText& Description, int32& RequiredLevel, int32& SkillPointCost, FName& PrerequisiteSkill,
        bool& bUnlocked, bool& bCanUnlock, FText& FailureReason) const;
    UFUNCTION(BlueprintPure, Category="UI|Skills") float GetCooldownRemainingForSlot(int32 AbilitySlotIndex) const;
    UFUNCTION(BlueprintCallable, Category="UI|Save") bool SaveProfile();
    UFUNCTION(BlueprintCallable, Category="UI|Save") bool LoadProfile();

protected:
    UPROPERTY(BlueprintReadOnly, Category="UI") TObjectPtr<ACVADPlayerState> CachedPlayerState;

    UFUNCTION(BlueprintImplementableEvent, Category="UI") void OnPlayerDataReady();
private:
    UFUNCTION() void HandlePauseResumeClicked();
    UFUNCTION() void HandlePauseSaveClicked();
    UFUNCTION() void HandlePauseLoadClicked();
    UFUNCTION() void HandlePauseReturnMenuClicked();
};

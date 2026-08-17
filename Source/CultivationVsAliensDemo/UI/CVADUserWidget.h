#pragma once

#include "Blueprint/UserWidget.h"
#include "CVADUserWidget.generated.h"

class ACVADPlayerState;
class UCVADInventoryComponent;
class UButton;
class ACVADCharacter;

UCLASS(Abstract, Blueprintable)
class CULTIVATIONVSALIENSDEMO_API UCVADUserWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    virtual void NativeConstruct() override;
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;
    UFUNCTION(BlueprintCallable, Category="UI") void InitializeFromOwningPlayer();
    UFUNCTION(BlueprintPure, Category="UI") ACVADPlayerState* GetCVADPlayerState() const { return CachedPlayerState; }
    UFUNCTION(BlueprintPure, Category="UI") UCVADInventoryComponent* GetInventory() const;
    UFUNCTION(BlueprintCallable, Category="UI|Navigation") void CloseScreen();
    UFUNCTION(BlueprintCallable, Category="UI|Navigation") void ResumeGame();
    UFUNCTION(BlueprintCallable, Category="UI|Settings") void ApplyVideoSettings(int32 ResolutionScale, int32 Quality, bool bFullscreen, bool bVSync);
    UFUNCTION(BlueprintCallable, Category="UI|Settings") void ApplyAudioSettings(float MasterVolume, float MusicVolume, float SFXVolume);
    UFUNCTION(BlueprintPure, Category="UI|Settings") void GetSavedAudioSettings(float& MasterVolume, float& MusicVolume, float& SFXVolume) const;
    UFUNCTION(BlueprintCallable, Category="UI|Profile") void SubmitPlayerName(const FString& NewName);
    UFUNCTION(BlueprintPure, Category="UI|Profile") bool ValidatePlayerName(const FString& NewName, FText& FailureReason) const;
    UFUNCTION(BlueprintCallable, Category="UI|Skills") void SpendSkillPoint(FName SkillRowName);
    UFUNCTION(BlueprintPure, Category="UI|Skills") bool GetSkillNodeInfo(FName SkillRowName, FText& DisplayName,
        FText& Description, int32& RequiredLevel, int32& SkillPointCost, FName& PrerequisiteSkill,
        bool& bUnlocked, bool& bCanUnlock, FText& FailureReason) const;
    UFUNCTION(BlueprintPure, Category="UI|Skills") float GetCooldownRemainingForSlot(int32 AbilitySlotIndex) const;
    UFUNCTION(BlueprintCallable, Category="UI|Save") bool SaveProfile();
    UFUNCTION(BlueprintCallable, Category="UI|Save") bool LoadProfile();
    UFUNCTION(BlueprintCallable, Category="UI|Save") bool SaveProfileToSlot(int32 SlotIndex);
    UFUNCTION(BlueprintCallable, Category="UI|Save") bool LoadProfileFromSlot(int32 SlotIndex);
    UFUNCTION(BlueprintCallable, Category="UI|Save") bool DeleteProfileSlot(int32 SlotIndex);
    UFUNCTION(BlueprintPure, Category="UI|Save") bool GetProfileSlotInfo(int32 SlotIndex, FString& PlayerName,
        int32& Level, FString& SavedAt, float& PlayTimeSeconds, bool& bCompleted) const;
    UFUNCTION(BlueprintPure, Category="UI|Save") static FString GetProfileSlotName(int32 SlotIndex);
    UFUNCTION(BlueprintPure, Category="UI|Save") static int32 GetLastUsedProfileSlot();

protected:
    UPROPERTY(BlueprintReadOnly, Category="UI") TObjectPtr<ACVADPlayerState> CachedPlayerState;

    /** Outfit preview camera, editable in WBP_OutfitSelect Class Defaults. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Outfit Preview") FVector OutfitPreviewCameraLocation = FVector(210.f, 0.f, 35.f);
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="UI|Outfit Preview") FRotator OutfitPreviewCameraRotation = FRotator(0.f, 180.f, 0.f);

    UFUNCTION(BlueprintImplementableEvent, Category="UI") void OnPlayerDataReady();
private:
    UFUNCTION() void HandlePauseResumeClicked();
    UFUNCTION() void HandlePauseSaveClicked();
    UFUNCTION() void HandlePauseLoadClicked();
    UFUNCTION() void HandlePauseReturnMenuClicked();
    UFUNCTION() void HandleOpenInventoryClicked();
    UFUNCTION() void HandleOpenSkillTreeClicked();
    UFUNCTION() void HandleOpenSettingsClicked();
    UFUNCTION() void HandleCloseClicked();
    UFUNCTION() void HandleConfirmNameClicked();
    UFUNCTION() void OutfitHeadPrev(); UFUNCTION() void OutfitHeadNext(); UFUNCTION() void OutfitHairPrev(); UFUNCTION() void OutfitHairNext();
    UFUNCTION() void OutfitHatPrev(); UFUNCTION() void OutfitHatNext(); UFUNCTION() void OutfitUpperPrev(); UFUNCTION() void OutfitUpperNext();
    UFUNCTION() void OutfitHandsPrev(); UFUNCTION() void OutfitHandsNext(); UFUNCTION() void OutfitLowerPrev(); UFUNCTION() void OutfitLowerNext();
    UFUNCTION() void OutfitFeetPrev(); UFUNCTION() void OutfitFeetNext();
    UFUNCTION() void HandleOutfitConfirm();
    UFUNCTION() void OutfitCameraZoomIn();
    UFUNCTION() void OutfitCameraZoomOut();
    UFUNCTION() void OutfitCameraMoveUp();
    UFUNCTION() void OutfitCameraMoveDown();
    UFUNCTION() void OutfitCameraReset();
    void ApplyOutfitPreviewCamera(bool bSaveSettings);
    void RefreshOutfitPreview();
    UFUNCTION() void HandleSettingsApplyClicked();
    UFUNCTION() void HandleSettingsResetClicked();
    UFUNCTION() void CaptureMoveForward();
    UFUNCTION() void CaptureMoveBack();
    UFUNCTION() void CaptureMoveLeft();
    UFUNCTION() void CaptureMoveRight();
    UFUNCTION() void CaptureJump();
    UFUNCTION() void CaptureLightAttack();
    UFUNCTION() void CaptureHeavyAttack();
    UFUNCTION() void CaptureDodge();
    UFUNCTION() void CaptureFlyingSword();
    UFUNCTION() void CaptureSwitchStance();
    UFUNCTION() void SaveSlot0(); UFUNCTION() void SaveSlot1(); UFUNCTION() void SaveSlot2();
    UFUNCTION() void SaveSelectedSlot();
    UFUNCTION() void LoadSelectedSlot();
    UFUNCTION() void DeleteSelectedSlot();
    UFUNCTION() void SelectSwordAttack1(); UFUNCTION() void SelectSwordAttack2(); UFUNCTION() void SelectSwordAttack3();
    UFUNCTION() void SelectSwordAttack4(); UFUNCTION() void SelectSwordAttack5();
    UFUNCTION() void SelectFlyingSword1(); UFUNCTION() void SelectFlyingSword2(); UFUNCTION() void SelectFlyingSword3();
    UFUNCTION() void EquipSelectedSkill();
    UFUNCTION() void HandleSkillLoadoutChanged();
    void SelectSkill(FName SkillRowName);
    void RefreshSkillDetails();
    void BeginKeyCapture(FName ActionName);
    void InitializeSettingsControls();
    void RefreshKeyBindingLabels();
    void RefreshSaveSlotPreviews();
    void SelectSaveSlot(int32 SlotIndex);
    void SetSaveSlotPreviewText(int32 SlotIndex, class UTextBlock* TextWidget);
    FName PendingRebindAction;
    FName SelectedSkillRow;
    int32 SelectedSaveSlot = 0;
    int32 PreviewOutfitParts[7] = {0,0,0,0,0,0,0};
    UPROPERTY(Transient) TObjectPtr<ACVADCharacter> OutfitPreviewCharacter;
    void ChangeOutfitPart(int32 Part,int32 Direction);
};

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
#include "Components/EditableTextBox.h"
#include "GenericPlatform/GenericPlatformHttp.h"
#include "Components/Slider.h"
#include "Components/CheckBox.h"
#include "Components/Viewport.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Player/CVADPlayerController.h"
#include "Input/Reply.h"
#include "Components/Viewport.h"
#include "Character/CVADCharacter.h"

static const TCHAR* CVADAudioConfigSection = TEXT("CVAD.AudioSettings");
static const TCHAR* CVADSaveConfigSection = TEXT("CVAD.SaveSlots");

void UCVADUserWidget::NativeConstruct()
{
    Super::NativeConstruct();
    InitializeFromOwningPlayer();
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_Resume")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePauseResumeClicked);
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_SaveGame")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePauseSaveClicked);
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_LoadGame")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePauseLoadClicked);
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_ReturnMainMenu")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandlePauseReturnMenuClicked);
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_Inventory")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOpenInventoryClicked);
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_SkillTree")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOpenSkillTreeClicked);
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_Settings")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOpenSettingsClicked);
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_CustomKeybindings")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleOpenCustomKeybindingsClicked);
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_Close")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_Back")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_Cancel")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_CancelName")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCloseClicked);
    if (UButton* Button = Cast<UButton>(GetWidgetFromName(TEXT("Button_ConfirmName")))) Button->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirmNameClicked);
    #define BIND_OUTFIT(Name,Fn) if(UButton* B=Cast<UButton>(GetWidgetFromName(TEXT(Name)))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::Fn)
    BIND_OUTFIT("Button_HeadPrev",OutfitHeadPrev);BIND_OUTFIT("Button_HeadNext",OutfitHeadNext);BIND_OUTFIT("Button_HairPrev",OutfitHairPrev);BIND_OUTFIT("Button_HairNext",OutfitHairNext);
    BIND_OUTFIT("Button_HatPrev",OutfitHatPrev);BIND_OUTFIT("Button_HatNext",OutfitHatNext);BIND_OUTFIT("Button_UpperPrev",OutfitUpperPrev);BIND_OUTFIT("Button_UpperNext",OutfitUpperNext);
    BIND_OUTFIT("Button_HandsPrev",OutfitHandsPrev);BIND_OUTFIT("Button_HandsNext",OutfitHandsNext);BIND_OUTFIT("Button_LowerPrev",OutfitLowerPrev);BIND_OUTFIT("Button_LowerNext",OutfitLowerNext);
    BIND_OUTFIT("Button_FeetPrev",OutfitFeetPrev);BIND_OUTFIT("Button_FeetNext",OutfitFeetNext);
    BIND_OUTFIT("Button_CameraZoomIn",OutfitCameraZoomIn);BIND_OUTFIT("Button_CameraZoomOut",OutfitCameraZoomOut);
    BIND_OUTFIT("Button_CameraUp",OutfitCameraMoveUp);BIND_OUTFIT("Button_CameraDown",OutfitCameraMoveDown);BIND_OUTFIT("Button_CameraReset",OutfitCameraReset);
    #undef BIND_OUTFIT
    if (UButton* Button=Cast<UButton>(GetWidgetFromName(TEXT("Button_OutfitConfirm")))) Button->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleOutfitConfirm);
    if(GetClass()->GetName().Contains(TEXT("WBP_OutfitSelect")))
    {
        for(int32 I=0;I<7;++I)if(GConfig)GConfig->GetInt(TEXT("CVAD.Appearance"),*FString::Printf(TEXT("Part%d"),I),PreviewOutfitParts[I],GGameUserSettingsIni);
        if(GConfig)
        {
            float CameraDistance=static_cast<float>(OutfitPreviewCameraLocation.X);
            float CameraHeight=static_cast<float>(OutfitPreviewCameraLocation.Z);
            GConfig->GetFloat(TEXT("CVAD.OutfitPreview"),TEXT("CameraDistance"),CameraDistance,GGameUserSettingsIni);
            GConfig->GetFloat(TEXT("CVAD.OutfitPreview"),TEXT("CameraHeight"),CameraHeight,GGameUserSettingsIni);
            if(FMath::IsNearlyEqual(CameraDistance,140.f) && FMath::IsNearlyEqual(CameraHeight,92.f))
            {CameraDistance=210.f;CameraHeight=35.f;}
            else if(FMath::IsNearlyEqual(CameraHeight,78.f))
            {CameraHeight=35.f;}
            OutfitPreviewCameraLocation.X=CameraDistance;
            OutfitPreviewCameraLocation.Z=CameraHeight;
        }
        if(UViewport* Preview=Cast<UViewport>(GetWidgetFromName(TEXT("Viewport_OutfitPreview"))))
        {
            Preview->SetBackgroundColor(FLinearColor(0.01f,0.018f,0.035f,1.f));
            Preview->SetLightIntensity(1.0f);
            Preview->SetSkyIntensity(0.f);
            Preview->SetShowFlag(TEXT("PostProcessing"), false);
            Preview->SetShowFlag(TEXT("Bloom"), false);
            Preview->SetShowFlag(TEXT("DepthOfField"), false);
            Preview->SetShowFlag(TEXT("MotionBlur"), false);
            Preview->SetShowFlag(TEXT("Fog"), false);
            Preview->SetShowFlag(TEXT("DynamicShadows"), false);
            Preview->SetViewLocation(OutfitPreviewCameraLocation); Preview->SetViewRotation(OutfitPreviewCameraRotation);
            TSubclassOf<ACVADCharacter> PreviewClass=LoadClass<ACVADCharacter>(nullptr,TEXT("/Game/CVAD/Blueprints/Characters/BP_LanfangCharacter.BP_LanfangCharacter_C"));
            UClass* SpawnClass=PreviewClass.Get() ? PreviewClass.Get() : ACVADCharacter::StaticClass();
            OutfitPreviewCharacter=Cast<ACVADCharacter>(Preview->Spawn(SpawnClass));
            if(OutfitPreviewCharacter) OutfitPreviewCharacter->ApplyAppearanceSelection(PreviewOutfitParts);
        }
        ApplyOutfitPreviewCamera(false);
        RefreshOutfitPreview();
    }
    if(UEditableTextBox* NameInput=Cast<UEditableTextBox>(GetWidgetFromName(TEXT("Input_PlayerName"))))
    {
        NameInput->SetIsReadOnly(false);
        NameInput->SetClearKeyboardFocusOnCommit(false);
        NameInput->SetSelectAllTextWhenFocused(false);
        NameInput->SetKeyboardFocus();
    }
    if (UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_SaveSlot0")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::SaveSlot0);
    if (UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_SaveSlot1")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::SaveSlot1);
    if (UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_SaveSlot2")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::SaveSlot2);
    if (UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_SaveSelected")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::SaveSelectedSlot);
    if (UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_LoadSelected")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::LoadSelectedSlot);
    if (UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_DeleteSelected")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::DeleteSelectedSlot);
    if (GetClass()->GetName().Contains(TEXT("WBP_SaveSlots"))) { SelectedSaveSlot=GetLastUsedProfileSlot(); RefreshSaveSlotPreviews(); }
    if (GetClass()->GetName().Contains(TEXT("WBP_SkillTree")))
    {
        if (UViewport* Preview = Cast<UViewport>(GetWidgetFromName(TEXT("Viewport_SkillPreview"))))
        {
            Preview->SetBackgroundColor(FLinearColor(0.01f, 0.018f, 0.035f, 1.f));
            Preview->SetShowFlag(TEXT("PostProcessing"), false);
            Preview->SetShowFlag(TEXT("Bloom"), false);
            Preview->SetShowFlag(TEXT("DepthOfField"), false);
            TSubclassOf<ACVADCharacter> PreviewClass = LoadClass<ACVADCharacter>(
                nullptr, TEXT("/Game/CVAD/Blueprints/Characters/BP_LanfangCharacter.BP_LanfangCharacter_C"));
            SkillPreviewCharacter = Cast<ACVADCharacter>(Preview->Spawn(PreviewClass.Get() ? PreviewClass.Get() : ACVADCharacter::StaticClass()));
            if (SkillPreviewCharacter) SkillPreviewCharacter->ApplyAppearanceSelection(PreviewOutfitParts);
        }
        if(UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_SwordAttack1")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::SelectSwordAttack1);
        if(UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_SwordAttack2")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::SelectSwordAttack2);
        if(UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_SwordAttack3")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::SelectSwordAttack3);
        if(UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_SwordAttack4")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::SelectSwordAttack4);
        if(UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_SwordAttack5")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::SelectSwordAttack5);
        if(UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_FlyingSword1")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::SelectFlyingSword1);
        if(UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_FlyingSword2")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::SelectFlyingSword2);
        if(UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_FlyingSword3")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::SelectFlyingSword3);
        if(UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_EquipSelected")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::EquipSelectedSkill);
        if(CachedPlayerState) CachedPlayerState->OnSkillLoadoutChanged.AddUniqueDynamic(this,&ThisClass::HandleSkillLoadoutChanged);
        SelectedSkillRow=TEXT("SwordAttack1"); RefreshSkillDetails();
    }
    if (GetClass()->GetName().Contains(TEXT("WBP_Settings")) || GetClass()->GetName().Contains(TEXT("WBP_CustomKeybindings")))
    {
        if (UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_ResetBindings")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleSettingsResetClicked);
        if (UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_RebindMoveForward")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::CaptureMoveForward);
        if (UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_RebindMoveBack")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::CaptureMoveBack);
        if (UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_RebindMoveLeft")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::CaptureMoveLeft);
        if (UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_RebindMoveRight")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::CaptureMoveRight);
        if (UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_RebindJump")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::CaptureJump);
        if (UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_RebindLightAttack")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::CaptureLightAttack);
        if (UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_RebindHeavyAttack")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::CaptureHeavyAttack);
        if (UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_RebindDodge")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::CaptureDodge);
        if (UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_RebindFlyingSword")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::CaptureFlyingSword);
        if (UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_RebindSwitchStance")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::CaptureSwitchStance);
        if (GetClass()->GetName().Contains(TEXT("WBP_Settings")))
        {
            if (UButton* B=Cast<UButton>(GetWidgetFromName(TEXT("Button_Apply")))) B->OnClicked.AddUniqueDynamic(this,&ThisClass::HandleSettingsApplyClicked);
            InitializeSettingsControls();
        }
        else
        {
            RefreshKeyBindingLabels();
        }
        SetIsFocusable(true);
    }
}

void UCVADUserWidget::InitializeSettingsControls()
{
    if(UEditableTextBox* NameInput=Cast<UEditableTextBox>(GetWidgetFromName(TEXT("Input_PlayerName"))))
    {
        FString SavedName;
        if(GConfig) GConfig->GetString(TEXT("CVAD.Profile"),TEXT("PlayerName"),SavedName,GGameUserSettingsIni);
        if(SavedName.IsEmpty() && CachedPlayerState) SavedName=CachedPlayerState->GetPlayerName();
        NameInput->SetText(FText::FromString(SavedName));
    }
    float Master=1.f, Music=1.f, SFX=1.f; GetSavedAudioSettings(Master, Music, SFX);
    if (USlider* W=Cast<USlider>(GetWidgetFromName(TEXT("Slider_MasterVolume")))) W->SetValue(Master);
    if (USlider* W=Cast<USlider>(GetWidgetFromName(TEXT("Slider_MusicVolume")))) W->SetValue(Music);
    if (USlider* W=Cast<USlider>(GetWidgetFromName(TEXT("Slider_SFXVolume")))) W->SetValue(SFX);
    if (UGameUserSettings* S=GEngine ? GEngine->GetGameUserSettings() : nullptr)
    {
        if (USlider* W=Cast<USlider>(GetWidgetFromName(TEXT("Slider_ResolutionScale")))) W->SetValue(S->GetResolutionScaleNormalized());
        if (UCheckBox* W=Cast<UCheckBox>(GetWidgetFromName(TEXT("Check_Fullscreen")))) W->SetIsChecked(S->GetFullscreenMode()!=EWindowMode::Windowed);
        if (UCheckBox* W=Cast<UCheckBox>(GetWidgetFromName(TEXT("Check_VSync")))) W->SetIsChecked(S->IsVSyncEnabled());
        if (UComboBoxString* W=Cast<UComboBoxString>(GetWidgetFromName(TEXT("Combo_Quality"))))
        { if (W->GetOptionCount()==0) for (const TCHAR* Q : {TEXT("Low"),TEXT("Medium"),TEXT("High"),TEXT("Epic"),TEXT("Cinematic")}) W->AddOption(Q); W->SetSelectedIndex(FMath::Clamp(S->GetOverallScalabilityLevel(),0,4)); }
    }
    if (ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer()))
    {
        if (USlider* W=Cast<USlider>(GetWidgetFromName(TEXT("Slider_MouseSensitivity")))) W->SetValue((PC->GetMouseSensitivity()-0.1f)/2.9f);
        if (UCheckBox* W=Cast<UCheckBox>(GetWidgetFromName(TEXT("Check_MouseFacing")))) W->SetIsChecked(PC->IsMouseFacingEnabled());
    }
    RefreshKeyBindingLabels();
}

void UCVADUserWidget::RefreshKeyBindingLabels()
{
    ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer());
    if (!PC) return;

    struct FKeyLabelBinding { const TCHAR* WidgetName; const TCHAR* ActionName; };
    static const FKeyLabelBinding Bindings[] = {
        {TEXT("Text_Key_MoveForward"), TEXT("MoveForward")},
        {TEXT("Text_Key_MoveBack"), TEXT("MoveBack")},
        {TEXT("Text_Key_MoveLeft"), TEXT("MoveLeft")},
        {TEXT("Text_Key_MoveRight"), TEXT("MoveRight")},
        {TEXT("Text_Key_Jump"), TEXT("Jump")},
        {TEXT("Text_Key_LightAttack"), TEXT("LightAttack")},
        {TEXT("Text_Key_HeavyAttack"), TEXT("HeavyAttack")},
        {TEXT("Text_Key_Dodge"), TEXT("Dodge")},
        {TEXT("Text_Key_FlyingSword"), TEXT("FlyingSword")},
        {TEXT("Text_Key_SwitchStance"), TEXT("SwitchStance")}
    };
    for (const FKeyLabelBinding& Binding : Bindings)
    {
        if (UTextBlock* Label=Cast<UTextBlock>(GetWidgetFromName(Binding.WidgetName)))
        {
            const FKey BoundKey=PC->GetBoundKey(FName(Binding.ActionName));
            Label->SetText(BoundKey.IsValid() ? BoundKey.GetDisplayName() : NSLOCTEXT("CVAD", "UnboundKey", "未绑定"));
        }
    }
}

void UCVADUserWidget::HandleSettingsApplyClicked()
{
    if(UEditableTextBox* NameInput=Cast<UEditableTextBox>(GetWidgetFromName(TEXT("Input_PlayerName"))))
    {
        FText Failure; const FString RequestedName=NameInput->GetText().ToString();
        if(!ValidatePlayerName(RequestedName,Failure))
        { if(UTextBlock* Error=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_NameError")))) Error->SetText(Failure); return; }
        SubmitPlayerName(RequestedName);
        if(UTextBlock* Error=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_NameError")))) Error->SetText(NSLOCTEXT("CVAD","NameSaved","名称已保存"));
    }
    const float Master=Cast<USlider>(GetWidgetFromName(TEXT("Slider_MasterVolume"))) ? Cast<USlider>(GetWidgetFromName(TEXT("Slider_MasterVolume")))->GetValue() : 1.f;
    const float Music=Cast<USlider>(GetWidgetFromName(TEXT("Slider_MusicVolume"))) ? Cast<USlider>(GetWidgetFromName(TEXT("Slider_MusicVolume")))->GetValue() : 1.f;
    const float SFX=Cast<USlider>(GetWidgetFromName(TEXT("Slider_SFXVolume"))) ? Cast<USlider>(GetWidgetFromName(TEXT("Slider_SFXVolume")))->GetValue() : 1.f;
    ApplyAudioSettings(Master,Music,SFX);
    const int32 Scale=Cast<USlider>(GetWidgetFromName(TEXT("Slider_ResolutionScale"))) ? FMath::RoundToInt(50.f+Cast<USlider>(GetWidgetFromName(TEXT("Slider_ResolutionScale")))->GetValue()*50.f) : 100;
    const int32 Quality=Cast<UComboBoxString>(GetWidgetFromName(TEXT("Combo_Quality"))) ? Cast<UComboBoxString>(GetWidgetFromName(TEXT("Combo_Quality")))->GetSelectedIndex() : 3;
    const bool Full=Cast<UCheckBox>(GetWidgetFromName(TEXT("Check_Fullscreen"))) && Cast<UCheckBox>(GetWidgetFromName(TEXT("Check_Fullscreen")))->IsChecked();
    const bool VSync=Cast<UCheckBox>(GetWidgetFromName(TEXT("Check_VSync"))) && Cast<UCheckBox>(GetWidgetFromName(TEXT("Check_VSync")))->IsChecked();
    ApplyVideoSettings(Scale,Quality,Full,VSync);
    if (ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer()))
    {
        const float Normalized=Cast<USlider>(GetWidgetFromName(TEXT("Slider_MouseSensitivity"))) ? Cast<USlider>(GetWidgetFromName(TEXT("Slider_MouseSensitivity")))->GetValue() : 0.31f;
        PC->SetMouseSensitivity(0.1f+Normalized*2.9f);
        if (UCheckBox* W=Cast<UCheckBox>(GetWidgetFromName(TEXT("Check_MouseFacing")))) PC->SetMouseFacingEnabled(W->IsChecked());
    }
    UE_LOG(LogTemp,Log,TEXT("Settings applied from widget"));
}

void UCVADUserWidget::HandleSettingsResetClicked()
{
    if (ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer()))
    {
        PC->ResetInputBindings();
        RefreshKeyBindingLabels();
    }
}
void UCVADUserWidget::BeginKeyCapture(FName ActionName) { PendingRebindAction=ActionName; SetKeyboardFocus(); if (UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_RebindPrompt")))) T->SetText(FText::Format(NSLOCTEXT("CVAD","PressKey","请为 {0} 按下新按键（Esc 取消）"),FText::FromName(ActionName))); }
void UCVADUserWidget::CaptureMoveForward(){BeginKeyCapture(TEXT("MoveForward"));} void UCVADUserWidget::CaptureMoveBack(){BeginKeyCapture(TEXT("MoveBack"));}
void UCVADUserWidget::CaptureMoveLeft(){BeginKeyCapture(TEXT("MoveLeft"));} void UCVADUserWidget::CaptureMoveRight(){BeginKeyCapture(TEXT("MoveRight"));}
void UCVADUserWidget::CaptureJump(){BeginKeyCapture(TEXT("Jump"));} void UCVADUserWidget::CaptureLightAttack(){BeginKeyCapture(TEXT("LightAttack"));}
void UCVADUserWidget::CaptureHeavyAttack(){BeginKeyCapture(TEXT("HeavyAttack"));} void UCVADUserWidget::CaptureDodge(){BeginKeyCapture(TEXT("Dodge"));}
void UCVADUserWidget::CaptureFlyingSword(){BeginKeyCapture(TEXT("FlyingSword"));} void UCVADUserWidget::CaptureSwitchStance(){BeginKeyCapture(TEXT("SwitchStance"));}

FReply UCVADUserWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (PendingRebindAction.IsNone()) return Super::NativeOnPreviewKeyDown(InGeometry,InKeyEvent);
    const FKey Key=InKeyEvent.GetKey();
    if (Key==EKeys::Escape) { PendingRebindAction=NAME_None; return FReply::Handled(); }
    ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer());
    if (!PC || !Key.IsValid()) return FReply::Handled();
    static const FName Actions[]={TEXT("Jump"),TEXT("LightAttack"),TEXT("HeavyAttack"),TEXT("Dodge"),TEXT("FlyingSword"),TEXT("SwitchStance"),TEXT("Inventory"),TEXT("Pause"),TEXT("Sprint"),TEXT("Interact")};
    for (const FName Other : Actions) if (Other!=PendingRebindAction && PC->GetBoundKey(Other)==Key)
    { if (UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_RebindPrompt")))) T->SetText(FText::Format(NSLOCTEXT("CVAD","KeyConflict","按键已用于 {0}"),FText::FromName(Other))); return FReply::Handled(); }
    const bool bSuccess=PC->RebindAction(PendingRebindAction,Key);
    if (UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_RebindPrompt")))) T->SetText(bSuccess?FText::Format(NSLOCTEXT("CVAD","KeyBound","已绑定为 {0}"),Key.GetDisplayName()):NSLOCTEXT("CVAD","KeyFailed","绑定失败"));
    if (bSuccess) RefreshKeyBindingLabels();
    PendingRebindAction=NAME_None;
    return FReply::Handled();
}

void UCVADUserWidget::HandlePauseResumeClicked() { ResumeGame(); }
void UCVADUserWidget::HandlePauseSaveClicked() { if (ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer())) PC->ShowSaveSlotsScreen(); }
void UCVADUserWidget::HandlePauseLoadClicked() { if (ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer())) PC->ShowSaveSlotsScreen(); }
void UCVADUserWidget::HandlePauseReturnMenuClicked()
{
    if (ACVADPlayerController* PC = Cast<ACVADPlayerController>(GetOwningPlayer())) PC->RequestReturnToMainMenu();
}
void UCVADUserWidget::HandleOpenInventoryClicked() { if (ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer())) PC->ShowSkillTreeScreen(); }
void UCVADUserWidget::HandleOpenSkillTreeClicked() { if (ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer())) PC->ShowSkillTreeScreen(); }
void UCVADUserWidget::HandleOpenSettingsClicked() { if (ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer())) PC->ShowSettingsScreen(); }
void UCVADUserWidget::HandleOpenCustomKeybindingsClicked() { if (ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer())) PC->ShowCustomKeybindingsScreen(); }
void UCVADUserWidget::HandleCloseClicked() { if (ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer())) PC->CloseTopScreen(); else CloseScreen(); }
void UCVADUserWidget::HandleConfirmNameClicked()
{
    UEditableTextBox* Input=Cast<UEditableTextBox>(GetWidgetFromName(TEXT("Input_PlayerName")));
    UTextBlock* Error=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_NameError")));
    const FString Name=Input?Input->GetText().ToString():FString();
    FText Failure;
    if(!ValidatePlayerName(Name,Failure)){if(Error) Error->SetText(Failure);return;}
    SubmitPlayerName(Name);
    if(ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer())) { PC->CloseTopScreen(); PC->ContinuePendingMenuAction(); }
}

void UCVADUserWidget::ChangeOutfitPart(int32 P,int32 D){static const int32 Counts[]={3,4,6,5,3,3,7};PreviewOutfitParts[P]=(PreviewOutfitParts[P]+D+Counts[P])%Counts[P];RefreshOutfitPreview();}

void UCVADUserWidget::ApplyOutfitPreviewCamera(bool bSaveSettings)
{
    OutfitPreviewCameraLocation.X=FMath::Clamp(OutfitPreviewCameraLocation.X,70.f,400.f);
    OutfitPreviewCameraLocation.Z=FMath::Clamp(OutfitPreviewCameraLocation.Z,20.f,220.f);
    if(UViewport* Preview=Cast<UViewport>(GetWidgetFromName(TEXT("Viewport_OutfitPreview"))))
    {
        // Keep the preview presentation front-facing. The character turns toward the
        // camera and the camera targets the upper torso, so zoom/height adjustments
        // never leave the model side-on or looking past it.
        const FVector CharacterOrigin=OutfitPreviewCharacter ? OutfitPreviewCharacter->GetActorLocation() : FVector::ZeroVector;
        if(OutfitPreviewCharacter)
        {
            const FVector FacingDirection=(OutfitPreviewCameraLocation-CharacterOrigin).GetSafeNormal2D();
            if(!FacingDirection.IsNearlyZero()) OutfitPreviewCharacter->SetActorRotation(FacingDirection.Rotation());
        }
        const FVector LookAtPoint=CharacterOrigin+FVector(0.f,0.f,35.f);
        OutfitPreviewCameraRotation=(LookAtPoint-OutfitPreviewCameraLocation).Rotation();
        Preview->SetViewLocation(OutfitPreviewCameraLocation);
        Preview->SetViewRotation(OutfitPreviewCameraRotation);
    }
    if(UTextBlock* Status=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_OutfitStatus"))))
        Status->SetText(FText::FromString(FString::Printf(TEXT("镜头距离 %.0f  高度 %.0f"),OutfitPreviewCameraLocation.X,OutfitPreviewCameraLocation.Z)));
    if(bSaveSettings && GConfig)
    {
        GConfig->SetFloat(TEXT("CVAD.OutfitPreview"),TEXT("CameraDistance"),OutfitPreviewCameraLocation.X,GGameUserSettingsIni);
        GConfig->SetFloat(TEXT("CVAD.OutfitPreview"),TEXT("CameraHeight"),OutfitPreviewCameraLocation.Z,GGameUserSettingsIni);
        GConfig->Flush(false,GGameUserSettingsIni);
    }
}

void UCVADUserWidget::OutfitCameraZoomIn(){OutfitPreviewCameraLocation.X-=15.f;ApplyOutfitPreviewCamera(true);}
void UCVADUserWidget::OutfitCameraZoomOut(){OutfitPreviewCameraLocation.X+=15.f;ApplyOutfitPreviewCamera(true);}
void UCVADUserWidget::OutfitCameraMoveUp(){OutfitPreviewCameraLocation.Z+=10.f;ApplyOutfitPreviewCamera(true);}
void UCVADUserWidget::OutfitCameraMoveDown(){OutfitPreviewCameraLocation.Z-=10.f;ApplyOutfitPreviewCamera(true);}
void UCVADUserWidget::OutfitCameraReset(){OutfitPreviewCameraLocation=FVector(210.f,0.f,35.f);OutfitPreviewCameraRotation=FRotator(0.f,180.f,0.f);ApplyOutfitPreviewCamera(true);}
#define OUTFIT_PAIR(N,I) void UCVADUserWidget::Outfit##N##Prev(){ChangeOutfitPart(I,-1);} void UCVADUserWidget::Outfit##N##Next(){ChangeOutfitPart(I,1);}
OUTFIT_PAIR(Head,0) OUTFIT_PAIR(Hair,1) OUTFIT_PAIR(Hat,2) OUTFIT_PAIR(Upper,3) OUTFIT_PAIR(Hands,4) OUTFIT_PAIR(Lower,5) OUTFIT_PAIR(Feet,6)
#undef OUTFIT_PAIR
void UCVADUserWidget::RefreshOutfitPreview()
{
    if(OutfitPreviewCharacter) OutfitPreviewCharacter->ApplyAppearanceSelection(PreviewOutfitParts);
    static const TCHAR* Names[]={TEXT("Text_HeadValue"),TEXT("Text_HairValue"),TEXT("Text_HatValue"),TEXT("Text_UpperValue"),TEXT("Text_HandsValue"),TEXT("Text_LowerValue"),TEXT("Text_FeetValue")};
    static const TCHAR* Labels[]={TEXT("脸型"),TEXT("发型"),TEXT("帽子"),TEXT("上装"),TEXT("手部"),TEXT("下装"),TEXT("鞋子")}; static const int32 Counts[]={3,4,6,5,3,3,7};
    for(int32 I=0;I<7;++I)if(UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(Names[I])))T->SetText(FText::FromString(Labels[I]));
}
void UCVADUserWidget::HandleOutfitConfirm()
{
    UEditableTextBox* NameInput=Cast<UEditableTextBox>(GetWidgetFromName(TEXT("Input_PlayerName")));FText Failure;
    if(NameInput)
    {
        const FString RequestedName = NameInput->GetText().ToString().TrimStartAndEnd();
        if(!RequestedName.IsEmpty())
        {
            if(!ValidatePlayerName(RequestedName,Failure))
            {
                if(UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_OutfitStatus")))) T->SetText(Failure);
                return;
            }
            SubmitPlayerName(RequestedName);
        }
    }
    if(GConfig){for(int32 I=0;I<7;++I)GConfig->SetInt(TEXT("CVAD.Appearance"),*FString::Printf(TEXT("Part%d"),I),PreviewOutfitParts[I],GGameUserSettingsIni);GConfig->Flush(false,GGameUserSettingsIni);}
    if(UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_OutfitStatus")))) T->SetText(NSLOCTEXT("CVAD","OutfitSaved","已确认，进入游戏后生效"));
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
    FText Failure;
    if (!ValidatePlayerName(NewName, Failure)) { UE_LOG(LogTemp, Warning, TEXT("Rejected player name: %s"), *Failure.ToString()); return; }
    const FString CleanName = NewName.TrimStartAndEnd().Left(20);
    if (APlayerController* PC = GetOwningPlayer()) PC->ServerChangeName(CleanName);
    if (GConfig) { GConfig->SetString(TEXT("CVAD.Profile"), TEXT("PlayerName"), *CleanName, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); }
}

bool UCVADUserWidget::ValidatePlayerName(const FString& NewName, FText& FailureReason) const
{
    const FString CleanName = NewName.TrimStartAndEnd();
    if (CleanName.Len() < 1) { FailureReason = NSLOCTEXT("CVAD", "NameShort", "请输入玩家名称"); return false; }
    if (CleanName.Len() > 20) { FailureReason = NSLOCTEXT("CVAD", "NameLong", "名称不能超过 20 个字符"); return false; }
    for (const TCHAR Character : CleanName)
        if (Character < 32 || Character == TEXT('/') || Character == TEXT('\\'))
        { FailureReason = NSLOCTEXT("CVAD", "NameInvalid", "名称包含无效字符"); return false; }
    FailureReason = FText::GetEmpty();
    return true;
}

void UCVADUserWidget::SpendSkillPoint(FName SkillRowName)
{
    if (!CachedPlayerState) InitializeFromOwningPlayer();
    if (CachedPlayerState) CachedPlayerState->RequestSpendSkillPoint(SkillRowName);
}

void UCVADUserWidget::SelectSkill(FName SkillRowName)
{
    SelectedSkillRow=SkillRowName;
    if(!CachedPlayerState) InitializeFromOwningPlayer();
    RefreshSkillDetails();
    PlaySkillPreviewAnimation(SkillRowName);
    UE_LOG(LogTemp,Log,TEXT("Skill tree selected %s"),*SkillRowName.ToString());
}
void UCVADUserWidget::SelectSwordAttack1(){SelectSkill(TEXT("SwordAttack1"));} void UCVADUserWidget::SelectSwordAttack2(){SelectSkill(TEXT("SwordAttack2"));}
void UCVADUserWidget::SelectSwordAttack3(){SelectSkill(TEXT("SwordAttack3"));} void UCVADUserWidget::SelectSwordAttack4(){SelectSkill(TEXT("SwordAttack4"));}
void UCVADUserWidget::SelectSwordAttack5(){SelectSkill(TEXT("SwordAttack5"));} void UCVADUserWidget::SelectFlyingSword1(){SelectSkill(TEXT("FlyingSword1"));}
void UCVADUserWidget::SelectFlyingSword2(){SelectSkill(TEXT("FlyingSword2"));} void UCVADUserWidget::SelectFlyingSword3(){SelectSkill(TEXT("FlyingSword3"));}

void UCVADUserWidget::EquipSelectedSkill()
{
    if(!CachedPlayerState || SelectedSkillRow.IsNone()) return;
    UDataTable* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/CVAD/Data/DT_Skills.DT_Skills"));
    const FCVADSkillRow* Row=Table?Table->FindRow<FCVADSkillRow>(SelectedSkillRow,TEXT("EquipSelectedUI")):nullptr;
    if(!Row) return;
    if(!Row->bUnlockedByDefault && !CachedPlayerState->IsSkillUnlocked(SelectedSkillRow))
    {
        FText Failure;
        if(CachedPlayerState->CanUnlockSkill(SelectedSkillRow,Failure))
        {
            CachedPlayerState->RequestSpendSkillPoint(SelectedSkillRow);
            CachedPlayerState->EquipSkill(Row->SkillSlot,SelectedSkillRow);
            if(GConfig)
            {
                const FString Key=FString::Printf(TEXT("EquippedSkill%d"),static_cast<int32>(Row->SkillSlot));
                GConfig->SetString(TEXT("CVAD.SkillLoadout"),*Key,*SelectedSkillRow.ToString(),GGameUserSettingsIni);
                GConfig->Flush(false,GGameUserSettingsIni);
            }
            RefreshSkillDetails();
            if(UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_SkillCost")))) T->SetText(NSLOCTEXT("CVAD","SkillPurchasedAndEquipped","已购买并装配"));
        }
        else if(UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_SkillCost")))) T->SetText(Failure);
        return;
    }
    CachedPlayerState->EquipSkill(Row->SkillSlot,SelectedSkillRow);
    if(GConfig)
    {
        const FString Key=FString::Printf(TEXT("EquippedSkill%d"),static_cast<int32>(Row->SkillSlot));
        GConfig->SetString(TEXT("CVAD.SkillLoadout"),*Key,*SelectedSkillRow.ToString(),GGameUserSettingsIni);
        GConfig->Flush(false,GGameUserSettingsIni);
    }
    if(UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_SkillCost"))))
        T->SetText(NSLOCTEXT("CVAD","SkillEquippedSaved","已装配并保存，下次进入战斗自动生效"));
    UE_LOG(LogTemp,Log,TEXT("Skill tree equip requested %s Slot=%d"),*SelectedSkillRow.ToString(),static_cast<int32>(Row->SkillSlot));
}

void UCVADUserWidget::HandleSkillLoadoutChanged(){RefreshSkillDetails();}

void UCVADUserWidget::RefreshSkillDetails()
{
    if(!CachedPlayerState || SelectedSkillRow.IsNone()) return;
    UDataTable* Table=LoadObject<UDataTable>(nullptr,TEXT("/Game/CVAD/Data/DT_Skills.DT_Skills"));
    const FCVADSkillRow* Row=Table?Table->FindRow<FCVADSkillRow>(SelectedSkillRow,TEXT("SkillDetailsUI")):nullptr;
    if(!Row) return;
    const int32 Level=CachedPlayerState->PlayerLevel;
    const bool bUnlocked=Row->bUnlockedByDefault||CachedPlayerState->IsSkillUnlocked(SelectedSkillRow);
    const bool bEquipped=CachedPlayerState->GetEquippedSkill(Row->SkillSlot)==SelectedSkillRow;
    FText Failure; const bool bCanUnlock=!bUnlocked&&CachedPlayerState->CanUnlockSkill(SelectedSkillRow,Failure);
    const TSubclassOf<UGameplayAbility> LoadedAbilityClass=Row->AbilityClass.LoadSynchronous();
    const UCVADCombatAbility* Ability=LoadedAbilityClass ? Cast<UCVADCombatAbility>(LoadedAbilityClass->GetDefaultObject()) : nullptr;
    if(UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_SelectedSkillName"))))
        T->SetText(FText::Format(NSLOCTEXT("CVAD","SkillNameState","{0}  [{1}]"),Row->DisplayName,
            bEquipped?NSLOCTEXT("CVAD","Equipped","已装配"):(bUnlocked?NSLOCTEXT("CVAD","Unlocked","已解锁"):NSLOCTEXT("CVAD","Locked","未解锁"))));
    if(UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_SelectedSkillDescription"))))
    {
        const FString Stats=Ability?FString::Printf(TEXT("\nLv.%d 伤害 %.1f | 范围 %.1f | 冷却 %.2fs | 消耗 %.1f"),Level,
            Ability->GetPreviewDamage(Level),Ability->GetPreviewRadius(Level),Ability->GetPreviewCooldown(Level),Ability->GetPreviewResourceCost()):TEXT("");
        T->SetText(FText::FromString(Row->Description.ToString()+Stats));
    }
    if(UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Prerequisite"))))
        T->SetText(Row->PrerequisiteSkill.IsNone()?NSLOCTEXT("CVAD","NoPrereq","无前置技能"):FText::Format(NSLOCTEXT("CVAD","Prereq","前置：{0}"),FText::FromName(Row->PrerequisiteSkill)));
    if(UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_SkillCost"))))
        T->SetText(bUnlocked?NSLOCTEXT("CVAD","SkillReady","可以装配"):bCanUnlock?FText::Format(NSLOCTEXT("CVAD","UnlockCost","点击解锁：{0} 技能点"),Row->SkillPointCost):Failure);
    if(UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Level")))) T->SetText(FText::Format(NSLOCTEXT("CVAD","SkillLevelUI","等级 {0}"),Level));
    if(UTextBlock* T=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_SkillPoints")))) T->SetText(FText::Format(NSLOCTEXT("CVAD","SkillPointsUI","技能点 {0}"),CachedPlayerState->SkillPoints));
    if(UButton* EquipButton=Cast<UButton>(GetWidgetFromName(TEXT("Button_EquipSelected"))))
        EquipButton->SetIsEnabled(((bUnlocked&&!bEquipped)||bCanUnlock)&&LoadedAbilityClass!=nullptr);
    if(UTextBlock* EquipLabel=Cast<UTextBlock>(GetWidgetFromName(TEXT("Label_Button_EquipSelected"))))
        EquipLabel->SetText(bUnlocked?NSLOCTEXT("CVAD","EquipSkillButton","装配所选技能"):NSLOCTEXT("CVAD","BuySkillButton","购买所选技能"));
    if(UTextBlock* EquippedText=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_EquippedSkills"))))
    {
        static const TCHAR* SlotNames[]={TEXT("普通攻击"),TEXT("重攻击"),TEXT("闪避"),TEXT("飞剑技能"),TEXT("战斗姿态")};
        FString Summary=TEXT("当前已装配\n");
        for(int32 SlotIndex=0;SlotIndex<5;++SlotIndex)
        {
            const FName Equipped=CachedPlayerState->GetEquippedSkill(static_cast<ECVADAbilityInput>(SlotIndex));
            const FCVADSkillRow* EquippedRow=Table->FindRow<FCVADSkillRow>(Equipped,TEXT("EquippedSummary"),false);
            Summary+=FString::Printf(TEXT("%s：%s\n"),SlotNames[SlotIndex],EquippedRow?*EquippedRow->DisplayName.ToString():TEXT("未装配"));
        }
        EquippedText->SetText(FText::FromString(Summary));
    }
    if(UTextBlock* AvailableText=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_AvailableSkills"))))
    {
        FString Available=TEXT("可装配功法：");
        for(const FName SkillName:Table->GetRowNames())
        {
            const FCVADSkillRow* Skill=Table->FindRow<FCVADSkillRow>(SkillName,TEXT("AvailableSummary"),false);
            if(Skill&&(Skill->bUnlockedByDefault||CachedPlayerState->IsSkillUnlocked(SkillName))) Available+=TEXT("  ")+Skill->DisplayName.ToString();
        }
        AvailableText->SetText(FText::FromString(Available));
    }
}

void UCVADUserWidget::PlaySkillPreviewAnimation(FName SkillRowName)
{
    if (!SkillPreviewCharacter) return;
    UDataTable* Table = LoadObject<UDataTable>(nullptr, TEXT("/Game/CVAD/Data/DT_Skills.DT_Skills"));
    const FCVADSkillRow* Row = Table ? Table->FindRow<FCVADSkillRow>(SkillRowName, TEXT("SkillPreview")) : nullptr;
    if (!Row) return;
    const TSubclassOf<UGameplayAbility> AbilityClass = Row->AbilityClass.LoadSynchronous();
    const UCVADCombatAbility* Ability = AbilityClass
        ? Cast<UCVADCombatAbility>(AbilityClass->GetDefaultObject())
        : nullptr;
    if (!Ability || !Ability->GetPreviewAnimation()) return;
    SkillPreviewCharacter->GetMesh()->PlayAnimation(Ability->GetPreviewAnimation(), false);
    UE_LOG(LogTemp, Log, TEXT("Playing skill preview %s"), *SkillRowName.ToString());
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
    return SaveProfileToSlot(GetLastUsedProfileSlot());
}

FString UCVADUserWidget::GetProfileSlotName(int32 SlotIndex)
{
    return FString::Printf(TEXT("CVAD_Profile_%d"), FMath::Clamp(SlotIndex, 0, 2));
}

int32 UCVADUserWidget::GetLastUsedProfileSlot()
{
    int32 Slot = 0;
    if (GConfig) GConfig->GetInt(CVADSaveConfigSection, TEXT("LastUsedSlot"), Slot, GGameUserSettingsIni);
    return FMath::Clamp(Slot, 0, 2);
}

bool UCVADUserWidget::SaveProfileToSlot(int32 SlotIndex)
{
    if (!CachedPlayerState) InitializeFromOwningPlayer();
    if (!CachedPlayerState) return false;
    SlotIndex = FMath::Clamp(SlotIndex, 0, 2);
    const FString SlotName = GetProfileSlotName(SlotIndex);
    UCVADSaveGame* Save = nullptr;
    if (UGameplayStatics::DoesSaveGameExist(SlotName, 0))
    {
        Save = Cast<UCVADSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
    }
    if (!Save) Save = Cast<UCVADSaveGame>(UGameplayStatics::CreateSaveGameObject(UCVADSaveGame::StaticClass()));
    if (!Save) return false;
    Save->SaveVersion = 2;
    Save->SavedAtLocalTime = FDateTime::Now().ToString(TEXT("%Y-%m-%d %H:%M"));
    if(ACVADPlayerController* PC=Cast<ACVADPlayerController>(GetOwningPlayer()))
        Save->TotalPlayTimeSeconds += PC->ConsumeUnsavedPlayTime();
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
    const bool bSaved = UGameplayStatics::SaveGameToSlot(Save, SlotName, 0);
    if (bSaved && GConfig) { GConfig->SetInt(CVADSaveConfigSection,TEXT("LastUsedSlot"),SlotIndex,GGameUserSettingsIni); GConfig->Flush(false,GGameUserSettingsIni); }
    UE_LOG(LogTemp, Log, TEXT("Profile save Slot=%d Result=%s Level=%d"), SlotIndex, bSaved?TEXT("true"):TEXT("false"), Save->PlayerLevel);
    if (bSaved) RefreshSaveSlotPreviews();
    return bSaved;
}

bool UCVADUserWidget::LoadProfile()
{
    return LoadProfileFromSlot(GetLastUsedProfileSlot());
}

bool UCVADUserWidget::LoadProfileFromSlot(int32 SlotIndex)
{
    SlotIndex=FMath::Clamp(SlotIndex,0,2);
    if (!UGameplayStatics::DoesSaveGameExist(GetProfileSlotName(SlotIndex), 0)) return false;
    UCVADSaveGame* Save = Cast<UCVADSaveGame>(UGameplayStatics::LoadGameFromSlot(GetProfileSlotName(SlotIndex), 0));
    if (!Save) return false;
    const bool bMainMenuMap = GetWorld() && GetWorld()->GetMapName().Contains(TEXT("L_MainMenu"));
    if (GConfig) { GConfig->SetInt(CVADSaveConfigSection,TEXT("LastUsedSlot"),SlotIndex,GGameUserSettingsIni); GConfig->Flush(false,GGameUserSettingsIni); }
    if (bMainMenuMap)
    {
        UE_LOG(LogTemp, Log, TEXT("Starting battle from profile Slot=%d"), SlotIndex);
        const FString CleanName=Save->PlayerDisplayName.TrimStartAndEnd().Left(20);
        if(GConfig && CleanName.Len()>=1)
        {
            GConfig->SetString(TEXT("CVAD.Profile"),TEXT("PlayerName"),*CleanName,GGameUserSettingsIni);
            GConfig->Flush(false,GGameUserSettingsIni);
        }
        const FString Options=TEXT("LoadProfile=1?PlayerName=")+FGenericPlatformHttp::UrlEncode(CleanName.Len()>=1?CleanName:TEXT("Player"));
        UGameplayStatics::OpenLevel(this, TEXT("L_CastleBattle"), true, Options);
        return true;
    }
    if (APlayerController* PC = GetOwningPlayer()) PC->ServerChangeName(Save->PlayerDisplayName);
    if (!CachedPlayerState) InitializeFromOwningPlayer();
    if (CachedPlayerState)
    {
        CachedPlayerState->RequestRestoreProfile(Save->PlayerLevel, Save->Experience, Save->SkillPoints,
            Save->UnlockedSkillRows, Save->EquippedSkillRows, Save->UnlockedItemIds, Save->EquipmentLoadout);
    }
    UE_LOG(LogTemp, Log, TEXT("Profile loaded Slot=%d Version=%d"), SlotIndex, Save->SaveVersion);
    return true;
}

bool UCVADUserWidget::DeleteProfileSlot(int32 SlotIndex) { const bool bDeleted=UGameplayStatics::DeleteGameInSlot(GetProfileSlotName(SlotIndex),0); RefreshSaveSlotPreviews(); return bDeleted; }
bool UCVADUserWidget::GetProfileSlotInfo(int32 SlotIndex,FString& PlayerName,int32& Level,FString& SavedAt,float& PlayTimeSeconds,bool& bCompleted) const
{
    if (!UGameplayStatics::DoesSaveGameExist(GetProfileSlotName(SlotIndex), 0)) return false;
    const UCVADSaveGame* Save=Cast<UCVADSaveGame>(UGameplayStatics::LoadGameFromSlot(GetProfileSlotName(SlotIndex),0));
    if(!Save) return false; PlayerName=Save->PlayerDisplayName; Level=Save->PlayerLevel; SavedAt=Save->SavedAtLocalTime;
    PlayTimeSeconds=Save->TotalPlayTimeSeconds; bCompleted=Save->bHasCompletedDemo; return true;
}
void UCVADUserWidget::SaveSlot0(){SelectSaveSlot(0);} void UCVADUserWidget::SaveSlot1(){SelectSaveSlot(1);} void UCVADUserWidget::SaveSlot2(){SelectSaveSlot(2);}
void UCVADUserWidget::SaveSelectedSlot(){SaveProfileToSlot(SelectedSaveSlot);}
void UCVADUserWidget::LoadSelectedSlot(){LoadProfileFromSlot(SelectedSaveSlot);}
void UCVADUserWidget::DeleteSelectedSlot(){DeleteProfileSlot(SelectedSaveSlot);}
void UCVADUserWidget::SelectSaveSlot(int32 SlotIndex){SelectedSaveSlot=FMath::Clamp(SlotIndex,0,2);RefreshSaveSlotPreviews();}

void UCVADUserWidget::RefreshSaveSlotPreviews()
{
    SetSaveSlotPreviewText(0,Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Slot0"))));
    SetSaveSlotPreviewText(1,Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Slot1"))));
    SetSaveSlotPreviewText(2,Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_Slot2"))));
    if(UTextBlock* Selected=Cast<UTextBlock>(GetWidgetFromName(TEXT("Text_SelectedSlot"))))
        Selected->SetText(FText::FromString(FString::Printf(TEXT("当前选择：存档 %d"),SelectedSaveSlot+1)));
    for(int32 SlotIndex=0;SlotIndex<3;++SlotIndex)
        if(UButton* Button=Cast<UButton>(GetWidgetFromName(FName(*FString::Printf(TEXT("Button_SaveSlot%d"),SlotIndex)))))
            Button->SetBackgroundColor(SlotIndex==SelectedSaveSlot?FLinearColor(0.15f,0.55f,0.8f,1.f):FLinearColor(0.08f,0.15f,0.28f,1.f));
    const bool bHasSave=UGameplayStatics::DoesSaveGameExist(GetProfileSlotName(SelectedSaveSlot),0);
    if(UButton* Load=Cast<UButton>(GetWidgetFromName(TEXT("Button_LoadSelected")))) Load->SetIsEnabled(bHasSave);
    if(UButton* Delete=Cast<UButton>(GetWidgetFromName(TEXT("Button_DeleteSelected")))) Delete->SetIsEnabled(bHasSave);
}

void UCVADUserWidget::SetSaveSlotPreviewText(int32 SlotIndex,UTextBlock* TextWidget)
{
    if(!TextWidget) return;
    FString Name,SavedAt; int32 Level=1; float PlayTime=0.f; bool bCompleted=false;
    if(!GetProfileSlotInfo(SlotIndex,Name,Level,SavedAt,PlayTime,bCompleted))
    { TextWidget->SetText(FText::Format(NSLOCTEXT("CVAD","EmptySlot","槽位 {0}：空"),SlotIndex+1)); return; }
    const int32 Hours=FMath::FloorToInt(PlayTime/3600.f);
    const int32 Minutes=FMath::FloorToInt(FMath::Fmod(PlayTime,3600.f)/60.f);
    TextWidget->SetText(FText::FromString(FString::Printf(TEXT("槽位 %d | %s | Lv.%d | %02d:%02d | %s | %s"),
        SlotIndex+1,*Name,Level,Hours,Minutes,*SavedAt,bCompleted?TEXT("已通关"):TEXT("进行中"))));
}

UCVADInventoryComponent* UCVADUserWidget::GetInventory() const
{
    return CachedPlayerState ? CachedPlayerState->GetInventoryComponent() : nullptr;
}

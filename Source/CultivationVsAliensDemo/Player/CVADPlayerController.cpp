#include "Player/CVADPlayerController.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "GameFramework/Character.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "AbilitySystemInterface.h"
#include "AbilitySystemComponent.h"
#include "GameplayTagContainer.h"
#include "Character/CVADCharacter.h"
#include "AbilitySystem/Abilities/CVADCombatAbility.h"
#include "UI/CVADUserWidget.h"
#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Battle/CVADBattleDirector.h"
#include "EngineUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "Save/CVADSaveGame.h"
#include "Player/CVADPlayerState.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogCVADInput, Log, All);
static const TCHAR* CVADInputConfigSection = TEXT("CVAD.InputBindings");

ACVADPlayerController::ACVADPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ACVADPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    if (IsLocalController() && bMouseFacingEnabled) UpdateMouseFacing();
    if (IsLocalController() && !bResultShown && ResultWidgetClass)
    {
        for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It)
        {
            if (It->BattlePhase != ECVADBattlePhase::Results) break;
            ResultWidget = CreateWidget<UCVADUserWidget>(this, ResultWidgetClass);
            if (ResultWidget)
            {
                ResultWidget->AddToViewport(30);
                SetInputMode(FInputModeUIOnly());
                bShowMouseCursor = true;
                SetIgnoreMoveInput(true);
                SetIgnoreLookInput(false);
                bResultShown = true;
                UE_LOG(LogCVADInput, Log, TEXT("Battle result screen shown"));
            }
            break;
        }
    }
}

void ACVADPlayerController::SetMouseFacingEnabled(bool bEnabled)
{
    bMouseFacingEnabled = bEnabled;
    UE_LOG(LogCVADInput, Log, TEXT("Mouse character facing: %s"), bEnabled ? TEXT("enabled") : TEXT("disabled"));
    if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn()))
    {
        ControlledCharacter->bUseControllerRotationYaw = bEnabled;
        ControlledCharacter->GetCharacterMovement()->bOrientRotationToMovement = !bEnabled;
    }
}

void ACVADPlayerController::UpdateMouseFacing()
{
    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn) return;
    const float Yaw = GetControlRotation().Yaw;
    const FRotator Desired(0.f, Yaw, 0.f);
    ControlledPawn->SetActorRotation(FMath::RInterpTo(ControlledPawn->GetActorRotation(), Desired,
        GetWorld()->GetDeltaSeconds(), FacingInterpSpeed));
    if (!HasAuthority()) ServerSetFacingYaw(Yaw);
}

void ACVADPlayerController::ServerSetFacingYaw_Implementation(float Yaw)
{
    if (APawn* ControlledPawn = GetPawn()) ControlledPawn->SetActorRotation(FRotator(0.f, Yaw, 0.f));
}

void ACVADPlayerController::RequestRestartBattle()
{
    if (HasAuthority()) RestartBattleAuthority();
    else ServerRestartBattle();
}

void ACVADPlayerController::ServerRestartBattle_Implementation()
{
    RestartBattleAuthority();
}

void ACVADPlayerController::RestartBattleAuthority()
{
    if (!HasAuthority() || !GetWorld()) return;
    UE_LOG(LogCVADInput, Log, TEXT("Restarting battle for all connected players"));
    GetWorld()->ServerTravel(TEXT("/Game/CVAD/Maps/L_BattlePrototype?listen"), false);
}

void ACVADPlayerController::RequestReturnToMainMenu()
{
    if (HasAuthority()) ServerReturnToMainMenu_Implementation();
    else ServerReturnToMainMenu();
}

void ACVADPlayerController::ServerReturnToMainMenu_Implementation()
{
    if (!GetWorld()) return;
    UE_LOG(LogCVADInput, Log, TEXT("Returning all players to main menu"));
    GetWorld()->ServerTravel(TEXT("/Game/CVAD/Maps/L_MainMenu"), true);
}

void ACVADPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!HUDWidgetClass) HUDWidgetClass = LoadClass<UCVADUserWidget>(nullptr, TEXT("/Game/CVAD/UI/WBP_HUD.WBP_HUD_C"));
    if (!InventoryWidgetClass) InventoryWidgetClass = LoadClass<UCVADUserWidget>(nullptr, TEXT("/Game/CVAD/UI/WBP_Inventory.WBP_Inventory_C"));
    if (!PauseWidgetClass) PauseWidgetClass = LoadClass<UCVADUserWidget>(nullptr, TEXT("/Game/CVAD/UI/WBP_Pause.WBP_Pause_C"));
    if (!ResultWidgetClass) ResultWidgetClass = LoadClass<UCVADUserWidget>(nullptr, TEXT("/Game/CVAD/UI/WBP_Result.WBP_Result_C"));
    const bool bMainMenuMap = GetWorld() && GetWorld()->GetMapName().Contains(TEXT("L_MainMenu"));
    if (IsLocalController() && bMainMenuMap)
    {
        if (!MainMenuWidgetClass)
            MainMenuWidgetClass = LoadClass<UCVADUserWidget>(nullptr, TEXT("/Game/CVAD/UI/WBP_MainMenu.WBP_MainMenu_C"));
        if (MainMenuWidgetClass)
        {
            MainMenuWidget = CreateWidget<UCVADUserWidget>(this, MainMenuWidgetClass);
            if (MainMenuWidget) MainMenuWidget->AddToViewport(100);
        }
        SetInputMode(FInputModeUIOnly());
        bShowMouseCursor = true;
        UE_LOG(LogCVADInput, Log, TEXT("Main menu initialized Widget=%s"), *GetNameSafe(MainMenuWidget));
        return;
    }
    CameraPitchMin = FMath::Clamp(CameraPitchMin, -89.f, 89.f);
    CameraPitchMax = FMath::Clamp(CameraPitchMax, CameraPitchMin, 89.f);
    if (PlayerCameraManager)
    {
        PlayerCameraManager->ViewPitchMin = CameraPitchMin;
        PlayerCameraManager->ViewPitchMax = CameraPitchMax;
    }
    FRotator InitialControlRotation = GetControlRotation();
    InitialControlRotation.Pitch = FMath::Clamp(InitialCameraPitch, CameraPitchMin, CameraPitchMax);
    InitialControlRotation.Roll = 0.f;
    SetControlRotation(InitialControlRotation);
    UE_LOG(LogCVADInput, Log, TEXT("Camera pitch initialized Initial=%.1f Min=%.1f Max=%.1f"),
        InitialControlRotation.Pitch, CameraPitchMin, CameraPitchMax);
    UE_LOG(LogCVADInput, Log, TEXT("BeginPlay Controller=%s Local=%s Pawn=%s"),
        *GetNameSafe(this), IsLocalController() ? TEXT("true") : TEXT("false"), *GetNameSafe(GetPawn()));
    if (IsLocalController())
    {
        BuildRuntimeMappingContext();
        SetMouseFacingEnabled(bMouseFacingEnabled);
        if (ULocalPlayer* LocalPlayer = GetLocalPlayer())
        {
            if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            {
                if (RuntimeMappingContext)
                {
                    Subsystem->AddMappingContext(RuntimeMappingContext, 0);
                    UE_LOG(LogCVADInput, Log, TEXT("Added runtime mapping context: %s"), *GetNameSafe(RuntimeMappingContext));
                }
                else
                {
                    UE_LOG(LogCVADInput, Warning, TEXT("PlayerMappingContext is null; enhanced input will not fire."));
                }
            }
        }
        if (HUDWidgetClass)
        {
            HUDWidget = CreateWidget<UCVADUserWidget>(this, HUDWidgetClass);
            if (HUDWidget) HUDWidget->AddToViewport();
        }
        if (GetWorld() && GetWorld()->URL.HasOption(TEXT("LoadProfile")))
        {
            FTimerHandle StartupLoadTimer;
            GetWorldTimerManager().SetTimer(StartupLoadTimer, this, &ThisClass::ApplyStartupProfile, 0.5f, false);
        }
    }
}

void ACVADPlayerController::ApplyStartupProfile()
{
    UCVADSaveGame* Save = Cast<UCVADSaveGame>(UGameplayStatics::LoadGameFromSlot(TEXT("CVAD_Profile_0"), 0));
    ACVADPlayerState* PS = GetPlayerState<ACVADPlayerState>();
    if (!Save || !PS) { UE_LOG(LogCVADInput, Warning, TEXT("Startup profile load failed")); return; }
    ServerChangeName(Save->PlayerDisplayName.Left(20));
    PS->RequestRestoreProfile(Save->PlayerLevel, Save->Experience, Save->SkillPoints,
        Save->UnlockedSkillRows, Save->EquippedSkillRows, Save->UnlockedItemIds, Save->EquipmentLoadout);
    UE_LOG(LogCVADInput, Log, TEXT("Startup profile submitted for restoration"));
}

const UInputAction* ACVADPlayerController::FindInputAction(FName ActionName) const
{
    const TMap<FName, TObjectPtr<UInputAction>> Actions = {
        {TEXT("Move"), MoveAction}, {TEXT("Look"), LookAction}, {TEXT("Jump"), JumpAction},
        {TEXT("LightAttack"), LightAttackAction}, {TEXT("HeavyAttack"), HeavyAttackAction},
        {TEXT("Dodge"), DodgeAction}, {TEXT("FlyingSword"), FlyingSwordAction},
        {TEXT("SwitchStance"), SwitchStanceAction}, {TEXT("Inventory"), InventoryAction},
        {TEXT("Pause"), PauseAction}, {TEXT("Sprint"), SprintAction}, {TEXT("Interact"), InteractAction}};
    if (const TObjectPtr<UInputAction>* Found = Actions.Find(ActionName)) return Found->Get();
    return nullptr;
}

void ACVADPlayerController::BuildRuntimeMappingContext()
{
    RuntimeMappingContext = PlayerMappingContext ? DuplicateObject<UInputMappingContext>(PlayerMappingContext, this) : nullptr;
    ApplySavedInputBindings();
}

void ACVADPlayerController::ApplySavedInputBindings()
{
    if (!RuntimeMappingContext || !GConfig) return;
    static const FName Names[] = {TEXT("Jump"),TEXT("LightAttack"),TEXT("HeavyAttack"),TEXT("Dodge"),
        TEXT("FlyingSword"),TEXT("SwitchStance"),TEXT("Inventory"),TEXT("Pause"),TEXT("Sprint"),TEXT("Interact")};
    for (const FName Name : Names)
    {
        FString KeyName;
        if (GConfig->GetString(CVADInputConfigSection, *Name.ToString(), KeyName, GGameUserSettingsIni) && !KeyName.IsEmpty())
            RebindAction(Name, FKey(*KeyName));
    }
}

bool ACVADPlayerController::RebindAction(FName ActionName, FKey NewKey)
{
    const UInputAction* Action = FindInputAction(ActionName);
    if (!RuntimeMappingContext || !Action || !NewKey.IsValid()) return false;
    RuntimeMappingContext->UnmapAllKeysFromAction(Action);
    RuntimeMappingContext->MapKey(Action, NewKey);
    if (GConfig)
    {
        GConfig->SetString(CVADInputConfigSection, *ActionName.ToString(), *NewKey.GetFName().ToString(), GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }
    RebuildInputMappings();
    UE_LOG(LogCVADInput, Log, TEXT("Rebound %s to %s"), *ActionName.ToString(), *NewKey.ToString());
    return true;
}

void ACVADPlayerController::ResetInputBindings()
{
    if (GConfig) { GConfig->EmptySection(CVADInputConfigSection, GGameUserSettingsIni); GConfig->Flush(false, GGameUserSettingsIni); }
    if (ULocalPlayer* LP = GetLocalPlayer())
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            if (RuntimeMappingContext) Subsystem->RemoveMappingContext(RuntimeMappingContext);
    BuildRuntimeMappingContext();
    if (ULocalPlayer* LP = GetLocalPlayer())
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            if (RuntimeMappingContext) Subsystem->AddMappingContext(RuntimeMappingContext, 0);
    UE_LOG(LogCVADInput, Log, TEXT("Input bindings reset to defaults"));
}

FKey ACVADPlayerController::GetBoundKey(FName ActionName) const
{
    const UInputAction* Action = FindInputAction(ActionName);
    if (RuntimeMappingContext && Action)
        for (const FEnhancedActionKeyMapping& Mapping : RuntimeMappingContext->GetMappings())
            if (Mapping.Action == Action) return Mapping.Key;
    return FKey();
}

void ACVADPlayerController::RebuildInputMappings() const
{
    if (const ULocalPlayer* LP = GetLocalPlayer())
        if (UEnhancedInputLocalPlayerSubsystem* Subsystem = LP->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>())
            Subsystem->RequestRebuildControlMappings(FModifyContextOptions(), EInputMappingRebuildType::RebuildWithFlush);
}

void ACVADPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();
    UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(InputComponent);
    if (!Enhanced)
    {
        UE_LOG(LogCVADInput, Error, TEXT("InputComponent is not UEnhancedInputComponent: %s"), *GetNameSafe(InputComponent));
        return;
    }
    UE_LOG(LogCVADInput, Log, TEXT("Binding enhanced input actions for %s"), *GetNameSafe(this));

    if (MoveAction) Enhanced->BindAction(MoveAction, ETriggerEvent::Triggered, this, &ThisClass::Move);
    if (LookAction) Enhanced->BindAction(LookAction, ETriggerEvent::Triggered, this, &ThisClass::Look);
    if (JumpAction)
    {
        Enhanced->BindAction(JumpAction, ETriggerEvent::Started, this, &ThisClass::StartJump);
        Enhanced->BindAction(JumpAction, ETriggerEvent::Completed, this, &ThisClass::StopJump);
    }
    if (LightAttackAction) Enhanced->BindAction(LightAttackAction, ETriggerEvent::Started, this, &ThisClass::OnLightAttackPressed);
    if (HeavyAttackAction) Enhanced->BindAction(HeavyAttackAction, ETriggerEvent::Started, this, &ThisClass::OnHeavyAttackPressed);
    if (DodgeAction) Enhanced->BindAction(DodgeAction, ETriggerEvent::Started, this, &ThisClass::OnDodgePressed);
    if (FlyingSwordAction) Enhanced->BindAction(FlyingSwordAction, ETriggerEvent::Started, this, &ThisClass::OnFlyingSwordPressed);
    if (SwitchStanceAction) Enhanced->BindAction(SwitchStanceAction, ETriggerEvent::Started, this, &ThisClass::OnSwitchStancePressed);
    if (InventoryAction) Enhanced->BindAction(InventoryAction, ETriggerEvent::Started, this, &ThisClass::ToggleInventory);
    if (PauseAction) Enhanced->BindAction(PauseAction, ETriggerEvent::Started, this, &ThisClass::TogglePauseMenu);
    if (SprintAction)
    {
        Enhanced->BindAction(SprintAction, ETriggerEvent::Started, this, &ThisClass::StartSprint);
        Enhanced->BindAction(SprintAction, ETriggerEvent::Completed, this, &ThisClass::StopSprint);
    }
    if (InteractAction) Enhanced->BindAction(InteractAction, ETriggerEvent::Started, this, &ThisClass::Interact);
}

void ACVADPlayerController::Move(const FInputActionValue& Value)
{
    APawn* ControlledPawn = GetPawn();
    if (!ControlledPawn) return;
    const FVector2D RawInput = Value.Get<FVector2D>();
    const FVector2D Input = RawInput.SizeSquared() > 1.f ? RawInput.GetSafeNormal() : RawInput;
    UE_LOG(LogCVADInput, VeryVerbose, TEXT("Input Move X=%.3f Y=%.3f Pawn=%s"), Input.X, Input.Y, *GetNameSafe(ControlledPawn));
    const FRotator YawRotation(0.f, GetControlRotation().Yaw, 0.f);
    ControlledPawn->AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::X), Input.Y);
    ControlledPawn->AddMovementInput(FRotationMatrix(YawRotation).GetUnitAxis(EAxis::Y), Input.X);
}

void ACVADPlayerController::Look(const FInputActionValue& Value)
{
    const FVector2D Input = Value.Get<FVector2D>();
    UE_LOG(LogCVADInput, VeryVerbose, TEXT("Input Look X=%.3f Y=%.3f"), Input.X, Input.Y);
    AddYawInput(Input.X);
    AddPitchInput(Input.Y);
}

void ACVADPlayerController::StartJump()
{
    UE_LOG(LogCVADInput, Log, TEXT("Input Jump Started Pawn=%s"), *GetNameSafe(GetPawn()));
    if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn())) ControlledCharacter->Jump();
}

void ACVADPlayerController::StopJump()
{
    UE_LOG(LogCVADInput, Log, TEXT("Input Jump Completed Pawn=%s"), *GetNameSafe(GetPawn()));
    if (ACharacter* ControlledCharacter = Cast<ACharacter>(GetPawn())) ControlledCharacter->StopJumping();
}

void ACVADPlayerController::StartSprint()
{
    if (ACVADCharacter* C = Cast<ACVADCharacter>(GetPawn())) C->SetSprinting(true);
}

void ACVADPlayerController::StopSprint()
{
    if (ACVADCharacter* C = Cast<ACVADCharacter>(GetPawn())) C->SetSprinting(false);
}

void ACVADPlayerController::Interact()
{
    UE_LOG(LogCVADInput, Log, TEXT("Input Interact/Revive"));
    if (ACVADCharacter* CVADPawn = Cast<ACVADCharacter>(GetPawn())) CVADPawn->ServerTryReviveNearbyPlayer();
}

void ACVADPlayerController::OnLightAttackPressed() { UE_LOG(LogCVADInput, Log, TEXT("Input LightAttack")); if (ACVADCharacter* C = Cast<ACVADCharacter>(GetPawn())) C->ActivateCombatInput(ECVADAbilityInput::LightAttack); }
void ACVADPlayerController::OnHeavyAttackPressed() { UE_LOG(LogCVADInput, Log, TEXT("Input HeavyAttack")); if (ACVADCharacter* C = Cast<ACVADCharacter>(GetPawn())) C->ActivateCombatInput(ECVADAbilityInput::HeavyAttack); }
void ACVADPlayerController::OnDodgePressed() { UE_LOG(LogCVADInput, Log, TEXT("Input Dodge")); if (ACVADCharacter* C = Cast<ACVADCharacter>(GetPawn())) C->ActivateCombatInput(ECVADAbilityInput::Dodge); }
void ACVADPlayerController::OnFlyingSwordPressed() { UE_LOG(LogCVADInput, Log, TEXT("Input FlyingSword")); if (ACVADCharacter* C = Cast<ACVADCharacter>(GetPawn())) C->ActivateCombatInput(ECVADAbilityInput::FlyingSword); }
void ACVADPlayerController::OnSwitchStancePressed() { UE_LOG(LogCVADInput, Log, TEXT("Input SwitchStance")); if (ACVADCharacter* C = Cast<ACVADCharacter>(GetPawn())) C->ActivateCombatInput(ECVADAbilityInput::SwitchStance); }

void ACVADPlayerController::ToggleInventory()
{
    UE_LOG(LogCVADInput, Log, TEXT("Input Inventory Toggle CurrentVisible=%s"),
        InventoryWidget && InventoryWidget->IsInViewport() ? TEXT("true") : TEXT("false"));
    if (!IsLocalController() || !InventoryWidgetClass) return;
    if (InventoryWidget && InventoryWidget->IsInViewport())
    {
        InventoryWidget->RemoveFromParent();
        SetInputMode(FInputModeGameOnly());
        bShowMouseCursor = false;
        return;
    }
    InventoryWidget = CreateWidget<UCVADUserWidget>(this, InventoryWidgetClass);
    if (InventoryWidget)
    {
        InventoryWidget->AddToViewport(10);
        SetInputMode(FInputModeGameAndUI());
        bShowMouseCursor = true;
    }
}

void ACVADPlayerController::TogglePauseMenu()
{
    UE_LOG(LogCVADInput, Log, TEXT("Input Pause Toggle"));
    if (PauseWidget && PauseWidget->IsInViewport())
    {
        PauseWidget->ResumeGame();
        PauseWidget = nullptr;
        return;
    }
    if (PauseWidgetClass)
    {
        PauseWidget = CreateWidget<UCVADUserWidget>(this, PauseWidgetClass);
        if (PauseWidget)
        {
            PauseWidget->AddToViewport(20);
            if (GetWorld() && GetWorld()->GetNetMode() == NM_Standalone) UGameplayStatics::SetGamePaused(this, true);
            SetInputMode(FInputModeGameAndUI());
            bShowMouseCursor = true;
        }
    }
    OnTogglePauseMenuRequested();
}

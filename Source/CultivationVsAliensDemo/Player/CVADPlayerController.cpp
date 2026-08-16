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
#include "UI/CVADMenuWidget.h"
#include "Blueprint/UserWidget.h"
#include "Camera/PlayerCameraManager.h"
#include "Kismet/GameplayStatics.h"
#include "Battle/CVADBattleDirector.h"
#include "EngineUtils.h"
#include "Misc/ConfigCacheIni.h"
#include "Save/CVADSaveGame.h"
#include "Player/CVADPlayerState.h"
#include "TimerManager.h"
#include "GameFramework/GameStateBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogCVADInput, Log, All);
static const TCHAR* CVADInputConfigSection = TEXT("CVAD.InputBindings");
static const TCHAR* CVADControlConfigSection = TEXT("CVAD.ControlSettings");

ACVADPlayerController::ACVADPlayerController()
{
    PrimaryActorTick.bCanEverTick = true;
}

void ACVADPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if(GEngine && NetworkFailureHandle.IsValid()) GEngine->OnNetworkFailure().Remove(NetworkFailureHandle);
    NetworkFailureHandle.Reset();
    Super::EndPlay(EndPlayReason);
}

void ACVADPlayerController::HandleNetworkFailure(UWorld* FailedWorld,UNetDriver*,ENetworkFailure::Type FailureType,const FString& ErrorString)
{
    if(!IsLocalController() || (FailedWorld && FailedWorld!=GetWorld())) return;
    FString FriendlyReason=ErrorString;
    if(ErrorString.Contains(TEXT("LobbyFull"))) FriendlyReason=TEXT("大厅已满，最多两名玩家");
    else if(FailureType==ENetworkFailure::ConnectionTimeout) FriendlyReason=TEXT("连接超时，请检查主机地址和防火墙");
    else if(FailureType==ENetworkFailure::ConnectionLost) FriendlyReason=TEXT("与主机的连接已断开");
    if(GConfig){GConfig->SetString(TEXT("CVAD.Network"),TEXT("LastError"),*FriendlyReason,GGameUserSettingsIni);GConfig->Flush(false,GGameUserSettingsIni);}
    UE_LOG(LogCVADInput,Error,TEXT("Network failure Type=%d Reason=%s"),static_cast<int32>(FailureType),*FriendlyReason);
    // Opening a map from inside the net-driver failure delegate can tear down the
    // world recursively. Defer it until the delegate has completely unwound.
    if(UWorld* World=GetWorld())
    {
        TWeakObjectPtr<ACVADPlayerController> WeakThis(this);
        World->GetTimerManager().SetTimerForNextTick([WeakThis]()
        {
            if(WeakThis.IsValid()) UGameplayStatics::OpenLevel(WeakThis.Get(),TEXT("L_MainMenu"));
        });
    }
}

void ACVADPlayerController::PlayerTick(float DeltaTime)
{
    Super::PlayerTick(DeltaTime);
    const bool bFrontend=GetWorld()&&GetWorld()->GetMapName().Contains(TEXT("L_MainMenu"));
    if(IsLocalController()&&!bFrontend&&!bGameplayInputRestored) RestoreGameplayInputState();
    if (IsLocalController() && bMouseFacingEnabled) UpdateMouseFacing();
    if (IsLocalController() && !bResultShown && ResultWidgetClass)
    {
        const ACVADCharacter* LocalCharacter=Cast<ACVADCharacter>(GetPawn());
        const bool bLocalPlayerDown=LocalCharacter && LocalCharacter->IsPlayerDown();
        bool bBattleFinished=false;
        for (TActorIterator<ACVADBattleDirector> It(GetWorld()); It; ++It)
        {
            bBattleFinished=It->BattlePhase==ECVADBattlePhase::Results;
            break;
        }
        if(bLocalPlayerDown || bBattleFinished)
        {
            ResultWidget = CreateWidget<UCVADUserWidget>(this, ResultWidgetClass);
            if (ResultWidget)
            {
                ResultWidget->AddToViewport(30);
                SetInputMode(FInputModeUIOnly());
                bShowMouseCursor = true;
                SetIgnoreMoveInput(true);
                SetIgnoreLookInput(false);
                bResultShown = true;
                UE_LOG(LogCVADInput, Log, TEXT("Battle result/death screen shown PlayerDown=%s"),bLocalPlayerDown?TEXT("true"):TEXT("false"));
            }
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

void ACVADPlayerController::SetMouseSensitivity(float NewSensitivity)
{
    MouseSensitivity = FMath::Clamp(NewSensitivity, 0.1f, 3.f);
    if (GConfig)
    {
        GConfig->SetFloat(CVADControlConfigSection, TEXT("MouseSensitivity"), MouseSensitivity, GGameUserSettingsIni);
        GConfig->SetBool(CVADControlConfigSection, TEXT("MouseFacing"), bMouseFacingEnabled, GGameUserSettingsIni);
        GConfig->Flush(false, GGameUserSettingsIni);
    }
    UE_LOG(LogCVADInput, Log, TEXT("Mouse sensitivity set to %.2f"), MouseSensitivity);
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
    // Only the listen host owns session-wide travel. A remote client must never
    // be able to restart the match for every connected player.
    if (HasAuthority() && IsLocalController()) RestartBattleAuthority();
    else UE_LOG(LogCVADInput, Warning, TEXT("Restart rejected: only the listen host can restart the battle"));
}

void ACVADPlayerController::ServerRestartBattle_Implementation()
{
    if (IsLocalController()) RestartBattleAuthority();
    else UE_LOG(LogCVADInput, Warning, TEXT("Remote restart RPC rejected"));
}

void ACVADPlayerController::RestartBattleAuthority()
{
    if (!HasAuthority() || !GetWorld()) return;
    UE_LOG(LogCVADInput, Log, TEXT("Restarting battle for all connected players"));
    GetWorld()->ServerTravel(TEXT("/Game/CVAD/Maps/L_CastleBattle?listen"), false);
}

void ACVADPlayerController::RequestReturnToMainMenu()
{
    if (HasAuthority() && IsLocalController()) ServerReturnToMainMenu_Implementation();
    else
    {
        UE_LOG(LogCVADInput, Log, TEXT("Client leaving session and returning to main menu"));
        ClientTravel(TEXT("/Game/CVAD/Maps/L_MainMenu"), TRAVEL_Absolute);
    }
}

void ACVADPlayerController::ServerReturnToMainMenu_Implementation()
{
    if (!GetWorld() || !IsLocalController()) { UE_LOG(LogCVADInput,Warning,TEXT("Remote return-to-menu RPC rejected")); return; }
    UE_LOG(LogCVADInput, Log, TEXT("Returning all players to main menu"));
    GetWorld()->ServerTravel(TEXT("/Game/CVAD/Maps/L_MainMenu"), true);
}

bool ACVADPlayerController::IsLobbyHost() const { return GetNetMode()!=NM_Client; }
void ACVADPlayerController::RequestStartLobbyGame(){if(HasAuthority()) ServerStartLobbyGame_Implementation();else ServerStartLobbyGame();}
void ACVADPlayerController::ServerStartLobbyGame_Implementation()
{
    if(!GetWorld() || !IsLobbyHost()) return;
    AGameStateBase* GS=GetWorld()->GetGameState();
    if(!GS || GS->PlayerArray.IsEmpty() || GS->PlayerArray.Num()>2) return;
    UE_LOG(LogCVADInput,Log,TEXT("Listen host starting battle with %d player(s); ready state is informational"),GS->PlayerArray.Num());
    GetWorld()->ServerTravel(TEXT("/Game/CVAD/Maps/L_CastleBattle?listen"),false);
}

void ACVADPlayerController::BeginPlay()
{
    Super::BeginPlay();
    if (!PlayerMappingContext) PlayerMappingContext = LoadObject<UInputMappingContext>(nullptr, TEXT("/Game/CVAD/Input/IMC_Player.IMC_Player"));
    if (!InventoryAction) InventoryAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/CVAD/Input/Actions/IA_Inventory.IA_Inventory"));
    if (!PauseAction) PauseAction = LoadObject<UInputAction>(nullptr, TEXT("/Game/CVAD/Input/Actions/IA_Pause.IA_Pause"));
    if(IsLocalController() && GEngine && !NetworkFailureHandle.IsValid())
        NetworkFailureHandle=GEngine->OnNetworkFailure().AddUObject(this,&ThisClass::HandleNetworkFailure);
    if (!HUDWidgetClass) HUDWidgetClass = LoadClass<UCVADUserWidget>(nullptr, TEXT("/Game/CVAD/UI/WBP_HUD.WBP_HUD_C"));
    if (!InventoryWidgetClass) InventoryWidgetClass = LoadClass<UCVADUserWidget>(nullptr, TEXT("/Game/CVAD/UI/WBP_Inventory.WBP_Inventory_C"));
    if (!PauseWidgetClass) PauseWidgetClass = LoadClass<UCVADUserWidget>(nullptr, TEXT("/Game/CVAD/UI/WBP_Pause.WBP_Pause_C"));
    if (!ResultWidgetClass) ResultWidgetClass = LoadClass<UCVADUserWidget>(nullptr, TEXT("/Game/CVAD/UI/WBP_Result.WBP_Result_C"));
    if (!LobbyWidgetClass) LobbyWidgetClass = LoadClass<UCVADUserWidget>(nullptr, TEXT("/Game/CVAD/UI/WBP_Lobby.WBP_Lobby_C"));
    if (!SettingsWidgetClass) SettingsWidgetClass = LoadClass<UCVADUserWidget>(nullptr, TEXT("/Game/CVAD/UI/WBP_Settings.WBP_Settings_C"));
    if (!SkillTreeWidgetClass) SkillTreeWidgetClass = LoadClass<UCVADUserWidget>(nullptr, TEXT("/Game/CVAD/UI/WBP_SkillTree.WBP_SkillTree_C"));
    if (!SaveSlotsWidgetClass) SaveSlotsWidgetClass = LoadClass<UCVADUserWidget>(nullptr, TEXT("/Game/CVAD/UI/WBP_SaveSlots.WBP_SaveSlots_C"));
    if (!NameEntryWidgetClass) NameEntryWidgetClass = LoadClass<UCVADUserWidget>(nullptr, TEXT("/Game/CVAD/UI/WBP_NameEntry.WBP_NameEntry_C"));
    if (!OutfitWidgetClass) OutfitWidgetClass = LoadClass<UCVADUserWidget>(nullptr, TEXT("/Game/CVAD/UI/WBP_OutfitSelect.WBP_OutfitSelect_C"));
    if (!MultiplayerWidgetClass) MultiplayerWidgetClass = LoadClass<UCVADUserWidget>(nullptr, TEXT("/Game/CVAD/UI/WBP_Multiplayer.WBP_Multiplayer_C"));
    UE_LOG(LogCVADInput, Log, TEXT("UI/Input assets IMC=%s InventoryAction=%s PauseAction=%s InventoryUI=%s PauseUI=%s"),
        *GetNameSafe(PlayerMappingContext), *GetNameSafe(InventoryAction), *GetNameSafe(PauseAction),
        *GetNameSafe(InventoryWidgetClass), *GetNameSafe(PauseWidgetClass));
    const bool bMainMenuMap = GetWorld() && GetWorld()->GetMapName().Contains(TEXT("L_MainMenu"));
    if (IsLocalController() && bMainMenuMap)
    {
        // The listen host retains the Lobby URL option, but connected clients do
        // not reliably receive that local URL option. A client on L_MainMenu is
        // necessarily connected to the listen lobby and must see the lobby UI.
        const bool bLobbyMode=GetNetMode()==NM_Client||GetWorld()->URL.HasOption(TEXT("Lobby"));
        if(bLobbyMode && LobbyWidgetClass)
        {
            LobbyWidget=CreateWidget<UCVADUserWidget>(this,LobbyWidgetClass);
            if(LobbyWidget) LobbyWidget->AddToViewport(110);
            SetInputMode(FInputModeUIOnly()); bShowMouseCursor=true;
            UE_LOG(LogCVADInput,Log,TEXT("Lobby UI initialized Host=%s"),IsLobbyHost()?TEXT("true"):TEXT("false"));
            return;
        }
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
    if(IsLocalController()) RestoreGameplayInputState();
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
        if (GConfig)
        {
            GConfig->GetFloat(CVADControlConfigSection, TEXT("MouseSensitivity"), MouseSensitivity, GGameUserSettingsIni);
            GConfig->GetBool(CVADControlConfigSection, TEXT("MouseFacing"), bMouseFacingEnabled, GGameUserSettingsIni);
            MouseSensitivity = FMath::Clamp(MouseSensitivity, 0.1f, 3.f);
        }
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
    const FString SlotName = UCVADUserWidget::GetProfileSlotName(UCVADUserWidget::GetLastUsedProfileSlot());
    UCVADSaveGame* Save = Cast<UCVADSaveGame>(UGameplayStatics::LoadGameFromSlot(SlotName, 0));
    ACVADPlayerState* PS = GetPlayerState<ACVADPlayerState>();
    if (!Save || !PS) { UE_LOG(LogCVADInput, Warning, TEXT("Startup profile load failed")); return; }
    ServerChangeName(Save->PlayerDisplayName.Left(20));
    PS->RequestRestoreProfile(Save->PlayerLevel, Save->Experience, Save->SkillPoints,
        Save->UnlockedSkillRows, Save->EquippedSkillRows, Save->UnlockedItemIds, Save->EquipmentLoadout);
    UE_LOG(LogCVADInput, Log, TEXT("Startup profile submitted for restoration"));
}

const UInputAction* ACVADPlayerController::FindInputAction(FName ActionName) const
{
    if (ActionName == TEXT("MoveForward") || ActionName == TEXT("MoveBack") ||
        ActionName == TEXT("MoveLeft") || ActionName == TEXT("MoveRight")) return MoveAction;
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
        TEXT("FlyingSword"),TEXT("SwitchStance"),TEXT("Inventory"),TEXT("Pause"),TEXT("Sprint"),TEXT("Interact"),
        TEXT("MoveForward"),TEXT("MoveBack"),TEXT("MoveLeft"),TEXT("MoveRight")};
    for (const FName Name : Names)
    {
        FString KeyName;
        const bool bHasSavedKey=GConfig->GetString(CVADInputConfigSection,*Name.ToString(),KeyName,GGameUserSettingsIni)&&!KeyName.IsEmpty();
        if(bHasSavedKey&&Name==TEXT("Pause")&&KeyName==TEXT("Escape"))
        {
            KeyName=TEXT("P");
            GConfig->SetString(CVADInputConfigSection,TEXT("Pause"),TEXT("P"),GGameUserSettingsIni);
            GConfig->Flush(false,GGameUserSettingsIni);
        }
        if(bHasSavedKey) RebindAction(Name,FKey(*KeyName));
    }
}

bool ACVADPlayerController::RebindAction(FName ActionName, FKey NewKey)
{
    const UInputAction* Action = FindInputAction(ActionName);
    if (!RuntimeMappingContext || !Action || !NewKey.IsValid()) return false;
    const bool bMoveDirection = ActionName.ToString().StartsWith(TEXT("Move")) && ActionName != TEXT("Move");
    if (bMoveDirection)
    {
        FKey OldKey = GetBoundKey(ActionName);
        TArray<TObjectPtr<UInputModifier>> PreservedModifiers;
        TArray<TObjectPtr<UInputTrigger>> PreservedTriggers;
        for (const FEnhancedActionKeyMapping& Mapping : RuntimeMappingContext->GetMappings())
        {
            if (Mapping.Action == Action && Mapping.Key == OldKey)
            {
                PreservedModifiers = Mapping.Modifiers;
                PreservedTriggers = Mapping.Triggers;
                break;
            }
        }
        if (OldKey.IsValid()) RuntimeMappingContext->UnmapKey(Action, OldKey);
        FEnhancedActionKeyMapping& NewMapping = RuntimeMappingContext->MapKey(Action, NewKey);
        NewMapping.Modifiers = MoveTemp(PreservedModifiers);
        NewMapping.Triggers = MoveTemp(PreservedTriggers);
    }
    else
    {
        RuntimeMappingContext->UnmapAllKeysFromAction(Action);
        RuntimeMappingContext->MapKey(Action, NewKey);
    }
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
    {
        const TMap<FName, FKey> DefaultDirections = {{TEXT("MoveForward"),EKeys::W},{TEXT("MoveBack"),EKeys::S},
            {TEXT("MoveLeft"),EKeys::A},{TEXT("MoveRight"),EKeys::D}};
        if (const FKey* DefaultKey = DefaultDirections.Find(ActionName))
        {
            FString SavedKey;
            if (GConfig && GConfig->GetString(CVADInputConfigSection, *ActionName.ToString(), SavedKey, GGameUserSettingsIni) && !SavedKey.IsEmpty())
                return FKey(*SavedKey);
            return *DefaultKey;
        }
        for (const FEnhancedActionKeyMapping& Mapping : RuntimeMappingContext->GetMappings())
            if (Mapping.Action == Action) return Mapping.Key;
    }
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
    // Skill learning and loadout are frontend-only, so the old inventory action
    // is intentionally not bound while playing a battle map.
    if (PauseAction) Enhanced->BindAction(PauseAction, ETriggerEvent::Started, this, &ThisClass::TogglePauseMenu);
    if (SprintAction)
    {
        Enhanced->BindAction(SprintAction, ETriggerEvent::Started, this, &ThisClass::StartSprint);
        Enhanced->BindAction(SprintAction, ETriggerEvent::Completed, this, &ThisClass::StopSprint);
    }
    if (InteractAction) Enhanced->BindAction(InteractAction, ETriggerEvent::Started, this, &ThisClass::Interact);
    // Direct UI shortcuts remain available even if an IMC asset is edited later.
    InputComponent->BindKey(EKeys::F1, IE_Pressed, this, &ThisClass::ShowSettingsScreen);
    InputComponent->BindKey(EKeys::F5, IE_Pressed, this, &ThisClass::ShowSaveSlotsScreen);
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
    AddYawInput(Input.X * MouseSensitivity);
    AddPitchInput(Input.Y * MouseSensitivity);
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
    UE_LOG(LogCVADInput, Log, TEXT("Skill loadout input ignored during battle; configure it from the main menu"));
}

void ACVADPlayerController::ShowModalWidget(TSubclassOf<UCVADUserWidget> WidgetClass, int32 ZOrder)
{
    if (!IsLocalController() || !WidgetClass) return;
    if (ActiveModalWidget && ActiveModalWidget->IsInViewport()) ActiveModalWidget->RemoveFromParent();
    ActiveModalWidget = CreateWidget<UCVADUserWidget>(this, WidgetClass);
    if (!ActiveModalWidget) return;
    // Frontend roots use ZOrder 100 (main menu) and 110 (lobby). Keep every
    // modal above those roots even when an older caller supplies a low value.
    constexpr int32 ModalBaseZOrder = 200;
    ActiveModalWidget->AddToViewport(FMath::Max(ZOrder, ModalBaseZOrder));
    FInputModeUIOnly ModalInputMode;
    ModalInputMode.SetWidgetToFocus(ActiveModalWidget->TakeWidget());
    ModalInputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
    SetInputMode(ModalInputMode);
    bShowMouseCursor = true;
    UE_LOG(LogCVADInput, Log, TEXT("Opened modal UI %s"), *GetNameSafe(ActiveModalWidget));
}

void ACVADPlayerController::ShowSettingsScreen() { ShowModalWidget(SettingsWidgetClass); }
void ACVADPlayerController::ShowSkillTreeScreen()
{
    const bool bFrontend = GetWorld() && GetWorld()->GetMapName().Contains(TEXT("L_MainMenu"));
    if (bFrontend) ShowModalWidget(SkillTreeWidgetClass);
    else UE_LOG(LogCVADInput, Warning, TEXT("Skill tree rejected outside frontend"));
}
void ACVADPlayerController::ShowInventoryScreen() { UE_LOG(LogCVADInput, Log, TEXT("Inventory/equipment feature disabled")); }
void ACVADPlayerController::ShowSaveSlotsScreen() { ShowModalWidget(SaveSlotsWidgetClass, 50); }
void ACVADPlayerController::ShowNameEntryScreen()
{
    ShowModalWidget(NameEntryWidgetClass, 60);
}
void ACVADPlayerController::ShowOutfitScreen()
{
    const bool bFrontend=GetWorld()&&GetWorld()->GetMapName().Contains(TEXT("L_MainMenu"));
    if(bFrontend) ShowModalWidget(OutfitWidgetClass,55);
}

void ACVADPlayerController::RestoreGameplayInputState()
{
    if(!IsLocalController()) return;
    if(GetWorld()&&UGameplayStatics::IsGamePaused(this)) UGameplayStatics::SetGamePaused(this,false);
    ResetIgnoreMoveInput();
    ResetIgnoreLookInput();
    SetCinematicMode(false,false,false,true,true);
    SetInputMode(FInputModeGameOnly());
    bShowMouseCursor=false;
    bEnableClickEvents=false;
    bEnableMouseOverEvents=false;
    FlushPressedKeys();
    bGameplayInputRestored=true;
    UE_LOG(LogCVADInput,Log,TEXT("Gameplay input restored Paused=%s MoveIgnored=%s LookIgnored=%s"),
        UGameplayStatics::IsGamePaused(this)?TEXT("true"):TEXT("false"),
        IsMoveInputIgnored()?TEXT("true"):TEXT("false"),IsLookInputIgnored()?TEXT("true"):TEXT("false"));
}
void ACVADPlayerController::ShowMultiplayerScreen() { ShowModalWidget(MultiplayerWidgetClass,55); }
void ACVADPlayerController::SetPendingMenuAction(int32 Action,const FString& Address)
{
    PendingMenuAction=Action; PendingServerAddress=Address; ShowNameEntryScreen();
}
void ACVADPlayerController::ContinuePendingMenuAction()
{
    const int32 Action=PendingMenuAction; const FString Address=PendingServerAddress;
    PendingMenuAction=0; PendingServerAddress.Reset();
    UCVADMenuWidget* Menu=Cast<UCVADMenuWidget>(MainMenuWidget); if(!Menu) return;
    if(Action==1) Menu->StartSinglePlayer(); else if(Action==2) Menu->HostListenServer(); else if(Action==3) Menu->JoinServer(Address);
}

float ACVADPlayerController::ConsumeUnsavedPlayTime()
{
    const float Now=GetWorld()?GetWorld()->GetTimeSeconds():0.f;
    const float Delta=FMath::Max(0.f,Now-LastProfileSaveWorldTime);
    LastProfileSaveWorldTime=Now;
    return Delta;
}

void ACVADPlayerController::CloseTopScreen()
{
    if (ActiveModalWidget && ActiveModalWidget->IsInViewport()) ActiveModalWidget->RemoveFromParent();
    ActiveModalWidget = nullptr;
    const bool bMenuMap = GetWorld() && GetWorld()->GetMapName().Contains(TEXT("L_MainMenu"));
    const bool bPauseStillOpen = PauseWidget && PauseWidget->IsInViewport();
    if (bMenuMap) SetInputMode(FInputModeUIOnly());
    else if (bPauseStillOpen) SetInputMode(FInputModeGameAndUI());
    else SetInputMode(FInputModeGameOnly());
    bShowMouseCursor = bMenuMap || bPauseStillOpen;
    UE_LOG(LogCVADInput, Log, TEXT("Closed top modal UI"));
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
    else UE_LOG(LogCVADInput, Error, TEXT("Pause UI failed: PauseWidgetClass is null"));
    OnTogglePauseMenuRequested();
}

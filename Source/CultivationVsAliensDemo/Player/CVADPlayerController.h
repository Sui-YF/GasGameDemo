#pragma once

#include "GameFramework/PlayerController.h"
#include "CVADPlayerController.generated.h"

class UInputAction;
class UInputMappingContext;
class UCVADUserWidget;
struct FInputActionValue;
struct FKey;

UCLASS(Blueprintable)
class CULTIVATIONVSALIENSDEMO_API ACVADPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ACVADPlayerController();
    virtual void BeginPlay() override;
    virtual void PlayerTick(float DeltaTime) override;
    virtual void SetupInputComponent() override;

    /** Blueprint/UI button entry point: enables cursor-directed character facing. */
    UFUNCTION(BlueprintCallable, Category="Input|Facing")
    void SetMouseFacingEnabled(bool bEnabled);

    UFUNCTION(BlueprintPure, Category="Input|Facing")
    bool IsMouseFacingEnabled() const { return bMouseFacingEnabled; }

    /** Restarts the battle for every connected player. Clients forward the request to the listen server. */
    UFUNCTION(BlueprintCallable, Category="Battle") void RequestRestartBattle();
    UFUNCTION(BlueprintCallable, Category="Menu") void RequestReturnToMainMenu();
    UFUNCTION(BlueprintCallable, Category="Input|Rebinding") bool RebindAction(FName ActionName, FKey NewKey);
    UFUNCTION(BlueprintCallable, Category="Input|Rebinding") void ResetInputBindings();
    UFUNCTION(BlueprintPure, Category="Input|Rebinding") FKey GetBoundKey(FName ActionName) const;

protected:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputMappingContext> PlayerMappingContext;
    UPROPERTY(Transient) TObjectPtr<UInputMappingContext> RuntimeMappingContext;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> MoveAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> LookAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> JumpAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> LightAttackAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> HeavyAttackAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> DodgeAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> FlyingSwordAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> SwitchStanceAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> InventoryAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> PauseAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> SprintAction;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Input") TObjectPtr<UInputAction> InteractAction;

    void Move(const FInputActionValue& Value);
    void Look(const FInputActionValue& Value);
    void StartJump();
    void StopJump();
    void StartSprint();
    void StopSprint();
    void Interact();

    void OnLightAttackPressed();
    void OnHeavyAttackPressed();
    void OnDodgePressed();
    void OnFlyingSwordPressed();
    void OnSwitchStancePressed();
    void ToggleInventory();
    void TogglePauseMenu();
    UFUNCTION(BlueprintImplementableEvent, Category="UI") void OnTogglePauseMenuRequested();

    UPROPERTY(EditDefaultsOnly, Category="UI") TSubclassOf<UCVADUserWidget> HUDWidgetClass;
    UPROPERTY(EditDefaultsOnly, Category="UI") TSubclassOf<UCVADUserWidget> InventoryWidgetClass;
    UPROPERTY(EditDefaultsOnly, Category="UI") TSubclassOf<UCVADUserWidget> PauseWidgetClass;
    UPROPERTY(EditDefaultsOnly, Category="UI") TSubclassOf<UCVADUserWidget> ResultWidgetClass;
    UPROPERTY(EditDefaultsOnly, Category="UI") TSubclassOf<UCVADUserWidget> MainMenuWidgetClass;
    UPROPERTY(Transient) TObjectPtr<UCVADUserWidget> HUDWidget;
    UPROPERTY(Transient) TObjectPtr<UCVADUserWidget> InventoryWidget;
    UPROPERTY(Transient) TObjectPtr<UCVADUserWidget> PauseWidget;
    UPROPERTY(Transient) TObjectPtr<UCVADUserWidget> ResultWidget;
    UPROPERTY(Transient) TObjectPtr<UCVADUserWidget> MainMenuWidget;
    bool bResultShown = false;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Facing") bool bMouseFacingEnabled = true;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Input|Facing", meta=(ClampMin="0.0")) float FacingInterpSpeed = 18.f;

    /** Initial camera pitch is always CameraPitchMin. Vertical mouse look is clamped to this range. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(ClampMin="-89.0", ClampMax="89.0")) float CameraPitchMin = -75.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(ClampMin="-89.0", ClampMax="89.0")) float CameraPitchMax = -15.f;
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category="Camera", meta=(ClampMin="-89.0", ClampMax="89.0")) float InitialCameraPitch = -20.f;

private:
    void ApplyStartupProfile();
    const UInputAction* FindInputAction(FName ActionName) const;
    void BuildRuntimeMappingContext();
    void ApplySavedInputBindings();
    void RebuildInputMappings() const;
    void UpdateMouseFacing();
    UFUNCTION(Server, Unreliable) void ServerSetFacingYaw(float Yaw);
    UFUNCTION(Server, Reliable) void ServerRestartBattle();
    UFUNCTION(Server, Reliable) void ServerReturnToMainMenu();
    void RestartBattleAuthority();
};

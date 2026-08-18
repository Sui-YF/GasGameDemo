#include "Misc/AutomationTest.h"

#if WITH_AUTOMATION_TESTS

#include "AIController.h"
#include "AbilitySystemComponent.h"
#include "AbilitySystem/Abilities/CVADCombatAbility.h"
#include "AbilitySystem/CVADAttributeSet.h"
#include "Battle/CVADBattleDirector.h"
#include "Battle/CVADMinionSpawner.h"
#include "BehaviorTree/BehaviorTreeComponent.h"
#include "Character/CVADCharacter.h"
#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Editor.h"
#include "Enemy/CVADEnemyAIController.h"
#include "Enemy/CVADEnemyCharacter.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "NavigationSystem.h"
#include "Settings/LevelEditorPlaySettings.h"
#include "Tests/AutomationEditorCommon.h"

namespace CVADGauntlet
{
    constexpr double WorldReadyTimeout = 30.0;
    constexpr double MovementDuration = 1.25;
    constexpr double MinionWaitDuration = 2.0;
    constexpr double FrontlineTimeout = 30.0;
    constexpr double BossSpawnTimeout = 15.0;
    constexpr double VictoryTimeout = 15.0;

    UWorld* GetPlayWorld()
    {
        if (GEngine)
        {
            for (const FWorldContext& Context : GEngine->GetWorldContexts())
            {
                if (Context.WorldType == EWorldType::PIE && Context.World()
                    && Context.World()->GetNetMode() == NM_ListenServer)
                {
                    return Context.World();
                }
            }
        }
        return GEditor ? GEditor->PlayWorld : nullptr;
    }

    template <typename T>
    T* FindFirstActor(UWorld* World)
    {
        for (TActorIterator<T> It(World); It; ++It)
        {
            return *It;
        }
        return nullptr;
    }

    bool CheckAsset(FAutomationTestBase& Test, const TCHAR* Path)
    {
        UObject* Asset = StaticLoadObject(UObject::StaticClass(), nullptr, Path);
        return Test.TestNotNull(FString::Printf(TEXT("Asset %s"), Path), Asset);
    }
}

class FCVADPlaythroughCommand final : public IAutomationLatentCommand
{
public:
    explicit FCVADPlaythroughCommand(FAutomationTestBase* InTest)
        : Test(InTest)
        , CommandStartTime(FPlatformTime::Seconds())
    {
    }

    virtual bool Update() override
    {
        UWorld* World = CVADGauntlet::GetPlayWorld();
        if (!World)
        {
            if (FPlatformTime::Seconds() - CommandStartTime > CVADGauntlet::WorldReadyTimeout)
            {
                Test->AddError(TEXT("PIE world did not become ready within 30 seconds."));
                return true;
            }
            return false;
        }

        switch (Stage)
        {
        case 0:
            if (!IsNetworkSessionReady(World))
            {
                if (FPlatformTime::Seconds() - CommandStartTime > CVADGauntlet::WorldReadyTimeout)
                {
                    Test->AddError(TEXT("Listen-server PIE did not produce a server player and connected client."));
                    return true;
                }
                return false;
            }
            BeginPlayerChecks(World);
            return false;
        case 1:
            DrivePlayer(World);
            return false;
        case 2:
            CheckMinionsAndDriveFrontline(World);
            return false;
        case 3:
            return CheckBossesAndVictory(World);
        default:
            return true;
        }
    }

private:
    bool IsNetworkSessionReady(UWorld* World) const
    {
        if (!CVADGauntlet::FindFirstActor<ACVADCharacter>(World)) return false;

        int32 ClientWorlds = 0;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::PIE && Context.World()
                && Context.World()->GetNetMode() == NM_Client)
            {
                ++ClientWorlds;
            }
        }
        return ClientWorlds == 1;
    }

    void BeginPlayerChecks(UWorld* World)
    {
        int32 ListenServerWorlds = 0;
        int32 ClientWorlds = 0;
        for (const FWorldContext& Context : GEngine->GetWorldContexts())
        {
            if (Context.WorldType != EWorldType::PIE || !Context.World()) continue;
            ListenServerWorlds += Context.World()->GetNetMode() == NM_ListenServer ? 1 : 0;
            ClientWorlds += Context.World()->GetNetMode() == NM_Client ? 1 : 0;
        }
        Test->TestEqual(TEXT("PIE created one listen-server world"), ListenServerWorlds, 1);
        Test->TestEqual(TEXT("PIE created one connected client world"), ClientWorlds, 1);

        Player = CVADGauntlet::FindFirstActor<ACVADCharacter>(World);
        Test->TestNotNull(TEXT("Player character spawned"), Player.Get());
        if (!Player.IsValid())
        {
            Stage = 3;
            StageStartTime = FPlatformTime::Seconds();
            return;
        }

        Test->TestNotNull(TEXT("Player has an AbilitySystemComponent"), Player->GetAbilitySystemComponent());
        Test->TestNotNull(TEXT("Player has a skeletal mesh"), Player->GetMesh());
        Test->TestNotNull(TEXT("Player has a locomotion AnimInstance"),
            Player->GetMesh() ? Player->GetMesh()->GetAnimInstance() : nullptr);
        Test->TestTrue(TEXT("Player is server-authoritative in the smoke session"), Player->HasAuthority());

        int32 AbilityCount = 0;
        if (UAbilitySystemComponent* ASC = Player->GetAbilitySystemComponent())
        {
            AbilityCount = ASC->GetActivatableAbilities().Num();
        }
        Test->TestTrue(TEXT("Default GAS abilities were granted"), AbilityCount >= 5);

        Director = CVADGauntlet::FindFirstActor<ACVADBattleDirector>(World);
        for (TActorIterator<ACVADMinionSpawner> It(World); It; ++It)
        {
            if (*It) Spawners.Add(*It);
        }
        Spawner = Spawners.Num() > 0 ? Spawners[0] : nullptr;
        Test->TestTrue(TEXT("Battle map contains at least one minion spawner"), Spawners.Num() > 0);
        Test->TestNotNull(TEXT("Battle map contains a battle director"), Director.Get());

        for (TWeakObjectPtr<ACVADMinionSpawner> CurrentSpawner : Spawners)
        {
            if (CurrentSpawner.IsValid()) CurrentSpawner->StartSpawning();
        }

        InitialPlayerLocation = Player->GetActorLocation();
        StageStartTime = FPlatformTime::Seconds();
        Stage = 1;
        UE_LOG(LogTemp, Display, TEXT("CVAD_GAUNTLET Stage=PlayerReady Abilities=%d"), AbilityCount);
    }

    void DrivePlayer(UWorld*)
    {
        if (!Player.IsValid())
        {
            Test->AddError(TEXT("Player was destroyed during movement test."));
            Stage = 2;
            StageStartTime = FPlatformTime::Seconds();
            return;
        }

        Player->AddMovementInput(FVector::ForwardVector, 1.f, true);
        if (FPlatformTime::Seconds() - StageStartTime < CVADGauntlet::MovementDuration)
        {
            return;
        }

        const float TravelDistance = FVector::Dist2D(InitialPlayerLocation, Player->GetActorLocation());
        Test->TestTrue(TEXT("Player moved under automated input"), TravelDistance >= 30.f);

        Player->ActivateCombatInput(ECVADAbilityInput::LightAttack);
        Player->ActivateCombatInput(ECVADAbilityInput::HeavyAttack);
        const bool bPreviousStance = Player->IsFlyingSwordMode();
        Player->ToggleFlyingSwordMode();
        Test->TestNotEqual(TEXT("Flying-sword stance toggled"), Player->IsFlyingSwordMode(), bPreviousStance);
        Player->ActivateCombatInput(ECVADAbilityInput::FlyingSword);

        StageStartTime = FPlatformTime::Seconds();
        Stage = 2;
        UE_LOG(LogTemp, Display, TEXT("CVAD_GAUNTLET Stage=CombatInput Travel=%.1f"), TravelDistance);
    }

    void CheckMinionsAndDriveFrontline(UWorld* World)
    {
        if (!bMinionsValidated)
        {
            if (FPlatformTime::Seconds() - StageStartTime < CVADGauntlet::MinionWaitDuration)
            {
                return;
            }

            int32 MinionCount = 0;
            for (TActorIterator<ACVADEnemyCharacter> It(World); It; ++It)
            {
                ACVADEnemyCharacter* Enemy = *It;
                if (!Enemy || Enemy->IsBoss()) continue;
                ++MinionCount;

                Test->TestNotNull(TEXT("Minion mesh is configured"), Enemy->GetMesh()->GetSkeletalMeshAsset());
                Test->TestNotNull(TEXT("Minion animation is running"), Enemy->GetMesh()->GetAnimInstance());

                const ACVADEnemyAIController* AI = Cast<ACVADEnemyAIController>(Enemy->GetController());
                Test->TestNotNull(TEXT("Minion is possessed by CVAD AIController"), AI);
                if (AI)
                {
                    Test->TestNotNull(TEXT("Minion has a running brain component"), AI->GetBrainComponent());
                    Test->TestNotNull(TEXT("Minion Behavior Tree initialized Blackboard"), AI->GetBlackboardComponent());
                }

                FNavLocation FloorLocation;
                if (const UNavigationSystemV1* Nav = FNavigationSystem::GetCurrent<UNavigationSystemV1>(World);
                    Nav && Nav->ProjectPointToNavigation(Enemy->GetActorLocation(), FloorLocation, FVector(100.f, 100.f, 300.f)))
                {
                    const float CapsuleBottom = Enemy->GetActorLocation().Z
                        - Enemy->GetCapsuleComponent()->GetScaledCapsuleHalfHeight();
                    if (CapsuleBottom < FloorLocation.Location.Z - 10.f)
                    {
                        UE_LOG(LogTemp, Log,
                            TEXT("CVAD_GAUNTLET minion %s capsule bottom %.1f is below navigation floor %.1f"),
                            *GetNameSafe(Enemy), CapsuleBottom, FloorLocation.Location.Z);
                    }
                }
            }

            Test->TestTrue(TEXT("Spawner produced at least one minion"), MinionCount > 0);
            bMinionsValidated = true;
            if (MinionCount == 0)
            {
                Test->AddError(TEXT("No minions were available to drive the frontline objective."));
                StageStartTime = FPlatformTime::Seconds();
                Stage = 3;
                return;
            }
            UE_LOG(LogTemp, Display, TEXT("CVAD_GAUNTLET Stage=MinionsValidated Count=%d"), MinionCount);
        }

        DefeatMinions(World);
        if (Director.IsValid() && Director->IsBossStageReady())
        {
            Test->TestTrue(TEXT("Frontline kill quota advanced the battle director to Boss phase"), true);
            StageStartTime = FPlatformTime::Seconds();
            Stage = 3;
            UE_LOG(LogTemp, Display, TEXT("CVAD_GAUNTLET Stage=BossReady Defeats=%d"),
                Director.IsValid() ? Director->DefeatCount : -1);
            return;
        }

        if (FPlatformTime::Seconds() - StageStartTime > CVADGauntlet::FrontlineTimeout)
        {
            Test->AddError(TEXT("Frontline phase did not advance to Boss within 30 seconds."));
            StageStartTime = FPlatformTime::Seconds();
            Stage = 3;
        }
    }

    bool CheckBossesAndVictory(UWorld* World)
    {
        const double Elapsed = FPlatformTime::Seconds() - StageStartTime;

        if (!bBossesValidated)
        {
            int32 BossCount = CountBosses(World);
            if (BossCount < 3)
            {
                if (Elapsed < CVADGauntlet::BossSpawnTimeout)
                {
                    return false;
                }
                Test->AddError(FString::Printf(
                    TEXT("Boss stage did not spawn three opponents; found %d."), BossCount));
                return true;
            }

            for (TActorIterator<ACVADEnemyCharacter> It(World); It; ++It)
            {
                ACVADEnemyCharacter* Boss = *It;
                if (!Boss || !Boss->IsBoss()) continue;
                Test->TestNotNull(TEXT("Boss mesh is configured"), Boss->GetMesh()->GetSkeletalMeshAsset());
                Test->TestNotNull(TEXT("Boss is possessed by an AIController"), Boss->GetController());
                Boss->PlayBossAttackAnimation();
            }
            Test->TestEqual(TEXT("Boss stage spawned the three configured opponents"), BossCount, 3);
            Test->TestTrue(TEXT("Battle director entered Boss phase"),
                Director.IsValid() && Director->IsBossStageReady());
            bBossesValidated = true;
            UE_LOG(LogTemp, Display, TEXT("CVAD_GAUNTLET Stage=BossesValidated Bosses=%d"), BossCount);
        }

        DefeatBosses(World);
        if (Director.IsValid() && Director->bVictory)
        {
            Test->TestTrue(TEXT("Battle director recorded victory after all bosses were defeated"), true);
            Test->TestTrue(TEXT("Battle completed with a positive time"), Director->CompletionTimeSeconds > 0.f);
            Test->TestTrue(TEXT("Battle completed with experience reward"), Director->ExperienceEarned > 0);
            UE_LOG(LogTemp, Display,
                TEXT("CVAD_GAUNTLET Stage=Victory BossesRemaining=%d Time=%.1f XP=%d"),
                Director->BossesRemaining, Director->CompletionTimeSeconds, Director->ExperienceEarned);
            return true;
        }

        if (Elapsed > CVADGauntlet::VictoryTimeout)
        {
            Test->AddError(TEXT("Battle director did not reach victory after all bosses were defeated."));
            return true;
        }
        return false;
    }

    void DefeatMinions(UWorld* World)
    {
        for (TActorIterator<ACVADEnemyCharacter> It(World); It; ++It)
        {
            ACVADEnemyCharacter* Enemy = *It;
            if (!Enemy || Enemy->IsBoss()) continue;
            if (UAbilitySystemComponent* ASC = Enemy->GetAbilitySystemComponent())
            {
                ASC->SetNumericAttributeBase(UCVADAttributeSet::GetHealthAttribute(), 0.f);
            }
        }
    }

    void DefeatBosses(UWorld* World)
    {
        for (TActorIterator<ACVADEnemyCharacter> It(World); It; ++It)
        {
            ACVADEnemyCharacter* Boss = *It;
            if (!Boss || !Boss->IsBoss()) continue;
            if (UAbilitySystemComponent* ASC = Boss->GetAbilitySystemComponent())
            {
                ASC->SetNumericAttributeBase(UCVADAttributeSet::GetHealthAttribute(), 0.f);
            }
        }
    }

    int32 CountBosses(UWorld* World) const
    {
        int32 BossCount = 0;
        for (TActorIterator<ACVADEnemyCharacter> It(World); It; ++It)
        {
            const ACVADEnemyCharacter* Boss = *It;
            if (Boss && Boss->IsBoss())
            {
                ++BossCount;
            }
        }
        return BossCount;
    }

    FAutomationTestBase* Test = nullptr;
    TWeakObjectPtr<ACVADCharacter> Player;
    TWeakObjectPtr<ACVADMinionSpawner> Spawner;
    TArray<TWeakObjectPtr<ACVADMinionSpawner>> Spawners;
    TWeakObjectPtr<ACVADBattleDirector> Director;
    FVector InitialPlayerLocation = FVector::ZeroVector;
    double CommandStartTime = 0.0;
    double StageStartTime = 0.0;
    int32 Stage = 0;
    bool bMinionsValidated = false;
    bool bBossesValidated = false;
};

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCVADGauntletFullPlaythroughTest,
    "CVAD.Gauntlet.FullPlaythrough",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::ProductFilter)

bool FCVADGauntletFullPlaythroughTest::RunTest(const FString&)
{
    using namespace CVADGauntlet;

    CheckAsset(*this, TEXT("/Game/CVAD/AI/BT_EnemyCombat.BT_EnemyCombat"));
    CheckAsset(*this, TEXT("/Game/CVAD/AI/BB_EnemyCombat.BB_EnemyCombat"));
    CheckAsset(*this, TEXT("/Game/CVAD/Data/DT_Skills.DT_Skills"));
    CheckAsset(*this, TEXT("/Game/CVAD/Data/DT_PlayerBalance.DT_PlayerBalance"));
    CheckAsset(*this, TEXT("/Game/CVAD/Data/DT_EnemyBalance.DT_EnemyBalance"));
    CheckAsset(*this, TEXT("/Game/CVAD/Data/DT_SpawnerProfiles.DT_SpawnerProfiles"));
    CheckAsset(*this, TEXT("/Game/CVAD/Blueprints/Game/BP_CVADGameMode.BP_CVADGameMode"));
    CheckAsset(*this, TEXT("/Game/CVAD/Blueprints/Game/BP_CVADPlayerController.BP_CVADPlayerController"));
    CheckAsset(*this, TEXT("/Game/CVAD/Blueprints/Game/BP_BattleDirector.BP_BattleDirector"));
    CheckAsset(*this, TEXT("/Game/CVAD/Blueprints/Objectives/BP_MinionSpawner.BP_MinionSpawner"));
    CheckAsset(*this, TEXT("/Game/CVAD/Blueprints/Characters/BP_LanfangCharacter.BP_LanfangCharacter"));
    CheckAsset(*this, TEXT("/Game/CVAD/Blueprints/Characters/BP_TPPMinion.BP_TPPMinion"));
    CheckAsset(*this, TEXT("/Game/CVAD/Blueprints/Characters/BP_SkeletonMinion.BP_SkeletonMinion"));
    CheckAsset(*this, TEXT("/Game/CVAD/Blueprints/Characters/BP_AngelBoss.BP_AngelBoss"));
    CheckAsset(*this, TEXT("/Game/CVAD/Input/IMC_Player.IMC_Player"));
    CheckAsset(*this, TEXT("/Game/CVAD/UI/WBP_MainMenu.WBP_MainMenu"));
    CheckAsset(*this, TEXT("/Game/CVAD/UI/WBP_HUD.WBP_HUD"));
    CheckAsset(*this, TEXT("/Game/CVAD/UI/WBP_Result.WBP_Result"));
    CheckAsset(*this, TEXT("/Game/CVAD/UI/WBP_Inventory.WBP_Inventory"));
    CheckAsset(*this, TEXT("/Game/CVAD/UI/WBP_Pause.WBP_Pause"));
    CheckAsset(*this, TEXT("/Game/CVAD/UI/WBP_Settings.WBP_Settings"));
    CheckAsset(*this, TEXT("/Game/CVAD/UI/WBP_SaveSlots.WBP_SaveSlots"));
    CheckAsset(*this, TEXT("/Game/CVAD/UI/WBP_SkillTree.WBP_SkillTree"));
    CheckAsset(*this, TEXT("/Game/CVAD/Animations/ABP_LanFang_Normal.ABP_LanFang_Normal"));
    CheckAsset(*this, TEXT("/Game/CVAD/Animations/ABP_LanFang_FlyingSwordV2.ABP_LanFang_FlyingSwordV2"));
    CheckAsset(*this, TEXT("/Game/CVAD/Animations/ABP_SkeletonMinion.ABP_SkeletonMinion"));
    CheckAsset(*this, TEXT("/Game/CVAD/Maps/L_MainMenu.L_MainMenu"));
    CheckAsset(*this, TEXT("/Game/CVAD/Maps/L_BattlePrototype.L_BattlePrototype"));
    CheckAsset(*this, TEXT("/Game/CVAD/Maps/L_CastleBattle.L_CastleBattle"));

    ULevelEditorPlaySettings* PlaySettings = GetMutableDefault<ULevelEditorPlaySettings>();
    PlaySettings->SetPlayNetMode(PIE_ListenServer);
    PlaySettings->SetRunUnderOneProcess(true);
    PlaySettings->SetPlayNumberOfClients(2);

    ADD_LATENT_AUTOMATION_COMMAND(FEditorLoadMap(TEXT("/Game/CVAD/Maps/L_CastleBattle")));
    ADD_LATENT_AUTOMATION_COMMAND(FStartPIECommand(false));
    ADD_LATENT_AUTOMATION_COMMAND(FCVADPlaythroughCommand(this));
    ADD_LATENT_AUTOMATION_COMMAND(FEndPlayMapCommand());
    return true;
}

#endif

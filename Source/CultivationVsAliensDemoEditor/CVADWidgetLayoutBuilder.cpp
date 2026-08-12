#include "CVADWidgetLayoutBuilder.h"
#include "WidgetBlueprint.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/Button.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Components/ProgressBar.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "Components/ScrollBox.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/SavePackage.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimSequence.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AssetRegistry/AssetRegistryModule.h"

namespace
{
    UWidgetBlueprint* LoadWidgetBlueprint(const TCHAR* Path)
    {
        return LoadObject<UWidgetBlueprint>(nullptr, Path);
    }

    void SaveWidgetBlueprint(UWidgetBlueprint* Blueprint)
    {
        if (!Blueprint) return;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        UPackage* Package = Blueprint->GetOutermost();
        Package->MarkPackageDirty();
        const FString Filename = FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension());
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        UPackage::SavePackage(Package, Blueprint, *Filename, Args);
    }

    UTextBlock* AddText(UWidgetTree* Tree, UVerticalBox* Parent, FName Name, const FString& Text, int32 Size = 18)
    {
        UTextBlock* Widget = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        Widget->SetText(FText::FromString(Text));
        FSlateFontInfo Font = Widget->GetFont();
        Font.Size = Size;
        Widget->SetFont(Font);
        Widget->SetColorAndOpacity(FSlateColor(FLinearColor::White));
        Parent->AddChildToVerticalBox(Widget);
        return Widget;
    }

    UButton* AddButton(UWidgetTree* Tree, UVerticalBox* Parent, FName Name, const FString& Text)
    {
        UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Name);
        UTextBlock* Label = Tree->ConstructWidget<UTextBlock>();
        Label->SetText(FText::FromString(Text));
        FSlateFontInfo Font = Label->GetFont();
        Font.Size = 20;
        Label->SetFont(Font);
        Button->AddChild(Label);
        Parent->AddChildToVerticalBox(Button);
        return Button;
    }

    bool BuildHUD()
    {
        UWidgetBlueprint* Blueprint = LoadWidgetBlueprint(TEXT("/Game/CVAD/UI/WBP_HUD.WBP_HUD"));
        if (!Blueprint || !Blueprint->WidgetTree) return false;
        UWidgetTree* Tree = Blueprint->WidgetTree;
        Tree->Modify();
        Tree->RootWidget = nullptr;

        UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
        Tree->RootWidget = Root;
        UBorder* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("StatusPanel"));
        Panel->SetBrushColor(FLinearColor(0.015f, 0.025f, 0.05f, 0.78f));
        Panel->SetPadding(FMargin(16.f));
        UCanvasPanelSlot* PanelSlot = Root->AddChildToCanvas(Panel);
        PanelSlot->SetPosition(FVector2D(28.f, 28.f));
        PanelSlot->SetSize(FVector2D(380.f, 300.f));

        UVerticalBox* Stack = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("StatusStack"));
        Panel->AddChild(Stack);
        AddText(Tree, Stack, TEXT("TitleText"), TEXT("兰芳 · 宗门反攻"), 26);
        AddText(Tree, Stack, TEXT("HealthLabel"), TEXT("生命"));
        UProgressBar* Health = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("HealthBar"));
        Health->bIsVariable = true;
        Health->SetFillColorAndOpacity(FLinearColor(0.8f, 0.06f, 0.06f)); Stack->AddChildToVerticalBox(Health);
        AddText(Tree, Stack, TEXT("StaminaLabel"), TEXT("体力"));
        UProgressBar* Stamina = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("StaminaBar"));
        Stamina->bIsVariable = true;
        Stamina->SetFillColorAndOpacity(FLinearColor(0.08f, 0.75f, 0.2f)); Stack->AddChildToVerticalBox(Stamina);
        AddText(Tree, Stack, TEXT("SpiritLabel"), TEXT("灵力"));
        UProgressBar* Spirit = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("SpiritBar"));
        Spirit->bIsVariable = true;
        Spirit->SetFillColorAndOpacity(FLinearColor(0.05f, 0.4f, 1.f)); Stack->AddChildToVerticalBox(Spirit);
        AddText(Tree, Stack, TEXT("ObjectiveText"), TEXT("当前阶段：集结"))->bIsVariable = true;
        AddText(Tree, Stack, TEXT("DefeatText"), TEXT("击破：0"), 22)->bIsVariable = true;
        AddText(Tree, Stack, TEXT("LevelText"), TEXT("等级 1"), 18)->bIsVariable = true;
        AddText(Tree, Stack, TEXT("ExperienceText"), TEXT("经验 0/100"), 16)->bIsVariable = true;
        UProgressBar* Experience = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("ExperienceBar"));
        Experience->bIsVariable = true; Stack->AddChildToVerticalBox(Experience);
        AddText(Tree, Stack, TEXT("SkillPointsText"), TEXT("技能点 0"), 16)->bIsVariable = true;
        AddText(Tree, Stack, TEXT("BossNameText"), TEXT("外星机械统领"), 20)->bIsVariable = true;
        UProgressBar* BossHealth = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("BossHealthBar"));
        BossHealth->bIsVariable = true; Stack->AddChildToVerticalBox(BossHealth);
        AddText(Tree, Stack, TEXT("ResultStateText"), TEXT(""), 28)->bIsVariable = true;
        SaveWidgetBlueprint(Blueprint);
        return true;
    }

    bool BuildInventory()
    {
        UWidgetBlueprint* Blueprint = LoadWidgetBlueprint(TEXT("/Game/CVAD/UI/WBP_Inventory.WBP_Inventory"));
        if (!Blueprint || !Blueprint->WidgetTree) return false;
        UWidgetTree* Tree = Blueprint->WidgetTree;
        Tree->Modify();
        Tree->RootWidget = nullptr;

        UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
        Tree->RootWidget = Root;
        UBorder* Panel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("InventoryPanel"));
        Panel->SetBrushColor(FLinearColor(0.02f, 0.025f, 0.045f, 0.96f));
        Panel->SetPadding(FMargin(28.f));
        UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Panel);
        Slot->SetAnchors(FAnchors(0.5f));
        Slot->SetAlignment(FVector2D(0.5f));
        Slot->SetSize(FVector2D(560.f, 580.f));

        UVerticalBox* Stack = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("InventoryStack"));
        Panel->AddChild(Stack);
        AddText(Tree, Stack, TEXT("InventoryTitle"), TEXT("兰芳背包 / 换装"), 30);
        AddText(Tree, Stack, TEXT("InventoryHint"), TEXT("外观只在大厅或单人暂停时更换"), 16);
        AddButton(Tree, Stack, TEXT("Button_Head"), TEXT("头饰：竹笠 / 头盔"))->bIsVariable = true;
        AddButton(Tree, Stack, TEXT("Button_Upper"), TEXT("上装：护甲 / 轻云衣"))->bIsVariable = true;
        AddButton(Tree, Stack, TEXT("Button_Lower"), TEXT("下装：宗门 / 轻云"))->bIsVariable = true;
        AddButton(Tree, Stack, TEXT("Button_Feet"), TEXT("鞋：轻云靴 / 布鞋"))->bIsVariable = true;
        AddButton(Tree, Stack, TEXT("Button_Hands"), TEXT("护腕：装备 / 卸下"))->bIsVariable = true;
        AddButton(Tree, Stack, TEXT("Button_Close"), TEXT("关闭"))->bIsVariable = true;
        SaveWidgetBlueprint(Blueprint);
        return true;
    }
}

bool UCVADEditorAssetBuilder::BuildFlyingSwordAnimationBlueprint()
{
    UAnimBlueprint* SourceBlueprint = LoadObject<UAnimBlueprint>(nullptr,
        TEXT("/Game/LanFang/Animations/In-Place/MoveBasic/Female_AnimBP.Female_AnimBP"));
    UBlendSpace1D* SourceBlend = LoadObject<UBlendSpace1D>(nullptr,
        TEXT("/Game/LanFang/Animations/In-Place/MoveBasic/Female_2D.Female_2D"));
    UAnimSequence* Idle = LoadObject<UAnimSequence>(nullptr,
        TEXT("/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_Standing.Anim_FS_Standing"));
    UAnimSequence* Walk = LoadObject<UAnimSequence>(nullptr,
        TEXT("/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_Walk.Anim_FS_Walk"));
    UAnimSequence* Run = LoadObject<UAnimSequence>(nullptr,
        TEXT("/Game/LanFang/Animations/RootMotion/FlyingSwords/Anim_FS_Run.Anim_FS_Run"));
    if (!SourceBlueprint || !SourceBlend || !Idle || !Walk || !Run) return false;

    UPackage* BlendPackage = CreatePackage(TEXT("/Game/CVAD/Animations/BS_LanFang_FlyingSword"));
    UBlendSpace1D* FlyingBlend = FindObject<UBlendSpace1D>(BlendPackage, TEXT("BS_LanFang_FlyingSword"));
    if (!FlyingBlend)
    {
        FlyingBlend = Cast<UBlendSpace1D>(StaticDuplicateObject(SourceBlend, BlendPackage, TEXT("BS_LanFang_FlyingSword")));
        FAssetRegistryModule::AssetCreated(FlyingBlend);
    }
    const TArray<FBlendSample>& Samples = FlyingBlend->GetBlendSamples();
    if (Samples.Num() < 3) return false;
    FlyingBlend->ReplaceSampleAnimation(0, Idle);
    FlyingBlend->ReplaceSampleAnimation(1, Walk);
    FlyingBlend->ReplaceSampleAnimation(2, Run);
    FlyingBlend->ValidateSampleData();

    UPackage* BlueprintPackage = CreatePackage(TEXT("/Game/CVAD/Animations/ABP_LanFang_FlyingSword"));
    UAnimBlueprint* FlyingBlueprint = FindObject<UAnimBlueprint>(BlueprintPackage, TEXT("ABP_LanFang_FlyingSword"));
    if (!FlyingBlueprint)
    {
        FlyingBlueprint = Cast<UAnimBlueprint>(StaticDuplicateObject(SourceBlueprint, BlueprintPackage, TEXT("ABP_LanFang_FlyingSword")));
        FAssetRegistryModule::AssetCreated(FlyingBlueprint);
    }

    TArray<UEdGraph*> Graphs;
    FlyingBlueprint->GetAllGraphs(Graphs);
    int32 ReplacedPlayers = 0;
    for (UEdGraph* Graph : Graphs)
    {
        for (UEdGraphNode* GraphNode : Graph->Nodes)
        {
            if (UAnimGraphNode_BlendSpacePlayer* Player = Cast<UAnimGraphNode_BlendSpacePlayer>(GraphNode))
            {
                Player->Node.SetBlendSpace(FlyingBlend);
                ++ReplacedPlayers;
            }
        }
    }
    if (ReplacedPlayers == 0) return false;

    FlyingBlend->MarkPackageDirty();
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(FlyingBlueprint);
    FKismetEditorUtilities::CompileBlueprint(FlyingBlueprint);
    FlyingBlueprint->MarkPackageDirty();

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    UPackage::SavePackage(BlendPackage, FlyingBlend,
        *FPackageName::LongPackageNameToFilename(BlendPackage->GetName(), FPackageName::GetAssetPackageExtension()), SaveArgs);
    UPackage::SavePackage(BlueprintPackage, FlyingBlueprint,
        *FPackageName::LongPackageNameToFilename(BlueprintPackage->GetName(), FPackageName::GetAssetPackageExtension()), SaveArgs);
    UE_LOG(LogTemp, Display, TEXT("CVAD flying sword AnimBP built; replaced blend players=%d"), ReplacedPlayers);
    return true;
}

bool UCVADEditorAssetBuilder::BuildAllUIControlSkeletons()
{
    struct FPage { const TCHAR* Asset; TArray<FString> Buttons, Texts, Sliders, Checks, Combos, Inputs; };
    const TArray<FPage> Pages = {
        {TEXT("WBP_MainMenu"), {TEXT("Button_SinglePlayer"),TEXT("Button_HostListenServer"),TEXT("Button_JoinGame"),TEXT("Button_LoadGame"),TEXT("Button_Settings"),TEXT("Button_Quit")}, {TEXT("Text_Title"),TEXT("Text_Version"),TEXT("Text_Status")}},
        {TEXT("WBP_Lobby"), {TEXT("Button_Ready"),TEXT("Button_StartGame"),TEXT("Button_LeaveLobby"),TEXT("Button_CopyAddress")}, {TEXT("Text_LobbyTitle"),TEXT("Text_HostName"),TEXT("Text_Player1"),TEXT("Text_Player2"),TEXT("Text_ConnectionStatus")}, {}, {}, {}, {TEXT("Input_ServerAddress")}},
        {TEXT("WBP_Pause"), {TEXT("Button_Resume"),TEXT("Button_Inventory"),TEXT("Button_SkillTree"),TEXT("Button_Settings"),TEXT("Button_SaveGame"),TEXT("Button_LoadGame"),TEXT("Button_ReturnMainMenu")}, {TEXT("Text_PauseTitle")}},
        {TEXT("WBP_Settings"), {TEXT("Button_RebindMoveForward"),TEXT("Button_RebindMoveBack"),TEXT("Button_RebindMoveLeft"),TEXT("Button_RebindMoveRight"),TEXT("Button_RebindJump"),TEXT("Button_RebindLightAttack"),TEXT("Button_RebindHeavyAttack"),TEXT("Button_RebindDodge"),TEXT("Button_RebindFlyingSword"),TEXT("Button_RebindSwitchStance"),TEXT("Button_ResetBindings"),TEXT("Button_Apply"),TEXT("Button_Cancel")}, {TEXT("Text_SettingsTitle"),TEXT("Text_RebindPrompt")}, {TEXT("Slider_MasterVolume"),TEXT("Slider_MusicVolume"),TEXT("Slider_SFXVolume"),TEXT("Slider_MouseSensitivity"),TEXT("Slider_ResolutionScale")}, {TEXT("Check_Fullscreen"),TEXT("Check_VSync"),TEXT("Check_MouseFacing")}, {TEXT("Combo_Resolution"),TEXT("Combo_Quality"),TEXT("Combo_Language")}},
        {TEXT("WBP_Result"), {TEXT("Button_Retry"),TEXT("Button_ReturnLobby"),TEXT("Button_ReturnMainMenu")}, {TEXT("Text_ResultTitle"),TEXT("Text_ClearTime"),TEXT("Text_Defeats"),TEXT("Text_BossResult"),TEXT("Text_ExperienceEarned")}},
        {TEXT("WBP_NameEntry"), {TEXT("Button_ConfirmName"),TEXT("Button_CancelName")}, {TEXT("Text_NameTitle"),TEXT("Text_NameError")}, {}, {}, {}, {TEXT("Input_PlayerName")}},
        {TEXT("WBP_SkillTree"), {TEXT("Button_SwordAttack1"),TEXT("Button_SwordAttack2"),TEXT("Button_SwordAttack3"),TEXT("Button_SwordAttack4"),TEXT("Button_SwordAttack5"),TEXT("Button_FlyingSword1"),TEXT("Button_FlyingSword2"),TEXT("Button_FlyingSword3"),TEXT("Button_EquipSelected"),TEXT("Button_ResetSkills"),TEXT("Button_Close")}, {TEXT("Text_SkillTreeTitle"),TEXT("Text_Level"),TEXT("Text_Experience"),TEXT("Text_SkillPoints"),TEXT("Text_SelectedSkillName"),TEXT("Text_SelectedSkillDescription"),TEXT("Text_Prerequisite"),TEXT("Text_SkillCost")}},
    };
    bool bAllSucceeded = true;
    for (const FPage& Page : Pages)
    {
        const FString Path = FString::Printf(TEXT("/Game/CVAD/UI/%s.%s"), Page.Asset, Page.Asset);
        UWidgetBlueprint* BP = LoadWidgetBlueprint(*Path);
        if (!BP || !BP->WidgetTree) { bAllSucceeded = false; continue; }
        UWidgetTree* Tree = BP->WidgetTree; Tree->Modify(); Tree->RootWidget = nullptr;
        UScrollBox* Root = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ControlRoot")); Tree->RootWidget = Root;
        UVerticalBox* Controls = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Controls")); Root->AddChild(Controls);
        for (const FString& N : Page.Texts) { UTextBlock* W=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *N); W->bIsVariable=true; Controls->AddChild(W); }
        for (const FString& N : Page.Buttons) { UButton* W=Tree->ConstructWidget<UButton>(UButton::StaticClass(), *N); W->bIsVariable=true; Controls->AddChild(W); }
        for (const FString& N : Page.Sliders) { USlider* W=Tree->ConstructWidget<USlider>(USlider::StaticClass(), *N); W->bIsVariable=true; Controls->AddChild(W); }
        for (const FString& N : Page.Checks) { UCheckBox* W=Tree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), *N); W->bIsVariable=true; Controls->AddChild(W); }
        for (const FString& N : Page.Combos) { UComboBoxString* W=Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), *N); W->bIsVariable=true; Controls->AddChild(W); }
        for (const FString& N : Page.Inputs) { UEditableTextBox* W=Tree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), *N); W->bIsVariable=true; Controls->AddChild(W); }
        SaveWidgetBlueprint(BP);
    }
    return bAllSucceeded;
}

bool UCVADEditorAssetBuilder::BuildAllWidgetLayouts()
{
    return BuildHUD() && BuildInventory();
}

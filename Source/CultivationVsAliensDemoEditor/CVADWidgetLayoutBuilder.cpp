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
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/PanelWidget.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/EditableTextBox.h"
#include "Components/Slider.h"
#include "Components/ScrollBox.h"
#include "Components/Viewport.h"
#include "Components/SizeBox.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "UObject/SavePackage.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/BlendSpace1D.h"
#include "Animation/AnimSequence.h"
#include "AnimGraphNode_BlendSpacePlayer.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "Animation/Skeleton.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "AssetRegistry/AssetRegistryModule.h"

namespace
{
    bool InsertCombatSlot(UAnimBlueprint* Blueprint)
    {
        if (!Blueprint) return false;
        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        for (UEdGraph* Graph : Graphs)
        {
            TArray<UAnimGraphNode_Root*> Roots;
            Graph->GetNodesOfClass(Roots);
            if (Roots.Num() != 1) continue;

            for (UEdGraphNode* Existing : Graph->Nodes)
            {
                if (const UAnimGraphNode_LayeredBoneBlend* ExistingBlend = Cast<UAnimGraphNode_LayeredBoneBlend>(Existing))
                {
                    if (ExistingBlend->Node.LayerSetup.Num() > 0) return true;
                }
            }

            UEdGraphPin* RootInput = nullptr;
            for (UEdGraphPin* Pin : Roots[0]->Pins)
            {
                if (Pin && Pin->Direction == EGPD_Input) { RootInput = Pin; break; }
            }
            if (!RootInput || RootInput->LinkedTo.Num() != 1) continue;
            UAnimGraphNode_Slot* Slot = nullptr;
            for (UEdGraphNode* Existing : Graph->Nodes)
            {
                UAnimGraphNode_Slot* Candidate = Cast<UAnimGraphNode_Slot>(Existing);
                if (Candidate && Candidate->Node.SlotName == TEXT("UpperBody")) { Slot = Candidate; break; }
            }

            UEdGraphPin* LocomotionOutput = nullptr;
            if (Slot)
            {
                for (UEdGraphPin* Pin : Slot->Pins)
                {
                    if (Pin && Pin->Direction == EGPD_Input && Pin->LinkedTo.Num() == 1)
                    { LocomotionOutput = Pin->LinkedTo[0]; break; }
                }
            }
            else
            {
                LocomotionOutput = RootInput->LinkedTo[0];
                FGraphNodeCreator<UAnimGraphNode_Slot> Creator(*Graph);
                Slot = Creator.CreateNode();
                Slot->Node.SlotName = TEXT("UpperBody");
                Slot->Node.bAlwaysUpdateSourcePose = true;
                Slot->NodePosX = Roots[0]->NodePosX - 480;
                Slot->NodePosY = Roots[0]->NodePosY + 140;
                Creator.Finalize();
            }
            if (!LocomotionOutput) return false;

            UEdGraphPin* SlotInput = nullptr;
            UEdGraphPin* SlotOutput = nullptr;
            for (UEdGraphPin* Pin : Slot->Pins)
            {
                if (!Pin) continue;
                if (Pin->Direction == EGPD_Input && !SlotInput) SlotInput = Pin;
                if (Pin->Direction == EGPD_Output && !SlotOutput) SlotOutput = Pin;
            }
            if (!SlotInput || !SlotOutput) return false;
            const UEdGraphSchema* Schema = Graph->GetSchema();
            if (SlotInput->LinkedTo.Num() == 0 && !Schema->TryCreateConnection(LocomotionOutput, SlotInput)) return false;

            FName UpperBodyBone = NAME_None;
            if (Blueprint->TargetSkeleton)
            {
                const FReferenceSkeleton& RefSkeleton = Blueprint->TargetSkeleton->GetReferenceSkeleton();
                for (const FName Candidate : {FName(TEXT("spine_01")), FName(TEXT("Spine1")),
                    FName(TEXT("spine_1")), FName(TEXT("spine")), FName(TEXT("Spine"))})
                {
                    if (RefSkeleton.FindBoneIndex(Candidate) != INDEX_NONE) { UpperBodyBone = Candidate; break; }
                }
            }
            if (UpperBodyBone.IsNone())
            {
                UE_LOG(LogTemp, Error, TEXT("CVAD could not find an upper-body spine bone for %s"), *GetNameSafe(Blueprint));
                return false;
            }

            FGraphNodeCreator<UAnimGraphNode_LayeredBoneBlend> BlendCreator(*Graph);
            UAnimGraphNode_LayeredBoneBlend* LayeredBlend = BlendCreator.CreateNode();
            LayeredBlend->Node.AddPose();
            LayeredBlend->Node.LayerSetup[0].BranchFilters.Add({UpperBodyBone, 0});
            LayeredBlend->Node.bMeshSpaceRotationBlend = true;
            LayeredBlend->Node.bBlendRootMotionBasedOnRootBone = true;
            LayeredBlend->NodePosX = Roots[0]->NodePosX - 240;
            LayeredBlend->NodePosY = Roots[0]->NodePosY;
            BlendCreator.Finalize();

            UEdGraphPin* BasePosePin = LayeredBlend->FindPin(TEXT("BasePose"));
            UEdGraphPin* BlendPosePin = nullptr;
            UEdGraphPin* BlendOutput = nullptr;
            for (UEdGraphPin* Pin : LayeredBlend->Pins)
            {
                if (!Pin) continue;
                if (Pin->Direction == EGPD_Output && !BlendOutput) BlendOutput = Pin;
                if (Pin->Direction == EGPD_Input && Pin != BasePosePin && Pin->PinName.ToString().Contains(TEXT("BlendPoses")))
                    BlendPosePin = Pin;
            }
            if (!BasePosePin || !BlendPosePin || !BlendOutput) return false;
            RootInput->BreakAllPinLinks();
            if (!Schema->TryCreateConnection(LocomotionOutput, BasePosePin) ||
                !Schema->TryCreateConnection(SlotOutput, BlendPosePin) ||
                !Schema->TryCreateConnection(BlendOutput, RootInput)) return false;
            UE_LOG(LogTemp, Display, TEXT("CVAD inserted UpperBody layered blend at bone %s in %s"),
                *UpperBodyBone.ToString(), *GetNameSafe(Blueprint));
            return true;
        }
        return false;
    }

    bool SaveAnimBlueprint(UAnimBlueprint* Blueprint)
    {
        if (!Blueprint || !InsertCombatSlot(Blueprint)) return false;
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        Blueprint->MarkPackageDirty();
        UPackage* Package = Blueprint->GetOutermost();
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        return UPackage::SavePackage(Package, Blueprint,
            *FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()), Args);
    }

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
        AddText(Tree, Stack, TEXT("BossNameText"), TEXT("天穹三使 · 剑士 / 翼卫 / 天术师"), 20)->bIsVariable = true;
        UProgressBar* BossHealth = Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(), TEXT("BossHealthBar"));
        BossHealth->bIsVariable = true; Stack->AddChildToVerticalBox(BossHealth);
        AddText(Tree, Stack, TEXT("ResultStateText"), TEXT(""), 28)->bIsVariable = true;
        SaveWidgetBlueprint(Blueprint);
        return true;
    }

    bool BuildMainHUDV2()
    {
        UWidgetBlueprint* Blueprint=LoadWidgetBlueprint(TEXT("/Game/CVAD/UI/WBP_HUD.WBP_HUD"));
        if(!Blueprint||!Blueprint->WidgetTree) return false;
        UWidgetTree* Tree=Blueprint->WidgetTree; Tree->Modify(); Tree->RootWidget=nullptr;
        UCanvasPanel* Root=Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("RootCanvas")); Tree->RootWidget=Root;
        auto Panel=[&](FName Name,FAnchors Anchors,FVector2D Alignment,FVector2D Position,FVector2D Size)
        {
            UBorder* B=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),Name); B->SetBrushColor(FLinearColor(0.008f,0.015f,0.035f,0.82f)); B->SetPadding(FMargin(14.f));
            UCanvasPanelSlot* S=Root->AddChildToCanvas(B); S->SetAnchors(Anchors); S->SetAlignment(Alignment); S->SetPosition(Position); S->SetSize(Size); return B;
        };
        auto Text=[&](UPanelWidget* Parent,FName Name,const FString& Value,int32 Size=16)
        {
            UTextBlock* T=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name); T->bIsVariable=true; T->SetText(FText::FromString(Value)); FSlateFontInfo F=T->GetFont();F.Size=Size;T->SetFont(F);T->SetColorAndOpacity(FSlateColor(FLinearColor::White)); Parent->AddChild(T); return T;
        };
        auto Bar=[&](UVerticalBox* Parent,FName Name,FLinearColor Color)
        {
            UProgressBar* P=Tree->ConstructWidget<UProgressBar>(UProgressBar::StaticClass(),Name);P->bIsVariable=true;P->SetFillColorAndOpacity(Color);Parent->AddChildToVerticalBox(P);return P;
        };

        UBorder* PlayerPanel=Panel(TEXT("PlayerStatusPanel"),FAnchors(0.f,1.f),FVector2D(0.f,1.f),FVector2D(28.f,-28.f),FVector2D(430.f,255.f));
        UVerticalBox* Player=Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("PlayerStatusStack"));PlayerPanel->AddChild(Player);
        Text(Player,TEXT("PlayerNameText"),TEXT("兰芳"),24); Text(Player,TEXT("LevelText"),TEXT("等级 1"),16);
        Text(Player,TEXT("HealthLabel"),TEXT("生命"),14); Bar(Player,TEXT("HealthBar"),FLinearColor(0.85f,0.06f,0.08f)); Text(Player,TEXT("HealthValueText"),TEXT("100 / 100"),13);
        Text(Player,TEXT("StaminaLabel"),TEXT("体力"),14); Bar(Player,TEXT("StaminaBar"),FLinearColor(0.1f,0.8f,0.25f)); Text(Player,TEXT("StaminaValueText"),TEXT("100 / 100"),13);
        Text(Player,TEXT("SpiritLabel"),TEXT("灵力"),14); Bar(Player,TEXT("SpiritBar"),FLinearColor(0.05f,0.4f,1.f)); Text(Player,TEXT("SpiritValueText"),TEXT("100 / 100"),13);
        Text(Player,TEXT("ExperienceText"),TEXT("经验 0/100"),13);Bar(Player,TEXT("ExperienceBar"),FLinearColor(0.8f,0.55f,0.1f));Text(Player,TEXT("SkillPointsText"),TEXT("技能点 0"),13);

        UBorder* ObjectivePanel=Panel(TEXT("ObjectivePanel"),FAnchors(0.5f,0.f),FVector2D(0.5f,0.f),FVector2D(0.f,24.f),FVector2D(700.f,125.f));
        UVerticalBox* Objective=Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("ObjectiveStack"));ObjectivePanel->AddChild(Objective);
        Text(Objective,TEXT("ObjectiveText"),TEXT("肃清骷髅军团"),23);Text(Objective,TEXT("DefeatText"),TEXT("击破 0/15"),17);Text(Objective,TEXT("BossNameText"),TEXT("天穹三使"),18);Bar(Objective,TEXT("BossHealthBar"),FLinearColor(0.7f,0.08f,0.85f));

        UBorder* SkillPanel=Panel(TEXT("SkillBarPanel"),FAnchors(0.5f,1.f),FVector2D(0.5f,1.f),FVector2D(0.f,-28.f),FVector2D(720.f,105.f));
        UVerticalBox* SkillOuter=Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("SkillOuter"));SkillPanel->AddChild(SkillOuter);Text(SkillOuter,TEXT("StanceText"),TEXT("持剑模式 · 消耗体力"),18);
        UHorizontalBox* Skills=Tree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),TEXT("SkillSlots"));SkillOuter->AddChildToVerticalBox(Skills);
        Text(Skills,TEXT("SkillSlot1Text"),TEXT("1  技能"),15);Text(Skills,TEXT("SkillSlot2Text"),TEXT("2  普攻"),15);Text(Skills,TEXT("SkillSlot3Text"),TEXT("3  普攻"),15);Text(Skills,TEXT("SkillSlot4Text"),TEXT("4  普攻"),15);Text(Skills,TEXT("SkillSlot5Text"),TEXT("5  技能"),15);

        UBorder* HintPanel=Panel(TEXT("ShortcutPanel"),FAnchors(1.f,0.5f),FVector2D(1.f,0.5f),FVector2D(-28.f,0.f),FVector2D(270.f,210.f));
        UVerticalBox* Hints=Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("ShortcutStack"));HintPanel->AddChild(Hints);
        Text(Hints,TEXT("ShortcutTitle"),TEXT("快捷菜单"),20);Text(Hints,TEXT("ShortcutSkill"),TEXT("Tab / B  技能树"));Text(Hints,TEXT("ShortcutSettings"),TEXT("F1  设置"));Text(Hints,TEXT("ShortcutSave"),TEXT("F5  存档"));Text(Hints,TEXT("ShortcutPause"),TEXT("P  暂停"));Text(Hints,TEXT("ShortcutRevive"),TEXT("E  救援"));
        Text(Root,TEXT("DownedHintText"),TEXT(""),26);Text(Root,TEXT("ResultStateText"),TEXT(""),34);
        SaveWidgetBlueprint(Blueprint); return true;
    }

    bool BuildMainMenuV2()
    {
        UWidgetBlueprint* Blueprint=LoadWidgetBlueprint(TEXT("/Game/CVAD/UI/WBP_MainMenu.WBP_MainMenu"));
        if(!Blueprint||!Blueprint->WidgetTree) return false;
        UWidgetTree* Tree=Blueprint->WidgetTree; Tree->Modify(); Tree->RootWidget=nullptr;
        UCanvasPanel* Root=Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("RootCanvas")); Tree->RootWidget=Root;

        UBorder* Shade=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("BackgroundShade"));
        Shade->SetBrushColor(FLinearColor(0.003f,0.008f,0.02f,0.72f));
        UCanvasPanelSlot* ShadeSlot=Root->AddChildToCanvas(Shade); ShadeSlot->SetAnchors(FAnchors(0.f,0.f,1.f,1.f)); ShadeSlot->SetOffsets(FMargin(0.f));

        auto MakeText=[&](UPanelWidget* Parent,FName Name,const FString& Value,int32 Size,FLinearColor Color=FLinearColor::White)
        {
            UTextBlock* T=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name); T->bIsVariable=true; T->SetText(FText::FromString(Value));
            FSlateFontInfo Font=T->GetFont(); Font.Size=Size; T->SetFont(Font); T->SetColorAndOpacity(FSlateColor(Color)); Parent->AddChild(T); return T;
        };
        auto MakeButton=[&](UVerticalBox* Parent,FName Name,const FString& Label,FLinearColor Color=FLinearColor(0.08f,0.15f,0.28f,0.96f))
        {
            UButton* B=Tree->ConstructWidget<UButton>(UButton::StaticClass(),Name); B->bIsVariable=true; B->SetBackgroundColor(Color);
            UTextBlock* T=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),*(FString(TEXT("Label_"))+Name.ToString())); T->SetText(FText::FromString(Label));
            FSlateFontInfo Font=T->GetFont(); Font.Size=21; T->SetFont(Font); T->SetJustification(ETextJustify::Center); T->SetColorAndOpacity(FSlateColor(FLinearColor::White));
            B->AddChild(T); Parent->AddChildToVerticalBox(B); return B;
        };

        UVerticalBox* Brand=Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("BrandStack"));
        UCanvasPanelSlot* BrandSlot=Root->AddChildToCanvas(Brand); BrandSlot->SetAnchors(FAnchors(0.07f,0.27f)); BrandSlot->SetSize(FVector2D(760.f,330.f));
        MakeText(Brand,TEXT("Text_Title"),TEXT("修仙大战外星人"),54,FLinearColor(0.85f,0.94f,1.f));
        MakeText(Brand,TEXT("Text_Subtitle"),TEXT("CULTIVATION  VS  ALIENS"),24,FLinearColor(0.3f,0.72f,1.f));
        MakeText(Brand,TEXT("Text_Chapter"),TEXT("兰芳 · 天穹三使讨伐战"),26,FLinearColor(0.95f,0.78f,0.34f));
        MakeText(Brand,TEXT("Text_ModeInfo"),TEXT("单人无双战斗 / 双人 Listen Server 联机"),17,FLinearColor(0.72f,0.76f,0.84f));

        UBorder* MenuPanel=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("MenuPanel")); MenuPanel->SetBrushColor(FLinearColor(0.008f,0.018f,0.045f,0.94f)); MenuPanel->SetPadding(FMargin(28.f));
        UCanvasPanelSlot* MenuSlot=Root->AddChildToCanvas(MenuPanel); MenuSlot->SetAnchors(FAnchors(0.78f,0.5f)); MenuSlot->SetAlignment(FVector2D(0.5f)); MenuSlot->SetSize(FVector2D(460.f,690.f));
        UVerticalBox* Menu=Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(),TEXT("MenuStack")); MenuPanel->AddChild(Menu);
        MakeText(Menu,TEXT("Text_MenuHeader"),TEXT("主菜单"),30,FLinearColor(0.85f,0.94f,1.f));
        MakeText(Menu,TEXT("Text_PlayerName"),TEXT("玩家：兰芳"),18,FLinearColor(0.95f,0.78f,0.34f));
        MakeButton(Menu,TEXT("Button_SinglePlayer"),TEXT("开始游戏"),FLinearColor(0.1f,0.38f,0.58f,1.f));
        MakeButton(Menu,TEXT("Button_LoadGame"),TEXT("继续游戏 / 存档"));
        MakeButton(Menu,TEXT("Button_Skills"),TEXT("功法 / 技能装配"),FLinearColor(0.19f,0.16f,0.38f,1.f));
        MakeButton(Menu,TEXT("Button_Outfit"),TEXT("选择装扮"),FLinearColor(0.18f,0.26f,0.34f,1.f));
        MakeButton(Menu,TEXT("Button_Multiplayer"),TEXT("多人联机"),FLinearColor(0.1f,0.32f,0.48f,1.f));
        MakeButton(Menu,TEXT("Button_ChangeName"),TEXT("修改玩家名称"));
        MakeButton(Menu,TEXT("Button_Settings"),TEXT("设置 / 自定义按键"));
        MakeButton(Menu,TEXT("Button_Quit"),TEXT("退出游戏"),FLinearColor(0.24f,0.07f,0.09f,0.96f));
        MakeText(Menu,TEXT("Text_Status"),TEXT("准备就绪"),15,FLinearColor(0.55f,0.78f,0.9f));

        UTextBlock* Version=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("Text_Version")); Version->bIsVariable=true; Version->SetText(FText::FromString(TEXT("DEMO 0.1  |  GAS · 双人联机"))); Version->SetColorAndOpacity(FSlateColor(FLinearColor(0.55f,0.6f,0.7f)));
        UCanvasPanelSlot* VersionSlot=Root->AddChildToCanvas(Version); VersionSlot->SetAnchors(FAnchors(0.f,1.f)); VersionSlot->SetAlignment(FVector2D(0.f,1.f)); VersionSlot->SetPosition(FVector2D(30.f,-20.f)); VersionSlot->SetSize(FVector2D(500.f,32.f));
        SaveWidgetBlueprint(Blueprint); return true;
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

    UAnimBlueprint* NormalBlueprint = LoadObject<UAnimBlueprint>(nullptr,
        TEXT("/Game/CVAD/Animations/ABP_LanFang_Normal.ABP_LanFang_Normal"));
    if (!NormalBlueprint)
    {
        UPackage* NormalPackage = CreatePackage(TEXT("/Game/CVAD/Animations/ABP_LanFang_Normal"));
        NormalBlueprint = Cast<UAnimBlueprint>(StaticDuplicateObject(SourceBlueprint, NormalPackage, TEXT("ABP_LanFang_Normal")));
        FAssetRegistryModule::AssetCreated(NormalBlueprint);
    }
    if (!SaveAnimBlueprint(NormalBlueprint)) return false;

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

    UAnimBlueprint* FlyingBlueprint = LoadObject<UAnimBlueprint>(nullptr,
        TEXT("/Game/CVAD/Animations/ABP_LanFang_FlyingSword.ABP_LanFang_FlyingSword"));
    if (!FlyingBlueprint)
    {
        UPackage* BlueprintPackage = CreatePackage(TEXT("/Game/CVAD/Animations/ABP_LanFang_FlyingSword"));
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
    if (!SaveAnimBlueprint(FlyingBlueprint)) return false;

    FSavePackageArgs SaveArgs;
    SaveArgs.TopLevelFlags = RF_Public | RF_Standalone;
    UPackage::SavePackage(BlendPackage, FlyingBlend,
        *FPackageName::LongPackageNameToFilename(BlendPackage->GetName(), FPackageName::GetAssetPackageExtension()), SaveArgs);
    UE_LOG(LogTemp, Display, TEXT("CVAD combat AnimBPs built with UpperBody slot; flying blend players=%d"), ReplacedPlayers);
    return true;
}

bool UCVADEditorAssetBuilder::BuildAllUIControlSkeletons()
{
    struct FPage { const TCHAR* Asset; TArray<FString> Buttons, Texts, Sliders, Checks, Combos, Inputs; };
    const TArray<FPage> Pages = {
        {TEXT("WBP_Lobby"), {TEXT("Button_Ready"),TEXT("Button_StartGame"),TEXT("Button_LeaveLobby"),TEXT("Button_CopyAddress")}, {TEXT("Text_LobbyTitle"),TEXT("Text_HostName"),TEXT("Text_Player1"),TEXT("Text_Player2"),TEXT("Text_ConnectionStatus")}, {}, {}, {}, {TEXT("Input_ServerAddress")}},
        {TEXT("WBP_Multiplayer"), {TEXT("Button_HostListenServer"),TEXT("Button_JoinGame"),TEXT("Button_Close")}, {TEXT("Text_MultiplayerTitle"),TEXT("Text_RoomHelp"),TEXT("Text_Status")}, {}, {}, {}, {TEXT("Input_ServerAddress")}},
        {TEXT("WBP_Pause"), {TEXT("Button_Resume"),TEXT("Button_Settings"),TEXT("Button_SaveGame"),TEXT("Button_LoadGame"),TEXT("Button_ReturnMainMenu")}, {TEXT("Text_PauseTitle")}},
        {TEXT("WBP_Settings"), {TEXT("Button_RebindMoveForward"),TEXT("Button_RebindMoveBack"),TEXT("Button_RebindMoveLeft"),TEXT("Button_RebindMoveRight"),TEXT("Button_RebindJump"),TEXT("Button_RebindLightAttack"),TEXT("Button_RebindHeavyAttack"),TEXT("Button_RebindDodge"),TEXT("Button_RebindFlyingSword"),TEXT("Button_RebindSwitchStance"),TEXT("Button_ResetBindings"),TEXT("Button_Apply"),TEXT("Button_Cancel")}, {TEXT("Text_SettingsTitle"),TEXT("Text_RebindPrompt"),TEXT("Text_NameError")}, {TEXT("Slider_MasterVolume"),TEXT("Slider_MouseSensitivity"),TEXT("Slider_ResolutionScale")}, {TEXT("Check_Fullscreen"),TEXT("Check_VSync"),TEXT("Check_MouseFacing")}, {TEXT("Combo_Quality")}, {TEXT("Input_PlayerName")}},
        {TEXT("WBP_Result"), {TEXT("Button_Retry"),TEXT("Button_ReturnLobby"),TEXT("Button_ReturnMainMenu")}, {TEXT("Text_ResultTitle"),TEXT("Text_ClearTime"),TEXT("Text_Defeats"),TEXT("Text_BossResult"),TEXT("Text_ExperienceEarned")}},
        {TEXT("WBP_NameEntry"), {TEXT("Button_ConfirmName"),TEXT("Button_CancelName")}, {TEXT("Text_NameTitle"),TEXT("Text_NameError")}, {}, {}, {}, {TEXT("Input_PlayerName")}},
        {TEXT("WBP_OutfitSelect"), {TEXT("Button_HeadPrev"),TEXT("Button_HeadNext"),TEXT("Button_HairPrev"),TEXT("Button_HairNext"),TEXT("Button_HatPrev"),TEXT("Button_HatNext"),TEXT("Button_UpperPrev"),TEXT("Button_UpperNext"),TEXT("Button_HandsPrev"),TEXT("Button_HandsNext"),TEXT("Button_LowerPrev"),TEXT("Button_LowerNext"),TEXT("Button_FeetPrev"),TEXT("Button_FeetNext"),TEXT("Button_OutfitConfirm"),TEXT("Button_Close")}, {TEXT("Text_OutfitTitle"),TEXT("Text_HeadValue"),TEXT("Text_HairValue"),TEXT("Text_HatValue"),TEXT("Text_UpperValue"),TEXT("Text_HandsValue"),TEXT("Text_LowerValue"),TEXT("Text_FeetValue"),TEXT("Text_OutfitStatus")}, {}, {}, {}, {TEXT("Input_PlayerName")}},
        {TEXT("WBP_SaveSlots"), {TEXT("Button_SaveSlot0"),TEXT("Button_SaveSlot1"),TEXT("Button_SaveSlot2"),TEXT("Button_SaveSelected"),TEXT("Button_LoadSelected"),TEXT("Button_DeleteSelected"),TEXT("Button_Close")}, {TEXT("Text_SaveTitle"),TEXT("Text_SelectedSlot"),TEXT("Text_Slot0"),TEXT("Text_Slot1"),TEXT("Text_Slot2")}},
        {TEXT("WBP_SkillTree"), {TEXT("Button_SwordAttack1"),TEXT("Button_SwordAttack2"),TEXT("Button_SwordAttack3"),TEXT("Button_SwordAttack4"),TEXT("Button_SwordAttack5"),TEXT("Button_FlyingSword1"),TEXT("Button_FlyingSword2"),TEXT("Button_FlyingSword3"),TEXT("Button_EquipSelected"),TEXT("Button_Close")}, {TEXT("Text_SkillTreeTitle"),TEXT("Text_Level"),TEXT("Text_Experience"),TEXT("Text_SkillPoints"),TEXT("Text_EquippedSkills"),TEXT("Text_AvailableSkills"),TEXT("Text_SelectedSkillName"),TEXT("Text_SelectedSkillDescription"),TEXT("Text_Prerequisite"),TEXT("Text_SkillCost")}},
    };
    bool bAllSucceeded = true;
    const TMap<FString,FString> ButtonLabels={
        {TEXT("Button_SinglePlayer"),TEXT("开始游戏")},{TEXT("Button_HostListenServer"),TEXT("创建房间")},{TEXT("Button_JoinGame"),TEXT("加入房间")},{TEXT("Button_LoadGame"),TEXT("读取存档")},{TEXT("Button_Settings"),TEXT("游戏设置")},{TEXT("Button_Quit"),TEXT("退出游戏")},
        {TEXT("Button_Ready"),TEXT("准备 / 取消准备")},{TEXT("Button_StartGame"),TEXT("开始战斗")},{TEXT("Button_LeaveLobby"),TEXT("离开房间")},{TEXT("Button_CopyAddress"),TEXT("复制主机地址")},
        {TEXT("Button_Resume"),TEXT("继续游戏")},{TEXT("Button_Inventory"),TEXT("技能装配")},{TEXT("Button_SkillTree"),TEXT("技能树")},{TEXT("Button_SaveGame"),TEXT("保存游戏")},{TEXT("Button_ReturnMainMenu"),TEXT("返回主菜单")},
        {TEXT("Button_RebindMoveForward"),TEXT("前进按键")},{TEXT("Button_RebindMoveBack"),TEXT("后退按键")},{TEXT("Button_RebindMoveLeft"),TEXT("左移按键")},{TEXT("Button_RebindMoveRight"),TEXT("右移按键")},{TEXT("Button_RebindJump"),TEXT("跳跃按键")},
        {TEXT("Button_RebindLightAttack"),TEXT("普通攻击按键")},{TEXT("Button_RebindHeavyAttack"),TEXT("重攻击按键")},{TEXT("Button_RebindDodge"),TEXT("闪避按键")},{TEXT("Button_RebindFlyingSword"),TEXT("飞剑技能按键")},{TEXT("Button_RebindSwitchStance"),TEXT("切换模式按键")},{TEXT("Button_ResetBindings"),TEXT("恢复默认按键")},{TEXT("Button_Apply"),TEXT("应用设置")},{TEXT("Button_Cancel"),TEXT("取消")},
        {TEXT("Button_Multiplayer"),TEXT("多人联机")},{TEXT("Button_SaveSelected"),TEXT("保存到所选存档")},{TEXT("Button_LoadSelected"),TEXT("读取所选存档")},{TEXT("Button_DeleteSelected"),TEXT("删除所选存档")},
        {TEXT("Button_Retry"),TEXT("重新挑战")},{TEXT("Button_ReturnLobby"),TEXT("返回大厅")},
        {TEXT("Button_ConfirmName"),TEXT("确认名称")},{TEXT("Button_CancelName"),TEXT("取消")},
        {TEXT("Button_OutfitPrevious"),TEXT("← 上一套")},{TEXT("Button_OutfitNext"),TEXT("下一套 →")},{TEXT("Button_OutfitConfirm"),TEXT("确认使用")},
        {TEXT("Button_HeadPrev"),TEXT("← 脸型")},{TEXT("Button_HeadNext"),TEXT("脸型 →")},{TEXT("Button_HairPrev"),TEXT("← 发型")},{TEXT("Button_HairNext"),TEXT("发型 →")},
        {TEXT("Button_HatPrev"),TEXT("← 帽子")},{TEXT("Button_HatNext"),TEXT("帽子 →")},{TEXT("Button_UpperPrev"),TEXT("← 上装")},{TEXT("Button_UpperNext"),TEXT("上装 →")},
        {TEXT("Button_HandsPrev"),TEXT("← 手部")},{TEXT("Button_HandsNext"),TEXT("手部 →")},{TEXT("Button_LowerPrev"),TEXT("← 下装")},{TEXT("Button_LowerNext"),TEXT("下装 →")},
        {TEXT("Button_FeetPrev"),TEXT("← 鞋子")},{TEXT("Button_FeetNext"),TEXT("鞋子 →")},
        {TEXT("Button_SaveSlot0"),TEXT("选择存档 1")},{TEXT("Button_SaveSlot1"),TEXT("选择存档 2")},{TEXT("Button_SaveSlot2"),TEXT("选择存档 3")},{TEXT("Button_Close"),TEXT("返回")},
        {TEXT("Button_SwordAttack1"),TEXT("持剑技能 1")},{TEXT("Button_SwordAttack2"),TEXT("持剑攻击 2")},{TEXT("Button_SwordAttack3"),TEXT("持剑攻击 3")},{TEXT("Button_SwordAttack4"),TEXT("持剑攻击 4")},{TEXT("Button_SwordAttack5"),TEXT("持剑技能 5")},
        {TEXT("Button_FlyingSword1"),TEXT("御剑技能 1")},{TEXT("Button_FlyingSword2"),TEXT("御剑攻击 2")},{TEXT("Button_FlyingSword3"),TEXT("御剑攻击 3")},{TEXT("Button_EquipSelected"),TEXT("装配所选技能")},{TEXT("Button_ResetSkills"),TEXT("重置技能")}
    };
    const TMap<FString,FString> TextLabels={
        {TEXT("Text_MultiplayerTitle"),TEXT("多人联机")},{TEXT("Text_RoomHelp"),TEXT("创建房间后把房间号发给好友；加入时输入房主的房间号或地址。")},{TEXT("Text_Status"),TEXT("等待操作")},
        {TEXT("Text_SaveTitle"),TEXT("三个存档槽")},{TEXT("Text_SelectedSlot"),TEXT("当前选择：存档 1")},
        {TEXT("Text_SkillTreeTitle"),TEXT("功法与技能装配")},{TEXT("Text_EquippedSkills"),TEXT("当前已装配")},{TEXT("Text_AvailableSkills"),TEXT("可装配功法")},
        {TEXT("Text_OutfitTitle"),TEXT("角色装扮预览")},{TEXT("Text_OutfitStatus"),TEXT("选择部件后可在预览中查看")}
    };
    for (const FPage& Page : Pages)
    {
        const FString Path = FString::Printf(TEXT("/Game/CVAD/UI/%s.%s"), Page.Asset, Page.Asset);
        UWidgetBlueprint* BP = LoadWidgetBlueprint(*Path);
        if (!BP || !BP->WidgetTree) { bAllSucceeded = false; continue; }
        UWidgetTree* Tree = BP->WidgetTree; Tree->Modify(); Tree->RootWidget = nullptr;
        UScrollBox* Root = Tree->ConstructWidget<UScrollBox>(UScrollBox::StaticClass(), TEXT("ControlRoot")); Tree->RootWidget = Root;
        UVerticalBox* Controls = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("Controls")); Root->AddChild(Controls);
        for (const FString& N : Page.Texts) { UTextBlock* W=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), *N); W->bIsVariable=true; W->SetText(FText::FromString(TextLabels.FindRef(N))); Controls->AddChild(W); }
        for (const FString& N : Page.Buttons)
        {
            UButton* W=Tree->ConstructWidget<UButton>(UButton::StaticClass(),*N);W->bIsVariable=true;
            UTextBlock* Label=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),*(TEXT("Label_")+N));
            Label->SetText(FText::FromString(ButtonLabels.FindRef(N).IsEmpty()?N:ButtonLabels.FindRef(N)));
            FSlateFontInfo Font=Label->GetFont();Font.Size=18;Label->SetFont(Font);Label->SetJustification(ETextJustify::Center);
            W->AddChild(Label);Controls->AddChild(W);
        }
        for (const FString& N : Page.Sliders) { USlider* W=Tree->ConstructWidget<USlider>(USlider::StaticClass(), *N); W->bIsVariable=true; Controls->AddChild(W); }
        for (const FString& N : Page.Checks) { UCheckBox* W=Tree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(), *N); W->bIsVariable=true; Controls->AddChild(W); }
        for (const FString& N : Page.Combos) { UComboBoxString* W=Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(), *N); W->bIsVariable=true; Controls->AddChild(W); }
        for (const FString& N : Page.Inputs) { UEditableTextBox* W=Tree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(), *N); W->bIsVariable=true; Controls->AddChild(W); }
        if(Page.Asset==FString(TEXT("WBP_OutfitSelect")))
        {
            USizeBox* PreviewSize=Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(),TEXT("OutfitPreviewSize"));
            PreviewSize->SetWidthOverride(440.f); PreviewSize->SetHeightOverride(520.f); Controls->InsertChildAt(1,PreviewSize);
            UViewport* Preview=Tree->ConstructWidget<UViewport>(UViewport::StaticClass(),TEXT("Viewport_OutfitPreview"));
            Preview->bIsVariable=true; PreviewSize->AddChild(Preview);
        }
        SaveWidgetBlueprint(BP);
    }
    return bAllSucceeded;
}

bool UCVADEditorAssetBuilder::BuildAllWidgetLayouts()
{
    return BuildMainHUDV2() && BuildMainMenuV2();
}

bool UCVADEditorAssetBuilder::UpdateUIBackButtons()
{
    struct FBackButtonPage
    {
        const TCHAR* AssetPath;
        const TCHAR* ButtonName;
    };

    const FBackButtonPage Pages[] = {
        {TEXT("/Game/CVAD/UI/WBP_Settings.WBP_Settings"), TEXT("Button_Cancel")},
        {TEXT("/Game/CVAD/UI/WBP_NameEntry.WBP_NameEntry"), TEXT("Button_CancelName")},
        {TEXT("/Game/CVAD/UI/WBP_OutfitSelect.WBP_OutfitSelect"), TEXT("Button_Close")},
        {TEXT("/Game/CVAD/UI/WBP_SaveSlots.WBP_SaveSlots"), TEXT("Button_Close")},
        {TEXT("/Game/CVAD/UI/WBP_SkillTree.WBP_SkillTree"), TEXT("Button_Close")},
        {TEXT("/Game/CVAD/UI/WBP_Inventory.WBP_Inventory"), TEXT("Button_Close")},
        {TEXT("/Game/CVAD/UI/WBP_Pause.WBP_Pause"), TEXT("Button_Resume")}
    };

    bool bAllSucceeded = true;
    for (const FBackButtonPage& Page : Pages)
    {
        UWidgetBlueprint* Blueprint = LoadObject<UWidgetBlueprint>(nullptr, Page.AssetPath);
        UButton* Button = Blueprint && Blueprint->WidgetTree
            ? Cast<UButton>(Blueprint->WidgetTree->FindWidget(FName(Page.ButtonName))) : nullptr;
        UTextBlock* Label = Button && Button->GetChildrenCount() > 0
            ? Cast<UTextBlock>(Button->GetChildAt(0)) : nullptr;
        if (!Blueprint || !Button || !Label)
        {
            UE_LOG(LogTemp, Warning, TEXT("Back button update failed Asset=%s Button=%s"), Page.AssetPath, Page.ButtonName);
            bAllSucceeded = false;
            continue;
        }
        Blueprint->Modify();
        Label->Modify();
        Label->SetText(FText::FromString(TEXT("返回")));
        FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
        SaveWidgetBlueprint(Blueprint);
    }
    return bAllSucceeded;
}

bool UCVADEditorAssetBuilder::UpdateSettingsKeyLabels()
{
    UWidgetBlueprint* Blueprint=LoadObject<UWidgetBlueprint>(nullptr, TEXT("/Game/CVAD/UI/WBP_Settings.WBP_Settings"));
    if (!Blueprint || !Blueprint->WidgetTree) return false;

    struct FSettingsKeyRow { const TCHAR* ButtonName; const TCHAR* LabelName; };
    static const FSettingsKeyRow Rows[] = {
        {TEXT("Button_RebindMoveForward"), TEXT("Text_Key_MoveForward")},
        {TEXT("Button_RebindMoveBack"), TEXT("Text_Key_MoveBack")},
        {TEXT("Button_RebindMoveLeft"), TEXT("Text_Key_MoveLeft")},
        {TEXT("Button_RebindMoveRight"), TEXT("Text_Key_MoveRight")},
        {TEXT("Button_RebindJump"), TEXT("Text_Key_Jump")},
        {TEXT("Button_RebindLightAttack"), TEXT("Text_Key_LightAttack")},
        {TEXT("Button_RebindHeavyAttack"), TEXT("Text_Key_HeavyAttack")},
        {TEXT("Button_RebindDodge"), TEXT("Text_Key_Dodge")},
        {TEXT("Button_RebindFlyingSword"), TEXT("Text_Key_FlyingSword")},
        {TEXT("Button_RebindSwitchStance"), TEXT("Text_Key_SwitchStance")}
    };

    Blueprint->Modify();
    bool bAllSucceeded=true;
    for (const FSettingsKeyRow& Entry : Rows)
    {
        if (Blueprint->WidgetTree->FindWidget(FName(Entry.LabelName))) continue;
        UButton* Button=Cast<UButton>(Blueprint->WidgetTree->FindWidget(FName(Entry.ButtonName)));
        UPanelWidget* Parent=Button ? Cast<UPanelWidget>(Button->GetParent()) : nullptr;
        if (!Button || !Parent)
        {
            UE_LOG(LogTemp, Warning, TEXT("Could not add key label beside %s"), Entry.ButtonName);
            bAllSucceeded=false;
            continue;
        }

        UHorizontalBox* Row=Cast<UHorizontalBox>(Parent);
        if (!Row)
        {
            const int32 ChildIndex=Parent->GetChildIndex(Button);
            Parent->RemoveChild(Button);
            Row=Blueprint->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
                FName(*FString::Printf(TEXT("Row_%s"), Entry.ButtonName)));
            Parent->InsertChildAt(ChildIndex, Row);
            UHorizontalBoxSlot* ButtonSlot=Row->AddChildToHorizontalBox(Button);
            ButtonSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
            ButtonSlot->SetPadding(FMargin(0.f, 2.f, 12.f, 2.f));
        }

        UTextBlock* Label=Blueprint->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), FName(Entry.LabelName));
        Label->bIsVariable=true;
        Label->SetText(FText::FromString(TEXT("未绑定")));
        FSlateFontInfo Font=Label->GetFont();
        Font.Size=18;
        Label->SetFont(Font);
        Label->SetColorAndOpacity(FSlateColor(FLinearColor(0.95f, 0.78f, 0.28f, 1.f)));
        UHorizontalBoxSlot* LabelSlot=Row->AddChildToHorizontalBox(Label);
        LabelSlot->SetSize(FSlateChildSize(ESlateSizeRule::Automatic));
        LabelSlot->SetHorizontalAlignment(HAlign_Right);
        LabelSlot->SetVerticalAlignment(VAlign_Center);
        LabelSlot->SetPadding(FMargin(12.f, 2.f, 8.f, 2.f));
    }

    struct FControlCaption { const TCHAR* WidgetName; const TCHAR* Caption; };
    static const FControlCaption Captions[] = {
        {TEXT("Input_PlayerName"), TEXT("玩家名称")},
        {TEXT("Slider_MasterVolume"), TEXT("总音量")},
        {TEXT("Slider_MouseSensitivity"), TEXT("鼠标灵敏度")},
        {TEXT("Slider_ResolutionScale"), TEXT("分辨率比例")},
        {TEXT("Check_Fullscreen"), TEXT("全屏显示")},
        {TEXT("Check_VSync"), TEXT("垂直同步")},
        {TEXT("Check_MouseFacing"), TEXT("角色朝向鼠标")},
        {TEXT("Combo_Quality"), TEXT("画质等级")}
    };
    for (const FControlCaption& Entry : Captions)
    {
        const FName CaptionName(*FString::Printf(TEXT("Text_Label_%s"), Entry.WidgetName));
        if (Blueprint->WidgetTree->FindWidget(CaptionName)) continue;
        UWidget* Control=Blueprint->WidgetTree->FindWidget(FName(Entry.WidgetName));
        UPanelWidget* Parent=Control ? Cast<UPanelWidget>(Control->GetParent()) : nullptr;
        if (!Control || !Parent)
        {
            UE_LOG(LogTemp, Warning, TEXT("Could not add caption beside %s"), Entry.WidgetName);
            bAllSucceeded=false;
            continue;
        }

        const int32 ChildIndex=Parent->GetChildIndex(Control);
        Parent->RemoveChild(Control);
        UHorizontalBox* ControlRow=Blueprint->WidgetTree->ConstructWidget<UHorizontalBox>(UHorizontalBox::StaticClass(),
            FName(*FString::Printf(TEXT("Row_%s"), Entry.WidgetName)));
        Parent->InsertChildAt(ChildIndex, ControlRow);

        UTextBlock* Caption=Blueprint->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), CaptionName);
        Caption->bIsVariable=true;
        Caption->SetText(FText::FromString(Entry.Caption));
        FSlateFontInfo CaptionFont=Caption->GetFont();
        CaptionFont.Size=17;
        Caption->SetFont(CaptionFont);
        Caption->SetColorAndOpacity(FSlateColor(FLinearColor(0.9f, 0.9f, 0.9f, 1.f)));
        UHorizontalBoxSlot* CaptionSlot=ControlRow->AddChildToHorizontalBox(Caption);
        CaptionSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        CaptionSlot->SetVerticalAlignment(VAlign_Center);
        CaptionSlot->SetPadding(FMargin(8.f, 5.f, 16.f, 5.f));
        UHorizontalBoxSlot* ControlSlot=ControlRow->AddChildToHorizontalBox(Control);
        ControlSlot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
        ControlSlot->SetVerticalAlignment(VAlign_Center);
        ControlSlot->SetPadding(FMargin(8.f, 5.f));
    }

    struct FSettingsButtonCaption { const TCHAR* WidgetName; const TCHAR* Caption; };
    static const FSettingsButtonCaption ButtonCaptions[] = {
        {TEXT("Button_RebindMoveForward"), TEXT("修改前进按键")},
        {TEXT("Button_RebindMoveBack"), TEXT("修改后退按键")},
        {TEXT("Button_RebindMoveLeft"), TEXT("修改左移按键")},
        {TEXT("Button_RebindMoveRight"), TEXT("修改右移按键")},
        {TEXT("Button_RebindJump"), TEXT("修改跳跃按键")},
        {TEXT("Button_RebindLightAttack"), TEXT("修改普通攻击按键")},
        {TEXT("Button_RebindHeavyAttack"), TEXT("修改重攻击按键")},
        {TEXT("Button_RebindDodge"), TEXT("修改闪避按键")},
        {TEXT("Button_RebindFlyingSword"), TEXT("修改飞剑技能按键")},
        {TEXT("Button_RebindSwitchStance"), TEXT("修改战斗姿态按键")},
        {TEXT("Button_ResetBindings"), TEXT("恢复默认按键")},
        {TEXT("Button_Apply"), TEXT("应用设置")},
        {TEXT("Button_Cancel"), TEXT("返回")}
    };
    for (const FSettingsButtonCaption& Entry : ButtonCaptions)
    {
        UButton* Button=Cast<UButton>(Blueprint->WidgetTree->FindWidget(FName(Entry.WidgetName)));
        UTextBlock* ButtonText=Button && Button->GetChildrenCount() > 0 ? Cast<UTextBlock>(Button->GetChildAt(0)) : nullptr;
        if (ButtonText) ButtonText->SetText(FText::FromString(Entry.Caption));
        else bAllSucceeded=false;
    }

    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    SaveWidgetBlueprint(Blueprint);
    return bAllSucceeded;
}

#include "CVADWidgetLayoutBuilder.h"
#include "WidgetBlueprint.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "UI/CVADUserWidget.h"
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
#include "AnimGraphNode_SequencePlayer.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_LayeredBoneBlend.h"
#include "Animation/Skeleton.h"
#include "BehaviorTree/BehaviorTree.h"
#include "BehaviorTree/BlackboardData.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Object.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Bool.h"
#include "BehaviorTree/Blackboard/BlackboardKeyType_Int.h"
#include "BehaviorTree/Composites/BTComposite_Sequence.h"
#include "Enemy/CVADBTTask_Combat.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
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

            bool bExistingCombatBlend = false;
            for (UEdGraphNode* Existing : Graph->Nodes)
            {
                UAnimGraphNode_LayeredBoneBlend* ExistingBlend = Cast<UAnimGraphNode_LayeredBoneBlend>(Existing);
                if (!ExistingBlend) continue;
                for (FInputBlendPose& Layer : ExistingBlend->Node.LayerSetup)
                {
                    for (FBranchFilter& Filter : Layer.BranchFilters)
                    {
                        if (Filter.BoneName == UpperBodyBone)
                        {
                            Filter.BlendDepth = 1;
                            bExistingCombatBlend = true;
                        }
                    }
                }
            }
            if (bExistingCombatBlend)
            {
                UEdGraphPin* ExistingBlendOutput = nullptr;
                for (UEdGraphNode* Existing : Graph->Nodes)
                {
                    const UAnimGraphNode_LayeredBoneBlend* ExistingBlend = Cast<UAnimGraphNode_LayeredBoneBlend>(Existing);
                    if (!ExistingBlend) continue;
                    for (UEdGraphPin* Pin : ExistingBlend->Pins)
                    {
                        if (Pin && Pin->Direction == EGPD_Output) { ExistingBlendOutput = Pin; break; }
                    }
                    if (ExistingBlendOutput) break;
                }
                if (ExistingBlendOutput && !RootInput->LinkedTo.Contains(ExistingBlendOutput))
                {
                    RootInput->BreakAllPinLinks();
                    Schema->TryCreateConnection(ExistingBlendOutput, RootInput);
                }
                UE_LOG(LogTemp, Display, TEXT("CVAD repaired existing UpperBody blend depth in %s"), *GetNameSafe(Blueprint));
                return true;
            }

            FGraphNodeCreator<UAnimGraphNode_LayeredBoneBlend> BlendCreator(*Graph);
            UAnimGraphNode_LayeredBoneBlend* LayeredBlend = BlendCreator.CreateNode();
            LayeredBlend->Node.AddPose();
            LayeredBlend->Node.LayerSetup[0].BranchFilters.Add({UpperBodyBone, 1});
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

    bool RepairCombatSlotGraph(UAnimBlueprint* Blueprint)
    {
        if (!Blueprint) return false;

        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        for (UEdGraph* Graph : Graphs)
        {
            TArray<UAnimGraphNode_Root*> Roots;
            Graph->GetNodesOfClass(Roots);
            if (Roots.Num() != 1) continue;

            UAnimGraphNode_Root* Root = Roots[0];
            UAnimGraphNode_LayeredBoneBlend* Blend = nullptr;
            UAnimGraphNode_Slot* Slot = nullptr;
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                if (!Node) continue;
                if (!Blend) Blend = Cast<UAnimGraphNode_LayeredBoneBlend>(Node);
                if (!Slot)
                {
                    UAnimGraphNode_Slot* Candidate = Cast<UAnimGraphNode_Slot>(Node);
                    if (Candidate && Candidate->Node.SlotName == TEXT("UpperBody"))
                    {
                        Slot = Candidate;
                    }
                }
            }

            // Assets generated by an older pass are missing the slot input connection.
            // Rebuild the complete layer graph instead of leaving the anim graph in a
            // half-connected state.
            if (!Blend || !Slot)
            {
                if (!InsertCombatSlot(Blueprint)) return false;
                return RepairCombatSlotGraph(Blueprint);
            }

            UEdGraphPin* BasePosePin = Blend->FindPin(TEXT("BasePose"));
            UEdGraphPin* LocomotionOutput = (BasePosePin && BasePosePin->LinkedTo.Num() == 1)
                ? BasePosePin->LinkedTo[0]
                : nullptr;
            if (!LocomotionOutput)
            {
                UEdGraphPin* RootInput = nullptr;
                for (UEdGraphPin* Pin : Root->Pins)
                {
                    if (Pin && Pin->Direction == EGPD_Input) { RootInput = Pin; break; }
                }
                if (RootInput && RootInput->LinkedTo.Num() == 1)
                {
                    LocomotionOutput = RootInput->LinkedTo[0];
                }
            }
            if (!LocomotionOutput) return false;

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
            if (UpperBodyBone.IsNone()) return false;

            Blend->Node.LayerSetup.SetNum(1);
            Blend->Node.LayerSetup[0].BranchFilters.Reset();
            Blend->Node.LayerSetup[0].BranchFilters.Add({UpperBodyBone, 1});
            Blend->Node.bMeshSpaceRotationBlend = true;
            Blend->Node.bBlendRootMotionBasedOnRootBone = true;
            Blend->ReconstructNode();

            BasePosePin = Blend->FindPin(TEXT("BasePose"));
            UEdGraphPin* BlendPosePin = Blend->FindPin(TEXT("BlendPoses_0"));
            UEdGraphPin* BlendOutput = Blend->FindPin(TEXT("Pose"));
            if (!BasePosePin || !BlendPosePin || !BlendOutput) return false;

            UAnimGraphNode_Slot* FullBodySlot = nullptr;
            for (UEdGraphNode* Node : Graph->Nodes)
            {
                UAnimGraphNode_Slot* Candidate = Cast<UAnimGraphNode_Slot>(Node);
                if (Candidate && Candidate->Node.SlotName == TEXT("FullBody"))
                {
                    FullBodySlot = Candidate;
                    break;
                }
            }
            if (!FullBodySlot)
            {
                FGraphNodeCreator<UAnimGraphNode_Slot> FullBodyCreator(*Graph);
                FullBodySlot = FullBodyCreator.CreateNode();
                FullBodySlot->Node.SlotName = TEXT("FullBody");
                FullBodySlot->Node.bAlwaysUpdateSourcePose = true;
                FullBodySlot->NodePosX = Root->NodePosX - 720;
                FullBodySlot->NodePosY = Root->NodePosY + 280;
                FullBodyCreator.Finalize();
            }

            UEdGraphPin* FullBodyInput = nullptr;
            UEdGraphPin* FullBodyOutput = nullptr;
            for (UEdGraphPin* Pin : FullBodySlot->Pins)
            {
                if (!Pin) continue;
                if (Pin->Direction == EGPD_Input && !FullBodyInput) FullBodyInput = Pin;
                if (Pin->Direction == EGPD_Output && !FullBodyOutput) FullBodyOutput = Pin;
            }
            if (!FullBodyInput || !FullBodyOutput) return false;

            UEdGraphPin* SlotInput = nullptr;
            UEdGraphPin* SlotOutput = nullptr;
            for (UEdGraphPin* Pin : Slot->Pins)
            {
                if (!Pin) continue;
                if (Pin->Direction == EGPD_Input && !SlotInput) SlotInput = Pin;
                if (Pin->Direction == EGPD_Output && !SlotOutput) SlotOutput = Pin;
            }
            if (!SlotInput || !SlotOutput) return false;

            UEdGraphPin* RootInput = nullptr;
            for (UEdGraphPin* Pin : Root->Pins)
            {
                if (Pin && Pin->Direction == EGPD_Input) { RootInput = Pin; break; }
            }
            if (!RootInput) return false;

            RootInput->BreakAllPinLinks();
            BasePosePin->BreakAllPinLinks();
            BlendPosePin->BreakAllPinLinks();
            SlotInput->BreakAllPinLinks();
            SlotOutput->BreakAllPinLinks();
            FullBodyInput->BreakAllPinLinks();
            FullBodyOutput->BreakAllPinLinks();

            const bool bConnected = LocomotionOutput->LinkedTo.Num() == 0
                || (LocomotionOutput->Direction == EGPD_Output
                    && BasePosePin->Direction == EGPD_Input
                    && SlotInput->Direction == EGPD_Input);
            if (!bConnected) return false;

            LocomotionOutput->MakeLinkTo(SlotInput);
            LocomotionOutput->MakeLinkTo(BasePosePin);
            SlotOutput->MakeLinkTo(BlendPosePin);
            BlendOutput->MakeLinkTo(FullBodyInput);
            FullBodyOutput->MakeLinkTo(RootInput);

            UE_LOG(LogTemp, Display, TEXT("CVAD repaired combat slot graph in %s at bone %s"),
                *GetNameSafe(Blueprint), *UpperBodyBone.ToString());
            return true;
        }
        return false;
    }

    bool SaveAnimBlueprint(UAnimBlueprint* Blueprint)
    {
        if (!Blueprint || !RepairCombatSlotGraph(Blueprint)) return false;
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
        MakeButton(Menu,TEXT("Button_Settings"),TEXT("设置"));
        MakeButton(Menu,TEXT("Button_CustomKeybindings"),TEXT("自定义按键"));
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
        TEXT("/Game/CVAD/Animations/ABP_LanFang_FlyingSwordV2.ABP_LanFang_FlyingSwordV2"));
    if (!FlyingBlueprint)
    {
        UPackage* BlueprintPackage = CreatePackage(TEXT("/Game/CVAD/Animations/ABP_LanFang_FlyingSwordV2"));
        FlyingBlueprint = Cast<UAnimBlueprint>(StaticDuplicateObject(SourceBlueprint, BlueprintPackage, TEXT("ABP_LanFang_FlyingSwordV2")));
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

    // The blend asset already exists and is saved by the outer asset rebuild. Saving
    // its package again from this commandlet path can re-enter package finalization.
    UE_LOG(LogTemp, Display, TEXT("CVAD combat AnimBPs built with an UpperBody layer; flying blend players=%d"), ReplacedPlayers);
    return true;
}

static bool EnsureAnimBlueprintFloatVariable(UAnimBlueprint* Blueprint, FName VarName)
{
    if (!Blueprint) return false;
    UE_LOG(LogTemp, Display, TEXT("CVAD minion var check before: vars=%d"), Blueprint->NewVariables.Num());
    for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        if (Variable.VarName == VarName)
        {
            UE_LOG(LogTemp, Display, TEXT("CVAD minion var %s already exists"), *VarName.ToString());
            return true;
        }
    }
    FEdGraphPinType PinType;
    // UE 5 stores blueprint floats as PC_Real with the PC_Float sub-category.
    PinType.PinCategory = UEdGraphSchema_K2::PC_Real;
    PinType.PinSubCategory = UEdGraphSchema_K2::PC_Float;
    FBlueprintEditorUtils::AddMemberVariable(Blueprint, VarName, PinType);
    UE_LOG(LogTemp, Display, TEXT("CVAD minion var %s add requested: vars=%d"),
        *VarName.ToString(), Blueprint->NewVariables.Num());
    return true;
}

// The duplicated LanFang AnimBP keeps its graph nodes but can lose the actual
// variable declarations. Re-declare the locomotion variables and re-point every
// existing getter/setter node so the blend space actually receives speed.
static void RepairMinionLocomotionVariable(UAnimBlueprint* Blueprint)
{
    if (!Blueprint) return;

    // The duplicated ABP ships a stale Speed declaration whose pin type no longer
    // compiles into the generated class. Drop it and re-declare it cleanly.
    for (int32 Index = Blueprint->NewVariables.Num() - 1; Index >= 0; --Index)
    {
        const FBPVariableDescription& Variable = Blueprint->NewVariables[Index];
        if (Variable.VarName == TEXT("Speed"))
        {
            UE_LOG(LogTemp, Display, TEXT("CVAD minion removing stale Speed var (type=%s)"),
                *Variable.VarType.PinCategory.ToString());
            FBlueprintEditorUtils::RemoveMemberVariable(Blueprint, Variable.VarName);
        }
    }
    EnsureAnimBlueprintFloatVariable(Blueprint, TEXT("Speed"));

    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
    {
        for (UEdGraphNode* Node : Graph->Nodes)
        {
            FMemberReference* Reference = nullptr;
            FName VarName = NAME_None;
            if (UK2Node_VariableGet* Get = Cast<UK2Node_VariableGet>(Node))
            {
                VarName = Get->GetVarName();
                Reference = &Get->VariableReference;
            }
            else if (UK2Node_VariableSet* Set = Cast<UK2Node_VariableSet>(Node))
            {
                VarName = Set->GetVarName();
                Reference = &Set->VariableReference;
            }
            if (!Reference || VarName != TEXT("Speed")) continue;
            Reference->SetSelfMember(TEXT("Speed"));
        }
    }
    UE_LOG(LogTemp, Display, TEXT("CVAD repaired minion locomotion Speed variable in %s"),
        *Blueprint->GetPathName());
}

bool UCVADEditorAssetBuilder::BuildMinionAnimationBlueprint()
{
    UAnimBlueprint* SourceBlueprint=LoadObject<UAnimBlueprint>(nullptr,
        TEXT("/Game/LanFang/Animations/In-Place/MoveBasic/Female_AnimBP.Female_AnimBP"));
    UBlendSpace1D* SourceBlend=LoadObject<UBlendSpace1D>(nullptr,
        TEXT("/Game/LanFang/Animations/In-Place/MoveBasic/Female_2D.Female_2D"));
    UAnimSequence* Idle=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/SkeletonArmy/Animations/Footman/Skeleton_Idle.Skeleton_Idle"));
    UAnimSequence* Walk=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/SkeletonArmy/Animations/Footman/Skeleton_1H_walk.Skeleton_1H_walk"));
    UAnimSequence* Run=LoadObject<UAnimSequence>(nullptr,TEXT("/Game/SkeletonArmy/Animations/Footman/Skeleton_Run.Skeleton_Run"));
    if(!SourceBlueprint||!SourceBlend||!Idle||!Walk||!Run||!Idle->GetSkeleton()) return false;

    UPackage* BlendPackage=CreatePackage(TEXT("/Game/CVAD/Animations/BS_SkeletonMinion"));
    UBlendSpace1D* Blend=FindObject<UBlendSpace1D>(BlendPackage,TEXT("BS_SkeletonMinion"));
    if(!Blend)
    {
        Blend=Cast<UBlendSpace1D>(StaticDuplicateObject(SourceBlend,BlendPackage,TEXT("BS_SkeletonMinion")));
        FAssetRegistryModule::AssetCreated(Blend);
    }
    Blend->SetSkeleton(Idle->GetSkeleton());
    if(Blend->GetBlendSamples().Num()<3) return false;
    Blend->ReplaceSampleAnimation(0,Idle);
    Blend->ReplaceSampleAnimation(1,Walk);
    Blend->ReplaceSampleAnimation(2,Run);
    Blend->ValidateSampleData();

    UPackage* BlueprintPackage=CreatePackage(TEXT("/Game/CVAD/Animations/ABP_SkeletonMinion"));
    UAnimBlueprint* Blueprint=FindObject<UAnimBlueprint>(BlueprintPackage,TEXT("ABP_SkeletonMinion"));
    if(!Blueprint)
    {
        Blueprint=Cast<UAnimBlueprint>(StaticDuplicateObject(SourceBlueprint,BlueprintPackage,TEXT("ABP_SkeletonMinion")));
        FAssetRegistryModule::AssetCreated(Blueprint);
    }
    Blueprint->TargetSkeleton=Idle->GetSkeleton();
    int32 Replaced=0;
    TArray<UEdGraph*> Graphs;Blueprint->GetAllGraphs(Graphs);
    for(UEdGraph* Graph:Graphs) for(UEdGraphNode* Node:Graph->Nodes)
        if(UAnimGraphNode_BlendSpacePlayer* Player=Cast<UAnimGraphNode_BlendSpacePlayer>(Node))
        {Player->Node.SetBlendSpace(Blend);++Replaced;}
    if(Replaced==0) return false;
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    Blueprint->MarkPackageDirty();Blend->MarkPackageDirty();
    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
    const bool bBlendSaved=UPackage::SavePackage(BlendPackage,Blend,
        *FPackageName::LongPackageNameToFilename(BlendPackage->GetName(),FPackageName::GetAssetPackageExtension()),Args);
    const bool bBlueprintSaved=UPackage::SavePackage(BlueprintPackage,Blueprint,
        *FPackageName::LongPackageNameToFilename(BlueprintPackage->GetName(),FPackageName::GetAssetPackageExtension()),Args);
    UE_LOG(LogTemp,Display,TEXT("CVAD minion AnimBP built Nodes=%d BlendSaved=%s BlueprintSaved=%s"),Replaced,
        bBlendSaved?TEXT("true"):TEXT("false"),bBlueprintSaved?TEXT("true"):TEXT("false"));
    return bBlendSaved&&bBlueprintSaved;
}

bool UCVADEditorAssetBuilder::DiagnoseAnimationBlueprints()
{
    const TArray<const TCHAR*> Paths = {
        TEXT("/Game/CVAD/Animations/ABP_LanFang_Normal"),
        TEXT("/Game/CVAD/Animations/ABP_LanFang_FlyingSword"),
        TEXT("/Game/CVAD/Animations/ABP_LanFang_FlyingSwordV2"),
        TEXT("/Game/CVAD/Animations/ABP_SkeletonMinion"),
    };

    for (const TCHAR* Path : Paths)
    {
        UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, Path);
        if (!Blueprint)
        {
            UE_LOG(LogTemp, Error, TEXT("CVAD_ANIM_DIAG missing AnimBP %s"), Path);
            continue;
        }

        UE_LOG(LogTemp, Display, TEXT("CVAD_ANIM_DIAG ====== %s status=%d parent=%s target_skeleton=%s"),
            *Blueprint->GetPathName(), static_cast<int32>(Blueprint->Status),
            Blueprint->ParentClass ? *Blueprint->ParentClass->GetName() : TEXT("None"),
            Blueprint->TargetSkeleton ? *Blueprint->TargetSkeleton->GetPathName() : TEXT("None"));

        TArray<UEdGraph*> Graphs;
        Blueprint->GetAllGraphs(Graphs);
        for (UEdGraph* Graph : Graphs)
        {
            if (!Graph) continue;
            int32 Roots = 0;
            int32 Slots = 0;
            int32 Blends = 0;
            int32 BlendPlayers = 0;
            int32 Sequences = 0;
            int32 States = 0;
            int32 Others = 0;
            for (UEdGraphNode* GraphNode : Graph->Nodes)
            {
                if (!GraphNode) continue;
                const FString NodeClass = GraphNode->GetClass()->GetName();
                if (Cast<UAnimGraphNode_Root>(GraphNode)) ++Roots;
                else if (Cast<UAnimGraphNode_Slot>(GraphNode)) ++Slots;
                else if (Cast<UAnimGraphNode_LayeredBoneBlend>(GraphNode)) ++Blends;
                else if (Cast<UAnimGraphNode_BlendSpacePlayer>(GraphNode)) ++BlendPlayers;
                else if (GraphNode->IsA<UAnimGraphNode_SequencePlayer>()) ++Sequences;
                else if (NodeClass.Contains(TEXT("StateMachine"))) ++States;
                else ++Others;

                if (UAnimGraphNode_Slot* Slot = Cast<UAnimGraphNode_Slot>(GraphNode))
                {
                    int32 InputLinks = 0;
                    for (UEdGraphPin* Pin : Slot->Pins)
                    {
                        if (Pin && Pin->Direction == EGPD_Input) InputLinks += Pin->LinkedTo.Num();
                    }
                    UE_LOG(LogTemp, Display, TEXT("CVAD_ANIM_DIAG   Slot name=%s input_links=%d"),
                        *Slot->Node.SlotName.ToString(), InputLinks);
                    for (UEdGraphPin* Pin : Slot->Pins)
                    {
                        if (!Pin) continue;
                        for (UEdGraphPin* Linked : Pin->LinkedTo)
                        {
                            UE_LOG(LogTemp, Display, TEXT("CVAD_ANIM_DIAG     Slot pin=%s dir=%d linked_to=%s.%s"),
                                *Pin->PinName.ToString(), static_cast<int32>(Pin->Direction),
                                Linked && Linked->GetOwningNode() ? *Linked->GetOwningNode()->GetName() : TEXT("None"),
                                Linked ? *Linked->PinName.ToString() : TEXT("None"));
                        }
                    }
                }
                else if (UAnimGraphNode_BlendSpacePlayer* Player = Cast<UAnimGraphNode_BlendSpacePlayer>(GraphNode))
                {
                    UE_LOG(LogTemp, Display, TEXT("CVAD_ANIM_DIAG   BlendSpacePlayer blend=%s"),
                        Player->Node.GetBlendSpace() ? *Player->Node.GetBlendSpace()->GetPathName() : TEXT("None"));
                    for (UEdGraphPin* Pin : Player->Pins)
                    {
                        if (!Pin) continue;
                        FString Links;
                        for (UEdGraphPin* Linked : Pin->LinkedTo)
                        {
                            if (Linked && Linked->GetOwningNode())
                            {
                                Links += FString::Printf(TEXT("%s.%s,"),
                                    *Linked->GetOwningNode()->GetName(), *Linked->PinName.ToString());
                            }
                        }
                        UE_LOG(LogTemp, Display, TEXT("CVAD_ANIM_DIAG     BSPlayer pin=%s dir=%d links=%s"),
                            *Pin->PinName.ToString(), static_cast<int32>(Pin->Direction), *Links);
                    }
                }
                else if (UAnimGraphNode_LayeredBoneBlend* Blend = Cast<UAnimGraphNode_LayeredBoneBlend>(GraphNode))
                {
                    for (int32 LayerIndex = 0; LayerIndex < Blend->Node.LayerSetup.Num(); ++LayerIndex)
                    {
                        FString Filters;
                        for (const FBranchFilter& Filter : Blend->Node.LayerSetup[LayerIndex].BranchFilters)
                        {
                            Filters += FString::Printf(TEXT("%s:%d,"), *Filter.BoneName.ToString(), Filter.BlendDepth);
                        }
                        UE_LOG(LogTemp, Display, TEXT("CVAD_ANIM_DIAG   LayeredBlend layer=%d filters=%s"),
                            LayerIndex, *Filters);
                    }
                    for (UEdGraphPin* Pin : Blend->Pins)
                    {
                        if (!Pin) continue;
                        for (UEdGraphPin* Linked : Pin->LinkedTo)
                        {
                            UE_LOG(LogTemp, Display, TEXT("CVAD_ANIM_DIAG     Blend pin=%s dir=%d linked_to=%s.%s"),
                                *Pin->PinName.ToString(), static_cast<int32>(Pin->Direction),
                                Linked && Linked->GetOwningNode() ? *Linked->GetOwningNode()->GetName() : TEXT("None"),
                                Linked ? *Linked->PinName.ToString() : TEXT("None"));
                        }
                    }
                }
                else if (UAnimGraphNode_Root* Root = Cast<UAnimGraphNode_Root>(GraphNode))
                {
                    for (UEdGraphPin* Pin : Root->Pins)
                    {
                        if (!Pin) continue;
                        for (UEdGraphPin* Linked : Pin->LinkedTo)
                        {
                            UE_LOG(LogTemp, Display, TEXT("CVAD_ANIM_DIAG     Root pin=%s dir=%d linked_to=%s.%s"),
                                *Pin->PinName.ToString(), static_cast<int32>(Pin->Direction),
                                Linked && Linked->GetOwningNode() ? *Linked->GetOwningNode()->GetName() : TEXT("None"),
                                Linked ? *Linked->PinName.ToString() : TEXT("None"));
                        }
                    }
                }
            }
            UE_LOG(LogTemp, Display,
                TEXT("CVAD_ANIM_DIAG   Graph=%s roots=%d slots=%d blends=%d blend_players=%d sequences=%d states=%d others=%d"),
                *Graph->GetName(), Roots, Slots, Blends, BlendPlayers, Sequences, States, Others);
        }
    }

    for (const TCHAR* Path : {
        TEXT("/Game/CVAD/Animations/BS_LanFang_FlyingSword"),
        TEXT("/Game/CVAD/Animations/BS_SkeletonMinion")})
    {
        UBlendSpace1D* Blend = LoadObject<UBlendSpace1D>(nullptr, Path);
        if (!Blend)
        {
            UE_LOG(LogTemp, Error, TEXT("CVAD_ANIM_DIAG missing BlendSpace %s"), Path);
            continue;
        }
        UE_LOG(LogTemp, Display, TEXT("CVAD_ANIM_DIAG BlendSpace=%s skeleton=%s samples=%d"),
            *Blend->GetPathName(), Blend->GetSkeleton() ? *Blend->GetSkeleton()->GetPathName() : TEXT("None"),
            Blend->GetBlendSamples().Num());
    }
    return true;
}

bool UCVADEditorAssetBuilder::RepairAnimationBlueprints()
{
    const TArray<const TCHAR*> AnimBlueprintPaths = {
        TEXT("/Game/CVAD/Animations/ABP_LanFang_Normal"),
        TEXT("/Game/CVAD/Animations/ABP_LanFang_FlyingSword"),
        TEXT("/Game/CVAD/Animations/ABP_LanFang_FlyingSwordV2"),
        TEXT("/Game/CVAD/Animations/ABP_SkeletonMinion"),
    };

    bool bAllRepaired = true;
    for (const TCHAR* Path : AnimBlueprintPaths)
    {
        UAnimBlueprint* Blueprint = LoadObject<UAnimBlueprint>(nullptr, Path);
        if (!Blueprint)
        {
            UE_LOG(LogTemp, Error, TEXT("CVAD_ANIM_REPAIR missing %s"), Path);
            bAllRepaired = false;
            continue;
        }
        if (!RepairCombatSlotGraph(Blueprint))
        {
            UE_LOG(LogTemp, Warning, TEXT("CVAD_ANIM_REPAIR could not ensure combat slot in %s"), *Blueprint->GetPathName());
            bAllRepaired = false;
        }
        if (FString(Path).Contains(TEXT("ABP_SkeletonMinion")))
        {
            RepairMinionLocomotionVariable(Blueprint);
        }
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        FKismetEditorUtilities::CompileBlueprint(Blueprint);
        Blueprint->MarkPackageDirty();
        UPackage* Package = Blueprint->GetOutermost();
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        const bool bSaved = UPackage::SavePackage(Package, Blueprint,
            *FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()), Args);
        if (!bSaved) bAllRepaired = false;
        UE_LOG(LogTemp, Display, TEXT("CVAD_ANIM_REPAIR %s saved=%s"),
            *Blueprint->GetPathName(), bSaved ? TEXT("true") : TEXT("false"));
        if (FString(Path).Contains(TEXT("ABP_SkeletonMinion")))
        {
            UClass* GeneratedClass = Blueprint->GeneratedClass;
            FString Props;
            if (GeneratedClass)
            {
                for (TFieldIterator<FProperty> It(GeneratedClass); It; ++It)
                {
                    Props += FString::Printf(TEXT("%s(%s),"), *It->GetName(), *It->GetClass()->GetName());
                }
            }
            UE_LOG(LogTemp, Display, TEXT("CVAD minion generated class %s speed_float=%d props=[%s]"),
                *GetNameSafe(GeneratedClass),
                GeneratedClass ? (FindFProperty<FFloatProperty>(GeneratedClass, TEXT("Speed")) ? 1 : 0) : -1,
                *Props);
        }
    }

    const TArray<TPair<const TCHAR*, const TCHAR*>> SkeletonMeshPairs = {
        {TEXT("/Game/SkeletonArmy/Characters/Footman/SkeletonFootman_Skeleton"),
         TEXT("/Game/SkeletonArmy/Characters/Footman/SK_SkeletonFootman")},
    };
    for (const auto& Pair : SkeletonMeshPairs)
    {
        USkeleton* Skeleton = LoadObject<USkeleton>(nullptr, Pair.Key);
        USkeletalMesh* PreviewMesh = LoadObject<USkeletalMesh>(nullptr, Pair.Value);
        if (!Skeleton || !PreviewMesh)
        {
            UE_LOG(LogTemp, Warning, TEXT("CVAD_ANIM_REPAIR could not repair skeleton preview mesh %s"), Pair.Key);
            bAllRepaired = false;
            continue;
        }
        Skeleton->SetPreviewMesh(PreviewMesh, true);
        Skeleton->MarkPackageDirty();
        UPackage* Package = Skeleton->GetOutermost();
        FSavePackageArgs Args;
        Args.TopLevelFlags = RF_Public | RF_Standalone;
        const bool bSaved = UPackage::SavePackage(Package, Skeleton,
            *FPackageName::LongPackageNameToFilename(Package->GetName(), FPackageName::GetAssetPackageExtension()), Args);
        if (!bSaved) bAllRepaired = false;
        UE_LOG(LogTemp, Display, TEXT("CVAD_ANIM_REPAIR skeleton=%s preview=%s saved=%s"),
            *Skeleton->GetPathName(), *PreviewMesh->GetPathName(), bSaved ? TEXT("true") : TEXT("false"));
    }

    return bAllRepaired;
}

bool UCVADEditorAssetBuilder::BuildEnemyAIAssets()
{
    UPackage* BlackboardPackage=CreatePackage(TEXT("/Game/CVAD/AI/BB_EnemyCombat"));
    UBlackboardData* Blackboard=FindObject<UBlackboardData>(BlackboardPackage,TEXT("BB_EnemyCombat"));
    if(!Blackboard)
    {
        Blackboard=NewObject<UBlackboardData>(BlackboardPackage,TEXT("BB_EnemyCombat"),RF_Public|RF_Standalone);
        FAssetRegistryModule::AssetCreated(Blackboard);
    }
    Blackboard->Keys.Reset();
    auto AddKey=[Blackboard](const TCHAR* Name,UBlackboardKeyType* Type)
    {
        FBlackboardEntry Entry;Entry.EntryName=Name;Entry.KeyType=Type;Blackboard->Keys.Add(Entry);
    };
    UBlackboardKeyType_Object* TargetType=NewObject<UBlackboardKeyType_Object>(Blackboard);
    TargetType->BaseClass=AActor::StaticClass();
    AddKey(TEXT("TargetActor"),TargetType);
    AddKey(TEXT("IsBoss"),NewObject<UBlackboardKeyType_Bool>(Blackboard));
    AddKey(TEXT("BossPhase"),NewObject<UBlackboardKeyType_Int>(Blackboard));
    Blackboard->MarkPackageDirty();

    UPackage* TreePackage=CreatePackage(TEXT("/Game/CVAD/AI/BT_EnemyCombat"));
    UBehaviorTree* Tree=FindObject<UBehaviorTree>(TreePackage,TEXT("BT_EnemyCombat"));
    if(!Tree)
    {
        Tree=NewObject<UBehaviorTree>(TreePackage,TEXT("BT_EnemyCombat"),RF_Public|RF_Standalone);
        FAssetRegistryModule::AssetCreated(Tree);
    }
    Tree->BlackboardAsset=Blackboard;
    UBTComposite_Sequence* Root=NewObject<UBTComposite_Sequence>(Tree,TEXT("CombatLoop"));
    UCVADBTTask_Combat* CombatTask=NewObject<UCVADBTTask_Combat>(Tree,TEXT("AcquireMoveAttack"));
    Root->Children.AddDefaulted();Root->Children[0].ChildTask=CombatTask;Tree->RootNode=Root;
    Tree->MarkPackageDirty();

    FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;
    const bool bBlackboardSaved=UPackage::SavePackage(BlackboardPackage,Blackboard,
        *FPackageName::LongPackageNameToFilename(BlackboardPackage->GetName(),FPackageName::GetAssetPackageExtension()),Args);
    const bool bTreeSaved=UPackage::SavePackage(TreePackage,Tree,
        *FPackageName::LongPackageNameToFilename(TreePackage->GetName(),FPackageName::GetAssetPackageExtension()),Args);
    UE_LOG(LogTemp,Display,TEXT("CVAD AI assets Blackboard=%s BehaviorTree=%s"),
        bBlackboardSaved?TEXT("saved"):TEXT("failed"),bTreeSaved?TEXT("saved"):TEXT("failed"));
    return bBlackboardSaved&&bTreeSaved;
}

bool UCVADEditorAssetBuilder::BuildAllUIControlSkeletons()
{
    struct FPage { const TCHAR* Asset; TArray<FString> Buttons, Texts, Sliders, Checks, Combos, Inputs; };
    const TArray<FPage> Pages = {
        {TEXT("WBP_Lobby"), {TEXT("Button_Ready"),TEXT("Button_StartGame"),TEXT("Button_LeaveLobby"),TEXT("Button_CopyAddress")}, {TEXT("Text_LobbyTitle"),TEXT("Text_HostName"),TEXT("Text_Player1"),TEXT("Text_Player2"),TEXT("Text_ConnectionStatus")}, {}, {}, {}, {TEXT("Input_ServerAddress")}},
        {TEXT("WBP_Multiplayer"), {TEXT("Button_HostListenServer"),TEXT("Button_JoinGame"),TEXT("Button_Close")}, {TEXT("Text_MultiplayerTitle"),TEXT("Text_RoomHelp"),TEXT("Text_Status")}, {}, {}, {}, {TEXT("Input_ServerAddress")}},
        {TEXT("WBP_Pause"), {TEXT("Button_Resume"),TEXT("Button_Settings"),TEXT("Button_CustomKeybindings"),TEXT("Button_SaveGame"),TEXT("Button_LoadGame"),TEXT("Button_ReturnMainMenu")}, {TEXT("Text_PauseTitle")}},
        {TEXT("WBP_Settings"), {TEXT("Button_Apply"),TEXT("Button_Cancel")}, {TEXT("Text_SettingsTitle"),TEXT("Text_VideoSettingsHeader"),TEXT("Text_ResolutionScaleDesc"),TEXT("Text_QualityDesc"),TEXT("Text_FullscreenDesc"),TEXT("Text_VSyncDesc"),TEXT("Text_MouseSensitivityDesc"),TEXT("Text_MouseFacingDesc"),TEXT("Text_AudioSettingsHeader"),TEXT("Text_MasterVolumeDesc"),TEXT("Text_MusicVolumeDesc"),TEXT("Text_SFXVolumeDesc")}, {TEXT("Slider_ResolutionScale"),TEXT("Slider_MouseSensitivity"),TEXT("Slider_MasterVolume"),TEXT("Slider_MusicVolume"),TEXT("Slider_SFXVolume")}, {TEXT("Check_Fullscreen"),TEXT("Check_VSync"),TEXT("Check_MouseFacing")}, {TEXT("Combo_Quality")}, {TEXT("Input_PlayerName")}},
        {TEXT("WBP_Result"), {TEXT("Button_Retry"),TEXT("Button_ReturnLobby"),TEXT("Button_ReturnMainMenu")}, {TEXT("Text_ResultTitle"),TEXT("Text_ClearTime"),TEXT("Text_Defeats"),TEXT("Text_BossResult"),TEXT("Text_ExperienceEarned")}},
        {TEXT("WBP_NameEntry"), {TEXT("Button_ConfirmName"),TEXT("Button_CancelName")}, {TEXT("Text_NameTitle"),TEXT("Text_NameError")}, {}, {}, {}, {TEXT("Input_PlayerName")}},
        {TEXT("WBP_OutfitSelect"), {TEXT("Button_HeadPrev"),TEXT("Button_HeadNext"),TEXT("Button_HairPrev"),TEXT("Button_HairNext"),TEXT("Button_HatPrev"),TEXT("Button_HatNext"),TEXT("Button_UpperPrev"),TEXT("Button_UpperNext"),TEXT("Button_HandsPrev"),TEXT("Button_HandsNext"),TEXT("Button_LowerPrev"),TEXT("Button_LowerNext"),TEXT("Button_FeetPrev"),TEXT("Button_FeetNext"),TEXT("Button_OutfitConfirm"),TEXT("Button_Close")}, {TEXT("Text_OutfitTitle"),TEXT("Text_HeadValue"),TEXT("Text_HairValue"),TEXT("Text_HatValue"),TEXT("Text_UpperValue"),TEXT("Text_HandsValue"),TEXT("Text_LowerValue"),TEXT("Text_FeetValue"),TEXT("Text_OutfitStatus")}, {}, {}, {}, {TEXT("Input_PlayerName")}},
        {TEXT("WBP_SaveSlots"), {TEXT("Button_SaveSlot0"),TEXT("Button_SaveSlot1"),TEXT("Button_SaveSlot2"),TEXT("Button_SaveSelected"),TEXT("Button_LoadSelected"),TEXT("Button_DeleteSelected"),TEXT("Button_Close")}, {TEXT("Text_SaveTitle"),TEXT("Text_SelectedSlot"),TEXT("Text_Slot0"),TEXT("Text_Slot1"),TEXT("Text_Slot2")}},
        {TEXT("WBP_SkillTree"), {TEXT("Button_SwordAttack1"),TEXT("Button_SwordAttack2"),TEXT("Button_SwordAttack3"),TEXT("Button_SwordAttack4"),TEXT("Button_SwordAttack5"),TEXT("Button_FlyingSword1"),TEXT("Button_FlyingSword2"),TEXT("Button_FlyingSword3"),TEXT("Button_EquipSelected"),TEXT("Button_Close")}, {TEXT("Text_SkillTreeTitle"),TEXT("Text_Level"),TEXT("Text_Experience"),TEXT("Text_SkillPoints"),TEXT("Text_EquippedSkills"),TEXT("Text_AvailableSkills"),TEXT("Text_SelectedSkillName"),TEXT("Text_SelectedSkillDescription"),TEXT("Text_Prerequisite"),TEXT("Text_SkillCost")}},
    };
    bool bAllSucceeded = true;
    const TMap<FString,FString> ButtonLabels={
        {TEXT("Button_SinglePlayer"),TEXT("开始游戏")},{TEXT("Button_HostListenServer"),TEXT("创建房间")},{TEXT("Button_JoinGame"),TEXT("加入房间")},{TEXT("Button_LoadGame"),TEXT("读取存档")},{TEXT("Button_Settings"),TEXT("游戏设置")},{TEXT("Button_Quit"),TEXT("退出游戏")},
        {TEXT("Button_CustomKeybindings"),TEXT("自定义按键")},
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
        if(Page.Asset==FString(TEXT("WBP_OutfitSelect")))
        {
            UCanvasPanel* Root=Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("OutfitRoot")); Tree->RootWidget=Root;
            auto Place=[Root](UWidget* Widget,float X,float Y,float W,float H)
            {
                UCanvasPanelSlot* Slot=Root->AddChildToCanvas(Widget);
                Slot->SetPosition(FVector2D(X,Y)); Slot->SetSize(FVector2D(W,H));
            };
            auto AddText=[Tree,&Place](const TCHAR* Name,const FText& Value,float X,float Y,float W,float H,int32 FontSize)
            {
                UTextBlock* Label=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name); Label->bIsVariable=true;
                Label->SetText(Value); Label->SetJustification(ETextJustify::Center); FSlateFontInfo Font=Label->GetFont(); Font.Size=FontSize; Label->SetFont(Font);
                Place(Label,X,Y,W,H); return Label;
            };
            auto AddButton=[Tree,&Place](const TCHAR* Name,const FText& Value,float X,float Y,float W,float H)
            {
                UButton* Button=Tree->ConstructWidget<UButton>(UButton::StaticClass(),Name); Button->bIsVariable=true;
                UTextBlock* Label=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),*FString::Printf(TEXT("Label_%s"),Name));
                Label->SetText(Value); Label->SetJustification(ETextJustify::Center); FSlateFontInfo Font=Label->GetFont(); Font.Size=18; Label->SetFont(Font); Button->AddChild(Label);
                Place(Button,X,Y,W,H); return Button;
            };

            UBorder* Background=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("OutfitBackground"));
            Background->SetBrushColor(FLinearColor(0.012f,0.025f,0.055f,1.f)); Place(Background,0.f,0.f,1366.f,768.f);
            AddText(TEXT("Text_OutfitTitle"),FText::FromString(TEXT("角色外观")),483.f,20.f,400.f,48.f,30);
            UViewport* Preview=Tree->ConstructWidget<UViewport>(UViewport::StaticClass(),TEXT("Viewport_OutfitPreview")); Preview->bIsVariable=true;
            Place(Preview,403.f,78.f,560.f,570.f);
            const TCHAR* ValueNames[]={TEXT("Text_HairValue"),TEXT("Text_HatValue"),TEXT("Text_UpperValue"),TEXT("Text_LowerValue"),TEXT("Text_FeetValue")};
            const TCHAR* PrevNames[]={TEXT("Button_HairPrev"),TEXT("Button_HatPrev"),TEXT("Button_UpperPrev"),TEXT("Button_LowerPrev"),TEXT("Button_FeetPrev")};
            const TCHAR* NextNames[]={TEXT("Button_HairNext"),TEXT("Button_HatNext"),TEXT("Button_UpperNext"),TEXT("Button_LowerNext"),TEXT("Button_FeetNext")};
            const TCHAR* Categories[]={TEXT("发型"),TEXT("帽子"),TEXT("上装"),TEXT("下装"),TEXT("鞋子")};
            for(int32 Index=0;Index<5;++Index)
            {
                const float Y=145.f+Index*88.f;
                AddButton(PrevNames[Index],FText::FromString(FString::Printf(TEXT("<  %s"),Categories[Index])),183.f,Y,200.f,46.f);
                AddButton(NextNames[Index],FText::FromString(FString::Printf(TEXT("%s  >"),Categories[Index])),983.f,Y,200.f,46.f);
                AddText(ValueNames[Index],FText::FromString(Categories[Index]),513.f,Y,340.f,36.f,17);
            }
            UEditableTextBox* NameInput=Tree->ConstructWidget<UEditableTextBox>(UEditableTextBox::StaticClass(),TEXT("Input_PlayerName")); NameInput->bIsVariable=true;
            NameInput->SetHintText(FText::FromString(TEXT("输入角色名称"))); Place(NameInput,403.f,662.f,360.f,42.f);
            AddButton(TEXT("Button_OutfitConfirm"),FText::FromString(TEXT("确认使用")),783.f,662.f,180.f,42.f);
            AddButton(TEXT("Button_CameraZoomIn"),FText::FromString(TEXT("镜头拉近")),403.f,612.f,104.f,38.f);
            AddButton(TEXT("Button_CameraZoomOut"),FText::FromString(TEXT("镜头拉远")),513.f,612.f,104.f,38.f);
            AddButton(TEXT("Button_CameraUp"),FText::FromString(TEXT("镜头上移")),623.f,612.f,104.f,38.f);
            AddButton(TEXT("Button_CameraDown"),FText::FromString(TEXT("镜头下移")),733.f,612.f,104.f,38.f);
            AddButton(TEXT("Button_CameraReset"),FText::FromString(TEXT("重置镜头")),843.f,612.f,120.f,38.f);
            AddButton(TEXT("Button_Close"),FText::FromString(TEXT("返回")),34.f,30.f,130.f,42.f);
            AddText(TEXT("Text_OutfitStatus"),FText::GetEmpty(),403.f,710.f,560.f,32.f,16);
            SaveWidgetBlueprint(BP);
            continue;
        }
        if(Page.Asset==FString(TEXT("WBP_SkillTree")))
        {
            UCanvasPanel* Root=Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("SkillRoot"));Tree->RootWidget=Root;
            auto Place=[Root](UWidget* Widget,float X,float Y,float W,float H){UCanvasPanelSlot* Slot=Root->AddChildToCanvas(Widget);Slot->SetPosition(FVector2D(X,Y));Slot->SetSize(FVector2D(W,H));};
            auto Text=[Tree,&Place](const TCHAR* Name,const TCHAR* Value,float X,float Y,float W,float H,int32 Size,ETextJustify::Type Align=ETextJustify::Left)
            {UTextBlock* T=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);T->bIsVariable=true;T->SetText(FText::FromString(Value));T->SetJustification(Align);FSlateFontInfo F=T->GetFont();F.Size=Size;T->SetFont(F);Place(T,X,Y,W,H);return T;};
            auto Button=[Tree,&Place](const TCHAR* Name,const TCHAR* Value,float X,float Y,float W,float H)
            {UButton* B=Tree->ConstructWidget<UButton>(UButton::StaticClass(),Name);B->bIsVariable=true;UTextBlock* L=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),*FString::Printf(TEXT("Label_%s"),Name));L->SetText(FText::FromString(Value));L->SetJustification(ETextJustify::Center);FSlateFontInfo F=L->GetFont();F.Size=18;L->SetFont(F);B->AddChild(L);Place(B,X,Y,W,H);return B;};
            UBorder* BG=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("SkillBackground"));BG->SetBrushColor(FLinearColor(0.01f,0.02f,0.045f,1.f));Place(BG,0,0,1366,768);
            Text(TEXT("Text_SkillTreeTitle"),TEXT("技能装配"),410,24,546,48,30,ETextJustify::Center);
            Text(TEXT("Text_Level"),TEXT("等级 1"),70,82,180,34,18);Text(TEXT("Text_Experience"),TEXT("经验 0/100"),260,82,220,34,18);Text(TEXT("Text_SkillPoints"),TEXT("技能点 99"),500,82,220,34,20);
            Text(TEXT("Text_AvailableSkills"),TEXT("可选技能（点击查看演示）"),70,135,530,40,22);Text(TEXT("Text_EquippedTitle"),TEXT("当前装配"),70,590,530,36,20);
            const TCHAR* SkillButtons[]={TEXT("Button_SwordAttack1"),TEXT("Button_SwordAttack2"),TEXT("Button_SwordAttack3"),TEXT("Button_SwordAttack4"),TEXT("Button_SwordAttack5"),TEXT("Button_FlyingSword1"),TEXT("Button_FlyingSword2"),TEXT("Button_FlyingSword3")};
            const TCHAR* SkillLabels[]={TEXT("持剑 · 破阵式"),TEXT("持剑 · 普攻二"),TEXT("持剑 · 普攻三"),TEXT("持剑 · 普攻四"),TEXT("持剑 · 绝技"),TEXT("御剑 · 飞剑诀"),TEXT("御剑 · 剑阵"),TEXT("御剑 · 追魂剑")};
            for(int32 I=0;I<8;++I) Button(SkillButtons[I],SkillLabels[I],70+(I%2)*270,185+(I/2)*82,250,60);
            UBorder* Detail=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("SkillDetailPanel"));Detail->SetBrushColor(FLinearColor(0.035f,0.065f,0.12f,1.f));Place(Detail,650,135,640,475);
            Text(TEXT("Text_SelectedSkillName"),TEXT("请选择一项技能"),690,172,560,48,26);
            Text(TEXT("Text_SkillDemoLabel"),TEXT("技能演示"),690,225,200,26,18);
            UViewport* SkillPreview=Tree->ConstructWidget<UViewport>(UViewport::StaticClass(),TEXT("Viewport_SkillPreview"));SkillPreview->bIsVariable=true;Place(SkillPreview,690,250,300,170);
            Text(TEXT("Text_SelectedSkillDescription"),TEXT("这里显示技能效果、伤害、范围和升级说明。"),690,430,560,95,18);
            Text(TEXT("Text_Prerequisite"),TEXT("前置：无"),690,530,560,34,18);
            Text(TEXT("Text_SkillCost"),TEXT("消耗技能点：0"),690,566,560,34,18);
            Button(TEXT("Button_EquipSelected"),TEXT("购买并装配所选技能"),690,606,300,54);
            Text(TEXT("Text_EquippedSkills"),TEXT("已装配技能会显示在这里"),70,628,550,42,18);
            Button(TEXT("Button_Close"),TEXT("返回"),70,690,150,46);
            SaveWidgetBlueprint(BP);continue;
        }
        if(Page.Asset==FString(TEXT("WBP_Settings")))
        {
            UCanvasPanel* Root=Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(),TEXT("SettingsRoot"));Tree->RootWidget=Root;
            auto Place=[Root](UWidget* Widget,float X,float Y,float W,float H){UCanvasPanelSlot* Slot=Root->AddChildToCanvas(Widget);Slot->SetPosition(FVector2D(X,Y));Slot->SetSize(FVector2D(W,H));};
            auto Text=[Tree,&Place](const TCHAR* Name,const FText& Value,float X,float Y,float W,float H,int32 Size)
            {UTextBlock* T=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),Name);T->bIsVariable=true;T->SetText(Value);T->SetJustification(ETextJustify::Left);FSlateFontInfo F=T->GetFont();F.Size=Size;T->SetFont(F);Place(T,X,Y,W,H);return T;};
            auto Slider=[Tree,&Place](const TCHAR* Name,float X,float Y,float W,float H){USlider* S=Tree->ConstructWidget<USlider>(USlider::StaticClass(),Name);S->bIsVariable=true;Place(S,X,Y,W,H);return S;};
            auto Check=[Tree,&Place](const TCHAR* Name,const TCHAR* Label,float X,float Y,float W,float H)
            {UCheckBox* C=Tree->ConstructWidget<UCheckBox>(UCheckBox::StaticClass(),Name);C->bIsVariable=true;Place(C,X,Y,W,H);UTextBlock* L=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),*FString::Printf(TEXT("Label_%s"),Name));L->SetText(FText::FromString(Label));L->SetJustification(ETextJustify::Left);FSlateFontInfo F=L->GetFont();F.Size=16;L->SetFont(F);Place(L,X+42,Y,500,H);return C;};

            UBorder* BG=Tree->ConstructWidget<UBorder>(UBorder::StaticClass(),TEXT("SettingsBackground"));BG->SetBrushColor(FLinearColor(0.01f,0.02f,0.045f,1.f));Place(BG,0.f,0.f,1366.f,768.f);
            Text(TEXT("Text_SettingsTitle"),FText::FromString(TEXT("游戏设置")),410.f,24.f,546.f,48.f,30);
            Text(TEXT("Text_VideoSettingsHeader"),FText::FromString(TEXT("画面设置")),80.f,90.f,220.f,32.f,22);
            Text(TEXT("Text_ResolutionScaleDesc"),FText::FromString(TEXT("渲染比例：降低可提升帧数，提高画面更清晰。")),80.f,135.f,900.f,26.f,16);
            Slider(TEXT("Slider_ResolutionScale"),80.f,165.f,900.f,32.f);
            Text(TEXT("Text_QualityDesc"),FText::FromString(TEXT("整体画质：Low / Medium / High / Epic / Cinematic。")),80.f,205.f,900.f,26.f,16);
            UComboBoxString* Quality=Tree->ConstructWidget<UComboBoxString>(UComboBoxString::StaticClass(),TEXT("Combo_Quality"));Quality->bIsVariable=true;Place(Quality,80.f,235.f,400.f,36.f);
            Text(TEXT("Text_FullscreenDesc"),FText::FromString(TEXT("全屏：以独占或窗口化全屏模式运行游戏。")),80.f,285.f,900.f,26.f,16);
            Check(TEXT("Check_Fullscreen"),TEXT("全屏"),80.f,315.f,30.f,30.f);
            Text(TEXT("Text_VSyncDesc"),FText::FromString(TEXT("垂直同步：开启可减少画面撕裂，但可能增加输入延迟。")),80.f,355.f,900.f,26.f,16);
            Check(TEXT("Check_VSync"),TEXT("垂直同步"),80.f,385.f,30.f,30.f);
            Text(TEXT("Text_MouseSensitivityDesc"),FText::FromString(TEXT("鼠标灵敏度：控制镜头旋转速度。")),80.f,425.f,900.f,26.f,16);
            Slider(TEXT("Slider_MouseSensitivity"),80.f,455.f,900.f,32.f);
            Text(TEXT("Text_MouseFacingDesc"),FText::FromString(TEXT("鼠标朝向：开启后角色朝向跟随准星/镜头水平方向。")),80.f,495.f,900.f,26.f,16);
            Check(TEXT("Check_MouseFacing"),TEXT("鼠标朝向"),80.f,525.f,30.f,30.f);
            Text(TEXT("Text_AudioSettingsHeader"),FText::FromString(TEXT("音频设置")),80.f,575.f,220.f,32.f,22);
            Text(TEXT("Text_MasterVolumeDesc"),FText::FromString(TEXT("主音量：控制所有声音的总体大小。")),80.f,620.f,900.f,26.f,16);
            Slider(TEXT("Slider_MasterVolume"),80.f,650.f,900.f,32.f);
            Text(TEXT("Text_MusicVolumeDesc"),FText::FromString(TEXT("音乐音量：控制背景音乐。")),80.f,690.f,900.f,26.f,16);
            Slider(TEXT("Slider_MusicVolume"),80.f,720.f,900.f,32.f);
            Text(TEXT("Text_SFXVolumeDesc"),FText::FromString(TEXT("音效音量：控制攻击、受击等音效。")),80.f,760.f,900.f,26.f,16);
            Slider(TEXT("Slider_SFXVolume"),80.f,790.f,900.f,32.f);
            UButton* Apply=Tree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("Button_Apply"));Apply->bIsVariable=true;UTextBlock* ApplyLabel=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("Label_Button_Apply"));ApplyLabel->SetText(FText::FromString(TEXT("应用设置")));ApplyLabel->SetJustification(ETextJustify::Center);FSlateFontInfo AF=ApplyLabel->GetFont();AF.Size=18;ApplyLabel->SetFont(AF);Apply->AddChild(ApplyLabel);Place(Apply,80.f,840.f,220.f,46.f);
            UButton* Cancel=Tree->ConstructWidget<UButton>(UButton::StaticClass(),TEXT("Button_Cancel"));Cancel->bIsVariable=true;UTextBlock* CancelLabel=Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("Label_Button_Cancel"));CancelLabel->SetText(FText::FromString(TEXT("返回")));CancelLabel->SetJustification(ETextJustify::Center);FSlateFontInfo CF=CancelLabel->GetFont();CF.Size=18;CancelLabel->SetFont(CF);Cancel->AddChild(CancelLabel);Place(Cancel,320.f,840.f,220.f,46.f);
            SaveWidgetBlueprint(BP);continue;
        }
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
        SaveWidgetBlueprint(BP);
    }
    return bAllSucceeded;
}

bool UCVADEditorAssetBuilder::BuildAllWidgetLayouts()
{
    return BuildMainHUDV2() && BuildMainMenuV2() && BuildCustomKeybindingsScreen();
}

bool UCVADEditorAssetBuilder::BuildCustomKeybindingsScreen()
{
    UWidgetBlueprint* Blueprint = LoadWidgetBlueprint(TEXT("/Game/CVAD/UI/WBP_CustomKeybindings.WBP_CustomKeybindings"));
    if (!Blueprint)
    {
        UPackage* Package = CreatePackage(TEXT("/Game/CVAD/UI/WBP_CustomKeybindings"));
        Blueprint = NewObject<UWidgetBlueprint>(Package, TEXT("WBP_CustomKeybindings"), RF_Public | RF_Standalone);
        Blueprint->ParentClass = UCVADUserWidget::StaticClass();
        Blueprint->WidgetTree = NewObject<UWidgetTree>(Blueprint, TEXT("WidgetTree"), RF_Transactional);
        FAssetRegistryModule::AssetCreated(Blueprint);
    }
    if (!Blueprint || !Blueprint->WidgetTree) return false;
    Blueprint->ParentClass = UCVADUserWidget::StaticClass();

    UWidgetTree* Tree = Blueprint->WidgetTree;
    Tree->Modify();
    Tree->RootWidget = nullptr;

    UCanvasPanel* Root = Tree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("CustomKeybindingsRoot"));
    Tree->RootWidget = Root;

    auto Place = [Root](UWidget* Widget, float X, float Y, float W, float H)
    {
        UCanvasPanelSlot* Slot = Root->AddChildToCanvas(Widget);
        Slot->SetPosition(FVector2D(X, Y));
        Slot->SetSize(FVector2D(W, H));
    };
    auto Text = [Tree, &Place](const TCHAR* Name, const FText& Value, float X, float Y, float W, float H, int32 Size)
    {
        UTextBlock* T = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
        T->bIsVariable = true;
        T->SetText(Value);
        T->SetJustification(ETextJustify::Left);
        FSlateFontInfo Font = T->GetFont();
        Font.Size = Size;
        T->SetFont(Font);
        Place(T, X, Y, W, H);
        return T;
    };

    UBorder* Background = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("KeybindingsBackground"));
    Background->SetBrushColor(FLinearColor(0.01f, 0.02f, 0.045f, 1.f));
    Place(Background, 0.f, 0.f, 1366.f, 768.f);
    Text(TEXT("Text_KeybindingsTitle"), FText::FromString(TEXT("自定义按键")), 470.f, 30.f, 430.f, 48.f, 30);
    Text(TEXT("Text_KeybindingsHint"), FText::FromString(TEXT("点击任一按钮后，按下新的按键即可绑定。")), 400.f, 88.f, 570.f, 30.f, 17);

    struct FKeybindAction { const TCHAR* Button; const TCHAR* KeyText; const TCHAR* Label; };
    const FKeybindAction Actions[] = {
        {TEXT("Button_RebindMoveForward"), TEXT("Text_Key_MoveForward"), TEXT("前进")},
        {TEXT("Button_RebindMoveBack"), TEXT("Text_Key_MoveBack"), TEXT("后退")},
        {TEXT("Button_RebindMoveLeft"), TEXT("Text_Key_MoveLeft"), TEXT("左移")},
        {TEXT("Button_RebindMoveRight"), TEXT("Text_Key_MoveRight"), TEXT("右移")},
        {TEXT("Button_RebindJump"), TEXT("Text_Key_Jump"), TEXT("跳跃")},
        {TEXT("Button_RebindLightAttack"), TEXT("Text_Key_LightAttack"), TEXT("普通攻击")},
        {TEXT("Button_RebindHeavyAttack"), TEXT("Text_Key_HeavyAttack"), TEXT("重攻击")},
        {TEXT("Button_RebindDodge"), TEXT("Text_Key_Dodge"), TEXT("闪避 / 翻滚")},
        {TEXT("Button_RebindFlyingSword"), TEXT("Text_Key_FlyingSword"), TEXT("飞剑技能")},
        {TEXT("Button_RebindSwitchStance"), TEXT("Text_Key_SwitchStance"), TEXT("切换战斗姿态")},
    };

    for (int32 Index = 0; Index < UE_ARRAY_COUNT(Actions); ++Index)
    {
        const float Y = 135.f + Index * 48.f;
        UButton* Button = Tree->ConstructWidget<UButton>(UButton::StaticClass(), Actions[Index].Button);
        Button->bIsVariable = true;
        UTextBlock* Label = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),
            *FString::Printf(TEXT("Label_%s"), Actions[Index].Button));
        Label->SetText(FText::FromString(Actions[Index].Label));
        Label->SetJustification(ETextJustify::Center);
        FSlateFontInfo Font = Label->GetFont();
        Font.Size = 18;
        Label->SetFont(Font);
        Button->AddChild(Label);
        Place(Button, 300.f, Y, 260.f, 42.f);

        UTextBlock* KeyText = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Actions[Index].KeyText);
        KeyText->bIsVariable = true;
        KeyText->SetText(FText::FromString(TEXT("当前按键：未绑定")));
        KeyText->SetJustification(ETextJustify::Left);
        FSlateFontInfo KeyFont = KeyText->GetFont();
        KeyFont.Size = 17;
        KeyText->SetFont(KeyFont);
        Place(KeyText, 590.f, Y, 340.f, 42.f);
    }

    UButton* ResetButton = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_ResetBindings"));
    ResetButton->bIsVariable = true;
    UTextBlock* ResetLabel = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label_Button_ResetBindings"));
    ResetLabel->SetText(FText::FromString(TEXT("恢复默认按键")));
    ResetLabel->SetJustification(ETextJustify::Center);
    FSlateFontInfo ResetFont = ResetLabel->GetFont();
    ResetFont.Size = 18;
    ResetLabel->SetFont(ResetFont);
    ResetButton->AddChild(ResetLabel);
    Place(ResetButton, 300.f, 640.f, 260.f, 46.f);

    UButton* CloseButton = Tree->ConstructWidget<UButton>(UButton::StaticClass(), TEXT("Button_Close"));
    CloseButton->bIsVariable = true;
    UTextBlock* CloseLabel = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), TEXT("Label_Button_Close"));
    CloseLabel->SetText(FText::FromString(TEXT("返回")));
    CloseLabel->SetJustification(ETextJustify::Center);
    FSlateFontInfo CloseFont = CloseLabel->GetFont();
    CloseFont.Size = 18;
    CloseLabel->SetFont(CloseFont);
    CloseButton->AddChild(CloseLabel);
    Place(CloseButton, 590.f, 640.f, 260.f, 46.f);

    SaveWidgetBlueprint(Blueprint);
    return true;
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
        {TEXT("/Game/CVAD/UI/WBP_CustomKeybindings.WBP_CustomKeybindings"), TEXT("Button_Close")},
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

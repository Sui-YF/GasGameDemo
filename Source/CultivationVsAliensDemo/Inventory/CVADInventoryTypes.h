#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "CVADInventoryTypes.generated.h"

UENUM(BlueprintType)
enum class ECVADItemType : uint8
{
    Head,
    UpperBody,
    LowerBody,
    Feet,
    Hands,
    Consumable
};

USTRUCT(BlueprintType)
struct FCVADEquipmentLoadout
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Head = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName UpperBody = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName LowerBody = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Feet = NAME_None;
    UPROPERTY(EditAnywhere, BlueprintReadOnly) FName Hands = NAME_None;

    FName GetItem(ECVADItemType Type) const;
    void SetItem(ECVADItemType Type, FName ItemId);
};

UCLASS(BlueprintType)
class CULTIVATIONVSALIENSDEMO_API UCVADItemDefinition : public UPrimaryDataAsset
{
    GENERATED_BODY()

public:
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item") FName ItemId = NAME_None;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item") FText DisplayName;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item") ECVADItemType ItemType = ECVADItemType::Consumable;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Item") TObjectPtr<UTexture2D> Icon;
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="Appearance") TSoftObjectPtr<USkeletalMesh> AppearanceMesh;
};

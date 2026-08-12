#include "Inventory/CVADInventoryTypes.h"

FName FCVADEquipmentLoadout::GetItem(ECVADItemType Type) const
{
    switch (Type)
    {
    case ECVADItemType::Head: return Head;
    case ECVADItemType::UpperBody: return UpperBody;
    case ECVADItemType::LowerBody: return LowerBody;
    case ECVADItemType::Feet: return Feet;
    case ECVADItemType::Hands: return Hands;
    default: return NAME_None;
    }
}

void FCVADEquipmentLoadout::SetItem(ECVADItemType Type, FName ItemId)
{
    switch (Type)
    {
    case ECVADItemType::Head: Head = ItemId; break;
    case ECVADItemType::UpperBody: UpperBody = ItemId; break;
    case ECVADItemType::LowerBody: LowerBody = ItemId; break;
    case ECVADItemType::Feet: Feet = ItemId; break;
    case ECVADItemType::Hands: Hands = ItemId; break;
    default: break;
    }
}

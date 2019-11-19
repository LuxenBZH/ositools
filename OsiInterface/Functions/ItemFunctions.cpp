#include <stdafx.h>
#include "FunctionLibrary.h"
#include <OsirisProxy.h>
#include "PropertyMaps.h"

namespace osidbg
{
	namespace func
	{
		bool ItemGetStatsId(OsiArgumentDesc & args)
		{
			auto itemGuid = args[0].String;
			auto item = FindItemByNameGuid(itemGuid);
			if (item == nullptr) {
				OsiError("Item '" << itemGuid << "' does not exist!");
				return false;
			}

			if (!item->StatsId.Str) {
				OsiError("Item '" << itemGuid << "' has no stats ID!");
				return false;
			} else {
				args[1].String = item->StatsId.Str;
				return true;
			}
		}

		bool ItemGetGenerationParams(OsiArgumentDesc & args)
		{
			auto itemGuid = args[0].String;
			auto item = FindItemByNameGuid(itemGuid);
			if (item == nullptr) {
				OsiError("Item '" << itemGuid << "' does not exist!");
				return false;
			}

			if (!item->Generation) {
				OsiError("Item '" << itemGuid << "' has no generation data!");
				return false;
			} else {
				args[1].String = item->Generation->Base.Str ? item->Generation->Base.Str : "";
				args[2].String = item->Generation->ItemType.Str ? item->Generation->ItemType.Str : "";
				args[3].Int32 = item->Generation->Level;
				return true;
			}
		}

		bool ItemHasDeltaModifier(OsiArgumentDesc & args)
		{
			auto itemGuid = args[0].String;
			auto item = FindItemByNameGuid(itemGuid);
			if (item == nullptr) {
				OsiError("Item '" << itemGuid << "' does not exist!");
				return false;
			}

			auto deltaMod = args[1].String;

			int32_t count = 0;
			if (item->StatsDynamic != nullptr) {
				auto const & boosts = item->StatsDynamic->BoostNameSet;
				for (uint32_t i = 0; i < boosts.Set.Size; i++) {
					if (strcmp(boosts.Set.Buf[i].Str, deltaMod) == 0) {
						count++;
					}
				}
			}

			if (item->Generation != nullptr) {
				auto const & boosts = item->Generation->Boosts;
				for (uint32_t i = 0; i < boosts.Size; i++) {
					if (strcmp(boosts.Buf[i].Str, deltaMod) == 0) {
						count++;
					}
				}
			}

			args[2].Int32 = count;
			return count > 0;
		}

		void ItemSetIdentified(OsiArgumentDesc const & args)
		{
			auto itemGuid = args[0].String;
			auto item = FindItemByNameGuid(itemGuid);
			if (item == nullptr) {
				OsiError("Item '" << itemGuid << "' does not exist!");
				return;
			}

			item->StatsDynamic->IsIdentified = args[1].Int32 ? 1 : 0;
		}

		CDivinityStats_Equipment_Attributes * GetItemDynamicStat(esv::Item * item, uint32_t index)
		{
			if (item->StatsDynamic == nullptr) {
				OsiError("Item has no dynamic stats!");
				return nullptr;
			}

			auto numStats = item->StatsDynamic->DynamicAttributes_End - item->StatsDynamic->DynamicAttributes_Start;
			if (numStats <= 1) {
				OsiError("Tried to get dynamic stat " << index << ", item only has " << numStats);
				return nullptr;
			}

			return item->StatsDynamic->DynamicAttributes_Start[index];
		}

		template <OsiPropertyMapType Type>
		bool ItemGetPermanentBoost(OsiArgumentDesc & args)
		{
			auto item = FindItemByNameGuid(args[0].String);
			if (item == nullptr) return false;

			auto permanentBoosts = GetItemDynamicStat(item, 1);
			if (permanentBoosts == nullptr) return false;

			switch (permanentBoosts->StatsType) {
			case EquipmentStatsType::Weapon:
			{
				auto * weapon = static_cast<CDivinityStats_Equipment_Attributes_Weapon *>(permanentBoosts);
				return OsirisPropertyMapGet(gEquipmentAttributesWeaponPropertyMap, weapon, args, 1, Type);
			}

			case EquipmentStatsType::Armor:
			{
				auto * armor = static_cast<CDivinityStats_Equipment_Attributes_Armor *>(permanentBoosts);
				return OsirisPropertyMapGet(gEquipmentAttributesArmorPropertyMap, armor, args, 1, Type);
			}

			case EquipmentStatsType::Shield:
			{
				auto * shield = static_cast<CDivinityStats_Equipment_Attributes_Shield *>(permanentBoosts);
				return OsirisPropertyMapGet(gEquipmentAttributesShieldPropertyMap, shield, args, 1, Type);
			}

			default:
				OsiError("Unknown equipment stats type: " << (unsigned)permanentBoosts->StatsType);
				return false;
			}
		}

		template <OsiPropertyMapType Type>
		void ItemSetPermanentBoost(OsiArgumentDesc const & args)
		{
			auto item = FindItemByNameGuid(args[0].String);
			if (item == nullptr) return;

			auto permanentBoosts = GetItemDynamicStat(item, 1);
			if (permanentBoosts == nullptr) return;

			switch (permanentBoosts->StatsType) {
			case EquipmentStatsType::Weapon:
			{
				auto * weapon = static_cast<CDivinityStats_Equipment_Attributes_Weapon *>(permanentBoosts);
				OsirisPropertyMapSet(gEquipmentAttributesWeaponPropertyMap, weapon, args, 1, Type);
				break;
			}

			case EquipmentStatsType::Armor:
			{
				auto * armor = static_cast<CDivinityStats_Equipment_Attributes_Armor *>(permanentBoosts);
				OsirisPropertyMapSet(gEquipmentAttributesArmorPropertyMap, armor, args, 1, Type);
				break;
			}

			case EquipmentStatsType::Shield:
			{
				auto * shield = static_cast<CDivinityStats_Equipment_Attributes_Shield *>(permanentBoosts);
				OsirisPropertyMapSet(gEquipmentAttributesShieldPropertyMap, shield, args, 1, Type);
				break;
			}

			default:
				OsiError("Unknown equipment stats type: " << (unsigned)permanentBoosts->StatsType);
			}
		}


		void ItemCloneBegin(OsiArgumentDesc const & args)
		{
			if (ExtensionState::Get().PendingItemClone) {
				OsiWarn("ItemCloneBegin() called when a clone is already in progress. Previous clone state will be discarded.");
			}

			auto parseItem = gOsirisProxy->GetLibraryManager().ParseItem;
			auto createItemFromParsed = gOsirisProxy->GetLibraryManager().CreateItemFromParsed;
			if (parseItem == nullptr || createItemFromParsed == nullptr) {
				OsiError("esv::ParseItem not found!");
				return;
			}
			
			ExtensionState::Get().PendingItemClone.reset();

			auto itemGuid = args[0].String;
			auto item = FindItemByNameGuid(itemGuid);
			if (item == nullptr) return;

			auto & clone = ExtensionState::Get().PendingItemClone;
			clone = std::make_unique<ObjectSet<eoc::ItemDefinition>>();
			parseItem(item, clone.get(), false, false);

			if (clone->Set.Size != 1) {
				OsiError("Something went wrong during item parsing. Item set size: " << clone->Set.Size);
				clone.reset();
				return;
			}
		}


		bool ItemClone(OsiArgumentDesc & args)
		{
			if (!ExtensionState::Get().PendingItemClone) {
				OsiError("No item clone is in progress");
				return false;
			}

			auto & clone = ExtensionState::Get().PendingItemClone;
			auto item = gOsirisProxy->GetLibraryManager().CreateItemFromParsed(clone.get(), 0);
			clone.reset();

			if (item == nullptr) {
				OsiError("Failed to clone item.");
				return false;
			}

			args[0].String = item->GetGuid()->Str;
			return true;
		}


		template <OsiPropertyMapType Type>
		void ItemCloneSet(OsiArgumentDesc const & args)
		{
			auto & clone = ExtensionState::Get().PendingItemClone;
			if (!clone) {
				OsiError("No item clone is currently in progress!");
				return;
			}

			OsirisPropertyMapSet(gEoCItemDefinitionPropertyMap, 
				&clone->Set.Buf[0], args, 0, Type);
		}


	}

	void CustomFunctionLibrary::RegisterItemFunctions()
	{
		auto & functionMgr = osiris_.GetCustomFunctionManager();

		auto itemGetStatsId = std::make_unique<CustomQuery>(
			"NRD_ItemGetStatsId",
			std::vector<CustomFunctionParam>{
				{ "Item", ValueType::ItemGuid, FunctionArgumentDirection::In },
				{ "StatsId", ValueType::String, FunctionArgumentDirection::Out }
			},
			&func::ItemGetStatsId
		);
		functionMgr.Register(std::move(itemGetStatsId));

		auto itemGetGenerationParams = std::make_unique<CustomQuery>(
			"NRD_ItemGetGenerationParams",
			std::vector<CustomFunctionParam>{
				{ "Item", ValueType::ItemGuid, FunctionArgumentDirection::In },
				{ "Base", ValueType::String, FunctionArgumentDirection::Out },
				{ "ItemType", ValueType::String, FunctionArgumentDirection::Out },
				{ "Level", ValueType::Integer, FunctionArgumentDirection::Out }
			},
			&func::ItemGetGenerationParams
		);
		functionMgr.Register(std::move(itemGetGenerationParams));

		auto itemHasDeltaMod = std::make_unique<CustomQuery>(
			"NRD_ItemHasDeltaModifier",
			std::vector<CustomFunctionParam>{
				{ "Item", ValueType::ItemGuid, FunctionArgumentDirection::In },
				{ "DeltaMod", ValueType::String, FunctionArgumentDirection::Out },
				{ "Count", ValueType::Integer, FunctionArgumentDirection::Out }
			},
			&func::ItemHasDeltaModifier
		);
		functionMgr.Register(std::move(itemHasDeltaMod));

		auto itemSetIdentified = std::make_unique<CustomCall>(
			"NRD_ItemSetIdentified",
			std::vector<CustomFunctionParam>{
				{ "Item", ValueType::ItemGuid, FunctionArgumentDirection::In },
				{ "IsIdentified", ValueType::Integer, FunctionArgumentDirection::In }
			},
			&func::ItemSetIdentified
		);
		functionMgr.Register(std::move(itemSetIdentified));


		auto itemGetPermanentBoostInt = std::make_unique<CustomQuery>(
			"NRD_ItemGetPermanentBoostInt",
			std::vector<CustomFunctionParam>{
				{ "Item", ValueType::ItemGuid, FunctionArgumentDirection::In },
				{ "Stat", ValueType::String, FunctionArgumentDirection::In },
				{ "Value", ValueType::Integer, FunctionArgumentDirection::Out },
			},
			&func::ItemGetPermanentBoost<OsiPropertyMapType::Integer>
		);
		functionMgr.Register(std::move(itemGetPermanentBoostInt));


		auto itemGetPermanentBoostReal = std::make_unique<CustomQuery>(
			"NRD_ItemGetPermanentBoostReal",
			std::vector<CustomFunctionParam>{
				{ "Item", ValueType::ItemGuid, FunctionArgumentDirection::In },
				{ "Stat", ValueType::String, FunctionArgumentDirection::In },
				{ "Value", ValueType::Real, FunctionArgumentDirection::Out },
			},
			&func::ItemGetPermanentBoost<OsiPropertyMapType::Real>
		);
		functionMgr.Register(std::move(itemGetPermanentBoostReal));


		auto itemSetPermanentBoostInt = std::make_unique<CustomCall>(
			"NRD_ItemSetPermanentBoostInt",
			std::vector<CustomFunctionParam>{
				{ "Item", ValueType::ItemGuid, FunctionArgumentDirection::In },
				{ "Stat", ValueType::String, FunctionArgumentDirection::In },
				{ "Value", ValueType::Integer, FunctionArgumentDirection::In },
			},
			&func::ItemSetPermanentBoost<OsiPropertyMapType::Integer>
		);
		functionMgr.Register(std::move(itemSetPermanentBoostInt));


		auto itemSetPermanentBoostReal = std::make_unique<CustomCall>(
			"NRD_ItemSetPermanentBoostReal",
			std::vector<CustomFunctionParam>{
				{ "Item", ValueType::ItemGuid, FunctionArgumentDirection::In },
				{ "Stat", ValueType::String, FunctionArgumentDirection::In },
				{ "Value", ValueType::Real, FunctionArgumentDirection::In },
			},
			&func::ItemSetPermanentBoost<OsiPropertyMapType::Real>
		);
		functionMgr.Register(std::move(itemSetPermanentBoostReal));


		auto itemCloneBegin = std::make_unique<CustomCall>(
			"NRD_ItemCloneBegin",
			std::vector<CustomFunctionParam>{
				{ "Item", ValueType::ItemGuid, FunctionArgumentDirection::In }
			},
			&func::ItemCloneBegin
		);
		functionMgr.Register(std::move(itemCloneBegin));

		auto itemClone = std::make_unique<CustomQuery>(
			"NRD_ItemClone",
			std::vector<CustomFunctionParam>{
				{ "NewItem", ValueType::ItemGuid, FunctionArgumentDirection::Out }
			},
			&func::ItemClone
		);
		functionMgr.Register(std::move(itemClone));

		auto itemCloneSetInt = std::make_unique<CustomCall>(
			"NRD_ItemCloneSetInt",
			std::vector<CustomFunctionParam>{
				{ "Property", ValueType::String, FunctionArgumentDirection::In },
				{ "Value", ValueType::Integer, FunctionArgumentDirection::In },
			},
			&func::ItemCloneSet<OsiPropertyMapType::Integer>
		);
		functionMgr.Register(std::move(itemCloneSetInt));

		auto itemCloneSetString = std::make_unique<CustomCall>(
			"NRD_ItemCloneSetString",
			std::vector<CustomFunctionParam>{
				{ "Property", ValueType::String, FunctionArgumentDirection::In },
				{ "Value", ValueType::String, FunctionArgumentDirection::In },
			},
			&func::ItemCloneSet<OsiPropertyMapType::String>
		);
		functionMgr.Register(std::move(itemCloneSetString));
	}

}

#include <libultraship/bridge/consolevariablebridge.h>

#include "option.h"
#include "randomizerEnumStrings.h"
#include "static_data.h"
#include "soh/SohGui/SohMenu.h"
#include "soh/Enhancements/Lang/Lang.h"
#include <soh/cvar_prefixes.h>
#include "soh/MergeShip/MergeShipTerminaRando.h"
#include "soh/MergeShip/MergeShipEntrances.h"

namespace SohGui {
extern std::shared_ptr<SohMenu> mSohMenu;
}

namespace Rando {
Option Option::Bool(RandomizerSettingKey key_, std::vector<std::string> options_, const OptionCategory category_,
                    std::string cvarName_, WidgetType widgetType_, const uint8_t defaultOption_,
                    const bool defaultHidden_, WidgetFunc callback_, int imFlags_) {
    return { static_cast<size_t>(key_),
             std::move(options_),
             category_,
             std::move(cvarName_),
             widgetType_,
             defaultOption_,
             defaultHidden_,
             callback_,
             imFlags_ };
}

Option Option::Bool(RandomizerSettingKey key_, std::string cvarName_, const int imFlags_, const WidgetType widgetType_,
                    const bool defaultOption_, WidgetFunc callback_) {
    return Option(key_, { "Off", "On" }, OptionCategory::Setting, std::move(cvarName_), widgetType_, defaultOption_,
                  false, callback_, imFlags_);
}

Option Option::U8(RandomizerSettingKey key_, std::vector<std::string> options_, const OptionCategory category_,
                  std::string cvarName_, WidgetType widgetType_, const uint8_t defaultOption_,
                  const bool defaultHidden_, WidgetFunc callback_, int imFlags_) {
    return { static_cast<size_t>(key_),
             std::move(options_),
             category_,
             std::move(cvarName_),
             widgetType_,
             defaultOption_,
             defaultHidden_,
             callback_,
             imFlags_ };
}

OptionValue::OptionValue(uint8_t val) : mVal(val) {
}

uint8_t OptionValue::Get() {
    return mVal;
}

void OptionValue::Set(uint8_t val) {
    mVal = val;
}

OptionValue::operator bool() const {
    return mVal != 0;
}

size_t Option::GetOptionCount() const {
    return options.size();
}

RandomizerSettingKey Option::GetKey() const {
    return static_cast<RandomizerSettingKey>(key);
}

#pragma region Lang

const static std::string namePostfix = ".name";
const static std::string descriptionPostfix = ".description";

const static std::string settingPrefix = "randomizer.settings.";
const static std::string trickPrefix = "randomizer.tricks.";

const static std::string empty = "";
const static std::string error = "[ERROR]";

// MergeShip2: built-in English names and descriptions for MergeShip's settings, used when the language data
// packed in soh.o2r predates them (the name is also the setting's key in spoiler logs, so it must never fall
// back to the shared error text).
static const std::string* MergeShipSettingText(RandomizerSettingKey key, bool description) {
    static const std::string kShuffleName = "Shuffle Transformation Masks";
    static const std::string kShuffleDesc =
        "Adds Majora's Mask's Deku, Goron, Zora and Fierce Deity's masks to the item pool as their own items; one "
        "chosen in Starting Items is given at the start instead. Each is weighted (placed and hinted) like an Ocarina "
        "of Time item: Deku Mask = Stone of Agony, Goron Mask (MM) = Megaton Hammer, Zora Mask (MM) = Iron Boots, Fierce "
        "Deity's Mask = Biggoron's Sword. While this is on, the Happy Mask Shop's Goron and Zora masks stay cosmetic.";
    static const std::string kLogicName = "Transformation Masks in Logic";
    static const std::string kLogicDesc =
        "The transformation masks are separate items: none of them stands in for an Ocarina of Time item in logic "
        "(their weights only set how they are placed and hinted). This option is reserved for future logic that "
        "counts on the forms' abilities; it currently has no effect.";
    static const std::string kBeansName = "Shuffle Deku Beans";
    static const std::string kBeansDesc =
        "Adds two Progressive Deku Beans to the item pool (needs the Mergeship \"Deku Beans\" option). The first "
        "lets Deku Link plant Deku Beans; the second makes them Gold Deku Beans, which sprout Majora's Mask's gold "
        "Deku Flower and launch him higher.";
    static const std::string kMMMasksName = "Shuffle Majora's Mask Masks";
    static const std::string kMMMasksDesc =
        "Adds Majora's Mask's other 20 masks (Mask of Truth (MM) through Giant's Mask (MM)) to the item pool, one of "
        "each, in place of junk. Logic does not need them. Each one found goes to the Masks page.";
    static const std::string kMMEquipName = "Shuffle Majora's Mask Equipment";
    static const std::string kMMEquipDesc =
        "Adds Majora's Mask's equipment to the item pool: two Progressive Sword (MM) (the Razor Sword, then the Gilded "
        "Sword), the Great Fairy's Sword, the Hero's Shield, the Mirror Shield (MM), the Hero's Bow and the Hookshot "
        "(MM). Each shares the slot of its Ocarina of Time partner (needs the Mergeship \"Majora's Mask Equipment\" "
        "option to be used as MM's). Logic counts the Hookshot (MM) as the Hookshot and the Hero's Bow as the Fairy "
        "Bow (adult only, and never as the Longshot); it needs none of the others.";
    static const std::string kMMItemsName = "Shuffle Majora's Mask Items";
    static const std::string kMMItemsDesc =
        "Adds Majora's Mask's Powder Keg, Pictograph Box, bottles (Deku Princess, Zora Egg, Gold Dust, Magic Mushroom, "
        "Seahorse, Hylian Loach, Spring Water, Hot Spring Water, Chateau Romani) and trade items (Moon's Tear, the "
        "four Title Deeds, Room Key, Letter to Mama, Letter to Kafei, Pendant of Memories) to the item pool, one of "
        "each. An MM bottle is a new bottle only while Ocarina of Time's bottles still to be found leave a slot free; "
        "otherwise it fills an empty bottle. Logic does not need them.";
    static const std::string kMMSongsName = "Shuffle Majora's Mask Songs";
    static const std::string kMMSongsDesc =
        "Adds Majora's Mask's songs (Sonata of Awakening, two Progressive Goron Lullaby -- the Intro, then the full "
        "Lullaby --, New Wave Bossa Nova, Elegy of Emptiness, Oath to Order, Song of Healing, Song of Soaring), the "
        "four remains and the Bomber's Notebook to the item pool. Logic does not need them.";
    static const std::string kStrayFairiesName = "Shuffle Stray Fairies";
    static const std::string kStrayFairiesDesc =
        "Adds Majora's Mask's 61 stray fairies to the item pool: Clock Town's one and fifteen for each of Woodfall, "
        "Snowhead, Great Bay and Stone Tower. A pot, crate or chest holding one lets the fairy out as in Majora's "
        "Mask. Logic does not need them.";
    // MergeShip2: starting masks have names only, like the other "Start with" settings. Each names its game: a name
    // is also a spoiler-log key, and Ocarina of Time's Goron and Zora masks already have theirs.
    static const std::string kStartDekuName = "Start with Deku Mask (Majora's Mask)";
    static const std::string kStartGoronName = "Start with Goron Mask (Majora's Mask)";
    static const std::string kStartZoraName = "Start with Zora Mask (Majora's Mask)";
    static const std::string kStartFierceDeityName = "Start with Fierce Deity's Mask (Majora's Mask)";
    switch (key) {
        case RSK_SHUFFLE_MM_EQUIPMENT:
            return description ? &kMMEquipDesc : &kMMEquipName;
        case RSK_SHUFFLE_MM_ITEMS:
            return description ? &kMMItemsDesc : &kMMItemsName;
        case RSK_SHUFFLE_MM_SONGS:
            return description ? &kMMSongsDesc : &kMMSongsName;
        case RSK_SHUFFLE_STRAY_FAIRIES:
            return description ? &kStrayFairiesDesc : &kStrayFairiesName;
        case RSK_SHUFFLE_MM_MASKS:
            return description ? &kMMMasksDesc : &kMMMasksName;
        case RSK_SHUFFLE_DEKU_BEANS:
            return description ? &kBeansDesc : &kBeansName;
        case RSK_SHUFFLE_TRANSFORMATION_MASKS:
            return description ? &kShuffleDesc : &kShuffleName;
        case RSK_TRANSFORMATION_MASKS_IN_LOGIC:
            return description ? &kLogicDesc : &kLogicName;
        case RSK_STARTING_MM_DEKU_MASK:
            return description ? nullptr : &kStartDekuName;
        case RSK_STARTING_MM_GORON_MASK:
            return description ? nullptr : &kStartGoronName;
        case RSK_STARTING_MM_ZORA_MASK:
            return description ? nullptr : &kStartZoraName;
        case RSK_STARTING_MM_FIERCE_DEITY_MASK:
            return description ? nullptr : &kStartFierceDeityName;
        default:
            // MergeShip2: entrance shuffle over both worlds (RSK_MERGESHIP_ER_*): MergeShipEntrances.cpp.
            if (const std::string* entrances = MergeShip_Entrances_SettingText(key, description)) {
                return entrances;
            }
            // The unified randomizer's settings (RSK_MERGESHIP_TERMINA, RSK_MM_*): MergeShipTerminaRando.cpp.
            return MergeShip_Termina_SettingText(key, description);
    }
}

static const std::string& MakeSettingName(RandomizerSettingKey key) {
    std::string settingNamePart(EnumToString(key).substr(4));
    std::transform(settingNamePart.begin(), settingNamePart.end(), settingNamePart.begin(), ::tolower);
    const std::string* mergeShip = MergeShipSettingText(key, false);
    if (mergeShip != nullptr) {
        auto result = Lang::TryTranslate((settingPrefix + settingNamePart + namePostfix).c_str());
        if (std::holds_alternative<std::reference_wrapper<const std::string>>(result)) {
            return std::get<std::reference_wrapper<const std::string>>(result);
        }
        return *mergeShip;
    }
    return Lang::Translate((settingPrefix + settingNamePart + namePostfix).c_str());
}

static const std::string& MakeSettingDescription(RandomizerSettingKey key) {
    std::string settingNamePart(EnumToString(key).substr(4));
    std::transform(settingNamePart.begin(), settingNamePart.end(), settingNamePart.begin(), ::tolower);
    auto result = Lang::TryTranslate((settingPrefix + settingNamePart + descriptionPostfix).c_str());
    if (std::holds_alternative<std::reference_wrapper<const std::string>>(result)) {
        return std::get<std::reference_wrapper<const std::string>>(result);
    } else if (std::holds_alternative<Lang::Error>(result)) {
        const std::string* mergeShip = MergeShipSettingText(key, true);
        return (mergeShip != nullptr) ? *mergeShip : empty;
    } else {
        assert(false);
        return error;
    }
}

static const std::string& MakeTrickName(RandomizerTrick key) {
    std::string trickNamePart(EnumToString(key).substr(3));
    std::transform(trickNamePart.begin(), trickNamePart.end(), trickNamePart.begin(), ::tolower);
    return Lang::Translate((trickPrefix + trickNamePart + namePostfix).c_str());
}

static const std::string& MakeTrickDescription(RandomizerTrick key) {
    std::string trickNamePart(EnumToString(key).substr(3));
    std::transform(trickNamePart.begin(), trickNamePart.end(), trickNamePart.begin(), ::tolower);
    return Lang::Translate((trickPrefix + trickNamePart + descriptionPostfix).c_str());
}

#pragma endregion

const std::string& Option::GetName() const {
    switch (this->GetCategory()) {
        case OptionCategory::Setting:
        case OptionCategory::Toggle:
            return MakeSettingName(static_cast<RandomizerSettingKey>(this->key));
        case OptionCategory::Trick:
            return MakeTrickName(static_cast<RandomizerTrick>(this->key));
        case OptionCategory::LocationExclusion:
            return StaticData::GetLocation(static_cast<RandomizerCheck>(this->key))->GetName();
        default:
            assert(false);
            return error;
    }
}

const std::string& Option::GetDescription() const {
    switch (this->GetCategory()) {
        case OptionCategory::Setting:
        case OptionCategory::Toggle:
            return MakeSettingDescription(static_cast<RandomizerSettingKey>(this->key));
        case OptionCategory::Trick:
            return MakeTrickDescription(static_cast<RandomizerTrick>(this->key));
        case OptionCategory::LocationExclusion:
            return empty;
        default:
            assert(false);
            return error;
    }
}

uint8_t Option::GetOptionIndex() const {
    return CVarGetInteger(cvarName.c_str(), defaultOption);
}

uint8_t Option::GetMenuOptionDefault() const {
    return defaultOption;
}

const std::string& Option::GetOptionText(size_t index) const {
    if (options.empty()) {
        return empty;
    }
    if (index >= options.size()) {
        index = options.size() - 1;
    }
    return options[index];
}

const std::string& Option::GetCVarName() const {
    return cvarName;
}

void Option::SetContextIndex(uint8_t idx) {
    // TODO: Set to Context's OptionValue array.
    contextSelection = idx;
    if (contextSelection > static_cast<uint8_t>(options.size() - 1)) {
        contextSelection = static_cast<uint8_t>(options.size() - 1);
    }
}

void Option::Hide() {
    hidden = true;
}

void Option::Unhide() {
    hidden = false;
}

bool Option::IsHidden() const {
    return hidden;
}

void Option::ChangeOptions(std::vector<std::string> opts) {
    if (GetOptionIndex() >= opts.size()) {
        CVarSetInteger(cvarName.c_str(), static_cast<uint8_t>(opts.size() - 1));
    }
    options = std::move(opts);
}

void Option::Enable() {
    disabled = false;
}

void Option::Disable(std::string text) {
    if (!disabled || disabledText != text) {
        disabled = true;
        disabledText = std::move(text);
    }
}

OptionCategory Option::GetCategory() const {
    return this->category;
}

void Option::AddFlag(const int imFlag_) {
    imFlags |= imFlag_;
}

void Option::RemoveFlag(const int imFlag_) {
    imFlags &= ~imFlag_;
}

uint8_t Option::GetValueFromText(const std::string text) {
    if (optionsTextToVar.contains(text)) {
        return optionsTextToVar[text];
    } else {
        SPDLOG_ERROR("Option {} does not have a var named {}.", this->GetName(), text);
        assert(false);
    }
    return defaultOption;
}

Option::Option(size_t key_, std::vector<std::string> options_, OptionCategory category_, std::string cvarName_,
               WidgetType widgetType_, uint8_t defaultOption_, bool defaultHidden_, WidgetFunc callback_, int imFlags_)
    : key(key_), options(std::move(options_)), category(category_), cvarName(std::move(cvarName_)),
      widgetType(widgetType_), defaultOption(defaultOption_), defaultHidden(defaultHidden_), imFlags(imFlags_),
      callback(callback_) {
    contextSelection = defaultOption;
    hidden = defaultHidden;
    for (size_t i = 0; i < options.size(); i++) {
        optionsMap.emplace(static_cast<int32_t>(i), options[i].c_str());
    }
    UIWidgets::LabelPositions labelPosition;
    switch (widgetType) {
        case WIDGET_CVAR_CHECKBOX:
            // labelPosition = UIWidgets::LabelPositions::Near;
            // if (imFlags_ & IMFLAG_LABEL_INLINE) {
            //     labelPosition = UIWidgets::LabelPositions::Near;
            // }
            widgetOptions = std::make_shared<UIWidgets::CheckboxOptions>(
                UIWidgets::CheckboxOptions().DefaultValue(defaultOption).Tooltip(this->GetDescription()));
            break;
        case WIDGET_CVAR_COMBOBOX:
            labelPosition = UIWidgets::LabelPositions::Above;
            if (imFlags_ & IMFLAG_LABEL_INLINE) {
                labelPosition = UIWidgets::LabelPositions::Near;
            }
            widgetOptions = std::make_shared<UIWidgets::ComboboxOptions>(UIWidgets::ComboboxOptions()
                                                                             .DefaultIndex(defaultOption)
                                                                             .ComboMap(optionsMap)
                                                                             .Tooltip(this->GetDescription())
                                                                             .LabelPosition(labelPosition));
            break;
        case WIDGET_CVAR_SLIDER_INT:
            labelPosition = UIWidgets::LabelPositions::Above;
            if (imFlags_ & IMFLAG_LABEL_INLINE) {
                labelPosition = UIWidgets::LabelPositions::Near;
            }
            widgetOptions =
                std::make_shared<UIWidgets::IntSliderOptions>(UIWidgets::IntSliderOptions()
                                                                  .DefaultValue(defaultOption)
                                                                  .Tooltip(this->GetDescription())
                                                                  .Min(0)
                                                                  .Max(static_cast<int32_t>(options.size() - 1))
                                                                  .Format(options[defaultOption].c_str())
                                                                  .LabelPosition(labelPosition));
            break;
        default:
            break;
    }
    PopulateTextToNum();
}

void Option::AddWidget(WidgetPath& path) {
    auto widget = SohGui::mSohMenu->AddWidget(path, this->GetName() + "##Randomizer", widgetType)
                      .Callback(callback)
                      .PreFunc([this](WidgetInfo& info) {
                          info.isHidden = this->IsHidden();
                          info.options->disabled = this->disabled;
                          info.options->disabledTooltip = this->disabledText.c_str();
                          info.options->tooltip = this->GetDescription();
                          if (info.type == WIDGET_CVAR_SLIDER_INT) {
                              UIWidgets::IntSliderOptions* sliderOpts =
                                  (UIWidgets::IntSliderOptions*)info.options.get();
                              size_t maxIndex = this->options.size() - 1;
                              if (this->GetKey() == RSK_SHOPSANITY_COUNT && maxIndex > 7 &&
                                  CVarGetInteger(CVAR_RANDOMIZER_SETTING("NoLogic"), RO_GENERIC_OFF) != RO_GENERIC_ON) {
                                  maxIndex = 7;
                              }
                              sliderOpts->Max(static_cast<int32_t>(maxIndex));
                              if (this->GetOptionIndex() > maxIndex) {
                                  CVarSetInteger(cvarName.c_str(), static_cast<int32_t>(maxIndex));
                              }
                              sliderOpts->Format(this->GetOptionText(this->GetOptionIndex()).c_str());
                          }
                      })
                      .CVar(cvarName.c_str())
                      .Options(widgetOptions)
                      .SameLine(imFlags & IMFLAG_SAME_LINE);
    widgetInfo = widget;
    if (callback != nullptr) {
        callback(widgetInfo);
    }
}

void Option::PopulateTextToNum() {
    for (uint8_t count = 0; count < options.size(); count++) {
        optionsTextToVar[options[count]] = count;
    }
}

void Option::SetCallback(WidgetFunc callback) {
    this->callback = callback;
}

void Option::RunCallback() {
    if (callback != nullptr) {
        this->callback(widgetInfo);
    }
}

LocationOption::LocationOption(RandomizerCheck key_)
    : Option(key_, { "Included", "Excluded" }, OptionCategory::LocationExclusion, "", WIDGET_CVAR_CHECKBOX,
             RO_LOCATION_INCLUDE, false, nullptr, IMFLAG_NONE) {
}

RandomizerCheck LocationOption::GetKey() const {
    return static_cast<RandomizerCheck>(key);
}

TrickSetting::TrickSetting(RandomizerTrick key_, const RandomizerCheckQuest quest_, const RandomizerArea area_,
                           std::set<Tricks::Tag> tags_, const std::string nameTag_)
    : Option(key_, { "Disabled", "Enabled" }, OptionCategory::Trick, "", WIDGET_CVAR_CHECKBOX, 0, false, nullptr,
             IMFLAG_NONE),
      mQuest(quest_), mArea(area_), mNameTag(nameTag_), mTags(std::move(tags_)) {
}

TrickSetting TrickSetting::LogicTrick(RandomizerTrick key_, RandomizerCheckQuest quest_, RandomizerArea area_,
                                      std::set<Tricks::Tag> tags_, const std::string nameTag_) {
    return { key_, quest_, area_, std::move(tags_), nameTag_ };
}

RandomizerTrick TrickSetting::GetKey() const {
    return static_cast<RandomizerTrick>(key);
}

RandomizerCheckQuest TrickSetting::GetQuest() const {
    return mQuest;
}

RandomizerArea TrickSetting::GetArea() const {
    return mArea;
}

std::string TrickSetting::GetNameTag() const {
    return mNameTag;
}

bool TrickSetting::HasTag(const Tricks::Tag tag) const {
    return mTags.contains(tag);
}

const std::set<Tricks::Tag>& TrickSetting::GetTags() const {
    return mTags;
}

OptionGroup::OptionGroup(std::string name, std::vector<Option*> options, const OptionGroupType groupType,
                         const WidgetContainerType containerType, std::string description)
    : mName(std::move(name)), mOptions(std::move(options)), mGroupType(groupType), mContainerType(containerType),
      mDescription(std::move(description)) {
}

OptionGroup::OptionGroup(std::string name, std::vector<OptionGroup*> subGroups, const OptionGroupType groupType,
                         const WidgetContainerType containerType, std::string description)
    : mName(std::move(name)), mSubGroups(std::move(subGroups)), mGroupType(groupType),
      mContainsType(OptionGroupType::SUBGROUP), mContainerType(containerType), mDescription(std::move(description)) {
}

OptionGroup OptionGroup::SubGroup(std::string name, std::vector<Option*> options,
                                  const WidgetContainerType containerType, std::string description) {
    return { std::move(name), std::move(options), OptionGroupType::SUBGROUP, containerType, std::move(description) };
}

OptionGroup OptionGroup::SubGroup(std::string name, std::vector<OptionGroup*> subGroups,
                                  const WidgetContainerType containerType, std::string description) {
    return { std::move(name), std::move(subGroups), OptionGroupType::SUBGROUP, containerType, std::move(description) };
}

const std::string& OptionGroup::GetName() const {
    return mName;
}

const std::vector<Option*>& OptionGroup::GetOptions() const {
    return mOptions;
}

const std::vector<OptionGroup*>& OptionGroup::GetSubGroups() const {
    return mSubGroups;
}

OptionGroupType OptionGroup::GetGroupType() const {
    return mGroupType;
}

OptionGroupType OptionGroup::GetContainsType() const {
    return mContainsType;
}

WidgetContainerType OptionGroup::GetContainerType() const {
    return mContainerType;
}

const std::string& OptionGroup::GetDescription() const {
    return mDescription;
}

void OptionGroup::Enable() {
    mDisabled = false;
}

void OptionGroup::Disable() {
    mDisabled = true;
}

void OptionGroup::AddWidgets(WidgetPath& path) const {
    if (mContainerType == WidgetContainerType::TABLE) {
        path.column = SECTION_COLUMN_1;
        path.sidebarName = mName;
        SohGui::mSohMenu->AddSidebarEntry("Randomizer", path.sidebarName, static_cast<uint32_t>(mSubGroups.size()));
    }
    if (mContainerType == WidgetContainerType::SECTION || mContainerType == WidgetContainerType::COLUMN) {
        if (!mName.empty()) {
            SohGui::mSohMenu->AddWidget(path, mName.c_str(), WIDGET_SEPARATOR_TEXT);
        }
    }
    if (mContainsType == OptionGroupType::SUBGROUP) {
        for (const auto optionGroup : mSubGroups) {
            optionGroup->AddWidgets(path);
        }
    } else {
        for (const auto option : mOptions) {
            option->AddWidget(path);
        }
    }
    if (mContainerType == WidgetContainerType::COLUMN) {
        assert(path.column < 3);
        path.column = static_cast<SectionColumns>(path.column + 1);
    }
}
} // namespace Rando
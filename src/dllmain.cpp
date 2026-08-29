#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>

#include <DynamicOutput/DynamicOutput.hpp>
#include <Mod/CppUserModBase.hpp>
#include <Unreal/Core/Containers/Array.hpp>
#include <Unreal/CoreUObject/UObject/Class.hpp>
#include <Unreal/CoreUObject/UObject/UnrealType.hpp>
#include <Unreal/FPrimaryAssetId.hpp>
#include <Unreal/NameTypes.hpp>
#include <Unreal/UObject.hpp>
#include <polyhook2/Detour/x64Detour.hpp>

#include "helmet_render_fit.hpp"

namespace
{
    using RC::Unreal::FName;
    using RC::Unreal::FPrimaryAssetId;
    using RC::Unreal::FPrimaryAssetType;
    using RC::Unreal::TArray;
    using RC::Unreal::UObject;

    static_assert(sizeof(FName) == 8, "Zero Company wardrobe mod requires an eight-byte FName ABI");
    static_assert(sizeof(FPrimaryAssetId) == 16, "Zero Company wardrobe mod requires a 16-byte FPrimaryAssetId ABI");

    constexpr std::uintptr_t kDoesPartMeetRequirementsRva = 0x63C6400;
    constexpr std::uintptr_t kFilterAssetDataByTagsRva = 0x63D1C20;
    constexpr std::uintptr_t kGameplayTagContainerCopyCtorRva = 0x40F7B20;
    constexpr std::uintptr_t kGameplayTagContainerDtorRva = 0x16C2E60;
    constexpr std::uintptr_t kGameplayTagContainerAddTagRva = 0x41017A0;
    constexpr std::uintptr_t kSolveForVoiceoverPresetRva = 0x639E1E0;
    constexpr std::uintptr_t kGetSlotInstanceRva = 0x63C3860;
    constexpr std::uintptr_t kGetPartDefinitionFromPartIdRva = 0x63C5660;
    constexpr std::uintptr_t kSlotPrimaryAssetIdOffset = 0xF0;

    constexpr std::uint32_t kExpectedPeTimestamp = 0xE10ABE56;
    constexpr std::uint32_t kExpectedImageSize = 0x0E354000;

    constexpr std::array<std::uint8_t, 16> kDoesPartMeetRequirementsBytes{
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74,
        0x24, 0x10, 0x48, 0x89, 0x7C, 0x24, 0x18, 0x4C,
    };
    constexpr std::array<std::uint8_t, 16> kFilterAssetDataByTagsBytes{
        0x48, 0x89, 0x5C, 0x24, 0x20, 0x55, 0x56, 0x57,
        0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
    };
    constexpr std::array<std::uint8_t, 16> kGameplayTagContainerCopyCtorBytes{
        0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x33, 0xC0,
        0x48, 0x8B, 0xD9, 0x48, 0x89, 0x01, 0x48, 0x89,
    };
    constexpr std::array<std::uint8_t, 16> kGameplayTagContainerDtorBytes{
        0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B,
        0xD9, 0x48, 0x8B, 0x49, 0x10, 0x48, 0x85, 0xC9,
    };
    constexpr std::array<std::uint8_t, 16> kGameplayTagContainerAddTagBytes{
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
        0xEC, 0x20, 0x48, 0x8B, 0x02, 0x48, 0x8B, 0xF9,
    };
    constexpr std::array<std::uint8_t, 16> kSolveForVoiceoverPresetBytes{
        0x4C, 0x8B, 0xDC, 0x55, 0x41, 0x54, 0x48, 0x81,
        0xEC, 0x98, 0x00, 0x00, 0x00, 0xC7, 0x01, 0xFF,
    };
    constexpr std::array<std::uint8_t, 16> kGetSlotInstanceBytes{
        0x4C, 0x8B, 0xDC, 0x48, 0x83, 0xEC, 0x48, 0x49,
        0x8D, 0x43, 0x08, 0x49, 0x89, 0x53, 0xD8, 0x49,
    };
    constexpr std::array<std::uint8_t, 16> kGetPartDefinitionFromPartIdBytes{
        0x4C, 0x8B, 0xDC, 0x53, 0x55, 0x48, 0x81, 0xEC,
        0xB8, 0x00, 0x00, 0x00, 0x48, 0x8B, 0x01, 0x33,
    };

    struct GameplayTag
    {
        FName tag_name{};
    };

    struct GameplayTagContainer
    {
        TArray<GameplayTag> gameplay_tags{};
        TArray<GameplayTag> parent_tags{};
    };

    static_assert(sizeof(GameplayTag) == 8, "Zero Company wardrobe mod requires an eight-byte FGameplayTag ABI");
    static_assert(sizeof(GameplayTagContainer) == 32, "Zero Company wardrobe mod requires a 32-byte FGameplayTagContainer ABI");

    struct TargetSpec
    {
        const RC::Unreal::TCHAR* asset_name;
        bool visible_tile;
        bool requires_cly_name;
    };

    // The twelve normal Man001/Man002 CPDs plus Cly's seven shipped modular
    // CPDs are compatible. Runtime proved the HAAF-named Cly helmet meshes can
    // resolve on masculine Hawks. This hook is gated by the exact Human species
    // tag, supplies only the exact Mdo/Cly tags that human recipients lack, and
    // leaves every other original requirements check intact. The authored-only
    // KervisNoHelm torso is deliberately absent. PACK CPDs remain valid for
    // authored torso behaviour but are never appended to a visible catalogue.
    constexpr std::array<TargetSpec, 19> kTargetSpecs{{
        {STR("CPD_H_Outfit_Man001A_TORS"), true, false},
        {STR("CPD_H_Outfit_Man001A_LEGS"), true, false},
        {STR("CPD_H_Outfit_Man001A_ARMS"), true, false},
        {STR("CPD_H_Outfit_Man001A_BOOT"), true, false},
        {STR("CPD_H_Outfit_Man001A_HELM"), true, true},
        {STR("CPD_H_Outfit_Man001A_PACK"), false, false},
        {STR("CPD_H_Outfit_Man002A_TORS"), true, false},
        {STR("CPD_H_Outfit_Man002A_LEGS"), true, false},
        {STR("CPD_H_Outfit_Man002A_ARMS"), true, false},
        {STR("CPD_H_Outfit_Man002A_BOOT"), true, false},
        {STR("CPD_H_Outfit_Man002A_HELM"), true, false},
        {STR("CPD_H_Outfit_Man002A_PACK"), false, false},
        {STR("CPD_H_Outfit_Cly_TORS"), true, true},
        {STR("CPD_H_Outfit_Cly_LEGS"), true, true},
        {STR("CPD_H_Outfit_Cly_ARMS"), true, true},
        {STR("CPD_H_Outfit_Cly_BOOT"), true, true},
        {STR("CPD_H_Outfit_Cly_HELM"), true, true},
        {STR("CPD_H_Outfit_Cly_PACK"), false, true},
        {STR("CPD_H_Outfit_ClyB_HELM"), true, true},
    }};

    struct RuntimeIdentity
    {
        std::uintptr_t base{};
        std::uintptr_t image_size{};
    };

    using DoesPartMeetRequirementsFunction = bool(__fastcall*)(const FPrimaryAssetId*, const GameplayTagContainer*);
    using FilterAssetDataByTagsFunction = void(__fastcall*)(void*, const GameplayTagContainer*, TArray<FPrimaryAssetId>*);
    using GameplayTagContainerCopyCtorFunction = GameplayTagContainer*(__fastcall*)(GameplayTagContainer*, const GameplayTagContainer*);
    using GameplayTagContainerDtorFunction = void(__fastcall*)(GameplayTagContainer*);
    using GameplayTagContainerAddTagFunction = void(__fastcall*)(GameplayTagContainer*, const GameplayTag*);
    using SolveForVoiceoverPresetFunction = void(__fastcall*)(int*, UObject*, const TArray<GameplayTag>*);
    using GetSlotInstanceFunction = UObject*(__fastcall*)(UObject*, const GameplayTag*);
    using GetPartDefinitionFromPartIdFunction = const UObject*(__fastcall*)(const FPrimaryAssetId*);

    std::array<FPrimaryAssetId, kTargetSpecs.size()> g_target_ids{};
    FPrimaryAssetId g_voice_source_id{};
    GameplayTag g_human_species_tag{};
    GameplayTag g_mdo_tag{};
    GameplayTag g_cly_name_tag{};
    RuntimeIdentity g_runtime{};

    std::uint64_t g_does_part_trampoline{};
    std::uint64_t g_filter_trampoline{};
    std::uint64_t g_voice_solver_trampoline{};
    std::unique_ptr<PLH::x64Detour> g_does_part_hook{};
    std::unique_ptr<PLH::x64Detour> g_filter_hook{};
    std::unique_ptr<PLH::x64Detour> g_voice_solver_hook{};
    bool g_hooks_active{};
    std::atomic<std::uint32_t> g_seen_compatibility_mask{};
    std::atomic<std::uint32_t> g_pending_compatibility_mask{};
    std::atomic<std::uint32_t> g_seen_catalogue_mask{};
    std::atomic<std::uint32_t> g_pending_catalogue_mask{};
    std::atomic<std::uint32_t> g_seen_voice_mask{};
    std::atomic<std::uint32_t> g_pending_voice_mask{};
    std::atomic<int> g_voice_preset{-1};
    std::atomic<bool> g_voice_preset_log_pending{};
    std::atomic<bool> g_voice_source_failure_seen{};
    std::atomic<bool> g_voice_source_failure_log_pending{};

    auto record_once(std::atomic<std::uint32_t>& seen,
                     std::atomic<std::uint32_t>& pending,
                     std::size_t index) -> void
    {
        if (index >= kTargetSpecs.size())
        {
            return;
        }

        const auto bit = std::uint32_t{1} << static_cast<std::uint32_t>(index);
        const auto previous = seen.fetch_or(bit, std::memory_order_relaxed);
        if ((previous & bit) == 0)
        {
            pending.fetch_or(bit, std::memory_order_release);
        }
    }

    auto bytes_match(std::uintptr_t base,
                     std::uintptr_t image_size,
                     std::uintptr_t rva,
                     const std::array<std::uint8_t, 16>& expected) -> bool
    {
        return rva + expected.size() <= image_size &&
               std::memcmp(reinterpret_cast<const void*>(base + rva), expected.data(), expected.size()) == 0;
    }

    auto validate_runtime(RuntimeIdentity& identity, const char*& reason) -> bool
    {
        const auto module = ::GetModuleHandleW(nullptr);
        if (module == nullptr)
        {
            reason = "main-module-not-found";
            return false;
        }

        const auto base = reinterpret_cast<std::uintptr_t>(module);
        const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
        if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
        {
            reason = "invalid-dos-header";
            return false;
        }

        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + static_cast<std::uintptr_t>(dos->e_lfanew));
        if (nt->Signature != IMAGE_NT_SIGNATURE || nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
            nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            nt->FileHeader.TimeDateStamp != kExpectedPeTimestamp ||
            nt->OptionalHeader.SizeOfImage != kExpectedImageSize)
        {
            reason = "retail-pe-identity-mismatch";
            return false;
        }

        const auto image_size = static_cast<std::uintptr_t>(nt->OptionalHeader.SizeOfImage);
        if (!bytes_match(base, image_size, kDoesPartMeetRequirementsRva, kDoesPartMeetRequirementsBytes) ||
            !bytes_match(base, image_size, kFilterAssetDataByTagsRva, kFilterAssetDataByTagsBytes) ||
            !bytes_match(base, image_size, kGameplayTagContainerCopyCtorRva, kGameplayTagContainerCopyCtorBytes) ||
            !bytes_match(base, image_size, kGameplayTagContainerDtorRva, kGameplayTagContainerDtorBytes) ||
            !bytes_match(base, image_size, kGameplayTagContainerAddTagRva, kGameplayTagContainerAddTagBytes) ||
            !bytes_match(base, image_size, kSolveForVoiceoverPresetRva, kSolveForVoiceoverPresetBytes) ||
            !bytes_match(base, image_size, kGetSlotInstanceRva, kGetSlotInstanceBytes) ||
            !bytes_match(base, image_size, kGetPartDefinitionFromPartIdRva, kGetPartDefinitionFromPartIdBytes))
        {
            reason = "pdb-target-byte-mismatch";
            return false;
        }

        identity = {base, image_size};
        return true;
    }

    auto names_equal(const FName& left, const FName& right) -> bool
    {
        return std::memcmp(&left, &right, sizeof(FName)) == 0;
    }

    auto ids_equal(const FPrimaryAssetId& left, const FPrimaryAssetId& right) -> bool
    {
        return std::memcmp(&left, &right, sizeof(FPrimaryAssetId)) == 0;
    }

    auto container_has_exact_tag(const GameplayTagContainer& container, const GameplayTag& tag) -> bool
    {
        const auto contains = [&tag](const TArray<GameplayTag>& tags) {
            for (const auto& candidate : tags)
            {
                if (names_equal(candidate.tag_name, tag.tag_name))
                {
                    return true;
                }
            }
            return false;
        };
        return contains(container.gameplay_tags) || contains(container.parent_tags);
    }

    auto target_index(const FPrimaryAssetId& id) -> std::size_t
    {
        for (std::size_t index = 0; index < g_target_ids.size(); ++index)
        {
            if (ids_equal(id, g_target_ids[index]))
            {
                return index;
            }
        }
        return g_target_ids.size();
    }

    auto is_mando_helmet_target(std::size_t index) -> bool
    {
        return index == 4 || index == 10 || index == 16 || index == 18;
    }

    auto read_slot_id(const UObject* slot) -> FPrimaryAssetId
    {
        FPrimaryAssetId id{};
        std::memcpy(&id,
                    reinterpret_cast<const void*>(reinterpret_cast<std::uintptr_t>(slot) +
                                                  kSlotPrimaryAssetIdOffset),
                    sizeof(id));
        return id;
    }

    // The shipped Mandalorian helmet definitions contain no HelmetVO fragment.
    // Resolve the value from a known stock human helmet instead of hard-coding
    // an undocumented Wwise configuration value.
    auto resolve_stock_helmet_voice_preset() -> int
    {
        const int cached = g_voice_preset.load(std::memory_order_acquire);
        if (cached >= 0)
        {
            return cached;
        }

        const auto get_part = reinterpret_cast<GetPartDefinitionFromPartIdFunction>(
            g_runtime.base + kGetPartDefinitionFromPartIdRva);
        const UObject* source_const = get_part(&g_voice_source_id);
        auto* source = const_cast<UObject*>(source_const);
        if (source == nullptr || !UObject::IsReal(source) || source->IsUnreachable())
        {
            if (!g_voice_source_failure_seen.exchange(true, std::memory_order_relaxed))
            {
                g_voice_source_failure_log_pending.store(true, std::memory_order_release);
            }
            return -1;
        }

        auto* fragments = source->GetValuePtrByPropertyNameInChain<TArray<UObject*>>(STR("Fragments"));
        if (fragments == nullptr)
        {
            if (!g_voice_source_failure_seen.exchange(true, std::memory_order_relaxed))
            {
                g_voice_source_failure_log_pending.store(true, std::memory_order_release);
            }
            return -1;
        }

        for (auto* fragment : *fragments)
        {
            if (fragment == nullptr || !UObject::IsReal(fragment) || fragment->IsUnreachable() ||
                fragment->GetClassPrivate() == nullptr ||
                fragment->GetClassPrivate()->GetFName().ToString() != STR("CustomizationFragmentHelmetVO"))
            {
                continue;
            }

            const auto* value = fragment->GetValuePtrByPropertyNameInChain<int>(STR("HelmetFxRtpcValue"));
            if (value == nullptr || *value < 0)
            {
                break;
            }

            int expected = -1;
            if (g_voice_preset.compare_exchange_strong(expected, *value, std::memory_order_release))
            {
                g_voice_preset_log_pending.store(true, std::memory_order_release);
            }
            return g_voice_preset.load(std::memory_order_acquire);
        }

        if (!g_voice_source_failure_seen.exchange(true, std::memory_order_relaxed))
        {
            g_voice_source_failure_log_pending.store(true, std::memory_order_release);
        }
        return -1;
    }

    auto array_contains(const TArray<FPrimaryAssetId>& ids, const FPrimaryAssetId& target) -> bool
    {
        for (const auto& id : ids)
        {
            if (ids_equal(id, target))
            {
                return true;
            }
        }
        return false;
    }

    auto compatible_original_check(const FPrimaryAssetId& id,
                                   const GameplayTagContainer& owned_tags,
                                   std::size_t index) -> bool
    {
        if (index >= kTargetSpecs.size() ||
            !container_has_exact_tag(owned_tags, g_human_species_tag) ||
            g_does_part_trampoline == 0)
        {
            return false;
        }

        alignas(GameplayTagContainer) std::array<std::byte, sizeof(GameplayTagContainer)> storage{};
        auto* adapted_tags = reinterpret_cast<GameplayTagContainer*>(storage.data());
        const auto copy_ctor = reinterpret_cast<GameplayTagContainerCopyCtorFunction>(
            g_runtime.base + kGameplayTagContainerCopyCtorRva);
        const auto destructor = reinterpret_cast<GameplayTagContainerDtorFunction>(
            g_runtime.base + kGameplayTagContainerDtorRva);
        const auto add_tag = reinterpret_cast<GameplayTagContainerAddTagFunction>(
            g_runtime.base + kGameplayTagContainerAddTagRva);
        const auto original = reinterpret_cast<DoesPartMeetRequirementsFunction>(g_does_part_trampoline);

        copy_ctor(adapted_tags, &owned_tags);
        add_tag(adapted_tags, &g_mdo_tag);
        if (kTargetSpecs[index].requires_cly_name)
        {
            add_tag(adapted_tags, &g_cly_name_tag);
        }
        const bool result = original(&id, adapted_tags);
        destructor(adapted_tags);
        return result;
    }

    auto hook_does_part_meet_requirements(const FPrimaryAssetId* id,
                                          const GameplayTagContainer* owned_tags) -> bool
    {
        const auto original = reinterpret_cast<DoesPartMeetRequirementsFunction>(g_does_part_trampoline);
        if (original == nullptr || id == nullptr || owned_tags == nullptr)
        {
            return false;
        }

        if (original(id, owned_tags))
        {
            return true;
        }

        const auto index = target_index(*id);
        const bool compatible = index < kTargetSpecs.size() &&
                                compatible_original_check(*id, *owned_tags, index);
        if (compatible)
        {
            record_once(g_seen_compatibility_mask, g_pending_compatibility_mask, index);
        }
        return compatible;
    }

    auto hook_filter_asset_data_by_tags(void* subsystem,
                                        const GameplayTagContainer* owned_tags,
                                        TArray<FPrimaryAssetId>* output) -> void
    {
        const auto original = reinterpret_cast<FilterAssetDataByTagsFunction>(g_filter_trampoline);
        if (original == nullptr)
        {
            return;
        }

        original(subsystem, owned_tags, output);
        if (owned_tags == nullptr || output == nullptr ||
            !container_has_exact_tag(*owned_tags, g_human_species_tag))
        {
            return;
        }

        for (std::size_t index = 0; index < kTargetSpecs.size(); ++index)
        {
            if (!kTargetSpecs[index].visible_tile || array_contains(*output, g_target_ids[index]))
            {
                continue;
            }
            if (compatible_original_check(g_target_ids[index], *owned_tags, index))
            {
                output->Add(g_target_ids[index]);
                record_once(g_seen_catalogue_mask, g_pending_catalogue_mask, index);
            }
        }
    }

    auto hook_solve_for_voiceover_preset(int* output,
                                         UObject* customization,
                                         const TArray<GameplayTag>* slot_tags) -> void
    {
        const auto original = reinterpret_cast<SolveForVoiceoverPresetFunction>(g_voice_solver_trampoline);
        if (original == nullptr)
        {
            return;
        }

        original(output, customization, slot_tags);
        if (output == nullptr || *output != -1 || customization == nullptr || slot_tags == nullptr)
        {
            return;
        }

        const auto get_slot = reinterpret_cast<GetSlotInstanceFunction>(g_runtime.base + kGetSlotInstanceRva);
        for (const auto& slot_tag : *slot_tags)
        {
            if (slot_tag.tag_name.IsNone())
            {
                continue;
            }
            UObject* slot = get_slot(customization, &slot_tag);
            if (slot == nullptr || !UObject::IsReal(slot) || slot->IsUnreachable())
            {
                continue;
            }

            const FPrimaryAssetId selected_id = read_slot_id(slot);
            const std::size_t index = target_index(selected_id);
            if (!is_mando_helmet_target(index))
            {
                continue;
            }

            const int preset = resolve_stock_helmet_voice_preset();
            if (preset >= 0)
            {
                *output = preset;
                record_once(g_seen_voice_mask, g_pending_voice_mask, index);
            }
            return;
        }
    }

    class ZeroCompanyMandoWardrobeMod final : public RC::CppUserModBase
    {
      public:
        ZeroCompanyMandoWardrobeMod()
        {
#if defined(ZERO_COMPANY_MANDO_WARDROBE_INIT_CANARY)
            ModName = STR("ZeroCompanyMandoWardrobeInitCanary");
#else
            ModName = STR("ZeroCompanyMandoWardrobe");
#endif
            ModVersion = STR("0.4.1");
            ModDescription = STR("Colourable Man001/Man002/Cly human wardrobe with a head-pivot render-palette Mandalorian helmet fit for Zero Company build 24874058");
            ModAuthors = STR("Sternab");
#if defined(ZERO_COMPANY_MANDO_WARDROBE_INIT_CANARY)
            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZeroCompanyMandoWardrobe] loaded init_canary=true hooks_pending=false mutation_capability=false\n"));
#else
            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZeroCompanyMandoWardrobe] loaded scope=exact-Man001A-Man002A-Cly-all-human helmet_fit=Man001-Man002-head-pivot-render-palette-horizontal-1.06 matrix_mutation=true scene_transform_writes=false face_visibility_mutation=false hooks_pending=true global_validation_bypass=false unequip_flags_untouched=true authored_only_excluded=true exact_ids=19 visible_candidate_ids=16 hidden_pack_ids=3 human_species_gate=true\n"));
#endif
        }

        ~ZeroCompanyMandoWardrobeMod() override
        {
            ::ZeroCompanyMandoWardrobe::HelmetRenderFit::shutdown();
            if (g_voice_solver_hook)
            {
                g_voice_solver_hook->unHook();
            }
            if (g_filter_hook)
            {
                g_filter_hook->unHook();
            }
            if (g_does_part_hook)
            {
                g_does_part_hook->unHook();
            }
            g_filter_hook.reset();
            g_does_part_hook.reset();
            g_voice_solver_hook.reset();
            g_voice_solver_trampoline = 0;
            g_hooks_active = false;
            RC::Output::send<RC::LogLevel::Verbose>(STR("[ZeroCompanyMandoWardrobe] unloaded hooks_active=false\n"));
        }

        auto on_unreal_init() -> void override
        {
            const char* reason = "ok";
            if (!validate_runtime(g_runtime, reason))
            {
                RC::Output::send<RC::LogLevel::Error>(
                    STR("[ZeroCompanyMandoWardrobe] REFUSED reason={} hooks_active=false\n"), RC::ensure_str(reason));
                return;
            }
            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZeroCompanyMandoWardrobe] init_step=runtime-identity-pass hooks_active=false\n"));

            const FName asset_type_name(STR("CustomizationPartDefinition"), RC::Unreal::FNAME_Add);
            for (std::size_t index = 0; index < kTargetSpecs.size(); ++index)
            {
                g_target_ids[index] = FPrimaryAssetId{
                    FPrimaryAssetType{asset_type_name},
                    FName(kTargetSpecs[index].asset_name, RC::Unreal::FNAME_Add),
                };
            }
            g_voice_source_id = FPrimaryAssetId{
                FPrimaryAssetType{asset_type_name},
                FName(STR("CPD_H_Outfit_Clo008_HELM_TintB"), RC::Unreal::FNAME_Add),
            };

            const FName human_species_name(STR("br.Customization.Part.Character.Species.Human"), RC::Unreal::FNAME_Find);
            const FName mdo_name(STR("br.Customization.Accepts.Outfit.Mdo"), RC::Unreal::FNAME_Find);
            const FName cly_name(STR("br.Customization.Part.Character.Info.Name.ClyKullervo"), RC::Unreal::FNAME_Find);
            g_human_species_tag = GameplayTag{human_species_name};
            g_mdo_tag = GameplayTag{mdo_name};
            g_cly_name_tag = GameplayTag{cly_name};
            if (g_human_species_tag.tag_name.IsNone() || g_mdo_tag.tag_name.IsNone() ||
                g_cly_name_tag.tag_name.IsNone())
            {
                RC::Output::send<RC::LogLevel::Error>(
                    STR("[ZeroCompanyMandoWardrobe] REFUSED reason=required-gameplay-tag-not-found hooks_active=false\n"));
                return;
            }
            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZeroCompanyMandoWardrobe] init_step=tag-fnames-found request_gameplay_tag_called=false hooks_active=false\n"));

#if defined(ZERO_COMPANY_MANDO_WARDROBE_INIT_CANARY)
            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZeroCompanyMandoWardrobe] CANARY_PASS init_only=true hooks_active=false mutation_capability=false request_gameplay_tag_called=false\n"));
            return;
#endif

            g_does_part_hook = std::make_unique<PLH::x64Detour>(
                g_runtime.base + kDoesPartMeetRequirementsRva,
                reinterpret_cast<std::uint64_t>(&hook_does_part_meet_requirements),
                &g_does_part_trampoline);
            if (!g_does_part_hook->hook() || g_does_part_trampoline == 0)
            {
                g_does_part_hook.reset();
                RC::Output::send<RC::LogLevel::Error>(
                    STR("[ZeroCompanyMandoWardrobe] REFUSED reason=requirements-hook-install-failed hooks_active=false\n"));
                return;
            }

            g_filter_hook = std::make_unique<PLH::x64Detour>(
                g_runtime.base + kFilterAssetDataByTagsRva,
                reinterpret_cast<std::uint64_t>(&hook_filter_asset_data_by_tags),
                &g_filter_trampoline);
            if (!g_filter_hook->hook() || g_filter_trampoline == 0)
            {
                g_does_part_hook->unHook();
                g_does_part_hook.reset();
                g_filter_hook.reset();
                g_does_part_trampoline = 0;
                RC::Output::send<RC::LogLevel::Error>(
                    STR("[ZeroCompanyMandoWardrobe] REFUSED reason=catalogue-hook-install-failed hooks_active=false\n"));
                return;
            }

            g_voice_solver_hook = std::make_unique<PLH::x64Detour>(
                g_runtime.base + kSolveForVoiceoverPresetRva,
                reinterpret_cast<std::uint64_t>(&hook_solve_for_voiceover_preset),
                &g_voice_solver_trampoline);
            if (!g_voice_solver_hook->hook() || g_voice_solver_trampoline == 0)
            {
                g_filter_hook->unHook();
                g_does_part_hook->unHook();
                g_voice_solver_hook.reset();
                g_filter_hook.reset();
                g_does_part_hook.reset();
                g_filter_trampoline = 0;
                g_does_part_trampoline = 0;
                RC::Output::send<RC::LogLevel::Error>(
                    STR("[ZeroCompanyMandoWardrobe] REFUSED reason=helmet-voice-hook-install-failed hooks_active=false\n"));
                return;
            }

            if (!::ZeroCompanyMandoWardrobe::HelmetRenderFit::initialize())
            {
                g_voice_solver_hook->unHook();
                g_filter_hook->unHook();
                g_does_part_hook->unHook();
                g_voice_solver_hook.reset();
                g_filter_hook.reset();
                g_does_part_hook.reset();
                g_voice_solver_trampoline = 0;
                g_filter_trampoline = 0;
                g_does_part_trampoline = 0;
                RC::Output::send<RC::LogLevel::Error>(
                    STR("[ZeroCompanyMandoWardrobe] REFUSED reason=helmet-render-fit-initialization-failed hooks_active=false\n"));
                return;
            }

            g_hooks_active = true;
            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZeroCompanyMandoWardrobe] READY hooks_active=true build=24874058 requirement_policy=human-species-gate-then-original-check-with-temporary-Mdo-and-exact-Cly-tags exact_ids=19 visible_candidate_ids=16 hidden_pack_ids=3 helmet_voice_policy=original-first-then-exact-Mando-ID-with-stock-authored-preset helmet_fit=Man001-Man002-head-pivot-render-palette-horizontal-1.06 matrix_mutation=true scene_transform_writes=false face_visibility_mutation=false KervisNoHelm=false unequip_flags_untouched=true global_validation_bypass=false\n"));
        }

        auto on_update() -> void override
        {
            ::ZeroCompanyMandoWardrobe::HelmetRenderFit::update();
            const auto compatibility_mask = g_pending_compatibility_mask.exchange(0, std::memory_order_acquire);
            const auto catalogue_mask = g_pending_catalogue_mask.exchange(0, std::memory_order_acquire);
            const auto voice_mask = g_pending_voice_mask.exchange(0, std::memory_order_acquire);
            if (g_voice_preset_log_pending.exchange(false, std::memory_order_acquire))
            {
                RC::Output::send<RC::LogLevel::Verbose>(
                    STR("[ZeroCompanyMandoWardrobe] helmet_voice_source_resolved asset=CPD_H_Outfit_Clo008_HELM_TintB fragment=CustomizationFragmentHelmetVO preset={}\n"),
                    g_voice_preset.load(std::memory_order_acquire));
            }
            if (g_voice_source_failure_log_pending.exchange(false, std::memory_order_acquire))
            {
                RC::Output::send<RC::LogLevel::Warning>(
                    STR("[ZeroCompanyMandoWardrobe] helmet_voice_source_pending asset=CPD_H_Outfit_Clo008_HELM_TintB behavior=leave-original-result-unchanged retry=true\n"));
            }
            for (std::size_t index = 0; index < kTargetSpecs.size(); ++index)
            {
                const auto bit = std::uint32_t{1} << static_cast<std::uint32_t>(index);
                if ((compatibility_mask & bit) != 0)
                {
                    RC::Output::send<RC::LogLevel::Verbose>(
                        STR("[ZeroCompanyMandoWardrobe] compatibility_applied asset={} policy=adapt-tags-then-original-check\n"),
                        kTargetSpecs[index].asset_name);
                }
                if ((catalogue_mask & bit) != 0)
                {
                    RC::Output::send<RC::LogLevel::Verbose>(
                        STR("[ZeroCompanyMandoWardrobe] catalogue_tile_added asset={} ordinary_view_model=true\n"),
                        kTargetSpecs[index].asset_name);
                }
                if ((voice_mask & bit) != 0)
                {
                    RC::Output::send<RC::LogLevel::Verbose>(
                        STR("[ZeroCompanyMandoWardrobe] helmet_voice_compatibility_applied asset={} preset={} original_solver_result=none exact_id=true\n"),
                        kTargetSpecs[index].asset_name,
                        g_voice_preset.load(std::memory_order_acquire));
                }
            }
        }
    };
}

#define ZERO_COMPANY_MANDO_WARDROBE_API __declspec(dllexport)

extern "C"
{
    ZERO_COMPANY_MANDO_WARDROBE_API RC::CppUserModBase* start_mod()
    {
        return new ZeroCompanyMandoWardrobeMod();
    }

    ZERO_COMPANY_MANDO_WARDROBE_API void uninstall_mod(RC::CppUserModBase* mod)
    {
        delete mod;
    }
}

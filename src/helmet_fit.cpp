#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <utility>
#include <vector>

#include <DynamicOutput/DynamicOutput.hpp>
#include <Mod/CppUserModBase.hpp>
#include <Unreal/FPrimaryAssetId.hpp>
#include <Unreal/FWeakObjectPtr.hpp>
#include <Unreal/NameTypes.hpp>
#include <Unreal/UObject.hpp>
#include <Unreal/UObjectGlobals.hpp>
#include <polyhook2/Detour/x64Detour.hpp>

#include "helmet_fit.hpp"

namespace
{
    using RC::Unreal::FName;
    using RC::Unreal::FPrimaryAssetId;
    using RC::Unreal::FPrimaryAssetType;
    using RC::Unreal::FWeakObjectPtr;
    using RC::Unreal::UObject;

    static_assert(sizeof(FName) == 8, "Zero Company helmet-fit canary requires an eight-byte FName ABI");
    static_assert(sizeof(FPrimaryAssetId) == 16,
                  "Zero Company helmet-fit canary requires a 16-byte FPrimaryAssetId ABI");
    static_assert(sizeof(FWeakObjectPtr) == 8,
                  "Zero Company helmet-fit canary requires an eight-byte FWeakObjectPtr ABI");

    // Exact retail build 24874058 symbols, resolved from the shipped PDB.
    constexpr std::uintptr_t kBroadcastRefreshedRva = 0x63C36A0;
    constexpr std::uintptr_t kGetSlotInstanceRva = 0x63C3860;
    constexpr std::uintptr_t kGetSkeletalMeshAssetRva = 0x4688A00;
    constexpr std::uintptr_t kGetRelativeTransformRva = 0x466FA40;
    constexpr std::uintptr_t kSetRelativeTransformRva = 0x466F9B0;
    constexpr std::uintptr_t kGetNumBonesRva = 0x469D920;
    constexpr std::uintptr_t kGetBoneNameRva = 0x469DAD0;
    constexpr std::uintptr_t kGetBoneTransformRva = 0x469A3D0;
    constexpr std::uintptr_t kShouldRenderRva = 0x4676590;
    constexpr std::uintptr_t kSlotPrimaryAssetIdOffset = 0xF0;
    constexpr std::uint32_t kExpectedPeTimestamp = 0xE10ABE56;
    constexpr std::uint32_t kExpectedImageSize = 0x0E354000;
    constexpr std::size_t kMaxSelectionStates = 64;
    constexpr std::size_t kMaxTargetHelmetComponents = 128;
    constexpr std::uint8_t kMaximumScanAttempts = 4;
    // UE character/skeletal convention uses X/Y for forward/lateral extent and
    // Z for height. Preserve authored height so the attachment-origin bias does
    // not pull the lower helmet opening away from the chin.
    constexpr double kHelmetFitScaleX = 1.06;
    constexpr double kHelmetFitScaleY = 1.06;
    constexpr double kHelmetFitScaleZ = 1.00;
    // Both shipped masculine Mandalorian helmet meshes report 632 bones in the
    // build-24874058 asset registry. Keep the traversal bounded, but do not
    // reject their full MetaHuman-compatible skeleton before examining it.
    constexpr std::int32_t kMaximumBoneCount = 768;
    constexpr std::uint32_t kPivotSummaryIntervalFrames = 120;

    constexpr std::array<std::uint8_t, 16> kBroadcastRefreshedBytes{
        0x40, 0x56, 0x48, 0x83, 0xEC, 0x30, 0x80, 0xB9,
        0xC1, 0x02, 0x00, 0x00, 0x00, 0x48, 0x8B, 0xF1,
    };
    constexpr std::array<std::uint8_t, 16> kGetSlotInstanceBytes{
        0x4C, 0x8B, 0xDC, 0x48, 0x83, 0xEC, 0x48, 0x49,
        0x8D, 0x43, 0x08, 0x49, 0x89, 0x53, 0xD8, 0x49,
    };
    constexpr std::array<std::uint8_t, 16> kGetSkeletalMeshAssetBytes{
        0x40, 0x53, 0x48, 0x83, 0xEC, 0x20, 0x48, 0x8B,
        0x99, 0xD8, 0x05, 0x00, 0x00, 0x48, 0x85, 0xDB,
    };
    constexpr std::array<std::uint8_t, 16> kGetRelativeTransformBytes{
        0x48, 0x8B, 0xC4, 0x48, 0x89, 0x58, 0x18, 0x57,
        0x48, 0x81, 0xEC, 0xF0, 0x00, 0x00, 0x00, 0x0F,
    };
    constexpr std::array<std::uint8_t, 16> kSetRelativeTransformBytes{
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x57, 0x48, 0x83,
        0xEC, 0x70, 0x0F, 0x10, 0x0A, 0x48, 0x8B, 0xDA,
    };
    constexpr std::array<std::uint8_t, 16> kGetNumBonesBytes{
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74,
        0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x0F,
    };
    constexpr std::array<std::uint8_t, 16> kGetBoneNameBytes{
        0x48, 0x89, 0x5C, 0x24, 0x10, 0x48, 0x89, 0x6C,
        0x24, 0x18, 0x56, 0x57, 0x41, 0x56, 0x48, 0x83,
    };
    constexpr std::array<std::uint8_t, 16> kGetBoneTransformBytes{
        0x48, 0x89, 0x5C, 0x24, 0x20, 0x55, 0x56, 0x57,
        0x41, 0x54, 0x41, 0x55, 0x41, 0x56, 0x41, 0x57,
    };
    constexpr std::array<std::uint8_t, 16> kShouldRenderBytes{
        0x48, 0x89, 0x5C, 0x24, 0x08, 0x48, 0x89, 0x74,
        0x24, 0x10, 0x57, 0x48, 0x83, 0xEC, 0x20, 0x48,
    };

    struct GameplayTag
    {
        FName tag_name{};
    };

    struct Vector3d
    {
        double x{};
        double y{};
        double z{};
    };
    static_assert(sizeof(Vector3d) == 24,
                  "Zero Company helmet-fit canary requires a 24-byte UE5 FVector ABI");

    struct Quaternion4d
    {
        double x{};
        double y{};
        double z{};
        double w{1.0};
    };
    static_assert(sizeof(Quaternion4d) == 32,
                  "Zero Company helmet-fit canary requires a 32-byte UE5 FQuat ABI");

    struct alignas(16) Transform3d
    {
        Quaternion4d rotation{};
        alignas(16) Vector3d translation{};
        alignas(16) Vector3d scale{1.0, 1.0, 1.0};
    };
    static_assert(sizeof(Transform3d) == 96,
                  "Zero Company helmet-fit canary requires a 96-byte UE5 FTransform ABI");
    static_assert(offsetof(Transform3d, rotation) == 0 &&
                      offsetof(Transform3d, translation) == 32 &&
                      offsetof(Transform3d, scale) == 64,
                  "Zero Company helmet-fit canary requires the UE5 double FTransform field layout");

    struct RuntimeIdentity
    {
        std::uintptr_t base{};
        std::uintptr_t image_size{};
    };

    struct SelectionFingerprint
    {
        FPrimaryAssetId helmet{};
        bool target{};
    };

    struct SelectionState
    {
        std::uintptr_t customization{};
        SelectionFingerprint fingerprint{};
    };

    struct ScaledHelmet
    {
        FWeakObjectPtr weak_component{};
        std::uintptr_t component{};
        Transform3d original{};
        Transform3d fitted{};
        std::int32_t head_bone_index{-1};
        FName head_bone_name{};
        bool restore_pending{};
    };

    struct ScanResult
    {
        std::size_t component_instances{};
        std::size_t exact_target_helmets{};
        std::size_t newly_scaled{};
        std::size_t reapplied{};
        std::size_t already_scaled{};
        std::size_t restored{};
        std::size_t stale_pruned{};
        std::size_t head_bones_resolved{};
        std::size_t head_bones_missing{};
        std::size_t component_refusals{};
        std::size_t rollback_restored{};
        std::size_t active_scaled{};
        bool bound_refused{};
    };

    using BroadcastRefreshedFunction = void(__fastcall*)(UObject*);
    using GetSlotInstanceFunction = UObject*(__fastcall*)(UObject*, const GameplayTag*);
    using GetSkeletalMeshAssetFunction = UObject*(__fastcall*)(UObject*);
    // These are C++ member functions returning a 96-byte FTransform. On Win64
    // the shipped functions receive `this` in RCX and the hidden return buffer
    // in RDX. Express the hidden buffer explicitly; a free-function pointer
    // returning Transform3d would reverse those two registers.
    using GetRelativeTransformFunction = void(__fastcall*)(const UObject*, Transform3d*);
    using SetRelativeTransformFunction = void(__fastcall*)(UObject*, const Transform3d*, bool, void*, std::uint8_t);
    using GetNumBonesFunction = std::int32_t(__fastcall*)(const UObject*);
    // FName is eight bytes, but the shipped non-trivial return ABI still uses
    // an explicit hidden output buffer: component=RCX, output=RDX, index=R8D.
    using GetBoneNameFunction = void(__fastcall*)(const UObject*, FName*, std::int32_t);
    using GetBoneTransformFunction = void(__fastcall*)(const UObject*, Transform3d*, std::int32_t, const Transform3d*);
    using ShouldRenderFunction = bool(__fastcall*)(const UObject*);

    RuntimeIdentity g_runtime{};
    GameplayTag g_helmet_slot_tag{};
    std::array<FPrimaryAssetId, 2> g_target_helmet_ids{};

    std::uint64_t g_broadcast_refreshed_trampoline{};
    std::unique_ptr<PLH::x64Detour> g_broadcast_refreshed_hook{};

    std::array<SelectionState, kMaxSelectionStates> g_selection_states{};
    std::mutex g_selection_mutex{};
    std::vector<ScaledHelmet> g_scaled_helmets{};
    std::mutex g_scaled_helmets_mutex{};

    std::atomic<bool> g_scan_pending{};
    std::atomic<std::uint8_t> g_frames_until_scan{};
    std::atomic<std::uint8_t> g_scan_attempt{};
    std::atomic<std::uint32_t> g_pending_trigger_logs{};
    std::atomic<std::uint32_t> g_pending_pivot_updates{};
    std::atomic<std::uint32_t> g_pending_pivot_failures{};
    std::uint32_t g_pivot_summary_frames{};
    bool g_initialized{};

    auto bytes_match(std::uintptr_t base,
                     std::uintptr_t image_size,
                     std::uintptr_t rva,
                     const std::array<std::uint8_t, 16>& expected) -> bool
    {
        return rva + expected.size() <= image_size &&
               std::memcmp(reinterpret_cast<const void*>(base + rva),
                           expected.data(),
                           expected.size()) == 0;
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

        const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
            base + static_cast<std::uintptr_t>(dos->e_lfanew));
        if (nt->Signature != IMAGE_NT_SIGNATURE ||
            nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
            nt->FileHeader.Machine != IMAGE_FILE_MACHINE_AMD64 ||
            nt->FileHeader.TimeDateStamp != kExpectedPeTimestamp ||
            nt->OptionalHeader.SizeOfImage != kExpectedImageSize)
        {
            reason = "retail-pe-identity-mismatch";
            return false;
        }

        const auto image_size = static_cast<std::uintptr_t>(nt->OptionalHeader.SizeOfImage);
        if (!bytes_match(base, image_size, kBroadcastRefreshedRva, kBroadcastRefreshedBytes) ||
            !bytes_match(base, image_size, kGetSlotInstanceRva, kGetSlotInstanceBytes) ||
            !bytes_match(base, image_size, kGetSkeletalMeshAssetRva, kGetSkeletalMeshAssetBytes) ||
            !bytes_match(base, image_size, kGetRelativeTransformRva, kGetRelativeTransformBytes) ||
            !bytes_match(base, image_size, kSetRelativeTransformRva, kSetRelativeTransformBytes) ||
            !bytes_match(base, image_size, kGetNumBonesRva, kGetNumBonesBytes) ||
            !bytes_match(base, image_size, kGetBoneNameRva, kGetBoneNameBytes) ||
            !bytes_match(base, image_size, kGetBoneTransformRva, kGetBoneTransformBytes) ||
            !bytes_match(base, image_size, kShouldRenderRva, kShouldRenderBytes))
        {
            reason = "pdb-target-byte-mismatch";
            return false;
        }

        identity = RuntimeIdentity{base, image_size};
        return true;
    }

    auto contains_fragment(const RC::StringType& value,
                           const RC::Unreal::TCHAR* fragment) -> bool
    {
        return value.find(fragment) != RC::StringType::npos;
    }

    auto ids_equal(const FPrimaryAssetId& left, const FPrimaryAssetId& right) -> bool
    {
        return std::memcmp(&left, &right, sizeof(FPrimaryAssetId)) == 0;
    }

    auto read_slot_id(const UObject* slot) -> FPrimaryAssetId
    {
        FPrimaryAssetId id{};
        if (slot != nullptr)
        {
            std::memcpy(&id,
                        reinterpret_cast<const void*>(reinterpret_cast<std::uintptr_t>(slot) +
                                                      kSlotPrimaryAssetIdOffset),
                        sizeof(id));
        }
        return id;
    }

    auto read_selected_helmet_id(UObject* customization) -> FPrimaryAssetId
    {
        if (customization == nullptr || g_runtime.base == 0)
        {
            return {};
        }
        const auto get_slot = reinterpret_cast<GetSlotInstanceFunction>(
            g_runtime.base + kGetSlotInstanceRva);
        return read_slot_id(get_slot(customization, &g_helmet_slot_tag));
    }

    auto is_target_helmet_id(const FPrimaryAssetId& id) -> bool
    {
        return ids_equal(id, g_target_helmet_ids[0]) || ids_equal(id, g_target_helmet_ids[1]);
    }

    auto read_fingerprint(UObject* customization) -> SelectionFingerprint
    {
        SelectionFingerprint result{};
        result.helmet = read_selected_helmet_id(customization);
        result.target = is_target_helmet_id(result.helmet);
        return result;
    }

    auto previous_fingerprint(UObject* customization,
                              const SelectionFingerprint& current,
                              bool update) -> std::pair<bool, SelectionFingerprint>
    {
        std::scoped_lock lock(g_selection_mutex);
        const auto address = reinterpret_cast<std::uintptr_t>(customization);
        SelectionState* empty{};
        for (auto& state : g_selection_states)
        {
            if (state.customization == 0)
            {
                if (empty == nullptr)
                {
                    empty = &state;
                }
                continue;
            }
            if (state.customization == address)
            {
                const auto previous = state.fingerprint;
                if (update)
                {
                    state.fingerprint = current;
                }
                return {true, previous};
            }
        }
        if (update && empty != nullptr)
        {
            *empty = SelectionState{address, current};
        }
        else if (update)
        {
            const auto index = (address >> 4) % g_selection_states.size();
            g_selection_states[index] = SelectionState{address, current};
        }
        return {false, {}};
    }

    auto finite_vector(const Vector3d& value) -> bool
    {
        return std::isfinite(value.x) && std::isfinite(value.y) && std::isfinite(value.z);
    }

    auto scale_sane(const Vector3d& value) -> bool
    {
        return finite_vector(value) &&
               std::abs(value.x) >= 0.05 && std::abs(value.x) <= 20.0 &&
               std::abs(value.y) >= 0.05 && std::abs(value.y) <= 20.0 &&
               std::abs(value.z) >= 0.05 && std::abs(value.z) <= 20.0;
    }

    auto quaternion_sane(const Quaternion4d& value) -> bool
    {
        if (!std::isfinite(value.x) || !std::isfinite(value.y) ||
            !std::isfinite(value.z) || !std::isfinite(value.w))
        {
            return false;
        }
        const double norm_squared = value.x * value.x + value.y * value.y +
                                    value.z * value.z + value.w * value.w;
        return norm_squared >= 0.90 && norm_squared <= 1.10;
    }

    auto transform_sane(const Transform3d& value) -> bool
    {
        constexpr double maximum_translation = 1000000.0;
        return quaternion_sane(value.rotation) && finite_vector(value.translation) &&
               std::abs(value.translation.x) <= maximum_translation &&
               std::abs(value.translation.y) <= maximum_translation &&
               std::abs(value.translation.z) <= maximum_translation &&
               scale_sane(value.scale);
    }

    auto pivot_sane(const Vector3d& value) -> bool
    {
        constexpr double maximum_pivot_extent = 100000.0;
        return finite_vector(value) &&
               std::abs(value.x) <= maximum_pivot_extent &&
               std::abs(value.y) <= maximum_pivot_extent &&
               std::abs(value.z) <= maximum_pivot_extent;
    }

    auto vectors_near(const Vector3d& left,
                      const Vector3d& right,
                      double tolerance = 0.0001) -> bool
    {
        return std::abs(left.x - right.x) <= tolerance &&
               std::abs(left.y - right.y) <= tolerance &&
               std::abs(left.z - right.z) <= tolerance;
    }

    auto quaternions_near(const Quaternion4d& left,
                          const Quaternion4d& right,
                          double tolerance = 0.000001) -> bool
    {
        const bool direct = std::abs(left.x - right.x) <= tolerance &&
                            std::abs(left.y - right.y) <= tolerance &&
                            std::abs(left.z - right.z) <= tolerance &&
                            std::abs(left.w - right.w) <= tolerance;
        const bool negated = std::abs(left.x + right.x) <= tolerance &&
                             std::abs(left.y + right.y) <= tolerance &&
                             std::abs(left.z + right.z) <= tolerance &&
                             std::abs(left.w + right.w) <= tolerance;
        return direct || negated;
    }

    auto transforms_near(const Transform3d& left,
                         const Transform3d& right,
                         double translation_tolerance = 0.0005) -> bool
    {
        return quaternions_near(left.rotation, right.rotation) &&
               vectors_near(left.translation, right.translation, translation_tolerance) &&
               vectors_near(left.scale, right.scale);
    }

    auto cross(const Vector3d& left, const Vector3d& right) -> Vector3d
    {
        return Vector3d{
            left.y * right.z - left.z * right.y,
            left.z * right.x - left.x * right.z,
            left.x * right.y - left.y * right.x,
        };
    }

    auto rotate_vector(const Quaternion4d& rotation, const Vector3d& value) -> Vector3d
    {
        const Vector3d quaternion_vector{rotation.x, rotation.y, rotation.z};
        const Vector3d first_cross = cross(quaternion_vector, value);
        const Vector3d doubled_cross{
            first_cross.x * 2.0,
            first_cross.y * 2.0,
            first_cross.z * 2.0,
        };
        const Vector3d second_cross = cross(quaternion_vector, doubled_cross);
        return Vector3d{
            value.x + rotation.w * doubled_cross.x + second_cross.x,
            value.y + rotation.w * doubled_cross.y + second_cross.y,
            value.z + rotation.w * doubled_cross.z + second_cross.z,
        };
    }

    auto read_relative_transform_known_live(const UObject* component) -> Transform3d
    {
        const auto getter = reinterpret_cast<GetRelativeTransformFunction>(
            g_runtime.base + kGetRelativeTransformRva);
        Transform3d result{};
        getter(component, &result);
        return result;
    }

    auto set_relative_transform_known_live(UObject* component,
                                           const Transform3d& value) -> bool
    {
        const auto setter = reinterpret_cast<SetRelativeTransformFunction>(
            g_runtime.base + kSetRelativeTransformRva);
        setter(component, &value, false, nullptr, 0);
        return transforms_near(read_relative_transform_known_live(component), value);
    }

    auto ascii_lower(RC::Unreal::TCHAR value) -> RC::Unreal::TCHAR
    {
        return value >= static_cast<RC::Unreal::TCHAR>('A') &&
                       value <= static_cast<RC::Unreal::TCHAR>('Z')
                   ? static_cast<RC::Unreal::TCHAR>(value +
                                                    (static_cast<RC::Unreal::TCHAR>('a') -
                                                     static_cast<RC::Unreal::TCHAR>('A')))
                   : value;
    }

    auto equals_ascii_case_insensitive(const RC::StringType& value,
                                       const RC::Unreal::TCHAR* expected) -> bool
    {
        if (expected == nullptr)
        {
            return false;
        }
        std::size_t length{};
        while (expected[length] != 0)
        {
            ++length;
        }
        if (value.size() != length)
        {
            return false;
        }
        for (std::size_t index = 0; index < length; ++index)
        {
            if (ascii_lower(value[index]) != ascii_lower(expected[index]))
            {
                return false;
            }
        }
        return true;
    }

    auto head_bone_name_score(const RC::StringType& value) -> int
    {
        if (equals_ascii_case_insensitive(value, STR("head")))
        {
            return 0;
        }
        constexpr std::array<const RC::Unreal::TCHAR*, 6> authored_candidates{
            STR("c_head"), STR("b_head"), STR("head_jnt"),
            STR("head_bind"), STR("j_head"), STR("head_bone"),
        };
        for (const auto* candidate : authored_candidates)
        {
            if (equals_ascii_case_insensitive(value, candidate))
            {
                return 1;
            }
        }
        return 100;
    }

    struct HeadBoneResolution
    {
        std::int32_t index{-1};
        FName name{};
    };

    auto resolve_head_bone_known_live(UObject* component) -> HeadBoneResolution
    {
        const auto get_num_bones = reinterpret_cast<GetNumBonesFunction>(
            g_runtime.base + kGetNumBonesRva);
        const auto get_bone_name = reinterpret_cast<GetBoneNameFunction>(
            g_runtime.base + kGetBoneNameRva);
        const std::int32_t bone_count = get_num_bones(component);
        if (bone_count <= 0 || bone_count > kMaximumBoneCount)
        {
            return {};
        }

        HeadBoneResolution best{};
        int best_score = 100;
        bool ambiguous{};
        for (std::int32_t index = 0; index < bone_count; ++index)
        {
            FName name{};
            get_bone_name(component, &name, index);
            if (name.IsNone())
            {
                continue;
            }
            const int score = head_bone_name_score(name.ToString());
            if (score < best_score)
            {
                best = HeadBoneResolution{index, name};
                best_score = score;
                ambiguous = false;
            }
            else if (score == best_score && score < 100)
            {
                ambiguous = true;
            }
        }
        return best_score < 100 && !ambiguous ? best : HeadBoneResolution{};
    }

    auto read_bone_transform_component_space_known_live(const UObject* component,
                                                         std::int32_t bone_index) -> Transform3d
    {
        const auto get_bone_transform = reinterpret_cast<GetBoneTransformFunction>(
            g_runtime.base + kGetBoneTransformRva);
        const Transform3d identity{};
        Transform3d result{};
        get_bone_transform(component, &result, bone_index, &identity);
        return result;
    }

    auto make_head_pivot_compensated_transform(const Transform3d& original,
                                                const Vector3d& head_pivot) -> Transform3d
    {
        Transform3d fitted = original;
        fitted.scale = Vector3d{
            original.scale.x * kHelmetFitScaleX,
            original.scale.y * kHelmetFitScaleY,
            original.scale.z * kHelmetFitScaleZ,
        };

        // Preserve the animated head-bone pivot in parent space:
        // T1 + R(S1*P) == T0 + R(S0*P).
        const Vector3d local_compensation{
            (original.scale.x - fitted.scale.x) * head_pivot.x,
            (original.scale.y - fitted.scale.y) * head_pivot.y,
            (original.scale.z - fitted.scale.z) * head_pivot.z,
        };
        const Vector3d parent_compensation = rotate_vector(original.rotation, local_compensation);
        fitted.translation = Vector3d{
            original.translation.x + parent_compensation.x,
            original.translation.y + parent_compensation.y,
            original.translation.z + parent_compensation.z,
        };
        return fitted;
    }

    auto is_target_helmet_component_known_live(UObject* component) -> bool
    {
        const auto get_mesh = reinterpret_cast<GetSkeletalMeshAssetFunction>(
            g_runtime.base + kGetSkeletalMeshAssetRva);
        UObject* mesh = get_mesh(component);
        if (mesh == nullptr || mesh->IsUnreachable())
        {
            return false;
        }
        const auto mesh_name = mesh->GetFullName();
        return contains_fragment(mesh_name, STR("Man001A_HELM")) ||
               contains_fragment(mesh_name, STR("Man002A_HELM"));
    }

    auto find_component(const std::vector<UObject*>& components,
                        std::uintptr_t address) -> UObject*
    {
        const auto wanted = reinterpret_cast<UObject*>(address);
        const auto it = std::find(components.begin(), components.end(), wanted);
        return it == components.end() ? nullptr : *it;
    }

    auto find_scaled(std::uintptr_t address) -> std::vector<ScaledHelmet>::iterator
    {
        return std::find_if(g_scaled_helmets.begin(),
                            g_scaled_helmets.end(),
                            [address](const ScaledHelmet& entry) {
                                return entry.component == address;
                            });
    }

    auto restore_all_known_live(const std::vector<UObject*>& components) -> std::size_t
    {
        std::size_t restored{};
        std::scoped_lock lock(g_scaled_helmets_mutex);
        for (const auto& entry : g_scaled_helmets)
        {
            UObject* component = find_component(components, entry.component);
            if (component != nullptr && !component->IsUnreachable() &&
                set_relative_transform_known_live(component, entry.original))
            {
                ++restored;
            }
        }
        g_scaled_helmets.clear();
        return restored;
    }

    auto collect_and_fit_target_helmets() -> ScanResult
    {
        ScanResult result{};
        std::vector<UObject*> components{};
        RC::Unreal::UObjectGlobals::FindAllOf(STR("SkeletalMeshComponent"), components);
        result.component_instances = components.size();

        std::vector<UObject*> targets{};
        targets.reserve(32);
        for (UObject* component : components)
        {
            if (component == nullptr || component->IsUnreachable())
            {
                continue;
            }
            if (is_target_helmet_component_known_live(component))
            {
                targets.push_back(component);
            }
        }
        result.exact_target_helmets = targets.size();
        if (targets.size() > kMaxTargetHelmetComponents)
        {
            result.bound_refused = true;
            result.rollback_restored = restore_all_known_live(components);
            return result;
        }

        std::scoped_lock lock(g_scaled_helmets_mutex);

        // Prune dead addresses without dereferencing them. If the game reused a
        // still-live component for a stock mesh, restore that component's exact
        // authored transform. A transient restore failure remains tracked and is
        // retried independently; it must not roll back unrelated live helmets.
        for (auto it = g_scaled_helmets.begin(); it != g_scaled_helmets.end();)
        {
            UObject* live = find_component(components, it->component);
            if (live == nullptr || live->IsUnreachable())
            {
                it = g_scaled_helmets.erase(it);
                ++result.stale_pruned;
                continue;
            }
            if (std::find(targets.begin(), targets.end(), live) == targets.end())
            {
                if (!set_relative_transform_known_live(live, it->original))
                {
                    it->restore_pending = true;
                    ++result.component_refusals;
                    ++it;
                    continue;
                }
                it = g_scaled_helmets.erase(it);
                ++result.restored;
                continue;
            }
            it->restore_pending = false;
            ++it;
        }

        for (UObject* target : targets)
        {
            const auto address = reinterpret_cast<std::uintptr_t>(target);
            auto tracked = find_scaled(address);
            if (tracked == g_scaled_helmets.end())
            {
                const Transform3d original = read_relative_transform_known_live(target);
                if (!transform_sane(original))
                {
                    ++result.component_refusals;
                    continue;
                }
                const HeadBoneResolution head_bone = resolve_head_bone_known_live(target);
                if (head_bone.index < 0)
                {
                    ++result.head_bones_missing;
                    continue;
                }
                const Transform3d head_transform =
                    read_bone_transform_component_space_known_live(target, head_bone.index);
                if (!transform_sane(head_transform) || !pivot_sane(head_transform.translation))
                {
                    ++result.head_bones_missing;
                    continue;
                }
                const Transform3d fitted =
                    make_head_pivot_compensated_transform(original, head_transform.translation);
                if (!transform_sane(fitted))
                {
                    ++result.component_refusals;
                    continue;
                }
                if (!set_relative_transform_known_live(target, fitted))
                {
                    // The setter can race a preview component still being
                    // constructed. Restore this component only; if even that
                    // readback is transient, retain it as a bounded retry.
                    const bool restored = set_relative_transform_known_live(target, original);
                    ++result.component_refusals;
                    if (!restored)
                    {
                        g_scaled_helmets.push_back(ScaledHelmet{
                            FWeakObjectPtr{target},
                            address,
                            original,
                            fitted,
                            head_bone.index,
                            head_bone.name,
                            true,
                        });
                    }
                    continue;
                }
                g_scaled_helmets.push_back(ScaledHelmet{
                    FWeakObjectPtr{target},
                    address,
                    original,
                    fitted,
                    head_bone.index,
                    head_bone.name,
                    false,
                });
                ++result.head_bones_resolved;
                ++result.newly_scaled;
                continue;
            }

            tracked->restore_pending = false;
            const Transform3d head_transform =
                read_bone_transform_component_space_known_live(target,
                                                               tracked->head_bone_index);
            if (!transform_sane(head_transform) || !pivot_sane(head_transform.translation))
            {
                ++result.head_bones_missing;
                continue;
            }
            tracked->fitted = make_head_pivot_compensated_transform(
                tracked->original, head_transform.translation);
            const Transform3d current = read_relative_transform_known_live(target);
            if (transforms_near(current, tracked->fitted))
            {
                ++result.already_scaled;
                continue;
            }
            if (!transform_sane(current) ||
                !set_relative_transform_known_live(target, tracked->fitted))
            {
                ++result.component_refusals;
                continue;
            }
            ++result.reapplied;
        }
        result.active_scaled = g_scaled_helmets.size();
        return result;
    }

    auto restore_all_live_components() -> std::size_t
    {
        if (g_runtime.base == 0)
        {
            return 0;
        }
        std::vector<UObject*> components{};
        RC::Unreal::UObjectGlobals::FindAllOf(STR("SkeletalMeshComponent"), components);
        return restore_all_known_live(components);
    }

    auto update_visible_head_pivot_compensation() -> void
    {
        if (g_runtime.base == 0)
        {
            return;
        }
        const auto should_render = reinterpret_cast<ShouldRenderFunction>(
            g_runtime.base + kShouldRenderRva);

        std::uint32_t updates{};
        std::uint32_t failures{};
        std::scoped_lock lock(g_scaled_helmets_mutex);
        for (auto it = g_scaled_helmets.begin(); it != g_scaled_helmets.end();)
        {
            UObject* component = it->weak_component.Get();
            if (component == nullptr || component->IsUnreachable())
            {
                it = g_scaled_helmets.erase(it);
                continue;
            }
            if (!should_render(component))
            {
                ++it;
                continue;
            }

            if (it->restore_pending)
            {
                if (set_relative_transform_known_live(component, it->original))
                {
                    it = g_scaled_helmets.erase(it);
                    continue;
                }
                ++failures;
                ++it;
                continue;
            }

            const Transform3d head_transform =
                read_bone_transform_component_space_known_live(component,
                                                               it->head_bone_index);
            if (!transform_sane(head_transform) || !pivot_sane(head_transform.translation))
            {
                ++failures;
                ++it;
                continue;
            }

            const Transform3d desired = make_head_pivot_compensated_transform(
                it->original, head_transform.translation);
            const Transform3d current = read_relative_transform_known_live(component);
            if (!transform_sane(desired) || !transform_sane(current))
            {
                ++failures;
                ++it;
                continue;
            }
            if (transforms_near(current, desired))
            {
                it->fitted = desired;
                ++it;
                continue;
            }
            if (!set_relative_transform_known_live(component, desired))
            {
                // A preview component may reject a transform while it is being
                // rebuilt. Keep its authored transform and weak identity so a
                // later frame can retry, without disturbing verified siblings.
                ++failures;
                ++it;
                continue;
            }
            it->fitted = desired;
            ++updates;
            ++it;
        }
        if (updates != 0)
        {
            g_pending_pivot_updates.fetch_add(updates, std::memory_order_release);
        }
        if (failures != 0)
        {
            g_pending_pivot_failures.fetch_add(failures, std::memory_order_release);
        }
    }

    auto schedule_scan() -> void
    {
        g_scan_attempt.store(0, std::memory_order_release);
        g_frames_until_scan.store(2, std::memory_order_release);
        g_scan_pending.store(true, std::memory_order_release);
        g_pending_trigger_logs.fetch_add(1, std::memory_order_release);
    }

    auto hook_broadcast_refreshed(UObject* customization) -> void
    {
        const auto original = reinterpret_cast<BroadcastRefreshedFunction>(
            g_broadcast_refreshed_trampoline);
        if (original == nullptr)
        {
            return;
        }

        const SelectionFingerprint current = read_fingerprint(customization);
        const auto [had_previous, previous] = previous_fingerprint(customization, current, false);
        original(customization);
        previous_fingerprint(customization, current, true);

        if (current.target || (had_previous && previous.target))
        {
            schedule_scan();
        }
    }

}

namespace ZeroCompanyMandoWardrobe::HelmetFit
{
    auto initialize() -> bool
    {
        if (g_initialized)
        {
            return true;
        }

        const char* reason = "ok";
        if (!validate_runtime(g_runtime, reason))
        {
            RC::Output::send<RC::LogLevel::Error>(
                STR("[ZeroCompanyMandoWardrobe] helmet_fit_REFUSED reason={} hooks_active=false\n"),
                RC::ensure_str(reason));
            return false;
        }

        const FName asset_type_name(STR("CustomizationPartDefinition"), RC::Unreal::FNAME_Add);
        g_target_helmet_ids = {
            FPrimaryAssetId{FPrimaryAssetType{asset_type_name},
                            FName(STR("CPD_H_Outfit_Man001A_HELM"), RC::Unreal::FNAME_Add)},
            FPrimaryAssetId{FPrimaryAssetType{asset_type_name},
                            FName(STR("CPD_H_Outfit_Man002A_HELM"), RC::Unreal::FNAME_Add)},
        };
        g_helmet_slot_tag = GameplayTag{FName(
            STR("br.Customization.Slot.Character.Outfit.Helmet.Mesh"), RC::Unreal::FNAME_Find)};
        if (g_helmet_slot_tag.tag_name.IsNone())
        {
            RC::Output::send<RC::LogLevel::Error>(
                STR("[ZeroCompanyMandoWardrobe] helmet_fit_REFUSED reason=helmet-gameplay-tag-not-found hooks_active=false\n"));
            return false;
        }

        g_broadcast_refreshed_hook = std::make_unique<PLH::x64Detour>(
            g_runtime.base + kBroadcastRefreshedRva,
            reinterpret_cast<std::uint64_t>(&hook_broadcast_refreshed),
            &g_broadcast_refreshed_trampoline);
        if (!g_broadcast_refreshed_hook->hook() || g_broadcast_refreshed_trampoline == 0)
        {
            g_broadcast_refreshed_hook.reset();
            g_broadcast_refreshed_trampoline = 0;
            RC::Output::send<RC::LogLevel::Error>(
                STR("[ZeroCompanyMandoWardrobe] helmet_fit_REFUSED reason=broadcast-refreshed-hook-install-failed hooks_active=false\n"));
            return false;
        }

        g_initialized = true;
        RC::Output::send<RC::LogLevel::Verbose>(
            STR("[ZeroCompanyMandoWardrobe] helmet_fit_READY hooks_active=true build=24874058 policy=exact-Man001A-Man002A-head-pivot-compensated-horizontal-scale failure_isolation=per-component-retry fit_scale_factors_x_y_z=1.06,1.06,1.00 authored_vertical_scale_preserved=true original_transform_preserved=true pivot=live-head-bone-component-space setter=USceneComponent::SetRelativeTransform scan=refresh-triggered-bounded-settle tracked_pivot_update=visible-targets-only max_target_components=128 face_visibility_mutation=false per_frame_global_scan=false save_mutation=false\n"));
        return true;
    }

    auto shutdown() -> void
    {
        const auto restored = restore_all_live_components();
        if (g_broadcast_refreshed_hook)
        {
            g_broadcast_refreshed_hook->unHook();
        }
        g_broadcast_refreshed_hook.reset();
        g_broadcast_refreshed_trampoline = 0;
        g_initialized = false;
        g_scan_pending.store(false, std::memory_order_release);
        RC::Output::send<RC::LogLevel::Verbose>(
            STR("[ZeroCompanyMandoWardrobe] helmet_fit_unloaded hooks_active=false restored_transforms={} pending_pivot_updates={} pending_pivot_failures={}\n"),
            restored,
            g_pending_pivot_updates.exchange(0, std::memory_order_acquire),
            g_pending_pivot_failures.exchange(0, std::memory_order_acquire));
    }

    auto update() -> void
    {
        if (!g_initialized)
        {
            return;
        }

        const auto triggers = g_pending_trigger_logs.exchange(0, std::memory_order_acquire);
        if (triggers != 0)
        {
            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZeroCompanyMandoWardrobe] helmet_fit_transition_observed count={} scan_pending=true\n"),
                triggers);
        }

        // Scaling a full skeletal mesh around its component origin also scales
        // animated bone translations. Re-anchor only tracked, renderable target
        // components around their live head-bone pivot; never globally scan per frame.
        update_visible_head_pivot_compensation();
        ++g_pivot_summary_frames;
        if (g_pivot_summary_frames >= kPivotSummaryIntervalFrames)
        {
            g_pivot_summary_frames = 0;
            const auto updates = g_pending_pivot_updates.exchange(0, std::memory_order_acquire);
            const auto failures = g_pending_pivot_failures.exchange(0, std::memory_order_acquire);
            if (updates != 0 || failures != 0)
            {
                RC::Output::send<RC::LogLevel::Verbose>(
                    STR("[ZeroCompanyMandoWardrobe] helmet_pivot_compensation_summary updates={} failures={} interval_frames={} global_scan=false face_visibility_mutation=false\n"),
                    updates,
                    failures,
                    kPivotSummaryIntervalFrames);
            }
        }

        if (!g_scan_pending.load(std::memory_order_acquire))
        {
            return;
        }
        const auto frames = g_frames_until_scan.load(std::memory_order_acquire);
        if (frames != 0)
        {
            g_frames_until_scan.store(static_cast<std::uint8_t>(frames - 1),
                                      std::memory_order_release);
            return;
        }

        const auto attempt = static_cast<std::uint8_t>(
            g_scan_attempt.fetch_add(1, std::memory_order_acq_rel) + 1);
        const ScanResult result = collect_and_fit_target_helmets();
        RC::Output::send<RC::LogLevel::Verbose>(
            STR("[ZeroCompanyMandoWardrobe] helmet_fit_bounded_scan attempt={} component_instances={} exact_target_helmets={} newly_scaled={} reapplied={} already_scaled={} restored={} stale_pruned={} head_bones_resolved={} head_bones_missing={} component_refusals={} rollback_restored={} active_scaled={} maximum={} bound_refused={} failure_isolation=per-component-retry\n"),
            static_cast<unsigned int>(attempt),
            result.component_instances,
            result.exact_target_helmets,
            result.newly_scaled,
            result.reapplied,
            result.already_scaled,
            result.restored,
            result.stale_pruned,
            result.head_bones_resolved,
            result.head_bones_missing,
            result.component_refusals,
            result.rollback_restored,
            result.active_scaled,
            kMaxTargetHelmetComponents,
            result.bound_refused);

        if (result.bound_refused)
        {
            g_scan_pending.store(false, std::memory_order_release);
            RC::Output::send<RC::LogLevel::Error>(
                STR("[ZeroCompanyMandoWardrobe] helmet_fit_REFUSED reason=target-bound-exceeded rollback=true rollback_restored={} scan_pending=false\n"),
                result.rollback_restored);
            return;
        }

        if (result.newly_scaled != 0 || result.reapplied != 0)
        {
            RC::Output::send<RC::LogLevel::Verbose>(
                STR("[ZeroCompanyMandoWardrobe] helmet_fit_apply_complete newly_scaled={} reapplied={} head_bones_resolved={} scale_factors_x_y_z=1.06,1.06,1.00 pivot=live-head-bone-component-space authored_vertical_scale_preserved=true face_visibility_mutation=false settle_followup={}\n"),
                result.newly_scaled,
                result.reapplied,
                result.head_bones_resolved,
                attempt < kMaximumScanAttempts);
        }

        if (attempt < kMaximumScanAttempts)
        {
            constexpr std::array<std::uint8_t, 3> settle_frames{8, 30, 120};
            g_frames_until_scan.store(settle_frames[attempt - 1], std::memory_order_release);
            return;
        }

        g_scan_pending.store(false, std::memory_order_release);
        RC::Output::send<RC::LogLevel::Verbose>(
            STR("[ZeroCompanyMandoWardrobe] helmet_fit_settle_complete active_scaled={} exact_target_helmets={} head_bones_missing={} scan_pending=false pivot_compensation_active=true face_visibility_mutation=false\n"),
            result.active_scaled,
            result.exact_target_helmets,
            result.head_bones_missing);
    }
}

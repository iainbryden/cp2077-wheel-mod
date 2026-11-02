#include "steering_speed.h"
#include "logging.h"

#include <RED4ext/RTTISystem.hpp>
#include <RED4ext/TweakDB.hpp>
#include <RED4ext/Scripting/Natives/gamedataTweakDBRecord.hpp>

#include <windows.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <system_error>
#include <unordered_map>
#include <vector>

namespace gwheel::steering_speed
{
    namespace
    {
        // Drive-model record classes that carry the two rate flats. Classes
        // missing on a given build are skipped.
        constexpr const char* kRecordClasses[] = {
            "gamedataVehicleDriveModelData_Record",
            "gamedataBikeDriveModelData_Record",
        };

        constexpr const char* kRateFlats[] = {
            ".wheelTurnMaxAddPerSecond",
            ".wheelTurnMaxSubPerSecond",
        };

        // A loaded TweakDB has hundreds of thousands of flats; anything
        // smaller means it is still being populated.
        constexpr uint32_t kMinFlatsLoaded = 1000;

        // Authored rates are ~50-200 deg/s. Values outside this window mean
        // the flat isn't what we think it is; leave such records alone.
        constexpr float kMinPlausibleRate = 1.f;
        constexpr float kMaxPlausibleRate = 5000.f;

        // Upper bound on any rate we write, whatever the multiplier.
        constexpr float kMaxWrittenRate = 3000.f;

        // Abort the whole apply (write nothing) if more than this share of
        // the candidate flats fail validation: the schema has likely changed
        // under us and partial writes would be guesswork.
        constexpr float kMaxRejectedShare = 0.10f;

        struct Original
        {
            float   value;
            int32_t tdbOffset; // the authored flat value, restored at 100%
        };

        struct Target
        {
            RED4ext::gamedataTweakDBRecord* record;
            RED4ext::TweakDBID              flatID;
            Original                        original;
        };

        struct State
        {
            std::mutex                              mtx;
            int32_t                                 appliedPct = 100;
            bool                                    sentinelChecked = false;
            bool                                    disabled = false;  // for the rest of this session
            std::unordered_map<uint64_t, Original>  originals;         // key: flat ID without offset
        };

        State& S() { static State s; return s; }

        // ---- Crash sentinel --------------------------------------------------
        //
        // A marker file next to the DLL exists only while TweakDB is being
        // written. If the game dies mid-write, the next session finds it,
        // logs why, and skips steering-speed writes for that session instead
        // of repeating the same crash. Deleting the marker (automatic after
        // that skipped session) re-enables the feature.

        std::filesystem::path SentinelPath()
        {
            HMODULE mod = nullptr;
            GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS
                                   | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               reinterpret_cast<LPCWSTR>(&SentinelPath), &mod);
            wchar_t buf[MAX_PATH] = {};
            GetModuleFileNameW(mod, buf, MAX_PATH);
            std::filesystem::path p(buf);
            p.replace_filename(L"steering_speed.applying");
            return p;
        }

        void WriteSentinel(int32_t pct)
        {
            std::ofstream f(SentinelPath(), std::ios::trunc);
            f << "Steering speed " << pct << "% was being applied when the game stopped.\n";
        }

        void ClearSentinel()
        {
            std::error_code ec;
            std::filesystem::remove(SentinelPath(), ec);
        }

        // Re-point an existing flat at a different value offset. The flat
        // entry stays in the sorted array (no remove/insert window in which
        // concurrent readers would find it missing).
        bool RepointFlat(RED4ext::TweakDB* tdb, RED4ext::TweakDBID flatID, int32_t tdbOffset)
        {
            std::lock_guard<RED4ext::SharedSpinLock> _(tdb->mutex00);
            auto* entry = tdb->flats.Find(flatID);
            if (entry == tdb->flats.end()) return false;
            entry->SetTDBOffset(tdbOffset);
            return true;
        }

        int32_t CreateFloatFlat(RED4ext::TweakDB* tdb, RED4ext::rtti::IType* floatType, float value)
        {
            RED4ext::CStackType st;
            st.type  = floatType;
            st.value = &value;
            return tdb->CreateFlatValue(st);
        }

        enum class Resolve { Ok, Unset, Rejected };

        // Looks up (and caches) the authored value of one rate flat.
        // Unset: the rate is 0 (feature off for that record) — leave it.
        // Rejected: missing, not a Float, or an implausible value — leave
        // it, and count it toward the abort threshold.
        Resolve ResolveOriginal(State& s, RED4ext::TweakDB* tdb, RED4ext::TweakDBID flatID, Original& out)
        {
            if (auto it = s.originals.find(flatID.value); it != s.originals.end())
            {
                out = it->second;
                return Resolve::Ok;
            }

            auto* fv = tdb->GetFlatValue(flatID);
            if (!fv) return Resolve::Rejected;

            RED4ext::CName typeName;
            fv->GetTypeName(&typeName);
            if (typeName != RED4ext::CName("Float")) return Resolve::Rejected;

            const float value = *fv->GetValue<float>();
            if (value == 0.f) return Resolve::Unset;
            if (!(value >= kMinPlausibleRate && value <= kMaxPlausibleRate)) return Resolve::Rejected;

            out = Original{ value, fv->ToTDBOffset() };
            s.originals.emplace(flatID.value, out);
            return Resolve::Ok;
        }
    }

    bool Apply(int32_t pct)
    {
        auto& s = S();
        std::lock_guard lk(s.mtx);

        if (s.disabled) return false;
        if (pct == s.appliedPct && !s.originals.empty()) return true;
        if (pct == 100 && s.originals.empty()) return true; // never changed: nothing to restore

        if (!s.sentinelChecked)
        {
            s.sentinelChecked = true;
            std::error_code ec;
            if (std::filesystem::exists(SentinelPath(), ec))
            {
                ClearSentinel();
                s.disabled = true;
                log::Warn("[gwheel:steer-speed] the previous session stopped while steering speed was being "
                          "applied; leaving vehicles at the game's own steering speed for this session "
                          "(it will be tried again next launch)");
                return false;
            }
        }

        auto* tdb  = RED4ext::TweakDB::Get();
        auto* rtti = RED4ext::CRTTISystem::Get();
        if (!tdb || !rtti || tdb->flats.size < kMinFlatsLoaded)
        {
            log::Info("[gwheel:steer-speed] TweakDB not ready yet; will apply on the next settings push");
            return false;
        }

        auto* floatType = rtti->GetType(RED4ext::CName("Float"));
        if (!floatType)
        {
            log::Warn("[gwheel:steer-speed] Float RTTI type not found; steering speed unchanged");
            return false;
        }

        // ---- Pass 1: collect and validate, write nothing --------------------
        std::vector<Target> targets;
        uint32_t records = 0, candidates = 0, rejected = 0;

        // Record handles stay alive in these arrays until the writes are
        // done, so the raw record pointers in `targets` remain valid.
        std::vector<RED4ext::DynArray<RED4ext::Handle<RED4ext::IScriptable>>> lists;
        lists.reserve(std::size(kRecordClasses));

        for (const char* className : kRecordClasses)
        {
            auto* cls = rtti->GetClass(RED4ext::CName(className));
            if (!cls) continue;

            auto& list = lists.emplace_back();
            if (!tdb->TryGetRecordsByType(cls, list)) continue;

            for (auto& handle : list)
            {
                auto* record = reinterpret_cast<RED4ext::gamedataTweakDBRecord*>(handle.GetPtr());
                if (!record || !record->recordID.IsValid()) continue;
                ++records;

                for (const char* flatName : kRateFlats)
                {
                    const RED4ext::TweakDBID flatID(record->recordID, flatName);
                    Original o;
                    switch (ResolveOriginal(s, tdb, flatID, o))
                    {
                    case Resolve::Ok:       ++candidates; targets.push_back({ record, flatID, o }); break;
                    case Resolve::Rejected: ++candidates; ++rejected; break;
                    case Resolve::Unset:    break;
                    }
                }
            }
        }

        if (records == 0 || targets.empty())
        {
            log::Warn("[gwheel:steer-speed] no drive-model rate flats found; steering speed unchanged");
            return false;
        }
        if (static_cast<float>(rejected) > kMaxRejectedShare * static_cast<float>(candidates))
        {
            s.disabled = true;
            log::WarnF("[gwheel:steer-speed] %u of %u rate flats failed validation (missing, not Float, or "
                       "out of range); not touching TweakDB this session",
                       rejected, candidates);
            return false;
        }

        // ---- Pass 2: write ---------------------------------------------------
        const float mul = static_cast<float>(pct) / 100.f;

        // One new flat value per distinct scaled value (many records share
        // authored rates), rather than one per record.
        std::unordered_map<float, int32_t> createdByValue;

        uint32_t flatsChanged = 0, recordsUpdated = 0, updateFailures = 0;
        float    sampleOrig = 0.f, sampleNew = 0.f;
        bool     bufferFull = false;
        RED4ext::gamedataTweakDBRecord* pendingRecord = nullptr;

        auto flushRecord = [&]() {
            if (!pendingRecord) return;
            if (tdb->UpdateRecord(pendingRecord)) ++recordsUpdated;
            else                                  ++updateFailures;
            pendingRecord = nullptr;
        };

        WriteSentinel(pct);

        for (const Target& t : targets)
        {
            if (t.record != pendingRecord) flushRecord();

            int32_t offset = t.original.tdbOffset;
            if (pct != 100)
            {
                const float scaled = std::min(t.original.value * mul, kMaxWrittenRate);
                auto c = createdByValue.find(scaled);
                if (c == createdByValue.end())
                {
                    const int32_t created = CreateFloatFlat(tdb, floatType, scaled);
                    if (created < 0)
                    {
                        log::Warn("[gwheel:steer-speed] TweakDB flat buffer full; stopping early");
                        bufferFull = true;
                        break;
                    }
                    c = createdByValue.emplace(scaled, created).first;
                }
                offset = c->second;
                if (flatsChanged == 0) { sampleOrig = t.original.value; sampleNew = scaled; }
            }

            if (RepointFlat(tdb, t.flatID, offset))
            {
                ++flatsChanged;
                pendingRecord = t.record;
            }
        }
        flushRecord();

        ClearSentinel();

        s.appliedPct = pct;
        log::InfoF("[gwheel:steer-speed] applied %d%%: %u records refreshed (%u failed), %u rate flats set, "
                   "%u skipped by validation%s%s",
                   pct, recordsUpdated, updateFailures, flatsChanged, rejected,
                   bufferFull ? ", stopped early (flat buffer full)" : "",
                   pct == 100 ? " — authored values restored" : "");
        if (pct != 100 && flatsChanged > 0)
            log::InfoF("[gwheel:steer-speed]   e.g. %.1f -> %.1f deg/s; applies to vehicles spawned from now on",
                       sampleOrig, sampleNew);
        return flatsChanged > 0;
    }
}

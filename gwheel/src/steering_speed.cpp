#include "steering_speed.h"
#include "logging.h"

#include <RED4ext/RTTISystem.hpp>
#include <RED4ext/TweakDB.hpp>
#include <RED4ext/Scripting/Natives/gamedataTweakDBRecord.hpp>

#include <algorithm>
#include <mutex>
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

        struct Original
        {
            float   value;
            int32_t tdbOffset; // the authored flat value, restored at 100%
        };

        struct State
        {
            std::mutex                              mtx;
            int32_t                                 appliedPct = 100;
            std::unordered_map<uint64_t, Original>  originals;  // key: flat ID without offset
        };

        State& S() { static State s; return s; }

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
    }

    bool Apply(int32_t pct)
    {
        auto& s = S();
        std::lock_guard lk(s.mtx);

        if (pct == s.appliedPct && !s.originals.empty()) return true;
        if (pct == 100 && s.originals.empty()) return true; // never changed: nothing to restore

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

        const float mul = static_cast<float>(pct) / 100.f;

        // One new flat value per distinct scaled value (many records share
        // authored rates), rather than one per record.
        std::unordered_map<float, int32_t> createdByValue;

        uint32_t records = 0, flatsChanged = 0, flatsMissing = 0;
        float    sampleOrig = 0.f, sampleNew = 0.f;
        bool     bufferFull = false;

        for (const char* className : kRecordClasses)
        {
            if (bufferFull) break;
            auto* cls = rtti->GetClass(RED4ext::CName(className));
            if (!cls) continue;

            RED4ext::DynArray<RED4ext::Handle<RED4ext::IScriptable>> list;
            if (!tdb->TryGetRecordsByType(cls, list)) continue;

            for (auto& handle : list)
            {
                if (bufferFull) break;
                auto* record = reinterpret_cast<RED4ext::gamedataTweakDBRecord*>(handle.GetPtr());
                if (!record) continue;
                ++records;

                bool changed = false;
                for (const char* flatName : kRateFlats)
                {
                    const RED4ext::TweakDBID flatID(record->recordID, flatName);

                    auto it = s.originals.find(flatID.value);
                    if (it == s.originals.end())
                    {
                        auto* fv = tdb->GetFlatValue(flatID);
                        if (!fv) { ++flatsMissing; continue; }
                        const float orig = *fv->GetValue<float>();
                        it = s.originals.emplace(flatID.value, Original{ orig, fv->ToTDBOffset() }).first;
                    }

                    const Original& o = it->second;
                    if (o.value <= 0.f) continue; // unset / disabled rate: leave alone

                    int32_t offset = o.tdbOffset;
                    if (pct != 100)
                    {
                        const float scaled = o.value * mul;
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
                        if (flatsChanged == 0) { sampleOrig = o.value; sampleNew = scaled; }
                    }

                    if (RepointFlat(tdb, flatID, offset))
                    {
                        ++flatsChanged;
                        changed = true;
                    }
                }

                if (changed)
                    tdb->UpdateRecord(record);
            }
        }

        if (records == 0)
        {
            log::Warn("[gwheel:steer-speed] no drive-model records found; steering speed unchanged");
            return false;
        }

        s.appliedPct = pct;
        log::InfoF("[gwheel:steer-speed] applied %d%% to %u records (%u rate flats, %u missing)%s",
                   pct, records, flatsChanged, flatsMissing,
                   pct == 100 ? " — authored values restored" : "");
        if (pct != 100 && flatsChanged > 0)
            log::InfoF("[gwheel:steer-speed]   e.g. %.1f -> %.1f deg/s; applies to vehicles spawned from now on",
                       sampleOrig, sampleNew);
        return true;
    }
}

#include "testfw.h"

#include "rtti_offsets.cpp"

#include <memory>

using namespace gwheel;
using namespace gwheel::rtti_offsets;

// The struct-offset probe finds physics / physicsData on a live vehicle by
// looking for pointers whose target points back at the vehicle. A plain
// buffer laid out like a 2.5 vehicle stands in for the game object.

namespace
{
    constexpr size_t kVehicleSize = 0x400;
    constexpr size_t kInnerSize   = 0x200;

    constexpr size_t kPhysicsOuter     = 0x2D0;
    constexpr size_t kPhysicsDataOuter = 0x2D8;
    constexpr size_t kPhysicsBackRef     = 0x060;
    constexpr size_t kPhysicsDataBackRef = 0x180;

    // The scan covers calls 0, 30, 60, ... so this many calls is one attempt.
    constexpr int kCallsPerAttempt = 30;
    constexpr int kMaxAttempts     = 40;

    void ResetProbe()
    {
        auto& st = S();
        st.vehicleBaseObject_physics.store(0);
        st.vehicleBaseObject_physicsData.store(0);
        st.probed_physicsOffset.store(0);
        st.probed_physicsDataOffset.store(0);
        st.probeDone.store(false);
        st.probeCalls.store(0);
        st.probeAttempts.store(0);
    }

    struct FakeVehicle
    {
        // 8-byte aligned, zeroed heap blocks; the probe checks both with
        // VirtualQuery and an alignment test.
        std::unique_ptr<uint64_t[]> vehicle{ new uint64_t[kVehicleSize / 8]() };
        std::unique_ptr<uint64_t[]> physics{ new uint64_t[kInnerSize / 8]() };
        std::unique_ptr<uint64_t[]> physicsData{ new uint64_t[kInnerSize / 8]() };

        void* self() { return vehicle.get(); }

        void SetSlot(size_t offset, void* target)
        {
            vehicle[offset / 8] = reinterpret_cast<uint64_t>(target);
        }

        void LinkPhysics()
        {
            physics[kPhysicsBackRef / 8] = reinterpret_cast<uint64_t>(self());
            SetSlot(kPhysicsOuter, physics.get());
        }

        void LinkPhysicsData()
        {
            physicsData[kPhysicsDataBackRef / 8] = reinterpret_cast<uint64_t>(self());
            SetSlot(kPhysicsDataOuter, physicsData.get());
        }
    };

    struct Fixture
    {
        Fixture()  { ResetProbe(); }
        ~Fixture() { ResetProbe(); }
    };

    // Calls the probe through `attempts` scan attempts, as ticks would.
    void RunAttempts(void* vehicle, int attempts)
    {
        for (int i = 0; i < attempts * kCallsPerAttempt && !StructProbeDone(); ++i)
            ProbeStructOffsets(vehicle);
    }
}

TEST_CASE(probe_finds_both_offsets_on_a_2_5_layout)
{
    Fixture fx;
    FakeVehicle car;
    car.LinkPhysics();
    car.LinkPhysicsData();

    CHECK(ProbeStructOffsets(car.self()));
    CHECK(StructProbeDone());
    CHECK_EQ(VehicleBaseObject_physicsOffset(), static_cast<uint32_t>(kPhysicsOuter));
    CHECK_EQ(VehicleBaseObject_physicsDataOffset(), static_cast<uint32_t>(kPhysicsDataOuter));
}

TEST_CASE(probe_ignores_a_float_in_the_old_physics_slot)
{
    // On 2.5, 0x2C8 holds a float; it must not be taken for a pointer.
    Fixture fx;
    FakeVehicle car;
    car.vehicle[0x2C8 / 8] = 0x3DCCCDE1ull;
    car.LinkPhysics();
    car.LinkPhysicsData();

    CHECK(ProbeStructOffsets(car.self()));
    CHECK_EQ(VehicleBaseObject_physicsOffset(), static_cast<uint32_t>(kPhysicsOuter));
}

TEST_CASE(probe_retries_while_the_physics_chain_is_missing)
{
    // The bug: one failed scan on an early tick latched "done" for the whole
    // session, even though the chain appeared moments later.
    Fixture fx;
    FakeVehicle car;

    CHECK(!ProbeStructOffsets(car.self()));
    CHECK(!StructProbeDone());
    CHECK_EQ(VehicleBaseObject_physicsOffset(), 0u);

    car.LinkPhysics();
    car.LinkPhysicsData();
    RunAttempts(car.self(), 2);
    CHECK(StructProbeDone());
    CHECK_EQ(VehicleBaseObject_physicsOffset(), static_cast<uint32_t>(kPhysicsOuter));
    CHECK_EQ(VehicleBaseObject_physicsDataOffset(), static_cast<uint32_t>(kPhysicsDataOuter));
}

TEST_CASE(probe_keeps_retrying_after_a_partial_result)
{
    Fixture fx;
    FakeVehicle car;
    car.LinkPhysicsData();

    CHECK(!ProbeStructOffsets(car.self()));
    CHECK(!StructProbeDone());
    CHECK_EQ(VehicleBaseObject_physicsDataOffset(), static_cast<uint32_t>(kPhysicsDataOuter));
    CHECK_EQ(VehicleBaseObject_physicsOffset(), 0u);

    car.LinkPhysics();
    RunAttempts(car.self(), 2);
    CHECK(StructProbeDone());
    CHECK_EQ(VehicleBaseObject_physicsOffset(), static_cast<uint32_t>(kPhysicsOuter));
}

TEST_CASE(probe_scans_only_every_30_calls)
{
    Fixture fx;
    FakeVehicle car;
    for (int i = 0; i < kCallsPerAttempt; ++i)
        ProbeStructOffsets(car.self());
    CHECK_EQ(S().probeAttempts.load(), 1u);
    ProbeStructOffsets(car.self());
    CHECK_EQ(S().probeAttempts.load(), 2u);
}

TEST_CASE(probe_gives_up_after_its_attempts_and_keeps_fallbacks)
{
    Fixture fx;
    FakeVehicle car;
    RunAttempts(car.self(), kMaxAttempts - 1);
    CHECK(!StructProbeDone());
    RunAttempts(car.self(), 1);
    CHECK(StructProbeDone());
    CHECK_EQ(S().probeAttempts.load(), static_cast<uint32_t>(kMaxAttempts));
    CHECK_EQ(VehicleBaseObject_physicsOffset(), 0u);       // 0 = use the fallback
    CHECK_EQ(VehicleBaseObject_physicsDataOffset(), 0u);

    // Finished means finished: no further scans.
    ProbeStructOffsets(car.self());
    CHECK_EQ(S().probeAttempts.load(), static_cast<uint32_t>(kMaxAttempts));
}

TEST_CASE(probe_null_vehicle_is_a_no_op)
{
    Fixture fx;
    CHECK(!ProbeStructOffsets(nullptr));
    CHECK(!StructProbeDone());
    CHECK_EQ(S().probeAttempts.load(), 0u);
}

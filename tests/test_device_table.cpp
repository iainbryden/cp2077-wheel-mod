#include "testfw.h"

#include "device_table.h"

#include <set>

using namespace gwheel;

TEST_CASE(device_table_finds_g29_in_both_modes)
{
    const ModelInfo* native = LookupByPid(0xC24F);
    REQUIRE(native != nullptr);
    CHECK_EQ(native->id, Model::G29Native);
    CHECK(native->ffb_default);
    CHECK(native->has_rev_leds);
    CHECK(native->has_right_cluster);
    CHECK_EQ(native->steering_range_deg, 900);

    const ModelInfo* ps = LookupByPid(0xC260);
    REQUIRE(ps != nullptr);
    CHECK_EQ(ps->id, Model::G29Ps);
}

TEST_CASE(device_table_g920_has_no_leds_or_right_cluster)
{
    for (uint32_t pid : { 0xC261u, 0xC262u })
    {
        const ModelInfo* m = LookupByPid(pid);
        REQUIRE(m != nullptr);
        CHECK(m->ffb_default);
        CHECK(!m->has_rev_leds);
        CHECK(!m->has_right_cluster);
    }
}

TEST_CASE(device_table_g_pro_is_1080_degrees)
{
    for (uint32_t pid : { 0xC268u, 0xC272u })
    {
        const ModelInfo* m = LookupByPid(pid);
        REQUIRE(m != nullptr);
        CHECK_EQ(m->steering_range_deg, 1080);
    }
}

TEST_CASE(device_table_unknown_pid_returns_null)
{
    CHECK(LookupByPid(0) == nullptr);
    CHECK(LookupByPid(0xFFFF) == nullptr);
    CHECK(LookupByPid(0xC24E) == nullptr);
}

TEST_CASE(device_table_rows_are_unique_and_sane)
{
    std::set<uint32_t> pids;
    std::set<int>      models;
    int rows = 0;
    // Every PID in the documented 0xC200..0xCAFF block is either absent or
    // maps to exactly one row; walk the range to see all of them.
    for (uint32_t pid = 0xC200; pid <= 0xCAFF; ++pid)
    {
        const ModelInfo* m = LookupByPid(pid);
        if (!m) continue;
        ++rows;
        CHECK_EQ(m->pid, pid);
        CHECK(m->id != Model::Unknown);
        CHECK(!m->name.empty());
        CHECK(m->steering_range_deg >= 200 && m->steering_range_deg <= 1080);
        // Rev LEDs only exist on wheels that also have the right cluster.
        if (m->has_rev_leds) CHECK(m->has_right_cluster);
        pids.insert(m->pid);
        models.insert(static_cast<int>(m->id));
    }
    CHECK_EQ(rows, 22);
    CHECK_EQ(pids.size(), static_cast<size_t>(22));
    CHECK_EQ(models.size(), static_cast<size_t>(22));
}

#include "testfw.h"

#include <cstring>

// Usage: gwheel_tests [substring]
// With an argument, runs only the test cases whose name contains it.
int main(int argc, char** argv)
{
    const char* filter = argc > 1 ? argv[1] : nullptr;
    int ran = 0, failed = 0;

    for (const auto& c : testfw::Registry())
    {
        if (filter && !std::strstr(c.name, filter)) continue;
        ++ran;
        testfw::FailuresInCase() = 0;
        bool threw = false;
        try
        {
            c.fn();
        }
        catch (const testfw::RequireFailed&)
        {
            threw = true;
        }
        catch (const std::exception& e)
        {
            testfw::ReportFailure(c.file, 0, std::string("unexpected exception: ") + e.what());
        }
        catch (...)
        {
            testfw::ReportFailure(c.file, 0, "unexpected non-standard exception");
        }

        const bool ok = testfw::FailuresInCase() == 0 && !threw;
        if (!ok) ++failed;
        std::printf("%s %s\n", ok ? "[ ok ]" : "[FAIL]", c.name);
    }

    std::printf("\n%d test case(s), %d failed\n", ran, failed);
    return failed == 0 && ran > 0 ? 0 : 1;
}

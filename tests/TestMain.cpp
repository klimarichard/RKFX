#include <juce_core/juce_core.h>

#include <cstdio>

int main (int, char**)
{
    juce::UnitTestRunner runner;
    runner.setAssertOnFailure (false); // record failures, don't abort
    runner.runAllTests();

    int failures = 0;
    for (int i = 0; i < runner.getNumResults(); ++i)
        if (auto* r = runner.getResult (i))
            failures += r->failures;

    if (failures > 0)
    {
        std::fprintf (stderr, "RKFXTests: %d test failure(s)\n", failures);
        return 1;
    }

    std::printf ("RKFXTests: all tests passed\n");
    return 0;
}

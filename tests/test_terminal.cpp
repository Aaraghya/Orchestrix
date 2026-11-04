// Tests for Terminal.h — the optional ANSI colour layer.
//
// The important guarantees are:
//   1. Stripping the escape codes from a styled line gives back the original
//      text, so colour never changes what the report says.
//   2. Lines are coloured by meaning (rejected = red, allocated = green, ...).
//   3. Colour is off unless main() switches it on, so other output is plain.

#include "Terminal.h"

#include <iostream>
#include <sstream>
#include <string>
#include <vector>

static int g_failures = 0;

#define CHECK(cond)                                                         \
    do {                                                                    \
        if (!(cond)) {                                                      \
            std::cerr << "FAILED " << __FILE__ << ":" << __LINE__ << "  "   \
                      << #cond << "\n";                                     \
            ++g_failures;                                                   \
        }                                                                   \
    } while (0)

/// Removes every "ESC [ ... m" sequence.
static std::string stripAnsi(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\033' && i + 1 < s.size() && s[i + 1] == '[') {
            while (i < s.size() && s[i] != 'm') ++i;
        } else {
            out.push_back(s[i]);
        }
    }
    return out;
}

static bool contains(const std::string& s, const std::string& part) {
    return s.find(part) != std::string::npos;
}

static void testColourIsOffByDefault() {
    CHECK(!term::enabled());
}

static void testStrippingRestoresOriginalText() {
    const std::vector<std::string> samples = {
        "============================================================",
        "------------------------------------------------------------",
        "                  ORCHESTRIX SIMULATION",
        "Strategy: First Fit",
        "Jobs processed: 20",
        "Jobs allocated: 18",
        "Jobs rejected: 2",
        "Average allocation time: N/A",
        "Resource utilization: CPU: 9.9% | RAM: 8.2% | Storage: 3.2% | GPU: N/A",
        "Job ID    Server ID    Status",
        "job-0     srv-3        Allocated",
        "job-4     -            Rejected",
        "job-9     -            Queued",
        "  Strategy  : BestFit",
        "  Status    : Completed normally",
        "    Completed   : 20  (100.0%)",
        "    Rejected    : 3  (15.0%)",
        "    CPU     : 85.0%",
        "    GPU     : N/A",
        "  1. Run simulation - First Fit",
        "     First compatible server with enough resources.",
        "  8. Exit",
        "Choice: ",
        "> ",
        "  [tick 3 - job job-4 started on srv-2]",
        "  srv-0   GPUServer     10.0    10.0    33.0    33.0",
        "  Invalid choice 'x'. Please enter a number between 1 and 8.",
        "  Goodbye!",
        "",
        "   ",
        "plain text with no special meaning",
    };
    for (const auto& line : samples) {
        CHECK(stripAnsi(term::styleLine(line)) == line);
    }
}

static void testStatusColours() {
    CHECK(contains(term::styleLine("job-4     -            Rejected"),
                   std::string(term::ansi::boldRed) + "Rejected"));
    CHECK(contains(term::styleLine("job-0     srv-3        Allocated"),
                   std::string(term::ansi::boldGreen) + "Allocated"));
    CHECK(contains(term::styleLine("job-9     -            Queued"),
                   std::string(term::ansi::boldYellow) + "Queued"));
}

static void testCountsOnlyTurnRedWhenNonZero() {
    CHECK(contains(term::styleLine("    Rejected    : 3  (15.0%)"), term::ansi::boldRed));
    CHECK(!contains(term::styleLine("    Rejected    : 0  (0.0%)"), term::ansi::boldRed));
}

static void testUtilizationThresholds() {
    CHECK(std::string(term::utilizationStyle("12.0%"))  == term::ansi::green);
    CHECK(std::string(term::utilizationStyle("65.0%"))  == term::ansi::boldYellow);
    CHECK(std::string(term::utilizationStyle("93.5%"))  == term::ansi::boldRed);
    CHECK(std::string(term::utilizationStyle("N/A"))    == term::ansi::dim);
}

static void testHeadingsAndPlainLines() {
    CHECK(contains(term::styleLine("======"), term::ansi::cyan));
    CHECK(contains(term::styleLine("   ALLOCATION DETAILS"), term::ansi::boldCyan));
    CHECK(term::styleLine("plain text with no special meaning") ==
          "plain text with no special meaning");
    CHECK(term::styleLine("") == "");
}

static void testLineStylerForwardsText() {
    std::ostringstream sink;
    {
        term::LineStyler styler(sink.rdbuf());
        std::ostream out(&styler);
        out << "job-4     -            Rejected\n";
        out << "Choice: ";
        out.flush();   // prompts without a newline are styled on flush
    }
    const std::string result = sink.str();
    CHECK(contains(result, term::ansi::boldRed));
    CHECK(stripAnsi(result) == "job-4     -            Rejected\nChoice: ");
}

int main() {
    testColourIsOffByDefault();
    testStrippingRestoresOriginalText();
    testStatusColours();
    testCountsOnlyTurnRedWhenNonZero();
    testUtilizationThresholds();
    testHeadingsAndPlainLines();
    testLineStylerForwardsText();

    if (g_failures == 0) {
        std::cout << "All Terminal tests passed.\n";
        return 0;
    }
    std::cerr << g_failures << " Terminal check(s) failed.\n";
    return 1;
}

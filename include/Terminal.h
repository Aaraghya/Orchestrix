#pragma once

/// Terminal.h — optional ANSI colour for the Orchestrix CLI.
///
/// Design:
///  - The report generators keep producing plain text (so tests and any
///    piped output stay byte-for-byte identical).
///  - main() installs a LineStyler between std::cout and the terminal.  It
///    colours each finished line according to what the line *means*
///    (headings, statuses, menu items, utilization, ...).
///  - Colour is only switched on when stdout is a terminal.  It can be
///    forced with ORCHESTRIX_COLOR=always, or disabled with NO_COLOR=1 /
///    ORCHESTRIX_COLOR=never.
///  - Header-only; standard library (plus the Windows console API on Windows).

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <streambuf>
#include <string>

#ifdef _WIN32
#  ifndef NOMINMAX
#    define NOMINMAX
#  endif
#  ifndef WIN32_LEAN_AND_MEAN
#    define WIN32_LEAN_AND_MEAN
#  endif
#  include <io.h>
#  include <windows.h>
#else
#  include <unistd.h>
#endif

namespace term {

// ─── ANSI escape sequences ────────────────────────────────────────────────────

namespace ansi {
inline constexpr const char* reset       = "\033[0m";
inline constexpr const char* bold        = "\033[1m";
inline constexpr const char* dim         = "\033[2m";
inline constexpr const char* red         = "\033[31m";
inline constexpr const char* green       = "\033[32m";
inline constexpr const char* yellow      = "\033[33m";
inline constexpr const char* blue        = "\033[34m";
inline constexpr const char* magenta     = "\033[35m";
inline constexpr const char* cyan        = "\033[36m";
inline constexpr const char* boldRed     = "\033[31m";
inline constexpr const char* boldGreen   = "\033[32m";
inline constexpr const char* boldYellow  = "\033[33m";
inline constexpr const char* boldMagenta = "\033[35m";
inline constexpr const char* boldCyan    = "\033[36m";
}  // namespace ansi

// ─── Small string helpers ─────────────────────────────────────────────────────

inline std::string wrap(const std::string& text, const char* style) {
    if (text.empty()) return text;
    return std::string(style) + text + ansi::reset;
}

inline std::string trimCopy(const std::string& s) {
    const auto a = s.find_first_not_of(" \t\r");
    if (a == std::string::npos) return {};
    const auto b = s.find_last_not_of(" \t\r");
    return s.substr(a, b - a + 1);
}

inline bool startsWith(const std::string& s, const std::string& prefix) {
    return s.compare(0, prefix.size(), prefix) == 0;
}

inline bool allOf(const std::string& s, char c) {
    return !s.empty() && s.find_first_not_of(c) == std::string::npos;
}

inline bool hasLetter(const std::string& s) {
    for (unsigned char ch : s) {
        if (std::isalpha(ch)) return true;
    }
    return false;
}

/// First number found in \p s (0 when there is none).
inline double firstNumber(const std::string& s) {
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (std::isdigit(static_cast<unsigned char>(s[i]))) {
            return std::strtod(s.c_str() + i, nullptr);
        }
    }
    return 0.0;
}

// ─── Line styling ─────────────────────────────────────────────────────────────

/// Colour for a job status word ("" when \p word is not a status).
inline const char* statusStyle(const std::string& word) {
    if (word == "Allocated" || word == "Completed") return ansi::boldGreen;
    if (word == "Running")                          return ansi::boldCyan;
    if (word == "Queued" || word == "Pending")      return ansi::boldYellow;
    if (word == "Rejected" || word == "Failed" ||
        word == "Cancelled")                        return ansi::boldRed;
    return "";
}

/// Colour for a utilization percentage: green is healthy, red is saturated.
inline const char* utilizationStyle(const std::string& value) {
    if (value.find("N/A") != std::string::npos) return ansi::dim;
    const double pct = firstNumber(value);
    if (pct >= 80.0) return ansi::boldRed;
    if (pct >= 50.0) return ansi::boldYellow;
    return ansi::green;
}

/// Colour for the value part of a "Label : value" line ("" = leave plain).
inline const char* valueStyle(const std::string& label, const std::string& value) {
    if (label == "Strategy")                          return ansi::boldMagenta;
    if (label == "Status") {
        if (value.find("Completed normally") != std::string::npos) return ansi::boldGreen;
        return ansi::boldRed;
    }
    if (label == "Completed" || label == "Jobs allocated") return ansi::boldGreen;
    if (label == "Rejected" || label == "Jobs rejected" ||
        label == "Failed"   || label == "Cancelled")
        return firstNumber(value) > 0 ? ansi::boldRed : ansi::dim;
    if (label == "Queued")  return firstNumber(value) > 0 ? ansi::boldYellow : ansi::dim;
    if (label == "Jobs processed")                    return ansi::bold;
    if (label == "Avg wait time" || label == "Throughput" ||
        label == "Elapsed")                           return ansi::cyan;
    if (label == "CPU" || label == "RAM" ||
        label == "Storage" || label == "GPU")         return utilizationStyle(value);
    return "";
}

/// Colours every "12.3%" / "N/A" token on a "CPU: 9.9% | RAM: ..." line.
inline std::string styleUtilizationLine(const std::string& line) {
    std::string out;
    std::size_t i = 0;
    while (i < line.size()) {
        const auto colon = line.find(": ", i);
        if (colon == std::string::npos) { out += line.substr(i); break; }
        out += line.substr(i, colon + 2 - i);
        const auto end = line.find(" |", colon + 2);
        const std::string value = line.substr(
            colon + 2, (end == std::string::npos ? line.size() : end) - (colon + 2));
        out += wrap(value, utilizationStyle(value));
        if (end == std::string::npos) break;
        i = end;
    }
    return out;
}

/// Returns \p line (without its trailing newline) with ANSI colour added.
/// Stripping the escape sequences always gives back the original text.
inline std::string styleLine(const std::string& line) {
    const std::string t = trimCopy(line);
    if (t.empty()) return line;

    // Rulers.
    if (allOf(t, '=')) return wrap(line, ansi::cyan);
    if (allOf(t, '-')) return wrap(line, ansi::dim);

    // Banner artwork and subtitle.
    if (!hasLetter(t) && t.find_first_of("_|/\\") != std::string::npos)
        return wrap(line, ansi::boldYellow);
    if (startsWith(t, "Discrete-Event") || startsWith(t, "Resource Allocation  |"))
        return wrap(line, ansi::cyan);

    // Screen titles.
    static const char* const titles[] = {
        "ORCHESTRIX SIMULATION", "ALLOCATION DETAILS", "Strategy Comparison Report",
        "Server Resource Status", "MAIN MENU", "FILTER EVENT LOG BY TYPE",
        "CONFIGURE WORKLOAD", "Job Outcome Summary", "Simulation Events:"};
    for (const char* title : titles) {
        if (startsWith(t, title)) return wrap(line, ansi::boldCyan);
    }

    // Section headings inside a report.
    static const char* const sections[] = {
        "Job Outcomes", "Performance", "Resource Utilization (",
        "Current workload settings", "Updated workload settings"};
    for (const char* section : sections) {
        if (startsWith(t, section)) return wrap(line, ansi::boldYellow);
    }

    // Table header rows.
    if (startsWith(t, "Job ID") ||
        (startsWith(t, "Strategy") && t.find("Completed") != std::string::npos) ||
        (startsWith(t, "Tick") && t.find("Job ID") != std::string::npos) ||
        (startsWith(t, "Server") && t.find("Type") != std::string::npos))
        return wrap(line, ansi::bold);

    // Menu items: "  1. Run simulation - First Fit".
    if (t.size() > 3 && std::isdigit(static_cast<unsigned char>(t[0])) &&
        t[1] == '.' && t[2] == ' ') {
        const std::string indent = line.substr(0, line.find_first_not_of(' '));
        const bool isExit = t.find("Exit") == 3;
        return indent + wrap(t.substr(0, 2), ansi::boldYellow) + " " +
               wrap(t.substr(3), isExit ? ansi::red : ansi::bold);
    }
    // Menu descriptions (5-space indent, sentence ending in a full stop).
    if (startsWith(line, "     ") && line[5] != ' ' && t.back() == '.')
        return wrap(line, ansi::dim);

    // Server-status snapshots: "[tick 3 - job job-4 started on srv-2]".
    if (startsWith(t, "[tick ")) {
        if (t.find("started on") != std::string::npos)  return wrap(line, ansi::boldGreen);
        if (t.find("released")   != std::string::npos)  return wrap(line, ansi::boldYellow);
        return wrap(line, ansi::boldCyan);
    }

    // Server-status table rows: colour the id and the server type.
    if (startsWith(t, "srv-")) {
        const auto idStart = line.find_first_not_of(' ');
        const auto idEnd   = line.find(' ', idStart);
        const auto tyStart = line.find_first_not_of(' ', idEnd);
        const auto tyEnd   = line.find(' ', tyStart);
        if (idEnd != std::string::npos && tyStart != std::string::npos) {
            const std::string type = line.substr(tyStart, tyEnd - tyStart);
            const char* tyStyle = type == "GPUServer"     ? ansi::boldMagenta
                                : type == "StorageServer" ? ansi::boldYellow
                                                          : ansi::boldCyan;
            return line.substr(0, idStart) +
                   wrap(line.substr(idStart, idEnd - idStart), ansi::bold) +
                   line.substr(idEnd, tyStart - idEnd) +
                   wrap(type, tyStyle) +
                   (tyEnd == std::string::npos ? std::string() : line.substr(tyEnd));
        }
    }

    // Allocation rows: "job-4     -            Rejected".
    if (startsWith(t, "job-")) {
        const auto sp = line.find_last_of(' ');
        if (sp != std::string::npos) {
            const std::string word = line.substr(sp + 1);
            const char* style = statusStyle(trimCopy(word));
            if (*style != '\0')
                return line.substr(0, sp + 1) + wrap(word, style);
        }
    }

    // "Label: value" and "Label : value" lines.
    const auto colon = line.find(':');
    if (colon != std::string::npos && colon + 1 < line.size() &&
        line[colon + 1] == ' ' && !startsWith(t, "[") && !startsWith(t, "Invalid")) {
        std::string label = trimCopy(line.substr(0, colon));
        const std::string value = line.substr(colon + 1);

        if (label == "Resource utilization")
            return wrap(line.substr(0, colon + 1), ansi::dim) + " " +
                   styleUtilizationLine(trimCopy(value));

        const char* style = valueStyle(label, value);
        if (*style != '\0') {
            const auto vStart = value.find_first_not_of(' ');
            return wrap(line.substr(0, colon + 1), ansi::dim) +
                   value.substr(0, vStart) +
                   wrap(value.substr(vStart), style);
        }
        if (label.size() < 24 && label.find("  ") == std::string::npos &&
            !startsWith(label, "Choice"))
            return wrap(line.substr(0, colon + 1), ansi::dim) + value;
    }

    // Prompts.
    if (startsWith(t, "Choice"))                     return wrap(line, ansi::boldCyan);
    if (startsWith(t, "New "))                       return wrap(line, ansi::cyan);
    if (t == ">")                                    return wrap(line, ansi::boldGreen);
    if (startsWith(t, "Showing results"))            return wrap(line, ansi::dim);
    if (startsWith(t, "[N] Next"))                   return wrap(line, ansi::yellow);

    // Messages.
    if (startsWith(t, "Invalid") || startsWith(t, "Please enter"))
        return wrap(line, ansi::boldRed);
    if (startsWith(t, "Already on") || startsWith(t, "(No "))
        return wrap(line, ansi::yellow);
    if (startsWith(t, "(Keeping"))                   return wrap(line, ansi::dim);
    if (startsWith(t, "Running ") || startsWith(t, "Comparing "))
        return wrap(line, ansi::cyan);
    if (t.find("Goodbye!") != std::string::npos)     return wrap(line, ansi::boldGreen);

    return line;
}

// ─── Colour on/off switch ─────────────────────────────────────────────────────

inline bool& enabledFlag() {
    static bool flag = false;   // off by default: tests never call init()
    return flag;
}

inline bool enabled() { return enabledFlag(); }

/// True when stdout looks like an interactive terminal.
inline bool stdoutIsTerminal() {
#ifdef _WIN32
    if (_isatty(_fileno(stdout))) return true;
    // MSYS2 / mintty hands native programs a pipe instead of a console, but
    // exports MSYSTEM and TERM, so treat that as a terminal too.
    return std::getenv("MSYSTEM") != nullptr && std::getenv("TERM") != nullptr;
#else
    return isatty(fileno(stdout)) != 0;
#endif
}

/// Lets the classic Windows console (cmd / PowerShell) understand ANSI codes.
inline void enableVirtualTerminal() {
#ifdef _WIN32
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD mode = 0;
    if (h != INVALID_HANDLE_VALUE && GetConsoleMode(h, &mode)) {
        SetConsoleMode(h, mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
    }
#endif
}

/// Decides whether colour is used (call once, at the start of main()).
inline void init() {
    const char* noColor = std::getenv("NO_COLOR");
    const char* pref    = std::getenv("ORCHESTRIX_COLOR");
    const std::string mode = pref ? pref : "";

    bool on = false;
    if ((noColor && *noColor) || mode == "never") on = false;
    else if (mode == "always")                    on = true;
    else                                          on = stdoutIsTerminal();

    if (on) enableVirtualTerminal();
    enabledFlag() = on;
}

// ─── std::cout adapter ────────────────────────────────────────────────────────

/// A stream buffer that colours each finished line and forwards it.
/// Partial lines (prompts such as "Choice: ") are styled when flushed.
class LineStyler : public std::streambuf {
public:
    explicit LineStyler(std::streambuf* dest) : dest_(dest) {}

protected:
    int_type overflow(int_type ch) override {
        if (traits_type::eq_int_type(ch, traits_type::eof()))
            return traits_type::not_eof(ch);
        const char c = traits_type::to_char_type(ch);
        if (c == '\n') emit(true);
        else           pending_.push_back(c);
        return ch;
    }

    std::streamsize xsputn(const char* s, std::streamsize n) override {
        for (std::streamsize i = 0; i < n; ++i) overflow(traits_type::to_int_type(s[i]));
        return n;
    }

    int sync() override {
        emit(false);
        return dest_->pubsync();
    }

private:
    void emit(bool newline) {
        std::string out = styleLine(pending_);
        if (newline) out.push_back('\n');
        if (!out.empty()) dest_->sputn(out.data(), static_cast<std::streamsize>(out.size()));
        pending_.clear();
    }

    std::streambuf* dest_;
    std::string     pending_;
};

/// RAII helper: while alive (and colour is enabled), std::cout is coloured.
class ColorScope {
public:
    ColorScope() {
        if (!enabled()) return;
        original_ = std::cout.rdbuf();
        styler_   = new LineStyler(original_);
        std::cout.rdbuf(styler_);
    }
    ~ColorScope() {
        if (!styler_) return;
        std::cout.flush();
        std::cout.rdbuf(original_);
        delete styler_;
    }
    ColorScope(const ColorScope&)            = delete;
    ColorScope& operator=(const ColorScope&) = delete;

private:
    std::streambuf* original_ = nullptr;
    LineStyler*     styler_   = nullptr;
};

}  // namespace term

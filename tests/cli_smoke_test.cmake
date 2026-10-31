# CLI smoke test driver.
#
# Runs the Orchestrix executable with a scripted stdin and checks:
#   1. the process exits with status 0,
#   2. the expected output marker is present.
#
# Invoked by CTest with:
#   -DCLI=<path to orchestrix executable>
#   -DSCENARIO=<exit|eof|single|compare>
#   -DINPUT=<path to a writable input file>

if(NOT DEFINED CLI OR NOT DEFINED SCENARIO OR NOT DEFINED INPUT)
    message(FATAL_ERROR "cli_smoke_test.cmake: CLI, SCENARIO and INPUT must be defined")
endif()

if(SCENARIO STREQUAL "exit")
    set(INPUT_TEXT "8\n")
    set(EXPECT_MARKER "Goodbye!")
elseif(SCENARIO STREQUAL "eof")
    # Empty stdin: the CLI must terminate instead of looping on a failed read.
    set(INPUT_TEXT "")
    set(EXPECT_MARKER "Input closed. Goodbye!")
elseif(SCENARIO STREQUAL "single")
    set(INPUT_TEXT "1\nQ\n8\n")
    set(EXPECT_MARKER "ORCHESTRIX SIMULATION")
elseif(SCENARIO STREQUAL "compare")
    set(INPUT_TEXT "4\n8\n")
    set(EXPECT_MARKER "Strategy Comparison Report")
else()
    message(FATAL_ERROR "cli_smoke_test.cmake: unknown scenario '${SCENARIO}'")
endif()

file(WRITE "${INPUT}" "${INPUT_TEXT}")

execute_process(
    COMMAND "${CLI}"
    INPUT_FILE "${INPUT}"
    OUTPUT_VARIABLE cli_stdout
    ERROR_VARIABLE  cli_stderr
    RESULT_VARIABLE cli_result
    TIMEOUT 30
)

if(NOT cli_result EQUAL 0)
    message(FATAL_ERROR
        "CLI scenario '${SCENARIO}' failed with result '${cli_result}'.\n"
        "stderr:\n${cli_stderr}\n"
        "stdout (tail):\n${cli_stdout}")
endif()

string(FIND "${cli_stdout}" "${EXPECT_MARKER}" marker_pos)
if(marker_pos EQUAL -1)
    message(FATAL_ERROR
        "CLI scenario '${SCENARIO}' did not contain expected marker "
        "'${EXPECT_MARKER}'.\nstdout:\n${cli_stdout}")
endif()

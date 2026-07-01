Local YarnSpinner harness for stage-1 development.

Build with:
  build.bat

Run modes:
  1) YarnC + CSV:
     yarnspinner_harness.exe assets\sample_project\out\main.yarnc assets\sample_project\out\main-Lines.csv --interactive

  2) Native YSA1:
     yarnspinner_harness.exe assets\sample_project\out\main.ysa --interactive

Options:
  --interactive, -i   Enable manual option selection in the console
  --start <node>      Start execution from a specific node (default: Start)
  --lines <path>      Override lines CSV path for .yarnc input

The sample story now includes multiple nodes for manual branching tests.

# Maintenance schedule template

## Purpose
Provide a repeatable workflow for contributors working on four-cell battery-management thermal fault guard.

## Repository entry points
- Core: `firmware/bms_state_machine.c`
- Supporting tool: `scripts/thermal_fault_sim.py`
- Existing project instructions: `README.md`

## Procedure
1. Start from a clean working copy and record the revision.
2. Check the prerequisites listed in the root README and the relevant build/configuration files.
3. Preserve original input data; write generated artifacts to an ignored or explicitly designated output directory.
4. Run the smallest relevant check before the full workflow.
5. Save the exact command, configuration, tool versions, and output summary.
6. Review warnings and failed checks; do not report a successful run if a required stage was skipped.

## Operational cautions
This repository is an engineering/research project. Simulation output is not automatically a certified safety assessment or a substitute for hardware validation. For physical deployment, obtain the appropriate engineering review and use verified limits.

## Record template
- Date/time and operator:
- Revision and environment:
- Input/configuration:
- Command:
- Result:
- Warnings/failures:
- Follow-up action:

# Input validation and malformed-record handling

## Project context
four-cell battery-management thermal fault guard. This scenario exercises the project's core behavior: cell voltage/temperature monitoring, coulomb counting, passive balancing, runaway injection simulation.

## Objective
Define a repeatable check for **input validation and malformed-record handling** without assuming unverified hardware behavior or undocumented numerical limits.

## Relevant implementation
- Primary implementation: `firmware/bms_state_machine.c`
- Supporting implementation: `scripts/thermal_fault_sim.py`
- Main engineering concerns: NTC conversion, sensor plausibility, overtemperature trips, dT/dt trips, balancing hysteresis, contactor-safe states.

## Procedure
1. Record the source revision, toolchain/runtime versions, and configuration.
2. Prepare a controlled input fixture and preserve its original bytes.
3. Execute the relevant function or command using the documented interface.
4. Capture raw input, output, diagnostics, and run metadata.
5. Repeat with the boundary or fault condition relevant to this scenario.
6. Compare the result with an independently reviewed expectation.

## Expected evidence
- Reproducible command or test harness invocation.
- Input fixture identifier and checksum.
- Observed output and diagnostic log.
- Pass/fail rationale tied to explicit acceptance criteria.

## Acceptance criteria
The behavior is deterministic for identical inputs, invalid data is handled according to the documented contract, and no safety-relevant failure is silently converted into a valid result.

## Open items
- Add measured acceptance limits when bench evidence is available.
- Link a checked-in fixture and automated test when implemented.
- Record unresolved assumptions rather than treating them as verified facts.

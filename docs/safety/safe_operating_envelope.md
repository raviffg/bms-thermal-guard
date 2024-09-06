# Safe operating envelope

## System
four-cell battery-management thermal fault guard

## Hazards and failure considerations
Relevant concerns include NTC conversion, sensor plausibility, overtemperature trips, dT/dt trips, balancing hysteresis, contactor-safe states. The exact severity and likelihood must be assessed for the intended hardware, environment, and operating procedure.

## Required controls
- Define operating limits from verified component specifications and bench evidence.
- Use independent protective mechanisms for hazardous energy or motion where applicable.
- Ensure sensor faults, stale measurements, numerical failures, and communication loss lead to a documented safe response.
- Record who is authorized to clear a latched fault and what inspection is required.
- Validate protective behavior using controlled fault injection before any real-world use.

## Evidence required
A hazard log, test record, reviewed limits, and named engineering sign-off are required before deployment. This note is not a certification or a claim that the current implementation meets a safety standard.

## Unresolved items
List hazards specific to the final hardware and close them through documented verification.

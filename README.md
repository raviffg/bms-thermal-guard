# bms-thermal-guard

4S 18650 pack BMS: Coulomb-counting SoC, passive bleed balancing, NTC
Steinhart-Hart temperature sensing, and a latching dT/dt runaway trip.

## Layout

    firmware/bms_state_machine.c   control loop, 1 kHz tick
    scripts/thermal_fault_sim.py   internal-short thermal model + trip latency
    data/ntc_runaway_injection.csv bench log, injected fault on cell 3

## Build

Cross-compiled for STM32G431. Not wired to a HAL in this repo on purpose -
`gpio_bleed_set()` / `gpio_mosfet_gate_set()` are the two hooks you implement.

## Notes

- Trip threshold is 1.2 C/s, latching. Clearing needs an operator ack and a
  pack under 45 C.
- Balancing only runs in CV/BALANCE. 20 mV enter, 8 mV exit hysteresis.
- Sim defaults (0.12 ohm short at 22 A) trip at ~1.5 s; the NTC bead lag is
  most of that latency, not the threshold.
- The bench log is a softer short: trip at 3.0 s, then the can keeps climbing
  because a contactor cannot interrupt an internal short.

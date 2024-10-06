#!/usr/bin/env python3
"""thermal_fault_sim.py

Lumped-thermal model of an internal short in one 18650 of a 4S pack.
Compares Joule heating (I^2 * R_int) against convective cooling, then runs the
firmware's dT/dt > 1.2 C/s rule against a first-order NTC to measure trip
latency.

Note the contactor only interrupts pack terminal current. An internal short
keeps heating the jelly roll after the trip, which is exactly what
data/ntc_runaway_injection.csv shows.

Run:  python thermal_fault_sim.py --r-int 0.12 --short-current 22
"""

import argparse

CELL_MASS_KG = 0.047          # 18650, ~47 g
CP_J_PER_KG_K = 830.0         # effective specific heat, NCA jelly roll
C_TH = CELL_MASS_KG * CP_J_PER_KG_K   # ~39 J/K per cell

H_CONV_W_PER_M2K = 12.0       # still air, natural convection
AREA_M2 = 0.0042              # lateral can area
T_AMBIENT_C = 24.0

DTD_T_TRIP = 1.2              # matches BMS_DTD_T_TRIP_C_PER_S
DT_S = 0.001                  # 1 kHz, same as the control loop
RATE_WINDOW_S = 0.25          # firmware finite-difference window
NTC_TAU_S = 0.8               # can -> NTC bead lag, measured on the rig


def simulate(r_int_ohm, short_current_a, cell_index, t_max_s=10.0):
    """Return (trip_time_s or None, peak_temp_c, rows)."""
    temps = [T_AMBIENT_C] * 4
    sensed = [T_AMBIENT_C] * 4
    rows = []
    q_joule = (short_current_a ** 2) * r_int_ohm

    t = 0.0
    t_trip = None
    step = 0
    hist = []                     # sensed temp history for the rate window

    while t < t_max_s:
        t += DT_S
        step += 1

        for i in range(4):
            # Neighbours pick up ~8% of the faulted cell's heat through the
            # busbar and holder. Rough, but it keeps the pack honest.
            q_in = q_joule if i == cell_index else q_joule * 0.08
            q_out = H_CONV_W_PER_M2K * AREA_M2 * (temps[i] - T_AMBIENT_C)
            temps[i] += (q_in - q_out) / C_TH * DT_S
            sensed[i] += (temps[i] - sensed[i]) * DT_S / NTC_TAU_S

        hist.append(sensed[cell_index])
        n_win = int(RATE_WINDOW_S / DT_S)
        if len(hist) > n_win:
            hist.pop(0)
        rate = (hist[-1] - hist[0]) / RATE_WINDOW_S if len(hist) == n_win else 0.0

        if t_trip is None and rate > DTD_T_TRIP:
            t_trip = t
            rows.append((t, sensed[cell_index], rate, True))

        if step % int(RATE_WINDOW_S / DT_S) == 0:
            rows.append((t, sensed[cell_index], rate, False))

        if t_trip is not None and t > t_trip + 0.5:
            break

    return t_trip, max(sensed), rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--r-int", type=float, default=0.12,
                    help="internal short resistance in ohms")
    ap.add_argument("--short-current", type=float, default=22.0,
                    help="fault current through the short, amps")
    ap.add_argument("--cell", type=int, default=2, help="faulted cell index 0-3")
    args = ap.parse_args()

    p_joule = (args.short_current ** 2) * args.r_int
    t_trip, peak, rows = simulate(args.r_int, args.short_current, args.cell)

    print(f"R_int={args.r_int:.3f} ohm  I_short={args.short_current:.1f} A  "
          f"P_joule={p_joule:.1f} W")
    print(f"adiabatic rate {p_joule / C_TH:.2f} C/s   faulted cell {args.cell}")
    print()
    print(f"{'t_s':>8} {'T_c':>8} {'dT/dt':>8}  trip")
    for t, temp, rate, fired in rows:
        print(f"{t:8.3f} {temp:8.2f} {rate:8.3f}  {'TRIP' if fired else ''}")

    print()
    if t_trip is None:
        print(f"NO TRIP within horizon. peak={peak:.2f} C")
        print("=> fault energy too low to cross 1.2 C/s; threshold never fires.")
    else:
        print(f"trip latency: {t_trip * 1000:.0f} ms  peak={peak:.2f} C")
        # NTC lag dominates. The can needs ~2 s to reach venting on a hard
        # short, so anything under that is margin we can spend.
        if t_trip < 2.0:
            print("PASS: trip inside the venting margin.")
        else:
            print("FAIL: latency too long, review threshold or NTC placement.")


if __name__ == "__main__":
    main()

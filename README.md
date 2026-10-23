# TCP/UDP Competing Traffic and QoS Study — ns-3.48

A network simulation study exploring competing TCP and UDP traffic over a shared bottleneck link, comparing baseline behavior with strict-priority Quality of Service (QoS).

## Overview

The simulation models a TCP sender and a UDP sender transmitting through a router to a destination server. Both flows share a constrained bottleneck link.

### Network configuration

- Simulator: ns-3.48
- Access links: 100 Mbps, 1 ms delay
- Bottleneck link: 5 Mbps, 10 ms delay
- Bottleneck queue: DropTail, 50 packets
- UDP offered rates: 500 Kbps, 2.5 Mbps, and 4.5 Mbps
- Application interval: 1–19 seconds

### QoS configuration

The QoS implementation uses the ns-3 `PrioQueueDisc` queue discipline with IP protocol classification:

- UDP (IP protocol 17): higher-priority band
- TCP (IP protocol 6): lower-priority band

## Repository structure

```text
.
├── scratch/
│   ├── competing-tcp-udp-study.cc
│   └── competing-tcp-udp-qos.cc
├── scripts/
│   └── generate-graphs.py
└── results/
    ├── csv/
    ├── flowmon/
    ├── graphs/
    ├── netanim/
    └── wireshark/
```

## Requirements

- ns-3.48 and its supported C++ build dependencies
- Python 3
- Matplotlib for generating graphs
- Wireshark for packet inspection (optional)
- NetAnim for viewing animation files (optional)

This repository contains the simulation source files and selected results, not the full ns-3 simulator.

## Running the simulations

Obtain and configure ns-3.48 according to its official installation instructions. Copy the two C++ files from this repository's `scratch/` directory into the `scratch/` directory of your ns-3.48 source tree.

From the ns-3.48 source directory, create the output directories:

```bash
mkdir -p results/wireshark results/netanim results/flowmon results/csv
```

Build and run the baseline implementation:

```bash
./ns3 build scratch/competing-tcp-udp-study

./ns3 run "scratch/competing-tcp-udp-study --scenario=combined --udpRate=500Kbps --prefix=combined-light"
```

Build and run the QoS implementation:

```bash
./ns3 build scratch/competing-tcp-udp-qos

./ns3 run "scratch/competing-tcp-udp-qos --scenario=combined --udpRate=4.5Mbps --prefix=qos-prioritized"
```

Create the output directories before running a simulation. The programs generate CSV measurements, FlowMonitor XML, NetAnim XML, and packet captures.

## Regenerating graphs

From the root of this repository, install Matplotlib if needed:

```bash
python3 -m pip install matplotlib
```

Run:

```bash
python3 scripts/generate-graphs.py
```

The script reads the scenario CSV files from `results/csv/` and generates SVG plots in `results/graphs/`. The comparison summary is saved to `results/csv/comparison-summary.csv`.

## Included artifacts

- `results/csv/`: per-scenario measurements and comparison summary
- `results/graphs/`: TCP completion time, TCP goodput, UDP delivery ratio, UDP loss ratio, and UDP mean delay plots
- `results/wireshark/`: selected bottleneck-link packet captures
- `results/flowmon/`: selected FlowMonitor XML output
- `results/netanim/`: selected NetAnim animation XML output

## Observed behavior

The included heavy-load baseline result shows degraded UDP delivery under contention. In the included QoS-prioritized result, UDP delivery reaches 100% and mean delay is lower than in the heavy-load baseline, while TCP throughput decreases.

This illustrates the trade-off associated with strict-priority scheduling: improving service for one traffic class can reduce the resources available to another.

Use the included CSV files as the source of truth for recorded measurements. Results may vary with simulator configuration and environment.

## License

No license has been selected yet. Add an appropriate license before distributing the project under explicit open-source terms.

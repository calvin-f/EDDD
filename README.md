# EDDD: Distributed DDoS Dataset Generator

EDDD is a tool designed to create distributed DDoS datasets. It incorporates both legitimate and malicious traffic
types, providing a robust dataset for distributed DDoS detection.

## Features

- Generates distributed DDoS datasets.
- Incorporates legitimate traffic based on [CIC flow data](https://www.kaggle.com/datasets/devendra416/ddos-datasets).
- Incorporates SYN flood traffic based on the [StopDDoS dataset](https://github.com/StopDDoS/packet-captures).
- Incorporates ICMP flood traffic based on
  the [CAIDA UCSD "DDoS Attack 2007" dataset](https://www.caida.org/catalog/datasets/ddos-20070804_dataset).
- Configurable network topology using connected countries

## Installation

To install EDDD, simply use the included installation script:

```bash
./install.sh
```

This script installs ns3 along with the required packages and loads the EDDD module into it.

## Configuration

Configuration of EDDD is done through the `input-config.json` file. This file allows you to parameterize the dataset
generation process. Here are the configurable properties:

- `countries`: (object) Country with the following properties:
    - `name`: (string) Name of the country.
    - `nodes`: (number) Number of backbone nodes.
    - `area`: (number) Area of the country in sqkm.
    - `population`: (number) Population of the country.
    - `enablePcap`: (boolean) Boolean value indicating whether pcap recording is enabled for backbone nodes of the
      country.
    - `attackTrafficFactor`: (number) A value indicating how much of the clients produce attack traffic in this country.
    - `neighbors`: (array of country objects) A list of neighboring countries (without loops).


- `backboneLatencyFactor`: (number) The factor for the backbone network latency compared to optimal conditions.
- `regionalLatencyFactor`: (number) The factor for the regional network latency to optimal conditions.
- `localLatencyFactor`: (number) The factor for the local network latency to optimal conditions.


- `backboneBandwidthGbps`: (number) The bandwidth between backbone routers in gbps.
- `regionalBandwidthGbps`: (number) The bandwidth between regional routers in gbps.
- `localBandwidthGbps`: (number) The bandwidth between local routers in gbps.
- `clientBandwidthGbps`: (number) The bandwidth between local routers and their clients in gbps.


- `networkFactor`: (number) The factor of how many subnet routers of a parent node are represented.
- `clientFactor`: (number) The factor of how many clients of a local router are represented.


- `logPings`: (boolean) A flag indicating whether pings should be logged.
- `logGraphviz`: (boolean) A flag indicating whether Graphviz output should be logged.


- `removePcapPayload`: (boolean) A flag to control whether a minimized version of the dataset without payloads should be
  created


- `attackType`: (string) Type of the attack that should be executed (syn-flood or icmp-flood)


- `cores`: (number) Number of physical CPU cores that should be used

## Running

To run EDDD, simply use the included run script:

```bash
./run.sh
```

This outputs a `Dataset` folder with all the data recorded in the nodes and if enabled another folder
called `ShortDatasets` where the payload from the packets is removed.

## Network Topology

The network topology used in EDDD is defined in the `input-config.json` file. Here is how it works:

- **Countries**: The simulation comprises various countries defined in the `input-config.json`. Each country adopts a
  multi-layer network structure.

- **Backbone Layer**: This layer represents the backbone network of a country. The backbone routers are distributed
  across the country, connecting different regions within the country, as well as providing connections to neighboring
  countries.

- **Regional Layer**: Each backbone router has a subnet of regional routers connected to it. These routers manage
  traffic within specific regions of the country.

- **Local Layer**: Connected to each regional router is a subnet of local routers. These routers manage traffic within
  smaller, local areas within a region.

- **Clients**: Each local router connects to multiple clients. These clients generate either attack traffic or
  legitimate traffic, as defined in the `input-config.json` file.

- **Target**: The target of the DDoS attack is always located in the first country listed in the `input-config.json`
  file. This target could potentially receive traffic from clients across all countries and regions included in the
  simulation.

This multi-layered, geographically distributed approach allows EDDD to simulate complex, realistic network conditions
for generating distributed DDoS datasets.

## Contribution

Contributing to EDDD requires an understanding of NS3 due to its inherent complexity.
However, the modular design of EDDD promotes extensibility, allowing for the addition of new traffic types.

Currently, EDDD is tailored towards specific datasets.
Adapting it to be more generic or configurable could extend its use-cases considerably.
Potential improvements include incorporating applications for diverse attack traffic, such as IP spoofing and reflection
attacks.

If you wish to contribute, we recommend gaining a deep understanding of the existing codebase, focusing on the current
application design and the specific NS3 features utilized.

---

Made at UZH with ❤️
Enjoy using EDDD!
We look forward to seeing what you can achieve with the datasets generated by this tool.


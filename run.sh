#!/bin/bash

ns3_directory="ns-allinone-3.38/ns-3.38"

# Cleanup

rm -f -- Datasets/*.pcap
rm -f -- ShortDatasets/*.pcap

rm -f -- $ns3_directory/*.pcap
rm -rf $ns3_directory/Datasets
rm -rf $ns3_directory/ShortDatasets

# Read JSON file
json_file="input-config.json"
json_data=$(cat "$json_file")

# Validate JSON properties
network_factor=$(echo "$json_data" | jq -r '.networkFactor')
client_factor=$(echo "$json_data" | jq -r '.clientFactor')
backbone_latency_factor=$(echo "$json_data" | jq -r '.backboneLatencyFactor')
regional_latency_factor=$(echo "$json_data" | jq -r '.regionalLatencyFactor')
local_latency_factor=$(echo "$json_data" | jq -r '.localLatencyFactor')
backbone_bandwidth_gbps=$(echo "$json_data" | jq -r '.backboneBandwidthGbps')
regional_bandwidth_gbps=$(echo "$json_data" | jq -r '.regionalBandwidthGbps')
local_bandwidth_gbps=$(echo "$json_data" | jq -r '.localBandwidthGbps')
client_bandwidth_gbps=$(echo "$json_data" | jq -r '.clientBandwidthGbps')
attack_type=$(echo "$json_data" | jq -r '.attackType')

validationSuccessful=true

if [ -z "$network_factor" ] || (($(bc <<<"$network_factor <= 0"))) || (($(bc <<<"$network_factor > 1"))); then
  validationSuccessful=false
  echo "Property networkFactor must be in the range (0,1]"
fi

if [ -z "$client_factor" ] || (($(bc <<<"$client_factor <= 0"))) || (($(bc <<<"$client_factor > 1"))); then
  echo "Property clientFactor must be in the range (0,1]"
fi

if [ -z "$backbone_latency_factor" ] || (($(bc <<<"$backbone_latency_factor < 1"))); then
  validationSuccessful=false
  echo "Property backboneLatencyFactor must be >= 1"
fi

if [ -z "$regional_latency_factor" ] || (($(bc <<<"$regional_latency_factor < 1"))); then
  validationSuccessful=false
  echo "Property regionalLatencyFactor must be >= 1"
fi

if [ -z "$local_latency_factor" ] || (($(bc <<<"$local_latency_factor < 1"))); then
  validationSuccessful=false
  echo "Property localLatencyFactor must be >= 1"
fi

if [ -z "$backbone_bandwidth_gbps" ] || (($(bc <<<"$backbone_bandwidth_gbps <= 0"))); then
  validationSuccessful=false
  echo "Property backboneBandwidthGbps must be > 0"
fi

if [ -z "$regional_bandwidth_gbps" ] || (($(bc <<<"$regional_bandwidth_gbps <= 0"))); then
  validationSuccessful=false
  echo "Property regionalBandwidthGbps must be > 0"
fi

if [ -z "$local_bandwidth_gbps" ] || (($(bc <<<"$local_bandwidth_gbps <= 0"))); then
  validationSuccessful=false
  echo "Property localBandwidthGbps must be > 0"
fi

if [ -z "$client_bandwidth_gbps" ] || (($(bc <<<"$client_bandwidth_gbps <= 0"))); then
  validationSuccessful=false
  echo "Property clientBandwidthGbps must be > 0"
fi

if [ -z "$attack_type" ] || { [ "$attack_type" != "syn-flood" ] && [ "$attack_type" != "icmp-flood" ]; }; then
  validationSuccessful=false
  echo "Property attackType must be either syn-flood or icmp-flood"
fi

countries=$(echo "$json_data" | jq -r '.countries')

validate_country() {
  local json="$1"

  # Validate the JSON object
  valid=$(echo "$json" | jq '
    . |
    has("name") and (.name | type == "string") and
    has("nodes") and (.nodes | type == "number") and
    has("area") and (.area | type == "number") and
    has("population") and (.population | type == "number") and
    has("enablePcap") and (.enablePcap | type == "boolean") and
    has("attackTrafficFactor") and (.attackTrafficFactor | type == "number") and
    has("neighbors") and (.neighbors | type == "array")
  ')

  if [ "$valid" == "true" ]; then
    nodes=$(echo "$json" | jq -r '.nodes')
    area=$(echo "$json" | jq -r '.area')
    population=$(echo "$json" | jq -r '.population')
    attackTrafficFactor=$(echo "$json" | jq -r '.attackTrafficFactor')

    if (($(bc <<<"$nodes < 1"))) || (($(bc <<<"$nodes % 1 != 0"))); then
      echo "Country property nodes is invalid. Make sure, it is a positive integer."
      validationSuccessful=false
    fi

    if (($(bc <<<"$area <= 0"))); then
      echo "Country property area is invalid. Make sure, it is a positive number."
      validationSuccessful=false
    fi

    if (($(bc <<<"$population <= 0"))) || (($(bc <<<"$population % 1 != 0"))); then
      echo "Country property population is invalid. Make sure, it is a positive integer."
      validationSuccessful=false
    fi

    if (($(bc <<<"$attackTrafficFactor < 0"))) || (($(bc <<<"$attackTrafficFactor > 1"))); then
      echo "Country property attackTrafficFactor is invalid. Make sure, it is a number in the range [0,1]."
      validationSuccessful=false
    fi

    if $validationSuccessful; then
      # Check each neighbor recursively
      neighbors=$(echo "$json" | jq -c '.neighbors | .[]')
      for neighbor in $neighbors; do
        validate_country "$neighbor"
      done
    fi
  else
    echo "Country property is incomplete. Make sure, the properties name, nodes, area, population, enablePcap, attackTrafficFactor and neighbors are present."
    validationSuccessful=false
  fi
}

validate_country "$countries"

log_pings=$(echo "$json_data" | jq -r '.logPings')
if [ -n "$log_pings" ]; then
  type=$(echo "$log_pings" | jq -r 'type')

  if [ "$type" != "boolean" ]; then
    log_pings=false
  fi
else
  log_pings=false
fi

log_graphviz=$(echo "$json_data" | jq -r '.logGraphviz')
if [ -n "$log_graphviz" ]; then
  type=$(echo "$log_graphviz" | jq -r 'type')

  if [ "$type" != "boolean" ]; then
    log_graphviz=false
  fi
else
  log_graphviz=false
fi

cores=$(echo "$json_data" | jq -r '.cores')
if [ -n "$cores" ]; then
  type=$(echo "$cores" | jq -r 'type')

  if [ "$type" != "number" ]; then
    cores=$(lscpu | grep "^Core(s) per socket:" | awk '{print $4}')
  fi
else
  cores=$(lscpu | grep "^Core(s) per socket:" | awk '{print $4}')
fi

remove_pcap_payload=$(echo "$json_data" | jq -r '.removePcapPayload')
if [ -n "$remove_pcap_payload" ]; then
  type=$(echo "$remove_pcap_payload" | jq -r 'type')

  if [ "$type" != "boolean" ]; then
    remove_pcap_payload=false
  fi
else
  remove_pcap_payload=false
fi

#./$ns3_directory/ns3 build &&
# Start the simulation if validation was successful
if [ "$validationSuccessful" = true ]; then
    ./$ns3_directory/ns3 run \
      "eddd \
  --networkFactor=$network_factor \
  --clientFactor=$client_factor \
  --backboneLatencyFactor=$backbone_latency_factor \
  --regionalLatencyFactor=$regional_latency_factor \
  --localLatencyFactor=$local_latency_factor \
  --backboneBandwidthGbps=$backbone_bandwidth_gbps \
  --regionalBandwidthGbps=$regional_bandwidth_gbps \
  --localBandwidthGbps=$local_bandwidth_gbps \
  --clientBandwidthGbps=$client_bandwidth_gbps \
  --logPings=$log_pings \
  --logGraphviz=$log_graphviz \
  --attackType=$attack_type \
  --country=$(echo "$countries" | jq -c)" \
      --command-template="mpiexec -np $cores %s"
else
  exit 1
fi

# Extract the datasets from the simulation

mkdir -p Datasets

mv -- $ns3_directory/*.pcap Datasets/

if [[ $attack_type == "syn-flood" ]]; then
  for file in "Datasets"/*.pcap; do
      # Check if file ends with .pcap
      if [[ $file == *.pcap ]]; then
          # Create the output file name by appending "_filtered" to the original file name
          output_file="${file%.pcap}_filtered.pcap"

          # Execute the tshark command to filter packets with reset flag
          tshark -r "$file" -Y "tcp.flags.reset==0" -w "$output_file"

          # rename to the old names
          rm "$file"
          mv "$output_file" "$file"
      fi
  done
fi

if [ "$remove_pcap_payload" == true ]; then
  mkdir -p ShortDatasets
  for file in Datasets/*.pcap; do
    filename=$(basename "$file")
    tshark -r Datasets/"$filename" -w ShortDatasets/"$filename" -Y "not tcp.payload"
  done
fi

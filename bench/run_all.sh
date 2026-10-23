#!/usr/bin/env bash
cd "$(dirname "$0")" || exit 1
./spawn_latency.sh
./pipeline_throughput.sh
./job_overhead.sh
./trace_overhead.sh

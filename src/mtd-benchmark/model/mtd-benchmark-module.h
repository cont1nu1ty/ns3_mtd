/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Moving Target Defense Performance Measurement Platform
 * 
 * Main module header that includes all MTD-benchmark components.
 * 
 * This module provides a complete proxy-switching MTD network architecture
 * for evaluating DDoS defense algorithms in NS-3 simulations.
 * 
 * Components:
 * - Attack Detection Layer (LocalDetector, CrossAgentDetector, GlobalDetector)
 * - Score Manager (risk scoring and classification)
 * - Domain Manager (domain operations: split/merge/migrate)
 * - Shuffle Controller (MTD proxy switching)
 * - Attack Generator (dynamic attack simulation)
 * - Export API (metrics and logging)
 * - Event Bus (inter-module communication)
 */

#ifndef MTD_BENCHMARK_MODULE_H
#define MTD_BENCHMARK_MODULE_H

// Core types and enums
#include "ns3/mtd-common.h"

// Packet tag for deterministic RCA attribution
#include "ns3/mtd-security-tag.h"

// Event bus for inter-module communication
#include "ns3/mtd-event-bus.h"

// Incremental event stream adapter (Python bridge)
#include "ns3/mtd-event-stream.h"

// Detection layer
#include "ns3/mtd-detector.h"

// Score manager
#include "ns3/mtd-score-manager.h"

// Domain manager
#include "ns3/mtd-domain-manager.h"

// Shuffle controller
#include "ns3/mtd-shuffle-controller.h"

// Attack generator
#include "ns3/mtd-attack-generator.h"

// Export API
#include "ns3/mtd-export-api.h"

// Helper classes
#include "ns3/mtd-network-helper.h"
#include "ns3/mtd-traffic-helper.h"
#include "ns3/mtd-analysis-helper.h"
#include "ns3/mtd-telemetry-helper.h"

#endif // MTD_BENCHMARK_MODULE_H

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
#include "mtd-common.h"

// Event bus for inter-module communication
#include "mtd-event-bus.h"

// Incremental event stream adapter (Python bridge)
#include "mtd-event-stream.h"

// Detection layer
#include "mtd-detector.h"

// Score manager
#include "mtd-score-manager.h"

// Domain manager
#include "mtd-domain-manager.h"

// Shuffle controller
#include "mtd-shuffle-controller.h"

// Attack generator
#include "mtd-attack-generator.h"

// Export API
#include "mtd-export-api.h"

#endif // MTD_BENCHMARK_MODULE_H

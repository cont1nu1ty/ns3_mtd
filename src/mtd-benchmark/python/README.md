# MTD-Benchmark Python Interface

Python bindings for NS-3 MTD-Benchmark module, enabling external defense algorithm integration.

## Quick Start

### Installation

```bash
# Build with Python bindings
cd /path/to/ns3
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNS3_EXAMPLES=ON
cmake --build build

# Set environment
export PYTHONPATH="/path/to/ns3/build/lib/python:$PYTHONPATH"
export LD_LIBRARY_PATH="/path/to/ns3/build/lib:$LD_LIBRARY_PATH"
```

### Basic Usage

```python
from mtd_benchmark_py import (
    DefenseAlgorithm, SimulationState, DefenseDecision,
    ShuffleMode, RiskLevel
)

class MyDefenseAlgorithm(DefenseAlgorithm):
    """Custom defense algorithm"""
    
    def __init__(self, threshold=0.6):
        super().__init__("MyDefense")
        self.threshold = threshold
    
    def evaluate(self, state: SimulationState) -> list:
        decisions = []
        
        for domain_id, domain in state.domains.items():
            avg_score = self._calculate_avg_score(domain, state)
            
            if avg_score > self.threshold:
                decisions.append(DefenseDecision.trigger_shuffle(
                    domain_id,
                    ShuffleMode.SCORE_DRIVEN,
                    f"High risk: {avg_score:.2f}"
                ))
        
        return decisions
    
    def _calculate_avg_score(self, domain, state):
        scores = [state.user_scores[uid].current_score 
                  for uid in domain.user_ids 
                  if uid in state.user_scores]
        return sum(scores) / len(scores) if scores else 0
```

## Components

| Component | Description |
|-----------|-------------|
| `DefenseAlgorithm` | Base class for defense algorithms |
| `ScoreCalculator` | Custom scoring algorithm interface |
| `RiskClassifier` | Custom risk classification interface |
| `ShuffleStrategy` | Custom proxy selection interface |
| `SimulationState` | Simulation state snapshot |
| `DefenseDecision` | Defense action specification |

## Decision Types

```python
# Trigger shuffle
DefenseDecision.trigger_shuffle(domain_id, ShuffleMode.RANDOM, "reason")

# Migrate user
DefenseDecision.migrate_user(user_id, new_domain_id, "reason")

# Update score
DefenseDecision.update_score(user_id, new_score, "reason")

# Change shuffle frequency
DefenseDecision.change_frequency(domain_id, new_freq, "reason")

# No action
DefenseDecision.no_action()
```

## File Structure

```
python/
├── __init__.py              # Package initialization
├── mtd_defense.py           # Base classes and utilities
├── custom_defense_example.py # Complete example
└── config_example.json      # Configuration template
```

## Documentation

See [PYTHON_INTERFACE_MANUAL.md](doc/PYTHON_INTERFACE_MANUAL.md) for complete documentation including:

- Environment setup
- API reference
- Development examples
- Advanced usage
- Troubleshooting

## Examples

### 1. Simple Threshold Algorithm

```python
from mtd_defense import SimpleThresholdAlgorithm

algorithm = SimpleThresholdAlgorithm(threshold=0.5)
decisions = algorithm.evaluate(state)
```

### 2. Adaptive Frequency

```python
from mtd_defense import AdaptiveFrequencyAlgorithm

algorithm = AdaptiveFrequencyAlgorithm(
    base_frequency=30.0,
    min_frequency=5.0,
    max_frequency=120.0
)
```

### 3. Custom Scorer

```python
from mtd_defense import ScoreCalculator

class MyScorer(ScoreCalculator):
    def calculate(self, user_id, observation, current_score):
        alpha = 0.3
        obs_score = 0.5 * observation.rate_anomaly + 0.5 * observation.pattern_anomaly
        return alpha * obs_score + (1 - alpha) * current_score
```

## License

MIT License

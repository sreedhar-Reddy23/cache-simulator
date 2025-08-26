# Cache Simulator

A configurable cache memory simulator implementing LRU replacement policy and WBWA (Write-Back + Write-Allocate) write policy.

## Features

- **Configurable Cache Parameters**: Block size, cache size, associativity
- **Multi-level Cache Hierarchy**: L1 and L2 cache support
- **LRU Replacement Policy**: Least Recently Used block replacement
- **WBWA Write Policy**: Write-Back + Write-Allocate
- **Two-step Block Allocation**: Prevents inconsistent cache states
- **Trace File Processing**: Reads memory access traces in `r|w <hex_address>` format
- **Comprehensive Output**: Configuration, cache contents, and detailed statistics

## Usage

```bash
make
./cache_simulator <BLOCKSIZE> <L1_SIZE> <L1_ASSOC> <L2_SIZE> <L2_ASSOC> <PREF_N> <PREF_M> <trace_file>
```

### Parameters

- `BLOCKSIZE`: Block size in bytes
- `L1_SIZE`: L1 cache size in bytes (0 = disabled)
- `L1_ASSOC`: L1 associativity
- `L2_SIZE`: L2 cache size in bytes (0 = disabled)
- `L2_ASSOC`: L2 associativity
- `PREF_N`: Number of stream buffers (0 = disabled)
- `PREF_M`: Blocks per stream buffer
- `trace_file`: Memory trace file

### Example

```bash
# L1-only cache: 32-byte blocks, 1KB size, 2-way associative
./cache_simulator 32 1024 2 0 0 0 0 trace.txt

# Two-level hierarchy: L1 (1KB, 2-way) + L2 (4KB, 4-way)
./cache_simulator 32 1024 2 4096 4 0 0 trace.txt
```

## Files

- `main.cpp`: Complete cache simulator implementation
- `Makefile`: Build configuration
- `run_experiment.sh`: Automated performance experiments
- `plot_results_simple.py`: Results visualization
- `analyze_results.py`: Performance analysis

## Cache Architecture

- **Block Structure**: Valid bit, dirty bit, tag
- **LRU Implementation**: Per-set lists tracking access order
- **Memory Hierarchy**: CPU → L1 → L2 → Memory
- **Address Format**: 32-bit hexadecimal (leading zeros optional)

## Performance Analysis

The simulator includes scripts for comprehensive performance analysis:

```bash
# Run cache size vs. associativity experiment
./run_experiment.sh

# Generate performance plots
python3 plot_results_simple.py

# Detailed analysis
python3 analyze_results.py
```

## Output Format

The simulator outputs:
1. **Configuration**: All cache parameters
2. **Cache Contents**: Valid blocks (MRU → LRU order)
3. **Statistics**: Hit/miss rates, memory traffic, and performance metrics

## Build Requirements

- C++17 compatible compiler
- GNU Make
- Python 3 (for analysis scripts)

## Author

**Sreedhar Reddy**  
Cache Simulator for Computer Architecture Analysis

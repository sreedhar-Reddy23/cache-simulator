# Cache Simulator

A configurable cache memory simulator implementing LRU replacement policy and WBWA (Write-Back + Write-Allocate) write policy.

## Features

- **Configurable Cache Parameters**: Block size, cache size, associativity
- **Multi-level Cache Hierarchy**: L1 and L2 cache support
- **LRU Replacement Policy**: Least Recently Used block replacement
- **WBWA Write Policy**: Write-Back + Write-Allocate
- **Two-step Block Allocation**: Prevents inconsistent cache states
- **Trace File Processing**: Reads memory access traces in `r|w <hex_address>` format
- **AAT Analysis**: Average Access Time calculations with configurable timing
- **Area Modeling**: Cache area estimation and performance/area trade-offs
- **Spatial/Temporal Locality Analysis**: Comprehensive access pattern analysis
- **Cache Pollution Detection**: Conflict vs capacity miss analysis
- **Automated Report Generation**: Professional analytical reports with visualizations
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

- `main.cpp`: Complete cache simulator with AAT and area analysis
- `Makefile`: Build configuration
- `run_experiment.sh`: Basic miss rate experiments
- `run_enhanced_experiment.sh`: Comprehensive AAT/area experiments
- `plot_results_simple.py`: Basic results visualization
- `generate_analytical_report.py`: Advanced analysis and reporting
- `analyze_results.py`: Legacy detailed analysis

## Cache Architecture

- **Block Structure**: Valid bit, dirty bit, tag
- **LRU Implementation**: Per-set lists tracking access order
- **Memory Hierarchy**: CPU → L1 → L2 → Memory
- **Address Format**: 32-bit hexadecimal (leading zeros optional)

## Performance Analysis

The simulator includes comprehensive performance analysis capabilities:

### **Basic Experiments**
```bash
# Run cache size vs. associativity experiment (miss rate analysis)
./run_experiment.sh

# Generate basic performance plots
python3 plot_results_simple.py
```

### **Enhanced Analysis** 
```bash
# Run enhanced experiments (AAT, area, trade-offs)
./run_enhanced_experiment.sh

# Generate comprehensive analytical reports
python3 generate_analytical_report.py
```

### **Analysis Features**
- **AAT Analysis**: Average Access Time vs cache configuration
- **Area Modeling**: Cache area estimation and efficiency metrics
- **Spatial Locality**: Sequential, stride, and random access pattern analysis
- **Temporal Locality**: Reuse distance and working set analysis
- **Cache Pollution**: Conflict vs capacity miss detection
- **Trade-off Analysis**: Performance vs area efficiency curves
- **Design Recommendations**: Optimal configuration suggestions

## Output Format

The simulator outputs:
1. **Configuration**: All cache parameters
2. **Cache Contents**: Valid blocks (MRU → LRU order)
3. **Statistics**: Hit/miss rates, memory traffic, and performance metrics
4. **Performance Metrics**: AAT, cache area, and efficiency ratios
5. **Comprehensive Analysis**: Spatial/temporal locality and pollution analysis

## Build Requirements

- C++17 compatible compiler
- GNU Make
- Python 3 (for analysis scripts)

## Author

**Sreedhar Reddy**  
Cache Simulator for Computer Architecture Analysis

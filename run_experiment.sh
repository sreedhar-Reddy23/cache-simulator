#!/bin/bash

# Cache Performance Experiment Script
# Benchmark: gcc_trace.txt
# L1 cache: SIZE varied (1KB to 1MB), ASSOC varied, BLOCKSIZE = 32
# L2 cache: None
# Prefetching: None

echo "Cache Performance Experiment"
echo "============================"
echo "Trace: gcc_trace.txt"
echo "Block Size: 32 bytes"
echo "L2 Cache: Disabled"
echo "Prefetching: Disabled"
echo ""

# Output file for results
OUTPUT_FILE="experiment_results.csv"
echo "log2_size,size_kb,associativity,miss_rate" > $OUTPUT_FILE

# Cache sizes in KB (powers of 2 from 1KB to 1MB)
SIZES_KB=(1 2 4 8 16 32 64 128 256 512 1024)

# Associativities to test
ASSOCS=(1 2 4 8)

echo "Running experiments..."
echo "Progress: [Size] x [Associativity]"

for size_kb in "${SIZES_KB[@]}"; do
    size_bytes=$((size_kb * 1024))
    
    # Calculate log2(size_kb) + 10 (since 1KB = 2^10 bytes)
    case $size_kb in
        1) log2_size=10 ;;
        2) log2_size=11 ;;
        4) log2_size=12 ;;
        8) log2_size=13 ;;
        16) log2_size=14 ;;
        32) log2_size=15 ;;
        64) log2_size=16 ;;
        128) log2_size=17 ;;
        256) log2_size=18 ;;
        512) log2_size=19 ;;
        1024) log2_size=20 ;;
    esac
    
    echo ""
    echo "Testing cache size: ${size_kb}KB (${size_bytes} bytes, log2=${log2_size})"
    
    for assoc in "${ASSOCS[@]}"; do
        echo -n "  Associativity ${assoc}... "
        
        # Run cache simulator
        ./cache_simulator 32 $size_bytes $assoc 0 0 0 0 gcc_trace.txt > temp_output.txt 2>&1
        
        # Extract miss rate from output
        miss_rate=$(grep "e. L1 miss rate:" temp_output.txt | awk '{print $5}')
        
        if [ -n "$miss_rate" ]; then
            echo "Miss Rate: $miss_rate"
            echo "$log2_size,$size_kb,$assoc,$miss_rate" >> $OUTPUT_FILE
        else
            echo "ERROR - Check temp_output.txt"
        fi
    done
    
    # Test fully-associative (associativity = total number of blocks)
    num_blocks=$((size_bytes / 32))
    echo -n "  Fully-associative (${num_blocks}-way)... "
    
    ./cache_simulator 32 $size_bytes $num_blocks 0 0 0 0 gcc_trace.txt > temp_output.txt 2>&1
    miss_rate=$(grep "e. L1 miss rate:" temp_output.txt | awk '{print $5}')
    
    if [ -n "$miss_rate" ]; then
        echo "Miss Rate: $miss_rate"
        echo "$log2_size,$size_kb,fully,$miss_rate" >> $OUTPUT_FILE
    else
        echo "ERROR - Check temp_output.txt"
    fi
done

echo ""
echo "Experiment complete! Results saved to: $OUTPUT_FILE"
echo ""
echo "Results summary:"
head -1 $OUTPUT_FILE
echo "..."
tail -5 $OUTPUT_FILE

# Clean up
rm -f temp_output.txt

echo ""
echo "You can now plot the data using the CSV file."
echo "X-axis: log2_size (10 to 20)"
echo "Y-axis: miss_rate"
echo "Series: associativity (1, 2, 4, 8, fully)"

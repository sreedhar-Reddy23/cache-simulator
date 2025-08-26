#!/usr/bin/env python3

import csv

# Read the experiment results
results = {}
with open('experiment_results.csv', 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        log2_size = int(row['log2_size'])
        assoc = row['associativity']
        miss_rate = float(row['miss_rate'])
        
        if log2_size not in results:
            results[log2_size] = {}
        results[log2_size][assoc] = miss_rate

# Create ASCII plot
print("Cache Performance Analysis - gcc_trace.txt")
print("=" * 70)
print("X-axis: log2(Cache Size) | Y-axis: Miss Rate")
print("Block Size: 32 bytes | No L2 | No Prefetching")
print("=" * 70)

# Print header
print(f"{'log2(Size)':>9} {'Size':>8} {'Direct':>8} {'2-way':>8} {'4-way':>8} {'8-way':>8} {'Fully':>8}")
print("-" * 70)

# Print data rows
size_labels = {10: '1KB', 11: '2KB', 12: '4KB', 13: '8KB', 14: '16KB', 
               15: '32KB', 16: '64KB', 17: '128KB', 18: '256KB', 19: '512KB', 20: '1MB'}

for log2_size in sorted(results.keys()):
    size_label = size_labels[log2_size]
    row_data = results[log2_size]
    
    print(f"{log2_size:>9} {size_label:>8} "
          f"{row_data.get('1', 0):.3f}    "
          f"{row_data.get('2', 0):.3f}    "
          f"{row_data.get('4', 0):.3f}    "
          f"{row_data.get('8', 0):.3f}    "
          f"{row_data.get('fully', 0):.3f}")

print("\nKey Findings:")
print("=" * 40)

# Calculate improvement from direct-mapped to fully-associative
improvements = []
for log2_size in sorted(results.keys()):
    direct = results[log2_size]['1']
    fully = results[log2_size]['fully']
    improvement = ((direct - fully) / direct) * 100
    improvements.append((size_labels[log2_size], improvement, direct, fully))

print("\nAssociativity Impact (Direct-mapped vs Fully-associative):")
print("-" * 60)
for size, improvement, direct, fully in improvements:
    print(f"{size:>6}: {direct:.3f} → {fully:.3f} ({improvement:+5.1f}% improvement)")

# Find optimal cache sizes
print("\nOptimal Cache Configurations:")
print("-" * 40)

# Find knee points where miss rate improvement slows down
print("Cache size recommendations based on diminishing returns:")

prev_improvement = float('inf')
for i, log2_size in enumerate(sorted(results.keys())[1:], 1):
    prev_log2 = sorted(results.keys())[i-1]
    
    # Use fully-associative as best case
    current_miss = results[log2_size]['fully']
    prev_miss = results[prev_log2]['fully']
    
    improvement = ((prev_miss - current_miss) / prev_miss) * 100
    
    if improvement < 10 and prev_improvement >= 10:  # Knee point
        print(f"  - Sweet spot around {size_labels[prev_log2]} (miss rate: {prev_miss:.3f})")
    
    prev_improvement = improvement

print(f"\nMinimum practical miss rate achieved: {results[20]['fully']:.3f} (at 1MB)")

# Create simple ASCII chart
print("\nVisual Representation (Direct-mapped):")
print("-" * 50)
print("Miss Rate |")
print("   0.20   |████████████████████████████████")
print("          |")
print("   0.15   |████████████████████████")
print("          |")
print("   0.10   |████████████████")
print("          |")
print("   0.05   |████████")
print("          |")
print("   0.00   |")
print("          └────────────────────────────────")
print("            1KB  4KB  16KB  64KB  256KB  1MB")
print("                 Cache Size (log scale)")

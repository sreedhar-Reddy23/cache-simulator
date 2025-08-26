#!/usr/bin/env python3

import matplotlib.pyplot as plt
import csv

# Read the experiment results
data = {'log2_size': [], 'size_kb': [], 'associativity': [], 'miss_rate': []}

with open('experiment_results.csv', 'r') as f:
    reader = csv.DictReader(f)
    for row in reader:
        data['log2_size'].append(int(row['log2_size']))
        data['size_kb'].append(int(row['size_kb']))
        data['associativity'].append(row['associativity'])
        data['miss_rate'].append(float(row['miss_rate']))

# Create the plot
plt.figure(figsize=(12, 8))

# Define colors and styles for each associativity
styles = {
    '1': {'color': 'red', 'marker': 'o', 'linestyle': '-', 'label': 'Direct-mapped (1-way)'},
    '2': {'color': 'blue', 'marker': 's', 'linestyle': '-', 'label': '2-way set-associative'},
    '4': {'color': 'green', 'marker': '^', 'linestyle': '-', 'label': '4-way set-associative'},
    '8': {'color': 'orange', 'marker': 'D', 'linestyle': '-', 'label': '8-way set-associative'},
    'fully': {'color': 'purple', 'marker': 'v', 'linestyle': '-', 'label': 'Fully-associative'}
}

# Organize data by associativity
assoc_data = {'1': [], '2': [], '4': [], '8': [], 'fully': []}

for i in range(len(data['log2_size'])):
    assoc = str(data['associativity'][i])
    log2_size = data['log2_size'][i]
    miss_rate = data['miss_rate'][i]
    assoc_data[assoc].append((log2_size, miss_rate))

# Sort data by log2_size for each associativity
for assoc in assoc_data:
    assoc_data[assoc].sort(key=lambda x: x[0])

# Plot each associativity curve
for assoc in ['1', '2', '4', '8', 'fully']:
    if assoc_data[assoc]:
        log2_sizes = [point[0] for point in assoc_data[assoc]]
        miss_rates = [point[1] for point in assoc_data[assoc]]
        style = styles[assoc]
        
        plt.plot(log2_sizes, miss_rates, 
                 color=style['color'], 
                 marker=style['marker'], 
                 linestyle=style['linestyle'],
                 linewidth=2, 
                 markersize=8,
                 label=style['label'])

# Customize the plot
plt.xlabel('log₂(Cache Size) [log₂(bytes)]', fontsize=14)
plt.ylabel('L1 Miss Rate', fontsize=14)
plt.title('L1 Cache Miss Rate vs. Cache Size for gcc_trace.txt\n(Block Size = 32 bytes, No L2 cache, No Prefetching)', fontsize=16)
plt.grid(True, alpha=0.3)
plt.legend(fontsize=12, loc='upper right')

# Set axis ranges and ticks
plt.xlim(9.5, 20.5)
plt.ylim(0, 0.25)
plt.xticks(range(10, 21), [f'{i}' for i in range(10, 21)])

# Add secondary x-axis with cache sizes
ax1 = plt.gca()
ax2 = ax1.twiny()
cache_sizes = ['1KB', '2KB', '4KB', '8KB', '16KB', '32KB', '64KB', '128KB', '256KB', '512KB', '1MB']
ax2.set_xlim(ax1.get_xlim())
ax2.set_xticks(range(10, 21))
ax2.set_xticklabels(cache_sizes)
ax2.set_xlabel('Cache Size', fontsize=14)

plt.tight_layout()

# Save the plot
plt.savefig('cache_miss_rate_analysis.png', dpi=300, bbox_inches='tight')
plt.savefig('cache_miss_rate_analysis.pdf', bbox_inches='tight')

print("Plot saved as 'cache_miss_rate_analysis.png' and 'cache_miss_rate_analysis.pdf'")

# Print summary statistics
print("\n" + "="*60)
print("EXPERIMENT SUMMARY")
print("="*60)
print(f"Trace file: gcc_trace.txt (100,000 memory accesses)")
print(f"Block size: 32 bytes")
print(f"Cache sizes: 1KB to 1MB (powers of 2)")
print(f"Associativities: 1, 2, 4, 8, fully-associative")
print()

print("RESULTS TABLE:")
print("-" * 50)
print("Cache Size | Direct | 2-way | 4-way | 8-way | Fully")
print("-" * 50)

# Organize results by cache size
cache_results = {}
for i in range(len(data['log2_size'])):
    size_kb = data['size_kb'][i]
    assoc = str(data['associativity'][i])
    miss_rate = data['miss_rate'][i]
    
    if size_kb not in cache_results:
        cache_results[size_kb] = {}
    cache_results[size_kb][assoc] = miss_rate

# Print results table
for size_kb in sorted(cache_results.keys()):
    results = cache_results[size_kb]
    print(f"{size_kb:8}KB | {results.get('1', 0):.3f} | {results.get('2', 0):.3f} | {results.get('4', 0):.3f} | {results.get('8', 0):.3f} | {results.get('fully', 0):.3f}")

print("\nKEY OBSERVATIONS:")
print("-" * 30)
print("1. Miss rate decreases significantly as cache size increases")
print("2. Higher associativity reduces conflict misses, especially for smaller caches")
print("3. Diminishing returns for very large caches (>256KB)")
print("4. Fully-associative provides best performance but with diminishing benefits")

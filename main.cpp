#include <iostream>
#include <string>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <list>
#include <unordered_map>
#include <unordered_set>
#include <fstream>
#include <sstream>
#include <iomanip>

// Cache block structure
struct CacheBlock {
    bool valid;
    bool dirty;      // For write-back policy: true if block has been written to
    unsigned long tag;
    
    CacheBlock() : valid(false), dirty(false), tag(0) {}
};

// Cache class to hold cache parameters and implement LRU policy
class Cache {
private:
    int block_size;
    int size;
    int associativity;
    int num_sets;
    int num_blocks;
    
    // Cache storage: vector of sets, each set contains blocks
    std::vector<std::vector<CacheBlock>> cache_sets;
    
    // LRU tracking: for each set, maintain a list of block indices in LRU order
    // Front = most recently used, Back = least recently used
    std::vector<std::list<int>> lru_lists;
    
    // Pointer to next level in memory hierarchy (L2 cache or nullptr for memory)
    Cache* next_level;

public:
    // Constructor
    Cache(int bs = 0, int s = 0, int assoc = 0, Cache* next = nullptr) 
        : block_size(bs), size(s), associativity(assoc), next_level(next) {
        if (is_enabled()) {
            num_blocks = size / block_size;
            num_sets = num_blocks / associativity;
            initialize_cache();
        } else {
            num_blocks = 0;
            num_sets = 0;
        }
    }
    
    // Set next level in memory hierarchy
    void set_next_level(Cache* next) {
        next_level = next;
    }
    
    // Calculate cache area based on configuration (simplified model)
    double calculate_area() const {
        if (!is_enabled()) return 0.0;
        
        // Simplified area model based on CACTI-like estimates
        // Area factors: tag array + data array + control logic
        
        // Basic area per bit (nm² per bit, converted to mm²)
        const double area_per_bit_nm2 = 0.05;  // 45nm technology node
        const double nm2_to_mm2 = 1e-12;
        
        // Calculate tag bits per block
        int tag_bits = 32 - static_cast<int>(log2(block_size)) - static_cast<int>(log2(num_sets));
        
        // Data array area
        double data_area = num_blocks * block_size * 8 * area_per_bit_nm2 * nm2_to_mm2;
        
        // Tag array area (including valid and dirty bits)
        double tag_area = num_blocks * (tag_bits + 2) * area_per_bit_nm2 * nm2_to_mm2;
        
        // Control logic overhead (proportional to associativity)
        double control_area = 0.1 * (data_area + tag_area) * (1.0 + 0.1 * associativity);
        
        return data_area + tag_area + control_area;
    }
    
    // Set timing parameters for AAT calculations
    void set_timing_parameters(int hit_cycles, int miss_penalty_cycles) {
        stats.hit_time = hit_cycles;
        stats.miss_penalty = miss_penalty_cycles;
        stats.area_mm2 = calculate_area();
    }
    
    // Getter methods
    int get_block_size() const { return block_size; }
    int get_size() const { return size; }
    int get_associativity() const { return associativity; }
    int get_num_sets() const { return num_sets; }
    int get_num_blocks() const { return num_blocks; }
    
    // Setter methods
    void set_block_size(int bs) { 
        block_size = bs; 
        recalculate_parameters();
    }
    void set_size(int s) { 
        size = s; 
        recalculate_parameters();
    }
    void set_associativity(int assoc) { 
        associativity = assoc; 
        recalculate_parameters();
    }
    
    // Check if cache is enabled
    bool is_enabled() const {
        return size > 0;
    }
    
    // Cache access methods
    bool access(unsigned long address, bool is_write = false) {
        if (!is_enabled()) return false;
        
        unsigned long block_addr = address / block_size;
        int set_index = block_addr % num_sets;
        unsigned long tag = block_addr / num_sets;
        
        // Check if tag exists in the set (HIT case)
        for (int i = 0; i < associativity; i++) {
            if (cache_sets[set_index][i].valid && 
                cache_sets[set_index][i].tag == tag) {
                
                // HIT: Update LRU order (move to most recently used)
                update_lru(set_index, i);
                
                // For write hits, mark block as dirty (write-back policy)
                if (is_write) {
                    cache_sets[set_index][i].dirty = true;
                }
                
                // Block remains valid (already was valid for a hit)
                // cache_sets[set_index][i].valid = true; // Already true
                
                return true; // HIT
            }
        }
        
        // MISS: Need to insert the block (write-allocate for both reads and writes)
        insert_block(set_index, tag, is_write);
        return false; // MISS
    }
    
    // Insert a new block using LRU replacement with proper two-step allocation
    void insert_block(int set_index, unsigned long tag, bool is_write = false) {
        int victim_way = get_lru_victim(set_index);
        
        // STEP 1: Make space for the requested block
        if (cache_sets[set_index][victim_way].valid) {
            // All blocks in set are valid, need to evict victim
            if (cache_sets[set_index][victim_way].dirty) {
                // Victim is dirty - must writeback to next level
                unsigned long victim_block_addr = cache_sets[set_index][victim_way].tag * num_sets + set_index;
                unsigned long victim_address = victim_block_addr * block_size;
                
                // Issue write request to next level
                if (next_level != nullptr) {
                    next_level->access(victim_address, true);
                } else {
                    // Write to main memory (no action needed in simulation)
                    handle_memory_write(victim_address);
                }
            }
            // Clear the victim block's state (it's being evicted)
            cache_sets[set_index][victim_way].valid = false;
            cache_sets[set_index][victim_way].dirty = false;
            cache_sets[set_index][victim_way].tag = 0;
        }
        
        // STEP 2: Bring in the requested block
        // Calculate original address from tag and set
        unsigned long requested_block_addr = tag * num_sets + set_index;
        unsigned long requested_address = requested_block_addr * block_size;
        
        // Issue read request to next level to bring in the block
        if (next_level != nullptr) {
            next_level->access(requested_address, false);
        } else {
            // Read from main memory (no action needed in simulation)
            handle_memory_read(requested_address);
        }
        
        // STEP 3: Install the new block and update all state
        cache_sets[set_index][victim_way].valid = true;        // Mark as valid
        cache_sets[set_index][victim_way].tag = tag;           // Set the tag
        cache_sets[set_index][victim_way].dirty = is_write;    // Mark dirty only if it's a write
        
        // STEP 4: Update LRU order (move newly installed block to most recently used)
        update_lru(set_index, victim_way);
    }
    
    // Handle memory operations (placeholder for actual memory interface)
    void handle_memory_write(unsigned long address) {
        // In a real implementation, this would interface with main memory
        // For simulation purposes, we just acknowledge the write
        stats.writebacks++;  // Count writebacks to memory
    }
    
    void handle_memory_read(unsigned long address) {
        // In a real implementation, this would interface with main memory
        // For simulation purposes, we just acknowledge the read
    }
    
    // Handle writeback of dirty block to next level (legacy method)
    void handle_writeback(int set_index, int way) {
        // This method is kept for backward compatibility
        // The new two-step allocation process handles writebacks directly
        cache_sets[set_index][way].dirty = false;
    }
    
    // Convenience methods for specific access types
    bool read(unsigned long address) {
        return access(address, false);
    }
    
    bool write(unsigned long address) {
        return access(address, true);
    }
    
    // Get statistics about dirty blocks
    int get_dirty_blocks_count() const {
        int count = 0;
        for (int set = 0; set < num_sets; set++) {
            for (int way = 0; way < associativity; way++) {
                if (cache_sets[set][way].valid && cache_sets[set][way].dirty) {
                    count++;
                }
            }
        }
        return count;
    }
    
    // Check if this cache has a next level
    bool has_next_level() const {
        return next_level != nullptr;
    }
    
    // Get cache level name for debugging
    std::string get_level_name() const {
        if (next_level == nullptr) {
            return "L2 (or Last Level)";
        } else if (next_level->has_next_level()) {
            return "L1";
        } else {
            return "L1";
        }
    }
    
    // Debug method to check block state
    void print_block_state(int set_index, int way) const {
        const auto& block = cache_sets[set_index][way];
        std::cout << "Block[" << set_index << "][" << way << "]: "
                  << "Valid=" << (block.valid ? "Y" : "N") << ", "
                  << "Dirty=" << (block.dirty ? "Y" : "N") << ", "
                  << "Tag=0x" << std::hex << block.tag << std::dec << std::endl;
    }
    
    // Debug method to print LRU order for a set
    void print_lru_order(int set_index) const {
        std::cout << "Set " << set_index << " LRU order (MRU->LRU): ";
        for (auto way : lru_lists[set_index]) {
            std::cout << way << " ";
        }
        std::cout << std::endl;
    }
    
    // Validate that all state is consistent
    bool validate_cache_state() const {
        for (int set = 0; set < num_sets; set++) {
            // Check LRU list has correct size
            if (lru_lists[set].size() != associativity) {
                return false;
            }
            
            // Check all ways are represented in LRU list
            std::vector<bool> way_present(associativity, false);
            for (int way : lru_lists[set]) {
                if (way < 0 || way >= associativity) return false;
                if (way_present[way]) return false; // Duplicate way
                way_present[way] = true;
            }
            
            // Check all ways are accounted for
            for (int way = 0; way < associativity; way++) {
                if (!way_present[way]) return false;
            }
        }
        return true;
    }
    
    // Get cache statistics
    struct CacheStats {
        unsigned long reads;
        unsigned long writes;
        unsigned long read_hits;
        unsigned long write_hits;
        unsigned long read_misses;
        unsigned long write_misses;
        unsigned long writebacks;
        
        // AAT (Average Access Time) parameters
        int hit_time;           // Cache hit time in cycles
        int miss_penalty;       // Miss penalty in cycles
        double area_mm2;        // Cache area in mm²
        
        CacheStats() : reads(0), writes(0), read_hits(0), write_hits(0), 
                       read_misses(0), write_misses(0), writebacks(0),
                       hit_time(1), miss_penalty(100), area_mm2(0.0) {}
                       
        double get_read_miss_rate() const {
            return reads > 0 ? (double)read_misses / reads : 0.0;
        }
        
        double get_write_miss_rate() const {
            return writes > 0 ? (double)write_misses / writes : 0.0;
        }
        
        double get_overall_miss_rate() const {
            unsigned long total = reads + writes;
            unsigned long total_misses = read_misses + write_misses;
            return total > 0 ? (double)total_misses / total : 0.0;
        }
        
        // Calculate Average Access Time (AAT)
        double get_aat() const {
            double miss_rate = get_overall_miss_rate();
            return hit_time + (miss_rate * miss_penalty);
        }
        
        // Calculate effective access time including write considerations
        double get_effective_aat() const {
            if (reads + writes == 0) return 0.0;
            
            double read_miss_rate = get_read_miss_rate();
            double write_miss_rate = get_write_miss_rate();
            double read_ratio = (double)reads / (reads + writes);
            double write_ratio = (double)writes / (reads + writes);
            
            // Weighted AAT considering read/write mix
            double read_aat = hit_time + (read_miss_rate * miss_penalty);
            double write_aat = hit_time + (write_miss_rate * miss_penalty);
            
            return (read_ratio * read_aat) + (write_ratio * write_aat);
        }
        
        // Performance per area metric
        double get_performance_per_area() const {
            if (area_mm2 <= 0.0) return 0.0;
            double aat = get_aat();
            return aat > 0.0 ? (1.0 / aat) / area_mm2 : 0.0;
        }
    };
    
    // Statistics tracking
    mutable CacheStats stats;
    
    // Print cache contents (blocks from MRU to LRU, only valid blocks)
    void print_cache_contents(const std::string& cache_name) const {
        std::cout << "===== " << cache_name << " contents =====" << std::endl;
        
        bool has_valid_blocks = false;
        
        for (int set = 0; set < num_sets; set++) {
            std::vector<std::pair<int, const CacheBlock*>> valid_blocks;
            
            // Collect valid blocks in LRU order (MRU first)
            for (int way : lru_lists[set]) {
                if (cache_sets[set][way].valid) {
                    valid_blocks.push_back({way, &cache_sets[set][way]});
                }
            }
            
            // Only print set if it has valid blocks
            if (!valid_blocks.empty()) {
                std::cout << "Set " << std::setfill(' ') << std::setw(3) << set << ":";
                
                for (const auto& block_pair : valid_blocks) {
                    const CacheBlock* block = block_pair.second;
                    std::cout << " " << std::hex << std::setfill('0') << std::setw(8) << block->tag;
                    if (block->dirty) {
                        std::cout << " D";
                    }
                }
                std::cout << std::dec << std::endl;
                has_valid_blocks = true;
            }
        }
        
        if (!has_valid_blocks) {
            std::cout << "Empty" << std::endl;
        }
        
        std::cout << std::endl;
    }
    
    // Access with statistics tracking
    bool access_with_stats(unsigned long address, bool is_write = false) {
        bool hit = access(address, is_write);
        
        // Update statistics
        if (is_write) {
            stats.writes++;
            if (hit) {
                stats.write_hits++;
            } else {
                stats.write_misses++;
            }
        } else {
            stats.reads++;
            if (hit) {
                stats.read_hits++;
            } else {
                stats.read_misses++;
            }
        }
        
        return hit;
    }
    
    // Get statistics
    const CacheStats& get_stats() const {
        return stats;
    }
    
    // Reset statistics
    void reset_stats() {
        stats = CacheStats();
    }
    
    // Get LRU victim (least recently used block)
    int get_lru_victim(int set_index) {
        // Return the block at the back of the LRU list (least recently used)
        return lru_lists[set_index].back();
    }
    
    // Update LRU order when a block is accessed
    void update_lru(int set_index, int way) {
        auto& lru_list = lru_lists[set_index];
        
        // Remove the way from its current position
        lru_list.remove(way);
        
        // Add it to the front (most recently used)
        lru_list.push_front(way);
    }
    
    // Validate cache configuration according to requirements
    bool is_valid_configuration() const {
        if (!is_enabled()) return true; // Disabled cache is valid
        
        // Check if block size is power of 2
        if (!is_power_of_two(block_size)) {
            return false;
        }
        
        // Check if number of sets is power of 2
        if (!is_power_of_two(num_sets)) {
            return false;
        }
        
        // Check basic constraints
        if (block_size <= 0 || size <= 0 || associativity <= 0) {
            return false;
        }
        
        // Check if associativity doesn't exceed total blocks
        if (associativity > num_blocks) {
            return false;
        }
        
        // Check if size is divisible by block_size
        if (size % block_size != 0) {
            return false;
        }
        
        // Check if num_blocks is divisible by associativity
        if (num_blocks % associativity != 0) {
            return false;
        }
        
        return true;
    }
    
    // Get configuration error message
    std::string get_config_error() const {
        if (!is_enabled()) return "";
        
        if (block_size <= 0) return "Block size must be positive";
        if (size <= 0) return "Cache size must be positive";
        if (associativity <= 0) return "Associativity must be positive";
        
        if (!is_power_of_two(block_size)) {
            return "Block size must be a power of 2";
        }
        
        if (size % block_size != 0) {
            return "Cache size must be divisible by block size";
        }
        
        if (num_blocks % associativity != 0) {
            return "Number of blocks must be divisible by associativity";
        }
        
        if (!is_power_of_two(num_sets)) {
            return "Number of sets must be a power of 2";
        }
        
        if (associativity > num_blocks) {
            return "Associativity cannot exceed total number of blocks";
        }
        
        return "";
    }

private:
    // Initialize cache storage and LRU structures
    void initialize_cache() {
        // Initialize cache sets
        cache_sets.clear();
        cache_sets.resize(num_sets, std::vector<CacheBlock>(associativity));
        
        // Initialize LRU lists
        lru_lists.clear();
        lru_lists.resize(num_sets);
        
        // Initialize LRU lists with way indices (0 to associativity-1)
        for (int set = 0; set < num_sets; set++) {
            for (int way = 0; way < associativity; way++) {
                lru_lists[set].push_back(way);
            }
        }
    }
    
    // Helper function to check if a number is power of 2
    bool is_power_of_two(int n) const {
        return n > 0 && (n & (n - 1)) == 0;
    }
    
    // Recalculate derived parameters when base parameters change
    void recalculate_parameters() {
        if (is_enabled()) {
            num_blocks = size / block_size;
            num_sets = num_blocks / associativity;
            initialize_cache();
        } else {
            num_blocks = 0;
            num_sets = 0;
            cache_sets.clear();
            lru_lists.clear();
        }
    }
};

// Trace file processing functions
struct TraceEntry {
    char operation;              // 'r' for read, 'w' for write
    unsigned long address;       // 32-bit hexadecimal address (0x00000000 to 0xFFFFFFFF)
    
    TraceEntry(char op, unsigned long addr) : operation(op), address(addr) {}
    
    // Helper to format address as 8-digit hex (showing leading zeros)
    std::string get_formatted_address() const {
        std::ostringstream oss;
        oss << std::hex << std::setfill('0') << std::setw(8) << address;
        return oss.str();
    }
};

// Parse a single line from trace file
bool parse_trace_line(const std::string& line, TraceEntry& entry) {
    std::istringstream iss(line);
    std::string op_str, addr_str;
    
    // Read operation and address
    if (!(iss >> op_str >> addr_str)) {
        return false; // Invalid format
    }
    
    // Validate operation
    if (op_str.length() != 1 || (op_str[0] != 'r' && op_str[0] != 'w')) {
        return false; // Invalid operation
    }
    entry.operation = op_str[0];
    
    // Parse hexadecimal address (32-bit, 8 hex digits)
    // Leading zeros may be omitted in trace file (e.g., "ffff" = "0000ffff")
    try {
        entry.address = std::stoul(addr_str, nullptr, 16);
        
        // Validate that address fits in 32 bits
        if (entry.address > 0xFFFFFFFF) {
            return false; // Address too large for 32-bit
        }
        
    } catch (const std::exception&) {
        return false; // Invalid address format
    }
    
    return true;
}

// Performance Analysis Class for comprehensive cache behavior evaluation
class PerformanceAnalyzer {
private:
    struct AccessPattern {
        std::vector<unsigned long> addresses;
        std::vector<char> operations;
        std::vector<double> timestamps;
        
        void add_access(unsigned long addr, char op, double time = 0.0) {
            addresses.push_back(addr);
            operations.push_back(op);
            timestamps.push_back(time);
        }
        
        void clear() {
            addresses.clear();
            operations.clear();
            timestamps.clear();
        }
    };
    
    AccessPattern pattern;
    
public:
    // Analyze spatial locality in access patterns
    struct SpatialLocalityStats {
        double sequential_ratio;        // Ratio of sequential accesses
        double stride_pattern_ratio;    // Ratio of regular stride patterns
        double random_access_ratio;     // Ratio of random accesses
        double avg_stride;              // Average stride distance
        int max_sequential_length;     // Longest sequential access sequence
        
        SpatialLocalityStats() : sequential_ratio(0.0), stride_pattern_ratio(0.0), 
                               random_access_ratio(0.0), avg_stride(0.0), max_sequential_length(0) {}
    };
    
    // Analyze temporal locality
    struct TemporalLocalityStats {
        double reuse_distance_avg;      // Average reuse distance
        double hit_rate_temporal;       // Hit rate due to temporal locality
        int unique_addresses;           // Number of unique addresses accessed
        double working_set_size;        // Estimated working set size
        
        TemporalLocalityStats() : reuse_distance_avg(0.0), hit_rate_temporal(0.0), 
                                unique_addresses(0), working_set_size(0.0) {}
    };
    
    // Cache pollution analysis
    struct PollutionStats {
        double pollution_rate;          // Rate of cache pollution
        double useful_data_ratio;       // Ratio of useful vs. polluting data
        int conflict_misses;            // Estimated conflict misses
        int capacity_misses;            // Estimated capacity misses
        
        PollutionStats() : pollution_rate(0.0), useful_data_ratio(0.0), 
                         conflict_misses(0), capacity_misses(0) {}
    };
    
    // Record access pattern for analysis
    void record_access(unsigned long address, char operation, double timestamp = 0.0) {
        pattern.add_access(address, operation, timestamp);
    }
    
    // Analyze spatial locality
    SpatialLocalityStats analyze_spatial_locality(int block_size = 32) const {
        SpatialLocalityStats stats;
        
        if (pattern.addresses.size() < 2) return stats;
        
        int sequential_count = 0;
        int stride_count = 0;
        int max_seq_length = 1;
        int current_seq_length = 1;
        long total_stride = 0;
        
        // Analyze access patterns
        for (size_t i = 1; i < pattern.addresses.size(); i++) {
            long stride = static_cast<long>(pattern.addresses[i]) - static_cast<long>(pattern.addresses[i-1]);
            total_stride += abs(stride);
            
            // Check for sequential access (within same block or next block)
            if (abs(stride) <= block_size) {
                sequential_count++;
                current_seq_length++;
            } else {
                max_seq_length = std::max(max_seq_length, current_seq_length);
                current_seq_length = 1;
            }
            
            // Check for regular stride patterns
            if (i >= 2) {
                long prev_stride = static_cast<long>(pattern.addresses[i-1]) - static_cast<long>(pattern.addresses[i-2]);
                if (stride == prev_stride && stride != 0) {
                    stride_count++;
                }
            }
        }
        
        max_seq_length = std::max(max_seq_length, current_seq_length);
        
        int total_accesses = pattern.addresses.size() - 1;
        stats.sequential_ratio = (double)sequential_count / total_accesses;
        stats.stride_pattern_ratio = (double)stride_count / std::max(total_accesses - 1, 1);
        stats.random_access_ratio = 1.0 - stats.sequential_ratio - stats.stride_pattern_ratio;
        stats.avg_stride = (double)total_stride / total_accesses;
        stats.max_sequential_length = max_seq_length;
        
        return stats;
    }
    
    // Analyze temporal locality
    TemporalLocalityStats analyze_temporal_locality() const {
        TemporalLocalityStats stats;
        
        if (pattern.addresses.empty()) return stats;
        
        std::unordered_map<unsigned long, std::vector<size_t>> address_positions;
        std::unordered_map<unsigned long, size_t> last_access;
        
        // Record positions of each address
        for (size_t i = 0; i < pattern.addresses.size(); i++) {
            address_positions[pattern.addresses[i]].push_back(i);
        }
        
        stats.unique_addresses = address_positions.size();
        
        // Calculate reuse distances
        double total_reuse_distance = 0.0;
        int reuse_count = 0;
        
        for (const auto& addr_positions : address_positions) {
            const auto& positions = addr_positions.second;
            for (size_t i = 1; i < positions.size(); i++) {
                total_reuse_distance += positions[i] - positions[i-1];
                reuse_count++;
            }
        }
        
        if (reuse_count > 0) {
            stats.reuse_distance_avg = total_reuse_distance / reuse_count;
        }
        
        // Estimate working set size (unique addresses in recent window)
        const size_t window_size = std::min((size_t)1000, pattern.addresses.size());
        std::unordered_set<unsigned long> recent_addresses;
        
        for (size_t i = pattern.addresses.size() - window_size; i < pattern.addresses.size(); i++) {
            recent_addresses.insert(pattern.addresses[i]);
        }
        
        stats.working_set_size = recent_addresses.size();
        
        return stats;
    }
    
    // Analyze cache pollution effects
    PollutionStats analyze_cache_pollution(const Cache& cache) const {
        PollutionStats stats;
        
        if (pattern.addresses.empty()) return stats;
        
        // Simulate cache behavior to identify pollution
        int block_size = cache.get_block_size();
        int num_sets = cache.get_num_sets();
        int associativity = cache.get_associativity();
        
        // Simple pollution analysis based on set conflicts
        std::unordered_map<int, std::vector<unsigned long>> set_accesses;
        
        for (unsigned long addr : pattern.addresses) {
            unsigned long block_addr = addr / block_size;
            int set_index = block_addr % num_sets;
            set_accesses[set_index].push_back(block_addr);
        }
        
        int total_conflicts = 0;
        int total_accesses = 0;
        
        for (const auto& set_access : set_accesses) {
            const auto& accesses = set_access.second;
            total_accesses += accesses.size();
            
            // Count potential conflicts (more unique blocks than associativity)
            std::unordered_set<unsigned long> unique_blocks(accesses.begin(), accesses.end());
            if (unique_blocks.size() > (size_t)associativity) {
                total_conflicts += accesses.size() - associativity;
            }
        }
        
        if (total_accesses > 0) {
            stats.pollution_rate = (double)total_conflicts / total_accesses;
            stats.useful_data_ratio = 1.0 - stats.pollution_rate;
        }
        
        stats.conflict_misses = total_conflicts;
        
        return stats;
    }
    
    // Generate comprehensive performance report
    void generate_performance_report(const Cache& l1_cache, const Cache& l2_cache, 
                                   const std::string& trace_name) const {
        
        std::cout << "\n" << std::string(80, '=') << std::endl;
        std::cout << "COMPREHENSIVE PERFORMANCE ANALYSIS REPORT" << std::endl;
        std::cout << std::string(80, '=') << std::endl;
        std::cout << "Trace: " << trace_name << std::endl;
        std::cout << "Total Accesses: " << pattern.addresses.size() << std::endl;
        std::cout << std::endl;
        
        // Cache Configuration Analysis
        std::cout << "CACHE CONFIGURATION ANALYSIS" << std::endl;
        std::cout << std::string(40, '-') << std::endl;
        
        const auto& l1_stats = l1_cache.get_stats();
        std::cout << "L1 Cache:" << std::endl;
        std::cout << "  Size: " << l1_cache.get_size() << " bytes" << std::endl;
        std::cout << "  Associativity: " << l1_cache.get_associativity() << "-way" << std::endl;
        std::cout << "  Block Size: " << l1_cache.get_block_size() << " bytes" << std::endl;
        std::cout << "  Area: " << std::fixed << std::setprecision(4) << l1_stats.area_mm2 << " mm²" << std::endl;
        std::cout << "  AAT: " << std::fixed << std::setprecision(2) << l1_stats.get_aat() << " cycles" << std::endl;
        std::cout << "  Performance/Area: " << std::scientific << std::setprecision(2) 
                  << l1_stats.get_performance_per_area() << " (1/cycles)/mm²" << std::endl;
        
        if (l2_cache.is_enabled()) {
            const auto& l2_stats = l2_cache.get_stats();
            std::cout << "\nL2 Cache:" << std::endl;
            std::cout << "  Size: " << l2_cache.get_size() << " bytes" << std::endl;
            std::cout << "  Associativity: " << l2_cache.get_associativity() << "-way" << std::endl;
            std::cout << "  Area: " << std::fixed << std::setprecision(4) << l2_stats.area_mm2 << " mm²" << std::endl;
            std::cout << "  AAT: " << std::fixed << std::setprecision(2) << l2_stats.get_aat() << " cycles" << std::endl;
        }
        
        // Spatial Locality Analysis
        auto spatial_stats = analyze_spatial_locality(l1_cache.get_block_size());
        std::cout << "\nSPATIAL LOCALITY ANALYSIS" << std::endl;
        std::cout << std::string(40, '-') << std::endl;
        std::cout << "Sequential Access Ratio: " << std::fixed << std::setprecision(3) 
                  << spatial_stats.sequential_ratio << std::endl;
        std::cout << "Stride Pattern Ratio: " << spatial_stats.stride_pattern_ratio << std::endl;
        std::cout << "Random Access Ratio: " << spatial_stats.random_access_ratio << std::endl;
        std::cout << "Average Stride: " << std::fixed << std::setprecision(1) 
                  << spatial_stats.avg_stride << " bytes" << std::endl;
        std::cout << "Max Sequential Length: " << spatial_stats.max_sequential_length << " accesses" << std::endl;
        
        // Temporal Locality Analysis
        auto temporal_stats = analyze_temporal_locality();
        std::cout << "\nTEMPORAL LOCALITY ANALYSIS" << std::endl;
        std::cout << std::string(40, '-') << std::endl;
        std::cout << "Unique Addresses: " << temporal_stats.unique_addresses << std::endl;
        std::cout << "Working Set Size: " << temporal_stats.working_set_size << " addresses" << std::endl;
        std::cout << "Average Reuse Distance: " << std::fixed << std::setprecision(1) 
                  << temporal_stats.reuse_distance_avg << " accesses" << std::endl;
        
        // Cache Pollution Analysis
        auto pollution_stats = analyze_cache_pollution(l1_cache);
        std::cout << "\nCACHE POLLUTION ANALYSIS" << std::endl;
        std::cout << std::string(40, '-') << std::endl;
        std::cout << "Pollution Rate: " << std::fixed << std::setprecision(3) 
                  << pollution_stats.pollution_rate << std::endl;
        std::cout << "Useful Data Ratio: " << pollution_stats.useful_data_ratio << std::endl;
        std::cout << "Estimated Conflict Misses: " << pollution_stats.conflict_misses << std::endl;
        
        // Performance Trade-offs
        std::cout << "\nPERFORMANCE TRADE-OFFS" << std::endl;
        std::cout << std::string(40, '-') << std::endl;
        std::cout << "Miss Rate vs. Area Trade-off:" << std::endl;
        std::cout << "  Miss Rate: " << std::fixed << std::setprecision(3) 
                  << l1_stats.get_overall_miss_rate() << std::endl;
        std::cout << "  Area Cost: " << l1_stats.area_mm2 << " mm²" << std::endl;
        std::cout << "  Performance Efficiency: " << std::scientific << std::setprecision(2) 
                  << (1.0 - l1_stats.get_overall_miss_rate()) / l1_stats.area_mm2 << " hit_rate/mm²" << std::endl;
        
        std::cout << std::endl;
    }
    
    // Clear recorded access patterns
    void clear() {
        pattern.clear();
    }
};

// Process trace file and simulate cache accesses
bool process_trace_file(const std::string& filename, Cache& l1_cache, Cache& l2_cache, 
                       PerformanceAnalyzer& analyzer, bool verbose = false) {
    std::ifstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Cannot open trace file '" << filename << "'" << std::endl;
        return false;
    }
    
    std::string line;
    int line_number = 0;
    unsigned long total_accesses = 0;
    
    std::cout << "Processing trace file: " << filename << std::endl;
    std::cout << "Note: All addresses are 32-bit (8 hex digits). Leading zeros may be omitted in trace file." << std::endl;
    
    while (std::getline(file, line)) {
        line_number++;
        
        // Skip empty lines and comments
        if (line.empty() || line[0] == '#') {
            continue;
        }
        
        TraceEntry entry('r', 0);
        if (!parse_trace_line(line, entry)) {
            std::cerr << "Warning: Invalid trace format at line " << line_number 
                      << ": '" << line << "'" << std::endl;
            continue;
        }
        
        // Show address interpretation for first few entries or if verbose
        if (verbose || total_accesses < 5) {
            std::cout << "Line " << line_number << ": " << entry.operation 
                      << " " << entry.get_formatted_address() 
                      << " (from: " << line << ")" << std::endl;
        }
        
        // Process the cache access
        bool is_write = (entry.operation == 'w');
        bool l1_hit = l1_cache.access_with_stats(entry.address, is_write);
        
        // Record access for performance analysis
        analyzer.record_access(entry.address, entry.operation, total_accesses);
        
        total_accesses++;
        
        // Optional: Print progress for large files
        if (total_accesses % 100000 == 0) {
            std::cout << "Processed " << total_accesses << " accesses..." << std::endl;
        }
    }
    
    file.close();
    
    std::cout << "Trace processing complete. Total accesses: " << total_accesses << std::endl;
    return true;
}

// Print cache statistics in the required format for ECE 463
void print_simulation_results(const Cache& l1_cache, const Cache& l2_cache) {
    std::cout << "===== Simulation results (raw) =====" << std::endl;
    
    const auto& l1_stats = l1_cache.get_stats();
    
    // L1 statistics
    std::cout << "a. number of L1 reads:        " << l1_stats.reads << std::endl;
    std::cout << "b. number of L1 read misses:  " << l1_stats.read_misses << std::endl;
    std::cout << "c. number of L1 writes:       " << l1_stats.writes << std::endl;
    std::cout << "d. number of L1 write misses: " << l1_stats.write_misses << std::endl;
    
    // L1 miss rate
    double l1_miss_rate = l1_stats.get_overall_miss_rate();
    std::cout << "e. L1 miss rate:              " << std::fixed << std::setprecision(6) << l1_miss_rate << std::endl;
    
    // L1 writebacks
    std::cout << "f. number of writebacks from L1: " << l1_stats.writebacks << std::endl;
    
    // L1 prefetches (always 0 for ECE 463)
    std::cout << "g. number of L1 prefetches:   0" << std::endl;
    
    if (l2_cache.is_enabled()) {
        const auto& l2_stats = l2_cache.get_stats();
        
        // L2 statistics
        std::cout << "h. number of L2 reads (demand): " << l2_stats.reads << std::endl;
        std::cout << "i. number of L2 read misses (demand): " << l2_stats.read_misses << std::endl;
        
        // L2 prefetch reads (always 0 for ECE 463)
        std::cout << "j. number of L2 reads (prefetch): 0" << std::endl;
        std::cout << "k. number of L2 read misses (prefetch): 0" << std::endl;
        
        std::cout << "l. number of L2 writes:       " << l2_stats.writes << std::endl;
        std::cout << "m. number of L2 write misses: " << l2_stats.write_misses << std::endl;
        
        // L2 miss rate (from CPU perspective)
        double l2_miss_rate = l2_stats.reads > 0 ? (double)l2_stats.read_misses / l2_stats.reads : 0.0;
        std::cout << "n. L2 miss rate:              " << std::fixed << std::setprecision(6) << l2_miss_rate << std::endl;
        
        std::cout << "o. number of writebacks from L2: " << l2_stats.writebacks << std::endl;
        
        // L2 prefetches (always 0 for ECE 463)
        std::cout << "p. number of L2 prefetches:   0" << std::endl;
        
        // Total memory traffic (with L2)
        unsigned long memory_traffic = l2_stats.read_misses + l2_stats.write_misses + l2_stats.writebacks;
        std::cout << "q. total memory traffic:      " << memory_traffic << std::endl;
        
    } else {
        // No L2 cache - print 0s for L2 stats
        std::cout << "h. number of L2 reads (demand): 0" << std::endl;
        std::cout << "i. number of L2 read misses (demand): 0" << std::endl;
        std::cout << "j. number of L2 reads (prefetch): 0" << std::endl;
        std::cout << "k. number of L2 read misses (prefetch): 0" << std::endl;
        std::cout << "l. number of L2 writes:       0" << std::endl;
        std::cout << "m. number of L2 write misses: 0" << std::endl;
        std::cout << "n. L2 miss rate:              0.000000" << std::endl;
        std::cout << "o. number of writebacks from L2: 0" << std::endl;
        std::cout << "p. number of L2 prefetches:   0" << std::endl;
        
        // Total memory traffic (without L2)
        unsigned long memory_traffic = l1_stats.read_misses + l1_stats.write_misses + l1_stats.writebacks;
        std::cout << "q. total memory traffic:      " << memory_traffic << std::endl;
    }
    
    std::cout << std::endl;
    
    // Additional performance metrics
    std::cout << "===== Performance Metrics =====" << std::endl;
    std::cout << "L1 Average Access Time:       " << std::fixed << std::setprecision(2) 
              << l1_stats.get_aat() << " cycles" << std::endl;
    std::cout << "L1 Cache Area:                " << std::fixed << std::setprecision(4) 
              << l1_stats.area_mm2 << " mm²" << std::endl;
    std::cout << "L1 Performance/Area:          " << std::scientific << std::setprecision(2) 
              << l1_stats.get_performance_per_area() << " (1/cycles)/mm²" << std::endl;
    
    if (l2_cache.is_enabled()) {
        const auto& l2_stats = l2_cache.get_stats();
        std::cout << "L2 Average Access Time:       " << std::fixed << std::setprecision(2) 
                  << l2_stats.get_aat() << " cycles" << std::endl;
        std::cout << "L2 Cache Area:                " << std::fixed << std::setprecision(4) 
                  << l2_stats.area_mm2 << " mm²" << std::endl;
        std::cout << "Total Cache Area:             " << std::fixed << std::setprecision(4) 
                  << (l1_stats.area_mm2 + l2_stats.area_mm2) << " mm²" << std::endl;
    }
    
    std::cout << std::endl;
}

// Helper function to create a sample trace file for testing
void create_sample_trace(const std::string& filename) {
    std::ofstream file(filename);
    if (file.is_open()) {
        file << "# Sample trace file demonstrating 32-bit address format\n";
        file << "# Leading zeros may be omitted\n";
        file << "r ffe04540\n";      // Full 8-digit address
        file << "r ffe04544\n";      // Full 8-digit address  
        file << "w eff2340\n";       // 7 digits (leading zero omitted)
        file << "r ffe04548\n";      // Full 8-digit address
        file << "w ffff\n";          // 4 digits (4 leading zeros omitted)
        file << "r 1000\n";          // 4 digits (4 leading zeros omitted)
        file << "w 1\n";             // 1 digit (7 leading zeros omitted)
        file << "r 0\n";             // Single zero
        file.close();
        std::cout << "Created sample trace file: " << filename << std::endl;
    }
}

int main(int argc, char* argv[]) {
    // Check if exactly 8 command-line arguments are provided
    if (argc != 9) {  // Program name + 8 arguments = 9 total
        std::cerr << "Usage: " << argv[0] << " <BLOCKSIZE> <L1_SIZE> <L1_ASSOC> <L2_SIZE> <L2_ASSOC> <PREF_N> <PREF_M> <trace_file>" << std::endl;
        std::cerr << std::endl;
        std::cerr << "Arguments:" << std::endl;
        std::cerr << "  BLOCKSIZE : Block size in bytes (positive integer)" << std::endl;
        std::cerr << "  L1_SIZE   : L1 cache size in bytes (positive integer)" << std::endl;
        std::cerr << "  L1_ASSOC  : L1 set-associativity (positive integer)" << std::endl;
        std::cerr << "  L2_SIZE   : L2 cache size in bytes (positive integer, 0 = no L2)" << std::endl;
        std::cerr << "  L2_ASSOC  : L2 set-associativity (positive integer)" << std::endl;
        std::cerr << "  PREF_N    : Number of Stream Buffers (positive integer, 0 = disabled)" << std::endl;
        std::cerr << "  PREF_M    : Number of memory blocks per Stream Buffer (positive integer)" << std::endl;
        std::cerr << "  trace_file: Full name of trace file" << std::endl;
        return 1;
    }

    // Parse command-line arguments
    int blocksize = std::atoi(argv[1]);
    int l1_size = std::atoi(argv[2]);
    int l1_assoc = std::atoi(argv[3]);
    int l2_size = std::atoi(argv[4]);
    int l2_assoc = std::atoi(argv[5]);
    int pref_n = std::atoi(argv[6]);
    int pref_m = std::atoi(argv[7]);
    std::string trace_file = argv[8];

    // Create cache structures with timing parameters
    Cache l2_cache(blocksize, l2_size, l2_assoc);  // L2 cache (next level = memory)
    Cache l1_cache(blocksize, l1_size, l1_assoc);  // L1 cache
    
    // Set up memory hierarchy: L1 -> L2 -> Memory
    if (l2_cache.is_enabled()) {
        l1_cache.set_next_level(&l2_cache);
        // L2's next level is memory (nullptr by default)
        l2_cache.set_timing_parameters(10, 100);  // L2: 10 cycle hit, 100 cycle miss penalty
    } else {
        // No L2 cache, L1 goes directly to memory (nullptr by default)
    }
    
    // Set L1 timing parameters
    l1_cache.set_timing_parameters(1, l2_cache.is_enabled() ? 10 : 100);  // L1: 1 cycle hit, miss penalty depends on L2
    
    // Create performance analyzer
    PerformanceAnalyzer analyzer;

    // Validate arguments using the cache validation methods
    if (!l1_cache.is_valid_configuration()) {
        std::cerr << "Error: Invalid L1 cache configuration - " << l1_cache.get_config_error() << std::endl;
        return 1;
    }
    
    if (!l2_cache.is_valid_configuration()) {
        std::cerr << "Error: Invalid L2 cache configuration - " << l2_cache.get_config_error() << std::endl;
        return 1;
    }
    
    if (pref_n < 0) {
        std::cerr << "Error: PREF_N must be a non-negative integer" << std::endl;
        return 1;
    }
    if (pref_m < 0) {
        std::cerr << "Error: PREF_M must be a non-negative integer" << std::endl;
        return 1;
    }
    if (pref_n > 0 && pref_m == 0) {
        std::cerr << "Error: PREF_M must be positive when PREF_N > 0" << std::endl;
        return 1;
    }

    // Print configuration for verification
    std::cout << "===== Simulator configuration =====" << std::endl;
    std::cout << "BLOCKSIZE:             " << l1_cache.get_block_size() << std::endl;
    std::cout << "L1_SIZE:               " << l1_cache.get_size() << std::endl;
    std::cout << "L1_ASSOC:              " << l1_cache.get_associativity() << std::endl;
    std::cout << "L2_SIZE:               " << l2_cache.get_size() << std::endl;
    std::cout << "L2_ASSOC:              " << l2_cache.get_associativity() << std::endl;
    std::cout << "PREF_N:                " << pref_n << std::endl;
    std::cout << "PREF_M:                " << pref_m << std::endl;
    std::cout << "trace_file:            " << trace_file << std::endl;
    std::cout << std::endl;

    // Process the trace file
    std::cout << "Starting cache simulation..." << std::endl;
    if (!process_trace_file(trace_file, l1_cache, l2_cache, analyzer)) {
        std::cerr << "Error: Failed to process trace file" << std::endl;
        return 1;
    }
    
    std::cout << std::endl;
    
    // Print final cache contents
    l1_cache.print_cache_contents("L1");
    if (l2_cache.is_enabled()) {
        l2_cache.print_cache_contents("L2");
    }
    
    // Print simulation results in required format
    print_simulation_results(l1_cache, l2_cache);
    
    // Generate comprehensive performance analysis report
    analyzer.generate_performance_report(l1_cache, l2_cache, trace_file);

    return 0;
}
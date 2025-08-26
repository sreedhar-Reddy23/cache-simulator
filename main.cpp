#include <iostream>
#include <string>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <list>
#include <unordered_map>
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
        
        CacheStats() : reads(0), writes(0), read_hits(0), write_hits(0), 
                       read_misses(0), write_misses(0), writebacks(0) {}
                       
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

// Process trace file and simulate cache accesses
bool process_trace_file(const std::string& filename, Cache& l1_cache, Cache& l2_cache, bool verbose = false) {
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

    // Create cache structures
    Cache l2_cache(blocksize, l2_size, l2_assoc);  // L2 cache (next level = memory)
    Cache l1_cache(blocksize, l1_size, l1_assoc);  // L1 cache
    
    // Set up memory hierarchy: L1 -> L2 -> Memory
    if (l2_cache.is_enabled()) {
        l1_cache.set_next_level(&l2_cache);
        // L2's next level is memory (nullptr by default)
    } else {
        // No L2 cache, L1 goes directly to memory (nullptr by default)
    }

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
    if (!process_trace_file(trace_file, l1_cache, l2_cache)) {
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

    return 0;
}
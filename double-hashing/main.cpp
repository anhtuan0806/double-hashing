#include <iostream>
#include <vector>
#include <random>
#include <chrono>
#include <fstream>
#include <string>
#include <iomanip>
#include <sstream>
#include <unordered_set>

enum SlotState { 
    EMPTY, 
    OCCUPIED, 
    DELETED 
};

template<typename K, typename V>
struct Entry {
    K key;
    V value;
    SlotState state;
    Entry() : state(EMPTY) {}
    Entry(const K& k, const V& v, SlotState s) : key(k), value(v), state(s) {}
};

struct StatResult {
    // đơn vị: microseconds
    long long insertTime;
    long long searchTime;
    long long deleteTime;
    double avgProbeSearchHit;
    double avgProbeSearchMiss;
    double avgProbeInsertAfterDelete;
};

struct HashStats {
    int totalProbesInsert = 0;
    int totalProbesSearch = 0;
    int totalProbesDelete = 0;
    int totalCollision = 0;
    int nInsert = 0;
    int nSearch = 0;
    int nDelete = 0;
};

namespace ClusterUtils {
    template<typename EntryType>
    static int maxClusterLength(const std::vector<EntryType>& table) {
        int maxLen = 0;
        int curLen = 0;
        for (const auto& entry : table) {
            if (entry.state == OCCUPIED) {
                ++curLen;
                maxLen = std::max(maxLen, curLen);
            } 
            else {
                curLen = 0;
            }
        }
        return maxLen;
    }

    template<typename EntryType>
    static double avgClusterLength(const std::vector<EntryType>& table) {
        int totalClusters = 0, totalLen = 0, curLen = 0;
        for (const auto& entry : table) {
            if (entry.state == OCCUPIED) {
                ++curLen;
            } 
            else {
                if (curLen > 0) {
                    ++totalClusters;
                    totalLen += curLen;
                    curLen = 0;
                }
            }
        }
        // Nếu bảng kết thúc bằng một cluster
        if (curLen > 0) {
            ++totalClusters;
            totalLen += curLen;
        }
        if (totalClusters == 0)
            return 0;
        else
            return (double)(totalLen) / totalClusters;
    }
};

namespace helper {

    // rng dùng chung cho toàn namespace, tránh sinh cùng seed liên tục
    inline std::mt19937& rng() {
        static thread_local std::mt19937 eng(static_cast<unsigned>(std::chrono::steady_clock::now().time_since_epoch().count()));
        return eng;
    }

    // Fisher–Yates shuffle
    template<typename T>
    void shuffle(std::vector<T>& vec) {
        for (int i = vec.size() - 1; i > 0; --i) {
            std::uniform_int_distribution<int> dist(0, i);
            int j = dist(rng());
            std::swap(vec[i], vec[j]);
        }
    }

    // Hàm iota thủ công 
    template<typename Iterator, typename T>
    void iota(Iterator begin, Iterator end, T value) {
        while (begin != end) {
            *begin++ = value++;
        }
    }

    // Kiểm tra số nguyên tố (đủ tốt cho n < 1e6)
    inline bool isPrime(int n) {
        if (n < 2) return false;
        for (int i = 2; i * i <= n; ++i) {
            if (n % i == 0) return false;
        }
        return true;
    }

    // Tìm số nguyên tố lớn hơn n
    inline int nextPrime(int n) {
        int x = n + 1;
        while (!isPrime(x)) ++x;
        return x;
    }

    // Double to string với precision tuỳ chỉnh
    inline std::string doubleToStr(double x, int precision = 2) {
        std::ostringstream oss;
        oss << std::fixed << std::setprecision(precision) << x;
        return oss.str();
    }

    // Ghi 1 dòng csv
    void writeCSVRow(std::ofstream& fout, const std::vector<std::string>& cols) {
        for (size_t i = 0; i < cols.size(); ++i) {
            fout << cols[i];
            if (i + 1 < cols.size()) fout << ",";
        }
        fout << "\n";
    }
}

// ======= Double Hashing Table =======
template<typename K, typename V>
class DoubleHashTable {
    int TABLE_SIZE;
    int keysPresent;
    int PRIME;
    std::vector<Entry<K, V>> hashTable;
    std::vector<bool> isPrimeArr;
public:
    HashStats stats;

    DoubleHashTable(int n) {
        TABLE_SIZE = n;
        keysPresent = 0;
        hashTable.assign(TABLE_SIZE, Entry<K, V>());

        // Khởi tạo mảng kiểm tra số nguyên tố
        isPrimeArr = std::vector<bool>(TABLE_SIZE, true);
        if (TABLE_SIZE >= 2) isPrimeArr[0] = isPrimeArr[1] = false;
        for (int i = 2; i * i < TABLE_SIZE; ++i) {
            if (isPrimeArr[i]) {
                for (int j = i * i; j < TABLE_SIZE; j += i) {
                    isPrimeArr[j] = false;
                }
            }
        }

        // Tìm số nguyên tố lớn nhất < TABLE_SIZE
        PRIME = TABLE_SIZE - 1;
        while (PRIME > 1 && !isPrimeArr[PRIME])
            PRIME--;
    }
    
    int hash1(const K& key) { 
        return std::hash<K>{}(key) % TABLE_SIZE; 
    }

    int hash2(const K& key) { 
        return PRIME - (std::hash<K>{}(key) % PRIME); 
    }

    bool isFull() {
        return keysPresent == TABLE_SIZE;
    }

    bool insert(const K& key, const V& value) {
        if (isFull()) return false;
        int probe = hash1(key);
        int offset = hash2(key);
        int probes = 1;
        if (hashTable[probe].state == OCCUPIED) 
            stats.totalCollision++;
        while (hashTable[probe].state == OCCUPIED && hashTable[probe].key != key) {
            probe = (probe + offset) % TABLE_SIZE;
            probes++;
        }
        if (hashTable[probe].state != OCCUPIED) {
            hashTable[probe].key = key;
            hashTable[probe].value = value;
            hashTable[probe].state = OCCUPIED;
            keysPresent++;
            stats.totalProbesInsert += probes;
            stats.nInsert++;
            return true;
        }
        else if (hashTable[probe].key == key) { 
            hashTable[probe].value = value;
            return true;
        }
        return false;
    }

    bool search(const K& key, V& outValue) {
        int probe = hash1(key);
        int offset = hash2(key);
        int initialPos = probe;
        int probes = 1;
        bool firstItr = true;
        while (true) {
            if (hashTable[probe].state == EMPTY) 
                break;
            if (hashTable[probe].state == OCCUPIED && hashTable[probe].key == key) {
                outValue = hashTable[probe].value;
                stats.totalProbesSearch += probes; 
                stats.nSearch++;
                return true;
            }
            if (probe == initialPos && !firstItr) 
                break;
            probe = (probe + offset) % TABLE_SIZE;
            probes++; 
            firstItr = false;
        }
        stats.totalProbesSearch += probes; 
        stats.nSearch++;
        return false;
    }

    void erase(const K& key) {
        int probe = hash1(key);
        int offset = hash2(key);
        int initialPos = probe;
        int probes = 1;
        bool firstItr = true;
        while (true) {
            if (hashTable[probe].state == EMPTY) { 
                stats.totalProbesDelete += probes; 
                stats.nDelete++; 
                return; 
            }
            if (hashTable[probe].state == OCCUPIED && hashTable[probe].key == key) {
                hashTable[probe].state = DELETED; 
                keysPresent--;
                stats.totalProbesDelete += probes; 
                stats.nDelete++; 
                return;
            }
            if (probe == initialPos && !firstItr) { 
                stats.totalProbesDelete += probes; 
                stats.nDelete++; 
                return; 
            }
            probe = (probe + offset) % TABLE_SIZE;
            probes++; 
            firstItr = false;
        }
    }

    int maxClusterLength() const {
        return ClusterUtils::maxClusterLength(hashTable);
    }

    double avgClusterLength() const {
        return ClusterUtils::avgClusterLength(hashTable);
    }
};

// ======= Linear Probing Table =======
template<typename K, typename V>
class LinearHashTable {
    int TABLE_SIZE;
    int keysPresent;
    std::vector<Entry<K, V>> hashTable;
public:
    HashStats stats;

    LinearHashTable(int n) {
        TABLE_SIZE = n;
        keysPresent = 0;
        hashTable.assign(TABLE_SIZE, Entry<K, V>());
    }

    int hash(const K& key) {
        return std::hash<K>{}(key) % TABLE_SIZE;
    }

    bool isFull() {
        return keysPresent == TABLE_SIZE;
    }

    bool insert(const K& key, const V& value) {
        if (isFull()) return false;
        int probe = hash(key);
        int probes = 1;
        if (hashTable[probe].state == OCCUPIED)
            stats.totalCollision++;
        while (hashTable[probe].state == OCCUPIED && hashTable[probe].key != key) {
            probe = (probe + 1) % TABLE_SIZE;
            probes++;
        }
        if (hashTable[probe].state != OCCUPIED) {
            hashTable[probe] = Entry<K, V>(key, value, OCCUPIED);
            keysPresent++;
            stats.totalProbesInsert += probes;
            stats.nInsert++;
            return true;
        } 
        else if (hashTable[probe].key == key) {
            hashTable[probe].value = value;
            return true;
        }
        return false;
    }

    bool search(const K& key, V& outValue) {
        int probe = hash(key);
        int probes = 1;
        while (hashTable[probe].state != EMPTY) {
            if (hashTable[probe].state == OCCUPIED && hashTable[probe].key == key) {
                outValue = hashTable[probe].value;
                stats.totalProbesSearch += probes;
                stats.nSearch++;
                return true;
            }
            probe = (probe + 1) % TABLE_SIZE;
            probes++;
        }
        stats.totalProbesSearch += probes;
        stats.nSearch++;
        return false;
    }

    void erase(const K& key) {
        int probe = hash(key);
        int probes = 1;
        while (hashTable[probe].state != EMPTY) {
            if (hashTable[probe].state == OCCUPIED && hashTable[probe].key == key) {
                hashTable[probe].state = DELETED;
                keysPresent--;
                stats.totalProbesDelete += probes;
                stats.nDelete++;
                return;
            }
            probe = (probe + 1) % TABLE_SIZE;
            probes++;
        }
        stats.totalProbesDelete += probes;
        stats.nDelete++;
    }

    int maxClusterLength() const {
        return ClusterUtils::maxClusterLength(hashTable);
    }

    double avgClusterLength() const {
        return ClusterUtils::avgClusterLength(hashTable);
    }
};

// ======= Quadratic Probing Table =======
template<typename K, typename V>
class QuadraticHashTable {
    int TABLE_SIZE;
    int keysPresent;
    std::vector<Entry<K, V>> hashTable;
public:
    HashStats stats;

    QuadraticHashTable(int n) {
        TABLE_SIZE = n;
        keysPresent = 0;
        hashTable.assign(TABLE_SIZE, Entry<K, V>());
    }

    int hash(const K& key) {
        return std::hash<K>{}(key) % TABLE_SIZE;
    }

    bool isFull() {
        return keysPresent == TABLE_SIZE;
    }

    bool insert(const K& key, const V& value) {
        if (isFull()) return false;
        int base = hash(key);
        int i = 0;
        int probes = 0;
        while (i < TABLE_SIZE) {
            int probe = (base + i * i) % TABLE_SIZE;
            probes++;
            if (i == 0 && hashTable[probe].state == OCCUPIED)
                stats.totalCollision++;
            if (hashTable[probe].state == EMPTY || hashTable[probe].state == DELETED) {
                hashTable[probe] = Entry<K, V>(key, value, OCCUPIED);
                keysPresent++;
                stats.totalProbesInsert += probes;
                stats.nInsert++;
                return true;
            } 
            else if (hashTable[probe].key == key) {
                hashTable[probe].value = value;
                return true;
            }
            i++;
        }
        return false;
    }

    bool search(const K& key, V& outValue) {
        int base = hash(key);
        int i = 0;
        int probes = 0;
        while (i < TABLE_SIZE) {
            int probe = (base + i * i) % TABLE_SIZE;
            probes++;
            if (hashTable[probe].state == EMPTY)
                break;
            if (hashTable[probe].state == OCCUPIED && hashTable[probe].key == key) {
                outValue = hashTable[probe].value;
                stats.totalProbesSearch += probes;
                stats.nSearch++;
                return true;
            }
            i++;
        }
        stats.totalProbesSearch += probes;
        stats.nSearch++;
        return false;
    }

    void erase(const K& key) {
        int base = hash(key);
        int i = 0;
        int probes = 0;
        while (i < TABLE_SIZE) {
            int probe = (base + i * i) % TABLE_SIZE;
            probes++;
            if (hashTable[probe].state == EMPTY)
                break;
            if (hashTable[probe].state == OCCUPIED && hashTable[probe].key == key) {
                hashTable[probe].state = DELETED;
                keysPresent--;
                stats.totalProbesDelete += probes;
                stats.nDelete++;
                return;
            }
            i++;
        }
        stats.totalProbesDelete += probes;
        stats.nDelete++;
    }

    int maxClusterLength() const {
        return ClusterUtils::maxClusterLength(hashTable);
    }

    double avgClusterLength() const {
        return ClusterUtils::avgClusterLength(hashTable);
    }
};

template<typename K, typename V>
class DynamicDoubleHashTable {
    int TABLE_SIZE;
    int keysPresent;
    int PRIME;
    std::vector<Entry<K, V>> hashTable;
    std::vector<bool> isPrimeArr;
    const double MAX_LOAD_FACTOR = 0.7;

public:
    HashStats stats;

    DynamicDoubleHashTable(int init_size = 101) {
        TABLE_SIZE = helper::nextPrime(init_size);
        keysPresent = 0;
        hashTable.assign(TABLE_SIZE, Entry<K, V>());
        precomputePrimes();
        PRIME = findLargestPrimeBelow(TABLE_SIZE);
    }

    int hash1(const K& key) const {
        return std::hash<K>{}(key) % TABLE_SIZE;
    }

    int hash2(const K& key) const {
        return PRIME - (std::hash<K>{}(key) % PRIME);
    }

    bool insert(const K& key, const V& value) {
        if (loadFactor() > MAX_LOAD_FACTOR) {
            rehash(TABLE_SIZE * 2);
        }

        int probe = hash1(key);
        int offset = hash2(key);
        int probes = 1;
        if (hashTable[probe].state == OCCUPIED)
            stats.totalCollision++;

        while (hashTable[probe].state == OCCUPIED && hashTable[probe].key != key) {
            probe = (probe + offset) % TABLE_SIZE;
            probes++;
        }

        if (hashTable[probe].state != OCCUPIED) {
            hashTable[probe] = Entry<K, V>(key, value, OCCUPIED);
            keysPresent++;
            stats.totalProbesInsert += probes;
            stats.nInsert++;
            return true;
        }
        else if (hashTable[probe].key == key) {
            hashTable[probe].value = value;
            return true;
        }
        return false;
    }

    bool search(const K& key, V& outValue) {
        int probe = hash1(key);
        int offset = hash2(key);
        int initialPos = probe;
        int probes = 1;
        bool firstItr = true;

        while (true) {
            if (hashTable[probe].state == EMPTY) break;
            if (hashTable[probe].state == OCCUPIED && hashTable[probe].key == key) {
                outValue = hashTable[probe].value;
                stats.totalProbesSearch += probes;
                stats.nSearch++;
                return true;
            }
            if (probe == initialPos && !firstItr) break;
            probe = (probe + offset) % TABLE_SIZE;
            probes++;
            firstItr = false;
        }
        stats.totalProbesSearch += probes;
        stats.nSearch++;
        return false;
    }

    void erase(const K& key) {
        int probe = hash1(key);
        int offset = hash2(key);
        int initialPos = probe;
        int probes = 1;
        bool firstItr = true;

        while (true) {
            if (hashTable[probe].state == EMPTY) {
                stats.totalProbesDelete += probes;
                stats.nDelete++;
                return;
            }
            if (hashTable[probe].state == OCCUPIED && hashTable[probe].key == key) {
                hashTable[probe].state = DELETED;
                keysPresent--;
                stats.totalProbesDelete += probes;
                stats.nDelete++;
                return;
            }
            if (probe == initialPos && !firstItr) {
                stats.totalProbesDelete += probes;
                stats.nDelete++;
                return;
            }
            probe = (probe + offset) % TABLE_SIZE;
            probes++;
            firstItr = false;
        }
    }

    double loadFactor() const {
        return static_cast<double>(keysPresent) / TABLE_SIZE;
    }

    void rehash(int new_size_hint) {
        int new_size = helper::nextPrime(new_size_hint);
        std::vector<Entry<K, V>> oldTable = hashTable;

        TABLE_SIZE = new_size;
        keysPresent = 0;
        hashTable.assign(TABLE_SIZE, Entry<K, V>());

        precomputePrimes();
        PRIME = findLargestPrimeBelow(TABLE_SIZE);

        for (const auto& entry : oldTable) {
            if (entry.state == OCCUPIED) {
                insert(entry.key, entry.value);
            }
        }
    }

    void precomputePrimes() {
        isPrimeArr.assign(TABLE_SIZE, true);
        if (TABLE_SIZE >= 2) isPrimeArr[0] = isPrimeArr[1] = false;
        for (int i = 2; i * i < TABLE_SIZE; ++i) {
            if (isPrimeArr[i]) {
                for (int j = i * i; j < TABLE_SIZE; j += i) {
                    isPrimeArr[j] = false;
                }
            }
        }
    }

    int findLargestPrimeBelow(int n) {
        for (int i = n - 1; i >= 2; --i) {
            if (isPrimeArr[i]) return i;
        }
        return 2;
    }

    int maxClusterLength() const {
        return ClusterUtils::maxClusterLength(hashTable);
    }

    double avgClusterLength() const {
        return ClusterUtils::avgClusterLength(hashTable);
    }

    int size() const {
        return TABLE_SIZE;
    }
};

template<typename K, typename V>
class DynamicLinearHashTable {
    int TABLE_SIZE;
    int keysPresent;
    std::vector<Entry<K, V>> hashTable;

    void rehash() {
        int newSize = helper::nextPrime(TABLE_SIZE * 2);
        std::vector<Entry<K, V>> oldTable = hashTable;
        TABLE_SIZE = newSize;
        keysPresent = 0;
        hashTable.assign(TABLE_SIZE, Entry<K, V>());

        for (const auto& entry : oldTable) {
            if (entry.state == OCCUPIED) {
                insert(entry.key, entry.value);
            }
        }
    }

public:
    HashStats stats;

    DynamicLinearHashTable(int initialSize = 17) {
        TABLE_SIZE = helper::nextPrime(initialSize);
        keysPresent = 0;
        hashTable.assign(TABLE_SIZE, Entry<K, V>());
    }

    double loadFactor() const {
        return 1.0 * keysPresent / TABLE_SIZE;
    }

    int hash(const K& key) const {
        return std::hash<K>{}(key) % TABLE_SIZE;
    }

    bool insert(const K& key, const V& value) {
        if (loadFactor() > 0.7)
            rehash();

        int probe = hash(key);
        int probes = 1;

        if (hashTable[probe].state == OCCUPIED)
            stats.totalCollision++;

        while (hashTable[probe].state == OCCUPIED && hashTable[probe].key != key) {
            probe = (probe + 1) % TABLE_SIZE;
            probes++;
        }

        if (hashTable[probe].state != OCCUPIED) {
            hashTable[probe] = Entry<K, V>(key, value, OCCUPIED);
            keysPresent++;
            stats.totalProbesInsert += probes;
            stats.nInsert++;
            return true;
        }
        else if (hashTable[probe].key == key) {
            hashTable[probe].value = value;
            return true;
        }

        return false;
    }

    bool search(const K& key, V& outValue) {
        int probe = hash(key);
        int probes = 1;

        while (hashTable[probe].state != EMPTY) {
            if (hashTable[probe].state == OCCUPIED && hashTable[probe].key == key) {
                outValue = hashTable[probe].value;
                stats.totalProbesSearch += probes;
                stats.nSearch++;
                return true;
            }
            probe = (probe + 1) % TABLE_SIZE;
            probes++;
        }

        stats.totalProbesSearch += probes;
        stats.nSearch++;
        return false;
    }

    void erase(const K& key) {
        int probe = hash(key);
        int probes = 1;

        while (hashTable[probe].state != EMPTY) {
            if (hashTable[probe].state == OCCUPIED && hashTable[probe].key == key) {
                hashTable[probe].state = DELETED;
                keysPresent--;
                stats.totalProbesDelete += probes;
                stats.nDelete++;
                return;
            }
            probe = (probe + 1) % TABLE_SIZE;
            probes++;
        }

        stats.totalProbesDelete += probes;
        stats.nDelete++;
    }

    int maxClusterLength() const {
        return ClusterUtils::maxClusterLength(hashTable);
    }

    double avgClusterLength() const {
        return ClusterUtils::avgClusterLength(hashTable);
    }

    int size() const {
        return TABLE_SIZE;
    }
};

template<typename K, typename V>
class DynamicQuadraticHashTable {
    int TABLE_SIZE;
    int keysPresent;
    std::vector<Entry<K, V>> hashTable;

    void rehash() {
        int newSize = helper::nextPrime(TABLE_SIZE * 2);
        std::vector<Entry<K, V>> oldTable = hashTable;
        TABLE_SIZE = newSize;
        keysPresent = 0;
        hashTable.assign(TABLE_SIZE, Entry<K, V>());

        for (const auto& entry : oldTable) {
            if (entry.state == OCCUPIED) {
                insert(entry.key, entry.value);
            }
        }
    }

public:
    HashStats stats;

    DynamicQuadraticHashTable(int initialSize = 17) {
        TABLE_SIZE = helper::nextPrime(initialSize);
        keysPresent = 0;
        hashTable.assign(TABLE_SIZE, Entry<K, V>());
    }

    double loadFactor() const {
        return 1.0 * keysPresent / TABLE_SIZE;
    }

    int hash(const K& key) const {
        return std::hash<K>{}(key) % TABLE_SIZE;
    }

    bool insert(const K& key, const V& value) {
        if (loadFactor() > 0.7)
            rehash();

        int base = hash(key);
        int i = 0;
        int probes = 0;

        while (i < TABLE_SIZE) {
            int probe = (base + i * i) % TABLE_SIZE;
            probes++;
            if (i == 0 && hashTable[probe].state == OCCUPIED)
                stats.totalCollision++;

            if (hashTable[probe].state == EMPTY || hashTable[probe].state == DELETED) {
                hashTable[probe] = Entry<K, V>(key, value, OCCUPIED);
                keysPresent++;
                stats.totalProbesInsert += probes;
                stats.nInsert++;
                return true;
            }
            else if (hashTable[probe].key == key) {
                hashTable[probe].value = value;
                return true;
            }
            i++;
        }

        return false;
    }

    bool search(const K& key, V& outValue) {
        int base = hash(key);
        int i = 0;
        int probes = 0;

        while (i < TABLE_SIZE) {
            int probe = (base + i * i) % TABLE_SIZE;
            probes++;
            if (hashTable[probe].state == EMPTY)
                break;
            if (hashTable[probe].state == OCCUPIED && hashTable[probe].key == key) {
                outValue = hashTable[probe].value;
                stats.totalProbesSearch += probes;
                stats.nSearch++;
                return true;
            }
            i++;
        }

        stats.totalProbesSearch += probes;
        stats.nSearch++;
        return false;
    }

    void erase(const K& key) {
        int base = hash(key);
        int i = 0;
        int probes = 0;

        while (i < TABLE_SIZE) {
            int probe = (base + i * i) % TABLE_SIZE;
            probes++;
            if (hashTable[probe].state == EMPTY)
                break;
            if (hashTable[probe].state == OCCUPIED && hashTable[probe].key == key) {
                hashTable[probe].state = DELETED;
                keysPresent--;
                stats.totalProbesDelete += probes;
                stats.nDelete++;
                return;
            }
            i++;
        }

        stats.totalProbesDelete += probes;
        stats.nDelete++;
    }

    int maxClusterLength() const {
        return ClusterUtils::maxClusterLength(hashTable);
    }

    double avgClusterLength() const {
        return ClusterUtils::avgClusterLength(hashTable);
    }

    int size() const {
        return TABLE_SIZE;
    }
};

namespace BenchmarkUtils {
    namespace getInput {
        int getTestSize() {
            int M;
            std::cout << "Enter the number of elements to test: ";
            std::cin >> M;
            return M;
        }

        double getUserLoadFactor() {
            double lf;
            std::cout << "Enter the first load factor to compare (e.g., 0.7): ";
            std::cin >> lf;
            std::cout << "Auto-selected optimal load factor: 0.5\n";
            return lf;
        }

        double getMissRate() {
            double missRate;
            std::cout << "Enter the miss rate for search operations (0-1): ";
            std::cin >> missRate;
            while (missRate < 0 || missRate > 1) {
                std::cout << "Invalid rate! Please enter a value between 0 and 1: ";
                std::cin >> missRate;
            }
            return missRate;
        }
    }

    namespace generator {

        // Hàm tiện ích tạo random engine, đảm bảo mỗi lần gọi đều random hóa
        inline std::mt19937& rng() {
            static thread_local std::mt19937 eng(std::chrono::steady_clock::now().time_since_epoch().count());
            return eng;
        }

        // Sinh M cặp (key, value) ngẫu nhiên, key không trùng
        // keyUpper: giá trị key lớn nhất có thể
        // valUpper: giá trị value lớn nhất có thể
        std::vector<std::pair<int, int>> generateRandomKeyVals(int M, int keyUpper, int valUpper = 1000000) {
            std::uniform_int_distribution<int> distKey(1, keyUpper);
            std::uniform_int_distribution<int> distVal(1, valUpper);

            std::unordered_set<int> used;
            std::vector<std::pair<int, int>> keyvals;
            while ((int)keyvals.size() < M) {
                int key = distKey(rng());
                if (used.count(key)) continue;
                used.insert(key);
                keyvals.emplace_back(key, distVal(rng()));
            }
            return keyvals;
        }

        // Sinh M cặp (key, value) tuần tự từ 1 tới M, value ngẫu nhiên
        std::vector<std::pair<int, int>> generateSequentialKeyVals(int M, int valUpper = 1000000) {
            std::uniform_int_distribution<int> distVal(1, valUpper);
            std::vector<std::pair<int, int>> keyvals;
            keyvals.reserve(M);
            for (int i = 1; i <= M; ++i)
                keyvals.emplace_back(i, distVal(rng()));
            return keyvals;
        }

        // Sinh M cặp (key, value) theo từng cụm liên tiếp, value ngẫu nhiên
        std::vector<std::pair<int, int>> generateClusteredKeyVals(int M, int keyUpper, int valUpper = 1000000) {
            std::uniform_int_distribution<int> distVal(1, valUpper);

            int numClusters = 5;
            int perCluster = M / numClusters;
            std::vector<std::pair<int, int>> keyvals;
            keyvals.reserve(M);
            int base = 1;
            for (int c = 0; c < numClusters; ++c) {
                for (int i = 0; i < perCluster && (int)keyvals.size() < M; ++i) {
                    keyvals.emplace_back(base + i, distVal(rng()));
                }
                base += keyUpper / numClusters;
            }
            while ((int)keyvals.size() < M) {
                keyvals.emplace_back(base++, distVal(rng()));
            }
            return keyvals;
        }

        // Sinh numMiss key chưa tồn tại trong existKeys
        std::vector<int> generateMissKeys(int numMiss, const std::unordered_set<int>& existKeys, int keyUpperBound) {
            std::unordered_set<int> used = existKeys; // copy
            std::vector<int> missKeys;
            std::uniform_int_distribution<int> distKey(1, keyUpperBound);

            while ((int)missKeys.size() < numMiss) {
                int key = distKey(rng());
                if (used.count(key)) continue;
                missKeys.push_back(key);
                used.insert(key);
            }
            return missKeys;
        }
    }

    /// Insert toàn bộ cặp key-value vào 3 bảng băm (double, linear, quadratic), sau đó in thống kê độ dài cụm (cluster length).
    template<typename DHTable, typename LTable, typename QTable>
    void insertAndPrintClusterStats(const DHTable& dht, const LTable& lpt, const QTable& qpt,
        const std::vector<std::pair<int, int>>& keyvals,
        const std::string& label = "")
    {
        // Copy bảng gốc để giữ nguyên trạng thái ngoài
        DHTable dht_copy = dht;
        LTable lpt_copy = lpt;
        QTable qpt_copy = qpt;

        // Insert dữ liệu vào các bản sao
        for (const auto& [key, value] : keyvals) {
            dht_copy.insert(key, value);
            lpt_copy.insert(key, value);
            qpt_copy.insert(key, value);
        }

        // In thống kê cluster length
        std::cout << "\n===== CLUSTER LENGTH STATISTICS"
            << (label.empty() ? "" : (" - " + label)) << " =====\n";
        std::cout << std::setw(32) << std::left << " "
            << std::setw(20) << "Double Hashing"
            << std::setw(20) << "Linear Probing"
            << std::setw(20) << "Quadratic Probing" << '\n';

        std::cout << std::setw(32) << std::left << "[Max cluster length]:"
            << std::setw(20) << dht_copy.maxClusterLength()
            << std::setw(20) << lpt_copy.maxClusterLength()
            << std::setw(20) << qpt_copy.maxClusterLength() << '\n';

        std::cout << std::setw(32) << std::left << "[Avg cluster length]:"
            << std::setw(20) << dht_copy.avgClusterLength()
            << std::setw(20) << lpt_copy.avgClusterLength()
            << std::setw(20) << qpt_copy.avgClusterLength() << '\n';
    }

    namespace printOutput {
        void printTableSizes(double lf1, double lf2, int N1, int N2) {
            std::cout << "TABLE_SIZE with load factor 1 (" << lf1 << "): " << N1 << '\n';
            std::cout << "TABLE_SIZE with load factor 2 (" << lf2 << "): " << N2 << '\n';
        }

        // Hàm in bảng thống kê tổng hợp thời gian thực hiện các thao tác
        void printSummaryTable(double lf1, double lf2, const StatResult& dht1, const StatResult& dht2, const StatResult& lpt1, const StatResult& lpt2, const StatResult& qpt1, const StatResult& qpt2) {
            std::cout << "\n===== TABLE OF PERFORMANCE COMPARISON (us) =====\n";
            std::cout << std::setw(50) << std::left << " "
                << std::setw(20) << "Double Hashing"
                << std::setw(20) << "Linear Probing"
                << std::setw(20) << "Quadratic Probing" << '\n';

            // Thời gian thực hiện các thao tác
            std::cout << std::setw(50) << std::left << ("[Insert Time] LF1 (" + helper::doubleToStr(lf1) + "):")
                << std::setw(20) << dht1.insertTime
                << std::setw(20) << lpt1.insertTime
                << std::setw(20) << qpt1.insertTime << '\n';

            std::cout << std::setw(50) << std::left << ("[Insert Time] LF2 (" + helper::doubleToStr(lf2) + "):")
                << std::setw(20) << dht2.insertTime
                << std::setw(20) << lpt2.insertTime
                << std::setw(20) << qpt2.insertTime << '\n';

            std::cout << std::setw(50) << std::left << ("[Search Time] LF1 (" + helper::doubleToStr(lf1) + "):")
                << std::setw(20) << dht1.searchTime
                << std::setw(20) << lpt1.searchTime
                << std::setw(20) << qpt1.searchTime << '\n';

            std::cout << std::setw(50) << std::left << ("[Search Time] LF2 (" + helper::doubleToStr(lf2) + "):")
                << std::setw(20) << dht2.searchTime
                << std::setw(20) << lpt2.searchTime
                << std::setw(20) << qpt2.searchTime << '\n';

            std::cout << std::setw(50) << std::left << ("[Delete Time] LF1 (" + helper::doubleToStr(lf1) + "):")
                << std::setw(20) << dht1.deleteTime
                << std::setw(20) << lpt1.deleteTime
                << std::setw(20) << qpt1.deleteTime << '\n';

            std::cout << std::setw(50) << std::left << ("[Delete Time] LF2 (" + helper::doubleToStr(lf2) + "):")
                << std::setw(20) << dht2.deleteTime
                << std::setw(20) << lpt2.deleteTime
                << std::setw(20) << qpt2.deleteTime << '\n';

            // In probe search hit/miss/insert after delete 
            std::cout << "\n----- PROBE STATISTICS (Average probes per operation) -----\n";
            std::cout << std::setw(50) << std::left << "[Avg probe/search HIT] LF1:"
                << std::setw(20) << dht1.avgProbeSearchHit
                << std::setw(20) << lpt1.avgProbeSearchHit
                << std::setw(20) << qpt1.avgProbeSearchHit << '\n';

            std::cout << std::setw(50) << std::left << "[Avg probe/search MISS] LF1:"
                << std::setw(20) << dht1.avgProbeSearchMiss
                << std::setw(20) << lpt1.avgProbeSearchMiss
                << std::setw(20) << qpt1.avgProbeSearchMiss << '\n';

            std::cout << std::setw(50) << std::left << "[Avg probe/insert-after-delete] LF1:"
                << std::setw(20) << dht1.avgProbeInsertAfterDelete
                << std::setw(20) << lpt1.avgProbeInsertAfterDelete
                << std::setw(20) << qpt1.avgProbeInsertAfterDelete << '\n';

            std::cout << std::setw(50) << std::left << "[Avg probe/search HIT] LF2:"
                << std::setw(20) << dht2.avgProbeSearchHit
                << std::setw(20) << lpt2.avgProbeSearchHit
                << std::setw(20) << qpt2.avgProbeSearchHit << '\n';

            std::cout << std::setw(50) << std::left << "[Avg probe/search MISS] LF2:"
                << std::setw(20) << dht2.avgProbeSearchMiss
                << std::setw(20) << lpt2.avgProbeSearchMiss
                << std::setw(20) << qpt2.avgProbeSearchMiss << '\n';

            std::cout << std::setw(50) << std::left << "[Avg probe/insert-after-delete] LF2:"
                << std::setw(20) << dht2.avgProbeInsertAfterDelete
                << std::setw(20) << lpt2.avgProbeInsertAfterDelete
                << std::setw(20) << qpt2.avgProbeInsertAfterDelete << '\n';
        }

        // Hàm in bảng thống kê chi tiết về số lần probe, số lần va chạm và tỷ lệ va chạm
        template <typename Table>
        void printDetailStats(const std::string& algoName, double lf, Table& table) {
            const auto& stats = table.stats;

            double avgInsertProbes = (stats.nInsert > 0) ? 1.0 * stats.totalProbesInsert / stats.nInsert : 0;
            double collisionRate = (stats.nInsert > 0) ? 100.0 * stats.totalCollision / stats.nInsert : 0;
            double avgSearchProbes = (stats.nSearch > 0) ? 1.0 * stats.totalProbesSearch / stats.nSearch : 0;
            double avgDeleteProbes = (stats.nDelete > 0) ? 1.0 * stats.totalProbesDelete / stats.nDelete : 0;

            std::cout << std::left
                << std::setw(20) << algoName
                << std::setw(12) << std::fixed << std::setprecision(2) << lf
                << std::setw(10) << stats.nInsert
                << std::setw(12) << std::fixed << std::setprecision(4) << avgInsertProbes
                << std::setw(10) << stats.totalCollision
                << std::setw(12) << std::fixed << std::setprecision(4) << collisionRate
                << std::setw(10) << stats.nSearch
                << std::setw(12) << std::fixed << std::setprecision(4) << avgSearchProbes
                << std::setw(10) << stats.nDelete
                << std::setw(12) << std::fixed << std::setprecision(4) << avgDeleteProbes
                << '\n';
        }

        // Hàm in tiêu đề cho bảng thống kê chi tiết
        void printDetailHeader(void) {
            std::cout << std::left << std::setw(20) << "Algorithm"
                << std::setw(12) << "LoadFac"
                << std::setw(10) << "Insert"
                << std::setw(12) << "Probe/Ins"
                << std::setw(10) << "Coll"
                << std::setw(12) << "CollRate"
                << std::setw(10) << "Search"
                << std::setw(12) << "Probe/S"
                << std::setw(10) << "Delete"
                << std::setw(12) << "Probe/D"
                << '\n';
            std::cout << std::string(120, '-') << '\n';
        }
    }

    // Hàm tính thời gian thực hiện các thao tác trên bảng băm
    template <typename Table>
    StatResult testTable(Table& table, const std::vector<std::pair<int, int>>& keyvals, const std::vector<int>& search_hit_indices, const std::vector<int>& search_missKeys, const std::vector<int>& delete_indices) {
        const int NUM_RUNS = 3;
        StatResult res;
        long long totalInsertTime = 0, totalSearchHitTime = 0, totalSearchMissTime = 0, totalDeleteTime = 0;

        // Thống kê probe cho từng loại search
        long long totalProbeSearchHit = 0, totalProbeSearchMiss = 0;
        long long totalProbeInsertAfterDelete = 0;
        int nInsertAfterDelete = 0;

        for (int run = 0; run < NUM_RUNS; ++run) {
            Table tempTable(table); // clone

            // Insert all keyvals
            auto t1 = std::chrono::high_resolution_clock::now();
            for (auto& kv : keyvals)
                tempTable.insert(kv.first, kv.second);
            auto t2 = std::chrono::high_resolution_clock::now();

            // Search HIT (tồn tại)
            int tmp;
            auto t3 = std::chrono::high_resolution_clock::now();
            for (int idx : search_hit_indices) {
                int probes_before = tempTable.stats.totalProbesSearch;
                tempTable.search(keyvals[idx].first, tmp);
                totalProbeSearchHit += tempTable.stats.totalProbesSearch - probes_before;
            }
            auto t4 = std::chrono::high_resolution_clock::now();

            // Search MISS (không tồn tại)
            auto t5 = std::chrono::high_resolution_clock::now();
            for (int key : search_missKeys) {
                int probes_before = tempTable.stats.totalProbesSearch;
                tempTable.search(key, tmp);
                totalProbeSearchMiss += tempTable.stats.totalProbesSearch - probes_before;
            }
            auto t6 = std::chrono::high_resolution_clock::now();

            // Delete các key
            auto t7 = std::chrono::high_resolution_clock::now();
            for (int idx : delete_indices)
                tempTable.erase(keyvals[idx].first);
            auto t8 = std::chrono::high_resolution_clock::now();

            // Insert lại các key vừa xóa (giá trị mới random)
            std::mt19937 rng(std::chrono::steady_clock::now().time_since_epoch().count());
            std::uniform_int_distribution<int> distVal(1, 1000000);
            for (int idx : delete_indices) {
                int probes_before = tempTable.stats.totalProbesInsert;
                tempTable.insert(keyvals[idx].first, distVal(rng));
                totalProbeInsertAfterDelete += tempTable.stats.totalProbesInsert - probes_before;
                nInsertAfterDelete++;
            }

            // Tính thời gian từng phần (chia nhỏ rõ ràng)
            totalInsertTime += std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();
            totalSearchHitTime += std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count();
            totalSearchMissTime += std::chrono::duration_cast<std::chrono::microseconds>(t6 - t5).count();
            totalDeleteTime += std::chrono::duration_cast<std::chrono::microseconds>(t8 - t7).count();

            // Ghi lại stats (1 lần duy nhất)
            if (run == 0)
                table.stats = tempTable.stats;
        }

        int nHit = search_hit_indices.size() * NUM_RUNS;
        int nMiss = search_missKeys.size() * NUM_RUNS;

        res.insertTime = totalInsertTime / NUM_RUNS;
        res.searchTime = (totalSearchHitTime + totalSearchMissTime) / NUM_RUNS;
        res.deleteTime = totalDeleteTime / NUM_RUNS;
        res.avgProbeSearchHit = (nHit ? 1.0 * totalProbeSearchHit / nHit : 0);
        res.avgProbeSearchMiss = (nMiss ? 1.0 * totalProbeSearchMiss / nMiss : 0);
        res.avgProbeInsertAfterDelete = (nInsertAfterDelete ? 1.0 * totalProbeInsertAfterDelete / nInsertAfterDelete : 0);

        return res;
    }

    void exportSummaryToCSV(const std::string& filename,
        double lf1, double lf2,
        const StatResult& dht1, const StatResult& dht2,
        const StatResult& lpt1, const StatResult& lpt2,
        const StatResult& qpt1, const StatResult& qpt2)
    {
        std::ofstream fout(filename, std::ios::app); // mở ở chế độ append
        if (!fout) {
            std::cerr << "ERROR: Cannot open output CSV!\n";
            return;
        }

        // Nếu là lần đầu ghi, ghi header
        static bool wroteHeader = false;
        if (!wroteHeader) {
            helper::writeCSVRow(fout, {
                "LoadFactor", "Algorithm", "InsertTime(us)", "SearchTime(us)", "DeleteTime(us)",
                "AvgProbeSearchHit", "AvgProbeSearchMiss", "AvgProbeInsertAfterDelete"
                });
            wroteHeader = true;
        }

        auto to_str = [](double v, int p = 2) {
            std::ostringstream oss; oss << std::fixed << std::setprecision(p) << v; return oss.str();
            };

        // Ghi 6 hàng: 3 loại bảng, 2 load factor
        helper::writeCSVRow(fout, { to_str(lf1), "Double Hashing", std::to_string(dht1.insertTime), std::to_string(dht1.searchTime), std::to_string(dht1.deleteTime),
            to_str(dht1.avgProbeSearchHit), to_str(dht1.avgProbeSearchMiss), to_str(dht1.avgProbeInsertAfterDelete) });

        helper::writeCSVRow(fout, { to_str(lf1), "Linear Probing", std::to_string(lpt1.insertTime), std::to_string(lpt1.searchTime), std::to_string(lpt1.deleteTime),
            to_str(lpt1.avgProbeSearchHit), to_str(lpt1.avgProbeSearchMiss), to_str(lpt1.avgProbeInsertAfterDelete) });

        helper::writeCSVRow(fout, { to_str(lf1), "Quadratic Probing", std::to_string(qpt1.insertTime), std::to_string(qpt1.searchTime), std::to_string(qpt1.deleteTime),
            to_str(qpt1.avgProbeSearchHit), to_str(qpt1.avgProbeSearchMiss), to_str(qpt1.avgProbeInsertAfterDelete) });

        helper::writeCSVRow(fout, { to_str(lf2), "Double Hashing", std::to_string(dht2.insertTime), std::to_string(dht2.searchTime), std::to_string(dht2.deleteTime),
            to_str(dht2.avgProbeSearchHit), to_str(dht2.avgProbeSearchMiss), to_str(dht2.avgProbeInsertAfterDelete) });

        helper::writeCSVRow(fout, { to_str(lf2), "Linear Probing", std::to_string(lpt2.insertTime), std::to_string(lpt2.searchTime), std::to_string(lpt2.deleteTime),
            to_str(lpt2.avgProbeSearchHit), to_str(lpt2.avgProbeSearchMiss), to_str(lpt2.avgProbeInsertAfterDelete) });

        helper::writeCSVRow(fout, { to_str(lf2), "Quadratic Probing", std::to_string(qpt2.insertTime), std::to_string(qpt2.searchTime), std::to_string(qpt2.deleteTime),
            to_str(qpt2.avgProbeSearchHit), to_str(qpt2.avgProbeSearchMiss), to_str(qpt2.avgProbeInsertAfterDelete) });
    }

    void runStaticBenchmark(int M, double lf1, double lf2, int N1, int N2, double missRate, int numDelete) {
        for (int datatype = 1; datatype <= 3; ++datatype) {
            std::string patternName;
            if (datatype == 1) patternName = "RANDOM";
            else if (datatype == 2) patternName = "SEQUENTIAL";
            else patternName = "CLUSTERED";

            std::cout << "\n===============================\n";
            std::cout << ">>> DATA PATTERN: " << patternName << "\n";

            // Tạo key-value tương ứng
            std::vector<std::pair<int, int>> keyvals;
            if (datatype == 1)
                keyvals = BenchmarkUtils::generator::generateRandomKeyVals(M, std::max(N1, N2) * 10);
            else if (datatype == 2)
                keyvals = BenchmarkUtils::generator::generateSequentialKeyVals(M);
            else
                keyvals = BenchmarkUtils::generator::generateClusteredKeyVals(M, std::max(N1, N2) * 10);

            // Sinh chỉ số search hit/miss
            std::vector<int> all_indices(M);
            helper::iota(all_indices.begin(), all_indices.end(), 0);

            int num_search = M;
            int numMiss = int(num_search * missRate + 0.5);
            int num_hit = num_search - numMiss;

            // Hit indices
            std::vector<int> indices = all_indices;
            helper::shuffle(indices);
            std::vector<int> search_hit_indices(indices.begin(), indices.begin() + num_hit);

            // Miss keys
            std::unordered_set<int> existKeys;
            for (const auto& kv : keyvals) existKeys.insert(kv.first);
            std::vector<int> search_missKeys = BenchmarkUtils::generator::generateMissKeys(numMiss, existKeys, std::max(N1, N2) * 10);

            // Delete indices
            std::vector<int> delete_indices = all_indices;
            helper::shuffle(delete_indices);
            delete_indices.resize(numDelete);

            // Tạo bảng băm cho 2 cấu hình LF1 và LF2
            DoubleHashTable<int, int> dht1(N1), dht2(N2);
            LinearHashTable<int, int> lpt1(N1), lpt2(N2);
            QuadraticHashTable<int, int> qpt1(N1), qpt2(N2);

            // Thống kê cluster
            BenchmarkUtils::insertAndPrintClusterStats(dht1, lpt1, qpt1, keyvals, "After Insert with LF1");
            BenchmarkUtils::insertAndPrintClusterStats(dht2, lpt2, qpt2, keyvals, "After Insert with LF2");

            // Đo hiệu năng
            auto dht1_stat = BenchmarkUtils::testTable(dht1, keyvals, search_hit_indices, search_missKeys, delete_indices);
            auto dht2_stat = BenchmarkUtils::testTable(dht2, keyvals, search_hit_indices, search_missKeys, delete_indices);
            auto lpt1_stat = BenchmarkUtils::testTable(lpt1, keyvals, search_hit_indices, search_missKeys, delete_indices);
            auto lpt2_stat = BenchmarkUtils::testTable(lpt2, keyvals, search_hit_indices, search_missKeys, delete_indices);
            auto qpt1_stat = BenchmarkUtils::testTable(qpt1, keyvals, search_hit_indices, search_missKeys, delete_indices);
            auto qpt2_stat = BenchmarkUtils::testTable(qpt2, keyvals, search_hit_indices, search_missKeys, delete_indices);

            // In bảng thống kê hiệu năng
            BenchmarkUtils::printOutput::printSummaryTable(lf1, lf2, dht1_stat, dht2_stat, lpt1_stat, lpt2_stat, qpt1_stat, qpt2_stat);

            std::cout << "\n===== SUMMARY TABLE: PROBES, COLLISIONS, RATES =====\n";
            BenchmarkUtils::printOutput::printDetailHeader();
            BenchmarkUtils::printOutput::printDetailStats("DoubleHash-LF1", lf1, dht1);
            BenchmarkUtils::printOutput::printDetailStats("LinearProb-LF1", lf1, lpt1);
            BenchmarkUtils::printOutput::printDetailStats("QuadraticProb-LF1", lf1, qpt1);
            BenchmarkUtils::printOutput::printDetailStats("DoubleHash-LF2", lf2, dht2);
            BenchmarkUtils::printOutput::printDetailStats("LinearProb-LF2", lf2, lpt2);
            BenchmarkUtils::printOutput::printDetailStats("QuadraticProb-LF2", lf2, qpt2);
            std::cout << "\n";

			// Xuất kết quả ra CSV
			BenchmarkUtils::exportSummaryToCSV("benchmark_results.csv", lf1, lf2,
				dht1_stat, dht2_stat, lpt1_stat, lpt2_stat, qpt1_stat, qpt2_stat);
        }
    }

    void runDynamicInsertExperiment(int M) {
        std::cout << "\n=== DYNAMIC TABLE TEST: INSERT M ITEMS ===\n";

        // In header bảng thống kê
        std::cout << std::left
            << std::setw(12) << "Pattern"
            << std::setw(25) << "Algorithm"
            << std::setw(15) << "InsertTime(us)"
            << std::setw(12) << "TableSize"
            << std::setw(12) << "LoadFactor"
            << std::setw(20) << "MaxClusterLen"
            << std::setw(20) << "AvgClusterLen" << '\n';
        std::cout << std::string(116, '-') << '\n';

        for (int pattern = 1; pattern <= 3; ++pattern) {
            std::string patternName;
            std::vector<std::pair<int, int>> keyvals;

            if (pattern == 1) {
                patternName = "RANDOM";
                keyvals = BenchmarkUtils::generator::generateRandomKeyVals(M, M * 10);
            }
            else if (pattern == 2) {
                patternName = "SEQUENTIAL";
                keyvals = BenchmarkUtils::generator::generateSequentialKeyVals(M);
            }
            else {
                patternName = "CLUSTERED";
                keyvals = BenchmarkUtils::generator::generateClusteredKeyVals(M, M * 10);
            }

            // Dynamic Linear
            DynamicLinearHashTable<int, int> dlt(17);
            auto t1 = std::chrono::high_resolution_clock::now();
            for (const auto& kv : keyvals)
                dlt.insert(kv.first, kv.second);
            auto t2 = std::chrono::high_resolution_clock::now();
            long long time_dlt = std::chrono::duration_cast<std::chrono::microseconds>(t2 - t1).count();

            std::cout << std::left
                << std::setw(12) << patternName
                << std::setw(25) << "Dynamic Linear"
                << std::setw(15) << time_dlt
                << std::setw(12) << dlt.size()
                << std::setw(12) << helper::doubleToStr(dlt.loadFactor(), 4)
                << std::setw(20) << dlt.maxClusterLength()
                << std::setw(20) << helper::doubleToStr(dlt.avgClusterLength(), 4)
                << '\n';

            // Dynamic Quadratic
            DynamicQuadraticHashTable<int, int> dqt(17);
            auto t3 = std::chrono::high_resolution_clock::now();
            for (const auto& kv : keyvals)
                dqt.insert(kv.first, kv.second);
            auto t4 = std::chrono::high_resolution_clock::now();
            long long time_dqt = std::chrono::duration_cast<std::chrono::microseconds>(t4 - t3).count();

            std::cout << std::left
                << std::setw(12) << patternName
                << std::setw(25) << "Dynamic Quadratic"
                << std::setw(15) << time_dqt
                << std::setw(12) << dqt.size()
                << std::setw(12) << helper::doubleToStr(dqt.loadFactor(), 4)
                << std::setw(20) << dqt.maxClusterLength()
                << std::setw(20) << helper::doubleToStr(dqt.avgClusterLength(), 4)
                << '\n';

            // Dynamic Double
            DynamicDoubleHashTable<int, int> ddt(17);
            auto t5 = std::chrono::high_resolution_clock::now();
            for (const auto& kv : keyvals)
                ddt.insert(kv.first, kv.second);
            auto t6 = std::chrono::high_resolution_clock::now();
            long long time_ddt = std::chrono::duration_cast<std::chrono::microseconds>(t6 - t5).count();

            std::cout << std::left
                << std::setw(12) << patternName
                << std::setw(25) << "Dynamic Double"
                << std::setw(15) << time_ddt
                << std::setw(12) << ddt.size()
                << std::setw(12) << helper::doubleToStr(ddt.loadFactor(), 4)
                << std::setw(20) << ddt.maxClusterLength()
                << std::setw(20) << helper::doubleToStr(ddt.avgClusterLength(), 4)
                << '\n';
        }

        std::cout << "\n=== FINISHED DYNAMIC TABLE TEST ===\n";
    }
}

struct BenchmarkConfig {
    int M;
    double lf1;
    double lf2;
    double missRate;
    int numDelete;
};

void runBenchmarks(const BenchmarkConfig& cfg) {
    int N1 = helper::nextPrime(int(cfg.M / cfg.lf1));
    int N2 = helper::nextPrime(int(cfg.M / cfg.lf2));
    BenchmarkUtils::printOutput::printTableSizes(cfg.lf1, cfg.lf2, N1, N2);

    BenchmarkUtils::runStaticBenchmark(cfg.M, cfg.lf1, cfg.lf2,
                                       N1, N2, cfg.missRate, cfg.numDelete);
    std::cout << "\n=== FINISHED ALL DATA PATTERNS ===\n";

    BenchmarkUtils::runDynamicInsertExperiment(cfg.M);
    std::cout << "\n=== FINISHED DYNAMIC INSERT EXPERIMENT ===\n";
}

int main() {
    BenchmarkConfig cfg;
    cfg.M = BenchmarkUtils::getInput::getTestSize();
    cfg.lf1 = BenchmarkUtils::getInput::getUserLoadFactor();
    cfg.lf2 = 0.5; // tự động chọn
    cfg.missRate = BenchmarkUtils::getInput::getMissRate();
    cfg.numDelete = cfg.M;

    runBenchmarks(cfg);
    return 0;
}

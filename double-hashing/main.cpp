#include <algorithm>
#include <chrono>
#include <cmath>
#include <iomanip>
#include <iostream>
#include <numeric>
#include <random>
#include <sstream>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>
using namespace std;

enum SlotState { EMPTY, OCCUPIED, DELETED };

template <class K, class V> struct Entry {
  K key{};
  V value{};
  SlotState state{EMPTY};
};

struct Stats {
  long long insProbe = 0, srchProbe = 0, delProbe = 0;
  long long collisions = 0;
  long long nIns = 0, nSearch = 0, nDel = 0;
};

enum class Probing { LINEAR, QUADRATIC, DOUBLE };

namespace helper {
inline mt19937 &rng() {
  static mt19937 r(
      (unsigned)chrono::steady_clock::now().time_since_epoch().count());
  return r;
}
inline bool isPrime(int n) {
  if (n <= 1)
    return false;
  if (n <= 3)
    return true;
  if (n % 2 == 0 || n % 3 == 0)
    return false;
  for (int i = 5; i * i <= n; i += 6)
    if (n % i == 0 || n % (i + 2) == 0)
      return false;
  return true;
}
inline int nextPrime(int n) {
  while (!isPrime(++n))
    ;
  return n;
}
} // namespace helper

template <class K, class V> class HashTable {
  vector<Entry<K, V>> table;
  int sz;
  int used;
  Probing type;
  bool allowRehash;
  int prime;

public:
  Stats stats;
  HashTable(int s, Probing t, bool rehash = true)
      : sz(helper::nextPrime(s)), used(0), type(t), allowRehash(rehash) {
    prime = helper::nextPrime(sz / 2);
    table.assign(sz, {});
  }

  double loadFactor() const { return (double)used / sz; }

  void rehash(int hint) {
    int newSize = helper::nextPrime(hint);
    vector<Entry<K, V>> old = table;
    table.assign(newSize, {});
    sz = newSize;
    used = 0;
    prime = helper::nextPrime(sz / 2);
    for (auto &e : old)
      if (e.state == OCCUPIED)
        insert(e.key, e.value);
  }

  bool insert(const K &key, const V &val) {
    if (allowRehash && loadFactor() > 0.7)
      rehash(sz * 2);
    int base = key % sz;
    int step = 0;
    int probe;
    int probes = 1;
    while (true) {
      probe = calcProbe(base, step, key);
      if (table[probe].state != OCCUPIED) {
        table[probe] = {key, val, OCCUPIED};
        used++;
        stats.insProbe += probes;
        stats.nIns++;
        return true;
      }
      if (table[probe].key == key) {
        table[probe].value = val;
        return true;
      }
      stats.collisions++;
      step++;
      probes++;
    }
  }

  bool search(const K &key, V &out) {
    int base = key % sz;
    int step = 0;
    int probe;
    int probes = 1;
    while (true) {
      probe = calcProbe(base, step, key);
      if (table[probe].state == EMPTY)
        break;
      if (table[probe].state == OCCUPIED && table[probe].key == key) {
        out = table[probe].value;
        stats.srchProbe += probes;
        stats.nSearch++;
        return true;
      }
      step++;
      probes++;
      if (step >= sz)
        break;
    }
    stats.srchProbe += probes;
    stats.nSearch++;
    return false;
  }

  void erase(const K &key) {
    int base = key % sz;
    int step = 0;
    int probe;
    int probes = 1;
    while (true) {
      probe = calcProbe(base, step, key);
      if (table[probe].state == EMPTY) {
        stats.delProbe += probes;
        stats.nDel++;
        return;
      }
      if (table[probe].state == OCCUPIED && table[probe].key == key) {
        table[probe].state = DELETED;
        used--;
        stats.delProbe += probes;
        stats.nDel++;
        return;
      }
      step++;
      probes++;
      if (step >= sz)
        break;
    }
    stats.delProbe += probes;
    stats.nDel++;
  }

  int maxCluster() const {
    int cur = 0, mx = 0;
    for (auto &e : table) {
      if (e.state == OCCUPIED) {
        cur++;
        mx = max(mx, cur);
      } else
        cur = 0;
    }
    return mx;
  }
  double avgCluster() const {
    int cur = 0, tot = 0, cnt = 0;
    for (auto &e : table) {
      if (e.state == OCCUPIED)
        cur++;
      else {
        if (cur) {
          tot += cur;
          cnt++;
          cur = 0;
        }
      }
    }
    if (cur) {
      tot += cur;
      cnt++;
    }
    return cnt ? double(tot) / cnt : 0.0;
  }

private:
  int calcProbe(int base, int step, K key) const {
    switch (type) {
    case Probing::LINEAR:
      return (base + step) % sz;
    case Probing::QUADRATIC:
      return (base + step * step) % sz;
    default:
      return (base + step * (prime - (key % prime))) % sz;
    }
  }
};

struct BenchResult {
  long long insTime = 0, searchTime = 0, delTime = 0;
  double avgHit = 0, avgMiss = 0, avgInsAfterDel = 0;
  int maxCluster = 0;
  double avgCluster = 0;
};

namespace Gen {
vector<pair<int, int>> randomKV(int M, int keyLim) {
  uniform_int_distribution<int> dk(1, keyLim), dv(1, 1e6);
  unordered_set<int> used;
  vector<pair<int, int>> res;
  while ((int)res.size() < M) {
    int k = dk(helper::rng());
    if (used.count(k))
      continue;
    used.insert(k);
    res.push_back({k, dv(helper::rng())});
  }
  return res;
}
vector<pair<int, int>> sequentialKV(int M) {
  uniform_int_distribution<int> dv(1, 1e6);
  vector<pair<int, int>> res;
  res.reserve(M);
  for (int i = 1; i <= M; i++)
    res.push_back({i, dv(helper::rng())});
  return res;
}
vector<pair<int, int>> clusteredKV(int M, int keyLim) {
  uniform_int_distribution<int> dv(1, 1e6);
  int clusters = 5, per = M / clusters;
  vector<pair<int, int>> res;
  int base = 1;
  for (int c = 0; c < clusters; c++) {
    for (int i = 0; i < per && (int)res.size() < M; i++)
      res.push_back({base + i, dv(helper::rng())});
    base += keyLim / clusters;
  }
  while ((int)res.size() < M)
    res.push_back({base++, dv(helper::rng())});
  return res;
}
vector<int> missKeys(int n, const unordered_set<int> &exist, int keyLim) {
  unordered_set<int> used = exist;
  vector<int> res;
  uniform_int_distribution<int> dk(1, keyLim);
  while ((int)res.size() < n) {
    int k = dk(helper::rng());
    if (used.count(k))
      continue;
    used.insert(k);
    res.push_back(k);
  }
  return res;
}
} // namespace Gen

BenchResult test(HashTable<int, int> &table, const vector<pair<int, int>> &kv,
                 const vector<int> &hitIdx, const vector<int> &missKeys,
                 const vector<int> &delIdx) {
  BenchResult res;
  const int RUN = 2;
  long long tIns = 0, tHit = 0, tMiss = 0, tDel = 0;
  long long probeHit = 0, probeMiss = 0, probeInsDel = 0;
  int nInsDel = 0;
  for (int r = 0; r < RUN; r++) {
    HashTable<int, int> t(table);
    auto t1 = chrono::high_resolution_clock::now();
    for (auto &kvp : kv)
      t.insert(kvp.first, kvp.second);
    auto t2 = chrono::high_resolution_clock::now();
    if (r == 0) {
      res.maxCluster = t.maxCluster();
      res.avgCluster = t.avgCluster();
    }
    int tmp;
    auto t3 = chrono::high_resolution_clock::now();
    for (int idx : hitIdx) {
      int before = t.stats.srchProbe;
      t.search(kv[idx].first, tmp);
      probeHit += t.stats.srchProbe - before;
    }
    auto t4 = chrono::high_resolution_clock::now();
    auto t5 = chrono::high_resolution_clock::now();
    for (int k : missKeys) {
      int before = t.stats.srchProbe;
      t.search(k, tmp);
      probeMiss += t.stats.srchProbe - before;
    }
    auto t6 = chrono::high_resolution_clock::now();
    auto t7 = chrono::high_resolution_clock::now();
    for (int idx : delIdx)
      t.erase(kv[idx].first);
    auto t8 = chrono::high_resolution_clock::now();
    uniform_int_distribution<int> dv(1, 1e6);
    for (int idx : delIdx) {
      int before = t.stats.insProbe;
      t.insert(kv[idx].first, dv(helper::rng()));
      probeInsDel += t.stats.insProbe - before;
      nInsDel++;
    }
    tIns += chrono::duration_cast<chrono::microseconds>(t2 - t1).count();
    tHit += chrono::duration_cast<chrono::microseconds>(t4 - t3).count();
    tMiss += chrono::duration_cast<chrono::microseconds>(t6 - t5).count();
    tDel += chrono::duration_cast<chrono::microseconds>(t8 - t7).count();
    if (r == 0)
      table.stats = t.stats;
  }
  int nHit = hitIdx.size() * RUN, nMiss = missKeys.size() * RUN;
  res.insTime = tIns / RUN;
  res.searchTime = (tHit + tMiss) / RUN;
  res.delTime = tDel / RUN;
  res.avgHit = nHit ? double(probeHit) / nHit : 0;
  res.avgMiss = nMiss ? double(probeMiss) / nMiss : 0;
  res.avgInsAfterDel = nInsDel ? double(probeInsDel) / nInsDel : 0;
  return res;
}

void summary(double lf1, double lf2, const BenchResult &a, const BenchResult &b,
             const BenchResult &c, const BenchResult &d, const BenchResult &e,
             const BenchResult &f) {
  cout << "\n===== TABLE OF PERFORMANCE COMPARISON (us) =====\n";
  cout << setw(50) << left << " " << setw(20) << "Double Hash" << setw(20)
       << "Linear" << setw(20) << "Quadratic" << '\n';
  cout << setw(50) << left << "[Insert Time] LF1" << setw(20) << a.insTime
       << setw(20) << c.insTime << setw(20) << e.insTime << '\n';
  cout << setw(50) << left << "[Insert Time] LF2" << setw(20) << b.insTime
       << setw(20) << d.insTime << setw(20) << f.insTime << '\n';
  cout << setw(50) << left << "[Search Time] LF1" << setw(20) << a.searchTime
       << setw(20) << c.searchTime << setw(20) << e.searchTime << '\n';
  cout << setw(50) << left << "[Search Time] LF2" << setw(20) << b.searchTime
       << setw(20) << d.searchTime << setw(20) << f.searchTime << '\n';
  cout << setw(50) << left << "[Delete Time] LF1" << setw(20) << a.delTime
       << setw(20) << c.delTime << setw(20) << e.delTime << '\n';
  cout << setw(50) << left << "[Delete Time] LF2" << setw(20) << b.delTime
       << setw(20) << d.delTime << setw(20) << f.delTime << '\n';
  cout << "\n----- PROBE STATISTICS (Average probes per operation) -----\n";
  cout << setw(50) << left << "[Avg probe/search HIT] LF1" << setw(20)
       << a.avgHit << setw(20) << c.avgHit << setw(20) << e.avgHit << '\n';
  cout << setw(50) << left << "[Avg probe/search MISS] LF1" << setw(20)
       << a.avgMiss << setw(20) << c.avgMiss << setw(20) << e.avgMiss << '\n';
  cout << setw(50) << left << "[Avg probe/insert-after-delete] LF1" << setw(20)
       << a.avgInsAfterDel << setw(20) << c.avgInsAfterDel << setw(20)
       << e.avgInsAfterDel << '\n';
  cout << setw(50) << left << "[Avg probe/search HIT] LF2" << setw(20)
       << b.avgHit << setw(20) << d.avgHit << setw(20) << f.avgHit << '\n';
  cout << setw(50) << left << "[Avg probe/search MISS] LF2" << setw(20)
       << b.avgMiss << setw(20) << d.avgMiss << setw(20) << f.avgMiss << '\n';
  cout << setw(50) << left << "[Avg probe/insert-after-delete] LF2" << setw(20)
       << b.avgInsAfterDel << setw(20) << d.avgInsAfterDel << setw(20)
       << f.avgInsAfterDel << '\n';
}

void printCluster(const BenchResult &dh, const BenchResult &lh,
                  const BenchResult &qh, const string &label) {
  cout << "\n===== CLUSTER LENGTH STATISTICS - " << label << " =====\n";
  cout << setw(32) << left << " " << setw(20) << "Double Hash" << setw(20)
       << "Linear" << setw(20) << "Quadratic" << '\n';
  cout << setw(32) << left << "[Max cluster length]:" << setw(20)
       << dh.maxCluster << setw(20) << lh.maxCluster << setw(20)
       << qh.maxCluster << '\n';
  cout << setw(32) << left << "[Avg cluster length]:" << setw(20)
       << dh.avgCluster << setw(20) << lh.avgCluster << setw(20)
       << qh.avgCluster << '\n';
}

vector<int> getSizes() {
  string line;
  cout << "Enter a list of the number of elements to test (space separated): ";
  getline(cin, line);
  stringstream ss(line);
  vector<int> v;
  int x;
  while (ss >> x)
    v.push_back(x);
  return v;
}

int main() {
  ios::sync_with_stdio(false);
  cin.tie(nullptr);
  cout << "=== HASH TABLE BENCHMARKING ===\n";
  vector<int> tests = getSizes();
  double lf1 = 0.5, lf2 = 0.9;
  cout << "Auto-selected optimal load factors: 0.5 and 0.9\n";
  double missRate = 0.5;
  cout << "Using default miss rate for search operations: 0.5 (50%)\n";
  for (int M : tests) {
    int N1 = helper::nextPrime(int(M / lf1)),
        N2 = helper::nextPrime(int(M / lf2));
    cout << "TABLE_SIZE with load factor 1 (" << lf1 << "): " << N1 << '\n';
    cout << "TABLE_SIZE with load factor 2 (" << lf2 << "): " << N2 << '\n';
    for (int rehash = 0; rehash < 2; ++rehash) {
      bool allow = rehash;
      string rh = allow ? "WITH" : "WITHOUT";
      cout << "\n===== RUN " << rh << " REHASH =====\n";
      for (int type = 1; type <= 3; ++type) {
        string pattern = type == 1   ? "RANDOM"
                         : type == 2 ? "SEQUENTIAL"
                                     : "CLUSTERED";
        cout << "\n>>> DATA PATTERN: " << pattern << '\n';
        vector<pair<int, int>> kv =
            type == 1   ? Gen::randomKV(M, max(N1, N2) * 10)
            : type == 2 ? Gen::sequentialKV(M)
                        : Gen::clusteredKV(M, max(N1, N2) * 10);
        vector<int> all(M);
        iota(all.begin(), all.end(), 0);
        int numSearch = M;
        int numMiss = lround(numSearch * missRate);
        int numHit = numSearch - numMiss;
        vector<int> idx = all;
        shuffle(idx.begin(), idx.end(), helper::rng());
        vector<int> hit(idx.begin(), idx.begin() + numHit);
        unordered_set<int> exist;
        for (auto &p : kv)
          exist.insert(p.first);
        vector<int> miss = Gen::missKeys(numMiss, exist, max(N1, N2) * 10);
        vector<int> del = all;
        shuffle(del.begin(), del.end(), helper::rng());
        del.resize(M);
        HashTable<int, int> dh1(N1, Probing::DOUBLE, allow),
            dh2(N2, Probing::DOUBLE, allow);
        HashTable<int, int> lh1(N1, Probing::LINEAR, allow),
            lh2(N2, Probing::LINEAR, allow);
        HashTable<int, int> qh1(N1, Probing::QUADRATIC, allow),
            qh2(N2, Probing::QUADRATIC, allow);
        BenchResult r1 = test(dh1, kv, hit, miss, del),
                    r2 = test(dh2, kv, hit, miss, del),
                    r3 = test(lh1, kv, hit, miss, del),
                    r4 = test(lh2, kv, hit, miss, del),
                    r5 = test(qh1, kv, hit, miss, del),
                    r6 = test(qh2, kv, hit, miss, del);
        printCluster(r1, r3, r5, "After Insert with LF1");
        printCluster(r2, r4, r6, "After Insert with LF2");
        summary(lf1, lf2, r1, r2, r3, r4, r5, r6);
      }
    }
    cout << "\n=== FINISHED TEST SIZE: " << M << " ===\n";
  }
}

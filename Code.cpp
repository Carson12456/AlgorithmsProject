// ============================================================
// Regional Network Project
// Visitor Pattern synthesis of:
//   - Madison's Dijkstra's Algorithm with Fibonacci Heap (Presentation 2)
//   - Ethan's Kruskal's MST with Strategy Pattern (Presentation 2)
//
// Authors: Jared Gregoire & Partner
// ============================================================

#include <iostream>
#include <vector>
#include <limits.h>
#include <algorithm>
#include <unordered_map>
#include <chrono>
#include <random>
#include <iomanip>
using namespace std;


// ============================================================
// SECTION 1: Forward declarations + Visitor Interface
// ============================================================
class NetworkGraph; // forward decl so visitor can reference it

// abstract visitor. Each algorithm will be its own visitor
// that gets applied to the NetworkGraph.
class GraphVisitor {
public:
    virtual void visit(NetworkGraph& graph) = 0;
    virtual ~GraphVisitor() {}
};


// ============================================================
// SECTION 2: NetworkGraph (the "visitable" element)
// ============================================================
// Edge stores two weights so we can demonstrate both algorithms
// meaningfully on the same graph:
//   - latency: used by Dijkstra (fastest route)
//   - cost:    used by Kruskal  (cheapest redundancy backbone)
struct NetworkEdge {
    int u, v;
    int latency; // ms
    int cost;    // $
};

class NetworkGraph {
public:
    int V; // num nodes
    vector<NetworkEdge> edges; // edge list (used by Kruskal)
    // adjacency list (used by Dijkstra). Each entry is (neighbor, latency).
    vector< vector< pair<int,int> > > adj;

    NetworkGraph(int V) {
        this->V = V;
        adj.resize(V);
    }

    void addEdge(int u, int v, int latency, int cost) {
        NetworkEdge e;
        e.u = u; e.v = v;
        e.latency = latency; e.cost = cost;
        edges.push_back(e);

        // undirected graph, add both directions to adj list
        adj[u].push_back(make_pair(v, latency));
        adj[v].push_back(make_pair(u, latency));
    }

    // accept method - standard Visitor Pattern entry point
    void accept(GraphVisitor& visitor) {
        visitor.visit(*this);
    }
};


// ============================================================
// SECTION 3: Madison's Dijkstra code, wrapped as a Visitor
// ============================================================
// Below is Madison's FibHeap and Node - kept as close to original
// as possible, with minor cleanup. The DijkstraVisitor wraps it.

struct Node {
    int vertex;
    int key;
};

class FibHeap {
public:
    vector<Node*> heap;

    void insert(Node* n) {
        heap.push_back(n);
    }

    Node* extractMin() {
        if (heap.empty()) return NULL;

        int minIndex = 0;
        for (int i = 1; i < (int)heap.size(); i++) {
            if (heap[i]->key < heap[minIndex]->key) {
                minIndex = i;
            }
        }

        Node* minNode = heap[minIndex];
        heap.erase(heap.begin() + minIndex);
        return minNode;
    }

    void decreaseKey(Node* n, int newKey) {
        n->key = newKey;
    }

    bool isEmpty() {
        return heap.empty();
    }
};

class DijkstraVisitor : public GraphVisitor {
private:
    int source;
    bool verbose; // whether to print step-by-step
public:
    vector<int> dist; // results exposed so benchmark can verify
    long long lastRuntimeMicros;

    DijkstraVisitor(int src, bool verbose = true) {
        this->source = src;
        this->verbose = verbose;
        this->lastRuntimeMicros = 0;
    }

    void visit(NetworkGraph& graph) override {
        auto start = chrono::high_resolution_clock::now();

        int V = graph.V;
        dist.assign(V, INT_MAX);

        FibHeap heap;
        vector<Node*> nodes(V);

        // init all nodes at infinity, push to heap
        for (int i = 0; i < V; i++) {
            nodes[i] = new Node;
            nodes[i]->vertex = i;
            nodes[i]->key = INT_MAX;
            heap.insert(nodes[i]);
        }

        dist[source] = 0;
        heap.decreaseKey(nodes[source], 0);

        while (!heap.isEmpty()) {
            Node* minNode = heap.extractMin();
            int u = minNode->vertex;

            // if smallest is still infinity, rest is unreachable
            if (minNode->key == INT_MAX) break;

            if (verbose) {
                cout << "Processing node: " << u << endl;
            }

            // relax all neighbors
            for (int i = 0; i < (int)graph.adj[u].size(); i++) {
                int v = graph.adj[u][i].first;
                int weight = graph.adj[u][i].second;

                if (dist[u] != INT_MAX && dist[u] + weight < dist[v]) {
                    dist[v] = dist[u] + weight;
                    heap.decreaseKey(nodes[v], dist[v]);

                    if (verbose) {
                        cout << "  Updated distance of " << v
                             << " to " << dist[v]
                             << " (Decrease-Key)" << endl;
                    }
                }
            }
        }

        // clean up nodes
        for (int i = 0; i < V; i++) delete nodes[i];

        auto end = chrono::high_resolution_clock::now();
        lastRuntimeMicros = chrono::duration_cast<chrono::microseconds>(end - start).count();

        if (verbose) {
            cout << "\nFinal shortest latencies from node " << source << ":\n";
            for (int i = 0; i < V; i++) {
                if (dist[i] == INT_MAX) {
                    cout << "Node " << i << " : unreachable\n";
                } else {
                    cout << "Node " << i << " : " << dist[i] << " ms\n";
                }
            }
        }
    }
};


// ============================================================
// SECTION 4: Ethan's Kruskal code, wrapped as a Visitor
// ============================================================
// Below is Ethan's DSU and Kruskal logic - largely preserved.
// Note: he used the Strategy pattern internally; we keep that
// as a nested detail but expose it through a Visitor wrapper.

class DSU {
private:
    vector<int> parent, dsu_rank; // renamed from 'rank' - C++ std already has std::rank
public:
    DSU(int n) {
        parent.resize(n);
        dsu_rank.resize(n, 0);
        for (int i = 0; i < n; i++)
            parent[i] = i;
    }

    int find(int x) {
        if (parent[x] != x)
            parent[x] = find(parent[x]); // path compression
        return parent[x];
    }

    void unite(int a, int b) {
        int rootA = find(a);
        int rootB = find(b);

        if (rootA != rootB) {
            if (dsu_rank[rootA] < dsu_rank[rootB]) {
                parent[rootA] = rootB;
            } else if (dsu_rank[rootA] > dsu_rank[rootB]) {
                parent[rootB] = rootA;
            } else {
                parent[rootB] = rootA;
                dsu_rank[rootA]++;
            }
        }
    }

    // alphabetical subset printing (only used for small demos)
    void printSets(const vector<char>& labels) {
        unordered_map<int, vector<char> > groups;

        for (int i = 0; i < (int)parent.size(); i++) {
            groups[find(i)].push_back(labels[i]);
        }

        vector<vector<char> > sortedGroups;
        for (auto &g : groups) {
            vector<char> group = g.second;
            sort(group.begin(), group.end());
            sortedGroups.push_back(group);
        }

        sort(sortedGroups.begin(), sortedGroups.end(),
            [](const vector<char>& a, const vector<char>& b) {
                return a[0] < b[0];
            });

        cout << sortedGroups.size() << " Subsets: ";
        for (int i = 0; i < (int)sortedGroups.size(); i++) {
            cout << "{";
            for (int j = 0; j < (int)sortedGroups[i].size(); j++) {
                cout << sortedGroups[i][j];
                if (j < (int)sortedGroups[i].size() - 1)
                    cout << ", ";
            }
            cout << "}";
            if (i < (int)sortedGroups.size() - 1)
                cout << ", ";
        }
        cout << "\n";
    }
};

class KruskalVisitor : public GraphVisitor {
private:
    bool verbose;
    vector<char> labels; // optional labels for demo graphs

public:
    int totalCost;              // exposed so we can verify results
    vector<NetworkEdge> mst;    // the chosen MST edges
    long long lastRuntimeMicros;

    KruskalVisitor(bool verbose = true, vector<char> labels = {}) {
        this->verbose = verbose;
        this->labels = labels;
        this->totalCost = 0;
        this->lastRuntimeMicros = 0;
    }

    void visit(NetworkGraph& graph) override {
        auto start = chrono::high_resolution_clock::now();

        // copy edges since we need to sort them without mutating the graph
        vector<NetworkEdge> edges = graph.edges;

        // sort by cost (not latency) - we want cheapest redundancy
        sort(edges.begin(), edges.end(), [](NetworkEdge a, NetworkEdge b) {
            return a.cost < b.cost;
        });

        DSU dsu(graph.V);
        totalCost = 0;
        mst.clear();

        bool useLabels = ((int)labels.size() == graph.V);

        if (verbose) {
            cout << "Initial State:\n";
            if (useLabels) dsu.printSets(labels);
            cout << "------------------------\n";
        }

        int step = 1;
        for (auto &e : edges) {
            if (verbose) {
                if (useLabels) {
                    cout << "Step " << step++ << ": Considering edge "
                         << labels[e.u] << "-" << labels[e.v]
                         << " (cost " << e.cost << ")\n";
                } else {
                    cout << "Step " << step++ << ": Considering edge "
                         << e.u << "-" << e.v
                         << " (cost " << e.cost << ")\n";
                }
            }

            if (dsu.find(e.u) != dsu.find(e.v)) {
                if (verbose) {
                    cout << " -> Adding edge\n";
                    if (useLabels) {
                        cout << " -> Union(" << labels[e.u]
                             << ", " << labels[e.v] << ")\n";
                    }
                }
                dsu.unite(e.u, e.v);
                mst.push_back(e);
                totalCost += e.cost;

                if (verbose && useLabels) dsu.printSets(labels);
            } else if (verbose) {
                cout << " -> Skipping (would form cycle)\n";
            }

            if (verbose) cout << "------------------------\n";

            if ((int)mst.size() == graph.V - 1) break;
        }

        auto end = chrono::high_resolution_clock::now();
        lastRuntimeMicros = chrono::duration_cast<chrono::microseconds>(end - start).count();

        if (verbose) {
            cout << "\nFinal MST (cheapest redundancy backbone):\n";
            for (auto &e : mst) {
                if (useLabels) {
                    cout << labels[e.u] << "-" << labels[e.v]
                         << " (cost " << e.cost << ")\n";
                } else {
                    cout << e.u << "-" << e.v
                         << " (cost " << e.cost << ")\n";
                }
            }
            cout << "Total Cost: " << totalCost << endl;
        }
    }
};


// ============================================================
// SECTION 5: Random graph generator
// ============================================================
// We build a random connected graph by:
//   1. Starting with a random spanning tree (guarantees connectivity)
//   2. Adding extra random edges to reach the requested density
NetworkGraph generateRandomGraph(int V, double density, unsigned seed) {
    NetworkGraph g(V);
    mt19937 rng(seed);
    uniform_int_distribution<int> latDist(1, 100);   // latency 1-100 ms
    uniform_int_distribution<int> costDist(1, 1000); // cost 1-1000 $

    // step 1: random spanning tree via shuffled permutation
    vector<int> perm(V);
    for (int i = 0; i < V; i++) perm[i] = i;
    shuffle(perm.begin(), perm.end(), rng);

    // track which edges already exist so we don't add duplicates later
    // key = u*V + v where u < v
    vector<bool> edgeExists;
    edgeExists.assign((long long)V * V, false);

    for (int i = 1; i < V; i++) {
        // connect perm[i] to a random earlier node
        uniform_int_distribution<int> parentDist(0, i-1);
        int parent = perm[parentDist(rng)];
        int child = perm[i];
        int a = min(parent, child);
        int b = max(parent, child);
        g.addEdge(parent, child, latDist(rng), costDist(rng));
        edgeExists[(long long)a * V + b] = true;
    }

    // step 2: add extra edges up to target density
    // max possible edges in undirected simple graph = V*(V-1)/2
    long long maxEdges = (long long)V * (V - 1) / 2;
    long long targetEdges = (long long)(density * maxEdges);
    if (targetEdges < V - 1) targetEdges = V - 1;
    if (targetEdges > maxEdges) targetEdges = maxEdges;

    uniform_int_distribution<int> nodeDist(0, V-1);
    long long safetyCounter = 0;
    long long safetyMax = targetEdges * 10 + 100; // avoid infinite loop on dense graphs

    while ((long long)g.edges.size() < targetEdges && safetyCounter < safetyMax) {
        int u = nodeDist(rng);
        int v = nodeDist(rng);
        if (u == v) { safetyCounter++; continue; }
        int a = min(u, v);
        int b = max(u, v);
        if (edgeExists[(long long)a * V + b]) { safetyCounter++; continue; }
        g.addEdge(u, v, latDist(rng), costDist(rng));
        edgeExists[(long long)a * V + b] = true;
        safetyCounter++;
    }

    return g;
}


// ============================================================
// SECTION 6: Benchmarking
// ============================================================
void runBenchmark(double density) {
    vector<int> sizes = {10, 50, 100, 250, 500, 1000, 2000};

    cout << "\n============================================================\n";
    cout << "EMPIRICAL BENCHMARK (density = " << density << ")\n";
    cout << "============================================================\n";
    cout << left << setw(8) << "V"
         << setw(10) << "E"
         << setw(18) << "Dijkstra (us)"
         << setw(18) << "Kruskal (us)" << "\n";
    cout << string(54, '-') << "\n";

    mt19937 sourceRng(12345); // deterministic source selection

    for (int V : sizes) {
        NetworkGraph g = generateRandomGraph(V, density, 42 + V);

        uniform_int_distribution<int> srcDist(0, V - 1);
        int randomSource = srcDist(sourceRng);

        DijkstraVisitor dv(randomSource, false);
        g.accept(dv);

        KruskalVisitor kv(false);
        g.accept(kv);

        cout << left << setw(8) << V
             << setw(10) << g.edges.size()
             << setw(18) << dv.lastRuntimeMicros
             << setw(18) << kv.lastRuntimeMicros << "\n";
    }

    cout << "\nTheoretical bounds for reference:\n";
    cout << "  Dijkstra (this impl, linear-scan priority queue): O(V^2 + E)\n";
    cout << "  Kruskal  (with DSU + path compression):           O(E log E)\n";
}


// ============================================================
// SECTION 7: Demo mode (small hardcoded office network)
// ============================================================
void runDemo() {
    cout << "\n============================================================\n";
    cout << "DEMO: Small office network (7 rooms)\n";
    cout << "============================================================\n";
    cout << "Nodes represent rooms/offices. Edges have two weights:\n";
    cout << "  latency (ms)  -> Dijkstra uses this for fastest route\n";
    cout << "  cost    ($)   -> Kruskal  uses this for cheapest MST\n\n";

    // 7-room office - same topology Ethan demoed, with added latencies
    NetworkGraph g(7);
    vector<char> labels = {'A','B','C','D','E','F','G'};

    // addEdge(u, v, latency, cost)
    g.addEdge(4, 3, 2, 1);   // E-D
    g.addEdge(6, 2, 3, 1);   // G-C
    g.addEdge(5, 0, 4, 2);   // F-A
    g.addEdge(0, 1, 5, 3);   // A-B
    g.addEdge(1, 6, 6, 4);   // B-G
    g.addEdge(0, 6, 7, 5);   // A-G
    g.addEdge(1, 2, 4, 5);   // B-C
    g.addEdge(3, 2, 3, 6);   // D-C
    g.addEdge(5, 3, 8, 7);   // F-D
    g.addEdge(3, 6, 5, 8);   // D-G
    g.addEdge(0, 3, 9, 12);  // A-D

    cout << "--- Visitor 1: DijkstraVisitor from node A (fastest routes) ---\n\n";
    DijkstraVisitor dv(0, true);
    g.accept(dv);

    cout << "\n--- Visitor 2: KruskalVisitor (cheapest redundancy backbone) ---\n\n";
    KruskalVisitor kv(true, labels);
    g.accept(kv);
}


// ============================================================
// SECTION 8: Custom random graph (user picks size + density)
// ============================================================
void runCustomRandom() {
    int V;
    double density;
    cout << "\nEnter number of nodes: ";
    cin >> V;
    cout << "Enter edge density (0.0 sparse - 1.0 fully connected): ";
    cin >> density;

    if (V < 2) { cout << "Need at least 2 nodes.\n"; return; }
    if (density < 0.0) density = 0.0;
    if (density > 1.0) density = 1.0;

    NetworkGraph g = generateRandomGraph(V, density, (unsigned)time(NULL));
    cout << "Generated graph: " << V << " nodes, "
         << g.edges.size() << " edges.\n";

    mt19937 rng((unsigned)time(NULL));
    uniform_int_distribution<int> srcDist(0, V - 1);
    int src = srcDist(rng);

    bool verbose = (V <= 20); // only print steps for small graphs

    cout << "\n--- DijkstraVisitor from node " << src << " ---\n";
    DijkstraVisitor dv(src, verbose);
    g.accept(dv);
    cout << "\nDijkstra runtime: " << dv.lastRuntimeMicros << " us\n";

    cout << "\n--- KruskalVisitor ---\n";
    KruskalVisitor kv(verbose);
    g.accept(kv);
    cout << "\nKruskal runtime: " << kv.lastRuntimeMicros << " us\n";
    cout << "MST total cost:  " << kv.totalCost << "\n";
}


// ============================================================
// MAIN: menu
// ============================================================
int main() {
    while (true) {
        cout << "\n============================================================\n";
        cout << "Regional Network Project - Visitor Pattern Synthesis\n";
        cout << "============================================================\n";
        cout << "1. Run small demo (7-room office)\n";
        cout << "2. Run custom random graph\n";
        cout << "3. Run empirical benchmark (multiple sizes)\n";
        cout << "4. Quit\n";
        cout << "Choice: ";

        int choice;
        if (!(cin >> choice)) break;

        if (choice == 1) {
            runDemo();
        } else if (choice == 2) {
            runCustomRandom();
        } else if (choice == 3) {
            double density;
            cout << "Enter edge density for benchmark (0.0 - 1.0): ";
            cin >> density;
            if (density < 0.0) density = 0.0;
            if (density > 1.0) density = 1.0;
            runBenchmark(density);
        } else if (choice == 4) {
            break;
        } else {
            cout << "Invalid choice.\n";
        }
    }

    return 0;
}
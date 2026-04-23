// ============================================================
// Regional Network Project
// Visitor Pattern synthesis of:
//   - Madison's Dijkstra's Algorithm with Fibonacci Heap (Presentation 2)
//   - Ethan's Kruskal's MST with Strategy Pattern (Presentation 2)
//
// Authors: Jared Gregoire & Partner
// ============================================================

#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <limits.h>
#include <algorithm>
#include <unordered_map>
#include <chrono>
#include <random>
#include <iomanip>
#include <cmath>
using namespace std;


// ============================================================
// SECTION 1: Forward declarations + Visitor Interface
// ============================================================
class NetworkGraph;

class GraphVisitor {
public:
    virtual void visit(NetworkGraph& graph) = 0;
    virtual ~GraphVisitor() {}
};


// ============================================================
// SECTION 2: NetworkGraph (the "visitable" element)
// ============================================================
struct NetworkEdge {
    int u, v;
    int latency; // ms - used by Dijkstra
    int cost;    // $  - used by Kruskal
};

class NetworkGraph {
public:
    int V;
    vector<NetworkEdge> edges;
    vector< vector< pair<int,int> > > adj; // (neighbor, latency)

    NetworkGraph(int V) {
        this->V = V;
        adj.resize(V);
    }

    void addEdge(int u, int v, int latency, int cost) {
        NetworkEdge e;
        e.u = u; e.v = v;
        e.latency = latency; e.cost = cost;
        edges.push_back(e);
        adj[u].push_back(make_pair(v, latency));
        adj[v].push_back(make_pair(u, latency));
    }

    void accept(GraphVisitor& visitor) {
        visitor.visit(*this);
    }
};


// ============================================================
// SECTION 3: Step snapshots for visualization
// ============================================================
struct DijkstraStep {
    int currentNode;
    vector<int> distances;
    int relaxedEdgeU;
    int relaxedEdgeV;
    string description;
};

struct KruskalStep {
    int edgeIdx;
    bool accepted;
    vector<int> parents;
    vector<int> mstEdgeIndices;
    int totalCostSoFar;
    string description;
};


// ============================================================
// SECTION 4: Madison's Dijkstra, wrapped as a Visitor
// ============================================================
struct Node {
    int vertex;
    int key;
};

class FibHeap {
public:
    vector<Node*> heap;

    void insert(Node* n) { heap.push_back(n); }

    Node* extractMin() {
        if (heap.empty()) return NULL;
        int minIndex = 0;
        for (int i = 1; i < (int)heap.size(); i++) {
            if (heap[i]->key < heap[minIndex]->key) minIndex = i;
        }
        Node* minNode = heap[minIndex];
        heap.erase(heap.begin() + minIndex);
        return minNode;
    }

    void decreaseKey(Node* n, int newKey) { n->key = newKey; }
    bool isEmpty() { return heap.empty(); }
};

class DijkstraVisitor : public GraphVisitor {
private:
    int source;
    bool verbose;
public:
    vector<int> dist;
    vector<DijkstraStep> steps;
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
        steps.clear();

        FibHeap heap;
        vector<Node*> nodes(V);

        for (int i = 0; i < V; i++) {
            nodes[i] = new Node;
            nodes[i]->vertex = i;
            nodes[i]->key = INT_MAX;
            heap.insert(nodes[i]);
        }

        dist[source] = 0;
        heap.decreaseKey(nodes[source], 0);

        // initial snapshot
        {
            DijkstraStep s;
            s.currentNode = -1;
            s.distances = dist;
            s.relaxedEdgeU = -1;
            s.relaxedEdgeV = -1;
            s.description = "Initialize: all distances set to infinity, source set to 0.";
            steps.push_back(s);
        }

        while (!heap.isEmpty()) {
            Node* minNode = heap.extractMin();
            int u = minNode->vertex;
            if (minNode->key == INT_MAX) break;

            if (verbose) cout << "Processing node: " << u << endl;

            // snapshot: node selected
            {
                DijkstraStep s;
                s.currentNode = u;
                s.distances = dist;
                s.relaxedEdgeU = -1;
                s.relaxedEdgeV = -1;
                stringstream ss;
                ss << "Extract-Min: selected node " << u
                   << " with distance " << dist[u] << ".";
                s.description = ss.str();
                steps.push_back(s);
            }

            for (int i = 0; i < (int)graph.adj[u].size(); i++) {
                int v = graph.adj[u][i].first;
                int weight = graph.adj[u][i].second;

                if (dist[u] != INT_MAX && dist[u] + weight < dist[v]) {
                    dist[v] = dist[u] + weight;
                    heap.decreaseKey(nodes[v], dist[v]);

                    if (verbose) {
                        cout << "  Updated distance of " << v
                             << " to " << dist[v] << " (Decrease-Key)" << endl;
                    }

                    DijkstraStep s;
                    s.currentNode = u;
                    s.distances = dist;
                    s.relaxedEdgeU = u;
                    s.relaxedEdgeV = v;
                    stringstream ss;
                    ss << "Relax edge " << u << "-" << v
                       << ": updated dist[" << v << "] = " << dist[v] << ".";
                    s.description = ss.str();
                    steps.push_back(s);
                }
            }
        }

        for (int i = 0; i < V; i++) delete nodes[i];

        {
            DijkstraStep s;
            s.currentNode = -1;
            s.distances = dist;
            s.relaxedEdgeU = -1;
            s.relaxedEdgeV = -1;
            s.description = "Done. Shortest path tree complete.";
            steps.push_back(s);
        }

        auto end = chrono::high_resolution_clock::now();
        lastRuntimeMicros = chrono::duration_cast<chrono::microseconds>(end - start).count();

        if (verbose) {
            cout << "\nFinal shortest latencies from node " << source << ":\n";
            for (int i = 0; i < V; i++) {
                if (dist[i] == INT_MAX) cout << "Node " << i << " : unreachable\n";
                else cout << "Node " << i << " : " << dist[i] << " ms\n";
            }
        }
    }

    int getSource() const { return source; }
};


// ============================================================
// SECTION 5: Ethan's Kruskal, wrapped as a Visitor
// ============================================================
class DSU {
private:
    vector<int> parent, dsu_rank;
public:
    DSU(int n) {
        parent.resize(n);
        dsu_rank.resize(n, 0);
        for (int i = 0; i < n; i++) parent[i] = i;
    }

    int find(int x) {
        if (parent[x] != x) parent[x] = find(parent[x]);
        return parent[x];
    }

    void unite(int a, int b) {
        int rootA = find(a);
        int rootB = find(b);
        if (rootA != rootB) {
            if (dsu_rank[rootA] < dsu_rank[rootB]) parent[rootA] = rootB;
            else if (dsu_rank[rootA] > dsu_rank[rootB]) parent[rootB] = rootA;
            else { parent[rootB] = rootA; dsu_rank[rootA]++; }
        }
    }

    vector<int> getParents() const { return parent; }

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
                if (j < (int)sortedGroups[i].size() - 1) cout << ", ";
            }
            cout << "}";
            if (i < (int)sortedGroups.size() - 1) cout << ", ";
        }
        cout << "\n";
    }
};

class KruskalVisitor : public GraphVisitor {
private:
    bool verbose;
    vector<char> labels;
public:
    int totalCost;
    vector<NetworkEdge> mst;
    vector<NetworkEdge> sortedEdges;
    vector<KruskalStep> steps;
    long long lastRuntimeMicros;

    KruskalVisitor(bool verbose = true, vector<char> labels = {}) {
        this->verbose = verbose;
        this->labels = labels;
        this->totalCost = 0;
        this->lastRuntimeMicros = 0;
    }

    void visit(NetworkGraph& graph) override {
        auto start = chrono::high_resolution_clock::now();

        sortedEdges = graph.edges;
        sort(sortedEdges.begin(), sortedEdges.end(),
            [](NetworkEdge a, NetworkEdge b) { return a.cost < b.cost; });

        DSU dsu(graph.V);
        totalCost = 0;
        mst.clear();
        steps.clear();

        vector<int> mstIndices;

        bool useLabels = ((int)labels.size() == graph.V);

        if (verbose) {
            cout << "Initial State:\n";
            if (useLabels) dsu.printSets(labels);
            cout << "------------------------\n";
        }

        {
            KruskalStep s;
            s.edgeIdx = -1;
            s.accepted = false;
            s.parents = dsu.getParents();
            s.mstEdgeIndices = mstIndices;
            s.totalCostSoFar = 0;
            s.description = "Initialize: each node is its own subset. Edges sorted by cost.";
            steps.push_back(s);
        }

        int step = 1;
        for (int idx = 0; idx < (int)sortedEdges.size(); idx++) {
            NetworkEdge &e = sortedEdges[idx];

            if (verbose) {
                if (useLabels) {
                    cout << "Step " << step++ << ": Considering edge "
                         << labels[e.u] << "-" << labels[e.v]
                         << " (cost " << e.cost << ")\n";
                } else {
                    cout << "Step " << step++ << ": Considering edge "
                         << e.u << "-" << e.v << " (cost " << e.cost << ")\n";
                }
            }

            bool accepted = false;
            if (dsu.find(e.u) != dsu.find(e.v)) {
                if (verbose) {
                    cout << " -> Adding edge\n";
                    if (useLabels) cout << " -> Union(" << labels[e.u]
                                         << ", " << labels[e.v] << ")\n";
                }
                dsu.unite(e.u, e.v);
                mst.push_back(e);
                mstIndices.push_back(idx);
                totalCost += e.cost;
                accepted = true;
                if (verbose && useLabels) dsu.printSets(labels);
            } else if (verbose) {
                cout << " -> Skipping (would form cycle)\n";
            }

            KruskalStep s;
            s.edgeIdx = idx;
            s.accepted = accepted;
            s.parents = dsu.getParents();
            s.mstEdgeIndices = mstIndices;
            s.totalCostSoFar = totalCost;
            stringstream ss;
            ss << "Consider edge " << e.u << "-" << e.v
               << " (cost " << e.cost << "): "
               << (accepted ? "added to MST." : "skipped (would form cycle).");
            s.description = ss.str();
            steps.push_back(s);

            if (verbose) cout << "------------------------\n";
            if ((int)mst.size() == graph.V - 1) break;
        }

        auto end = chrono::high_resolution_clock::now();
        lastRuntimeMicros = chrono::duration_cast<chrono::microseconds>(end - start).count();

        if (verbose) {
            cout << "\nFinal MST (cheapest redundancy backbone):\n";
            for (auto &e : mst) {
                if (useLabels) cout << labels[e.u] << "-" << labels[e.v]
                                    << " (cost " << e.cost << ")\n";
                else cout << e.u << "-" << e.v << " (cost " << e.cost << ")\n";
            }
            cout << "Total Cost: " << totalCost << endl;
        }
    }
};


// ============================================================
// SECTION 6: Random graph generator
// ============================================================
NetworkGraph generateRandomGraph(int V, double density, unsigned seed) {
    NetworkGraph g(V);
    mt19937 rng(seed);
    uniform_int_distribution<int> latDist(1, 100);
    uniform_int_distribution<int> costDist(1, 1000);

    vector<int> perm(V);
    for (int i = 0; i < V; i++) perm[i] = i;
    shuffle(perm.begin(), perm.end(), rng);

    vector<bool> edgeExists((long long)V * V, false);

    for (int i = 1; i < V; i++) {
        uniform_int_distribution<int> parentDist(0, i-1);
        int parent = perm[parentDist(rng)];
        int child = perm[i];
        int a = min(parent, child);
        int b = max(parent, child);
        g.addEdge(parent, child, latDist(rng), costDist(rng));
        edgeExists[(long long)a * V + b] = true;
    }

    long long maxEdges = (long long)V * (V - 1) / 2;
    long long targetEdges = (long long)(density * maxEdges);
    if (targetEdges < V - 1) targetEdges = V - 1;
    if (targetEdges > maxEdges) targetEdges = maxEdges;

    uniform_int_distribution<int> nodeDist(0, V-1);
    long long safetyCounter = 0;
    long long safetyMax = targetEdges * 10 + 100;

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
// SECTION 7: Benchmark
// ============================================================
void runBenchmark(double density) {
    vector<int> sizes = {10, 50, 100, 250, 500, 1000, 2000};

    cout << "\n============================================================\n";
    cout << "EMPIRICAL BENCHMARK (density = " << density << ")\n";
    cout << "============================================================\n";
    cout << left << setw(8) << "V" << setw(10) << "E"
         << setw(18) << "Dijkstra (us)" << setw(18) << "Kruskal (us)" << "\n";
    cout << string(54, '-') << "\n";

    mt19937 sourceRng(12345);

    for (int V : sizes) {
        NetworkGraph g = generateRandomGraph(V, density, 42 + V);
        uniform_int_distribution<int> srcDist(0, V - 1);
        int randomSource = srcDist(sourceRng);

        DijkstraVisitor dv(randomSource, false);
        g.accept(dv);

        KruskalVisitor kv(false);
        g.accept(kv);

        cout << left << setw(8) << V << setw(10) << g.edges.size()
             << setw(18) << dv.lastRuntimeMicros
             << setw(18) << kv.lastRuntimeMicros << "\n";
    }

    cout << "\nTheoretical bounds for reference:\n";
    cout << "  Dijkstra (this impl, linear-scan priority queue): O(V^2 + E)\n";
    cout << "  Kruskal  (with DSU + path compression):           O(E log E)\n";
}


// ============================================================
// SECTION 8: HTML export
// ============================================================
struct NodePos { double x, y; };

vector<NodePos> computeGridLayout(int V, double width, double height, double padding) {
    vector<NodePos> pos(V);
    int cols = (int)ceil(sqrt((double)V));
    int rows = (int)ceil((double)V / cols);

    double usableW = width - 2 * padding;
    double usableH = height - 2 * padding;
    double stepX = (cols > 1) ? usableW / (cols - 1) : 0;
    double stepY = (rows > 1) ? usableH / (rows - 1) : 0;

    for (int i = 0; i < V; i++) {
        int r = i / cols;
        int c = i % cols;
        pos[i].x = padding + c * stepX;
        pos[i].y = padding + r * stepY;
    }
    return pos;
}

string jsonEscape(const string& s) {
    string out;
    for (char c : s) {
        switch (c) {
            case '"':  out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n";  break;
            case '\r': out += "\\r";  break;
            case '\t': out += "\\t";  break;
            default:   out += c;
        }
    }
    return out;
}

void exportHTML(const string& filename,
                NetworkGraph& graph,
                DijkstraVisitor& dv,
                KruskalVisitor& kv,
                const vector<char>& labels)
{
    const double W = 600, H = 500, PAD = 60;
    vector<NodePos> pos = computeGridLayout(graph.V, W, H, PAD);

    bool useLabels = ((int)labels.size() == graph.V);

    ofstream f(filename);
    if (!f) { cout << "Could not open " << filename << " for writing.\n"; return; }

    // graph JSON
    stringstream gjs;
    gjs << "{\"V\":" << graph.V << ",\"nodes\":[";
    for (int i = 0; i < graph.V; i++) {
        if (i) gjs << ",";
        string lbl = useLabels ? string(1, labels[i]) : to_string(i);
        gjs << "{\"id\":" << i << ",\"label\":\"" << lbl
            << "\",\"x\":" << pos[i].x << ",\"y\":" << pos[i].y << "}";
    }
    gjs << "],\"edges\":[";
    for (int i = 0; i < (int)graph.edges.size(); i++) {
        if (i) gjs << ",";
        const NetworkEdge& e = graph.edges[i];
        gjs << "{\"u\":" << e.u << ",\"v\":" << e.v
            << ",\"latency\":" << e.latency << ",\"cost\":" << e.cost << "}";
    }
    gjs << "]}";

    // dijkstra JSON
    stringstream djs;
    djs << "{\"source\":" << dv.getSource() << ",\"steps\":[";
    for (int i = 0; i < (int)dv.steps.size(); i++) {
        if (i) djs << ",";
        const DijkstraStep& s = dv.steps[i];
        djs << "{\"current\":" << s.currentNode
            << ",\"relaxU\":" << s.relaxedEdgeU
            << ",\"relaxV\":" << s.relaxedEdgeV
            << ",\"dist\":[";
        for (int j = 0; j < (int)s.distances.size(); j++) {
            if (j) djs << ",";
            if (s.distances[j] == INT_MAX) djs << "null";
            else djs << s.distances[j];
        }
        djs << "],\"desc\":\"" << jsonEscape(s.description) << "\"}";
    }
    djs << "]}";

    // kruskal JSON
    stringstream kjs;
    kjs << "{\"sortedEdges\":[";
    for (int i = 0; i < (int)kv.sortedEdges.size(); i++) {
        if (i) kjs << ",";
        const NetworkEdge& e = kv.sortedEdges[i];
        kjs << "{\"u\":" << e.u << ",\"v\":" << e.v << ",\"cost\":" << e.cost << "}";
    }
    kjs << "],\"steps\":[";
    for (int i = 0; i < (int)kv.steps.size(); i++) {
        if (i) kjs << ",";
        const KruskalStep& s = kv.steps[i];
        kjs << "{\"edgeIdx\":" << s.edgeIdx
            << ",\"accepted\":" << (s.accepted ? "true" : "false")
            << ",\"totalCost\":" << s.totalCostSoFar
            << ",\"parents\":[";
        for (int j = 0; j < (int)s.parents.size(); j++) {
            if (j) kjs << ",";
            kjs << s.parents[j];
        }
        kjs << "],\"mst\":[";
        for (int j = 0; j < (int)s.mstEdgeIndices.size(); j++) {
            if (j) kjs << ",";
            kjs << s.mstEdgeIndices[j];
        }
        kjs << "],\"desc\":\"" << jsonEscape(s.description) << "\"}";
    }
    kjs << "]}";

    f << "<!DOCTYPE html>\n<html><head><meta charset='utf-8'>\n";
    f << "<title>Regional Network - Algorithm Visualizer</title>\n";
    f << "<style>\n";
    f << "body { font-family: -apple-system, Segoe UI, sans-serif; background: #f5f5f7; margin: 0; padding: 20px; }\n";
    f << "h1 { margin: 0 0 10px 0; }\n";
    f << ".sub { color: #666; margin-bottom: 20px; }\n";
    f << ".tabs { display: flex; gap: 4px; margin-bottom: 0; }\n";
    f << ".tab { padding: 10px 20px; background: #ddd; cursor: pointer; border-radius: 6px 6px 0 0; user-select: none; }\n";
    f << ".tab.active { background: white; font-weight: bold; }\n";
    f << ".panel { background: white; padding: 20px; border-radius: 0 6px 6px 6px; }\n";
    f << ".panel.hidden { display: none; }\n";
    f << ".layout { display: flex; gap: 20px; flex-wrap: wrap; }\n";
    f << ".graph-area { flex: 0 0 auto; }\n";
    f << ".info-area { flex: 1; min-width: 300px; }\n";
    f << "svg { background: #fafafa; border: 1px solid #e0e0e0; border-radius: 4px; }\n";
    f << ".controls { margin: 10px 0; display: flex; gap: 8px; align-items: center; }\n";
    f << "button { padding: 8px 16px; background: #0066cc; color: white; border: none; border-radius: 4px; cursor: pointer; font-size: 14px; }\n";
    f << "button:hover { background: #0052a3; } button:disabled { background: #aaa; cursor: not-allowed; }\n";
    f << ".step-counter { font-weight: bold; padding: 0 10px; }\n";
    f << ".desc { background: #f0f4f8; padding: 12px; border-radius: 4px; margin: 10px 0; min-height: 40px; }\n";
    f << "table { border-collapse: collapse; margin-top: 10px; font-size: 13px; }\n";
    f << "th, td { border: 1px solid #ddd; padding: 6px 10px; text-align: center; }\n";
    f << "th { background: #f0f0f0; }\n";
    f << "tr.highlight td { background: #fff4cc; font-weight: bold; }\n";
    f << "tr.accepted td { background: #d4edda; }\n";
    f << "tr.rejected td { background: #f8d7da; color: #666; }\n";
    f << "text { font-family: inherit; pointer-events: none; }\n";
    f << "</style>\n</head>\n<body>\n";

    f << "<h1>Regional Network Algorithm Visualizer</h1>\n";
    f << "<div class='sub'>Visitor Pattern synthesis: Dijkstra (fastest route) + Kruskal (cheapest redundancy)</div>\n";

    f << "<div class='tabs'>\n";
    f << "<div class='tab active' onclick='showTab(event, \"dijkstra\")'>Dijkstra's Algorithm</div>\n";
    f << "<div class='tab' onclick='showTab(event, \"kruskal\")'>Kruskal's Algorithm</div>\n";
    f << "</div>\n";

    f << "<div id='panel-dijkstra' class='panel'>\n";
    f << "<div class='layout'>\n";
    f << "<div class='graph-area'><svg id='svg-d' width='" << W << "' height='" << H << "'></svg></div>\n";
    f << "<div class='info-area'>\n";
    f << "<div class='controls'>\n";
    f << "<button onclick='dPrev()'>Previous</button>\n";
    f << "<button onclick='dNext()'>Next</button>\n";
    f << "<button onclick='dReset()'>Reset</button>\n";
    f << "<span class='step-counter' id='d-counter'>Step 0</span>\n";
    f << "</div>\n";
    f << "<div class='desc' id='d-desc'></div>\n";
    f << "<h3>Distance Table (latency from source)</h3>\n";
    f << "<div id='d-table'></div>\n";
    f << "</div></div></div>\n";

    f << "<div id='panel-kruskal' class='panel hidden'>\n";
    f << "<div class='layout'>\n";
    f << "<div class='graph-area'><svg id='svg-k' width='" << W << "' height='" << H << "'></svg></div>\n";
    f << "<div class='info-area'>\n";
    f << "<div class='controls'>\n";
    f << "<button onclick='kPrev()'>Previous</button>\n";
    f << "<button onclick='kNext()'>Next</button>\n";
    f << "<button onclick='kReset()'>Reset</button>\n";
    f << "<span class='step-counter' id='k-counter'>Step 0</span>\n";
    f << "</div>\n";
    f << "<div class='desc' id='k-desc'></div>\n";
    f << "<h3>Sorted Edges</h3>\n";
    f << "<div id='k-table'></div>\n";
    f << "<div id='k-total' style='margin-top:10px;font-weight:bold;'></div>\n";
    f << "</div></div></div>\n";

    f << "<script>\n";
    f << "const GRAPH = " << gjs.str() << ";\n";
    f << "const DIJK  = " << djs.str() << ";\n";
    f << "const KRUS  = " << kjs.str() << ";\n";
    f << R"JS(

function showTab(ev, name) {
  document.querySelectorAll('.tab').forEach(t => t.classList.remove('active'));
  document.querySelectorAll('.panel').forEach(p => p.classList.add('hidden'));
  ev.target.classList.add('active');
  document.getElementById('panel-' + name).classList.remove('hidden');
}

function edgeKey(u, v) { return Math.min(u,v) + '-' + Math.max(u,v); }

function drawNodes(svg, highlights) {
  GRAPH.nodes.forEach(n => {
    const c = document.createElementNS('http://www.w3.org/2000/svg','circle');
    c.setAttribute('cx', n.x); c.setAttribute('cy', n.y); c.setAttribute('r', 22);
    c.setAttribute('fill', highlights[n.id] || '#ffffff');
    c.setAttribute('stroke', '#333'); c.setAttribute('stroke-width', 2);
    svg.appendChild(c);
    const t = document.createElementNS('http://www.w3.org/2000/svg','text');
    t.setAttribute('x', n.x); t.setAttribute('y', n.y + 5);
    t.setAttribute('text-anchor', 'middle'); t.setAttribute('font-size', 14);
    t.setAttribute('font-weight', 'bold');
    t.textContent = n.label; svg.appendChild(t);
  });
}

function drawEdge(svg, u, v, color, width, label, labelColor) {
  const n1 = GRAPH.nodes[u], n2 = GRAPH.nodes[v];
  const line = document.createElementNS('http://www.w3.org/2000/svg','line');
  line.setAttribute('x1', n1.x); line.setAttribute('y1', n1.y);
  line.setAttribute('x2', n2.x); line.setAttribute('y2', n2.y);
  line.setAttribute('stroke', color); line.setAttribute('stroke-width', width);
  svg.appendChild(line);
  if (label !== null) {
    const mx = (n1.x + n2.x) / 2, my = (n1.y + n2.y) / 2;
    const bg = document.createElementNS('http://www.w3.org/2000/svg','rect');
    const tw = String(label).length * 7 + 8;
    bg.setAttribute('x', mx - tw/2); bg.setAttribute('y', my - 9);
    bg.setAttribute('width', tw); bg.setAttribute('height', 16);
    bg.setAttribute('fill', 'white'); bg.setAttribute('stroke', '#ccc');
    svg.appendChild(bg);
    const t = document.createElementNS('http://www.w3.org/2000/svg','text');
    t.setAttribute('x', mx); t.setAttribute('y', my + 3);
    t.setAttribute('text-anchor', 'middle'); t.setAttribute('font-size', 11);
    t.setAttribute('fill', labelColor || '#333'); t.textContent = label;
    svg.appendChild(t);
  }
}

// ----- Dijkstra -----
let dIdx = 0;
function dRender() {
  const svg = document.getElementById('svg-d');
  svg.innerHTML = '';
  const s = DIJK.steps[dIdx];

  GRAPH.edges.forEach(e => {
    let color = '#cccccc', width = 2;
    if ((s.relaxU === e.u && s.relaxV === e.v) || (s.relaxU === e.v && s.relaxV === e.u)) {
      color = '#28a745'; width = 4;
    }
    drawEdge(svg, e.u, e.v, color, width, e.latency, '#555');
  });

  const hl = {};
  hl[DIJK.source] = '#cce5ff';
  if (s.current >= 0) hl[s.current] = '#ffd699';
  drawNodes(svg, hl);

  document.getElementById('d-counter').textContent =
    'Step ' + (dIdx + 1) + ' / ' + DIJK.steps.length;
  document.getElementById('d-desc').textContent = s.desc;

  let html = '<table><tr><th>Node</th>';
  GRAPH.nodes.forEach(n => html += '<th>' + n.label + '</th>');
  html += '</tr><tr><td><b>dist</b></td>';
  s.dist.forEach(d => html += '<td>' + (d === null ? '&infin;' : d) + '</td>');
  html += '</tr></table>';
  document.getElementById('d-table').innerHTML = html;
}
function dNext()  { if (dIdx < DIJK.steps.length - 1) { dIdx++; dRender(); } }
function dPrev()  { if (dIdx > 0) { dIdx--; dRender(); } }
function dReset() { dIdx = 0; dRender(); }

// ----- Kruskal -----
let kIdx = 0;
function kRender() {
  const svg = document.getElementById('svg-k');
  svg.innerHTML = '';
  const s = KRUS.steps[kIdx];

  const mstSet = new Set(s.mst);

  const sortedByKey = {};
  KRUS.sortedEdges.forEach((e, i) => { sortedByKey[edgeKey(e.u, e.v)] = i; });

  GRAPH.edges.forEach(e => {
    const sidx = sortedByKey[edgeKey(e.u, e.v)];
    let color = '#cccccc', width = 2;
    if (mstSet.has(sidx)) { color = '#28a745'; width = 4; }
    if (s.edgeIdx === sidx) {
      color = s.accepted ? '#28a745' : '#dc3545';
      width = 5;
    }
    drawEdge(svg, e.u, e.v, color, width, e.cost, '#555');
  });

  const palette = ['#cce5ff','#ffd699','#d4edda','#f8d7da','#e2d4f0','#fff4cc','#d1ecf1','#f4cccc','#d9ead3','#ead1dc'];
  const rootColor = {};
  const hl = {};
  function findRoot(x) { while (s.parents[x] !== x) x = s.parents[x]; return x; }
  GRAPH.nodes.forEach(n => {
    const r = findRoot(n.id);
    if (!(r in rootColor)) rootColor[r] = palette[Object.keys(rootColor).length % palette.length];
    hl[n.id] = rootColor[r];
  });
  drawNodes(svg, hl);

  document.getElementById('k-counter').textContent =
    'Step ' + (kIdx + 1) + ' / ' + KRUS.steps.length;
  document.getElementById('k-desc').textContent = s.desc;
  document.getElementById('k-total').textContent =
    'Total MST Cost So Far: ' + s.totalCost;

  let html = '<table><tr><th>#</th><th>Edge</th><th>Cost</th><th>Status</th></tr>';
  KRUS.sortedEdges.forEach((e, i) => {
    let cls = '';
    const uLabel = GRAPH.nodes[e.u].label, vLabel = GRAPH.nodes[e.v].label;
    let status = 'pending';
    if (i === s.edgeIdx) { cls = 'highlight'; status = s.accepted ? 'accepted' : 'rejected'; }
    else if (mstSet.has(i)) { cls = 'accepted'; status = 'in MST'; }
    else if (i < s.edgeIdx) { cls = 'rejected'; status = 'skipped'; }
    html += '<tr class="' + cls + '"><td>' + (i+1) + '</td><td>'
          + uLabel + '-' + vLabel + '</td><td>' + e.cost + '</td><td>' + status + '</td></tr>';
  });
  html += '</table>';
  document.getElementById('k-table').innerHTML = html;
}
function kNext()  { if (kIdx < KRUS.steps.length - 1) { kIdx++; kRender(); } }
function kPrev()  { if (kIdx > 0) { kIdx--; kRender(); } }
function kReset() { kIdx = 0; kRender(); }

dRender();
kRender();
)JS";
    f << "</script>\n</body></html>\n";

    f.close();
    cout << "HTML visualization written to: " << filename << "\n";
}


// ============================================================
// SECTION 9: Demo mode
// ============================================================
void runDemo(bool exportVisualization) {
    cout << "\n============================================================\n";
    cout << "DEMO: Small office network (7 rooms)\n";
    cout << "============================================================\n";
    cout << "Nodes represent rooms/offices. Edges have two weights:\n";
    cout << "  latency (ms)  -> Dijkstra uses this for fastest route\n";
    cout << "  cost    ($)   -> Kruskal  uses this for cheapest MST\n\n";

    NetworkGraph g(7);
    vector<char> labels = {'A','B','C','D','E','F','G'};

    g.addEdge(4, 3, 2, 1);
    g.addEdge(6, 2, 3, 1);
    g.addEdge(5, 0, 4, 2);
    g.addEdge(0, 1, 5, 3);
    g.addEdge(1, 6, 6, 4);
    g.addEdge(0, 6, 7, 5);
    g.addEdge(1, 2, 4, 5);
    g.addEdge(3, 2, 3, 6);
    g.addEdge(5, 3, 8, 7);
    g.addEdge(3, 6, 5, 8);
    g.addEdge(0, 3, 9, 12);

    cout << "--- Visitor 1: DijkstraVisitor from node A (fastest routes) ---\n\n";
    DijkstraVisitor dv(0, true);
    g.accept(dv);

    cout << "\n--- Visitor 2: KruskalVisitor (cheapest redundancy backbone) ---\n\n";
    KruskalVisitor kv(true, labels);
    g.accept(kv);

    if (exportVisualization) {
        exportHTML("network_demo.html", g, dv, kv, labels);
    }
}


// ============================================================
// SECTION 10: Custom random graph
// ============================================================
void runCustomRandom(bool exportVisualization) {
    int V;
    double density;
    cout << "\nEnter number of nodes: ";
    cin >> V;
    cout << "Enter edge density (0.0 - 1.0): ";
    cin >> density;

    if (V < 2) { cout << "Need at least 2 nodes.\n"; return; }
    if (density < 0.0) density = 0.0;
    if (density > 1.0) density = 1.0;

    NetworkGraph g = generateRandomGraph(V, density, (unsigned)time(NULL));
    cout << "Generated graph: " << V << " nodes, " << g.edges.size() << " edges.\n";

    mt19937 rng((unsigned)time(NULL));
    uniform_int_distribution<int> srcDist(0, V - 1);
    int src = srcDist(rng);

    bool verbose = (V <= 20);

    cout << "\n--- DijkstraVisitor from node " << src << " ---\n";
    DijkstraVisitor dv(src, verbose);
    g.accept(dv);
    cout << "\nDijkstra runtime: " << dv.lastRuntimeMicros << " us\n";

    cout << "\n--- KruskalVisitor ---\n";
    KruskalVisitor kv(verbose);
    g.accept(kv);
    cout << "\nKruskal runtime: " << kv.lastRuntimeMicros << " us\n";
    cout << "MST total cost:  " << kv.totalCost << "\n";

    if (exportVisualization) {
        if (V > 25) {
            cout << "\nGraph too large for clean visualization (>25 nodes). Skipping HTML export.\n";
        } else {
            exportHTML("network_custom.html", g, dv, kv, {});
        }
    }
}


// ============================================================
// MAIN
// ============================================================
int main() {
    while (true) {
        cout << "\n============================================================\n";
        cout << "Regional Network Project - Visitor Pattern Synthesis\n";
        cout << "============================================================\n";
        cout << "1. Run small demo (7-room office)\n";
        cout << "2. Run demo + export HTML visualization\n";
        cout << "3. Run custom random graph\n";
        cout << "4. Run custom random graph + export HTML\n";
        cout << "5. Run empirical benchmark\n";
        cout << "6. Quit\n";
        cout << "Choice: ";

        int choice;
        if (!(cin >> choice)) break;

        if (choice == 1) runDemo(false);
        else if (choice == 2) runDemo(true);
        else if (choice == 3) runCustomRandom(false);
        else if (choice == 4) runCustomRandom(true);
        else if (choice == 5) {
            double density;
            cout << "Enter edge density (0.0 - 1.0): ";
            cin >> density;
            if (density < 0.0) density = 0.0;
            if (density > 1.0) density = 1.0;
            runBenchmark(density);
        }
        else if (choice == 6) break;
        else cout << "Invalid choice.\n";
    }

    return 0;
}
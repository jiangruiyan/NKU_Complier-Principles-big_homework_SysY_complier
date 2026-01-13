#include <dom_analyzer.h>
#include <debug.h>
#include <cassert>
#include <functional>
#include <algorithm>

/*
 * Lengauer–Tarjan (LT) 支配计算算法简述
 *
 * 基本概念（针对一张以入口 s 为根的控制流图）：
 * - 支配：若结点 d 在所有从 s 到 u 的路径上都出现，则称 d 支配 u。
 * - 直接支配 idom(u)：在所有“严格支配”（不含 u 本身）的结点中，离 u 最近的那个。
 * - DFS 树 T：从 s 做一次 DFS，得到父亲 parent 与 DFS 进入次序 dfn（本文用 1..n 的“次序号”表示）。
 * - 半支配 sdom(u)：满足“从某个结点 v 可以走到 u，且路径上除端点外的所有结点 dfn 都大于 dfn(u)”的所有 v 中，dfn
 * 最小者。 直观理解：sdom(u) 是一类“绕过更小 dfn 的点”到达 u 的最早可能起点，它是计算 idom 的关键桥梁。
 *
 * LT 的两条核心公式（以 DFS 序回溯顺序计算）：
 * - sdom(u) = min{ dfn(v) | v 是 u 的前驱且 dfn(v) < dfn(u) } 与 { sdom(eval(p)) | p 是 u 的前驱且 dfn(p) > dfn(u) }
 * 的最小者。 其中 eval/Link 由并查集维护，用于在“沿 DFS 树向上压缩”的同时，记录路径上的“最小祖先”。
 * - idom(u) = ( sdom(u) == sdom(eval(u)) ? parent(u) : eval(u) )，并最终做一遍链压缩。
 */

using namespace std;

DomAnalyzer::DomAnalyzer() {}

void DomAnalyzer::solve(const vector<vector<int>>& graph, const vector<int>& entry_points, bool reverse)
{
    int node_count = graph.size();

    int                 virtual_source = node_count;
    vector<vector<int>> working_graph;

    if (!reverse)
    {
        working_graph = graph;
        working_graph.push_back(vector<int>());
        for (int entry : entry_points) working_graph[virtual_source].push_back(entry);
    }
    else
    {
        working_graph.resize(node_count + 1);
        for (int u = 0; u < node_count; ++u)
            for (int v : graph[u]) working_graph[v].push_back(u);

        // working_graph.push_back(vector<int>());
        for (int exit : entry_points) working_graph[virtual_source].push_back(exit);
    }

    build(working_graph, node_count + 1, virtual_source, entry_points);
}

void DomAnalyzer::build(
    const vector<vector<int>>& working_graph, int node_count, int virtual_source, const std::vector<int>& entry_points)
{
    (void)entry_points;
    vector<vector<int>> backward_edges(node_count);
    // 构建反向边表 backward_edges[v] = { 所有指向 v 的前驱 }
    for (int u = 0; u < node_count; ++u)
    {
        for (int v : working_graph[u])
        {
            if (v >= 0 && v < node_count) backward_edges[v].push_back(u);
        }
    }

    dom_tree.clear();
    dom_tree.resize(node_count);
    dom_frontier.clear();
    dom_frontier.resize(node_count);
    imm_dom.clear();
    imm_dom.resize(node_count);

    // dfn 从 1 开始，避免根被误判为“未访问”
    int                 dfs_count = 0;
    vector<int>         block_to_dfs(node_count, 0), dfs_to_block(node_count + 1, -1), parent(node_count, -1);
    vector<int>         semi_dom(node_count, 0);
    vector<int>         dsu_parent(node_count), min_ancestor(node_count);
    vector<vector<int>> semi_children(node_count);

    for (int i = 0; i < node_count; ++i)
    {
        dsu_parent[i]   = i;
        min_ancestor[i] = i;
        semi_dom[i]     = 0;  // 尚未计算
    }

    function<void(int)> dfs = [&](int block) {
        if (block_to_dfs[block] != 0) return;
        block_to_dfs[block]     = ++dfs_count;
        dfs_to_block[dfs_count] = block;
        // 初始半支配为自身的 dfn
        semi_dom[block] = block_to_dfs[block];
        for (int next : working_graph[block])
        {
            if (block_to_dfs[next] == 0)
            {
                parent[next] = block;
                dfs(next);
            }
        }
    };
    dfs(virtual_source);

    // Tarjan Eval（路径压缩 + 最小祖先维护）
    auto dsu_find = [&](int u, const auto& self) -> int {
        int p = dsu_parent[u];
        if (p == u) return u;
        int r = self(p, self);
        if (semi_dom[min_ancestor[p]] < semi_dom[min_ancestor[u]]) min_ancestor[u] = min_ancestor[p];
        dsu_parent[u] = r;
        return r;
    };
    auto dsu_query = [&](int u) -> int {
        dsu_find(u, dsu_find);
        return min_ancestor[u];
    };

    // 逆 DFS 序回溯半支配与初始 idom 计算（跳过根：dfs_id = 2..dfs_count）
    for (int dfs_id = dfs_count; dfs_id >= 2; --dfs_id)
    {
        int v = dfs_to_block[dfs_id];
        if (v < 0) continue;

        // 合并两类候选：
        // a) pred 的 dfn（仅当 dfn(pred) < dfn(v)）
        // b) sdom(eval(pred))（当 dfn(pred) > dfn(v)）
        int best = semi_dom[v];
        for (int p : backward_edges[v])
        {
            if (block_to_dfs[p] == 0) continue;  // 不可达前驱忽略
            if (block_to_dfs[p] < block_to_dfs[v])
            {
                best = min(best, block_to_dfs[p]);  // 注意：这里应取 dfn(pred)，不是 semi_dom[p]
            }
            else
            {
                int u = dsu_query(p);
                best  = min(best, semi_dom[u]);
            }
        }
        semi_dom[v] = best;

        // 将 v 放入 sdom(v) 的桶
        int sdom_vertex = dfs_to_block[semi_dom[v]];
        if (sdom_vertex >= 0 && sdom_vertex < node_count) semi_children[sdom_vertex].push_back(v);

        // Link(v, parent[v])
        int par = parent[v];
        if (par >= 0) dsu_parent[v] = par;

        // 处理 parent[v] 的桶：给出初始 idom
        if (par >= 0 && par < node_count)
        {
            for (int w : semi_children[par])
            {
                int u = dsu_query(w);
                if (semi_dom[u] == semi_dom[w])
                    imm_dom[w] = par;
                else
                    imm_dom[w] = u;
            }
            semi_children[par].clear();
        }
    }

    // 直接支配者 idom 链压缩（dfs_id = 2..dfs_count）
    for (int dfs_id = 2; dfs_id <= dfs_count; ++dfs_id)
    {
        int curr = dfs_to_block[dfs_id];
        if (curr < 0) continue;
        int sdom_vertex = dfs_to_block[semi_dom[curr]];
        if (imm_dom[curr] != sdom_vertex) imm_dom[curr] = imm_dom[imm_dom[curr]];
    }

    // 构建支配树（以 idom 为树边）
    for (int i = 0; i < node_count; ++i)
    {
        if (block_to_dfs[i] == 0) continue;
        int idom = imm_dom[i];
        if (idom >= 0 && idom < node_count) dom_tree[idom].push_back(i);
    }

    // 移除虚拟源，调整 imm_dom 与 dom_tree
    dom_tree.resize(virtual_source);
    dom_frontier.resize(virtual_source);
    imm_dom.resize(virtual_source);

    {
        // 以虚拟源为父的节点设为根（自支配）
        for (int i = 0; i < virtual_source; ++i)
        {
            if (block_to_dfs[i] == 0) continue;  // 不可达
            if (parent[i] == virtual_source || imm_dom[i] >= virtual_source) imm_dom[i] = i;
        }

        // 重建 dom_tree：跳过自环边
        vector<vector<int>> new_dom_tree(virtual_source);
        for (int i = 0; i < virtual_source; ++i)
        {
            if (block_to_dfs[i] == 0) continue;
            int idom = imm_dom[i];
            if (idom >= 0 && idom < virtual_source && idom != i) new_dom_tree[idom].push_back(i);
        }
        dom_tree.swap(new_dom_tree);
    }

    // 构建支配边界（修正自环处理；根也参与 DF）
    for (int block = 0; block < virtual_source; ++block)
    {
        if (block_to_dfs[block] == 0) continue;
        for (int succ : working_graph[block])
        {
            if (succ < 0 || succ >= virtual_source) continue;
            if (block_to_dfs[succ] == 0) continue;

            int runner = block;
            int stop   = imm_dom[succ];

            // 沿 idom 链向上，将 succ 放入 runner 的支配边界集合
            while (runner != stop)
            {
                dom_frontier[runner].insert(succ);
                int next = imm_dom[runner];
                if (next == runner || next < 0 || next >= virtual_source) break;  // 防止异常自环
                runner = next;
            }
        }
    }
}

void DomAnalyzer::clear()
{
    dom_tree.clear();
    dom_frontier.clear();
    imm_dom.clear();
}
#ifndef ROADRUNNER_H
#define ROADRUNNER_H
/* SPDX-License-Identifier: Apache-2.0 */
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define RR_VERSION "0.1.0"
#define RR_NO_NODE UINT32_MAX
#define RR_INFINITY UINT64_MAX

typedef enum { RR_OK=0, RR_INVALID, RR_OVERFLOW, RR_NO_MEMORY,
               RR_NO_PATH, RR_BUFFER_TOO_SMALL, RR_UNSUPPORTED } rr_status;
typedef enum { RR_AUTO=0, RR_BFS, RR_DIJKSTRA, RR_ASTAR } rr_algorithm;
typedef struct { uint32_t from, to, cost; } rr_edge;
typedef void *(*rr_alloc_fn)(void *context, size_t bytes);
typedef void (*rr_free_fn)(void *context, void *pointer);
typedef struct { rr_alloc_fn allocate; rr_free_fn release; void *context; } rr_allocator;
typedef struct rr_graph rr_graph;
typedef struct rr_workspace rr_workspace;
typedef struct rr_heuristic_plan rr_heuristic_plan;
typedef struct {
    uint64_t cost, expanded, relaxed, pushes;
    size_t path_length, workspace_bytes;
    rr_algorithm algorithm;
    int heuristic_used;
    uint64_t query_allocations;
} rr_result;
/* Constructors clear *out on failure. n must be positive and < UINT32_MAX.
   Directed edges; duplicates, self-loops, zero costs accepted. Graph copies input.
   Optional allocator must have both callbacks, malloc-equivalent alignment. */
rr_status rr_graph_create(uint32_t n, size_t m, const rr_edge *edges,
                          const rr_allocator *allocator, rr_graph **out);
void rr_graph_destroy(rr_graph *graph);
uint32_t rr_graph_nodes(const rr_graph *graph);
size_t rr_graph_edges(const rr_graph *graph);
size_t rr_graph_bytes(const rr_graph *graph);
/* Workspace is one reusable allocation, bounded by n. No per-query allocation. */
rr_status rr_workspace_create(uint32_t n, const rr_allocator *allocator,
                              rr_workspace **out);
void rr_workspace_destroy(rr_workspace *workspace);
/* h is NULL (zero fallback), or n values. A* validates h[target]==0 and
   consistency on every directed edge; invalid h safely falls back to zero.
   Input graph, h and output path must not overlap workspace or each other.
   Path includes both endpoints. On BUFFER_TOO_SMALL, path_length is required
   capacity, cost is available, and no path is written. Other unsuccessful
   searches set cost=INFINITY.
   Workspaces are not concurrent; an immutable graph can be shared.
   Deterministic tie: (f, g, node id), sorted adjacency, first strict improvement. */
rr_status rr_search(const rr_graph *graph, rr_workspace *workspace,
                    rr_algorithm algorithm, uint32_t source, uint32_t target,
                    const uint64_t *h, uint32_t *path, size_t capacity,
                    rr_result *result);
/* Certified plan copies n heuristic values and validates the private copy once.
   It binds one immutable graph and target; invalid bounds return RR_INVALID
   (unlike rr_search's zero fallback). Constructor clears *out on failure.
   The graph is borrowed: it must outlive the plan and all its queries.
   Allocator callbacks/context must outlive the plan. Destroy only after queries
   finish, and before graph destruction. Plans can be shared with distinct
   workspaces; the input h may be changed or freed after successful creation.
   One checked allocation holds the plan and copy; bytes returns requested size
   (zero for NULL). No query allocation or public certificate bypass flag. */
rr_status rr_heuristic_plan_create(const rr_graph *graph, uint32_t target,
                                  const uint64_t *h, const rr_allocator *allocator,
                                  rr_heuristic_plan **out);
void rr_heuristic_plan_destroy(rr_heuristic_plan *plan);
size_t rr_heuristic_plan_bytes(const rr_heuristic_plan *plan);
/* Same path, result, error and workspace contracts as rr_search with RR_ASTAR.
   Skips only the certificate scan; preserves exact cost and deterministic ties. */
rr_status rr_search_plan(const rr_heuristic_plan *plan, rr_workspace *workspace,
                         uint32_t source, uint32_t *path, size_t capacity,
                         rr_result *result);
const char *rr_status_string(rr_status status);
#ifdef __cplusplus
}
#endif
#endif

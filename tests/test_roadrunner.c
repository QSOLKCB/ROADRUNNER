/* SPDX-License-Identifier: Apache-2.0 */
/* Independent, deterministic conformance tests. No runtime internals are used. */
#include "roadrunner.h"

#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MAX_N 32u
#define MAX_EDGES 512u
#define PATH_SENTINEL UINT32_C(0xdeadbeef)
#define GUARD_BYTES 32u

static uint64_t checks, queries;
static unsigned case_id;
static uint32_t case_source, case_target;
static rr_algorithm case_algorithm;

static void fail(const char *expression, const char *file, int line) {
    fprintf(stderr, "%s:%d: check failed: %s (case=%u, source=%" PRIu32
            ", target=%" PRIu32 ", algorithm=%d)\n", file, line, expression,
            case_id, case_source, case_target, (int)case_algorithm);
    exit(EXIT_FAILURE);
}
#define CHECK(expression) do { ++checks; if (!(expression)) \
    fail(#expression, __FILE__, __LINE__); } while (0)

static rr_status search(const rr_graph *graph, rr_workspace *workspace,
                        rr_algorithm algorithm, uint32_t source,
                        uint32_t target, const uint64_t *heuristic,
                        uint32_t *path, size_t capacity, rr_result *result) {
    ++queries;
    case_source=source;
    case_target=target;
    case_algorithm=algorithm;
    return rr_search(graph,workspace,algorithm,source,target,heuristic,
                     path,capacity,result);
}

static uint32_t random_state=UINT32_C(0x8d46af21);
static uint32_t random_u32(void) {
    /* Defined unsigned arithmetic, independent of libc's random generator. */
    random_state^=random_state<<13;
    random_state^=random_state>>17;
    random_state^=random_state<<5;
    return random_state;
}

/* Cubic all-pairs reference: separate representation and algorithm. */
static void floyd_warshall(uint32_t n, size_t m, const rr_edge *edges,
                           uint64_t distances[MAX_N][MAX_N]) {
    uint32_t i,j,k;
    size_t e;
    CHECK(n<=MAX_N);
    for (i=0;i<n;++i) {
        for (j=0;j<n;++j) distances[i][j]=i==j ? 0:RR_INFINITY;
    }
    for (e=0;e<m;++e) {
        uint64_t *d=&distances[edges[e].from][edges[e].to];
        if (edges[e].cost<*d) *d=edges[e].cost;
    }
    for (k=0;k<n;++k) {
        for (i=0;i<n;++i) {
            for (j=0;j<n;++j) {
                uint64_t candidate;
                if (distances[i][k]==RR_INFINITY ||
                    distances[k][j]==RR_INFINITY) continue;
                CHECK(distances[i][k]<=UINT64_MAX-distances[k][j]);
                candidate=distances[i][k]+distances[k][j];
                if (candidate<distances[i][j]) distances[i][j]=candidate;
            }
        }
    }
}

static void fill_path(uint32_t *path, size_t length) {
    size_t i;
    for (i=0;i<length;++i) path[i]=PATH_SENTINEL;
}

static void check_untouched(const uint32_t *path, size_t length) {
    size_t i;
    for (i=0;i<length;++i) CHECK(path[i]==PATH_SENTINEL);
}

static void check_path(uint32_t n, size_t m, const rr_edge *edges,
                       uint32_t source, uint32_t target, const uint32_t *path,
                       const rr_result *result, uint64_t expected_cost) {
    size_t i,e;
    uint64_t total=0;
    unsigned char seen[MAX_N]={0};
    CHECK(result->cost==expected_cost);
    CHECK(result->path_length>=1 && result->path_length<=n);
    CHECK(path[0]==source);
    CHECK(path[result->path_length-1]==target);
    for (i=0;i<result->path_length;++i) {
        uint64_t minimum=RR_INFINITY;
        CHECK(path[i]<n);
        CHECK(!seen[path[i]]);
        seen[path[i]]=1;
        if (i==0) continue;
        /* The public path contains nodes. Parallel arcs therefore contribute
           their minimum cost when independently validating its exact cost. */
        for (e=0;e<m;++e) {
            if (edges[e].from==path[i-1] && edges[e].to==path[i] &&
                edges[e].cost<minimum) minimum=edges[e].cost;
        }
        CHECK(minimum!=RR_INFINITY);
        CHECK(total<=UINT64_MAX-minimum);
        total+=minimum;
    }
    CHECK(total==expected_cost);
    check_untouched(path+result->path_length,MAX_N-result->path_length);
}

static void compare_results(const rr_result *a, const rr_result *b) {
    CHECK(a->cost==b->cost);
    CHECK(a->expanded==b->expanded);
    CHECK(a->relaxed==b->relaxed);
    CHECK(a->pushes==b->pushes);
    CHECK(a->path_length==b->path_length);
    CHECK(a->workspace_bytes==b->workspace_bytes);
    CHECK(a->algorithm==b->algorithm);
    CHECK(a->heuristic_used==b->heuristic_used);
    CHECK(a->query_allocations==b->query_allocations);
}

typedef struct {
    size_t calls, releases, live, fail_at, last_bytes;
} allocation_state;
typedef union {
    max_align_t alignment;
    struct { size_t bytes; allocation_state *owner; } record;
} allocation_header;

static void *tracked_allocate(void *context, size_t bytes) {
    allocation_state *state=context;
    allocation_header *header;
    ++state->calls;
    state->last_bytes=bytes;
    if (state->fail_at && state->calls>=state->fail_at) return NULL;
    CHECK(bytes<=SIZE_MAX-sizeof(*header)-GUARD_BYTES);
    header=malloc(sizeof(*header)+bytes+GUARD_BYTES);
    CHECK(header!=NULL);
    header->record.bytes=bytes;
    header->record.owner=state;
    memset(header+1,0xcd,bytes);
    memset((unsigned char *)(header+1)+bytes,0xa5,GUARD_BYTES);
    ++state->live;
    return header+1;
}

static void tracked_release(void *context, void *pointer) {
    allocation_state *state=context;
    allocation_header *header;
    unsigned char *guard;
    size_t i;
    CHECK(pointer!=NULL);
    header=(allocation_header *)pointer-1;
    CHECK(header->record.owner==state);
    guard=(unsigned char *)pointer+header->record.bytes;
    for (i=0;i<GUARD_BYTES;++i) CHECK(guard[i]==0xa5);
    CHECK(state->live>0);
    --state->live;
    ++state->releases;
    free(header);
}

static void test_invalid_inputs(void) {
    rr_graph *graph=NULL, *out;
    rr_workspace *workspace=NULL, *small=NULL, *wout;
    rr_edge edge={0,1,1}, bad_from={2,0,1}, bad_to={0,2,1};
    rr_result result;
    uint32_t path[MAX_N];
    allocation_state state={0};
    rr_allocator allocator={tracked_allocate,tracked_release,&state};
    rr_allocator missing_allocate={NULL,tracked_release,&state};
    rr_allocator missing_release={tracked_allocate,NULL,&state};
    case_id=1;
    CHECK(rr_graph_nodes(NULL)==0);
    CHECK(rr_graph_edges(NULL)==0);
    CHECK(rr_graph_bytes(NULL)==0);
    rr_graph_destroy(NULL);
    rr_workspace_destroy(NULL);
    CHECK(rr_graph_create(2,1,&edge,NULL,NULL)==RR_INVALID);
    CHECK(rr_workspace_create(2,NULL,NULL)==RR_INVALID);
    out=(rr_graph *)&edge;
    CHECK(rr_graph_create(0,0,NULL,NULL,&out)==RR_INVALID && out==NULL);
    out=(rr_graph *)&edge;
    CHECK(rr_graph_create(UINT32_MAX,0,NULL,NULL,&out)==RR_INVALID && out==NULL);
    out=(rr_graph *)&edge;
    CHECK(rr_graph_create(2,1,NULL,NULL,&out)==RR_INVALID && out==NULL);
    out=(rr_graph *)&edge;
    CHECK(rr_graph_create(2,1,&bad_from,NULL,&out)==RR_INVALID && out==NULL);
    out=(rr_graph *)&edge;
    CHECK(rr_graph_create(2,1,&bad_to,NULL,&out)==RR_INVALID && out==NULL);
    out=(rr_graph *)&edge;
    CHECK(rr_graph_create(2,1,&edge,&missing_allocate,&out)==RR_INVALID && out==NULL);
    out=(rr_graph *)&edge;
    CHECK(rr_graph_create(2,1,&edge,&missing_release,&out)==RR_INVALID && out==NULL);
    wout=(rr_workspace *)&edge;
    CHECK(rr_workspace_create(0,NULL,&wout)==RR_INVALID && wout==NULL);
    wout=(rr_workspace *)&edge;
    CHECK(rr_workspace_create(UINT32_MAX,NULL,&wout)==RR_INVALID && wout==NULL);
    wout=(rr_workspace *)&edge;
    CHECK(rr_workspace_create(2,&missing_allocate,&wout)==RR_INVALID && wout==NULL);
    wout=(rr_workspace *)&edge;
    CHECK(rr_workspace_create(2,&missing_release,&wout)==RR_INVALID && wout==NULL);
    CHECK(state.calls==0 && state.releases==0);
    /* Only one real edge exists. Size validation must precede edge traversal. */
    out=(rr_graph *)&edge;
    CHECK(rr_graph_create(2,SIZE_MAX,&edge,&allocator,&out)==RR_OVERFLOW);
    CHECK(out==NULL && state.calls==0);
    CHECK(rr_graph_create(2,1,&edge,NULL,&graph)==RR_OK);
    CHECK(rr_workspace_create(2,NULL,&workspace)==RR_OK);
    CHECK(rr_workspace_create(1,NULL,&small)==RR_OK);
    fill_path(path,MAX_N);
    memset(&result,0xa5,sizeof(result));
    CHECK(search(NULL,workspace,RR_DIJKSTRA,0,1,NULL,path,MAX_N,&result)==RR_INVALID);
    CHECK(result.cost==RR_INFINITY && result.path_length==0);
    CHECK(search(graph,NULL,RR_DIJKSTRA,0,1,NULL,path,MAX_N,&result)==RR_INVALID);
    CHECK(result.cost==RR_INFINITY);
    CHECK(search(graph,small,RR_DIJKSTRA,0,1,NULL,path,MAX_N,&result)==RR_INVALID);
    CHECK(result.cost==RR_INFINITY);
    CHECK(search(graph,workspace,RR_DIJKSTRA,2,1,NULL,path,MAX_N,&result)==RR_INVALID);
    CHECK(search(graph,workspace,RR_DIJKSTRA,0,2,NULL,path,MAX_N,&result)==RR_INVALID);
    CHECK(search(graph,workspace,(rr_algorithm)-1,0,1,NULL,path,MAX_N,&result)==RR_INVALID);
    CHECK(search(graph,workspace,(rr_algorithm)99,0,1,NULL,path,MAX_N,&result)==RR_INVALID);
    CHECK(search(graph,workspace,RR_DIJKSTRA,0,1,NULL,NULL,1,&result)==RR_INVALID);
    CHECK(search(graph,workspace,RR_DIJKSTRA,0,1,NULL,path,MAX_N,NULL)==RR_INVALID);
    check_untouched(path,MAX_N);
    CHECK(search(graph,workspace,RR_DIJKSTRA,0,1,NULL,path,MAX_N,&result)==RR_OK);
    CHECK(result.cost==1 && result.path_length==2);
    CHECK(strcmp(rr_status_string(RR_OK),"ok")==0);
    CHECK(strcmp(rr_status_string(RR_INVALID),"invalid")==0);
    CHECK(strcmp(rr_status_string(RR_OVERFLOW),"overflow")==0);
    CHECK(strcmp(rr_status_string(RR_NO_MEMORY),"no_memory")==0);
    CHECK(strcmp(rr_status_string(RR_NO_PATH),"no_path")==0);
    CHECK(strcmp(rr_status_string(RR_BUFFER_TOO_SMALL),"buffer_too_small")==0);
    CHECK(strcmp(rr_status_string(RR_UNSUPPORTED),"unsupported")==0);
    CHECK(strcmp(rr_status_string((rr_status)99),"unknown")==0);
    rr_workspace_destroy(small);
    rr_workspace_destroy(workspace);
    rr_graph_destroy(graph);
}

static void test_capacity_and_copy(void) {
    rr_edge edges[]={{0,1,UINT32_MAX},{1,2,UINT32_MAX},{2,3,UINT32_MAX}};
    rr_graph *graph=NULL;
    rr_workspace *workspace=NULL;
    uint32_t path[MAX_N];
    rr_result result;
    size_t capacity;
    case_id=2;
    CHECK(rr_graph_create(5,3,edges,NULL,&graph)==RR_OK);
    CHECK(rr_graph_nodes(graph)==5 && rr_graph_edges(graph)==3);
    CHECK(rr_graph_bytes(graph)>sizeof(edges));
    CHECK(rr_workspace_create(7,NULL,&workspace)==RR_OK);
    /* Mutation after construction must not change the copied CSR graph. */
    memset(edges,0,sizeof(edges));
    for (capacity=0;capacity<4;++capacity) {
        fill_path(path,MAX_N);
        CHECK(search(graph,workspace,RR_DIJKSTRA,0,3,NULL,path,capacity,&result)==RR_BUFFER_TOO_SMALL);
        CHECK(result.path_length==4);
        CHECK(result.cost==UINT64_C(3)*UINT32_MAX);
        CHECK(result.query_allocations==0);
        check_untouched(path,MAX_N);
    }
    CHECK(search(graph,workspace,RR_DIJKSTRA,0,3,NULL,NULL,0,&result)==RR_BUFFER_TOO_SMALL);
    CHECK(result.path_length==4 && result.cost==UINT64_C(3)*UINT32_MAX);
    fill_path(path,MAX_N);
    CHECK(search(graph,workspace,RR_DIJKSTRA,0,3,NULL,path,4,&result)==RR_OK);
    CHECK(result.cost==UINT64_C(3)*UINT32_MAX && result.path_length==4);
    CHECK(path[0]==0 && path[1]==1 && path[2]==2 && path[3]==3);
    check_untouched(path+4,MAX_N-4);
    fill_path(path,MAX_N);
    CHECK(search(graph,workspace,RR_DIJKSTRA,3,0,NULL,path,MAX_N,&result)==RR_NO_PATH);
    CHECK(result.cost==RR_INFINITY && result.path_length==0);
    check_untouched(path,MAX_N);
    CHECK(search(graph,workspace,RR_DIJKSTRA,4,4,NULL,NULL,0,&result)==RR_BUFFER_TOO_SMALL);
    CHECK(result.path_length==1 && result.cost==0);
    CHECK(search(graph,workspace,RR_DIJKSTRA,4,4,NULL,path,1,&result)==RR_OK);
    CHECK(result.cost==0 && result.path_length==1 && path[0]==4);
    CHECK(result.expanded==1 && result.relaxed==0 && result.pushes==1);
    rr_workspace_destroy(workspace);
    rr_graph_destroy(graph);
}

static void test_allocators_and_bounds(void) {
    rr_edge edges[]={{0,1,1},{0,2,9},{1,2,1},{2,3,1},{3,0,1}};
    allocation_state state={0};
    rr_allocator allocator={tracked_allocate,tracked_release,&state};
    rr_graph *graph=NULL;
    rr_workspace *workspace=NULL;
    rr_result result;
    uint32_t path[MAX_N];
    size_t graph_bytes, small_bytes, larger_bytes, i;
    case_id=3;
    state.fail_at=1;
    graph=(rr_graph *)&state;
    CHECK(rr_graph_create(4,5,edges,&allocator,&graph)==RR_NO_MEMORY);
    CHECK(graph==NULL && state.calls==1 && state.live==0 && state.releases==0);
    state.calls=0;
    workspace=(rr_workspace *)&state;
    CHECK(rr_workspace_create(4,&allocator,&workspace)==RR_NO_MEMORY);
    CHECK(workspace==NULL && state.calls==1 && state.live==0 && state.releases==0);
    state.fail_at=0;
    state.calls=0;
    CHECK(rr_graph_create(4,5,edges,&allocator,&graph)==RR_OK);
    CHECK(state.calls==1 && state.live==1);
    graph_bytes=state.last_bytes;
    CHECK(rr_graph_bytes(graph)==graph_bytes);
    state.fail_at=2;
    CHECK(rr_workspace_create(4,&allocator,&workspace)==RR_NO_MEMORY);
    CHECK(workspace==NULL && state.calls==2 && state.live==1);
    state.fail_at=0;
    CHECK(rr_workspace_create(4,&allocator,&workspace)==RR_OK);
    CHECK(state.calls==3 && state.live==2);
    small_bytes=state.last_bytes;
    /* Deny every future allocation: repeated searches must still succeed. */
    state.fail_at=state.calls+1;
    for (i=0;i<1000;++i) {
        rr_algorithm algorithm=i%2 ? RR_DIJKSTRA:RR_ASTAR;
        CHECK(search(graph,workspace,algorithm,0,3,NULL,path,MAX_N,&result)==RR_OK);
        CHECK(result.cost==3 && result.query_allocations==0);
        CHECK(result.workspace_bytes==small_bytes);
        CHECK(result.expanded<=4 && result.pushes<=4);
    }
    CHECK(state.calls==3 && state.live==2 && state.releases==0);
    rr_workspace_destroy(workspace);
    rr_graph_destroy(graph);
    CHECK(state.live==0 && state.releases==2);
    state.fail_at=0;
    CHECK(rr_workspace_create(8,&allocator,&workspace)==RR_OK);
    larger_bytes=state.last_bytes;
    CHECK(larger_bytes>small_bytes && larger_bytes<=small_bytes*2);
    rr_workspace_destroy(workspace);
    CHECK(rr_workspace_create(16,&allocator,&workspace)==RR_OK);
    CHECK(state.last_bytes-larger_bytes==2*(larger_bytes-small_bytes));
    rr_workspace_destroy(workspace);
    CHECK(state.live==0 && state.releases==4);
    /* The legal maximum node count should reach the allocator on a 64-bit
       size_t, or fail its size check on a narrow size_t, without huge malloc. */
    state.fail_at=state.calls+1;
    {
        rr_status status=rr_workspace_create(UINT32_MAX-1,&allocator,&workspace);
        CHECK(status==RR_NO_MEMORY || status==RR_OVERFLOW);
        CHECK(workspace==NULL && state.live==0);
        graph=(rr_graph *)&state;
        status=rr_graph_create(UINT32_MAX-1,0,NULL,&allocator,&graph);
        CHECK(status==RR_NO_MEMORY || status==RR_OVERFLOW);
        CHECK(graph==NULL && state.live==0);
    }
}

static void test_heuristics(void) {
    enum { WIDTH=6, HEIGHT=5, N=WIDTH*HEIGHT };
    rr_edge edges[MAX_EDGES];
    uint64_t heuristic[MAX_N], distance[MAX_N][MAX_N];
    uint32_t path[MAX_N], baseline_path[MAX_N], u, target=WIDTH-1;
    size_t m=0,i;
    rr_graph *graph=NULL;
    rr_workspace *workspace=NULL;
    rr_result good, baseline, fallback;
    case_id=4;
    for (u=0;u<N;++u) {
        unsigned x=u%WIDTH,y=u/WIDTH;
        if (x+1<WIDTH) edges[m++]=(rr_edge){u,u+1,1};
        if (x>0) edges[m++]=(rr_edge){u,u-1,1};
        if (y+1<HEIGHT) edges[m++]=(rr_edge){u,u+WIDTH,1};
        if (y>0) edges[m++]=(rr_edge){u,u-WIDTH,1};
        heuristic[u]=(WIDTH-1-x)+y;
    }
    floyd_warshall(N,m,edges,distance);
    CHECK(rr_graph_create(N,m,edges,NULL,&graph)==RR_OK);
    CHECK(rr_workspace_create(N,NULL,&workspace)==RR_OK);
    fill_path(path,MAX_N);
    CHECK(search(graph,workspace,RR_ASTAR,0,target,heuristic,path,MAX_N,&good)==RR_OK);
    CHECK(good.heuristic_used==1 && good.algorithm==RR_ASTAR);
    check_path(N,m,edges,0,target,path,&good,distance[0][target]);
    fill_path(baseline_path,MAX_N);
    CHECK(search(graph,workspace,RR_DIJKSTRA,0,target,NULL,baseline_path,MAX_N,&baseline)==RR_OK);
    CHECK(good.expanded<baseline.expanded);
    CHECK(baseline.heuristic_used==0);
    /* An inconsistent edge (even with an admissible heuristic), a nonzero
       target, and NULL all trigger the documented zero-h fallback. */
    for (i=0;i<4;++i) {
        const uint64_t *h=heuristic;
        for (u=0;u<N;++u) heuristic[u]=(WIDTH-1-u%WIDTH)+u/WIDTH;
        if (i==0) heuristic[0]=UINT64_MAX;
        if (i==1) heuristic[1]=0;
        if (i==2) heuristic[target]=1;
        if (i==3) h=NULL;
        fill_path(path,MAX_N);
        CHECK(search(graph,workspace,RR_ASTAR,0,target,h,path,MAX_N,&fallback)==RR_OK);
        CHECK(fallback.heuristic_used==0 && fallback.cost==baseline.cost);
        CHECK(fallback.expanded==baseline.expanded);
        CHECK(fallback.relaxed==baseline.relaxed && fallback.pushes==baseline.pushes);
        CHECK(fallback.path_length==baseline.path_length);
        CHECK(memcmp(path,baseline_path,sizeof(path))==0);
    }
    /* Non-A* algorithms must ignore heuristic values entirely. */
    heuristic[0]=UINT64_MAX;
    CHECK(search(graph,workspace,RR_DIJKSTRA,0,target,heuristic,path,MAX_N,&fallback)==RR_OK);
    compare_results(&baseline,&fallback);
    CHECK(search(graph,workspace,RR_AUTO,0,target,heuristic,path,MAX_N,&fallback)==RR_OK);
    CHECK(fallback.algorithm==RR_BFS && fallback.heuristic_used==0);
    rr_workspace_destroy(workspace);
    rr_graph_destroy(graph);
    {
        rr_edge disconnected[]={{0,1,1},{2,3,0},{3,2,0},{4,5,UINT32_MAX}};
        uint64_t h[]={1,0,UINT64_MAX,UINT64_MAX,UINT64_MAX,UINT64_MAX};
        CHECK(rr_graph_create(6,4,disconnected,NULL,&graph)==RR_OK);
        CHECK(rr_workspace_create(6,NULL,&workspace)==RR_OK);
        CHECK(search(graph,workspace,RR_ASTAR,0,1,h,path,MAX_N,&good)==RR_OK);
        CHECK(good.heuristic_used==1 && good.cost==1);
        CHECK(search(graph,workspace,RR_ASTAR,4,1,h,path,MAX_N,&good)==RR_NO_PATH);
        CHECK(good.heuristic_used==1 && good.cost==RR_INFINITY);
        /* Invalidity in an unreachable component is still checked. The
           subtraction form must handle UINT64_MAX without wraparound. */
        h[2]=1; h[3]=0;
        CHECK(search(graph,workspace,RR_ASTAR,0,1,h,path,MAX_N,&fallback)==RR_OK);
        CHECK(fallback.heuristic_used==0 && fallback.cost==1);
        rr_workspace_destroy(workspace);
        rr_graph_destroy(graph);
    }
}

static void exercise_graph(uint32_t n, size_t m, const rr_edge *edges) {
    uint64_t distances[MAX_N][MAX_N], heuristic[MAX_N];
    rr_edge reordered[MAX_EDGES];
    rr_graph *graph=NULL,*permutation=NULL;
    rr_workspace *workspace=NULL;
    allocation_state state={0};
    rr_allocator allocator={tracked_allocate,tracked_release,&state};
    uint32_t source,target,i;
    size_t e;
    int unit=1;
    CHECK(m<=MAX_EDGES);
    floyd_warshall(n,m,edges,distances);
    for (e=0;e<m;++e) {
        reordered[e]=edges[e];
        if (edges[e].cost!=1) unit=0;
    }
    for (e=m;e>1;--e) {
        size_t j=(size_t)random_u32()%e;
        rr_edge temporary=reordered[e-1];
        reordered[e-1]=reordered[j];
        reordered[j]=temporary;
    }
    CHECK(rr_graph_create(n,m,edges,&allocator,&graph)==RR_OK);
    CHECK(rr_graph_create(n,m,reordered,&allocator,&permutation)==RR_OK);
    CHECK(rr_workspace_create(n,&allocator,&workspace)==RR_OK);
    CHECK(state.calls==3 && state.live==3);
    CHECK(rr_graph_nodes(graph)==n && rr_graph_edges(graph)==m);
    CHECK(rr_graph_bytes(graph)==rr_graph_bytes(permutation));
    state.fail_at=4;
    for (target=0;target<n;++target) {
        for (i=0;i<n;++i) heuristic[i]=distances[i][target];
        for (source=0;source<n;++source) {
            unsigned mode;
            for (mode=0;mode<(unit ? 5u:4u);++mode) {
                rr_algorithm algorithm=mode==0 ? RR_DIJKSTRA:
                    mode==1 ? RR_AUTO:mode==4 ? RR_BFS:RR_ASTAR;
                const uint64_t *h=mode==3 ? heuristic:NULL;
                uint32_t path[MAX_N], replay[MAX_N], shuffled[MAX_N];
                rr_result result, repeat, reordered_result;
                rr_status status, replay_status, reordered_status;
                fill_path(path,MAX_N);
                fill_path(replay,MAX_N);
                fill_path(shuffled,MAX_N);
                status=search(graph,workspace,algorithm,source,target,h,path,MAX_N,&result);
                CHECK(result.algorithm==(algorithm==RR_AUTO ?
                      (unit ? RR_BFS:RR_DIJKSTRA):algorithm));
                CHECK(result.heuristic_used==(mode==3));
                CHECK(result.query_allocations==0);
                CHECK(result.expanded<=n && result.pushes<=n);
                CHECK(result.relaxed<=m);
                if (distances[source][target]==RR_INFINITY) {
                    CHECK(status==RR_NO_PATH);
                    CHECK(result.cost==RR_INFINITY && result.path_length==0);
                    check_untouched(path,MAX_N);
                } else {
                    CHECK(status==RR_OK);
                    check_path(n,m,edges,source,target,path,&result,distances[source][target]);
                }
                replay_status=search(graph,workspace,algorithm,source,target,h,replay,MAX_N,&repeat);
                reordered_status=search(permutation,workspace,algorithm,source,target,h,shuffled,MAX_N,&reordered_result);
                CHECK(replay_status==status && reordered_status==status);
                compare_results(&result,&repeat);
                compare_results(&result,&reordered_result);
                CHECK(memcmp(path,replay,sizeof(path))==0);
                CHECK(memcmp(path,shuffled,sizeof(path))==0);
                CHECK(state.calls==3);
            }
        }
    }
    if (!unit) {
        uint32_t path[MAX_N];
        rr_result result;
        fill_path(path,MAX_N);
        CHECK(search(graph,workspace,RR_BFS,0,0,NULL,path,MAX_N,&result)==RR_UNSUPPORTED);
        CHECK(result.cost==RR_INFINITY && result.path_length==0);
        check_untouched(path,MAX_N);
    }
    rr_workspace_destroy(workspace);
    rr_graph_destroy(permutation);
    rr_graph_destroy(graph);
    CHECK(state.live==0 && state.calls==3 && state.releases==3);
}

static void test_fixed_graphs(void) {
    const rr_edge weighted[]={
        {0,0,0},{0,1,UINT32_MAX},{0,1,8},{0,1,8},{0,2,1},
        {2,1,0},{1,2,0},{1,3,UINT32_MAX},{2,3,UINT32_MAX},
        {3,4,UINT32_MAX},{4,4,0},{5,6,1},{6,5,1}
    };
    const rr_edge tied[]={
        {0,2,1},{0,1,1},{1,3,1},{2,3,1},{1,4,1},{2,4,1},
        {3,5,1},{4,5,1},{5,5,1},{0,1,1}
    };
    const rr_edge decrease[]={
        {0,1,50},{0,2,40},{0,3,30},{0,4,20},{0,5,10},
        {5,4,1},{4,3,1},{3,2,1},{2,1,1},{1,6,1},
        {5,6,100},{4,6,90},{3,6,80},{2,6,70}
    };
    rr_graph *graph=NULL;
    rr_workspace *workspace=NULL;
    rr_result result;
    uint32_t path[MAX_N];
    case_id=5;
    exercise_graph(8,sizeof(weighted)/sizeof(weighted[0]),weighted);
    case_id=6;
    exercise_graph(6,sizeof(tied)/sizeof(tied[0]),tied);
    case_id=7;
    exercise_graph(7,sizeof(decrease)/sizeof(decrease[0]),decrease);
    case_id=8;
    exercise_graph(1,0,NULL);
    case_id=9;
    exercise_graph(12,0,NULL);
    CHECK(rr_graph_create(6,sizeof(tied)/sizeof(tied[0]),tied,NULL,&graph)==RR_OK);
    CHECK(rr_workspace_create(6,NULL,&workspace)==RR_OK);
    CHECK(search(graph,workspace,RR_DIJKSTRA,0,5,NULL,path,MAX_N,&result)==RR_OK);
    CHECK(result.path_length==4);
    CHECK(path[0]==0 && path[1]==1 && path[2]==3 && path[3]==5);
    rr_workspace_destroy(workspace);
    rr_graph_destroy(graph);
}

static void test_random_graphs(void) {
    unsigned trial;
    for (trial=0;trial<400;++trial) {
        rr_edge edges[MAX_EDGES];
        uint32_t n=1+random_u32()%12;
        size_t m=(size_t)random_u32()%(2*(size_t)n*n+1),e;
        int unit=trial%5==0;
        case_id=100+trial;
        if (trial%11==0) m=0;
        for (e=0;e<m;++e) {
            uint32_t selector=random_u32()%8;
            edges[e].from=random_u32()%n;
            edges[e].to=random_u32()%n;
            edges[e].cost=unit ? 1:selector<2 ? 0:
                selector==2 ? UINT32_MAX:selector==3 ? UINT32_MAX-1:
                selector==4 ? 1:random_u32()%1000;
            if (e>0 && e%7==0) edges[e]=edges[e-1];
            if (e%13==0) edges[e].to=edges[e].from;
        }
        exercise_graph(n,m,edges);
    }
}

int main(void) {
    test_invalid_inputs();
    test_capacity_and_copy();
    test_allocators_and_bounds();
    test_heuristics();
    test_fixed_graphs();
    test_random_graphs();
    printf("ROADRUNNER conformance: 400 fixed-seed random graphs, "
           "targeted cases, %" PRIu64 " queries, %" PRIu64 " checks passed\n",
           queries,checks);
    return EXIT_SUCCESS;
}

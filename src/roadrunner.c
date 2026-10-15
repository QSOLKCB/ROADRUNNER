/* SPDX-License-Identifier: Apache-2.0 */
#include "roadrunner.h"
#include <stdlib.h>
#include <string.h>

#ifndef RR_EPOCH_LIMIT
#define RR_EPOCH_LIMIT UINT32_MAX
#endif
_Static_assert(RR_EPOCH_LIMIT > 0 && RR_EPOCH_LIMIT <= UINT32_MAX,
               "epoch limit must fit a nonzero uint32_t");
typedef struct { uint32_t to, cost; } arc;
struct rr_graph {
    uint32_t n; size_t m, bytes; int unit;
    rr_allocator allocator; size_t *offset; arc *arcs;
};
struct rr_workspace {
    uint32_t n, epoch; size_t bytes, count;
    rr_allocator allocator;
    uint64_t *distance, *key;
    uint32_t *parent, *heap, *position, *mark;
};
struct rr_heuristic_plan {
    const rr_graph *graph;
    uint32_t target;
    size_t bytes;
    rr_allocator allocator;
    uint64_t h[];
};
static void *default_allocate(void *c, size_t n) { (void)c; return malloc(n); }
static void default_release(void *c, void *p) { (void)c; free(p); }
static int allocator_get(const rr_allocator *in, rr_allocator *out) {
    if (in) { if (!in->allocate || !in->release) return 0; *out=*in; }
    else { out->allocate=default_allocate; out->release=default_release; out->context=NULL; }
    return 1;
}
static int add_bytes(size_t *total, size_t count, size_t size) {
    if (count > (SIZE_MAX-*total)/size) return 0;
    *total += count*size; return 1;
}
static int compare_arc(const void *a, const void *b) {
    const arc *x=a, *y=b;
    if (x->to!=y->to) return x->to<y->to ? -1 : 1;
    return x->cost<y->cost ? -1 : x->cost>y->cost;
}
rr_status rr_graph_create(uint32_t n, size_t m, const rr_edge *edges,
                          const rr_allocator *allocator, rr_graph **out) {
    rr_allocator a; rr_graph *g; size_t bytes=sizeof(rr_graph), i;
    if (!out) return RR_INVALID;
    *out=NULL;
    if (!n || n==UINT32_MAX || (m && !edges) || !allocator_get(allocator,&a)) return RR_INVALID;
    if (!add_bytes(&bytes,(size_t)n+1,sizeof(size_t)) || !add_bytes(&bytes,m,sizeof(arc))) return RR_OVERFLOW;
    for (i=0;i<m;i++) if (edges[i].from>=n || edges[i].to>=n) return RR_INVALID;
    g=a.allocate(a.context,bytes); if (!g) return RR_NO_MEMORY;
    memset(g,0,sizeof(*g)); g->n=n; g->m=m; g->bytes=bytes; g->allocator=a; g->unit=1;
    g->offset=(size_t *)(g+1); g->arcs=(arc *)(g->offset+(size_t)n+1);
    memset(g->offset,0,((size_t)n+1)*sizeof(size_t));
    for(i=0;i<m;i++) { g->offset[edges[i].from+1]++; if(edges[i].cost!=1) g->unit=0; }
    for(i=1;i<=(size_t)n;i++) g->offset[i]+=g->offset[i-1];
    /* Use starts as insertion cursors, then restore CSR offsets. */
    for(i=0;i<m;i++) { size_t p=g->offset[edges[i].from]++; g->arcs[p].to=edges[i].to; g->arcs[p].cost=edges[i].cost; }
    for(i=n;i>0;i--) g->offset[i]=g->offset[i-1];
    g->offset[0]=0;
    for(i=0;i<n;i++) qsort(g->arcs+g->offset[i],g->offset[i+1]-g->offset[i],sizeof(arc),compare_arc);
    *out=g; return RR_OK;
}
void rr_graph_destroy(rr_graph *g) { if(g) g->allocator.release(g->allocator.context,g); }
uint32_t rr_graph_nodes(const rr_graph *g) { return g ? g->n:0; }
size_t rr_graph_edges(const rr_graph *g) { return g ? g->m:0; }
size_t rr_graph_bytes(const rr_graph *g) { return g ? g->bytes:0; }
rr_status rr_workspace_create(uint32_t n,const rr_allocator *allocator,rr_workspace **out) {
    rr_allocator a; rr_workspace *w; size_t bytes=sizeof(rr_workspace);
    if(!out) return RR_INVALID;
    *out=NULL;
    if(!n || n==UINT32_MAX || !allocator_get(allocator,&a)) return RR_INVALID;
    if(!add_bytes(&bytes,n,2*sizeof(uint64_t)+4*sizeof(uint32_t))) return RR_OVERFLOW;
    w=a.allocate(a.context,bytes); if(!w) return RR_NO_MEMORY;
    memset(w,0,sizeof(*w)); w->n=n; w->bytes=bytes; w->allocator=a;
    w->distance=(uint64_t *)(w+1); w->key=w->distance+n;
    w->parent=(uint32_t *)(w->key+n); w->heap=w->parent+n; w->position=w->heap+n; w->mark=w->position+n;
    memset(w->mark,0,(size_t)n*sizeof(*w->mark));
    *out=w; return RR_OK;
}
void rr_workspace_destroy(rr_workspace *w) { if(w) w->allocator.release(w->allocator.context,w); }
static void touch(rr_workspace *w,uint32_t node) {
    if(w->mark[node]!=w->epoch) { w->mark[node]=w->epoch; w->distance[node]=RR_INFINITY;
        w->parent[node]=RR_NO_NODE; w->position[node]=RR_NO_NODE; }
}
static int less(const rr_workspace *w,uint32_t a,uint32_t b) {
    if(w->key[a]!=w->key[b]) return w->key[a]<w->key[b];
    if(w->distance[a]!=w->distance[b]) return w->distance[a]<w->distance[b];
    return a<b;
}
static void sift_up(rr_workspace *w,size_t i) {
    uint32_t node=w->heap[i];
    while(i) { size_t p=(i-1)/2; if(!less(w,node,w->heap[p])) break;
        w->heap[i]=w->heap[p]; w->position[w->heap[i]]=(uint32_t)i; i=p; }
    w->heap[i]=node; w->position[node]=(uint32_t)i;
}
static void push_or_decrease(rr_workspace *w,uint32_t node) {
    size_t i=w->position[node];
    if(i==RR_NO_NODE) { i=w->count++; w->heap[i]=node; }
    sift_up(w,i);
}
static uint32_t pop(rr_workspace *w) {
    uint32_t root=w->heap[0],node=w->heap[--w->count]; size_t i=0;
    w->position[root]=RR_NO_NODE;
    if(w->count) {
        while(i<w->count/2) { size_t c=i*2+1;
            if(c+1<w->count && less(w,w->heap[c+1],w->heap[c])) c++;
            if(!less(w,w->heap[c],node)) break;
            w->heap[i]=w->heap[c]; w->position[w->heap[i]]=(uint32_t)i; i=c; }
        w->heap[i]=node; w->position[node]=(uint32_t)i;
    }
    return root;
}
static int heuristic_valid(const rr_graph *g,uint32_t target,const uint64_t *h) {
    uint32_t u; if(!h || h[target]!=0) return 0;
    for(u=0;u<g->n;u++) { size_t e;
        for(e=g->offset[u];e<g->offset[u+1];e++) { arc a=g->arcs[e];
            if(h[u]>h[a.to] && h[u]-h[a.to]>a.cost) return 0; }
    }
    return 1;
}
rr_status rr_heuristic_plan_create(const rr_graph *g,uint32_t target,const uint64_t *h,
                                    const rr_allocator *allocator,rr_heuristic_plan **out) {
    rr_allocator a; rr_heuristic_plan *p; size_t bytes=sizeof(rr_heuristic_plan);
    if(!out) return RR_INVALID;
    *out=NULL;
    if(!g || target>=g->n || !h || !allocator_get(allocator,&a)) return RR_INVALID;
    if(!add_bytes(&bytes,g->n,sizeof(uint64_t))) return RR_OVERFLOW;
    p=a.allocate(a.context,bytes); if(!p) return RR_NO_MEMORY;
    p->graph=g; p->target=target; p->bytes=bytes; p->allocator=a;
    memcpy(p->h,h,(size_t)g->n*sizeof(uint64_t));
    if(!heuristic_valid(g,target,p->h)) { a.release(a.context,p); return RR_INVALID; }
    *out=p; return RR_OK;
}
void rr_heuristic_plan_destroy(rr_heuristic_plan *p) {
    if(p) p->allocator.release(p->allocator.context,p);
}
size_t rr_heuristic_plan_bytes(const rr_heuristic_plan *p) { return p ? p->bytes:0; }
static rr_status search(const rr_graph *g,rr_workspace *w,rr_algorithm algorithm,
                   uint32_t source,uint32_t target,const uint64_t *h,
                   uint32_t *path,size_t capacity,rr_result *r,int certified) {
    uint32_t u; size_t head=0; int bfs;
    if(!r) return RR_INVALID;
    memset(r,0,sizeof(*r)); r->cost=RR_INFINITY;
    if(!g || !w || w->n<g->n || source>=g->n || target>=g->n || (!path && capacity) ||
       algorithm<RR_AUTO || algorithm>RR_ASTAR) return RR_INVALID;
    if(algorithm==RR_AUTO) algorithm=g->unit ? RR_BFS:RR_DIJKSTRA;
    if(algorithm==RR_BFS && !g->unit) return RR_UNSUPPORTED;
    r->algorithm=algorithm; r->workspace_bytes=w->bytes; bfs=algorithm==RR_BFS;
    r->heuristic_used=algorithm==RR_ASTAR && (certified || heuristic_valid(g,target,h));
    if(!r->heuristic_used) h=NULL;
    if(w->epoch==RR_EPOCH_LIMIT) { memset(w->mark,0,(size_t)w->n*sizeof(*w->mark)); w->epoch=0; }
    w->epoch++; touch(w,source);
    w->count=0; w->distance[source]=0; w->key[source]=h ? h[source]:0;
    if(bfs) w->heap[w->count++]=source; else push_or_decrease(w,source);
    r->pushes=1;
    while(bfs ? head<w->count:w->count>0) {
        size_t e; u=bfs ? w->heap[head++]:pop(w); r->expanded++;
        if(u==target) break;
        for(e=g->offset[u];e<g->offset[u+1];e++) {
            arc a=g->arcs[e]; uint64_t distance;
            r->relaxed++;
            if(w->distance[u]>UINT64_MAX-a.cost) return RR_OVERFLOW;
            distance=w->distance[u]+a.cost; touch(w,a.to);
            if(distance>=w->distance[a.to]) continue;
            w->distance[a.to]=distance; w->parent[a.to]=u;
            w->key[a.to]=h && h[a.to]>UINT64_MAX-distance ? UINT64_MAX:distance+(h ? h[a.to]:0);
            if(bfs) { if(a.to==target) goto route_found;
                w->heap[w->count++]=a.to; r->pushes++; }
            else { if(w->position[a.to]==RR_NO_NODE) r->pushes++; push_or_decrease(w,a.to); }
        }
    }
 route_found:
    if(w->mark[target]!=w->epoch || w->distance[target]==RR_INFINITY) return RR_NO_PATH;
    r->cost=w->distance[target]; r->path_length=1;
    for(u=target;u!=source;u=w->parent[u]) {
        if(w->parent[u]==RR_NO_NODE || r->path_length>=g->n) return RR_INVALID;
        r->path_length++;
    }
    if(capacity<r->path_length) return RR_BUFFER_TOO_SMALL;
    u=target;
    for(size_t j=r->path_length;j>0;j--) { path[j-1]=u; u=w->parent[u]; }
    return RR_OK;
}
rr_status rr_search(const rr_graph *g,rr_workspace *w,rr_algorithm algorithm,
                    uint32_t source,uint32_t target,const uint64_t *h,
                    uint32_t *path,size_t capacity,rr_result *r) {
    return search(g,w,algorithm,source,target,h,path,capacity,r,0);
}
rr_status rr_search_plan(const rr_heuristic_plan *p,rr_workspace *w,uint32_t source,
                         uint32_t *path,size_t capacity,rr_result *r) {
    return search(p ? p->graph:NULL,w,RR_ASTAR,source,p ? p->target:0,
                  p ? p->h:NULL,path,capacity,r,1);
}
const char *rr_status_string(rr_status s) {
    switch(s) { case RR_OK:return "ok"; case RR_INVALID:return "invalid";
    case RR_OVERFLOW:return "overflow"; case RR_NO_MEMORY:return "no_memory";
    case RR_NO_PATH:return "no_path"; case RR_BUFFER_TOO_SMALL:return "buffer_too_small";
    case RR_UNSUPPORTED:return "unsupported"; default:return "unknown"; }
}

/* SPDX-License-Identifier: Apache-2.0 */
#define _POSIX_C_SOURCE 200809L
#include "roadrunner.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct { size_t live, peak, allocations, frees; } tracker;
typedef union { max_align_t alignment; size_t bytes; } header;
static void *allocate(void *ctx,size_t bytes) {
    tracker *t=ctx; header *p=malloc(sizeof(*p)+bytes); if(!p) abort();
    p->bytes=bytes; t->live+=bytes; t->allocations++;
    if(t->live>t->peak) t->peak=t->live;
    return p+1;
}
static void release(void *ctx,void *ptr) {
    tracker *t=ctx; if(ptr) { header *p=(header *)ptr-1; t->live-=p->bytes; t->frees++; free(p); }
}
static double now(void) {
#ifdef RR_PLAN_FROZEN_CLOCK
    return 0;
#else
    struct timespec t; if(clock_gettime(CLOCK_MONOTONIC,&t)) abort();
    return (double)t.tv_sec+(double)t.tv_nsec*1e-9;
#endif
}
static uint32_t random32(uint32_t *s) { uint32_t x=*s; x^=x<<13; x^=x>>17; x^=x<<5; return *s=x; }
static uint64_t hash(uint64_t h,uint64_t x) { return (h^x)*UINT64_C(1099511628211); }
static unsigned number(const char *s) {
    unsigned x=0; if(!*s) return 0;
    for(;*s;s++) { if(*s<'0'||*s>'9'||x>100000000u) return 0; x=x*10+(unsigned)(*s-'0'); }
    return x;
}
typedef struct { double us; uint32_t source; rr_status status; rr_result result; uint64_t path_hash; } sample;
int main(int argc,char **argv) {
    if(argc!=6) { fprintf(stderr,"usage: rr_plan_bench {legacy|plan|auto} {local|weighted|zero|unit} SIDE {1|10|100|1000} SEED\n"); return 2; }
    const char *mode=argv[1],*family=argv[2];
    int planned=!strcmp(mode,"plan"), reference=!strcmp(mode,"auto");
    int local=!strcmp(family,"local"), zero=!strcmp(family,"zero"), unit=!strcmp(family,"unit");
    unsigned side=number(argv[3]),queries=number(argv[4]),seed=number(argv[5]);
    if((!planned&&!reference&&strcmp(mode,"legacy"))||(!local&&!zero&&!unit&&strcmp(family,"weighted"))||
       side<4||side>64||!(queries==1||queries==10||queries==100||queries==1000)||!seed||(reference&&!unit)) return 2;
    uint32_t n=side*side,target=n-1,rng=seed;
    tracker mem={0}; rr_allocator a={allocate,release,&mem}; rr_graph *g=NULL; rr_workspace *w=NULL; rr_heuristic_plan *plan=NULL;
    size_t m=0; double harness_begin=now(),begin,t,generate_us,graph_us,workspace_us,heuristic_us,plan_us=0,verify_us=0,teardown_us;
    rr_edge *edges=allocate(&mem,(size_t)n*4*sizeof(*edges));
    size_t *offset=allocate(&mem,((size_t)n+1)*sizeof(*offset));
    uint32_t *paths=allocate(&mem,(size_t)n*queries*sizeof(*paths));
    uint64_t *h=allocate(&mem,(size_t)n*sizeof(*h)),*oracle=allocate(&mem,(size_t)n*sizeof(*oracle));
    unsigned char *seen=allocate(&mem,n);
    sample *samples=allocate(&mem,(size_t)queries*sizeof(*samples));
    double harness_allocate_us=(now()-harness_begin)*1e6; begin=now(); t=begin;
    for(uint32_t u=0;u<n;u++) {
        uint32_t v[4]; size_t k=0; offset[u]=m;
        if(u%side) v[k++]=u-1;
        if(u%side+1<side) v[k++]=u+1;
        if(u>=side) v[k++]=u-side;
        if(u+side<n) v[k++]=u+side;
        for(size_t j=0;j<k;j++) edges[m++]=(rr_edge){u,v[j],unit?1:1+random32(&rng)%9};
    }
    offset[n]=m; generate_us=(now()-t)*1e6; t=now();
    if(rr_graph_create(n,m,edges,&a,&g)!=RR_OK) return 3;
    graph_us=(now()-t)*1e6; t=now();
    if(rr_workspace_create(n,&a,&w)!=RR_OK) return 3;
    workspace_us=(now()-t)*1e6; t=now();
    for(uint32_t u=0;u<n;u++) h[u]=zero?0:(side-1-u%side)+(side-1-u/side);
    heuristic_us=(now()-t)*1e6;
    size_t graph_bytes=rr_graph_bytes(g),plan_bytes=0;
    if(planned) { t=now(); if(rr_heuristic_plan_create(g,target,h,&a,&plan)!=RR_OK) return 3;
        plan_bytes=rr_heuristic_plan_bytes(plan); release(&mem,h); h=NULL; plan_us=(now()-t)*1e6; }
    size_t constructor_peak=mem.peak,steady=mem.live;
    rng=seed^UINT32_C(0x9e3779b9); size_t before=mem.allocations; double sum_us=0;
    for(unsigned q=0;q<queries;q++) {
        sample *s=samples+q; uint32_t r=random32(&rng); uint32_t *path=paths+(size_t)q*n;
        s->source=local?(side-1-r%4)*side+(side-1-(r/4)%4):r%n;
        t=now();
        s->status=planned?rr_search_plan(plan,w,s->source,path,n,&s->result):
            rr_search(g,w,reference?RR_AUTO:RR_ASTAR,s->source,target,h,path,n,&s->result);
        s->us=(now()-t)*1e6; sum_us+=s->us;
    }
    size_t query_allocations=mem.allocations-before,peak=mem.peak,allocations=mem.allocations;
    t=now(); rr_heuristic_plan_destroy(plan); rr_workspace_destroy(w); rr_graph_destroy(g); release(&mem,h);
    teardown_us=(now()-t)*1e6;
    double lifecycle_us=(now()-begin)*1e6;
    /* Independent dense reverse Dijkstra over original edges, not the routing API. */
    t=now(); memset(seen,0,n); for(uint32_t u=0;u<n;u++) oracle[u]=RR_INFINITY; oracle[target]=0;
    for(uint32_t k=0;k<n;k++) {
        uint32_t v=RR_NO_NODE;
        for(uint32_t u=0;u<n;u++) if(!seen[u]&&(v==RR_NO_NODE||oracle[u]<oracle[v])) v=u;
        if(v==RR_NO_NODE||oracle[v]==RR_INFINITY) break;
        seen[v]=1;
        for(size_t e=0;e<m;e++) if(edges[e].to==v&&oracle[edges[e].from]>oracle[v]+edges[e].cost)
            oracle[edges[e].from]=oracle[v]+edges[e].cost;
    }
    verify_us+=(now()-t)*1e6;
    uint64_t graph_hash=UINT64_C(1469598103934665603),query_hash=graph_hash;
    for(size_t e=0;e<m;e++) graph_hash=hash(hash(hash(graph_hash,edges[e].from),edges[e].to),edges[e].cost);
    for(unsigned q=0;q<queries;q++) {
        sample *s=samples+q; uint32_t *path=paths+(size_t)q*n;
        query_hash=hash(query_hash,s->source);
        t=now(); rr_result *res=&s->result;
        if(s->status!=RR_OK||res->cost!=oracle[s->source]||!res->path_length||res->path_length>n||
           path[0]!=s->source||path[res->path_length-1]!=target||res->query_allocations) return 4;
        uint64_t cost=0; s->path_hash=UINT64_C(1469598103934665603);
        for(size_t j=0;j<res->path_length;j++) {
            if(path[j]>=n) return 4;
            s->path_hash=hash(s->path_hash,path[j]);
            if(j) { uint32_t best=UINT32_MAX;
                for(size_t e=offset[path[j-1]];e<offset[path[j-1]+1];e++)
                    if(edges[e].to==path[j]&&edges[e].cost<best) best=edges[e].cost;
                if(best==UINT32_MAX) return 4;
                cost+=best;
            }
        }
        if(cost!=res->cost) return 4;
        verify_us+=(now()-t)*1e6;
    }
    printf("{\"mode\":\"%s\",\"family\":\"%s\",\"side\":%u,\"queries\":%u,\"seed\":%u,\"target\":%u,",mode,family,side,queries,seed,target);
    printf("\"graph_hash\":\"%016" PRIx64 "\",\"query_hash\":\"%016" PRIx64 "\",",graph_hash,query_hash);
    printf("\"generation_us\":%.9f,\"graph_build_us\":%.9f,\"workspace_build_us\":%.9f,\"heuristic_generation_us\":%.9f,\"plan_setup_us\":%.9f,",generate_us,graph_us,workspace_us,heuristic_us,plan_us);
    printf("\"verification_us\":%.9f,\"query_total_us\":%.9f,\"first_query_us\":%.9f,",verify_us,sum_us,samples[0].us);
    printf("\"graph_bytes\":%zu,\"workspace_bytes\":%zu,\"plan_bytes\":%zu,\"steady_requested_bytes\":%zu,\"constructor_peak_requested_bytes\":%zu,\"peak_requested_bytes\":%zu,\"allocations\":%zu,\"query_allocations\":%zu,\"samples\":[",graph_bytes,samples[0].result.workspace_bytes,plan_bytes,steady,constructor_peak,peak,allocations,query_allocations);
    for(unsigned q=0;q<queries;q++) { sample *s=samples+q;
        printf("%s{\"us\":%.9f,\"source\":%u,\"status\":%d,\"cost\":%" PRIu64 ",\"path_length\":%zu,\"path_hash\":\"%016" PRIx64 "\",\"expanded\":%" PRIu64 ",\"relaxed\":%" PRIu64 ",\"pushes\":%" PRIu64 "}",q?",":"",s->us,s->source,(int)s->status,s->result.cost,s->result.path_length,s->path_hash,s->result.expanded,s->result.relaxed,s->result.pushes);
    }
    t=now(); release(&mem,samples); release(&mem,edges); release(&mem,offset); release(&mem,paths); release(&mem,oracle); release(&mem,seen);
    double harness_free_us=(now()-t)*1e6;
    printf("],\"teardown_us\":%.9f,\"lifecycle_us\":%.9f,\"harness_allocate_us\":%.9f,\"harness_free_us\":%.9f,\"live_bytes_after_teardown\":%zu,\"frees\":%zu,\"verified\":true}\n",teardown_us,lifecycle_us,harness_allocate_us,harness_free_us,mem.live,mem.frees);
    return query_allocations||mem.live||mem.allocations!=mem.frees?4:0;
}

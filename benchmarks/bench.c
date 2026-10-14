/* SPDX-License-Identifier: Apache-2.0 */
#define _POSIX_C_SOURCE 200809L
#include "roadrunner.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

typedef struct { size_t allocations, live, peak; } tracker;
typedef union { max_align_t alignment; size_t bytes; } allocation_header;
static void *tracked_alloc(void *context,size_t bytes) {
    tracker *t=context; allocation_header *p;
    if(bytes>SIZE_MAX-sizeof(*p)) return NULL;
    p=malloc(bytes+sizeof(*p)); if(!p) return NULL;
    p->bytes=bytes; t->allocations++; t->live+=bytes;
    if(t->live>t->peak) t->peak=t->live;
    return p+1;
}
static void tracked_free(void *context,void *memory) {
    tracker *t=context; allocation_header *p;
    if(!memory) return;
    p=(allocation_header *)memory-1; t->live-=p->bytes; free(p);
}
static double seconds(void) { struct timespec t; if(clock_gettime(CLOCK_MONOTONIC,&t)) abort(); return (double)t.tv_sec+1e-9*(double)t.tv_nsec; }
static uint32_t random32(uint32_t *s) { uint32_t x=*s; x^=x<<13; x^=x>>17; x^=x<<5; *s=x; return x; }
static uint32_t morton(uint32_t x,uint32_t y) { uint32_t r=0; for(unsigned b=0;b<16;b++) r|=((x>>b)&1u)<<(2*b)|((y>>b)&1u)<<(2*b+1); return r; }
static uint32_t id(uint32_t logical,uint32_t side,int z) { return z ? morton(logical%side,logical/side):logical; }
static uint32_t logical_id(uint32_t physical,uint32_t side,int z) {
    uint32_t x=0,y=0; if(!z) return physical;
    for(unsigned b=0;b<16;b++) { x|=((physical>>(2*b))&1u)<<b; y|=((physical>>(2*b+1))&1u)<<b; }
    return y*side+x;
}
static int double_compare(const void *a,const void *b) { double x=*(const double *)a,y=*(const double *)b; return x<y ? -1:x>y; }
static uint64_t hash(uint64_t x,uint64_t y) { return (x^y)*UINT64_C(1099511628211); }
static size_t percentile(size_t n,unsigned p) { size_t rank=((size_t)p*n+99)/100; return rank ? rank-1:0; }
static int parse(const char *text,uint32_t *v) {
    uint64_t x=0; if(!*text) return 0;
    for(;*text;text++) { unsigned d=(unsigned char)*text-'0'; if(d>9 || x>(UINT32_MAX-d)/10) return 0; x=x*10+d; }
    *v=(uint32_t)x; return 1;
}
int main(int argc,char **argv) {
    const char *family,*strategy,*layout; uint32_t side=64,queries=201,seed=123456789,n,rng;
    size_t m=0,max_edges; int z,weighted,local,disconnected,corridor,sparse; rr_algorithm algorithm;
    rr_edge *edges; size_t *offset; uint32_t *path; uint64_t *h; double *samples,*sorted;
    rr_graph *g=NULL; rr_workspace *w=NULL; rr_result result; rr_status status;
    tracker memory={0}; rr_allocator allocator={tracked_alloc,tracked_free,&memory};
    double begin,generate_done,build_done,first_seconds=0,setup_seconds,sum=0;
    uint64_t graph_hash=UINT64_C(1469598103934665603),query_hash=graph_hash,checksum=0,expanded=0,relaxed=0,pushes=0;
    uint64_t first_expanded=0; size_t before_queries; unsigned found=0; int heuristic_used=0;
    if(argc!=7 || !parse(argv[4],&side) || !parse(argv[5],&queries) || !parse(argv[6],&seed) ||
       side<4 || side>512 || (side&(side-1)) || queries<3 || queries>10001 || !seed) {
        fprintf(stderr,"usage: rr_bench {grid|weighted|local|sparse|disconnected|corridor} {auto|dijkstra|astar} {row|morton} SIDE QUERIES SEED\n"); return 2;
    }
    family=argv[1]; strategy=argv[2]; layout=argv[3];
    z=!strcmp(layout,"morton"); if(!z && strcmp(layout,"row")) return 2;
    weighted=!strcmp(family,"weighted"); local=!strcmp(family,"local"); disconnected=!strcmp(family,"disconnected");
    corridor=!strcmp(family,"corridor"); sparse=!strcmp(family,"sparse");
    if(!weighted && !local && !disconnected && !corridor && !sparse && strcmp(family,"grid")) return 2;
    if(!strcmp(strategy,"auto")) algorithm=RR_AUTO;
    else if(!strcmp(strategy,"dijkstra")) algorithm=RR_DIJKSTRA;
    else if(!strcmp(strategy,"astar")) algorithm=RR_ASTAR;
    else return 2;
    n=side*side; max_edges=(size_t)n*5; rng=seed; begin=seconds();
    offset=tracked_alloc(&memory,((size_t)n+1)*sizeof(*offset));
    edges=tracked_alloc(&memory,max_edges*sizeof(*edges)); path=tracked_alloc(&memory,(size_t)n*sizeof(*path));
    h=tracked_alloc(&memory,(size_t)n*sizeof(*h)); samples=tracked_alloc(&memory,(size_t)queries*sizeof(*samples));
    sorted=tracked_alloc(&memory,(size_t)queries*sizeof(*sorted));
    if(!offset || !edges || !path || !h || !samples || !sorted) return 3;
    for(uint32_t u=0;u<n;u++) {
        uint32_t neighbors[5]; size_t count=0; offset[u]=m;
        if(corridor) { if(u) neighbors[count++]=u-1; if(u+1<n) neighbors[count++]=u+1; }
        else if(sparse) { if(u+1<n) neighbors[count++]=u+1; for(int j=0;j<3;j++) neighbors[count++]=random32(&rng)%n; }
        else { uint32_t x=u%side,y=u/side;
            if(x) neighbors[count++]=u-1;
            if(x+1<side) neighbors[count++]=u+1;
            if(y) neighbors[count++]=u-side;
            if(y+1<side) neighbors[count++]=u+side;
        }
        for(size_t j=0;j<count;j++) {
            uint32_t v=neighbors[j],cost=(weighted || sparse) ? 1+random32(&rng)%31:1;
            if(disconnected && ((u%side<side/2)!=(v%side<side/2))) continue;
            graph_hash=hash(hash(hash(graph_hash,u),v),cost);
            edges[m++]=(rr_edge){id(u,side,z),id(v,side,z),cost};
        }
    }
    offset[n]=m;
    generate_done=seconds(); status=rr_graph_create(n,m,edges,&allocator,&g); if(status!=RR_OK) return 4;
    status=rr_workspace_create(n,&allocator,&w); if(status!=RR_OK) return 4;
    build_done=seconds(); setup_seconds=build_done-begin;
    before_queries=memory.allocations; rng=seed^UINT32_C(0x9e3779b9);
    for(uint32_t q=0;q<queries+5;q++) {
        uint32_t source=random32(&rng)%n,target=random32(&rng)%n;
        const uint64_t *heuristic=NULL; double start,end; uint64_t expected;
        if(local) { uint32_t x=source%side,y=source/side; target=y*side+(x+1+q%4)%side; }
        if(disconnected) { source=(source/side)*side+source%(side/2); target=(target/side)*side+side/2+target%(side/2); }
        start=seconds();
        if(algorithm==RR_ASTAR && !sparse && !corridor) {
            for(uint32_t u=0;u<n;u++) {
                uint32_t ux=u%side,uy=u/side,tx=target%side,ty=target/side;
                h[id(u,side,z)]=(ux>tx ? ux-tx:tx-ux)+(uy>ty ? uy-ty:ty-uy);
            }
            heuristic=h;
        }
        status=rr_search(g,w,algorithm,id(source,side,z),id(target,side,z),heuristic,path,n,&result);
        end=seconds();
        if(status!=(disconnected ? RR_NO_PATH:RR_OK)) return 5;
        if(!weighted && !sparse && !disconnected) {
            uint32_t sx=source%side,sy=source/side,tx=target%side,ty=target/side;
            expected=corridor ? (source>target ? source-target:target-source):
                (uint64_t)(sx>tx ? sx-tx:tx-sx)+(sy>ty ? sy-ty:ty-sy);
            if(result.cost!=expected) return 6;
        }
        if(status==RR_OK) {
            uint64_t sum_cost=0;
            if(!result.path_length || path[0]!=id(source,side,z) || path[result.path_length-1]!=id(target,side,z)) return 8;
            for(size_t k=1;k<result.path_length;k++) {
                uint32_t u=logical_id(path[k-1],side,z); uint64_t best=RR_INFINITY;
                for(size_t e=offset[u];e<offset[u+1];e++)
                    if(edges[e].to==path[k] && edges[e].cost<best) best=edges[e].cost;
                if(best==RR_INFINITY || sum_cost>UINT64_MAX-best) return 8;
                sum_cost+=best;
            }
            if(sum_cost!=result.cost) return 8;
        }
        if(q==0) { first_seconds=end-start; first_expanded=result.expanded; }
        if(q<5) continue;
        samples[q-5]=(end-start)*1e6; sum+=end-start;
        query_hash=hash(hash(query_hash,source),target); checksum=hash(checksum,result.cost);
        expanded+=result.expanded; relaxed+=result.relaxed; pushes+=result.pushes;
        if(status==RR_OK) found++;
        heuristic_used=result.heuristic_used;
    }
    memcpy(sorted,samples,(size_t)queries*sizeof(*samples)); qsort(sorted,queries,sizeof(*sorted),double_compare);
    printf("{\"schema\":1,\"family\":\"%s\",\"strategy\":\"%s\",\"layout\":\"%s\",\"side\":%u,\"nodes\":%u,\"edges\":%zu,\"queries\":%u,\"seed\":%u,",family,strategy,layout,side,n,m,queries,seed);
    printf("\"graph_hash\":\"%016" PRIx64 "\",\"query_hash\":\"%016" PRIx64 "\",\"cost_checksum\":\"%016" PRIx64 "\",",graph_hash,query_hash,checksum);
    printf("\"median_us\":%.6f,\"p95_us\":%.6f,\"p99_us\":%.6f,\"first_query_us\":%.6f,\"first_result_end_to_end_us\":%.6f,",sorted[queries/2],sorted[percentile(queries,95)],sorted[percentile(queries,99)],first_seconds*1e6,(setup_seconds+first_seconds)*1e6);
    /* A monotonic clock can return equal ticks for every tiny query. Do not
       invent elapsed time or emit non-JSON inf/nan for an unmeasurable rate. */
    if(sum>0) printf("\"routes_per_second\":%.3f,",found/sum);
    else printf("\"routes_per_second\":null,");
    printf("\"generate_us\":%.6f,\"index_workspace_us\":%.6f,\"setup_us\":%.6f,\"expanded\":%" PRIu64 ",\"relaxed\":%" PRIu64 ",\"pushes\":%" PRIu64 ",\"first_expanded\":%" PRIu64 ",",(generate_done-begin)*1e6,(build_done-generate_done)*1e6,setup_seconds*1e6,expanded,relaxed,pushes,first_expanded);
    printf("\"successful_routes\":%u,\"heuristic_used\":%d,\"setup_allocations\":%zu,\"query_allocations\":%zu,\"peak_requested_bytes\":%zu,\"steady_requested_bytes\":%zu,\"graph_bytes\":%zu,\"workspace_bytes\":%zu,\"samples_us\":[",found,heuristic_used,before_queries,memory.allocations-before_queries,memory.peak,memory.live,rr_graph_bytes(g),result.workspace_bytes);
    for(uint32_t q=0;q<queries;q++) printf("%s%.6f",q ? ",":"",samples[q]);
    printf("]}\n");
    rr_workspace_destroy(w); rr_graph_destroy(g); tracked_free(&memory,path); tracked_free(&memory,h);
    tracked_free(&memory,samples); tracked_free(&memory,sorted); tracked_free(&memory,edges); tracked_free(&memory,offset); if(memory.live) return 7;
    return 0;
}

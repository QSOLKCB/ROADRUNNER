#define _POSIX_C_SOURCE 200809L
#include "roadrunner.h"
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef RR_HEX_SOURCE_ID
#error "Build must bind the hex benchmark to source bytes"
#endif
#ifdef RR_HEX_FROZEN_CLOCK
#define HEX_CLOCK "frozen"
#else
#define HEX_CLOCK "CLOCK_MONOTONIC"
#endif

/* Bounded synthetic fixture, not a general graph-file parser. */
static uint64_t tick(void) {
#ifdef RR_HEX_FROZEN_CLOCK
    return 0;
#else
    struct timespec t;
    if (clock_gettime(CLOCK_MONOTONIC, &t)) { perror("clock_gettime"); exit(2); }
    return (uint64_t)t.tv_sec * UINT64_C(1000000000) + (uint64_t)t.tv_nsec;
#endif
}
static void need(int ok) { if (!ok) { fputs("hex fixture failure\n", stderr); exit(2); } }
static uint64_t distance(uint32_t a, uint32_t b, uint32_t side) {
    int dq=(int)(a%side)-(int)(b%side), dr=(int)(a/side)-(int)(b/side);
    int x=abs(dq), y=abs(dr), z=abs(dq+dr);
    return (uint64_t)(x>y ? (x>z ? x:z) : (y>z ? y:z));
}
int main(int argc, char **argv) {
    if (argc!=3 || (strcmp(argv[1],"4") && strcmp(argv[1],"9") && strcmp(argv[1],"24")) ||
        (strcmp(argv[2],"open") && strcmp(argv[2],"wall") && strcmp(argv[2],"cut"))) return 2;
    uint32_t side=(uint32_t)atoi(argv[1]), n=side*side;
    rr_edge *edges=malloc((size_t)n*6*sizeof(*edges));
    uint64_t *h=malloc((size_t)n*sizeof(*h));
    uint32_t *path=malloc((size_t)n*sizeof(*path));
    need(edges && h && path);
    size_t m=0;
    const int directions[6][2]={{1,0},{0,1},{-1,1},{-1,0},{0,-1},{1,-1}};
    for (uint32_t u=0;u<n;u++) for (int d=0;d<6;d++) {
        int q=(int)(u%side), r=(int)(u/side);
        int vq=q+directions[d][0], vr=r+directions[d][1];
        if (vq<0 || vr<0 || vq>=(int)side || vr>=(int)side) continue;
        int crosses=(q<(int)(side/2))!=(vq<(int)(side/2));
        if (crosses && (!strcmp(argv[2],"cut") || (!strcmp(argv[2],"wall") && !(r==0 && vr==0)))) continue;
        edges[m++]=(rr_edge){u,(uint32_t)vr*side+(uint32_t)vq,1};
    }
    rr_graph *g=NULL; rr_workspace *w[2]={NULL,NULL};
    uint64_t setup=tick();
    need(rr_graph_create(n,m,edges,NULL,&g)==RR_OK);
    need(rr_workspace_create(n,NULL,&w[0])==RR_OK && rr_workspace_create(n,NULL,&w[1])==RR_OK);
    setup=tick()-setup;
    printf("{\"source_id\":\"%s\",\"clock\":\"%s\",\"kind\":\"graph\",\"side\":%u,\"mode\":\"%s\",\"nodes\":%u,\"setup_ns\":%"PRIu64",\"graph_bytes\":%zu,\"edges\":[",RR_HEX_SOURCE_ID,HEX_CLOCK,side,argv[2],n,setup,rr_graph_bytes(g));
    for(size_t i=0;i<m;i++) printf("%s[%u,%u,1]",i?",":"",edges[i].from,edges[i].to);
    puts("]}");
    /* Fixed queries include identity, both crossing directions and boundary goals.
       Two repetitions alternate execution order, with exclusive workspaces. */
    for(uint32_t repeat=0;repeat<2;repeat++) for(uint32_t query=0;query<16;query++) {
        uint32_t s=(query*37U+3U)%n, t=(query*53U+n-1U)%n;
        if(query==0) s=t=0;
        if(query==1) { s=0; t=n-1; }
        if(query==2) { s=n-1; t=0; }
        for(uint32_t turn=0;turn<2;turn++) {
            uint32_t a=(turn+repeat+query)%2;
            rr_result result;
            uint64_t start=tick();
            if(a) for(uint32_t i=0;i<n;i++) h[i]=distance(i,t,side);
            rr_status status=rr_search(g,w[a],a?RR_ASTAR:RR_BFS,s,t,a?h:NULL,path,n,&result);
            uint64_t elapsed=tick()-start;
            need(status==RR_OK || status==RR_NO_PATH);
            need(!result.query_allocations && (!a || result.heuristic_used));
            printf("{\"kind\":\"query\",\"repeat\":%u,\"query\":%u,\"order\":%u,\"algorithm\":\"%s\",\"source\":%u,\"target\":%u,\"status\":%d,\"cost\":%"PRIu64",\"elapsed_ns\":%"PRIu64",\"expanded\":%"PRIu64",\"relaxed\":%"PRIu64",\"pushes\":%"PRIu64",\"workspace_bytes\":%zu,\"query_allocations\":%"PRIu64",\"heuristic_used\":%d,\"path\":[",repeat,query,turn,a?"astar":"bfs",s,t,status,result.cost,elapsed,result.expanded,result.relaxed,result.pushes,result.workspace_bytes,result.query_allocations,result.heuristic_used);
            for(size_t i=0;i<result.path_length;i++) printf("%s%u",i?",":"",path[i]);
            puts("]}");
        }
    }
    rr_workspace_destroy(w[0]); rr_workspace_destroy(w[1]); rr_graph_destroy(g);
    free(edges); free(h); free(path); return 0;
}

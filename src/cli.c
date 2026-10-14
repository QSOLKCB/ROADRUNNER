/* SPDX-License-Identifier: Apache-2.0 */
#include "roadrunner.h"
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#define MAX_NODES 1000000u
#define MAX_EDGES 8000000u
static int number(const char *s,uint64_t *value) {
    uint64_t n=0; if(!*s) return 0;
    for(;*s;s++) { unsigned d=(unsigned char)*s-'0'; if(d>9 || n>(UINT64_MAX-d)/10) return 0; n=n*10+d; }
    *value=n; return 1;
}
/* Reject overlong tokens rather than accepting a truncated numeric prefix. */
static int token(FILE *f,char out[128]) {
    int c; size_t n=0;
    do { c=fgetc(f); } while(c==' ' || c=='\t' || c=='\n' || c=='\r');
    if(c==EOF) return ferror(f) ? -1:0;
    do { if(c==0 || n==127) return -1; out[n++]=(char)c; c=fgetc(f); }
    while(c!=EOF && c!=' ' && c!='\t' && c!='\n' && c!='\r');
    if(ferror(f)) return -1;
    out[n]=0; return 1;
}
static int read_number(FILE *f,uint64_t *v) { char text[128]; return token(f,text)==1 && number(text,v); }
int main(int argc,char **argv) {
    FILE *f=NULL; rr_edge *edges=NULL; uint32_t *path=NULL; rr_graph *g=NULL; rr_workspace *w=NULL;
    rr_result result; rr_status status=RR_INVALID; rr_algorithm algorithm=RR_AUTO;
    uint64_t n,m,source,target,a,b,c; char text[128]; int exit_code=2; clock_t start,end;
    if(argc!=5 && argc!=6) { fprintf(stderr,"usage: %s GRAPH SOURCE TARGET {auto|bfs|dijkstra|astar} [--json]\n",argv[0]); return 2; }
    if(argc==6 && strcmp(argv[5],"--json")) return 2;
    if(!number(argv[2],&source) || !number(argv[3],&target)) goto done;
    if(!strcmp(argv[4],"auto")) algorithm=RR_AUTO;
    else if(!strcmp(argv[4],"bfs")) algorithm=RR_BFS;
    else if(!strcmp(argv[4],"dijkstra")) algorithm=RR_DIJKSTRA;
    else if(!strcmp(argv[4],"astar")) algorithm=RR_ASTAR;
    else goto done;
    f=fopen(argv[1],"rb"); if(!f) { perror(argv[1]); goto done; }
    if(token(f,text)!=1 || strcmp(text,"rr1") || !read_number(f,&n) || !read_number(f,&m) ||
       !n || n>MAX_NODES || m>MAX_EDGES || source>=n || target>=n) goto done;
    if(m>SIZE_MAX/sizeof(*edges) || n>SIZE_MAX/sizeof(*path)) { status=RR_OVERFLOW; goto done; }
    edges=malloc((size_t)(m ? m:1)*sizeof(*edges)); path=malloc((size_t)n*sizeof(*path));
    if(!edges || !path) { status=RR_NO_MEMORY; goto done; }
    for(size_t i=0;i<(size_t)m;i++) {
        if(!read_number(f,&a) || !read_number(f,&b) || !read_number(f,&c) || a>=n || b>=n || c>UINT32_MAX) goto done;
        edges[i]=(rr_edge){(uint32_t)a,(uint32_t)b,(uint32_t)c};
    }
    if(token(f,text)!=0) goto done;
    fclose(f); f=NULL;
    status=rr_graph_create((uint32_t)n,(size_t)m,edges,NULL,&g); if(status!=RR_OK) goto done;
    free(edges); edges=NULL;
    status=rr_workspace_create((uint32_t)n,NULL,&w); if(status!=RR_OK) goto done;
    start=clock(); status=rr_search(g,w,algorithm,(uint32_t)source,(uint32_t)target,NULL,path,(size_t)n,&result); end=clock();
    if(status==RR_OK || status==RR_NO_PATH) {
        printf("{\"status\":\"%s\",\"algorithm\":%d,\"heuristic_used\":%d,\"cost\":",rr_status_string(status),result.algorithm,result.heuristic_used);
        if(status==RR_OK) printf("%" PRIu64,result.cost); else printf("null");
        printf(",\"expanded\":%" PRIu64 ",\"cpu_seconds\":%.9f,\"path\":[",result.expanded,(double)(end-start)/CLOCKS_PER_SEC);
        for(size_t i=0;i<result.path_length;i++) printf("%s%" PRIu32,i ? ",":"",path[i]);
        printf("]}\n"); exit_code=status==RR_OK ? 0:3;
    }
 done:
    if(exit_code==2) fprintf(stderr,"roadrunner: %s\n",rr_status_string(status));
    if(f) fclose(f);
    free(edges); free(path); rr_workspace_destroy(w); rr_graph_destroy(g); return exit_code;
}

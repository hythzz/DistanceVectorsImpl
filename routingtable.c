#include <stdio.h>
#include <string.h>
#include "ne.h"
#include "router.h"

#define DBG 0
#define DBG1 0

int _SubroutesUpdate(struct route_entry route, int costToNbr, int myID, int sender_id);

int NumRoutes;
struct route_entry routingTable[MAX_ROUTERS];


void InitRoutingTbl (struct pkt_INIT_RESPONSE *InitResponse, int myID)
{
    int i = 0;
    NumRoutes = InitResponse->no_nbr + 1;
    routingTable[0].dest_id = myID;
    routingTable[0].next_hop = myID;
    routingTable[0].cost = 0;
    for (i = 0; i < NumRoutes - 1; i++) {
        routingTable[i+1].dest_id = InitResponse->nbrcost[i].nbr;
        routingTable[i+1].next_hop = InitResponse->nbrcost[i].nbr;
        routingTable[i+1].cost = InitResponse->nbrcost[i].cost;
    }    
#if DBG
    for (i = 0; i < MAX_ROUTERS; i++)
        printf("%s: dest_id = %d, next_hop = %d, cost = %d\n", \
                __func__, routingTable[i].dest_id, routingTable[i].next_hop, routingTable[i].cost);
#endif
}

int UpdateRoutes(struct pkt_RT_UPDATE *RecvdUpdatePacket, int costToNbr, int myID)  // dest = myID, dest not exist, split horizon rule, force update, find a longer path, find a shorter path
{
	int i;
	int change_num = 0;

	if (RecvdUpdatePacket->dest_id != myID) return 0;

	for (i = 0; i < RecvdUpdatePacket->no_routes; i++) {
#if DBG1
	  printf("Num of routes: %d\n", RecvdUpdatePacket->no_routes);
#endif
		change_num =  _SubroutesUpdate(RecvdUpdatePacket->route[i], costToNbr, myID, RecvdUpdatePacket->sender_id) || change_num;
	}

    return change_num;
}

int _SubroutesUpdate(struct route_entry route, int costToNbr, int myID, int sender_id)  // dest = myID, dest not exist, split horizon rule, force update, find a longer path, find a shorter path
{
	int i;
	int next_cost = route.cost;

	for (i = 0; i < NumRoutes; i++) {
		if (route.dest_id == routingTable[i].dest_id) {
			//printf("\nRdest: %d, local dest id: %d\n", route.dest_id, routingTable[i].dest_id);
			break;
		}
	}

#if DBG
	printf("\ni: %d\n", i);
	printf("NbrID: %d\n", sender_id);
	printf("cost: %d, costToNbr: %d\n", next_cost, costToNbr);
#endif

	if (i == 0) return 0;  // dest = myID ( this is the dest )

	if (i == NumRoutes) {  // dest not exist ( new dest )
		routingTable[NumRoutes].dest_id = route.dest_id;
		routingTable[NumRoutes].next_hop = sender_id;
		routingTable[NumRoutes].cost = next_cost+costToNbr;
		if (routingTable[NumRoutes].cost > INFINITY) routingTable[NumRoutes].cost = INFINITY;
		NumRoutes++;
#if DBG1
	printf("\ni: %d\n", i);
	printf("cost: %d, costToNbr: %d\n", next_cost, costToNbr);
#endif
		return 1;
	}

	if (route.next_hop == myID) return 0;  // split horizon rule ( avoid infinite loop )

	if (routingTable[i].next_hop == sender_id) {  // forced update
		if (routingTable[i].cost == next_cost + costToNbr) return 0;  // unchanged
		else if (routingTable[i].cost >= INFINITY && (next_cost + costToNbr) >= INFINITY) return 0;
		else {
			routingTable[i].cost = next_cost + costToNbr;
			if (routingTable[i].cost > INFINITY) routingTable[i].cost = INFINITY;
#if DBG1
	printf("\ni: %d\n", i);
	printf("cost: %d, costToNbr: %d\n", next_cost, costToNbr);
#endif
			return 1;
		}
	}

	if (routingTable[i].cost <= next_cost + costToNbr) return 0; // find a longer path
	
	// find a shorter path
	routingTable[i].next_hop = sender_id;
	routingTable[i].cost = next_cost + costToNbr;
	if (routingTable[NumRoutes].cost > INFINITY) routingTable[NumRoutes].cost = INFINITY;
#if DBG1
	printf("\ni: %d\n", i);
	printf("cost: %d, costToNbr: %d\n", next_cost, costToNbr);
#endif
    return 1;
}

void ConvertTabletoPkt(struct pkt_RT_UPDATE *UpdatePacketToSend, int myID)
{
    UpdatePacketToSend->sender_id = myID;
    UpdatePacketToSend->no_routes = NumRoutes;
    memcpy(UpdatePacketToSend->route, routingTable, sizeof(struct route_entry) * MAX_ROUTERS);
}

void PrintRoutes (FILE* Logfile, int myID)
{
	int i = 0;
	fprintf(Logfile, "\nRouting Table:");
#if DBG
	printf("\nRouting Table:");
#endif
	for (i = 0; i < NumRoutes; i++) {
		fprintf(Logfile, "\nR%d -> R%d: R%d, %d", myID, routingTable[i].dest_id, routingTable[i].next_hop, routingTable[i].cost);
#if DBG
		printf("\nR%d -> R%d: R%d, %d", myID, routingTable[i].dest_id, routingTable[i].next_hop, routingTable[i].cost);
#endif
	}
	fprintf(Logfile, "\n");
#if DBG
	printf("\n");
#endif
	fflush(Logfile);
}

void UninstallRoutesOnNbrDeath(int DeadNbr)
{
	int i = 0;
	for (i = 0; i < NumRoutes; i++) {  // next_hop == DeadNbr to find the deadnbr. cost != 0 to avoid itself
		if (routingTable[i].next_hop == DeadNbr && routingTable[i].cost != 0) routingTable[i].cost = INFINITY;
	}
}

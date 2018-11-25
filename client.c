#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>

struct rdma_cm_event		*cm_evt, cm_evt_copy;
struct rdma_event_channel	*cm_ch;
struct rdma_cm_id			*cm_id;
struct rdma_addrinfo		*raddr, hints;
struct rdma_conn_param		cm_param;

uint16_t	port;

int
process_evt(struct rdma_cm_event *evt)
{
	int rval, ret = 0;

	switch (evt->event) {
	case RDMA_CM_EVENT_ADDR_RESOLVED:
		printf("%s: ADDR_RESOLVED\n", __func__);

		/* Now try to resolve the route ... timeout is 500ms. */
		rval = rdma_resolve_route(evt->id, 500);
		if (rval != 0) {
			printf("%s: rdma_resolve_route() failed. rval=%d\n", __func__, rval);
			ret = 1;
		}
		break;

	case RDMA_CM_EVENT_ROUTE_RESOLVED:
		printf("%s: ROUTE_RESOLVED ... trying to connect ...\n", __func__);

		memset(&cm_param, 0, sizeof(cm_param));
		cm_param.responder_resources = 1;
		cm_param.initiator_depth = 1;
		cm_param.retry_count = 7;

		rval = rdma_connect(evt->id, &cm_param);
		if (rval != 0) {
			printf("%s: rdma_connect() failed. rval=%d\n", __func__, rval);
			ret = 1;
		}
		break;

	case RDMA_CM_EVENT_REJECTED:
		printf("%s: REJECTED ... exiting\n", __func__);
		ret = 1;
		break;

	case RDMA_CM_EVENT_ESTABLISHED:
		printf("%s: ESTABLISHED\n", __func__);

		rval = rdma_disconnect(evt->id);
		if (rval != 0) {
			printf("%s: rdma_disconnect() failed. rval=%d\n", __func__, rval);
			ret = 1;
		}
		break;

	case RDMA_CM_EVENT_DISCONNECTED:
		printf("%s: DISCONNECTED ... exiting \n", __func__);
		ret = 1;
		break;
	default:
		printf("%s: unknown event. event=%d\n", __func__, evt->event);
	}
	return (ret);
}


int
main(int argc, char *argv[])
{
	int ret;

	/* 
	 * Create CM channel. CM channel structure is really  simple. It has a FD in
	 * it, which we print below.
	 */
	cm_ch = rdma_create_event_channel();
	if (cm_ch == NULL) {
		printf("Failed to allocate evt_ch.\n");
		return -1;
	}
	printf ("cm_ch=%p fd=%d\n", cm_ch, cm_ch->fd);

	/* Create CM ID ... roughtly equal to a socket. */
	ret = rdma_create_id(cm_ch, &cm_id, NULL, RDMA_PS_TCP);
	if (ret != 0) {
		printf("Failed to create CM ID.\n");
		return -1;
	}
	printf ("cm_id=%p\n", cm_id);

	/* Server side we set PASSIVE. */
	// hints.ai_flags = RAI_PASSIVE;
	hints.ai_port_space = RDMA_PS_TCP;

	/*
	 * Get the addr filled so we can use it for binding. 
	 * We are going to use 5000 as the SID.DPort.
	 */
	ret = rdma_getaddrinfo("10.1.1.59", "5000", &hints, &raddr);
	if (ret != 0) {
		printf("rdma_getaddrinfo() failed\n");
		return -1;
	}

	/* Now we call CM to resolv the address. We give 500ms as timeout. */
	ret = rdma_resolve_addr(cm_id, NULL, raddr->ai_dst_addr, 500);

	/* Now wait for CM events on the CM channel. */
	while (rdma_get_cm_event(cm_ch, &cm_evt) == 0) {
		/* Make a local copy of the CM event data. */
		memcpy(&cm_evt_copy, cm_evt, sizeof(*cm_evt));
		rdma_ack_cm_event(cm_evt);

		/* Let's look at the event we received. */
		if (process_evt(&cm_evt_copy) != 0) {
			break;
		}
	}

	/* Destroy the CM ID and Channel. */
	rdma_destroy_id(cm_id);
	rdma_destroy_event_channel(cm_ch);

	return 0;
}


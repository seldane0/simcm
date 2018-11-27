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
struct ibv_pd				*ib_pd;
struct ibv_comp_channel		*ib_ch;
struct ibv_cq				*ib_cq;
struct ibv_qp_init_attr		ib_qp_attr;


int
setup_resources(struct rdma_cm_event *evt)
{
	int ret;

	ib_pd = ibv_alloc_pd(evt->id->verbs);
	if (ib_pd == NULL) {
		printf("%s: ibv_alloc_pd() failed.\n", __func__);
		return 1;
	}
	printf ("%s: ib_pd=%p\n", __func__, ib_pd);

	ib_ch = ibv_create_comp_channel(evt->id->verbs);
	if (ib_ch == NULL) {
		printf("%s: ibv_create_comp_channel() failed.\n", __func__);
		return 1;
	}
	printf ("%s: ib_ch=%p\n", __func__, ib_ch);

	/* CQ with 16 entries... */
	ib_cq = ibv_create_cq(evt->id->verbs, 16, NULL, ib_ch, 0);
	if (ib_cq == NULL) {
		printf("%s: ibv_create_cq() failed.\n", __func__);
		return 1;
	}

	/* Notify for all events... no filter. */
	ret = ibv_req_notify_cq(ib_cq, 0);
	if (ret != 0) {
		printf("%s: ibv_req_notify()\n", __func__);
		return 1;
	}

	/* Last step ... setup the QP. */
	bzero(&ib_qp_attr, sizeof(ib_qp_attr));
	ib_qp_attr.cap.max_recv_sge = 2;
	ib_qp_attr.cap.max_recv_wr  = 8;
	ib_qp_attr.cap.max_send_sge = 2;
	ib_qp_attr.cap.max_send_wr  = 8;
	ib_qp_attr.qp_type			= IBV_QPT_RC;
	ib_qp_attr.recv_cq			= ib_cq;
	ib_qp_attr.send_cq			= ib_cq;

	ret = rdma_create_qp(evt->id, ib_pd, &ib_qp_attr);
	if (ret != 0) {
		printf("%s: rdma_create_qp() failed.\n", __func__);
		return 1;
	}

	return 0;
}

int
process_evt(struct rdma_cm_event *evt)
{
	int rval, ret = 0;

	switch (evt->event) {
	case RDMA_CM_EVENT_CONNECT_REQUEST:
		printf("%s: CONNECT ... returning accept ...\n", __func__);

		/* Now setup resources. */
		rval = setup_resources(evt);
		if (rval != 0) {
			ret = 1;
			break;
		}

		// rdma_reject(evt->id, NULL, 0);
		memset(&cm_param, 0, sizeof(cm_param));
		cm_param.responder_resources = 1;
		cm_param.initiator_depth = 1;
		cm_param.retry_count = 7;

		rval = rdma_accept(evt->id, &cm_param);
		if (rval != 0) {
			printf("%s: rdma_accept() failed. rval=%d\n", __func__, rval);
			ret = 1;
		}
		break;
    case RDMA_CM_EVENT_REJECTED:
        printf("%s: REJECTED\n", __func__);
        ret = 1;
        break;

	case RDMA_CM_EVENT_ESTABLISHED:
		printf("%s: ESTABLISHED\n", __func__);
		break;

	case RDMA_CM_EVENT_DISCONNECTED:
		printf("%s: DISCONNECTED\n", __func__);
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
	hints.ai_flags = RAI_PASSIVE;
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

	/* Now bind to address. */
	ret = rdma_bind_addr(cm_id, raddr->ai_src_addr);
	if (ret != 0) {
		printf("Failed to bind.\n");
		perror("Error: ");
		return -1;
	}

	port = ntohs(rdma_get_src_port(cm_id));
	printf("Bind port=%d\n", port);

	/* Let's start listening... */
	ret = rdma_listen(cm_id, 5);
	if (ret != 0) {
		printf("Failed to listen.\n");
		return -1;
	}
	port = ntohs(rdma_get_src_port(cm_id));
	printf("Listen port=%d\n", port);

	/* Now wait for CM events on the CM channel. */
	while (rdma_get_cm_event(cm_ch, &cm_evt) == 0) {
		/* Make a local copy of the CM event data. */
		memcpy(&cm_evt_copy, cm_evt, sizeof(*cm_evt));

		/* Send ACK back to the sender. */
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


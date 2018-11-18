#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>

struct rdma_cm_event		*cm_evt, cm_evt_copy;
struct rdma_event_channel	*cm_ch;
struct rdma_cm_id			*cm_id;
struct sockaddr_in			addr;
uint16_t	port;

int
main(int argc, char *argv[])
{
	int ret;

	/* Create CM channel. */
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

	/* Pick IPv4 address. */
	memset(&addr, 0, sizeof(addr));
	addr.sin_family = AF_INET;

	/* Now bind to address. */
	ret = rdma_bind_addr(cm_id, (struct sockaddr *)&addr);
	if (ret != 0) {
		printf("Failed to bind.\n");
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
		memcpy(&cm_evt_copy, cm_evt, sizeof(*cm_evt));
		rdma_ack_cm_event(cm_evt);
	}

	/* Destroy the CM ID and Channel. */
	rdma_destroy_id(cm_id);
	rdma_destroy_event_channel(cm_ch);

	return 0;
}

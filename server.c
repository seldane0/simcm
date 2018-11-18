#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <rdma/rdma_cma.h>

struct rdma_event_channel	*evt_ch;

int
main(int argc, char *argv[])
{
	evt_ch = rdma_create_event_channel();
	if (evt_ch == NULL) {
		printf("Failed to allocate evt_ch.\n");
		return -1;
	}
	printf ("evt_ch=%p fd=%d\n", evt_ch, evt_ch->fd);


	/* Destroy the event channel. */
	rdma_destroy_event_channel(evt_ch);

	return 0;
}

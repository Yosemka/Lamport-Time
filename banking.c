#include "banking.h"
#include "ipc.h"
#include "process.h"


void transfer(void * parent_data, local_id src, local_id dst, balance_t amount)
{
	//newEvent = 1;
	localTime++;
	Message startTransfer = {{MESSAGE_MAGIC, sizeof(TransferOrder), TRANSFER, get_lamport_time()}, ""};
	TransferOrder order = {src, dst, amount};

	memcpy(&startTransfer.s_payload, &order, sizeof(TransferOrder));

	send(parent_data, src, &startTransfer);
	//printf("%d: process 0 send %s to process %d\n", (int)get_lamport_time(), messageType[startTransfer.s_header.s_type], src);
	
	Message ackResult;

	receive(parent_data, dst, &ackResult);
	if(ackResult.s_header.s_type == ACK)
	{
		localTime = MAX(localTime, ackResult.s_header.s_local_time);
		localTime++;
		//printf("%d: process 0 get %s from process %d\n", get_lamport_time(), messageType[ackResult.s_header.s_type], dst);
	}
}


#include "ipc.h"
#include "process.h"
#include "banking.h"

int send(void * self, local_id dst, const Message * msg)
{
	int num = 0;
	int id = *(int*)self;

	//message = заголовок (структура) + тело сообения (char*)
	//	Заколовок = магика + тип + длина сообщения + время
	//write возвращает количество записанных байт
	//write(int descriptor, void * val, size)
	
	num = write(pipes[id][dst].field[WRITE], msg, sizeof(MessageHeader) + msg->s_header.s_payload_len);
	
	if(num > 0)
	{
		//printf("ch[%d] WRITING {%s} { %d } bytes to ch[%d] lamport_time(%d)\n", id, messageType[msg->s_header.s_type], num, dst, msg->s_header.s_local_time);
		return SUCCESS;
	}
	else if(EAGAIN == errno)
	{
		printf("Error writing from ch[%d] to ch[%d]\n", id, dst);
		return UNSUCCESS;
	}	
	return UNSUCCESS;
}

int send_multicast(void * self, const Message * msg)
{
	int id = *(int*)self;	//разыменовывание указателя
	
	for(int j = 0; j <= chProcAmount; j++)
	{	
		if(id != j)
		{
			if(send(&id, j, msg) == UNSUCCESS)
			{
				return UNSUCCESS;
			}
		}
	}
	return SUCCESS;
}

int receive(void * self, local_id from, Message * msg)
{
	int tmp = 0, tmp2 = 0;
	MessageHeader mh;		//временная переменная для хранения заголовка сообщения
	//char buf[MAX_PAYLOAD_LEN];
	int id = *(int*)self;
	
	//read возвращает количество записанных байт

	while(1)
	{
		tmp = read(pipes[from][id].field[READ], &mh, sizeof(MessageHeader));
		if(tmp <= 0)
		{
			if(EAGAIN == errno || tmp == 0)
				continue;
			else
			{
				printf("Error 1 reading pipe from %d to %d in ch[%d]\n", from, id, id);
				return UNSUCCESS;
			}
		}
		else
		{
			tmp2 = read(pipes[from][id].field[READ], msg->s_payload, mh.s_payload_len);

			//printf("ch[%d] READING {%s} %d symbols from ch[%d] %d\n", 
			//	   id, messageType[mh.s_type], tmp2 + tmp, from, mh.s_payload_len);	
			
			if(tmp2 == -1)
			{
				printf("Error 2 reading pipe from %d to %d in ch[%d]", from, id, id);
				return UNSUCCESS;
			}
			else
			{
				break;
			}	
		}		
	}	

	msg->s_header = mh;
	return (int)mh.s_type;
}

int receive_any(void * self, Message * msg)
{
	int id = *(int*)self;
	int from = 0;
	int tmp = 0;
	
	do
	{
		from++;
		if(from == id) from++;
		if(from > chProcAmount) from = 0;

		tmp = read(pipes[from][id].field[READ], &msg->s_header, sizeof(MessageHeader));
		if(id == 0)
			printf("READING %d %d\n", id, from);
		switch(tmp)
		{
			case -1:continue;
			case 0:  continue;
			default: 
				tmp = read(pipes[from][id].field[READ], &msg->s_payload, msg->s_header.s_payload_len);
				return from;
		}
	}while(1);
}

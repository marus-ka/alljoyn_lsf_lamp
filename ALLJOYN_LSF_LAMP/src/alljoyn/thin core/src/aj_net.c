/**
 * @file
 */
/******************************************************************************
 * Copyright AllSeen Alliance. All rights reserved.
 *
 *    Permission to use, copy, modify, and/or distribute this software for any
 *    purpose with or without fee is hereby granted, provided that the above
 *    copyright notice and this permission notice appear in all copies.
 *
 *    THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *    WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *    MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *    ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *    WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *    ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *    OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 ******************************************************************************/

#define AJ_MODULE NET
// IANA assigned UDP multicast port for AllJoyn
#define AJ_UDP_PORT 5353
//9956
//-----------------------------------------------------------------------------
#include <compiler.h>
#include "main.h"
//-----------------------------------------------------------------------------
#include "aj_target.h"
#include "aj_bufio.h"
#include "aj_net.h"
#include "aj_util.h"
#include "aj_debug.h"
#include "aj_disco.h"
#include "socket/include/socket.h"
#include "driver/include/m2m_wifi.h"
#include "delay.h"
#include <stdlib.h>
//-----------------------------------------------------------------------------
static uint8_t rxDataStash[256];
static uint16_t rxLeftover = 0;

//IANA assigned IPv4 multicast group for AllJoyn.
static const char AJ_IPV4_MULTICAST_GROUP[] = "224.0.0.113";

//IANA assigned IPv6 multicast group for AllJoyn.
static const char AJ_IPV6_MULTICAST_GROUP[] = "ff02::13a";
extern SOCKET rx_socket;
extern SOCKET tx_socket;
extern SOCKET tcp_client_socket;
extern struct sockaddr_in addr;
//extern volatile uint8_t gau8SocketTestBuffer[MAIN_WIFI_M2M_BUFFER_SIZE];
//extern volatile uint8_t udp_data_buffer[MAIN_WIFI_M2M_BUFFER_SIZE];
//static uint8_t rxData[1454];
//static uint8_t txData[1024];
//extern uint8_t data_to_send[1400];

extern volatile uint8_t udp_data_tx[MAIN_WIFI_M2M_BUFFER_SIZE];
extern volatile uint8_t udp_data_rx[MAIN_WIFI_M2M_BUFFER_SIZE];

extern volatile uint8_t tcp_data_tx[1400];
extern volatile uint8_t tcp_data_rx[1400];

volatile uint8_t AJ_in_data_tcp[1400]={0};

uint32 u32EnableCallbacks = 0;
extern volatile uint8_t tcp_ready_to_send;
extern volatile uint16_t tcp_rx_ready;
extern volatile uint8_t tcp_tx_ready;
//-----------------------------------------------------------------------------
AJ_Status AJ_Net_Send(AJ_IOBuffer* buf)
{
    uint32_t ret;
    uint32_t tx = AJ_IO_BUF_AVAIL(buf);

    printf("AJ_Net_Send(buf=0x%p)\n", buf);
 //   printf("tcp_client_socket=%d", tcp_client_socket);
    if (tx > 0) 
	{
      //  ret = g_client.write(buf->readPtr, tx);
		send(tcp_client_socket, buf->readPtr, tx, 0);
	//	while(tcp_tx_ready==0) m2m_wifi_handle_events(NULL);
     /*   if (ret != 0) 
		{
            //AJ_ErrPrintf(("AJ_Net_Send(): send() failed. error=%d, status=AJ_ERR_WRITE\n", g_client.getWriteError()));
            return AJ_ERR_WRITE;
        }*/
		
        buf->readPtr += tcp_tx_ready;
		tcp_tx_ready=0;
    }
 //   if (AJ_IO_BUF_AVAIL(buf) == 0)
//	{
	       AJ_IO_BUF_RESET(buf);
//	}
    //printf("AJ_Net_Send end\n");
    return AJ_OK;
}

AJ_Status AJ_Net_Recv(AJ_IOBuffer* buf, uint32_t len, uint32_t timeout)
{
    AJ_Status status = AJ_ERR_READ;
    uint32_t ret;
    uint32_t rx = AJ_IO_BUF_SPACE(buf);
    uint32_t recvd = 0;
    unsigned long Recv_lastCall = millis();

    // first we need to clear out our buffer
    uint32_t M = 0;
	//printf("AJ_Net_Recv(buf=0x%p, len=%d., timeout=%d.)\n", buf, len, timeout);
    if (rxLeftover != 0)
	{
	// there was something leftover from before,
	//    printf("AJ_NetRecv(): leftover was: %d\n", rxLeftover);
		M = min(rx, rxLeftover);
		memcpy(buf->writePtr, rxDataStash, M);  // copy leftover into buffer.
		buf->writePtr += M;  // move the data pointer over
		memmove(rxDataStash, rxDataStash + M, rxLeftover - M); // shift left-overs toward the start.
		rxLeftover -= M;
		recvd += M;
		// we have read as many bytes as we can
		// higher level isn't requesting any more
		if (recvd == rx)
		{
	//		printf("AJ_Net_Recv(): status=AJ_OK\n");
			return AJ_OK;
		}
	}
	if ((M != 0) && (rxLeftover != 0)) 
	{
	   printf("AJ_Net_REcv(): M was: %d, rxLeftover was: %d\n", M, rxLeftover);
	}
	//printf("NetRecv before while: tcp_data_rx[0]= %d\n", tcp_data_rx[0]);
	while ((tcp_rx_ready==0) && (millis() - Recv_lastCall < timeout))
	{
	//	recv(tcp_client_socket, gau8SocketTestBuffer, sizeof(gau8SocketTestBuffer), 0);
		recv(tcp_client_socket, tcp_data_rx, sizeof(tcp_data_rx), 0);
	//	recv(tcp_client_socket, buf->writePtr, sizeof(tcp_data_rx), 0);
		
		m2m_wifi_handle_events(NULL);
	}
//	printf("NetRecv: tcp_data_rx[0]= %d\n", tcp_data_rx[0]);
    if (tcp_rx_ready==0) 
	{
		printf("AJ_Net_Recv(): timeout. status=AJ_ERR_TIMEOUT\n");
        status = AJ_ERR_TIMEOUT;
    } 
	else
	{    
	   memcpy(AJ_in_data_tcp, tcp_data_rx,tcp_rx_ready);
	   uint32_t askFor = rx;
	   askFor -= M;
	   ret=tcp_rx_ready;
	 //  printf("AJ_Net_Recv(): ask for: %d\n", askFor);
	   if (askFor < ret) 
	   {
		   printf("AJ_Net_Recv(): BUFFER OVERRUN: askFor=%u, ret=%u\n", askFor, ret);
	   }
       if (ret == -1) 
	   {
	        printf("AJ_Net_Recv(): read() failed. status=AJ_ERR_READ\n");
	        status = AJ_ERR_READ;
	   } 
	   else
	   {
	    //    printf("AJ_Net_Recv(): ret now %d\n", ret);
	        AJ_DumpBytes("Recv", buf->writePtr, ret);

	        if (ret > askFor) 
			{
		        printf("AJ_Net_Recv(): new leftover %d\n", ret - askFor);
		        // now shove the extra into the stash
		        memcpy(rxDataStash + rxLeftover, buf->writePtr + askFor, ret - askFor);
		        rxLeftover += (ret - askFor);
		        buf->writePtr += rx;
		    }
			else
			{
		        buf->writePtr += ret;
	        }
	        status = AJ_OK;
        }
    }
  //  printf("!!!!!!!!!!!!!!!buf->writePtr=%x\n",buf->writePtr);
    tcp_rx_ready=0;
    return status;
}

/*
 * Need enough RX buffer space to receive a complete name service packet when
 * used in UDP mode.  NS expects MTU of 1500 subtracts UDP, IP and Ethernet
 * Type II overhead.  1500 - 8 -20 - 18 = 1454.  txData buffer size needs to
 * be big enough to hold a NS WHO-HAS for one name (4 + 2 + 256 = 262) in UDP
 * mode.  TCP buffer size dominates in that case.
 */
AJ_Status AJ_Net_Connect(AJ_BusAttachment* bus, const AJ_Service* service)
{
    int ret;
    //  IPAddress ip(service->ipv4);

    if (!(service->addrTypes & AJ_ADDR_TCP4)) 
	{
  //      AJ_ErrPrintf(("AJ_Net_Connect(): only IPV4 TCP supported\n", ret));
        return AJ_ERR_CONNECT;
    }
 //   printf("AJ_Net_Connect(netSock=0x%p, addrType=%d.)\n", netSock, addrType);
   
    printf("AJ_Net_Connect()\n");
    // ret = g_client.connect(ip, service->ipv4port);
	//   	addr.sin_port = _htons(48256);
	//   	addr.sin_addr.s_addr = _htonl(0xc0a8141d);
   	addr.sin_port = _htons(service->ipv4port);
    addr.sin_addr.s_addr = _htonl(service->ipv4);
	printf("AJ_Net_Connect(): ipv4= %x, port = %d\n",addr.sin_addr.s_addr,	addr.sin_port);
    tcp_client_socket = socket(AF_INET, SOCK_STREAM, 0);
	ret=connect(tcp_client_socket, (struct sockaddr *)&addr, sizeof(struct sockaddr_in));
	 printf("AJ_Net_Connect(): connect\n");
	//ждем подключения
	while(tcp_ready_to_send==0)
	{
		m2m_wifi_handle_events(NULL);
	}
	 printf("AJ_Net_Connect(): connect OK\n");
    if (ret == -1) 
	{
        //AJ_ErrPrintf(("AJ_Net_Connect(): connect() failed: %d: status=AJ_ERR_CONNECT\n", ret));
        return AJ_ERR_CONNECT;
    } 
	else
	{
       //AJ_IOBufInit(&netSock->rx, rxData, sizeof(rxData), AJ_IO_BUF_RX, (void*)&g_clientUDP);
       bus->sock.rx.bufStart = AJ_in_data_tcp;
       bus->sock.rx.bufSize = sizeof(AJ_in_data_tcp);
       bus->sock.rx.readPtr = AJ_in_data_tcp;
       bus->sock.rx.writePtr = AJ_in_data_tcp;
       bus->sock.rx.direction = AJ_IO_BUF_RX;
       bus->sock.rx.recv = AJ_Net_Recv;
       //   AJ_IOBufInit(&netSock->tx, txData, sizeof(txData), AJ_IO_BUF_TX, (void*)&g_clientUDP);
       bus->sock.tx.bufStart = tcp_data_tx;
       bus->sock.tx.bufSize = sizeof(tcp_data_tx);
       bus->sock.tx.readPtr = tcp_data_tx;
       bus->sock.tx.writePtr = tcp_data_tx;
       bus->sock.tx.direction = AJ_IO_BUF_TX;
       bus->sock.tx.send = AJ_Net_Send;
        printf("AJ_Net_Connect(): connect() success: status=AJ_OK\n");
        return AJ_OK;
    }
    printf("AJ_Net_Connect(): connect() failed: %d: status=AJ_ERR_CONNECT\n", ret);
    return AJ_ERR_CONNECT;
}

void AJ_Net_Disconnect(AJ_NetSocket* netSock)
{
 //   printf("AJ_Net_Disconnect(nexSock=0x%p)\n", netSock);
    close(tcp_client_socket);
	tcp_client_socket = -1;
	tcp_ready_to_send=0;
	tcp_tx_ready=0;
    //g_client.stop();
}
extern volatile int sock_rx_state;
extern volatile uint8_t sock_tx_state;

AJ_Status AJ_Net_SendTo(AJ_IOBuffer* buf)
{
    int ret;
    uint32_t tx = AJ_IO_BUF_AVAIL(buf);

    //AJ_InfoPrintf(("AJ_Net_SendTo(buf=0x%p)\n", buf));

    if (tx > 0)
	{
  	    ret = sendto(rx_socket, buf->readPtr, tx, 0, (struct sockaddr *)&addr, sizeof(addr));
		m2m_wifi_handle_events(NULL);
        //AJ_InfoPrintf(("AJ_Net_SendTo(): SendTo write %d\n", ret));
        if (sock_tx_state != 1) 
		{
            //AJ_ErrPrintf(("AJ_Net_Sendto(): no bytes. status=AJ_ERR_WRITE\n"));
            return AJ_ERR_WRITE;
        }

        buf->readPtr += ret;

    }
    AJ_IO_BUF_RESET(buf);
    //AJ_InfoPrintf(("AJ_Net_SendTo(): status=AJ_OK\n"));
    return AJ_OK;
}

AJ_Status AJ_Net_RecvFrom(AJ_IOBuffer* buf, uint32_t len, uint32_t timeout)
{
    //AJ_InfoPrintf(("AJ_Net_RecvFrom(buf=0x%p, len=%d., timeout=%d.)\n", buf, len, timeout));

    AJ_Status status = AJ_OK;
    int ret;
    uint32_t rx = AJ_IO_BUF_SPACE(buf);
    unsigned long Recv_lastCall = millis();

  //  printf("AJ_Net_RecvFrom(): len %d, rx %d, timeout %d\n", len, rx, timeout);
	
  //  rx = min(rx, len);

   while ((sock_rx_state==0) && (millis() - Recv_lastCall < timeout))
    {
		//printf("millis() - Recv_lastCall = %d \n", (millis() - Recv_lastCall));
		recv(rx_socket, udp_data_rx, MAIN_WIFI_M2M_BUFFER_SIZE, 0);
		m2m_wifi_handle_events(NULL);
		
   }
   ret=sock_rx_state;
  // printf("AJ_Net_RecvFrom(): millis %d, Last_call %d, timeout %d, Avail %d\n", millis(), Recv_lastCall, timeout, g_clientUDP.available());
    //ret = g_clientUDP.read(buf->writePtr, rx);
	
    //AJ_InfoPrintf(("AJ_Net_RecvFrom(): read() returns %d, rx %d\n", ret, rx));

    if (ret == -1) 
	{
        printf("AJ_Net_RecvFrom(): read() fails. status=AJ_ERR_READ\n");
        status = AJ_ERR_READ;
    }
	else
	{
        if (ret != -1) 
		{
            AJ_DumpBytes("AJ_Net_RecvFrom", buf->writePtr, ret);
        }
        buf->writePtr += ret;
     //   printf("AJ_Net_RecvFrom(): status=AJ_OK\n");
        status = AJ_OK;
    }
    printf("AJ_Net_RecvFrom(): status=%s\n", AJ_StatusText(status));
    return /*sock_rx_state;*/status;
}

uint16_t AJ_EphemeralPort(void)
{
    // Return a random port number in the IANA-suggested range
	srand(65535 - 49152);
    return 49152 + rand();
}


AJ_Status AJ_Net_MCastUp(AJ_NetSocket* netSock)
{
    uint8_t ret = 1;

    //AJ_InfoPrintf(("AJ_Net_MCastUp(nexSock=0x%p)\n", netSock));

    //
    // Arduino does not choose an ephemeral port if we enter 0 -- it happily
    // uses 0 and then increments each time we bind, up through the well-known
    // system ports.
    //
    //   ret = g_clientUDP.begin(AJ_EphemeralPort()); // библиотека arduino WiFiUdp.cpp

    if (ret != 1)
	{
        //g_clientUDP.stop();
        //AJ_ErrPrintf(("AJ_Net_MCastUp(): begin() fails. status=AJ_ERR_READ\n"));
        return AJ_ERR_READ;
    }
	else 
	{
		// aj_bufio проблема в последнем параметре, он из ардуино и что значит, я не понимаю
        //AJ_IOBufInit(&netSock->rx, rxData, sizeof(rxData), AJ_IO_BUF_RX, (void*)&g_clientUDP);
		netSock->rx.bufStart = udp_data_rx;
		netSock->rx.bufSize = sizeof(udp_data_rx);
		netSock->rx.readPtr = udp_data_rx;
		netSock->rx.writePtr = udp_data_rx;
		netSock->rx.direction = AJ_IO_BUF_RX;
        netSock->rx.recv = AJ_Net_RecvFrom;
        //   AJ_IOBufInit(&netSock->tx, txData, sizeof(txData), AJ_IO_BUF_TX, (void*)&g_clientUDP);
		netSock->tx.bufStart = udp_data_tx;
		netSock->tx.bufSize = sizeof(udp_data_tx);
		netSock->tx.readPtr = udp_data_tx;
		netSock->tx.writePtr = udp_data_tx;
		netSock->tx.direction = AJ_IO_BUF_TX;
        netSock->tx.send = AJ_Net_SendTo;
    }

    //AJ_InfoPrintf(("AJ_Net_MCastUp(): status=AJ_OK\n"));
    return AJ_OK;
}

void AJ_Net_MCastDown(AJ_NetSocket* netSock)
{
   // printf("AJ_Net_MCastDown(nexSock=0x%p)\n", netSock);
    //g_clientUDP.flush();
    //g_clientUDP.stop();
	//close(tx_socket);
	//close(rx_socket);
}

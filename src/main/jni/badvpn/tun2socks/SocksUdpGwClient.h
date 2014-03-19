/*
 * Copyright (C) Ambroz Bizjak <ambrop7@gmail.com>
 * Contributions:
 * Transparent DNS: Copyright (C) Kerem Hadimli <kerem.hadimli@gmail.com>
 * 
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the
 *    names of its contributors may be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 * 
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef BADVPN_TUN2SOCKS_SOCKSUDPGWCLIENT_H
#define BADVPN_TUN2SOCKS_SOCKSUDPGWCLIENT_H

#include <misc/debug.h>
#include <base/DebugObject.h>
#include <system/BReactor.h>
#ifdef ANDROID
#include <protocol/udpgw_proto.h>
#include <protocol/packetproto.h>
#include <system/BDatagram.h>
#include <flow/PacketBuffer.h>
#include <flow/SinglePacketBuffer.h>
#include <flow/BufferWriter.h>
#include <structure/BAVL.h>
#include <structure/LinkedList1.h>
#include <misc/offset.h>
#else
#include <udpgw_client/UdpGwClient.h>
#include <socksclient/BSocksClient.h>
#endif

typedef void (*SocksUdpGwClient_handler_received) (void *user, BAddr local_addr, BAddr remote_addr, const uint8_t *data, int data_len);

typedef struct {
    int udp_mtu;
    BAddr socks_server_addr;
    const struct BSocksClient_auth_info *auth_info;
    size_t num_auth_info;
    BAddr remote_udpgw_addr;
    BReactor *reactor;
    void *user;
    SocksUdpGwClient_handler_received handler_received;
#ifdef ANDROID
    int udpgw_mtu;
    int num_connections;
    int max_connections;
    BAVL connections_tree;
    LinkedList1 connections_list;
#else
    UdpGwClient udpgw_client;
    BTimer reconnect_timer;
    int have_socks;
    BSocksClient socks_client;
    int socks_up;
#endif
    DebugObject d_obj;
} SocksUdpGwClient;

#ifdef ANDROID
typedef struct {
    BAddr local_addr;
    BAddr remote_addr;
} SocksUdpGwClient_conaddr;

typedef struct {
    SocksUdpGwClient *client;
    SocksUdpGwClient_conaddr conaddr;
    BPending first_job;
    const uint8_t *first_data;
    int first_data_len;
    BDatagram udp_dgram;
    BufferWriter udp_send_writer;
    PacketBuffer udp_send_buffer;
    SinglePacketBuffer udp_recv_buffer;
    PacketPassInterface udp_recv_if;
    BAVLNode connections_tree_node;
    LinkedList1Node connections_list_node;
} SocksUdpGwClient_connection;
#endif

int SocksUdpGwClient_Init (SocksUdpGwClient *o, int udp_mtu, int max_connections, int send_buffer_size, btime_t keepalive_time,
                           BAddr socks_server_addr, const struct BSocksClient_auth_info *auth_info, size_t num_auth_info,
                           BAddr remote_udpgw_addr, btime_t reconnect_time, BReactor *reactor, void *user,
                           SocksUdpGwClient_handler_received handler_received) WARN_UNUSED;
void SocksUdpGwClient_Free (SocksUdpGwClient *o);
void SocksUdpGwClient_SubmitPacket (SocksUdpGwClient *o, BAddr local_addr, BAddr remote_addr, int is_dns, const uint8_t *data, int data_len);

#endif

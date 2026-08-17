#include "NetPlay.h"
#include <android/log.h>
#include "smpc.h"
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <cstring>

#define FRAME001 1
#define FRAMEACK 2

#include "peripheral.h"

NetPlay::NetPlay() {
    client = NULL;
    peer = NULL;
    isHost = false;
    running = false;
    peerFrameCount = 0;
    frameCount = 0;
}

NetPlay::~NetPlay() {
    close();
}


void NetPlay::init(){
    enet_initialize();
}

void NetPlay::close(){

    running = false;
    sendThread.join();
    receiveThread.join();

    ENetEvent event;
    enet_peer_disconnect(peer, 0);
    while(enet_host_service(client, &event, 3000) > 0){
        switch(event.type){
            case ENET_EVENT_TYPE_RECEIVE:
                enet_packet_destroy(event.packet);
                break;
            case ENET_EVENT_TYPE_DISCONNECT:
                __android_log_print(ANDROID_LOG_INFO, "YabaSanshiro", "%s disconnected.", event.peer->data);
                event.peer->data = NULL;
        }
    }
}

void NetPlay::destroy(){
    enet_host_destroy(client);
    enet_deinitialize();
}

void NetPlayHost::serve(){
    ENetAddress address;
    address.host = ENET_HOST_ANY;
    address.port = 1234;

    client = enet_host_create(&address, 1, 2, 0, 0);
    if(client == NULL){
        __android_log_print(ANDROID_LOG_INFO, "YabaSanshiro", "An error occurred while trying to create an ENet server host.");
        return;
    }

    // Get list of network interfaces
    struct ifaddrs *ifaddr, *ifa;
    if (getifaddrs(&ifaddr) == -1) {
        __android_log_print(ANDROID_LOG_INFO, "YabaSanshiro", "Failed to get network interfaces");
        return;
    }

    // Print all available IPv4 addresses
    for (ifa = ifaddr; ifa != NULL; ifa = ifa->ifa_next) {
        if (ifa->ifa_addr == NULL || ifa->ifa_addr->sa_family != AF_INET)
            continue;

        char ip[INET_ADDRSTRLEN];
        struct sockaddr_in *addr = (struct sockaddr_in*)ifa->ifa_addr;
        inet_ntop(AF_INET, &addr->sin_addr, ip, INET_ADDRSTRLEN);
        
        // Skip loopback address
        if (strcmp(ip, "127.0.0.1") != 0) {
            __android_log_print(ANDROID_LOG_INFO, "YabaSanshiro", "Server available at %s:%u", ip, address.port);
        }
    }

    running = true;

    sendThread = std::thread([&]() {
        while (running) {
            if( peer != NULL ) {
                ENetPacket *packet = enet_packet_create(&PORTDATA1, sizeof(PortData_struct), ENET_PACKET_FLAG_UNSEQUENCED);
                enet_peer_send(peer, 0, packet);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });

    receiveThread = std::thread([&]() {
        ENetEvent event;
        while (running) {
            if (enet_host_service(client, &event, 0) > 0) {
                if (event.type == ENET_EVENT_TYPE_CONNECT ){
                    __android_log_print(ANDROID_LOG_INFO, "YabaSanshiro", "A new client connected from %x:%u.", event.peer->address.host, event.peer->address.port);
                    peer = event.peer;
                    peerFrameCount = 0;
                    frameCount = 0;
                } else if (event.type == ENET_EVENT_TYPE_RECEIVE ) {
                    if( event.channelID == 0 && event.packet->dataLength == sizeof(PortData_struct)) {
                        memcpy(&PORTDATA2, event.packet->data, sizeof(PortData_struct));
                    }else if ( event.channelID == 1 && event.packet->dataLength == sizeof(uint64_t)) {
                        memcpy(&peerFrameCount, event.packet->data, sizeof(uint64_t));
                    }
                    enet_packet_destroy(event.packet);
                }
            }
        }
    });


    freeifaddrs(ifaddr);
}

NetPlayHost::NetPlayHost() {
    isHost = true;
    waitTime = UINT_MAX;
}

void NetPlayHost::proc() {
    if (peer == nullptr) return;
    ENetPacket *packet = enet_packet_create(&frameCount, sizeof(uint64_t), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 1, packet);
    while( peerFrameCount < frameCount ){
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    frameCount++;
}

void NetPlayClient::connect( char * serverAddress ){
    ENetAddress address;
    client = enet_host_create(NULL, 1, 2, 0, 0);
    enet_address_set_host(&address, serverAddress);
    address.port = 1234;
    peer = enet_host_connect(client, &address, 2, 0);
    if(peer == NULL){
        __android_log_print(ANDROID_LOG_INFO, "YabaSanshiro", "No available peers for initiating an ENet connection.");
    }
    peerFrameCount = 0;
    frameCount = 0;

    running = true;

    sendThread = std::thread([&]() {
        while (running) {
            ENetPacket *packet = enet_packet_create(&PORTDATA2, sizeof(PortData_struct), ENET_PACKET_FLAG_UNSEQUENCED);
            enet_peer_send(peer, 0, packet);
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    });


    receiveThread = std::thread([&]() {
        ENetEvent event;
        while (running) {
            if (enet_host_service(client, &event, 0) > 0) {
                if (event.type == ENET_EVENT_TYPE_RECEIVE ) {
                    if( event.channelID == 0 && event.packet->dataLength == sizeof(PortData_struct)) {
                        memcpy(&PORTDATA1, event.packet->data, sizeof(PortData_struct));
                    }else if ( event.channelID == 1 && event.packet->dataLength == sizeof(uint64_t)) {
                        memcpy(&peerFrameCount, event.packet->data, sizeof(uint64_t));
                    }
                    enet_packet_destroy(event.packet);
                }
                
            }

        }
    });
}

NetPlayClient::NetPlayClient() {
    isHost = false;
}

void NetPlayClient::proc() {
    if (peer == nullptr) return;
    // Non-blocking check for host's frame start signal
    ENetPacket *packet = enet_packet_create(&frameCount, sizeof(uint64_t), ENET_PACKET_FLAG_RELIABLE);
    enet_peer_send(peer, 1, packet);
    while( peerFrameCount < frameCount ){
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }
    frameCount++;
}

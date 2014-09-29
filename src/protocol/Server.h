/*
    Network Protocol Foundation Library.
    Copyright (c) 2014, The Network Protocol Company, Inc.
*/

#ifndef PROTOCOL_SERVER_H
#define PROTOCOL_SERVER_H

#include "Packets.h"
#include "Connection.h"
#include "NetworkInterface.h"
#include "ClientServerContext.h"
#include "ClientServerDataBlock.h"

namespace protocol
{
    class Allocator;
    class NetworkSimulator;
    
    struct ServerConfig
    {
        Allocator * allocator = nullptr;                        // allocator used for allocations that match the life cycle of this object. if null then default allocator is used.

        int maxClients = 16;                                    // max number of clients supported by this server.

        float connectingSendRate = 10;                          // packets to send per-second while a client slot is connecting.
        float connectedSendRate = 30;                           // packets to send per-second once a client is connected.

        float connectingTimeOut = 5.0f;                         // timeout in seconds while a client is connecting
        float connectedTimeOut = 10.0f;                         // timeout in seconds once a client is connected

        NetworkInterface * networkInterface = nullptr;          // network interface used to send and receive packets
        ChannelStructure * channelStructure = nullptr;          // defines the connection channel structure
        
        Block * serverData = nullptr;                           // server data sent to clients on connect. must be constant. this block is not owned by us (we don't destroy it)
        int maxClientDataSize = 64 * 1024;                      // maximum size for data received from client on connect. if the server data is larger than this then the connect will fail.
        int fragmentSize = 1024;                                // send server data in 1k fragments by default. good size given that MTU is typically 1200 bytes.
        int fragmentsPerSecond = 60;                            // number of fragment packets to send per-second. set pretty high because we want the data to get across quickly.

        NetworkSimulator * networkSimulator = nullptr;          // optional network simulator.
    };

    class Server
    {
        struct ClientData
        {
            Address address;                                        // the client address that started this connection.
            double accumulator;                                     // accumulator used to determine when to send next packet.
            double lastPacketTime;                                  // time at which the last valid packet was received from the client. used for timeouts.
            uint16_t clientId;                                      // the client id generated by the client and sent to us via connect request.
            uint16_t serverId;                                      // the server id generated randomly on connection request unique to this client.
            ServerClientState state;                                // the current state of this client slot.
            bool readyForConnection;                                // set to true once the client is ready for a connection to start, eg. client has sent their client data across (if any)
            Connection * connection;                                // connection object. active in SERVER_CLIENT_STATE_CONNECTION.
            ClientServerDataBlockSender * dataBlockSender;          // data block sender. active while in SERVER_CLIENT_STATE_SENDING_SERVER_DATA.
            ClientServerDataBlockReceiver * dataBlockReceiver;      // data block receiver. active while in SERVER_CLIENT_STATE_SENDING_SERVER_DATA.

            ClientData()
            {
                connection = nullptr;
                dataBlockSender = nullptr;
                dataBlockReceiver = nullptr;
                Clear();
            }

            void Clear()
            {
                accumulator = 0;
                lastPacketTime = 0;
                clientId = 0;
                serverId = 0;
                state = SERVER_CLIENT_STATE_DISCONNECTED;
                readyForConnection = false;

                if ( dataBlockSender )
                    dataBlockSender->Clear();

                if ( dataBlockReceiver )
                    dataBlockReceiver->Clear();
            }
        };

        const ServerConfig m_config;

        Allocator * m_allocator;

        TimeBase m_timeBase;

        bool m_open = true;

        int m_numClients = 0;

        ClientData * m_clients = nullptr;

        PacketFactory * m_packetFactory = nullptr;                  // important: we don't own this pointer. it comes from the network interface

        ClientServerContext m_clientServerContext;

        const void * m_context[MaxContexts];

    public:

        Server( const ServerConfig & config );

        ~Server();

        void Open();

        void Close();

        bool IsOpen() const;

        void Update( const TimeBase & timeBase );

        void DisconnectClient( int clientIndex );

        ServerClientState GetClientState( int clientIndex ) const;

        Connection * GetClientConnection( int clientIndex );

        const Block * GetClientData( int clientIndex ) const;

        void SetContext( int index, const void * ptr );

        int FindClientSlot( const Address & address, uint64_t clientId, uint64_t serverId ) const;

        const ServerConfig & GetConfig() const { return m_config; }

        const TimeBase & GetTimeBase() const { return m_timeBase; }

    protected:

        void UpdateClients();

        void UpdateSendingChallenge( int clientIndex );

        void UpdateSendingServerData( int clientIndex );

        void UpdateReadyForConnection( int clientIndex );

        void UpdateConnected( int clientIndex );

        void UpdateTimeouts( int clientIndex );

        void UpdateNetworkSimulator();

        void UpdateNetworkInterface();

        void UpdateReceivePackets();

        void ProcessConnectionRequestPacket( ConnectionRequestPacket * packet );

        void ProcessChallengeResponsePacket( ChallengeResponsePacket * packet );

        void ProcessReadyForConnectionPacket( ReadyForConnectionPacket * packet );

        void ProcessDisconnectedPacket( DisconnectedPacket * packet );

        void ProcessDataBlockFragmentPacket( DataBlockFragmentPacket * packet );

        void ProcessDataBlockFragmentAckPacket( DataBlockFragmentAckPacket * packet );

        void ProcessConnectionPacket( ConnectionPacket * packet );

        int FindClientSlot( const Address & address ) const;

        int FindClientSlot( const Address & address, uint64_t clientId ) const;

        int FindFreeClientSlot() const;

        void ResetClientSlot( int clientIndex );

        void SendPacket( const Address & address, Packet * packet );

        void SetClientState( int clientIndex, ServerClientState state );

    protected:

        virtual void OnClientStateChange( int clientIndex, ServerClientState previous, ServerClientState current ) {}

        virtual void OnClientDataReceived( int clientIndex, const Block & block ) {}

        virtual void OnClientTimedOut( int clientIndex ) {}
    };
}

#endif
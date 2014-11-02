// Client Server Library - Copyright (c) 2014, The Network Protocol Company, Inc.

#ifndef CLIENT_SERVER_PACKETS_H
#define CLIENT_SERVER_PACKETS_H

#include "protocol/Packet.h"
#include "protocol/Stream.h"
#include "protocol/Channel.h"
#include "protocol/PacketFactory.h"
#include "protocol/ProtocolEnums.h"
#include "core/Memory.h"

namespace clientServer
{
    enum Packets
    { 
        CLIENT_SERVER_PACKET_CONNECTION = protocol::CONNECTION_PACKET,

        // client -> server

        CLIENT_SERVER_PACKET_CONNECTION_REQUEST,                // client is requesting a connection.
        CLIENT_SERVER_PACKET_CHALLENGE_RESPONSE,                // client response to server connection challenge.

        // server -> client

        CLIENT_SERVER_PACKET_CONNECTION_DENIED,                 // server denies request for connection. contains reason int, eg. full, closed etc.
        CLIENT_SERVER_PACKET_CONNECTION_CHALLENGE,              // server response to client connection request.

        // bidirectional

        CLIENT_SERVER_PACKET_READY_FOR_CONNECTION,              // client/server are ready for connection packets. when both are ready the connection is established.
        CLIENT_SERVER_PACKET_DATA_BLOCK_FRAGMENT,               // a fragment of a data block being sent down.
        CLIENT_SERVER_PACKET_DATA_BLOCK_FRAGMENT_ACK,           // ack for a received data block fragment.
        CLIENT_SERVER_PACKET_DISCONNECTED,                      // courtesy packet sent in both directions to indicate that the client slot has been disconnected

        NUM_CLIENT_SERVER_NUM_PACKETS
    };

    struct ConnectionRequestPacket : public protocol::Packet
    {
        uint16_t clientId = 0;

        ConnectionRequestPacket() : Packet( CLIENT_SERVER_PACKET_CONNECTION_REQUEST ) {}

        template <typename Stream> void Serialize( Stream & stream )
        {
            serialize_uint16( stream, clientId );
        }

        void SerializeRead( protocol::ReadStream & stream )
        {
            Serialize( stream );
        }

        void SerializeWrite( protocol::WriteStream & stream )
        {
            Serialize( stream );
        }
    
        void SerializeMeasure( protocol::MeasureStream & stream )
        {
            Serialize( stream );
        }
    };

    struct ChallengeResponsePacket : public protocol::Packet
    {
        uint16_t clientId = 0;
        uint16_t serverId = 0;

        ChallengeResponsePacket() : Packet( CLIENT_SERVER_PACKET_CHALLENGE_RESPONSE ) {}

        template <typename Stream> void Serialize( Stream & stream )
        {
            serialize_uint16( stream, clientId );
            serialize_uint16( stream, serverId );
        }

        void SerializeRead( protocol::ReadStream & stream )
        {
            Serialize( stream );
        }

        void SerializeWrite( protocol::WriteStream & stream )
        {
            Serialize( stream );
        }
    
        void SerializeMeasure( protocol::MeasureStream & stream )
        {
            Serialize( stream );
        }
    };

    struct ConnectionDeniedPacket : public protocol::Packet
    {
        uint16_t clientId = 0;
        uint32_t reason = 0;

        ConnectionDeniedPacket() : Packet( CLIENT_SERVER_PACKET_CONNECTION_DENIED ) {}

        template <typename Stream> void Serialize( Stream & stream )
        {
            serialize_uint16( stream, clientId );
            serialize_uint32( stream, reason );
        }

        void SerializeRead( protocol::ReadStream & stream )
        {
            Serialize( stream );
        }

        void SerializeWrite( protocol::WriteStream & stream )
        {
            Serialize( stream );
        }
    
        void SerializeMeasure( protocol::MeasureStream & stream )
        {
            Serialize( stream );
        }
    };

    struct ConnectionChallengePacket : public protocol::Packet
    {
        uint16_t clientId = 0;
        uint16_t serverId = 0;

        ConnectionChallengePacket() : Packet( CLIENT_SERVER_PACKET_CONNECTION_CHALLENGE ) {}

        template <typename Stream> void Serialize( Stream & stream )
        {
            serialize_uint16( stream, clientId );
            serialize_uint16( stream, serverId );
        }

        void SerializeRead( protocol::ReadStream & stream )
        {
            Serialize( stream );
        }

        void SerializeWrite( protocol::WriteStream & stream )
        {
            Serialize( stream );
        }
    
        void SerializeMeasure( protocol::MeasureStream & stream )
        {
            Serialize( stream );
        }
    };

    struct ReadyForConnectionPacket : public protocol::Packet
    {
        uint16_t clientId = 0;
        uint16_t serverId = 0;

        ReadyForConnectionPacket() : Packet( CLIENT_SERVER_PACKET_READY_FOR_CONNECTION ) {}

        template <typename Stream> void Serialize( Stream & stream )
        {
            serialize_uint16( stream, clientId );
            serialize_uint16( stream, serverId );
        }

        void SerializeRead( protocol::ReadStream & stream )
        {
            Serialize( stream );
        }

        void SerializeWrite( protocol::WriteStream & stream )
        {
            Serialize( stream );
        }
    
        void SerializeMeasure( protocol::MeasureStream & stream )
        {
            Serialize( stream );
        }
    };

    struct DataBlockFragmentPacket : public protocol::Packet
    {
        uint16_t clientId = 0;
        uint16_t serverId = 0;
        uint32_t blockSize = 0;
        uint32_t fragmentSize : 16;
        uint32_t numFragments : 16;
        uint32_t fragmentId : 16;
        uint32_t fragmentBytes : 16;
        uint8_t * fragmentData = nullptr;

        DataBlockFragmentPacket() : Packet( CLIENT_SERVER_PACKET_DATA_BLOCK_FRAGMENT ) 
        {
            fragmentId = 0;
            fragmentSize = 0;
        }

        ~DataBlockFragmentPacket()
        {
            if ( fragmentData )
            {
                core::memory::scratch_allocator().Free( fragmentData );
                fragmentData = nullptr;
            }
        }

        template <typename Stream> void Serialize( Stream & stream )
        {
            if ( Stream::IsWriting )
                CORE_ASSERT( fragmentSize <= protocol::MaxFragmentSize );

            serialize_uint16( stream, clientId );
            serialize_uint16( stream, serverId );
            serialize_uint32( stream, blockSize );
            serialize_bits( stream, numFragments, 16 );
            serialize_bits( stream, fragmentSize, 16 );
            serialize_bits( stream, fragmentId, 16 );
            serialize_bits( stream, fragmentBytes, 16 );        // actual fragment bytes included in this packed. may be *less* than fragment size!

            if ( Stream::IsReading )
            {
                CORE_ASSERT( fragmentSize <= protocol::MaxFragmentSize );
                if ( fragmentSize <= protocol::MaxFragmentSize )
                    fragmentData = (uint8_t*) core::memory::scratch_allocator().Allocate( fragmentBytes );
            }

            CORE_ASSERT( fragmentData );

            serialize_bytes( stream, fragmentData, fragmentBytes );
        }

        void SerializeRead( protocol::ReadStream & stream )
        {
            Serialize( stream );
        }

        void SerializeWrite( protocol::WriteStream & stream )
        {
            Serialize( stream );
        }
    
        void SerializeMeasure( protocol::MeasureStream & stream )
        {
            Serialize( stream );
        }
    };

    struct DataBlockFragmentAckPacket : public protocol::Packet
    {
        uint16_t clientId = 0;
        uint16_t serverId = 0;
        uint32_t fragmentId : 16;

        DataBlockFragmentAckPacket() : Packet( CLIENT_SERVER_PACKET_DATA_BLOCK_FRAGMENT_ACK ) 
        {
            fragmentId = 0;
        }

        template <typename Stream> void Serialize( Stream & stream )
        {
            serialize_uint16( stream, clientId );
            serialize_uint16( stream, serverId );
            serialize_bits( stream, fragmentId, 16 );
        }

        void SerializeRead( protocol::ReadStream & stream )
        {
            Serialize( stream );
        }

        void SerializeWrite( protocol::WriteStream & stream )
        {
            Serialize( stream );
        }
    
        void SerializeMeasure( protocol::MeasureStream & stream )
        {
            Serialize( stream );
        }
    };

    struct DisconnectedPacket : public protocol::Packet
    {
        uint16_t clientId = 0;
        uint16_t serverId = 0;

        DisconnectedPacket() : Packet( CLIENT_SERVER_PACKET_DISCONNECTED ) {}

        template <typename Stream> void Serialize( Stream & stream )
        {
            serialize_uint16( stream, clientId );
            serialize_uint16( stream, serverId );
        }

        void SerializeRead( protocol::ReadStream & stream )
        {
            Serialize( stream );
        }

        void SerializeWrite( protocol::WriteStream & stream )
        {
            Serialize( stream );
        }
    
        void SerializeMeasure( protocol::MeasureStream & stream )
        {
            Serialize( stream );
        }
    };
}

#endif

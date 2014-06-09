#include "BSDSockets.h"

using namespace std;
using namespace protocol;

enum PacketType
{
    PACKET_Connect = 1,
    PACKET_Update,
    PACKET_Disconnect
};

struct ConnectPacket : public Packet
{
    int a,b,c;

    ConnectPacket() : Packet( PACKET_Connect )
    {
        a = 1;
        b = 2;
        c = 3;        
    }

    void Serialize( Stream & stream )
    {
        serialize_int( stream, a, -10, 10 );
        serialize_int( stream, b, -10, 10 );
        serialize_int( stream, c, -10, 10 );
    }

    bool operator ==( const ConnectPacket & other ) const
    {
        return a == other.a && b == other.b && c == other.c;
    }

    bool operator !=( const ConnectPacket & other ) const
    {
        return !( *this == other );
    }
};

struct UpdatePacket : public Packet
{
    uint16_t timestamp;

    UpdatePacket() : Packet( PACKET_Update )
    {
        timestamp = 0;
    }

    void Serialize( Stream & stream )
    {
        serialize_bits( stream, timestamp, 16 );
    }

    bool operator ==( const UpdatePacket & other ) const
    {
        return timestamp == other.timestamp;
    }

    bool operator !=( const UpdatePacket & other ) const
    {
        return !( *this == other );
    }
};

struct DisconnectPacket : public Packet
{
    int x;

    DisconnectPacket() : Packet( PACKET_Disconnect ) 
    {
        x = 2;
    }

    void Serialize( Stream & stream )
    {
        serialize_int( stream, x, -100, +100 );
    }

    bool operator ==( const DisconnectPacket & other ) const
    {
        return x == other.x;
    }

    bool operator !=( const DisconnectPacket & other ) const
    {
        return !( *this == other );
    }
};

class PacketFactory : public Factory<Packet>
{
public:
    PacketFactory()
    {
        Register( PACKET_Connect, [] { return make_shared<ConnectPacket>(); } );
        Register( PACKET_Update, [] { return make_shared<UpdatePacket>(); } );
        Register( PACKET_Disconnect, [] { return make_shared<DisconnectPacket>(); } );
    }
};

void test_bsd_sockets_send_and_receive_ipv4()
{
    cout << "test_bsd_sockets_send_and_receive_ipv4" << endl;

    BSDSocketsConfig config;

    auto packetFactory = make_shared<PacketFactory>();

    config.port = 10000;
    config.family = AF_INET;
    config.maxPacketSize = 1024;
    config.packetFactory = static_pointer_cast<Factory<Packet>>( packetFactory );

    BSDSockets interface( config );

    Address address( "127.0.0.1" );
    address.SetPort( config.port );

    TimeBase timeBase;
    timeBase.deltaTime = 0.01f;

    chrono::milliseconds ms( (int) ( timeBase.deltaTime * 1000 ) );

    bool receivedConnectPacket = false;
    bool receivedUpdatePacket = false;
    bool receivedDisconnectPacket = false;

    int iterations = 0;

    while ( true )
    {
        assert ( iterations++ < 10 );

        auto connectPacket = make_shared<ConnectPacket>();
        auto updatePacket = make_shared<UpdatePacket>();
        auto disconnectPacket = make_shared<DisconnectPacket>();

        connectPacket->a = 2;
        connectPacket->b = 6;
        connectPacket->c = -1;

        updatePacket->timestamp = 500;

        disconnectPacket->x = -100;

        interface.SendPacket( address, connectPacket );
        interface.SendPacket( address, updatePacket );
        interface.SendPacket( address, disconnectPacket );

        interface.Update( timeBase );

        this_thread::sleep_for( ms );

        while ( true )
        {
            auto packet = interface.ReceivePacket();
            if ( !packet )
                break;

            assert( packet->GetAddress() == address );

            switch ( packet->GetType() )
            {
                case PACKET_Connect:
                {
                    cout << "received connect packet" << endl;
                    auto recv_connectPacket = static_pointer_cast<ConnectPacket>( packet );
                    assert( *recv_connectPacket == *connectPacket );
                    receivedConnectPacket = true;
                }
                break;

                case PACKET_Update:
                {
                    cout << "received update packet" << endl;
                    auto recv_updatePacket = static_pointer_cast<UpdatePacket>( packet );
                    assert( *recv_updatePacket == *updatePacket );
                    receivedUpdatePacket = true;
                }
                break;

                case PACKET_Disconnect:
                {
                    cout << "received disconnect packet" << endl;
                    auto recv_disconnectPacket = static_pointer_cast<DisconnectPacket>( packet );
                    assert( *recv_disconnectPacket == *disconnectPacket );
                    receivedDisconnectPacket = true;
                }
                break;
            }
        }

        if ( receivedConnectPacket && receivedUpdatePacket && receivedDisconnectPacket )
            break;

        timeBase.time += timeBase.deltaTime;
    }
}

void test_bsd_sockets_send_and_receive_ipv6()
{
    cout << "test_bsd_sockets_send_and_receive_ipv6" << endl;

    BSDSocketsConfig config;

    auto packetFactory = make_shared<PacketFactory>();

    config.port = 10000;
    config.family = AF_INET6;
    config.maxPacketSize = 1024;
    config.packetFactory = static_pointer_cast<Factory<Packet>>( packetFactory );

    BSDSockets interface( config );

    Address address( "::1" );
    address.SetPort( config.port );

    TimeBase timeBase;
    timeBase.deltaTime = 0.01f;

    chrono::milliseconds ms( (int) ( timeBase.deltaTime * 1000 ) );

    bool receivedConnectPacket = false;
    bool receivedUpdatePacket = false;
    bool receivedDisconnectPacket = false;

    int iterations = 0;

    while ( true )
    {
        assert ( iterations++ < 10 );

        auto connectPacket = make_shared<ConnectPacket>();
        auto updatePacket = make_shared<UpdatePacket>();
        auto disconnectPacket = make_shared<DisconnectPacket>();

        connectPacket->a = 2;
        connectPacket->b = 6;
        connectPacket->c = -1;

        updatePacket->timestamp = 500;

        disconnectPacket->x = -100;

        interface.SendPacket( address, connectPacket );
        interface.SendPacket( address, updatePacket );
        interface.SendPacket( address, disconnectPacket );

        interface.Update( timeBase );

        this_thread::sleep_for( ms );

        while ( true )
        {
            auto packet = interface.ReceivePacket();
            if ( !packet )
                break;

            cout << "receive packet from address " << packet->GetAddress().ToString() << endl;

            assert( packet->GetAddress() == address );

            switch ( packet->GetType() )
            {
                case PACKET_Connect:
                {
                    cout << "received connect packet" << endl;
                    auto recv_connectPacket = static_pointer_cast<ConnectPacket>( packet );
                    assert( *recv_connectPacket == *connectPacket );
                    receivedConnectPacket = true;
                }
                break;

                case PACKET_Update:
                {
                    cout << "received update packet" << endl;
                    auto recv_updatePacket = static_pointer_cast<UpdatePacket>( packet );
                    assert( *recv_updatePacket == *updatePacket );
                    receivedUpdatePacket = true;
                }
                break;

                case PACKET_Disconnect:
                {
                    cout << "received disconnect packet" << endl;
                    auto recv_disconnectPacket = static_pointer_cast<DisconnectPacket>( packet );
                    assert( *recv_disconnectPacket == *disconnectPacket );
                    receivedDisconnectPacket = true;
                }
                break;
            }
        }

        if ( receivedConnectPacket && receivedUpdatePacket && receivedDisconnectPacket )
            break;

        timeBase.time += timeBase.deltaTime;
    }
}

void test_bsd_sockets_send_and_receive_multiple_interfaces_ipv4()
{
    cout << "test_bsd_sockets_send_and_receive_multiple_interfaces_ipv4" << endl;

    auto packetFactory = make_shared<PacketFactory>();

    BSDSocketsConfig sender_config;
    sender_config.port = 10000;
    sender_config.family = AF_INET;
    sender_config.maxPacketSize = 1024;
    sender_config.packetFactory = static_pointer_cast<Factory<Packet>>( packetFactory );

    BSDSockets interface_sender( sender_config );
    
    BSDSocketsConfig receiver_config;
    receiver_config.port = 10001;
    receiver_config.family = AF_INET;
    receiver_config.maxPacketSize = 1024;
    receiver_config.packetFactory = static_pointer_cast<Factory<Packet>>( packetFactory );

    BSDSockets interface_receiver( receiver_config );

    Address sender_address( "[127.0.0.1]:10000" );
    Address receiver_address( "[127.0.0.1]:10001" );

    TimeBase timeBase;
    timeBase.deltaTime = 0.01f;

    chrono::milliseconds ms( (int) ( timeBase.deltaTime * 1000 ) );

    bool receivedConnectPacket = false;
    bool receivedUpdatePacket = false;
    bool receivedDisconnectPacket = false;

    int iterations = 0;

    while ( true )
    {
        assert ( iterations++ < 4 );

        auto connectPacket = make_shared<ConnectPacket>();
        auto updatePacket = make_shared<UpdatePacket>();
        auto disconnectPacket = make_shared<DisconnectPacket>();

        connectPacket->a = 2;
        connectPacket->b = 6;
        connectPacket->c = -1;

        updatePacket->timestamp = 500;

        disconnectPacket->x = -100;

        interface_sender.SendPacket( receiver_address, connectPacket );
        interface_sender.SendPacket( receiver_address, updatePacket );
        interface_sender.SendPacket( receiver_address, disconnectPacket );

        interface_sender.Update( timeBase );
        interface_receiver.Update( timeBase );

        this_thread::sleep_for( ms );

        while ( true )
        {
            auto packet = interface_receiver.ReceivePacket();
            if ( !packet )
                break;

            cout << "receive packet from address " << packet->GetAddress().ToString() << endl;

            assert( packet->GetAddress() == sender_address );

            switch ( packet->GetType() )
            {
                case PACKET_Connect:
                {
                    cout << "received connect packet" << endl;
                    auto recv_connectPacket = static_pointer_cast<ConnectPacket>( packet );
                    assert( *recv_connectPacket == *connectPacket );
                    receivedConnectPacket = true;
                }
                break;

                case PACKET_Update:
                {
                    cout << "received update packet" << endl;
                    auto recv_updatePacket = static_pointer_cast<UpdatePacket>( packet );
                    assert( *recv_updatePacket == *updatePacket );
                    receivedUpdatePacket = true;
                }
                break;

                case PACKET_Disconnect:
                {
                    cout << "received disconnect packet" << endl;
                    auto recv_disconnectPacket = static_pointer_cast<DisconnectPacket>( packet );
                    assert( *recv_disconnectPacket == *disconnectPacket );
                    receivedDisconnectPacket = true;
                }
                break;
            }
        }

        if ( receivedConnectPacket && receivedUpdatePacket && receivedDisconnectPacket )
            break;

        timeBase.time += timeBase.deltaTime;
    }
}

void test_bsd_sockets_send_and_receive_multiple_interfaces_ipv6()
{
    cout << "test_bsd_sockets_send_and_receive_multiple_interfaces_ipv6" << endl;


    auto packetFactory = make_shared<PacketFactory>();

    BSDSocketsConfig sender_config;
    sender_config.port = 10000;
    sender_config.family = AF_INET6;
    sender_config.maxPacketSize = 1024;
    sender_config.packetFactory = static_pointer_cast<Factory<Packet>>( packetFactory );

    BSDSockets interface_sender( sender_config );
    
    BSDSocketsConfig receiver_config;
    receiver_config.port = 10001;
    receiver_config.family = AF_INET6;
    receiver_config.maxPacketSize = 1024;
    receiver_config.packetFactory = static_pointer_cast<Factory<Packet>>( packetFactory );

    BSDSockets interface_receiver( receiver_config );

    Address sender_address( "[::1]:10000" );
    Address receiver_address( "[::1]:10001" );

    TimeBase timeBase;
    timeBase.deltaTime = 0.01f;

    chrono::milliseconds ms( (int) ( timeBase.deltaTime * 1000 ) );

    bool receivedConnectPacket = false;
    bool receivedUpdatePacket = false;
    bool receivedDisconnectPacket = false;

    int iterations = 0;

    while ( true )
    {
        assert ( iterations++ < 4 );

        auto connectPacket = make_shared<ConnectPacket>();
        auto updatePacket = make_shared<UpdatePacket>();
        auto disconnectPacket = make_shared<DisconnectPacket>();

        connectPacket->a = 2;
        connectPacket->b = 6;
        connectPacket->c = -1;

        updatePacket->timestamp = 500;

        disconnectPacket->x = -100;

        interface_sender.SendPacket( receiver_address, connectPacket );
        interface_sender.SendPacket( receiver_address, updatePacket );
        interface_sender.SendPacket( receiver_address, disconnectPacket );

        interface_sender.Update( timeBase );
        interface_receiver.Update( timeBase );

        this_thread::sleep_for( ms );

        while ( true )
        {
            auto packet = interface_receiver.ReceivePacket();
            if ( !packet )
                break;

            cout << "receive packet from address " << packet->GetAddress().ToString() << endl;

            assert( packet->GetAddress() == sender_address );

            switch ( packet->GetType() )
            {
                case PACKET_Connect:
                {
                    cout << "received connect packet" << endl;
                    auto recv_connectPacket = static_pointer_cast<ConnectPacket>( packet );
                    assert( *recv_connectPacket == *connectPacket );
                    receivedConnectPacket = true;
                }
                break;

                case PACKET_Update:
                {
                    cout << "received update packet" << endl;
                    auto recv_updatePacket = static_pointer_cast<UpdatePacket>( packet );
                    assert( *recv_updatePacket == *updatePacket );
                    receivedUpdatePacket = true;
                }
                break;

                case PACKET_Disconnect:
                {
                    cout << "received disconnect packet" << endl;
                    auto recv_disconnectPacket = static_pointer_cast<DisconnectPacket>( packet );
                    assert( *recv_disconnectPacket == *disconnectPacket );
                    receivedDisconnectPacket = true;
                }
                break;
            }
        }

        if ( receivedConnectPacket && receivedUpdatePacket && receivedDisconnectPacket )
            break;

        timeBase.time += timeBase.deltaTime;
    }
}

int main()
{
    srand( time( NULL ) );

    if ( !InitializeSockets() )
    {
        cerr << "failed to initialize sockets" << endl;
        return 1;
    }

    try
    {
        test_bsd_sockets_send_and_receive_ipv4();
        test_bsd_sockets_send_and_receive_ipv6();
        test_bsd_sockets_send_and_receive_multiple_interfaces_ipv4();
        test_bsd_sockets_send_and_receive_multiple_interfaces_ipv6();
    }
    catch ( runtime_error & e )
    {
        cerr << string( "error: " ) + e.what() << endl;
    }

    ShutdownSockets();

    return 0;
}
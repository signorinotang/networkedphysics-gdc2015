// Network Library - Copyright (c) 2014, The Network Protocol Company, Inc.

#ifndef NETWORK_SIMULATOR_H
#define NETWORK_SIMULATOR_H

// todo: probably rename this to Simulator.h

#include "core/Common.h"
#include "network/NetworkInterface.h"

namespace core { class Allocator; }

namespace network
{
    class PacketFactory;

    struct SimulatorConfig
    {
        Allocator * allocator;
        PacketFactory * packetFactory;
        int stateChance;                    // 1 in n chance to change state per-update
        int numPackets;                     // number of packets to buffer

        SimulatorConfig()
        {   
            allocator = nullptr;            // allocator used for allocations with the same life cycle as this object.
            packetFactory = nullptr;        // packet factory. must be specified -- we need it to destroy buffered packets in destructor.
            stateChance = 1000;             // 1 in every 1000 chance per-update by default
            numPackets = 1024;              // buffer up to 1024 packets by default
        }
    };

    struct SimulatorState
    {
        float latency;                      // amount of latency in seconds
        float jitter;                       // amount of jitter +/- in seconds
        float packetLoss;                   // packet loss (%)

        SimulatorState()
        {
            latency = 0.0f;
            jitter = 0.0f;
            packetLoss = 0.0f;
        }

        SimulatorState( float _latency,
                        float _jitter,
                        float _packetLoss )
        {
            latency = _latency;
            jitter = _jitter;
            packetLoss = _packetLoss;
        }
    };

    class Simulator : public Interface
    {
    public:

        Simulator( const SimulatorConfig & config = SimulatorConfig() );

        ~Simulator();

        void AddState( const SimulatorState & state );

        void SendPacket( const Address & address, Packet * packet );

        Packet * ReceivePacket();

        void Update( const TimeBase & timeBase );

        // todo: this stuff simply doesn't belong here
        uint32_t GetMaxPacketSize() const;
        PacketFactory & GetPacketFactory() const;
        void SetContext( const void ** context ) {}        // not needed, we don't actually serialize the packets

    private:

        struct PacketData
        {
            Packet * packet = nullptr;
            double dequeueTime = 0.0;
            uint32_t packetNumber = 0;
        };

        const SimulatorConfig m_config;

        Allocator * m_allocator;

        TimeBase m_timeBase;
        uint32_t m_packetNumber;

        PacketData * m_packets;

        int m_numStates;
        SimulatorState m_state;
        SimulatorState m_states[MaxSimulatorStates];

        Simulator( const Simulator & other );
        const Simulator & operator = ( const Simulator & other );
    };
}

#endif
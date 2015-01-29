#include "DeltaDemo.h"

#ifdef CLIENT

#include "Cubes.h"
#include "Global.h"
#include "Snapshot.h"
#include "Font.h"
#include "FontManager.h"
#include "protocol/Stream.h"
#include "protocol/SlidingWindow.h"
#include "protocol/SequenceBuffer.h"
#include "protocol/PacketFactory.h"
#include "network/Simulator.h"

static const int LeftPort = 1000;
static const int RightPort = 1001;
static const int MaxSnapshots = 256;
static const int QuantizedPositionBoundXY = UnitsPerMeter * PositionBoundXY;
static const int QuantizedPositionBoundZ = UnitsPerMeter * PositionBoundZ;

enum Context
{
    CONTEXT_QUANTIZED_SNAPSHOT_SLIDING_WINDOW,      // quantized send snapshots (for serialize write)
    CONTEXT_QUANTIZED_SNAPSHOT_SEQUENCE_BUFFER,     // quantized recv snapshots (for serialize read)
    CONTEXT_QUANTIZED_INITIAL_SNAPSHOT              // quantized initial snapshot
};

enum DeltaMode
{
    DELTA_MODE_NOT_CHANGED,
    DELTA_MODE_CHANGED_INDEX,
    DELTA_MODE_RELATIVE_INDEX,
    DELTA_MODE_RELATIVE_POSITION,
    DELTA_MODE_RELATIVE_ORIENTATION,
    DELTA_NUM_MODES
};

const char * delta_mode_descriptions[]
{
    "Not changed",
    "Changed index",
    "Relative index",
    "Relative position",
    "Relative orientation",
};

struct DeltaModeData : public SnapshotModeData
{
    DeltaModeData()
    {
        playout_delay = 0.067f;
        send_rate = 60.0f;
        latency = 0.05f;      // 100ms round trip -- IMPORTANT! Otherwise delta compression is too easy!
        packet_loss = 5.0f;
        jitter = 1.0 / 60.0f;
        interpolation = SNAPSHOT_INTERPOLATION_LINEAR;
    }
};

static DeltaModeData delta_mode_data[DELTA_NUM_MODES];

static void InitDeltaModes()
{
    // ...
}

typedef protocol::SlidingWindow<QuantizedSnapshot> QuantizedSnapshotSlidingWindow;
typedef protocol::SequenceBuffer<QuantizedSnapshot> QuantizedSnapshotSequenceBuffer;

enum DeltaPackets
{
    DELTA_SNAPSHOT_PACKET,
    DELTA_ACK_PACKET,
    DELTA_NUM_PACKETS
};

template <typename Stream> void serialize_cube_relative_position( Stream & stream, QuantizedCubeState & cube, const QuantizedCubeState & base )
{
    serialize_bool( stream, cube.interacting );

    bool relative_position;

    const int RelativePositionBound = 1023;

    if ( Stream::IsWriting )
    {
        relative_position = abs( cube.position_x - base.position_x ) <= RelativePositionBound &&
                            abs( cube.position_y - base.position_y ) <= RelativePositionBound &&
                            abs( cube.position_z - base.position_z ) <= RelativePositionBound;
    }

    serialize_bool( stream, relative_position );

    if ( relative_position )
    {
        int offset_x, offset_y, offset_z;

        if ( Stream::IsWriting )
        {
            offset_x = cube.position_x - base.position_x;
            offset_y = cube.position_y - base.position_y;
            offset_z = cube.position_z - base.position_z;
        }

        serialize_int( stream, offset_x, -RelativePositionBound, +RelativePositionBound );
        serialize_int( stream, offset_y, -RelativePositionBound, +RelativePositionBound );
        serialize_int( stream, offset_z, -RelativePositionBound, +RelativePositionBound );

        cube.position_x = base.position_x + offset_x;
        cube.position_y = base.position_y + offset_y;
        cube.position_z = base.position_z + offset_z;
    }
    else
    {
        serialize_int( stream, cube.position_x, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
        serialize_int( stream, cube.position_y, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
        serialize_int( stream, cube.position_z, 0, +QuantizedPositionBoundZ );

        if ( Stream::IsReading )
            cube.interacting = false;
    }

    serialize_object( stream, cube.orientation );
}

/*
template <typename Stream> void serialize_cube_relative_position( Stream & stream, QuantizedCubeState & cube, const QuantizedCubeState & base )
{
    serialize_bool( stream, cube.interacting );

    bool relative_position_a;
    bool relative_position_b;

    const int RelativePositionBoundA = 63;
    const int RelativePositionBoundB = 511;

    if ( Stream::IsWriting )
    {
        relative_position_a = abs( cube.position_x - base.position_x ) <= RelativePositionBoundA &&
                              abs( cube.position_y - base.position_y ) <= RelativePositionBoundA &&
                              abs( cube.position_z - base.position_z ) <= RelativePositionBoundA;

        relative_position_b = abs( cube.position_x - base.position_x ) <= RelativePositionBoundB &&
                              abs( cube.position_y - base.position_y ) <= RelativePositionBoundB &&
                              abs( cube.position_z - base.position_z ) <= RelativePositionBoundB;
    }

    serialize_bool( stream, relative_position_a );

    if ( relative_position_a )
    {
        int offset_x, offset_y, offset_z;

        if ( Stream::IsWriting )
        {
            offset_x = cube.position_x - base.position_x;
            offset_y = cube.position_y - base.position_y;
            offset_z = cube.position_z - base.position_z;
        }

        serialize_int( stream, offset_x, -RelativePositionBoundA, +RelativePositionBoundA );
        serialize_int( stream, offset_y, -RelativePositionBoundA, +RelativePositionBoundA );
        serialize_int( stream, offset_z, -RelativePositionBoundA, +RelativePositionBoundA );

        cube.position_x = base.position_x + offset_x;
        cube.position_y = base.position_y + offset_y;
        cube.position_z = base.position_z + offset_z;
    }
    else
    {
        serialize_bool( stream, relative_position_b );

        if ( relative_position_b )
        {
            int offset_x, offset_y, offset_z;

            if ( Stream::IsWriting )
            {
                offset_x = cube.position_x - base.position_x;
                offset_y = cube.position_y - base.position_y;
                offset_z = cube.position_z - base.position_z;
            }

            serialize_int( stream, offset_x, -RelativePositionBoundB, +RelativePositionBoundB );
            serialize_int( stream, offset_y, -RelativePositionBoundB, +RelativePositionBoundB );
            serialize_int( stream, offset_z, -RelativePositionBoundB, +RelativePositionBoundB );

            cube.position_x = base.position_x + offset_x;
            cube.position_y = base.position_y + offset_y;
            cube.position_z = base.position_z + offset_z;
        }
        else
        {
            serialize_int( stream, cube.position_x, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
            serialize_int( stream, cube.position_y, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
            serialize_int( stream, cube.position_z, 0, +QuantizedPositionBoundZ );

            if ( Stream::IsReading )
                cube.interacting = false;
        }
    }

    serialize_object( stream, cube.orientation );
}
*/

template <typename Stream> void serialize_cube_relative_orientation( Stream & stream, QuantizedCubeState & cube, const QuantizedCubeState & base )
{
    serialize_bool( stream, cube.interacting );

    bool relative_position;

    const int RelativePositionBound = 1023;

    if ( Stream::IsWriting )
    {
        relative_position = abs( cube.position_x - base.position_x ) <= RelativePositionBound &&
                            abs( cube.position_y - base.position_y ) <= RelativePositionBound &&
                            abs( cube.position_z - base.position_z ) <= RelativePositionBound;
    }

    serialize_bool( stream, relative_position );

    if ( relative_position )
    {
        int offset_x, offset_y, offset_z;

        if ( Stream::IsWriting )
        {
            offset_x = cube.position_x - base.position_x;
            offset_y = cube.position_y - base.position_y;
            offset_z = cube.position_z - base.position_z;
        }

        serialize_int( stream, offset_x, -RelativePositionBound, +RelativePositionBound );
        serialize_int( stream, offset_y, -RelativePositionBound, +RelativePositionBound );
        serialize_int( stream, offset_z, -RelativePositionBound, +RelativePositionBound );

        cube.position_x = base.position_x + offset_x;
        cube.position_y = base.position_y + offset_y;
        cube.position_z = base.position_z + offset_z;
    }
    else
    {
        serialize_int( stream, cube.position_x, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
        serialize_int( stream, cube.position_y, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
        serialize_int( stream, cube.position_z, 0, +QuantizedPositionBoundZ );

        if ( Stream::IsReading )
            cube.interacting = false;
    }

    /*
    const int RelativeOrientationThreshold = 64;
    
    bool relative_orientation = false;
    int delta_a, delta_b, delta_c;

    if ( Stream::IsWriting && cube.orientation.largest == base.orientation.largest )
    {
        delta_a = cube.orientation.integer_a - base.orientation.integer_a;
        delta_b = cube.orientation.integer_b - base.orientation.integer_b;
        delta_c = cube.orientation.integer_c - base.orientation.integer_c;

        if ( delta_a >= -RelativeOrientationThreshold && delta_a < RelativeOrientationThreshold &&
             delta_b >= -RelativeOrientationThreshold && delta_b < RelativeOrientationThreshold &&
             delta_c >= -RelativeOrientationThreshold && delta_c < RelativeOrientationThreshold )
        {
            relative_orientation = true;
        }
    }

    serialize_bool( stream, relative_orientation );

    if ( relative_orientation )
    {
        serialize_int( stream, delta_a, -RelativeOrientationThreshold, RelativeOrientationThreshold - 1 );
        serialize_int( stream, delta_b, -RelativeOrientationThreshold, RelativeOrientationThreshold - 1 );
        serialize_int( stream, delta_c, -RelativeOrientationThreshold, RelativeOrientationThreshold - 1 );

        cube.orientation = base.orientation;

        cube.orientation.integer_a += delta_a;
        cube.orientation.integer_b += delta_b;
        cube.orientation.integer_c += delta_c;
    }
    else
    {
        serialize_object( stream, cube.orientation );

        if ( Stream::IsReading )
            cube.interacting = false;
    }
    */

    serialize_object( stream, cube.orientation );
}

/*
template <typename Stream> void serialize_cube_relative_orientation( Stream & stream, QuantizedCubeState & cube, const QuantizedCubeState & base )
{
    serialize_bool( stream, cube.interacting );

    bool relative_position_a;
    bool relative_position_b;

    const int RelativePositionBoundA = 63;
    const int RelativePositionBoundB = 511;

    if ( Stream::IsWriting )
    {
        relative_position_a = abs( cube.position_x - base.position_x ) <= RelativePositionBoundA &&
                              abs( cube.position_y - base.position_y ) <= RelativePositionBoundA &&
                              abs( cube.position_z - base.position_z ) <= RelativePositionBoundA;

        relative_position_b = abs( cube.position_x - base.position_x ) <= RelativePositionBoundB &&
                              abs( cube.position_y - base.position_y ) <= RelativePositionBoundB &&
                              abs( cube.position_z - base.position_z ) <= RelativePositionBoundB;
    }

    serialize_bool( stream, relative_position_a );

    if ( relative_position_a )
    {
        int offset_x, offset_y, offset_z;

        if ( Stream::IsWriting )
        {
            offset_x = cube.position_x - base.position_x;
            offset_y = cube.position_y - base.position_y;
            offset_z = cube.position_z - base.position_z;
        }

        serialize_int( stream, offset_x, -RelativePositionBoundA, +RelativePositionBoundA );
        serialize_int( stream, offset_y, -RelativePositionBoundA, +RelativePositionBoundA );
        serialize_int( stream, offset_z, -RelativePositionBoundA, +RelativePositionBoundA );

        cube.position_x = base.position_x + offset_x;
        cube.position_y = base.position_y + offset_y;
        cube.position_z = base.position_z + offset_z;
    }
    else
    {
        serialize_bool( stream, relative_position_b );

        if ( relative_position_b )
        {
            int offset_x, offset_y, offset_z;

            if ( Stream::IsWriting )
            {
                offset_x = cube.position_x - base.position_x;
                offset_y = cube.position_y - base.position_y;
                offset_z = cube.position_z - base.position_z;
            }

            serialize_int( stream, offset_x, -RelativePositionBoundB, +RelativePositionBoundB );
            serialize_int( stream, offset_y, -RelativePositionBoundB, +RelativePositionBoundB );
            serialize_int( stream, offset_z, -RelativePositionBoundB, +RelativePositionBoundB );

            cube.position_x = base.position_x + offset_x;
            cube.position_y = base.position_y + offset_y;
            cube.position_z = base.position_z + offset_z;
        }
        else
        {
            serialize_int( stream, cube.position_x, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
            serialize_int( stream, cube.position_y, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
            serialize_int( stream, cube.position_z, 0, +QuantizedPositionBoundZ );

            if ( Stream::IsReading )
                cube.interacting = false;
        }
    }

    const int RelativeOrientationThreshold_Large = 64;
    const int RelativeOrientationThreshold_Small = 16;

    bool relative_orientation = false;
    bool small = false;
    int delta_a, delta_b, delta_c;

    if ( Stream::IsWriting && cube.orientation.largest == base.orientation.largest )
    {
        delta_a = cube.orientation.integer_a - base.orientation.integer_a;
        delta_b = cube.orientation.integer_b - base.orientation.integer_b;
        delta_c = cube.orientation.integer_c - base.orientation.integer_c;

        if ( delta_a >= -RelativeOrientationThreshold_Large && delta_a < RelativeOrientationThreshold_Large &&
             delta_b >= -RelativeOrientationThreshold_Large && delta_b < RelativeOrientationThreshold_Large &&
             delta_c >= -RelativeOrientationThreshold_Large && delta_c < RelativeOrientationThreshold_Large )
        {
            relative_orientation = true;

            const bool small_a = delta_a >= -RelativeOrientationThreshold_Small && delta_a < RelativeOrientationThreshold_Small;
            const bool small_b = delta_b >= -RelativeOrientationThreshold_Small && delta_b < RelativeOrientationThreshold_Small;
            const bool small_c = delta_c >= -RelativeOrientationThreshold_Small && delta_c < RelativeOrientationThreshold_Small;

            small = small_a && small_b && small_c;
        }
    }

    serialize_bool( stream, relative_orientation );

    if ( relative_orientation )
    {
        serialize_bool( stream, small );

        if ( small )
        {
            serialize_int( stream, delta_a, -RelativeOrientationThreshold_Small, RelativeOrientationThreshold_Small - 1 );
            serialize_int( stream, delta_b, -RelativeOrientationThreshold_Small, RelativeOrientationThreshold_Small - 1 );
            serialize_int( stream, delta_c, -RelativeOrientationThreshold_Small, RelativeOrientationThreshold_Small - 1 );
        }
        else
        {
            serialize_int( stream, delta_a, -RelativeOrientationThreshold_Large, RelativeOrientationThreshold_Large - 1 );
            serialize_int( stream, delta_b, -RelativeOrientationThreshold_Large, RelativeOrientationThreshold_Large - 1 );
            serialize_int( stream, delta_c, -RelativeOrientationThreshold_Large, RelativeOrientationThreshold_Large - 1 );
        }

        cube.orientation = base.orientation;

        cube.orientation.integer_a += delta_a;
        cube.orientation.integer_b += delta_b;
        cube.orientation.integer_c += delta_c;
    }
    else
    {
        serialize_object( stream, cube.orientation );

        if ( Stream::IsReading )
            cube.interacting = false;
    }
}
*/

struct DeltaSnapshotPacket : public protocol::Packet
{
    uint16_t sequence;
    uint16_t base_sequence;
    bool initial;
    int delta_mode;

    DeltaSnapshotPacket() : Packet( DELTA_SNAPSHOT_PACKET )
    {
        sequence = 0;
        delta_mode = DELTA_MODE_NOT_CHANGED;
    }

    PROTOCOL_SERIALIZE_OBJECT( stream )
    {
        auto quantized_snapshot_sliding_window = (QuantizedSnapshotSlidingWindow*) stream.GetContext( CONTEXT_QUANTIZED_SNAPSHOT_SLIDING_WINDOW );
        auto quantized_snapshot_sequence_buffer = (QuantizedSnapshotSequenceBuffer*) stream.GetContext( CONTEXT_QUANTIZED_SNAPSHOT_SEQUENCE_BUFFER );
        auto quantized_initial_snapshot = (QuantizedSnapshot*) stream.GetContext( CONTEXT_QUANTIZED_INITIAL_SNAPSHOT );

        serialize_uint16( stream, sequence );

        serialize_int( stream, delta_mode, 0, DELTA_NUM_MODES - 1 );

        serialize_bool( stream, initial );

        if ( !initial )
            serialize_uint16( stream, base_sequence );

        QuantizedCubeState * quantized_cubes = nullptr;

        if ( Stream::IsWriting )
        {
            CORE_ASSERT( quantized_snapshot_sliding_window );
            auto & entry = quantized_snapshot_sliding_window->Get( sequence );
            quantized_cubes = (QuantizedCubeState*) &entry.cubes[0];
        }
        else
        {
            CORE_ASSERT( quantized_snapshot_sequence_buffer );
            auto entry = quantized_snapshot_sequence_buffer->Insert( sequence );
            CORE_ASSERT( entry );
            quantized_cubes = (QuantizedCubeState*) &entry->cubes[0];
        }
        CORE_ASSERT( quantized_cubes );

        switch ( delta_mode )
        {
            case DELTA_MODE_NOT_CHANGED:
            {
                CORE_ASSERT( quantized_initial_snapshot );

                QuantizedCubeState * quantized_base_cubes = nullptr;

//                if ( Stream::IsWriting )
//                    printf( "encoding snapshot %d relative to baseline %d\n", sequence, base_sequence );

                if ( initial )
                {
                    quantized_base_cubes = quantized_initial_snapshot->cubes;
                }
                else
                {
                    if ( Stream::IsWriting )
                    {
                        CORE_ASSERT( quantized_snapshot_sliding_window );
                        auto & entry = quantized_snapshot_sliding_window->Get( base_sequence );
                        quantized_base_cubes = (QuantizedCubeState*) &entry.cubes[0];
                    }
                    else
                    {
                        CORE_ASSERT( quantized_snapshot_sequence_buffer );
                        auto entry = quantized_snapshot_sequence_buffer->Find( base_sequence );
                        CORE_ASSERT( entry );
                        quantized_base_cubes = (QuantizedCubeState*) &entry->cubes[0];
                    }
                }

                for ( int i = 0; i < NumCubes; ++i )
                {
                    bool changed = false;

                    if ( Stream::IsWriting )
                        changed = quantized_cubes[i] != quantized_base_cubes[i];

                    serialize_bool( stream, changed );

                    if ( changed )
                    {
                        serialize_bool( stream, quantized_cubes[i].interacting );
                        serialize_int( stream, quantized_cubes[i].position_x, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                        serialize_int( stream, quantized_cubes[i].position_y, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                        serialize_int( stream, quantized_cubes[i].position_z, 0, +QuantizedPositionBoundZ );
                        serialize_object( stream, quantized_cubes[i].orientation );
                    }
                    else if ( Stream::IsReading )
                    {
                        memcpy( &quantized_cubes[i], &quantized_base_cubes[i], sizeof( QuantizedCubeState ) );
                    }
                }
            }
            break;

            case DELTA_MODE_CHANGED_INDEX:
            {
                CORE_ASSERT( quantized_initial_snapshot );

                QuantizedCubeState * quantized_base_cubes = nullptr;

                if ( initial )
                {
                    quantized_base_cubes = quantized_initial_snapshot->cubes;
                }
                else
                {
                    if ( Stream::IsWriting )
                    {
                        CORE_ASSERT( quantized_snapshot_sliding_window );
                        auto & entry = quantized_snapshot_sliding_window->Get( base_sequence );
                        quantized_base_cubes = (QuantizedCubeState*) &entry.cubes[0];
                    }
                    else
                    {
                        CORE_ASSERT( quantized_snapshot_sequence_buffer );
                        auto entry = quantized_snapshot_sequence_buffer->Find( base_sequence );
                        CORE_ASSERT( entry );
                        quantized_base_cubes = (QuantizedCubeState*) &entry->cubes[0];
                    }
                }

                const int MaxIndex = 89;

                int num_changed = 0;
                bool use_indices = false;
                bool changed[NumCubes];
                if ( Stream::IsWriting )
                {
                    for ( int i = 0; i < NumCubes; ++i )
                    {
                        changed[i] = quantized_cubes[i] != quantized_base_cubes[i];
                        if ( changed[i] )
                            num_changed++;
                    }
                    if ( num_changed < MaxIndex )
                        use_indices = true;
                }

                serialize_bool( stream, use_indices );

                if ( use_indices )
                {
                    serialize_int( stream, num_changed, 0, MaxIndex + 1 );

                    if ( Stream::IsWriting )
                    {
                        int num_written = 0;

                        for ( int i = 0; i < NumCubes; ++i )
                        {
                            if ( changed[i] )
                            {
                                serialize_int( stream, i, 0, NumCubes - 1 );
                                serialize_bool( stream, quantized_cubes[i].interacting );
                                serialize_int( stream, quantized_cubes[i].position_x, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                                serialize_int( stream, quantized_cubes[i].position_y, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                                serialize_int( stream, quantized_cubes[i].position_z, 0, +QuantizedPositionBoundZ );
                                serialize_object( stream, quantized_cubes[i].orientation );
                                num_written++;
                            }
                        }

                        CORE_ASSERT( num_written == num_changed );
                    }
                    else
                    {
                        memset( changed, 0, sizeof( changed ) );

                        for ( int j = 0; j < num_changed; ++j )
                        {
                            int i;
                            serialize_int( stream, i, 0, NumCubes - 1 );
                            serialize_bool( stream, quantized_cubes[i].interacting );
                            serialize_int( stream, quantized_cubes[i].position_x, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                            serialize_int( stream, quantized_cubes[i].position_y, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                            serialize_int( stream, quantized_cubes[i].position_z, 0, +QuantizedPositionBoundZ );
                            serialize_object( stream, quantized_cubes[i].orientation );
                            changed[i] = true;
                        }

                        for ( int i = 0; i < NumCubes; ++i )
                        {
                            if ( !changed[i] )
                                memcpy( &quantized_cubes[i], &quantized_base_cubes[i], sizeof( QuantizedCubeState ) );
                        }
                    }
                }
                else
                {
                    for ( int i = 0; i < NumCubes; ++i )
                    {
                        serialize_bool( stream, changed[i] );

                        if ( changed[i] )
                        {
                            serialize_bool( stream, quantized_cubes[i].interacting );
                            serialize_int( stream, quantized_cubes[i].position_x, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                            serialize_int( stream, quantized_cubes[i].position_y, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                            serialize_int( stream, quantized_cubes[i].position_z, 0, +QuantizedPositionBoundZ );
                            serialize_object( stream, quantized_cubes[i].orientation );
                        }
                        else if ( Stream::IsReading )
                        {
                            memcpy( &quantized_cubes[i], &quantized_base_cubes[i], sizeof( QuantizedCubeState ) );
                        }
                    }
                }
            }
            break;

            case DELTA_MODE_RELATIVE_INDEX:
            {
                CORE_ASSERT( quantized_initial_snapshot );

                QuantizedCubeState * quantized_base_cubes = nullptr;

                if ( initial )
                {
                    quantized_base_cubes = quantized_initial_snapshot->cubes;
                }
                else
                {
                    if ( Stream::IsWriting )
                    {
                        CORE_ASSERT( quantized_snapshot_sliding_window );
                        auto & entry = quantized_snapshot_sliding_window->Get( base_sequence );
                        quantized_base_cubes = (QuantizedCubeState*) &entry.cubes[0];
                    }
                    else
                    {
                        CORE_ASSERT( quantized_snapshot_sequence_buffer );
                        auto entry = quantized_snapshot_sequence_buffer->Find( base_sequence );
                        CORE_ASSERT( entry );
                        quantized_base_cubes = (QuantizedCubeState*) &entry->cubes[0];
                    }
                }

                const int MaxChanged = 255;

                int num_changed = 0;
                bool use_indices = false;
                bool changed[NumCubes];
                if ( Stream::IsWriting )
                {
                    for ( int i = 0; i < NumCubes; ++i )
                    {
                        changed[i] = quantized_cubes[i] != quantized_base_cubes[i];
                        if ( changed[i] )
                            num_changed++;
                    }

                    const int relative_index_bits = count_relative_index_bits( changed );

                    if ( relative_index_bits < 900 && num_changed <= MaxChanged )
                    {
//                        if ( num_changed > 0 )
//                            printf( "num changed: %d, relative index bits: %d (%.1f avg)\n", num_changed, relative_index_bits, relative_index_bits / float( num_changed ) );

                        use_indices = true;
                    }
                }

                serialize_bool( stream, use_indices );

                if ( use_indices )
                {
                    serialize_int( stream, num_changed, 0, MaxChanged );

                    if ( Stream::IsWriting )
                    {
                        int num_written = 0;

                        bool first = true;
                        int previous_index = 0;

                        for ( int i = 0; i < NumCubes; ++i )
                        {
                            if ( changed[i] )
                            {
                                if ( first )
                                {
                                    serialize_int( stream, i, 0, NumCubes - 1 );
                                    first = false;
                                }
                                else
                                {   
                                    serialize_index_relative( stream, previous_index, i );
                                }

                                serialize_bool( stream, quantized_cubes[i].interacting );
                                serialize_int( stream, quantized_cubes[i].position_x, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                                serialize_int( stream, quantized_cubes[i].position_y, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                                serialize_int( stream, quantized_cubes[i].position_z, 0, +QuantizedPositionBoundZ );
                                serialize_object( stream, quantized_cubes[i].orientation );

                                num_written++;

                                previous_index = i;
                            }
                        }

                        CORE_ASSERT( num_written == num_changed );
                    }
                    else
                    {
                        memset( changed, 0, sizeof( changed ) );

                        int previous_index = 0;

                        for ( int j = 0; j < num_changed; ++j )
                        {
                            int i;
                            if ( j == 0 )
                                serialize_int( stream, i, 0, NumCubes - 1 );
                            else                                
                                serialize_index_relative( stream, previous_index, i );

                            serialize_bool( stream, quantized_cubes[i].interacting );
                            serialize_int( stream, quantized_cubes[i].position_x, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                            serialize_int( stream, quantized_cubes[i].position_y, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                            serialize_int( stream, quantized_cubes[i].position_z, 0, +QuantizedPositionBoundZ );
                            serialize_object( stream, quantized_cubes[i].orientation );

                            changed[i] = true;

                            previous_index = i;
                        }

                        for ( int i = 0; i < NumCubes; ++i )
                        {
                            if ( !changed[i] )
                                memcpy( &quantized_cubes[i], &quantized_base_cubes[i], sizeof( QuantizedCubeState ) );
                        }
                    }
                }
                else
                {
                    for ( int i = 0; i < NumCubes; ++i )
                    {
                        serialize_bool( stream, changed[i] );

                        if ( changed[i] )
                        {
                            serialize_bool( stream, quantized_cubes[i].interacting );
                            serialize_int( stream, quantized_cubes[i].position_x, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                            serialize_int( stream, quantized_cubes[i].position_y, -QuantizedPositionBoundXY, +QuantizedPositionBoundXY );
                            serialize_int( stream, quantized_cubes[i].position_z, 0, +QuantizedPositionBoundZ );
                            serialize_object( stream, quantized_cubes[i].orientation );
                        }
                        else if ( Stream::IsReading )
                        {
                            memcpy( &quantized_cubes[i], &quantized_base_cubes[i], sizeof( QuantizedCubeState ) );
                        }
                    }
                }
            }
            break;

            case DELTA_MODE_RELATIVE_POSITION:
            {
                CORE_ASSERT( quantized_initial_snapshot );

                QuantizedCubeState * quantized_base_cubes = nullptr;

                if ( initial )
                {
                    quantized_base_cubes = quantized_initial_snapshot->cubes;
                }
                else
                {
                    if ( Stream::IsWriting )
                    {
                        CORE_ASSERT( quantized_snapshot_sliding_window );
                        auto & entry = quantized_snapshot_sliding_window->Get( base_sequence );
                        quantized_base_cubes = (QuantizedCubeState*) &entry.cubes[0];
                    }
                    else
                    {
                        CORE_ASSERT( quantized_snapshot_sequence_buffer );
                        auto entry = quantized_snapshot_sequence_buffer->Find( base_sequence );
                        CORE_ASSERT( entry );
                        quantized_base_cubes = (QuantizedCubeState*) &entry->cubes[0];
                    }
                }

                const int MaxIndex = 126;

                int num_changed = 0;
                bool use_indices = false;
                bool changed[NumCubes];
                if ( Stream::IsWriting )
                {
                    for ( int i = 0; i < NumCubes; ++i )
                    {
                        changed[i] = quantized_cubes[i] != quantized_base_cubes[i];
                        if ( changed[i] )
                            num_changed++;
                    }
                    if ( num_changed < MaxIndex )
                        use_indices = true;
                }

                serialize_bool( stream, use_indices );

                if ( use_indices )
                {
                    serialize_int( stream, num_changed, 0, MaxIndex + 1 );

                    if ( Stream::IsWriting )
                    {
                        int num_written = 0;

                        bool first = true;
                        int previous_index = 0;

                        for ( int i = 0; i < NumCubes; ++i )
                        {
                            if ( changed[i] )
                            {
                                if ( first )
                                {
                                    serialize_int( stream, i, 0, NumCubes - 1 );
                                    first = false;
                                }
                                else
                                {   
                                    serialize_index_relative( stream, previous_index, i );
                                }

                                serialize_cube_relative_position( stream, quantized_cubes[i], quantized_base_cubes[i] );

                                num_written++;

                                previous_index = i;
                            }
                        }

                        CORE_ASSERT( num_written == num_changed );
                    }
                    else
                    {
                        memset( changed, 0, sizeof( changed ) );

                        int previous_index = 0;

                        for ( int j = 0; j < num_changed; ++j )
                        {
                            int i;
                            if ( j == 0 )
                                serialize_int( stream, i, 0, NumCubes - 1 );
                            else                                
                                serialize_index_relative( stream, previous_index, i );

                            serialize_cube_relative_position( stream, quantized_cubes[i], quantized_base_cubes[i] );

                            changed[i] = true;

                            previous_index = i;
                        }

                        for ( int i = 0; i < NumCubes; ++i )
                        {
                            if ( !changed[i] )
                                memcpy( &quantized_cubes[i], &quantized_base_cubes[i], sizeof( QuantizedCubeState ) );
                        }
                    }
                }
                else
                {
                    for ( int i = 0; i < NumCubes; ++i )
                    {
                        serialize_bool( stream, changed[i] );

                        if ( changed[i] )
                        {
                            serialize_cube_relative_position( stream, quantized_cubes[i], quantized_base_cubes[i] );
                        }
                        else if ( Stream::IsReading )
                        {
                            memcpy( &quantized_cubes[i], &quantized_base_cubes[i], sizeof( QuantizedCubeState ) );
                        }
                    }
                }
            }
            break;

            case DELTA_MODE_RELATIVE_ORIENTATION:
            {
                CORE_ASSERT( quantized_initial_snapshot );

                QuantizedCubeState * quantized_base_cubes = nullptr;

                if ( initial )
                {
                    quantized_base_cubes = quantized_initial_snapshot->cubes;
                }
                else
                {
                    if ( Stream::IsWriting )
                    {
                        CORE_ASSERT( quantized_snapshot_sliding_window );
                        auto & entry = quantized_snapshot_sliding_window->Get( base_sequence );
                        quantized_base_cubes = (QuantizedCubeState*) &entry.cubes[0];
                    }
                    else
                    {
                        CORE_ASSERT( quantized_snapshot_sequence_buffer );
                        auto entry = quantized_snapshot_sequence_buffer->Find( base_sequence );
                        CORE_ASSERT( entry );
                        quantized_base_cubes = (QuantizedCubeState*) &entry->cubes[0];
                    }
                }

                const int MaxIndex = 126;

                int num_changed = 0;
                bool use_indices = false;
                bool changed[NumCubes];
                if ( Stream::IsWriting )
                {
                    for ( int i = 0; i < NumCubes; ++i )
                    {
                        changed[i] = quantized_cubes[i] != quantized_base_cubes[i];
                        if ( changed[i] )
                            num_changed++;
                    }
                    if ( num_changed < MaxIndex )
                        use_indices = true;
                }

                serialize_bool( stream, use_indices );

                if ( use_indices )
                {
                    serialize_int( stream, num_changed, 0, MaxIndex + 1 );

                    if ( Stream::IsWriting )
                    {
                        int num_written = 0;

                        bool first = true;
                        int previous_index = 0;

                        for ( int i = 0; i < NumCubes; ++i )
                        {
                            if ( changed[i] )
                            {
                                if ( first )
                                {
                                    serialize_int( stream, i, 0, NumCubes - 1 );
                                    first = false;
                                }
                                else
                                {   
                                    serialize_index_relative( stream, previous_index, i );
                                }

                                serialize_cube_relative_orientation( stream, quantized_cubes[i], quantized_base_cubes[i] );

                                num_written++;

                                previous_index = i;
                            }
                        }

                        CORE_ASSERT( num_written == num_changed );
                    }
                    else
                    {
                        memset( changed, 0, sizeof( changed ) );

                        int previous_index = 0;

                        for ( int j = 0; j < num_changed; ++j )
                        {
                            int i;
                            if ( j == 0 )
                                serialize_int( stream, i, 0, NumCubes - 1 );
                            else                                
                                serialize_index_relative( stream, previous_index, i );

                            serialize_cube_relative_orientation( stream, quantized_cubes[i], quantized_base_cubes[i] );

                            changed[i] = true;

                            previous_index = i;
                        }

                        for ( int i = 0; i < NumCubes; ++i )
                        {
                            if ( !changed[i] )
                                memcpy( &quantized_cubes[i], &quantized_base_cubes[i], sizeof( QuantizedCubeState ) );
                        }
                    }
                }
                else
                {
                    for ( int i = 0; i < NumCubes; ++i )
                    {
                        serialize_bool( stream, changed[i] );

                        if ( changed[i] )
                        {
                            serialize_cube_relative_orientation( stream, quantized_cubes[i], quantized_base_cubes[i] );
                        }
                        else if ( Stream::IsReading )
                        {
                            memcpy( &quantized_cubes[i], &quantized_base_cubes[i], sizeof( QuantizedCubeState ) );
                        }
                    }
                }
            }
            break;

            default:
                break;
        }
    }
};

struct DeltaAckPacket : public protocol::Packet
{
    uint16_t ack;

    DeltaAckPacket() : Packet( DELTA_ACK_PACKET )
    {
        ack = 0;
    }

    PROTOCOL_SERIALIZE_OBJECT( stream )
    {
        serialize_uint16( stream, ack );
    }
};

class DeltaPacketFactory : public protocol::PacketFactory
{
    core::Allocator * m_allocator;

public:

    DeltaPacketFactory( core::Allocator & allocator )
        : PacketFactory( allocator, DELTA_NUM_PACKETS )
    {
        m_allocator = &allocator;
    }

protected:

    protocol::Packet * CreateInternal( int type )
    {
        switch ( type )
        {
            case DELTA_SNAPSHOT_PACKET:   return CORE_NEW( *m_allocator, DeltaSnapshotPacket );
            case DELTA_ACK_PACKET:        return CORE_NEW( *m_allocator, DeltaAckPacket );
            default:
                return nullptr;
        }
    }
};

struct DeltaInternal
{
    DeltaInternal( core::Allocator & allocator, const SnapshotModeData & mode_data ) 
        : packet_factory( allocator ), interpolation_buffer( allocator, mode_data )
    {
        this->allocator = &allocator;
        network::SimulatorConfig networkSimulatorConfig;
        quantized_snapshot_sliding_window = CORE_NEW( allocator, QuantizedSnapshotSlidingWindow, allocator, MaxSnapshots );
        quantized_snapshot_sequence_buffer = CORE_NEW( allocator, QuantizedSnapshotSequenceBuffer, allocator, MaxSnapshots );
        networkSimulatorConfig.packetFactory = &packet_factory;
        networkSimulatorConfig.maxPacketSize = MaxPacketSize;
        network_simulator = CORE_NEW( allocator, network::Simulator, networkSimulatorConfig );
        context[0] = quantized_snapshot_sliding_window;
        context[1] = quantized_snapshot_sequence_buffer;
        context[2] = &quantized_initial_snapshot;
        network_simulator->SetContext( context );
        Reset( mode_data );
    }

    ~DeltaInternal()
    {
        CORE_ASSERT( network_simulator );
        typedef network::Simulator NetworkSimulator;
        CORE_DELETE( *allocator, NetworkSimulator, network_simulator );
        CORE_DELETE( *allocator, QuantizedSnapshotSlidingWindow, quantized_snapshot_sliding_window );
        CORE_DELETE( *allocator, QuantizedSnapshotSequenceBuffer, quantized_snapshot_sequence_buffer );
        network_simulator = nullptr;
        quantized_snapshot_sliding_window = nullptr;
        quantized_snapshot_sequence_buffer = nullptr;
    }

    void Reset( const SnapshotModeData & mode_data )
    {
        interpolation_buffer.Reset();
        network_simulator->Reset();
        network_simulator->ClearStates();
        network_simulator->AddState( { mode_data.latency, mode_data.jitter, mode_data.packet_loss } );
        quantized_snapshot_sliding_window->Reset();
        quantized_snapshot_sequence_buffer->Reset();
        send_sequence = 0;
        recv_sequence = 0;
        send_accumulator = 1.0f;
        received_ack = false;
    }

    core::Allocator * allocator;
    uint16_t send_sequence;
    uint16_t recv_sequence;
    bool received_ack;
    float send_accumulator;
    const void * context[3];
    network::Simulator * network_simulator;
    QuantizedSnapshotSlidingWindow * quantized_snapshot_sliding_window;
    QuantizedSnapshotSequenceBuffer * quantized_snapshot_sequence_buffer;
    DeltaPacketFactory packet_factory;
    SnapshotInterpolationBuffer interpolation_buffer;
    QuantizedSnapshot quantized_initial_snapshot;
};

DeltaDemo::DeltaDemo( core::Allocator & allocator )
{
    InitDeltaModes();
    m_allocator = &allocator;
    m_internal = nullptr;
    m_settings = CORE_NEW( *m_allocator, CubesSettings );
    m_delta = CORE_NEW( *m_allocator, DeltaInternal, *m_allocator, delta_mode_data[GetMode()] );
}

DeltaDemo::~DeltaDemo()
{
    Shutdown();
    CORE_DELETE( *m_allocator, DeltaInternal, m_delta );
    CORE_DELETE( *m_allocator, CubesSettings, m_settings );
    m_delta = nullptr;
    m_settings = nullptr;
    m_allocator = nullptr;
}

bool DeltaDemo::Initialize()
{
    if ( m_internal )
        Shutdown();

    m_internal = CORE_NEW( *m_allocator, CubesInternal );    

    CubesConfig config;
    
    config.num_simulations = 1;
    config.num_views = 2;

    m_internal->Initialize( *m_allocator, config, m_settings );

    auto game_instance = m_internal->GetGameInstance( 0 );

    bool result = GetQuantizedSnapshot( game_instance, m_delta->quantized_initial_snapshot );
    CORE_ASSERT( result );

    return true;
}

void DeltaDemo::Shutdown()
{
    CORE_ASSERT( m_allocator );

    CORE_ASSERT( m_delta );
    m_delta->Reset( delta_mode_data[GetMode()] );

    if ( m_internal )
    {
        m_internal->Free( *m_allocator );
        CORE_DELETE( *m_allocator, CubesInternal, m_internal );
        m_internal = nullptr;
    }
}

void DeltaDemo::Update()
{
    CubesUpdateConfig update_config;

    auto local_input = m_internal->GetLocalInput();

    // setup left simulation to update one frame with local input

    update_config.sim[0].num_frames = 1;
    update_config.sim[0].frame_input[0] = local_input;

    // send a snapshot packet to the right simulation

    m_delta->send_accumulator += global.timeBase.deltaTime;

    if ( m_delta->send_accumulator >= 1.0f / delta_mode_data[GetMode()].send_rate )
    {
        m_delta->send_accumulator = 0.0f;   

        auto game_instance = m_internal->GetGameInstance( 0 );

        auto snapshot_packet = (DeltaSnapshotPacket*) m_delta->packet_factory.Create( DELTA_SNAPSHOT_PACKET );

        snapshot_packet->sequence = m_delta->send_sequence++;
        snapshot_packet->base_sequence = m_delta->quantized_snapshot_sliding_window->GetAck() + 1;
        snapshot_packet->initial = !m_delta->received_ack;

        snapshot_packet->delta_mode = GetMode();

        uint16_t sequence;

        auto & snapshot = m_delta->quantized_snapshot_sliding_window->Insert( sequence );

        if ( GetQuantizedSnapshot( game_instance, snapshot ) )
            m_delta->network_simulator->SendPacket( network::Address( "::1", RightPort ), snapshot_packet );
        else
            m_delta->packet_factory.Destroy( snapshot_packet );
    }

    // update the network simulator

    m_delta->network_simulator->Update( global.timeBase );

    // receive packets from the simulator (with latency, packet loss and jitter applied...)

    bool received_snapshot_this_frame = false;
    uint16_t ack_sequence = 0;

    while ( true )
    {
        auto packet = m_delta->network_simulator->ReceivePacket();
        if ( !packet )
            break;

        const auto port = packet->GetAddress().GetPort();
        const auto type = packet->GetType();

        if ( type == DELTA_SNAPSHOT_PACKET && port == RightPort )
        {
            auto snapshot_packet = (DeltaSnapshotPacket*) packet;

            QuantizedSnapshot * quantized_snapshot = m_delta->quantized_snapshot_sequence_buffer->Find( snapshot_packet->sequence );
    
            CORE_ASSERT( quantized_snapshot );
    
            Snapshot snapshot;
            for ( int i = 0; i < NumCubes; ++i )
                quantized_snapshot->cubes[i].Save( snapshot.cubes[i] );

            m_delta->interpolation_buffer.AddSnapshot( global.timeBase.time, snapshot_packet->sequence, snapshot.cubes );

            if ( !received_snapshot_this_frame || ( received_snapshot_this_frame && core::sequence_greater_than( snapshot_packet->sequence, ack_sequence ) ) )
            {
                received_snapshot_this_frame = true;
                ack_sequence = snapshot_packet->sequence;
            }
        }
        else if ( type == DELTA_ACK_PACKET && port == LeftPort )
        {
            auto ack_packet = (DeltaAckPacket*) packet;

            m_delta->quantized_snapshot_sliding_window->Ack( ack_packet->ack - 1 );
            m_delta->received_ack = true;
        }

        m_delta->packet_factory.Destroy( packet );
    }

    // if any snapshots packets were received this frame, send an ack packet back to the left simulation

    if ( received_snapshot_this_frame )
    {
        auto ack_packet = (DeltaAckPacket*) m_delta->packet_factory.Create( DELTA_ACK_PACKET );
        ack_packet->ack = ack_sequence;
        m_delta->network_simulator->SetBandwidthExclude( true );
        m_delta->network_simulator->SendPacket( network::Address( "::1", LeftPort ), ack_packet );
        m_delta->network_simulator->SetBandwidthExclude( false );
    }

    // if we are an an interpolation mode, we need to grab the view updates for the right side from the interpolation buffer

    int num_object_updates = 0;

    view::ObjectUpdate object_updates[NumCubes];

    m_delta->interpolation_buffer.GetViewUpdate( delta_mode_data[GetMode()], global.timeBase.time, object_updates, num_object_updates );

    if ( num_object_updates > 0 )
        m_internal->view[1].objects.UpdateObjects( object_updates, num_object_updates );
    else if ( m_delta->interpolation_buffer.interpolating )
        printf( "no snapshot to interpolate towards!\n" );

    // run the simulation

    m_internal->Update( update_config );
}

bool DeltaDemo::Clear()
{
    return m_internal->Clear();
}

void DeltaDemo::Render()
{
    CubesRenderConfig render_config;

    render_config.render_mode = CUBES_RENDER_SPLITSCREEN;

    m_internal->Render( render_config );

    const float bandwidth = m_delta->network_simulator->GetBandwidth();

    char bandwidth_string[256];
    if ( bandwidth < 1024 )
        snprintf( bandwidth_string, (int) sizeof( bandwidth_string ), "Bandwidth: %d kbps", (int) bandwidth );
    else
        snprintf( bandwidth_string, (int) sizeof( bandwidth_string ), "Bandwidth: %.2f mbps", bandwidth / 1000 );

    Font * font = global.fontManager->GetFont( "Bandwidth" );
    if ( font )
    {
        const float text_x = ( global.displayWidth - font->GetTextWidth( bandwidth_string ) ) / 2;
        const float text_y = 5;
        font->Begin();
        font->DrawText( text_x, text_y, bandwidth_string, Color( 0.27f,0.81f,1.0f ) );
        font->End();
    }
}

bool DeltaDemo::KeyEvent( int key, int scancode, int action, int mods )
{
    return m_internal->KeyEvent( key, scancode, action, mods );
}

bool DeltaDemo::CharEvent( unsigned int code )
{
    // ...

    return false;
}

int DeltaDemo::GetNumModes() const
{
    return DELTA_NUM_MODES;
}

const char * DeltaDemo::GetModeDescription( int mode ) const
{
    return delta_mode_descriptions[mode];
}

#endif // #ifdef CLIENT

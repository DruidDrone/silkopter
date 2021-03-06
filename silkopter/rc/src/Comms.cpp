#include "Comms.h"
#include "Log.h"

#include "common/node/INode.h"
#include "common/node/IPilot.h"

#include "common/Comm_Data.h"

#include "utils/comms/UDP_Socket.h"
#include "utils/comms/RFMON_Socket.h"
#include "utils/comms/RF4463F30_Socket.h"
#include "utils/hw/SPI_PIGPIO.h"
#include "utils/hw/SPI_Dev.h"

#include "IHAL.h"
#include "settings.def.h"

namespace silk
{

//constexpr char const* VIDEO_INTERFACE1 = "wlan2";
//constexpr char const* VIDEO_INTERFACE2 = "wlan3";

//#define USE_SPI_PIGPIO

#ifdef USE_SPI_PIGPIO
constexpr size_t SPI_PORT = 0;
constexpr size_t SPI_CHANNEL = 0;
constexpr size_t SPI_SPEED = 4000000;
#else
constexpr size_t PHY_COUNT = 2;
static const char* SPI_DEVICES[] = { "/dev/spidev0.0", "/dev/spidev0.1" };
constexpr size_t SPI_SPEED = 10000000;
#endif


///////////////////////////////////////////////////////////////////////////////////////////////////

Comms::Comms(IHAL& hal)
    : m_hal(hal)
    , m_tx_queue(128)
    , m_rx_queue(128)
{
    srand((size_t)this + clock());
    m_station_id = rand();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Comms::~Comms()
{
    for (Phy_Data& phy_data: m_phy_data)
    {
        phy_data.thread_exit = true;
    }

    m_tx_queue.exit();
    m_rx_queue.exit();

    for (Phy_Data& phy_data: m_phy_data)
    {
        phy_data.thread_exit = true;
        if (phy_data.thread.joinable())
        {
            phy_data.thread.join();
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool Comms::start()
{
#if !defined RASPBERRY_PI
    return true;
#endif

    settings::Settings::Comms const& settings = m_hal.get_settings().get_comms();

    m_phy_data.resize(PHY_COUNT);

    size_t index = 0;
    for (Phy_Data& phy_data: m_phy_data)
    {
        phy_data.is_main = index == 0;
        phy_data.phy.reset(new Phy());
        if (phy_data.phy->init_dev(SPI_DEVICES[index], SPI_SPEED) != Phy::Init_Result::OK)
        {
            QLOGE("Cannot start main phy");
            return false;
        }
        index++;

        QLOGI("Phy FEC: K={}, N={}, MTU={}", settings.get_fec_coding_k(), settings.get_fec_coding_n(), settings.get_mtu());
        phy_data.phy->setup_fec_channel(settings.get_fec_coding_k(), settings.get_fec_coding_n(), settings.get_mtu());

        QLOGI("Phy Rate: {}", settings.get_rate());
        phy_data.phy->set_rate(static_cast<Phy::Rate>(settings.get_rate()));

        QLOGI("Phy TX Power: {}", settings.get_tx_power());
        phy_data.phy->set_power(settings.get_tx_power());

        QLOGI("Phy Channel: {}", settings.get_channel());
        phy_data.phy->set_channel(settings.get_channel());

        phy_data.thread = std::thread(std::bind(&Comms::phy_thread_proc, this, &phy_data));
    }

    QLOGI("Started receiving video");

    return true;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Comms::add_raw_stats(Raw_Stats& dst, Raw_Stats const& src) const
{
    dst.packets_dropped     += src.packets_dropped;
    dst.packets_received    += src.packets_received;
    dst.packets_sent        += src.packets_sent;
    dst.data_received       += src.data_received;
    dst.data_sent           += src.data_sent;
    dst.rx_rssi_count       += src.rx_rssi_count;
    dst.rx_rssi_accumulated += src.rx_rssi_accumulated;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Comms::phy_thread_proc(Phy_Data* phy_data_ptr)
{
    Phy_Data& phy_data = *phy_data_ptr;

    Packet_ptr rx_packet = m_packet_pool.acquire();
    rx_packet->resize(Phy::MAX_PAYLOAD_SIZE);

    while (!phy_data.thread_exit)
    {
        Raw_Stats local_stats;

        //TX
        if (phy_data.is_main)
        {
            Packet_ptr tx_packet;
            m_tx_queue.pop_front(tx_packet, false);
            if (tx_packet)
            {
                local_stats.packets_sent++;
                local_stats.data_sent += tx_packet->size();
                phy_data.phy->send_data(tx_packet->data(), tx_packet->size(), false);
            }
        }

        if (m_single_phy && !phy_data.is_main)
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
            continue;
        }

        //RX
        size_t rx_size = 0;
        int16_t rx_rssi = std::numeric_limits<int16_t>::lowest();
        size_t rounds = 10;
        while (phy_data.phy->receive_data(rx_packet->data(), rx_size, rx_rssi) && rounds-- > 0)
        {
            if (rx_size < sizeof(rc_comms::Packet_Header))
            {
                continue;
            }
            rc_comms::Packet_Header const& packet_header = *reinterpret_cast<const rc_comms::Packet_Header*>(rx_packet->data());
            if (packet_header.station_id == m_station_id) //reject my packets
            {
                continue;
            }

            //access common data
            {
                std::lock_guard<std::mutex> lg(m_thread_proc_mutex);
                Clock::time_point now = Clock::now();
                if (now - m_last_received_packet_tp > std::chrono::seconds(2))
                {
                    m_last_received_packet_index = 0;
                }
                if (packet_header.packet_index <= m_last_received_packet_index)
                {
                    //QLOGI("old packet: {}", packet_header.packet_index);
                    continue;
                }
                rx_packet->resize(rx_size);

                auto it = std::lower_bound(m_parked_packets.begin(),
                                           m_parked_packets.end(),
                                           packet_header.packet_index,
                                           [](const Parked_Packet& p, uint32_t packet_index)
                {
                    return p.packet_index < packet_index;
                });
                if (it == m_parked_packets.end() || it->packet_index != packet_header.packet_index)
                {
                    m_parked_packets.insert(it, Parked_Packet{packet_header.packet_index, rx_packet});
                }
                else
                {
                    //QLOGI("dup packet: {}", packet_header.packet_index);
                    //int a = 0;
                }

                //push the packets in order (preferably)
                while (!m_parked_packets.empty() && //while we have packets AND
                       (m_parked_packets.front().packet_index == m_last_received_packet_index + 1 || //they are in order OR
                        m_parked_packets.size() >= m_phy_data.size() * 5)) //we accumulated too many
                {
                    m_last_received_packet_tp = Clock::now();
                    Parked_Packet& parked_packet = m_parked_packets.front();

                    size_t gap = parked_packet.packet_index - (m_last_received_packet_index + 1);
                    local_stats.packets_dropped += gap;
                    if (gap > 0)
                    {
                        //QLOGI("gap: {}", gap);
                    }

                    local_stats.packets_received++;
                    local_stats.data_received += parked_packet.packet->size();

                    m_last_received_packet_index = parked_packet.packet_index;
                    m_rx_queue.push_back(parked_packet.packet, false);
                    m_parked_packets.pop_front();
                }
            }

            if (rx_rssi != std::numeric_limits<int16_t>::lowest())
            {
                local_stats.rx_rssi_count++;
                local_stats.rx_rssi_accumulated += rx_rssi;
            }

            rx_packet = m_packet_pool.acquire();
            rx_packet->resize(Phy::MAX_PAYLOAD_SIZE);
        }

        {
            std::lock_guard<std::mutex> lg(m_raw_stats_mutex);
            add_raw_stats(m_raw_stats, local_stats);
        }
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

const Comms::Stats& Comms::get_stats() const
{
    return m_stats;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Phy const& Comms::get_phy() const
{
    return *m_phy_data.front().phy;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Phy& Comms::get_phy()
{
    return *m_phy_data.front().phy;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Comms::set_single_phy(bool yes)
{
    m_single_phy = yes;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

rc_comms::Packet_Header Comms::prepare_packet_header(rc_comms::Packet_Type packet_type, uint32_t packet_index) const
{
    rc_comms::Packet_Header packet_header;
    packet_header.packet_type = uint32_t(packet_type);
    packet_header.station_id = m_station_id;
    packet_header.packet_index = packet_index;
    packet_header.rx_rssi = m_stats.rx_rssi;
    return packet_header;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool Comms::sent_multirotor_commands_packet()
{
    if (Clock::now() - m_multirotor_commands_last_valid_tp < std::chrono::milliseconds(500))
    {
        Packet_ptr packet = m_packet_pool.acquire();
        packet->clear();
        silk::rc_comms::Packet_Header packet_header = prepare_packet_header(silk::rc_comms::Packet_Type::MULTIROTOR_COMMANDS, ++m_last_sent_packet_index);
        size_t off = 0;
        util::serialization::serialize(*packet, packet_header, off);
        util::serialization::serialize(*packet, m_multirotor_commands, off);
        m_tx_queue.push_back(packet, false);
        return true;
    }
    else
    {
        //commands are out of date - don't send them
        return false;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

bool Comms::send_camera_commands_packet()
{
    if (Clock::now() - m_camera_commands_last_valid_tp < std::chrono::milliseconds(500))
    {
        Packet_ptr packet = m_packet_pool.acquire();
        packet->clear();
        silk::rc_comms::Packet_Header packet_header = prepare_packet_header(silk::rc_comms::Packet_Type::CAMERA_COMMANDS, ++m_last_sent_packet_index);
        size_t off = 0;
        util::serialization::serialize(*packet, packet_header, off);
        util::serialization::serialize(*packet, m_camera_commands, off);
        m_tx_queue.push_back(packet, false);
        return true;
    }
    else
    {
        //commands are out of date - don't send them
        return false;
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Comms::process_rx_packet(rc_comms::Packet_Type packet_type, std::vector<uint8_t> const& data, size_t offset)
{
//    m_rx_packet.rx_dBm = packet.rx_dBm;
//    m_rx_packet.tx_dBm = packet.tx_dBm;
//    m_rx_packet.rx_timepoint = packet.rx_timepoint;

    static size_t bps = 0;
    static size_t pps = 0;
    static Clock::time_point tp;
    if (Clock::now() - tp >= std::chrono::seconds(1))
    {
        QLOGI("bps: {}, pps: {}", bps, pps);
        bps = 0;
        pps = 0;
        tp = Clock::now();
    }
    bps += data.size();
    pps ++;

    bool dsz_ok = false;
    if (packet_type == rc_comms::Packet_Type::MULTIROTOR_STATE)
    {
        std::lock_guard<std::mutex> lg(m_samples_mutex);
        stream::IMultirotor_State::Value value = m_multirotor_state;
        dsz_ok = util::serialization::deserialize(data, value, offset);
        if (dsz_ok)
        {
            m_multirotor_state = value;
        }
    }
    else if (packet_type == rc_comms::Packet_Type::VIDEO)
    {
        rc_comms::Video_Header video_header;
        if (util::serialization::deserialize(data, video_header, offset))
        {
            math::vec2u16 resolution(video_header.width, video_header.height);
            if (on_video_data_received)
            {
                on_video_data_received(data.data() + offset, data.size() - offset, resolution);
            }
            dsz_ok = true;
        }
    }
//    else if (packet_type == rc_comms::Packet_Type::CAMERA_COMMANDS)
//    {
//        dsz_ok = true;
//    }
//    else if (packet_type == rc_comms::Packet_Type::MULTIROTOR_COMMANDS)
//    {
//        dsz_ok = true;
//    }

    if (!dsz_ok)
    {
        QLOGW("Cannot deserialize incoming packet");
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Comms::process_received_data(std::vector<uint8_t> const& data)
{
    if (data.size() < sizeof(rc_comms::Packet_Type))
    {
        QLOGE("Invalid data received");
        return;
    }

    rc_comms::Packet_Header const& packet_header = *reinterpret_cast<const rc_comms::Packet_Header*>(data.data());
    m_stats.tx_rssi = packet_header.rx_rssi;
    //QLOGI("packet index {}", packet_header.packet_index);
    process_rx_packet(rc_comms::Packet_Type(packet_header.packet_type), data, sizeof(rc_comms::Packet_Header));
}

///////////////////////////////////////////////////////////////////////////////////////////////////

Clock::time_point Comms::get_last_rx_tp() const
{
    return Clock::now();//m_rx_packet.rx_timepoint;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

//void Comms::get_video_data(std::vector<uint8_t>& dst, math::vec2u16& resolution)
//{
//    std::lock_guard<std::mutex> lg(m_samples_mutex);
//    size_t offset = dst.size();
//    size_t size = m_video_data.size();
//    if (size > 0)
//    {
//        dst.resize(offset + size);
//        memcpy(dst.data() + offset, m_video_data.data(), size);
//        m_video_data.clear();
//    }
//    resolution = m_video_resolution;
//}

///////////////////////////////////////////////////////////////////////////////////////////////////

stream::IMultirotor_State::Value Comms::get_multirotor_state() const
{
    std::lock_guard<std::mutex> lg(m_samples_mutex);
    return m_multirotor_state;
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Comms::send_multirotor_commands_value(stream::IMultirotor_Commands::Value const& value)
{
    m_multirotor_commands = value;
    m_multirotor_commands_last_valid_tp = Clock::now();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Comms::send_camera_commands_value(stream::ICamera_Commands::Value const& value)
{
    m_camera_commands = value;
    m_camera_commands_last_valid_tp = Clock::now();
}

///////////////////////////////////////////////////////////////////////////////////////////////////

void Comms::process()
{
    Clock::time_point now = Clock::now();
    if (now - m_telemetry_tp >= std::chrono::milliseconds(30))
    {
        m_telemetry_tp = now;
    }

    if (now - m_last_stats_tp >= std::chrono::seconds(1))
    {
        float dtsf = std::chrono::duration<float>(now - m_last_stats_tp).count();
        m_last_stats_tp = now;

        std::lock_guard<std::mutex> lg(m_raw_stats_mutex);
        m_stats.packets_dropped_per_second = float(m_raw_stats.packets_dropped) / dtsf;
        m_stats.packets_received_per_second = float(m_raw_stats.packets_received) / dtsf;
        m_stats.packets_sent_per_second = float(m_raw_stats.packets_sent) / dtsf;
        m_stats.data_received_per_second = float(m_raw_stats.data_received) / dtsf;
        m_stats.data_sent_per_second = float(m_raw_stats.data_sent) / dtsf;
        if (m_raw_stats.rx_rssi_count > 0)
        {
            m_stats.rx_rssi = int16_t(m_raw_stats.rx_rssi_accumulated / int32_t(m_raw_stats.rx_rssi_count));
        }
        else
        {
            m_stats.rx_rssi = std::numeric_limits<int16_t>::lowest();
        }
        m_raw_stats = Raw_Stats();
    }
//    if (now - m_rx_packet.rx_timepoint >= std::chrono::seconds(2))
//    {
//        m_rx_packet.rx_dBm = 0;
//        m_rx_packet.tx_dBm = 0;
//    }

    {
        Packet_ptr rx_packet;
        while (m_rx_queue.pop_front(rx_packet, false))
        {
            process_received_data(*rx_packet);
        }
    }

    if (now - m_multirotor_commands_last_sent_tp > std::chrono::milliseconds(20))
    {
        m_multirotor_commands_last_sent_tp = now;
        sent_multirotor_commands_packet();
    }
    if (now - m_camera_commands_last_sent_tp > std::chrono::milliseconds(100))
    {
        m_camera_commands_last_sent_tp = now;
        send_camera_commands_packet();
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////

}

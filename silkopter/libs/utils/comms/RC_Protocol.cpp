#include "RC_Protocol.h"
#include "util/murmurhash.h"
#include "fec.h"

namespace util
{
namespace comms
{

static constexpr unsigned BLOCK_NUMS[] = { 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
                                           10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20,
                                           21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31 };

//#define LOG

RC_Protocol::RC_Protocol(RC_Phy& phy, RX_Callback rx_callback)
    : m_phy(phy)
    , m_tx_fec_source_queue(32)
    , m_tx_fec_extra_queue(32)
    , m_rx_callback(rx_callback)
{
    m_phy.set_callbacks(std::bind(&RC_Protocol::compute_tx_data, this, std::placeholders::_1, std::placeholders::_2),
                        std::bind(&RC_Protocol::process_rx_data, this, std::placeholders::_1, std::placeholders::_2));

    //std::atomic_init(&m_crt_sent_packet_index, 0);
    //std::atomic_init(&m_received_packet_index, 0);

}

RC_Protocol::~RC_Protocol()
{
    if (m_fec)
    {
        fec_free(m_fec);
    }
}

bool RC_Protocol::init(uint8_t fec_coding_k, uint8_t fec_coding_n)
{
    if (fec_coding_k == 0 || fec_coding_n < fec_coding_k || fec_coding_k > m_fec_source_ptrs.size() || fec_coding_n > m_fec_extra_ptrs.size())
    {
        QLOGE("Invalid coding params: {} / {}" , fec_coding_k, fec_coding_n);
        return false;
    }
    m_fec_coding_k = fec_coding_k;
    m_fec_coding_n = fec_coding_n;

    if (m_fec)
    {
        fec_free(m_fec);
    }
    m_fec = fec_new(m_fec_coding_k, m_fec_coding_n);

    m_fec_buffer_size = std::min(400u, m_phy.get_mtu() - sizeof(FEC_Header));

    return true;
}

void RC_Protocol::add_periodic_packet(Clock::duration period, TX_Callback tx_callback)
{
    Periodic_Packet pp;
    pp.period = period;
    pp.tx_callback = tx_callback;
    m_periodic_packets.push_back(pp);
}

void RC_Protocol::send_reliable_packet(uint8_t packet_type, void const* data, size_t size)
{
    Reliable_Packet packet;
    packet.packet_type = packet_type;
    packet.offset = 0;
    if (data != nullptr && size > 0)
    {
        packet.payload.resize(size);
        memcpy(packet.payload.data(), data, size);
    }

    std::lock_guard<std::mutex> lg (m_reliable_packet_queue_mutex);
    m_reliable_packet_queue.push_back(std::move(packet));
}

void RC_Protocol::send_fec_packet(void const* _data, size_t size)
{
    uint8_t const* data = reinterpret_cast<uint8_t const*>(_data);
    while (size > 0)
    {
        if (!m_fec_crt_source_buffer)
        {
            m_fec_crt_source_buffer = m_tx_fec_source_queue.begin_producing();
            m_fec_crt_source_buffer->clear();
        }
        if (!m_fec_crt_source_buffer)
        {
            QLOGW("No FEC buffers available, packet not sent");
            return;
        }

        size_t s = std::min(size, m_fec_buffer_size - m_fec_crt_source_buffer->size());
        size_t offset = m_fec_crt_source_buffer->size();
        m_fec_crt_source_buffer->resize(offset + s);
        memcpy(m_fec_crt_source_buffer->data() + offset, data, s);
        data += s;
        size -= s;

        if (m_fec_crt_source_buffer->size() >= m_fec_buffer_size)
        {
            m_tx_fec_source_queue.end_producing(std::move(m_fec_crt_source_buffer), true);
        }
    }
}

void RC_Protocol::reset_session()
{
    //A party will always reply to the reset signal with a signal of it's own.
    //The reset signal always contains as payload the session id

    //Case A: one party initiates the reset
    //1. One party calculates a new session id and starts spamming the reset signal
    //2. The other party receives the reset signal, copies the session id and replies with another reset signal
    //3. The initial party receives the others reset signal and checks if the ids are the same. If yes, it stops. Otherwise go to 2

    //Case B: both parties are signaling reset at the same time. They both send different ids
    //  When a party receives the reset signal, based on the session id it does the following:
    //1. If the received session id is the same, it stops signalling. The reset is done
    //2. If the received session id is smaller the it continues signalling its own id
    //3. If the received session id is bigger then it adopts it and stops spamming the reset signal.


    m_send_reset_request = true;
    m_session_id = rand();
}

void RC_Protocol::reset_session_data()
{
    m_received_reliable_packet_index = Reliable_Header::INVALID_PACKET_INDEX;
}

bool RC_Protocol::compute_tx_data(RC_Phy::Buffer& buffer, bool& more_data)
{
    Clock::time_point now = Clock::now();

    size_t offset = buffer.size();

    bool packet_ready = false;

    //RESET -------------------------------------------------------
    if (m_send_reset_request || m_send_reset_reply)
    {
        reset_session_data();

        buffer.resize(sizeof(Reset_Header) + offset);
        Reset_Header& header = *reinterpret_cast<Reset_Header*>(buffer.data() + offset);
        header.header_type = Header::RESET;

        header.packet_type = m_send_reset_request == true ? Header::RESET_REQ_PACKET : Header::RESET_RES_PACKET;
        header.session_id = m_session_id;

        m_send_reset_reply = false;
#ifdef LOG
        QLOGI("TX reset packet, size {}", buffer.size());
#endif

        packet_ready = true;
    }

    //PERIODIC -------------------------------------------------------
    if (!packet_ready)
    {
        buffer.resize(sizeof(Periodic_Header) + offset);

        std::lock_guard<std::mutex> lg(m_periodic_packets_mutex);
        for (Periodic_Packet& p: m_periodic_packets)
        {
            if (now - p.last_tp >= p.period)
            {
                p.last_tp = now;

                uint8_t packet_type = 0;
                if (p.tx_callback(buffer, packet_type))
                {
                    QASSERT(packet_type <= Header::MAX_PACKET_TYPE);
                    Periodic_Header& header = *reinterpret_cast<Periodic_Header*>(buffer.data() + offset);
                    header.header_type = Header::PERIODIC;
                    header.packet_type = packet_type;

#ifdef LOG
                    QLOGI("Send periodic packet type {}, size {}", packet_type, size);
                    QLOGI("TX periodic packet type {}, size {}", packet_type, buffer.size());
#endif
                    packet_ready = true;
                    break;
                }
            }
        }
        if (!packet_ready)
        {
            buffer.resize(sizeof(Header) + offset);
        }
    }

    //RELIABLE -------------------------------------------------------
    if (!packet_ready)
    {
        std::lock_guard<std::mutex> lg (m_reliable_packet_queue_mutex);
        if (!m_reliable_packet_queue.empty())
        {
            //to give the other end a chance to confirm the packet
            //more_data = false;

            Reliable_Packet const& packet = m_reliable_packet_queue.front();
            size_t payload_size_left = packet.payload.size() - packet.offset;
            size_t size = std::min(payload_size_left, m_phy.get_mtu() - sizeof(Reliable_Header));

            buffer.resize(sizeof(Reliable_Header) + offset + size);
            Reliable_Header& header = *reinterpret_cast<Reliable_Header*>(buffer.data() + offset);
            header.header_type = Header::RELIABLE;
            if (size > 0)
            {
                memcpy(buffer.data() + offset + sizeof(Reliable_Header), packet.payload.data() + packet.offset, size);
            }

            header.packet_type = packet.packet_type;
            header.packet_index = m_crt_sent_reliable_packet_index;
            header.last_packet = (size == payload_size_left);

#ifdef LOG
            QLOGI("Sent fragment {} - {} out of {}KB", packet.offset, packet.offset + size, packet.payload.size());
            QLOGI("TX reliable packet type {}, index {}{}, size {}", packet.packet_type, m_crt_sent_reliable_packet_index, header.last_packet ? " last" : "", buffer.size());
#endif
            packet_ready = true;
        }
    }

    //FEC -------------------------------------------------------
    if (!packet_ready)
    {
        std::unique_ptr<FEC_Extra_Buffer> fec_buffer = m_tx_fec_extra_queue.begin_consuming(false);
        if (fec_buffer)
        {
            buffer.resize(sizeof(Periodic_Header) + offset + fec_buffer->data.size());
            FEC_Header& header = *reinterpret_cast<FEC_Header*>(buffer.data() + offset);
            header.header_type = Header::FEC;
            header.packet_type = FEC_Header::FEC_EXTRA;
            header.block_index = m_fec_last_block_index;
            header.fec_index = fec_buffer->fec_index;
            memcpy(buffer.data() + offset + sizeof(FEC_Header), fec_buffer->data.data(), fec_buffer->data.size());

            fec_buffer->data.clear();
            m_tx_fec_extra_queue.end_consuming(std::move(fec_buffer));
        }
        else
        {
            std::unique_ptr<std::vector<uint8_t>> fec_buffer = m_tx_fec_source_queue.begin_consuming(false);
            if (fec_buffer)
            {
                buffer.resize(sizeof(Periodic_Header) + offset + fec_buffer->size());
                FEC_Header& header = *reinterpret_cast<FEC_Header*>(buffer.data() + offset);
                header.header_type = Header::FEC;
                header.packet_type = FEC_Header::FEC_SOURCE;
                header.block_index = m_fec_last_block_index;
                header.fec_index = m_fec_block_source_buffers.size();
                memcpy(buffer.data() + offset + sizeof(FEC_Header), fec_buffer->data(), fec_buffer->size());

                m_fec_block_source_buffers.push_back(std::move(fec_buffer));
            }
        }

        //compute fec datagrams
        if (m_fec_block_source_buffers.size() >= m_fec_coding_k)
        {
            //init data for the fec_encode
            for (size_t i = 0; i < m_fec_coding_k; i++)
            {
                m_fec_source_ptrs[i] = m_fec_block_source_buffers[i]->data();
            }

            //prepare the exta buffers and store the pointers
            size_t fec_extra_count = m_fec_coding_n - m_fec_coding_k;
            m_fec_block_extra_buffers.resize(fec_extra_count);
            for (size_t i = 0; i < fec_extra_count; i++)
            {
                m_fec_block_extra_buffers[i] = m_tx_fec_extra_queue.begin_producing();
                m_fec_block_extra_buffers[i]->data.resize(m_fec_buffer_size);
                m_fec_extra_ptrs[i] = m_fec_block_extra_buffers[i]->data.data();
            }

            //encode
            fec_encode(m_fec, m_fec_source_ptrs.data(), m_fec_extra_ptrs.data(), BLOCK_NUMS + m_fec_coding_k, m_fec_coding_n - m_fec_coding_k, m_fec_buffer_size);

            //enqueue the extra buffers
            for (size_t i = 0; i < fec_extra_count; i++)
            {
                std::unique_ptr<FEC_Extra_Buffer>& fec_buffer = m_fec_block_extra_buffers[i];
                fec_buffer->fec_index = m_fec_coding_k + i;
                m_tx_fec_extra_queue.end_producing(std::move(fec_buffer), false);
            }
            m_fec_block_extra_buffers.clear();

            //release the source buffers
            for (size_t i = 0; i < m_fec_block_source_buffers.size(); i++)
            {
                m_tx_fec_source_queue.end_consuming(std::move(m_fec_block_source_buffers[i]));
            }
            m_fec_block_source_buffers.clear();

            m_fec_last_block_index++;
        }
    }

    //EMPTY -------------------------------------------------------
    if (!packet_ready && m_empty_packet_needed)
    {
        m_empty_packet_needed = false;
        buffer.resize(sizeof(Periodic_Header) + offset);
        Periodic_Header& header = *reinterpret_cast<Periodic_Header*>(buffer.data() + offset);
        header.header_type = Header::PERIODIC;
        header.packet_type = Header::EMPTY_PACKET;

#ifdef LOG
        QLOGI("TX empty packet, size {}", buffer.size());
#endif

        packet_ready = true;
    }

    //Seal the packet -------------------------------------------------------
    if (packet_ready)
    {
        Header& header = *reinterpret_cast<Header*>(buffer.data() + offset);
        header.last_received_reliable_packet_index = m_received_reliable_packet_index;

        header.crc = 0;
        uint16_t computed_crc = q::util::compute_murmur_hash16(buffer.data() + offset, buffer.size() - offset);
        header.crc = computed_crc;
    }

    return packet_ready;
}

void RC_Protocol::process_rx_data(util::comms::RC_Phy::RX_Data const& rx_data, RC_Phy::Buffer& buffer)
{
    QASSERT(buffer.size() >= sizeof(Header));
    Header& header = *reinterpret_cast<Header*>(buffer.data());
    //uint8_t const* payload_ptr = buffer.data() + sizeof(Header);

    uint16_t crc = header.crc;
    header.crc = 0;
    uint16_t computed_crc = q::util::compute_murmur_hash16(buffer.data(), buffer.size());
    if (crc != computed_crc)
    {
        QLOGW("Invalid crc. Expected {}, got {}", crc, computed_crc);
        return;
    }

    //handle the reset handshake!
    if (header.header_type == Header::RESET)
    {
#ifdef LOG
        QLOGI("RX reset packet, size {}", buffer.size());
#endif
        reset_session_data();

        Reset_Header const& header = *reinterpret_cast<Reset_Header const*>(buffer.data());
        uint32_t remote_session_id = header.session_id;
        if (header.packet_type == Header::RESET_REQ_PACKET)
        {
            //if I was resetting also, sync session ids
            if (m_send_reset_request)
            {
                //we have the same session? then the reset is complete
                if (remote_session_id >= m_session_id)
                {
                    m_send_reset_request = false; //stop spamming if the other party has the bigger id
                    m_session_id = remote_session_id;

                    QLOGI("RC Session synchronized, session id: {}", m_session_id);
                }
            }
            else
            {
                //if I was not resetting, take the others session id so they know we are in sync
                m_session_id = remote_session_id;
            }

            //respond to the request
            m_send_reset_reply = true;
        }
        else
        {
            //if I was resetting also, sync session ids
            if (m_send_reset_request)
            {
                //stop spamming only when the other party is replying correctly
                if (m_session_id == remote_session_id)
                {
                    m_send_reset_request = false;
                    QLOGI("RC Session synchronized, session id: {}", m_session_id);
                }
            }
        }

        return;
    }

    //ignore all packets until the reset is confirmed
    if (m_send_reset_request)
    {
        return;
    }

    //confirm and advance the packet queue
    {
        std::lock_guard<std::mutex> lg(m_reliable_packet_queue_mutex);
        if (!m_reliable_packet_queue.empty())
        {
            if (header.last_received_reliable_packet_index != Reliable_Header::INVALID_PACKET_INDEX &&
                header.last_received_reliable_packet_index == m_crt_sent_reliable_packet_index)
            {
                m_crt_sent_reliable_packet_index++;
                if (m_crt_sent_reliable_packet_index > Reliable_Header::MAX_PACKET_INDEX)
                {
                    m_crt_sent_reliable_packet_index = 0;
                }

                Reliable_Packet& packet = m_reliable_packet_queue.front();
                size_t size = std::min(packet.payload.size() - packet.offset, m_phy.get_mtu() - sizeof(Reliable_Header));
                packet.offset += size;
                if (packet.offset >= packet.payload.size())
                {
#ifdef LOG
                    QLOGI("Confirmed packet size {}KB. {} packets pending", packet.payload.size(), m_reliable_packet_queue.size());
#endif
                    m_reliable_packet_queue.pop_front();
                }
                else
                {
#ifdef LOG
                    QLOGI("Confirmed fragment {} - {} out of {}KB", packet.offset - size, packet.offset, packet.payload.size());
#endif
                }
            }
        }
    }

    if (header.header_type == Header::RELIABLE)
    {
        m_empty_packet_needed = true;

        Reliable_Header& header = *reinterpret_cast<Reliable_Header*>(buffer.data());
#ifdef LOG
        QLOGI("RX reliable packet type {}, index {}{}, size {}", (int)header.packet_type, (int)header.packet_index, header.last_packet ? " last" : "", buffer.size());
#endif
        if (header.packet_index != m_received_reliable_packet_index)
        {
            std::lock_guard<std::mutex> lg(m_incoming_packet_mutex);

            size_t offset = m_incoming_reliable_packet.payload.size();
            size_t size = buffer.size() - sizeof(Reliable_Header);
            if (size > 0)
            {
                m_incoming_reliable_packet.payload.resize(offset + size);
                memcpy(m_incoming_reliable_packet.payload.data() + offset, buffer.data() + sizeof(Reliable_Header), size);
            }

            m_incoming_reliable_packet.packet_type = header.packet_type;
            if (header.last_packet == 1)
            {
                m_incoming_reliable_packet.rx_dBm = rx_data.rx_dBm;
                m_incoming_reliable_packet.tx_dBm = rx_data.tx_dBm;
                m_incoming_reliable_packet.rx_timepoint = rx_data.rx_timepoint;
                m_rx_callback(m_incoming_reliable_packet);
                m_incoming_reliable_packet.payload.clear();
                m_incoming_reliable_packet.packet_type = static_cast<uint8_t>(-1);
            }

            m_received_reliable_packet_index = header.packet_index;
        }
    }
    if (header.header_type == Header::PERIODIC)
    {
        Periodic_Header const& header = *reinterpret_cast<Periodic_Header const*>(buffer.data());
        if (header.packet_type != Header::EMPTY_PACKET)
        {
#ifdef LOG
            QLOGI("RX periodic packet type {}, size {}", (int)header.packet_type, buffer.size());
#endif
            std::lock_guard<std::mutex> lg(m_incoming_packet_mutex);

            size_t offset = m_incoming_periodic_packet.payload.size();
            size_t size = buffer.size() - sizeof(Periodic_Header);
            if (size > 0)
            {
                m_incoming_periodic_packet.payload.resize(offset + size);
                memcpy(m_incoming_periodic_packet.payload.data() + offset, buffer.data() + sizeof(Periodic_Header), size);
            }

            m_incoming_periodic_packet.packet_type = header.packet_type;
            m_incoming_periodic_packet.rx_dBm = rx_data.rx_dBm;
            m_incoming_periodic_packet.tx_dBm = rx_data.tx_dBm;
            m_incoming_periodic_packet.rx_timepoint = rx_data.rx_timepoint;
            m_rx_callback(m_incoming_periodic_packet);
            m_incoming_periodic_packet.payload.clear();
            m_incoming_periodic_packet.packet_type = static_cast<uint8_t>(-1);
        }
        else
        {
#ifdef LOG
            QLOGI("RX empty, size {}", (int)header.packet_type, buffer.size());
#endif
        }
    }
}

void RC_Protocol::process()
{

}

}
}

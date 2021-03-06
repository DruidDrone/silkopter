#include "FCStdAfx.h"
#include "SRF01.h"
#include "physics/constants.h"
#include "utils/Timed_Scope.h"

#include "hal.def.h"

namespace silk
{
namespace node
{


constexpr uint8_t ADDR = 0x01;

constexpr uint8_t SW_REV                = 0x5D;
constexpr uint8_t REAL_RANGING_CM_TX    = 0x54;
constexpr uint8_t SET_ADVANCED_MODE     = 0x62;
constexpr uint8_t CLEAR_ADVANCED_MODE   = 0x63;



constexpr std::chrono::milliseconds MAX_MEASUREMENT_DURATION(75);


SRF01::SRF01(HAL& hal)
    : m_hal(hal)
    , m_descriptor(new hal::SRF01_Descriptor())
    , m_config(new hal::SRF01_Config())
{
    m_config->set_direction(math::vec3f(0, 0, -1)); //pointing down

    m_output_stream = std::make_shared<Output_Stream>();
}

auto SRF01::get_outputs() const -> std::vector<Output>
{
    std::vector<Output> outputs(1);
    outputs[0].name = "distance";
    outputs[0].stream = m_output_stream;
    return outputs;
}
ts::Result<void> SRF01::init(hal::INode_Descriptor const& descriptor)
{
    QLOG_TOPIC("SRF01::init");

    auto specialized = dynamic_cast<hal::SRF01_Descriptor const*>(&descriptor);
    if (!specialized)
    {
        return make_error("Wrong descriptor type");
    }
    *m_descriptor = *specialized;

    return init();
}

ts::Result<void> SRF01::init()
{
    m_uart_bus = m_hal.get_bus_registry().find_by_name<bus::IUART_Bus>(m_descriptor->get_bus());
    auto bus = m_uart_bus.lock();
    if (!bus)
    {
        return make_error("No bus configured");
    }

    util::hw::IUART& uart = bus->get_uart();

    QLOGI("Probing SRF01 on {}...", m_descriptor->get_bus());

    uint32_t tries = 0;
    constexpr uint32_t max_tries = 10;
    while (++tries <= max_tries)
    {
        bool ok = send_command(uart, SW_REV);

        uint8_t rev = 0;
        ok &= read_response_u8(uart, SW_REV, rev);
        if (ok && rev != 0 && rev != 255)
        {
            QLOGI("Found SRF01 rev {} after {} tries", rev, tries);
            break;
        }
        QLOGW("\tFailed {} try to initialize SRF01: bus {}, rev {}", tries, ok, rev);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }

    if (tries > max_tries)
    {
        return make_error("Failed to initialize SRF01");
    }

    //clear advanced mode
    //send_command(*bus, CLEAR_ADVANCED_MODE);
    send_command(uart, SET_ADVANCED_MODE);

    trigger(uart);

    m_output_stream->set_rate(m_descriptor->get_rate());

    return ts::success;
}

ts::Result<void> SRF01::start(Clock::time_point tp)
{
    m_last_trigger_tp = tp;
    m_output_stream->set_tp(tp);
    return ts::success;
}

auto SRF01::send_command(util::hw::IUART& uart, uint8_t command) -> bool
{
    std::array<uint8_t, 2> buf;
    buf[0] = ADDR;											//SRF01 address
    buf[1] = command;

    uart.send_break();
    return uart.write(buf.data(), buf.size());
}

auto SRF01::read_response(util::hw::IUART& uart, uint8_t sent_command, uint8_t* response, size_t size) -> bool
{
    QASSERT(response && size > 0);
    std::array<uint8_t, 32> buf;
    size_t count = uart.read(buf.data(), buf.size());
    if (count > 0)
    {
        std::copy(buf.begin(), buf.begin() + count, std::back_inserter(m_read_data));

        //consume the echo of what we sent
        while (m_read_data.size() > 2 && (m_read_data[0] != ADDR || m_read_data[1] != sent_command))
        {
            m_read_data.pop_front();
        }

        if (m_read_data.size() >= 2 + size && m_read_data[0] == ADDR && m_read_data[1] == sent_command)
        {
            std::copy(m_read_data.begin() + 2, m_read_data.begin() + 2 + size, response);
            m_read_data.clear();
            return true;
        }
    }

    return false;
}
auto SRF01::read_response_u8(util::hw::IUART& uart, uint8_t sent_command, uint8_t& response) -> bool
{
    return read_response(uart, sent_command, &response, 1);
}
auto SRF01::read_response_u16(util::hw::IUART& uart, uint8_t sent_command, uint16_t& response) -> bool
{
    uint8_t res[2];
    if (read_response(uart, sent_command, res, 2))
    {
        response = (uint16_t)(res[0] << 8) | res[1];
        return true;
    }
    return false;
}


void SRF01::trigger(util::hw::IUART& uart)
{
    m_last_trigger_tp = Clock::now();
    send_command(uart, REAL_RANGING_CM_TX);
}

void SRF01::process()
{
    QLOG_TOPIC("SRF01::process");

    m_output_stream->clear();

    //wait for echo
    auto now = Clock::now();
    if (now - m_last_trigger_tp < MAX_MEASUREMENT_DURATION ||
        now - m_last_trigger_tp < m_output_stream->get_dt())
    {
        return;
    }

    auto bus = m_uart_bus.lock();
    if (!bus)
    {
        return;
    }
    util::hw::IUART& uart = bus->get_uart();

    //TODO - add health indication


    //the trigger data acts as a header. Following this header there is the actual measurement
    uint16_t d = 0;
    if (read_response_u16(uart, REAL_RANGING_CM_TX, d))
    {
        float distance = static_cast<float>(d) / 100.f; //meters

        float min_distance = m_config->get_min_distance();//math::max(m_config->min_distance, static_cast<float>(min_d) / 100.f); //meters
        float max_distance = m_config->get_max_distance();
        auto value = m_config->get_direction() * math::clamp(distance, min_distance, max_distance);
        auto is_healthy = distance >= min_distance && distance <= max_distance;

        m_output_stream->clear();
        auto samples_needed = m_output_stream->compute_samples_needed();
        while (samples_needed > 0)
        {
            m_output_stream->push_sample(value, is_healthy);
            samples_needed--;
        }

        trigger(uart);
    }
    else
    {
        if (now - m_last_trigger_tp >= std::chrono::milliseconds(500))
        {
            QLOGI("Timeout. triggering again");
            trigger(uart);
        }
    }
}

ts::Result<void> SRF01::set_config(hal::INode_Config const& config)
{
    QLOG_TOPIC("SRF01::set_config");

    auto specialized = dynamic_cast<hal::SRF01_Config const*>(&config);
    if (!specialized)
    {
        return make_error("Wrong config type");
    }
    *m_config = *specialized;

//    m_config->min_distance = math::max(m_config->min_distance, 0.1f);
//    m_config->max_distance = math::min(m_config->max_distance, 12.f);
    if (math::is_zero(math::length(m_config->get_direction()), math::epsilon<float>()))
    {
        m_config->set_direction(math::vec3f(0, 0, -1)); //pointing down
    }
    m_config->set_direction(math::normalized(m_config->get_direction()));

    return ts::success;
}
auto SRF01::get_config() const -> std::shared_ptr<const hal::INode_Config>
{
    return m_config;
}

auto SRF01::get_descriptor() const -> std::shared_ptr<const hal::INode_Descriptor>
{
    return m_descriptor;
}

ts::Result<std::shared_ptr<messages::INode_Message>> SRF01::send_message(messages::INode_Message const& message)
{
    return make_error("Unknown message");
}

}
}

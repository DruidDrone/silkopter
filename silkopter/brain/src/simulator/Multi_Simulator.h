#pragma once

#include "HAL.h"
#include "common/stream/IAngular_Velocity.h"
#include "common/stream/IAcceleration.h"
#include "common/stream/IMagnetic_Field.h"
#include "common/stream/IPressure.h"
#include "common/stream/ITemperature.h"
#include "common/stream/IDistance.h"
#include "common/stream/IPosition.h"
#include "common/stream/IGPS_Info.h"
#include "common/stream/IVelocity.h"
#include "common/stream/IThrottle.h"
#include "common/node/IMulti_Simulator.h"

#include "Multi_Simulation.h"


namespace sz
{
namespace Multi_Simulator
{
struct Init_Params;
struct Config;
}
}

namespace silk
{
namespace node
{

class Multi_Simulator : public IMulti_Simulator
{
public:
    Multi_Simulator(HAL& hal);

    auto init(rapidjson::Value const& init_params) -> bool;
    auto get_init_params() const -> rapidjson::Document;

    auto set_config(rapidjson::Value const& json) -> bool;
    auto get_config() const -> rapidjson::Document;

    auto send_message(rapidjson::Value const& json) -> rapidjson::Document;

    void set_input_stream_path(size_t idx, q::Path const& path);
    auto get_inputs() const -> std::vector<Input>;
    auto get_outputs() const -> std::vector<Output>;

    void process();

private:
    auto init() -> bool;

    HAL& m_hal;

    std::shared_ptr<sz::Multi_Simulator::Init_Params> m_init_params;
    std::shared_ptr<sz::Multi_Simulator::Config> m_config;

    q::Clock::time_point m_last_tp;

    std::default_random_engine m_generator;
    std::normal_distribution<double> m_ecef_position_dist;
    std::normal_distribution<double> m_ecef_velocity_dist;
    std::normal_distribution<double> m_ecef_pacc_dist;
    std::normal_distribution<double> m_ecef_vacc_dist;

    struct Angular_Velocity : public stream::IAngular_Velocity
    {
        auto get_samples() const -> std::vector<Sample> const& { return samples; }
        auto get_rate() const -> uint32_t { return rate; }
        uint32_t rate = 0;
        q::Clock::duration accumulated_dt = q::Clock::duration{0};
        q::Clock::duration dt = q::Clock::duration{0};
        std::vector<Sample> samples;
        Sample last_sample;
    };
    struct Acceleration : public stream::IAcceleration
    {
        auto get_samples() const -> std::vector<Sample> const& { return samples; }
        auto get_rate() const -> uint32_t { return rate; }
        uint32_t rate = 0;
        q::Clock::duration accumulated_dt = q::Clock::duration{0};
        q::Clock::duration dt = q::Clock::duration{0};
        std::vector<Sample> samples;
        Sample last_sample;
    };
    struct Magnetic_Field : public stream::IMagnetic_Field
    {
        auto get_samples() const -> std::vector<Sample> const& { return samples; }
        auto get_rate() const -> uint32_t { return rate; }
        uint32_t rate = 0;
        q::Clock::duration accumulated_dt = q::Clock::duration{0};
        q::Clock::duration dt = q::Clock::duration{0};
        std::vector<Sample> samples;
        Sample last_sample;
    };
    struct Pressure : public stream::IPressure
    {
        auto get_samples() const -> std::vector<Sample> const& { return samples; }
        auto get_rate() const -> uint32_t { return rate; }
        uint32_t rate = 0;
        q::Clock::duration accumulated_dt = q::Clock::duration{0};
        q::Clock::duration dt = q::Clock::duration{0};
        std::vector<Sample> samples;
        Sample last_sample;
    };
    struct Temperature : public stream::ITemperature
    {
        auto get_samples() const -> std::vector<Sample> const& { return samples; }
        auto get_rate() const -> uint32_t { return rate; }
        uint32_t rate = 0;
        q::Clock::duration accumulated_dt = q::Clock::duration{0};
        q::Clock::duration dt = q::Clock::duration{0};
        std::vector<Sample> samples;
        Sample last_sample;
    };
    struct Distance : public stream::IDistance
    {
        auto get_samples() const -> std::vector<Sample> const& { return samples; }
        auto get_rate() const -> uint32_t { return rate; }
        uint32_t rate = 0;
        q::Clock::duration accumulated_dt = q::Clock::duration{0};
        q::Clock::duration dt = q::Clock::duration{0};
        std::vector<Sample> samples;
        Sample last_sample;
    };
    struct GPS_Info : public stream::IGPS_Info
    {
        auto get_samples() const -> std::vector<Sample> const& { return samples; }
        auto get_rate() const -> uint32_t { return rate; }
        uint32_t rate = 0;
        q::Clock::duration accumulated_dt = q::Clock::duration{0};
        q::Clock::duration dt = q::Clock::duration{0};
        std::vector<Sample> samples;
        Sample last_sample;
    };
    struct ECEF_Position : public stream::IECEF_Position
    {
        auto get_samples() const -> std::vector<Sample> const& { return samples; }
        auto get_rate() const -> uint32_t { return rate; }
        uint32_t rate = 0;
        q::Clock::duration accumulated_dt = q::Clock::duration{0};
        q::Clock::duration dt = q::Clock::duration{0};
        std::vector<Sample> samples;
        Sample last_sample;
    };
    struct ECEF_Velocity : public stream::IECEF_Velocity
    {
        auto get_samples() const -> std::vector<Sample> const& { return samples; }
        auto get_rate() const -> uint32_t { return rate; }
        uint32_t rate = 0;
        q::Clock::duration accumulated_dt = q::Clock::duration{0};
        q::Clock::duration dt = q::Clock::duration{0};
        std::vector<Sample> samples;
        Sample last_sample;
    };

    mutable std::shared_ptr<Angular_Velocity> m_angular_velocity_stream;
    mutable std::shared_ptr<Acceleration> m_acceleration_stream;
    mutable std::shared_ptr<Magnetic_Field> m_magnetic_field_stream;
    mutable std::shared_ptr<Pressure> m_pressure_stream;
    mutable std::shared_ptr<Temperature> m_temperature_stream;
    mutable std::shared_ptr<Distance> m_distance_stream;
    mutable std::shared_ptr<GPS_Info> m_gps_info_stream;
    mutable std::shared_ptr<ECEF_Position> m_ecef_position_stream;
    mutable std::shared_ptr<ECEF_Velocity> m_ecef_velocity_stream;

    std::vector<stream::IThrottle_wptr> m_input_throttle_streams;
    std::vector<q::Path> m_input_throttle_stream_paths;

    Multi_Simulation m_simulation;
};


}
}


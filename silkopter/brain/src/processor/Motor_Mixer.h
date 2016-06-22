#pragma once

#include "common/node/IProcessor.h"
#include "common/stream/ITorque.h"
#include "common/stream/IFloat.h"
#include "common/stream/IThrottle.h"
#include "Multirotor_Properties.h"

#include "UAV.h"

#include "Sample_Accumulator.h"


namespace silk
{
namespace uav
{
struct Motor_Mixer_Descriptor;
struct Motor_Mixer_Config;
}
}


namespace silk
{
namespace node
{

class Motor_Mixer : public IProcessor
{
public:
    Motor_Mixer(UAV& uav);

    bool init(uav::INode_Descriptor const& descriptor) override;
    std::shared_ptr<const uav::INode_Descriptor> get_descriptor() const override;

    bool set_config(uav::INode_Config const& config) override;
    std::shared_ptr<const uav::INode_Config> get_config() const override;

    //auto send_message(rapidjson::Value const& json) -> rapidjson::Document;

    auto start(q::Clock::time_point tp) -> bool override;

    void set_input_stream_path(size_t idx, q::Path const& path);
    auto get_inputs() const -> std::vector<Input>;
    auto get_outputs() const -> std::vector<Output>;

    auto get_cell_count() const -> boost::optional<uint8_t>;

    void process();

private:
    auto init() -> bool;

    void compute_throttles(Multirotor_Properties const& multirotor_properties, stream::IFloat::Value const& collective_thrust, stream::ITorque::Value const& torque);


    UAV& m_uav;

    std::shared_ptr<uav::Motor_Mixer_Descriptor> m_descriptor;
    std::shared_ptr<uav::Motor_Mixer_Config> m_config;

    Sample_Accumulator<stream::ITorque, stream::IFloat> m_accumulator;

    struct Stream : public stream::IThrottle
    {
        auto get_samples() const -> std::vector<Sample> const& { return samples; }
        auto get_rate() const -> uint32_t { return rate; }

        uint32_t rate = 0;
        Sample last_sample;
        std::vector<Sample> samples;

        struct Config
        {
            math::vec3f position;
            math::vec3f max_torque;
            math::vec3f torque_vector;
        } config;

        float throttle = 0;
        float thrust = 0;
        math::vec3f torque;
    };

    mutable std::vector<std::shared_ptr<Stream>> m_outputs;
};


}
}

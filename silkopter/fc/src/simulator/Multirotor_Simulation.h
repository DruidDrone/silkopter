#pragma once

#include "common/node/IMultirotor_Simulator.h"
#include "common/stream/IMultirotor_Simulator_State.h"
#include "uav_properties/IMultirotor_Properties.h"
#include "utils/Clock.h"
#include "messages.def.h"

class btCylinderShapeZ;
class btMotionState;
class btRigidBody;
class btDefaultCollisionConfiguration;
class btCollisionDispatcher;
class btDbvtBroadphase;
class btSequentialImpulseConstraintSolver;
class btCollisionShape;
class btRigidBody;
class btDiscreteDynamicsWorld;

namespace silk
{
namespace node
{

class Multirotor_Simulation : q::util::Noncopyable
{
public:

    Multirotor_Simulation();
    ~Multirotor_Simulation();

    auto init(uint32_t rate) -> bool;

    auto init_uav(std::shared_ptr<const IMultirotor_Properties> multirotor_properties) -> bool;

    void reset();
    void stop_motion();

    void process(Clock::duration dt, std::function<void(Multirotor_Simulation&, Clock::duration)> const& callback);

    void set_gravity_enabled(bool yes);
    void set_ground_enabled(bool yes);
    void set_simulation_enabled(bool yes);
    void set_drag_enabled(bool yes);

    typedef stream::IMultirotor_Simulator_State::Value State;

    State const& get_state() const;

    void set_motor_throttle(size_t motor, float throttle);

private:
    void process_world(Clock::duration dt);

    bool m_is_simulation_enabled = true;
    bool m_is_ground_enabled = true;
    bool m_is_gravity_enabled = true;
    bool m_is_drag_enabled = true;

    uint32_t m_rate = 0;
    Clock::duration m_dt;

    struct UAV
    {
        std::shared_ptr<const IMultirotor_Properties> properties;
        std::shared_ptr<btCylinderShapeZ> shape;
        std::shared_ptr<btMotionState> motion_state;
        std::shared_ptr<btRigidBody> body;
        State state;
        std::vector<float> motor_drag_factors;
        std::vector<float> motor_thrust_factors;
    } m_uav;

//    math::vec3f m_old_linear_velocity;
//    math::quatf m_old_rotation;

//    silk::Accelerometer_Sample m_accelerometer_sample;
//    Clock::time_point m_last_accelerometer_time_point;

//    silk::Gyroscope_Sample m_gyroscope_sample;
//    Clock::time_point m_last_gyroscope_time_point;

//    silk::Compass_Sample m_compass_sample;
//    Clock::time_point m_last_compass_time_point;

//    silk::Barometer_Sample m_barometer_sample;
//    Clock::time_point m_last_barometer_time_point;

//    silk::Thermometer_Sample m_thermometer_sample;
//    Clock::time_point m_last_thermometer_time_point;

//    silk::Sonar_Sample m_sonar_sample;
//    Clock::time_point m_last_sonar_time_point;

//    silk::GPS_Sample m_gps_sample;
//    Clock::time_point m_last_gps_time_point;

    std::shared_ptr<btDefaultCollisionConfiguration> m_collision_configuration;
    std::shared_ptr<btCollisionDispatcher> m_dispatcher;
    std::shared_ptr<btDbvtBroadphase> m_broadphase;
    std::shared_ptr<btSequentialImpulseConstraintSolver> m_solver;

    std::shared_ptr<btCollisionShape> m_ground_shape;
    std::shared_ptr<btRigidBody> m_ground_body;

    std::shared_ptr<btDiscreteDynamicsWorld> m_world;


    Clock::duration m_accumulated_duration{0};
    Clock::duration m_accumulated_physics_duration{0};

    uint32_t m_physics_rate = 0; //hz
    Clock::duration m_physics_dt;

    void process_uav(Clock::duration dt);
    struct
    {
        math::vec3f enu_velocity;
        math::vec3f prev_enu_velocity;
        math::vec3f enu_linear_acceleration;
        math::vec3f angular_velocity;
    } m_sensor_data;

    void process_uav_sensors(Clock::duration dt);
    void process_air_drag(Clock::duration dt);
};


}
}

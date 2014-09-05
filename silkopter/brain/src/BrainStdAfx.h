#include <thread>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/program_options.hpp>
#include <boost/optional.hpp>

#include "QBase.h"
#include "QData.h"
#include "qmath.h"

#include "utils/PID.h"
#include "utils/Serial_Channel.h"
#include "utils/Channel.h"

#include "rapidjson/rapidjson.h"
#include "rapidjson/document.h"
#include "rapidjson/stringbuffer.h"
#include "rapidjson/prettywriter.h"

#define SILK_DBG(fmt, ...)  QLOG_DBG("brain", fmt, ##__VA_ARGS__)
#define SILK_INFO(fmt, ...)  QLOG_INFO("brain", fmt, ##__VA_ARGS__)
#define SILK_WARNING(fmt, ...)  QLOG_WARNING("brain", fmt, ##__VA_ARGS__)
#define SILK_ERR(fmt, ...)  QLOG_ERR("brain", fmt, ##__VA_ARGS__)


namespace std
{
    template<typename T, typename ...Args>
    std::unique_ptr<T> make_unique( Args&& ...args )
    {
        return std::unique_ptr<T>( new T( std::forward<Args>(args)... ) );
    }
}
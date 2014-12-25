#pragma once

namespace silk
{

class i2c : public q::util::Noncopyable
{
public:
    ~i2c();

    auto open(std::string const& device) -> bool;
    void close();

    bool read(uint8_t address, uint8_t reg, uint8_t* data, uint32_t size);
    auto read_u16(uint8_t address, uint8_t reg) -> uint16_t
    {
        uint8_t val[2];
        uint16_t t = 0;
        if (read(address, reg, val, 2))
        {
            reinterpret_cast<uint8_t*>(&t)[1] = val[0];
            reinterpret_cast<uint8_t*>(&t)[0] = val[1];
        }
        return t;
    }
    auto read_u8(uint8_t address, uint8_t reg) -> uint8_t
    {
        uint8_t t = 0;
        if (read(address, reg, &t, 1))
        {
            return t;
        }
        return 0;
    }


    bool write(uint8_t address, uint8_t reg, uint8_t const* data, uint32_t size);
    bool write(uint8_t address, uint8_t reg)
    {
        return write(address, reg, nullptr, 0);
    }
    bool write_u8(uint8_t address, uint8_t reg, uint8_t const& t)
    {
        return write(address, reg, &t, 1);
    }
    bool write_u16(uint8_t address, uint8_t reg, uint16_t const& t)
    {
        uint8_t val[2];
        val[0] = reinterpret_cast<uint8_t const*>(&t)[1];
        val[1] = reinterpret_cast<uint8_t const*>(&t)[0];
        return write(address, reg, val, 2);
    }
private:
    

    std::string m_device;
    int m_fd = -1;
    std::vector<uint8_t> m_buffer;
};

}
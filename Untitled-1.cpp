#include <iostream>
#include <cstdint>

uint8_t crc8(const uint8_t *data, size_t length, uint8_t initial_value = 0xFF)
{
    uint8_t crc = initial_value;

    for (size_t i = 0; i < length; ++i)
    {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j)
        {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }

    return crc;
}

int main()
{
    uint8_t data[] = { 0x20, 0x70, 0x0a, 0x00, 0xff };
    size_t length = sizeof(data) / sizeof(data[0]);

    uint8_t checksum = crc8(data, length);
    std::cout << std::hex << static_cast<int>(checksum) << std::endl;

    return 0;
}

#include "test_buffpacket.h"

int test_buffpacket_number_data()
{
    //������������
    int ret = 0;
    std::string buffer;
    auto write_buffer = std::make_shared<CWriteBuffer>(&buffer);

    //������
    uint8 u8_data = 1;
    uint16 u16_data = 2;
    uint32 u32_data = 3;
    uint64 u64_data = 4;
    int8 n8_data = -1;
    int16 n16_data = -2;
    int32 n32_data = -3;
    int64 n64_data = -4;
    float32 f32_data = 3.14f;
    float64 f64_data = -3.14l;

    //��ȡ��
    uint8 u8_data_tag = 0;
    uint16 u16_data_tag = 0;
    uint32 u32_data_tag = 0;
    uint64 u64_data_tag = 0;
    int8 n8_data_tag = 0;
    int16 n16_data_tag = 0;
    int32 n32_data_tag = 0;
    int64 n64_data_tag = 0;
    float32 f32_data_tag = 0.0f;
    float64 f64_data_tag = 0.0l;

    (*write_buffer) << u8_data;
    (*write_buffer) << u16_data;
    (*write_buffer) << u32_data;
    (*write_buffer) << u64_data;
    (*write_buffer) << n8_data;
    (*write_buffer) << n16_data;
    (*write_buffer) << n32_data;
    (*write_buffer) << n64_data;
    (*write_buffer) << f32_data;
    (*write_buffer) << f64_data;


    auto read_buffer = std::make_shared<CReadBuffer>(&buffer);
    //�˶Բ��Խ��
    (*read_buffer) >> u8_data_tag;
    if (u8_data_tag != u8_data)
    {
        std::cout << "[test_buffpacket_number_data]u8_data_tag error." << std::endl;
        ret = 1;
    }
    (*read_buffer) >> u16_data_tag;
    if (u16_data_tag != u16_data)
    {
       std::cout << "[test_buffpacket_number_data]u16_data_tag error." << std::endl;
       ret = 1;
    }
    (*read_buffer) >> u32_data_tag;
    if (u32_data_tag != u32_data)
    {
        std::cout << "[test_buffpacket_number_data]u32_data_tag error." << std::endl;
        ret = 1;
    }
    (*read_buffer) >> u64_data_tag;
    if (u64_data_tag != u64_data)
    {
        std::cout << "[test_buffpacket_number_data]u64_data_tag error." << std::endl;
        ret = 1;
    }
    (*read_buffer) >> n8_data_tag;
    if (n8_data_tag != n8_data)
    {
        std::cout << "[test_buffpacket_number_data]n8_data_tag error." << std::endl;
        ret = 1;
    }
    (*read_buffer) >> n16_data_tag;
    if (n16_data_tag != n16_data)
    {
        std::cout << "[test_buffpacket_number_data]n16_data_tag error." << std::endl;
        ret = 1;
    }
    (*read_buffer) >> n32_data_tag;
    if (n32_data_tag != n32_data)
    {
        std::cout << "[test_buffpacket_number_data]n32_data_tag error." << std::endl;
        ret = 1;
    }
    (*read_buffer) >> n64_data_tag;
    if (n64_data_tag != n64_data)
    {
        std::cout << "[test_buffpacket_number_data]n64_data_tag error." << std::endl;
        ret = 1;
    }
    (*read_buffer) >> f32_data_tag;
    if (f32_data_tag != f32_data)
    {
        std::cout << "[test_buffpacket_number_data]f32_data_tag error." << std::endl;
        ret = 1;
    }
    (*read_buffer) >> f64_data_tag;
    if (f64_data_tag != f64_data)
    {
        std::cout << "[test_buffpacket_number_data]f64_data_tag error." << std::endl;
        ret = 1;
    }

    return ret;
}

int test_buffpacket_string_data()
{
    //�����ַ�������
    int ret = 0;
    std::string buffer;
    auto write_buffer = std::make_shared<CWriteBuffer>(&buffer);

    std::string string_data = "freeeyes";
    (*write_buffer) << string_data;

    std::string string_data_tag = "";
    auto read_buffer = std::make_shared<CReadBuffer>(&buffer);
    (*read_buffer) >> string_data_tag;
    if (string_data_tag != string_data)
    {
        std::cout << "[test_buffpacket_string_data]string_data_tag error." << std::endl;
        ret = 1;
    }

    return ret;
}

int test_offset_number_data()
{
    //����ƫ������
    int ret = 0;
    std::string buffer;
    auto write_buffer = std::make_shared<CWriteBuffer>(&buffer);

    //������
    uint8 u8_data = 1;
    uint16 u16_data = 2;
    uint32 u32_data = 3;
    uint64 u64_data = 4;

    (*write_buffer) << u8_data;
    (*write_buffer) << u16_data;
    (*write_buffer) << u32_data;
    (*write_buffer) << u64_data;


    auto read_buffer = std::make_shared<CReadBuffer>(&buffer);
    //����ƫ��ȡֵ
    read_buffer->read_offset(3);

    uint8 u8_data_tag = 0;
    uint32 u32_data_tag = 0;

    (*read_buffer) >> u32_data_tag;
    if (u32_data_tag != u32_data)
    {
        std::cout << "[test_offset_number_data]u32_data_tag error." << std::endl;
        ret = 1;
    }

    read_buffer->read_offset(-7);
    (*read_buffer) >> u8_data_tag;
    if (u8_data_tag != u8_data)
    {
        std::cout << "[test_offset_number_data]u8_data_tag error." << std::endl;
        ret = 1;
    }

    return ret;
}

int test_net_order_data()
{
    //���������ֽ����ת��
    int ret = 0;
    std::string buffer;
    auto write_buffer = std::make_shared<CWriteBuffer>(&buffer);
    write_buffer->set_net_sort(true);

    //������
    uint8 u8_data = 1;
    uint16 u16_data = 2;
    uint32 u32_data = 3;
    uint64 u64_data = 4;

    (*write_buffer) << u8_data;
    (*write_buffer) << u16_data;
    (*write_buffer) << u32_data;
    (*write_buffer) << u64_data;

    uint8 u8_data_tag = 0;
    uint16 u16_data_tag = 0;
    uint32 u32_data_tag = 0;
    uint64 u64_data_tag = 0;

    auto read_buffer = std::make_shared<CReadBuffer>(&buffer);
    read_buffer->set_net_sort(true);

    //�˶Բ��Խ��
    (*read_buffer) >> u8_data_tag;
    if (u8_data_tag != u8_data)
    {
        std::cout << "[test_buffpacket_number_data]u8_data_tag error." << std::endl;
        ret = 1;
    }
    (*read_buffer) >> u16_data_tag;
    if (u16_data_tag != u16_data)
    {
        std::cout << "[test_buffpacket_number_data]u16_data_tag error." << std::endl;
        ret = 1;
    }
    (*read_buffer) >> u32_data_tag;
    if (u32_data_tag != u32_data)
    {
        std::cout << "[test_buffpacket_number_data]u32_data_tag error." << std::endl;
        ret = 1;
    }
    (*read_buffer) >> u64_data_tag;
    if (u64_data_tag != u64_data)
    {
        std::cout << "[test_buffpacket_number_data]u64_data_tag error." << std::endl;
        ret = 1;
    }

    return ret;
}

int test_string_read_write()
{
    //�����ַ�����ֵ�Ͷ�ȡ
    int ret = 0;
    std::string buffer;

    std::string test_write_buffer = "freeeyes";
    auto write_buffer = std::make_shared<CWriteBuffer>(&buffer);

    write_buffer->write_data_from_string(test_write_buffer);

    auto read_buffer = std::make_shared<CReadBuffer>(&buffer);

    std::string test_read_buffer = "";

    read_buffer->read_data_to_string(test_read_buffer);

    if (test_read_buffer != test_write_buffer)
    {
        std::cout << "[test_string_read_write]test_read_buffer error." << std::endl;
        ret = 1;
    }

    return ret;
}


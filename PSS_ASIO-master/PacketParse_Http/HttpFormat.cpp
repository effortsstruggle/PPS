#include "HttpFormat.h"

void CHttpFormat::init_http_setting()
{
    http_settings_.on_url = sUrlCallback;
    http_settings_.on_header_field = sHeaderFieldCallback;
    http_settings_.on_status = sStatusHandle;
    http_settings_.on_body = sBodyHandle;
    http_settings_.on_header_value = sHeadValue;
    http_settings_.on_header_field = sHeadField;
    http_settings_.on_headers_complete = sHeadComplete;
    http_settings_.on_message_begin = sMessageBegin;
    http_settings_.on_message_complete = sMessageEnd;
    http_settings_.on_chunk_header = sChunkHeader;
    http_settings_.on_chunk_complete = sChunkComplete;

    http_text_buffer_.clear();

}

int CHttpFormat::try_parse(std::string http_text)
{
    //��һ���Ѿ���������ˣ����³�ʼ��
    if (true == http_text_buffer_.is_completed_)
    {
        http_text_buffer_.clear();
    }

    http_text_buffer_.http_request_text_ += http_text;

    http_parser_init(&http_parser_, HTTP_REQUEST);
    http_parser_.data = &http_text_buffer_;

    size_t nparsed = http_parser_execute(&http_parser_,
        &http_settings_, 
        http_text_buffer_.http_request_text_.c_str(), 
        http_text_buffer_.http_request_text_.length());
    if (http_parser_.upgrade)
    {
        //Э������
        std::cout << "parser->upgrade" << std::endl;
        return 0;
    }
    else if (nparsed != http_text_buffer_.http_request_text_.length())
    {
        http_text_buffer_.buffer_parse_pos_ = (int)nparsed;
        //std::cout << "parser->error(read:" << http_text.length() << ", parse:" << nparsed << ")" << std::endl;
        return -1;
    }

    if (is_websocket() == true)
    {
        //�����websocket����Э��
        return 0;
    }

    //�ж��Ƿ���post����Ϣ����
    //std::cout << "[CHttpFormat::try_parse]content_length_=" << http_text_buffer_.content_length_ << std::endl;
    if (http_text_buffer_.content_length_ > 0)
    {
        //�ж���Ϣ�����Ƿ��Ѿ���ȫ
        if ((int)http_text_buffer_.http_post_text_.length() < http_text_buffer_.content_length_)
        {
            //û����ȫ����Ҫ��������
            return 1;
        }
    }

    http_text_buffer_.is_completed_ = true;
    return 0;
}

std::string CHttpFormat::get_post_text()
{
    return http_text_buffer_.http_post_text_;
}

std::string CHttpFormat::get_post_error()
{
    if (http_text_buffer_.is_completed_ == true)
    {
        return "";
    }
    else
    {
        //���ش�����־
        std::string http_error = "";
        std::stringstream ss_format;
        ss_format << "[error]state=" 
            << http_text_buffer_.http_func_name_ 
            << ",position=" 
            << http_text_buffer_.buffer_parse_pos_;
        
        http_error = ss_format.str();
        return http_error;
    }
}

std::string CHttpFormat::get_websocket_client_key()
{
    return http_text_buffer_.websocket_key_;
}

std::string CHttpFormat::get_response_text(std::string data)
{
    std::stringstream ss_format;
    std::string original_data = "HTTP/1.1 200 OK\r\nContent-Type: application/json\r\nAccess-Control-Allow-Origin:*\r\nContent-Length:";

    ss_format << original_data << data.length() << "\r\n\r\n" << data;
    return ss_format.str();
}

std::string CHttpFormat::get_response_websocket_text(std::string data)
{
    std::stringstream ss_format;
    std::string original_data = "HTTP/1.1 101 Switching Protocols\r\nUpgrade: websocket\r\nConnection: Upgrade\r\nSec-WebSocket-Accept: ";

    ss_format << original_data << data << "\r\n\r\n";
    return ss_format.str();
}

bool CHttpFormat::is_websocket()
{
    //�ж��ǲ���websocketЭ������
    if (http_text_buffer_.upgrade_ == "websocket")
    {
        return true;
    }
    else
    {
        return false;
    }
}

std::string CHttpFormat::get_head_info(std::string field_name)
{
    transform(field_name.begin(), field_name.end(), field_name.begin(), ::tolower);

    auto f = http_text_buffer_.head_info_list_.find(field_name);
    if (f != http_text_buffer_.head_info_list_.end())
    {
        return f->second;
    }
    else
    {
        return "";
    }
}

int CHttpFormat::sChunkComplete(http_parser* hp)
{
    CHttpTextBuffer* http_text_buffer = (CHttpTextBuffer*)hp->data;
    http_text_buffer->http_func_name_ = "sChunkComplete";
    return 0;
}

int CHttpFormat::sUrlCallback(http_parser* hp, const char* at, size_t length)
{
    CHttpTextBuffer* http_text_buffer = (CHttpTextBuffer*)hp->data;
    http_text_buffer->http_func_name_ = "sUrlCallback";
    //std::string http_url;
    //http_url.append(at, length);
    //std::cout << "[my_url_callback]" << http_url << std::endl;
    return 0;
}

int CHttpFormat::sHeaderFieldCallback(http_parser* hp, const char* at, size_t length)
{
    CHttpTextBuffer* http_text_buffer = (CHttpTextBuffer*)hp->data;
    http_text_buffer->http_func_name_ = "sHeaderFieldCallback";

    //std::string http_head_field;
    //http_head_field.append(at, length);
    //std::cout << "[my_header_field_callback]" << http_head_field << std::endl;
    return 0;
}

int CHttpFormat::sStatusHandle(http_parser* hp, const char* at, size_t length)
{
    CHttpTextBuffer* http_text_buffer = (CHttpTextBuffer*)hp->data;
    http_text_buffer->http_func_name_ = "sStatusHandle";

    //std::string http_status;
    //http_status.append(at, length);
    //std::cout << "[sStatusHandle]" << http_status << std::endl;
    return 0;
}

int CHttpFormat::sBodyHandle(http_parser* hp, const char* at, size_t length)
{
    CHttpTextBuffer* http_text_buffer = (CHttpTextBuffer*)hp->data;
    http_text_buffer->http_func_name_ = "sBodyHandle";

    http_text_buffer->http_post_text_.append(at, length);
    return 0;
}

int CHttpFormat::sHeadValue(http_parser* hp, const char* at, size_t length)
{
    CHttpTextBuffer* http_text_buffer = (CHttpTextBuffer*)hp->data;
    http_text_buffer->http_func_name_ = "sHeadValue";

    std::string http_data;
    http_data.append(at, length);
    http_text_buffer->head_info_list_[http_text_buffer->http_head_info_.field_] = http_data;

    if (http_text_buffer->is_post_length_ == true)
    {
        http_text_buffer->content_length_ = atoi(http_data.c_str());
        http_text_buffer->is_post_length_ = false;
    }

    if (http_text_buffer->is_upgrade == true)
    {
        http_text_buffer->upgrade_.append(at, length);
        transform(http_text_buffer->upgrade_.begin(), http_text_buffer->upgrade_.end(), http_text_buffer->upgrade_.begin(), ::tolower);
        http_text_buffer->is_upgrade = false;
    }

    if (http_text_buffer->is_websocket_key_ == true)
    {
        http_text_buffer->websocket_key_.append(at, length);
        http_text_buffer->is_websocket_key_ = false;
    }

    return 0;
}

int CHttpFormat::sHeadField(http_parser* hp, const char* at, size_t length)
{
    CHttpTextBuffer* http_text_buffer = (CHttpTextBuffer*)hp->data;
    http_text_buffer->http_func_name_ = "sHeadField";
    std::string http_data;
    http_data.append(at, length);

    transform(http_data.begin(), http_data.end(), http_data.begin(), ::tolower);

    //��¼httpͷ����
    http_text_buffer->http_head_info_.field_ = http_data;

    if (http_data == "content-length")
    {
        http_text_buffer->is_post_length_ = true;
    }
    
    if (http_data == "upgrade")
    {
        http_text_buffer->is_upgrade = true;   //��Э������
    }

    if (http_data == "sec-websocket-key")
    {
        http_text_buffer->is_websocket_key_ = true;
    }

    //std::cout << "[sHeadField]" << http_data << std::endl;
    return 0;
}

int CHttpFormat::sHeadComplete(http_parser* hp)
{
    CHttpTextBuffer* http_text_buffer = (CHttpTextBuffer*)hp->data;
    http_text_buffer->http_func_name_ = "sHeadComplete";
    return 0;
}

int CHttpFormat::sMessageBegin(http_parser* hp)
{
    CHttpTextBuffer* http_text_buffer = (CHttpTextBuffer*)hp->data;
    http_text_buffer->http_func_name_ = "sMessageBegin";
    return 0;
}

int CHttpFormat::sMessageEnd(http_parser* hp)
{
    CHttpTextBuffer* http_text_buffer = (CHttpTextBuffer*)hp->data;
    http_text_buffer->http_func_name_ = "sMessageEnd";
    return 0;
}

int CHttpFormat::sChunkHeader(http_parser* hp)
{
    CHttpTextBuffer* http_text_buffer = (CHttpTextBuffer*)hp->data;
    http_text_buffer->http_func_name_ = "sChunkHeader";
    return 0;
}


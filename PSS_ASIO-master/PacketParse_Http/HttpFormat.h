#pragma once

#include <iostream>
#include <string>
#include <sstream>
#include <algorithm>
#include <map>
#include "http_parser.h"

// httpЭ�����
// ��ǰֻ����post���������
// add by freeeyes

class CHttpHeadInfo
{
public:
    std::string field_;
};

class CHttpTextBuffer
{
public:
    std::string http_func_name_;     //��ǰִ�лص�����������
    std::string http_request_text_;  //��ǰ��http��Ϣ�ı�
    std::string http_post_text_;     //��ǰ��post��Ϣ�ı�
    bool is_completed_ = false;      //�Ƿ�������
    int buffer_parse_pos_ = 0;       //��ǰ���������ֽ�λ��
    bool is_post_length_ = false;    //�Ƿ�����Content-Length��ǩ
    int content_length_ = 0;         //����post��Ϣ�ĳ��� 
    bool is_upgrade = false;         //�Ƿ��ǰ汾����
    std::string upgrade_;            //�汾�����ֶ�
    bool is_websocket_key_ = false;  //�Ƿ���websocket_key�ֶ�
    std::string websocket_key_;      //websocket_key����

    std::map<std::string, std::string> head_info_list_;
    CHttpHeadInfo http_head_info_;

    void clear()
    {
        buffer_parse_pos_ = 0;
        is_completed_ = false;
        http_request_text_ = "";
        http_post_text_ = "";
        http_func_name_ = "";
        is_post_length_ = false;
        content_length_ = 0;
        is_upgrade = false;
        is_websocket_key_ = false;

        head_info_list_.clear();
    }
};

class CHttpFormat
{
public:
    void init_http_setting();

    int try_parse(std::string http_text);

    std::string get_post_text();
    std::string get_post_error();
    std::string get_websocket_client_key();

    std::string get_response_text(std::string data);
    std::string get_response_websocket_text(std::string data);

    bool is_websocket();

    std::string get_head_info(std::string field_name);

    static int sChunkComplete(http_parser* hp);
    static int sChunkHeader(http_parser* hp);
    static int sMessageEnd(http_parser* hp);
    static int sMessageBegin(http_parser* hp);
    static int sHeadComplete(http_parser* hp);
    static int sHeadValue(http_parser* hp, const char* at, size_t length);
    static int sHeadField(http_parser* hp, const char* at, size_t length);
    static int sBodyHandle(http_parser* hp, const char* at, size_t length);
    static int sStatusHandle(http_parser* hp, const char* at, size_t length);
    static int sHeaderFieldCallback(http_parser* hp, const char* at, size_t length);
    static int sUrlCallback(http_parser* hp, const char* at, size_t length);

private:
    CHttpTextBuffer http_text_buffer_;
    http_parser http_parser_;
    http_parser_settings http_settings_;
};

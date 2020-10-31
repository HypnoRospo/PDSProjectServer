//
// Created by enrico_scalabrino on 17/10/20.
//
#ifndef PDSPROJECTSERVER_MESSAGE_H
#define PDSPROJECTSERVER_MESSAGE_H


#include <memory>
#include <thread>
#include <mutex>
#include <deque>
#include <optional>
#include <vector>
#include <iostream>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <boost/asio.hpp>

enum class MsgType : uint32_t
{
    NONCE,GETPATH,LOGIN,LOGOUT,REGISTER,CRC,ERROR, TRY_AGAIN_REGISTER,TRY_AGAIN_LOGIN
};

namespace Message {

    // Message Header is sent at start of all messages. The template allows us
    // to use "enum class" to ensure that the messages are valid at compile time
    template <typename T>
    struct message_header
    {
        T id{};
        uint32_t size = 0;
    };

// Message Body contains a header and a std::vector, containing raw bytes
// of infomation. This way the message can be variable length, but the size
// in the header must be updated.
    template <typename T>
    struct message
    {
        // Header & Body vector
        message_header<T> header{};
        std::vector<char> body;

// returns size of entire message packet in bytes
        size_t size() const
        {
            return body.size();
        }

        // Override for std::cout compatibility - produces friendly description of message
        friend std::ostream& operator << (std::ostream& os, const message<T>& msg)
        {
            os << "ID:" << int(msg.header.id) << " Size:" << msg.header.size <<std::endl;
            os << "BODY: " << msg.body.data() <<std::endl;
            return os;
        }

        // Convenience Operator overloads - These allow us to add and remove stuff from
        // the body vector as if it were a stack, so First in, Last Out. These are a
        // template in itself, because we dont know what data type the user is pushing or
        // popping, so lets allow them all. NOTE: It assumes the data type is fundamentally
        // Plain Old Data (POD). TLDR: Serialise & Deserialise into/from a vector

        // Pushes any POD-like data into the message buffer
        template<typename DataType>
        friend message<T>& operator << (message<T>& msg, const DataType& data)
        {
            // Check that the type of the data being pushed is trivially copyable
            static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pushed into vector");

            // Resize the vector by the size of the data being pushed
            msg.body.clear();

            msg.body.assign(data.begin(),data.end());

            // Recalculate the message size
            msg.header.size = msg.size();

            // Return the target message so it can be "chained"
            return msg;
        }

        // Pulls any POD-like data form the message buffer
        template<typename DataType>
        friend message<T>& operator >> (message<T>& msg, DataType& data)
        {
            // Check that the type of the data being pushed is trivially copyable
            static_assert(std::is_standard_layout<DataType>::value, "Data is too complex to be pulled from vector");

            // Physically copy the data from the vector into the user variable
            std::copy(msg.body.begin(), msg.body.end(), std::back_inserter(data));

            msg.header.id=MsgType::ERROR;
            msg.header.size=0;
            msg.body.clear();
            return msg;
        }

        boost::system::error_code sendMessage(boost::asio::ip::tcp::socket& socket)
        {
            boost::system::error_code errorCode;

            if(this->header.id != MsgType::TRY_AGAIN_REGISTER && this->header.id != MsgType::TRY_AGAIN_LOGIN)
            {
                boost::asio::write(socket, boost::asio::buffer(&(this->header.size), sizeof(this->header.size)), errorCode);
                if(!errorCode.failed())
                    boost::asio::write(socket, boost::asio::buffer(&(this->header.id), sizeof(this->header.id)), errorCode);
                if(!errorCode.failed())
                    boost::asio::write(socket, boost::asio::buffer(this->body.data(), this->body.size()), errorCode);
            }
            else
                boost::asio::write(socket, boost::asio::buffer(&(this->header.id), sizeof(this->header.id)), errorCode);

            return errorCode;

        }

        void set_id(MsgType type) {
            this->header.id=type;
        }

    };

};


#endif //PDSPROJECTSERVER_MESSAGE_H

#ifndef SERVER_HPP_VIA_HTTPLIB_
#define SERVER_HPP_VIA_HTTPLIB_

#pragma once

//////////////////////////////////////////////////////////////////////////////
// Copyright (c) 2013-2015 Ken Barker
//
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)
//////////////////////////////////////////////////////////////////////////////
/// @file server.hpp
/// @brief The server template class.
//////////////////////////////////////////////////////////////////////////////
#include "connection.hpp"
#include <boost/noncopyable.hpp>
#include <boost/asio.hpp>
#include <set>
#include <string>
#include <sstream>

namespace via
{
  namespace comms
  {
    //////////////////////////////////////////////////////////////////////////
    /// @class server
    /// A template class for implementing a tcp or ssl server using buffered
    /// connections.
    /// The class can be configured to use either tcp or ssl sockets depending
    /// upon which class is provided as the SocketAdaptor: tcp_adaptor or
    /// ssl::ssl_tcp_adaptor respectively.
    /// The other template parameters configure the type of container to use
    /// for the transmit buffers, the size of the receive buffer and
    /// whether to use asio::strand for an io_service running in multiple
    /// treads.
    /// @see connection
    /// @see tcp_adaptor
    /// @see ssl::ssl_tcp_adaptor
    /// @param SocketAdaptor the type of socket, use: tcp_adaptor or
    /// ssl::ssl_tcp_adaptor
    /// @param Container the container to use for the tx buffer, default
    /// std::vector<char>.
    /// It must contain a contiguous array of bytes. E.g. std::string or
    /// std::array<char, size>
    /// @param use_strand if true use an asio::strand to wrap the handlers,
    /// default false.
    //////////////////////////////////////////////////////////////////////////
    template <typename SocketAdaptor, typename Container = std::vector<char>,
              bool use_strand = false>
    class server : boost::noncopyable
    {
    public:

      /// The connection type used by this server.
      typedef connection<SocketAdaptor, Container, use_strand> connection_type;

      /// A set of connections.
      typedef std::set<boost::shared_ptr<connection_type> > connections;

      /// An iterator to the connections;
      typedef typename connections::iterator connections_iterator;

      /// Event callback function type.
      typedef typename connection_type::event_callback_type event_callback_type;

      /// Error callback function type.
      typedef typename connection_type::error_callback_type error_callback_type;

    private:
      /// The asio::io_service to use.
      boost::asio::io_service& io_service_;

      /// The IPv6 acceptor for this server.
      boost::asio::ip::tcp::acceptor acceptor_v6_;

      /// The IPv4 acceptor for this server.
      boost::asio::ip::tcp::acceptor acceptor_v4_;

      /// The next connection to be accepted.
      boost::shared_ptr<connection_type> next_connection_;

      /// The connections established with this server.
      connections connections_;

      /// The password. Only used by SSL servers.
      std::string password_;

      event_callback_type event_callback_;   ///< The event callback function.
      error_callback_type error_callback_;   ///< The error callback function.

      size_t rx_buffer_size_; ///< The size of the receive buffer.
      /// The connection timeouts, in milliseconds, zero is disabled.
      int timeout_;
      bool no_delay_;         ///< The tcp no delay status.
      bool keep_alive_;       ///< The tcp keep alive status.

      /// @accept_handler
      /// The callback function called by the acceptor when it accepts a
      /// new connection.
      /// If there is no error, it performs the following:
      /// - calls "start" on the new connection.
      /// - connects the connections event and error signals to the servers
      /// - add the new connection to the set
      /// - restart the acceptor to look for new connections.
      /// @param error the error, if any.
      void accept_handler(const boost::system::error_code& error)
      {
        if ((acceptor_v6_.is_open() || acceptor_v4_.is_open())&&
            (boost::asio::error::operation_aborted != error))
        {
          if (error)
            error_callback_(error, next_connection_);
          else
          {
            next_connection_->start(no_delay_, keep_alive_, timeout_);
            connections_.insert(next_connection_);
            next_connection_.reset();
          }

          start_accept();
        }
      }

      /// @fn event_handler.
      /// It forwards the connection's event signal.
      /// For a disconnected event, it deletes the connection.
      /// @param event the event, @see event_type.
      /// @param connection a weak_pointer to the connection that sent the
      /// event.
      void event_handler(int event, boost::weak_ptr<connection_type> ptr)
      {
        event_callback_(event, ptr);
        if (event == DISCONNECTED)
        {
          if (boost::shared_ptr<connection_type> connection = ptr.lock())
          {
            // search for the connection to delete
            connections_iterator iter(connections_.find(connection));
            if (iter != connections_.end())
              connections_.erase(iter);
          }
        }
      }

      /// @fn error_handler.
      /// It just forwards the connection's error signal.
      /// @param error the boost asio error.
      /// @param connection a weak_pointer to the connection that sent the
      /// error.
      void error_handler(const boost::system::error_code& error,
                         boost::weak_ptr<connection_type> connection)
      { error_callback_(error, connection); }

      /// @fn start_accept
      /// Wait for connections.
      void start_accept()
      {
        next_connection_ = connection_type::create(io_service_,
                                boost::bind(&server::event_handler, this, _1, _2),
                                boost::bind(&server::error_handler, this,
                                            boost::asio::placeholders::error, _2),
                                                   rx_buffer_size_);
        if (acceptor_v6_.is_open())
          acceptor_v6_.async_accept(next_connection_->socket(),
                                 boost::bind(&server::accept_handler, this,
                                             boost::asio::placeholders::error));

        if (acceptor_v4_.is_open())
          acceptor_v4_.async_accept(next_connection_->socket(),
                                 boost::bind(&server::accept_handler, this,
                                             boost::asio::placeholders::error));
      }

    public:

      /// The server constructor.
      /// @post the event_callback and error_callback functions MUST be set
      /// AFTER this constructor has been called.
      /// @see set_event_callback
      /// @see set_error_callback
      /// @param io_service the boost asio io_service used by the acceptor
      /// and connections.
      explicit server(boost::asio::io_service& io_service) :
        io_service_(io_service),
        acceptor_v6_(io_service),
        acceptor_v4_(io_service),
        next_connection_(),
        connections_(),
        password_(),
        event_callback_(),
        error_callback_(),
        rx_buffer_size_(SocketAdaptor::DEFAULT_RX_BUFFER_SIZE),
        timeout_(0),
        no_delay_(false),
        keep_alive_(false)
      {}

      /// The server constructor.
      /// @pre the event_callback and error_callback functions must exist.
      /// E.g. if either of them are class member functions then the class
      /// MUST have been constructed BEFORE this constructor is called.
      /// @param io_service the boost asio io_service used by the acceptor
      /// and connections.
      /// @param event_callback the event callback function.
      /// @param error_callback the error callback function.
      explicit server(boost::asio::io_service& io_service,
                      event_callback_type event_callback,
                      error_callback_type error_callback) :
        io_service_(io_service),
        acceptor_v6_(io_service),
        acceptor_v4_(io_service),
        next_connection_(),
        connections_(),
        password_(),
        event_callback_(event_callback),
        error_callback_(error_callback),
        timeout_(0),
        no_delay_(false),
        keep_alive_(false)
      {}

      /// Destructor, close the connections.
      ~server()
      { close(); }

      /// Function to create a shared pointer to a server.
      /// @post the event_callback and error_callback functions MUST be set
      /// AFTER this function has been called.
      /// @see set_event_callback
      /// @see set_error_callback
      /// @param io_service the boost asio io_service used by the server.
      static boost::shared_ptr<server> create(boost::asio::io_service& io_service)
      { return boost::shared_ptr<server>(new server(io_service)); }

      /// Function to create a shared pointer to a server.
      /// @pre the event_callback and error_callback functions must exist.
      /// E.g. if either of them are class member functions then the class
      /// MUST have been constructed BEFORE this function is called.
      /// @param io_service the boost asio io_service used by the server.
      /// @param event_callback the event callback function.
      /// @param error_callback the error callback function.
      static boost::shared_ptr<server> create(boost::asio::io_service& io_service,
                                              event_callback_type event_callback,
                                              error_callback_type error_callback)
      { return boost::shared_ptr<server>(new server(io_service, event_callback,
                                                    error_callback)); }

      /// @fn accept_connections
      /// Create the acceptor and wait for connections.
      /// @param port the port number to serve.
      /// @param ipv4_only whether an IPV4 only server is required.
      /// @return the boost error code, false if no error occured
      boost::system::error_code accept_connections(unsigned short port, bool ipv4_only)
      {
        // Determine whether the IPv6 acceptor accepts both IPv6 & IPv4
        boost::asio::ip::v6_only ipv6_only(false);
        boost::system::error_code ec;

        // Open the IPv6 acceptor unless IPv4 only mode
        if (!ipv4_only)
        {
          acceptor_v6_.open(boost::asio::ip::tcp::v6(), ec);
          if (!ec)
          {
            acceptor_v6_.set_option(ipv6_only, ec);
            acceptor_v6_.get_option(ipv6_only);
            acceptor_v6_.set_option
              (boost::asio::ip::tcp::acceptor::reuse_address(true));
            acceptor_v6_.bind
              (boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v6(), port));
            acceptor_v6_.listen();
          }
        }

        // Open the IPv4 acceptor if the IPv6 acceptor is not open or it
        // only supports IPv6
        if (!acceptor_v6_.is_open() || ipv6_only)
        {
          acceptor_v4_.open(boost::asio::ip::tcp::v4(), ec);
          if (!ec)
          {
            acceptor_v4_.set_option
                (boost::asio::ip::tcp::acceptor::reuse_address(true));
            acceptor_v4_.bind
              (boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port));
            acceptor_v4_.listen();
          }
        }

        start_accept();
        return ec;
      }

      /// @fn password
      /// Get the password.
      /// @pre It must be an SSL server.
      /// @return The password.
      const std::string password() const
      { return password_; }

      /// @fn set_password
      /// Set the password.
      /// @pre It must be an SSL server.
      /// @param password the password
      void set_password(std::string const& password)
      {
        password_ = password;
        connection_type::ssl_context().set_password_callback
            (boost::bind(&server::password, this));
      }

      /// @fn set_event_callback
      /// Set the event_callback function.
      /// For use with the Constructor or create function that doesn't take
      /// an event_callback parameter.
      /// @see server(boost::asio::io_service& io_service)
      /// @see create(boost::asio::io_service& io_service)
      /// @param event_callback the event callback function.
      void set_event_callback(event_callback_type event_callback)
      { event_callback_ = event_callback; }

      /// @fn set_error_callback
      /// Set the error_callback function.
      /// For use with the Constructor or create function that doesn't take
      /// an error_callback parameter.
      /// @see server(boost::asio::io_service& io_service)
      /// @see create(boost::asio::io_service& io_service)
      /// @param error_callback the error callback function.
      void set_error_callback(error_callback_type error_callback)
      { error_callback_ = error_callback; }

      /// Set the size of the receive buffer.
      /// @param size the new size of the receive buffer.
      void set_rx_buffer_size(size_t size)
      { rx_buffer_size_ = size; }

      /// @fn set_no_delay
      /// Set the tcp no delay status for all future connections.
      /// @param enable if true it disables the Nagle algorithm.
      void set_no_delay(bool enable)
      { no_delay_ = enable; }

      /// @fn set_keep_alive
      /// Set the tcp keep alive status for all future connections.
      /// @param enable if true enables the tcp socket keep alive status.
      void set_keep_alive(bool enable)
      { keep_alive_ = enable; }

      /// @fn set_timeout
      /// Set the send and receive timeout value for all future connections.
      /// @pre sockets may remain open forever
      /// @post sockets will close if no activity has occured after the
      /// timeout period.
      /// @param timeout the timeout in milliseconds.
      void set_timeout(int timeout)
      { timeout_ = timeout; }

      /// @fn close
      /// Close the server and all of the connections associated with it.
      void close()
      {
        acceptor_v6_.close();
        acceptor_v4_.close();
        next_connection_.reset();
        connections_.clear();
      }
    };
  }
}

#endif

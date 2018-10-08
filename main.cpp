#include <cstdlib>
#include <cstddef>
#include <iostream>
#include <string>
#include <iomanip>
#include <ctime>

#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread/mutex.hpp>

#include "smpp_pdu/header.hpp"
#include "smpp_pdu/deliver_sm.hpp"

namespace tcp_proxy
{
   namespace ip = boost::asio::ip;

   class bridge : public boost::enable_shared_from_this<bridge>
   {
   public:

      typedef ip::tcp::socket socket_type;
      typedef boost::shared_ptr<bridge> ptr_type;

      bridge(boost::asio::io_service& ios)
      : downstream_socket_(ios),  upstream_socket_  (ios)
      {}

      socket_type& downstream_socket()
      {
         return downstream_socket_;
      }

      socket_type& upstream_socket()
      {
         return upstream_socket_;
      }

      void start(const std::string& upstream_host, unsigned short upstream_port)
      {
         // Attempt connection to remote server (upstream side)
         upstream_socket_.async_connect(  ip::tcp::endpoint( boost::asio::ip::address::from_string(upstream_host), upstream_port), boost::bind(&bridge::handle_upstream_connect, shared_from_this(), boost::asio::placeholders::error));
      }

      void handle_upstream_connect(const boost::system::error_code& error)
      {
         if (!error)
         {
            // Setup async read from remote server (upstream)
            upstream_socket_.async_read_some(  boost::asio::buffer(upstream_data_,max_data_length),   boost::bind(&bridge::handle_upstream_read, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
            // Setup async read from client (downstream)
            downstream_socket_.async_read_some( boost::asio::buffer(downstream_data_,max_data_length), boost::bind(&bridge::handle_downstream_read, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
         }
         else
            close();
      }

   private:
      std::string ToHex(const std::string& s, bool upper_case /* = true */)
      {
          std::ostringstream ret;
          for (std::string::size_type i = 0; i < s.length(); ++i)
              ret << std::hex << std::setfill('0') << std::setw(2) << (upper_case ? std::uppercase : std::nouppercase) << (int)s[i];
          return ret.str();
      }

      std::string CurrentDate()
      {
          time_t t = time(0);   // get time now
          struct tm * now = localtime( & t );
          std::stringstream year_str, mon_str, day_str, hour_str, min_str, sec_str;
          year_str << now->tm_year + 1900;
          mon_str << (now->tm_mon + 1);
          day_str << now->tm_mday;
          hour_str << now->tm_hour;
          min_str << now->tm_min;
          sec_str << now->tm_sec;

          std::string data = year_str.str() + std::string("-") + mon_str.str() +  std::string("-") + day_str.str()
                  + std::string(" ")
                  + hour_str.str() + std::string(":") + min_str.str() +  std::string(":") + sec_str.str()
                  + std::string(" ");
          return data;
      }


      //   Section A: Remote Server --> Proxy --> Client   ( Process data recieved from remote sever then send to client. )
      // Read from remote server complete, now send data to client
      void handle_upstream_read(const boost::system::error_code& error, const size_t& bytes_transferred)
      {
               // LOG ORIGIN DATA
               std::string data_str;
               data_str.assign(reinterpret_cast<char *>(upstream_data_),bytes_transferred);
               std::cout << CurrentDate() << ToHex(data_str,true) << std::endl;

               // MAKE A COPY OF CLIENT DATA
                memcpy(upstream_data_copy_,upstream_data_,bytes_transferred);

               try
               {
                       Smpp::CommandLength t = Smpp::CommandLength::decode(reinterpret_cast<const char *> (upstream_data_copy_ ) );
                       Smpp::CommandId c_id( Smpp::CommandId::decode(reinterpret_cast<const char *> (upstream_data_copy_ ) ) );
                       // HERE WE CHANGE CLIENT Deliver_SM
                       if ( (c_id == Smpp::CommandId::DeliverSm) && (t == bytes_transferred)  )
                       {
                           std::cout << CurrentDate() << "Process DeliverSm "<< std::endl;
                           // PARSE DATA
                           Smpp::DeliverSm pdu_DeliverSm;
                           pdu_DeliverSm.decode( reinterpret_cast<const Smpp::Uint8 *>(upstream_data_copy_) );
                           std::vector<Smpp::Uint8> short_message_vector = pdu_DeliverSm.short_message();
                           std::string short_message_str("");
                           for(std::vector<Smpp::Uint8>::iterator it = short_message_vector.begin(); it != short_message_vector.end(); ++it) {
                               short_message_str.push_back(*it);
                           }


                           if (short_message_str.find("sub:") == std::string::npos)  // NO "sub:" IN short_message
                           {
                               // Add "sub:001 dlvrd:001 " after first space
                               int pos = short_message_str.find(" ");
                               short_message_str.insert(pos,std::string(" sub:001 dlvrd:001"));

                           }
                            short_message_str = short_message_str.substr(0,159);
                            pdu_DeliverSm.short_message(reinterpret_cast<const Smpp::Uint8 *>(short_message_str.c_str()), short_message_str.length());
                            memcpy(upstream_data_copy_,pdu_DeliverSm.encode(),pdu_DeliverSm.command_length());

                            if (!error) async_write(downstream_socket_, boost::asio::buffer(upstream_data_copy_,pdu_DeliverSm.command_length()), boost::bind(&bridge::handle_downstream_write, shared_from_this(), boost::asio::placeholders::error));
                            else        close();
                       }
                       else
                       {
                           if (!error) async_write(downstream_socket_, boost::asio::buffer(upstream_data_,bytes_transferred), boost::bind(&bridge::handle_downstream_write, shared_from_this(), boost::asio::placeholders::error));
                           else        close();
                       }

               }
                   catch (...)
               {
                       // IF SOMETHING GOES WRONG JUST SEND AS TRANSPARENT PROXY
                       if (!error) async_write(downstream_socket_, boost::asio::buffer(upstream_data_,bytes_transferred), boost::bind(&bridge::handle_downstream_write, shared_from_this(), boost::asio::placeholders::error));
                       else        close();
               }





      }

      // Write to client complete, Async read from remote server
      void handle_downstream_write(const boost::system::error_code& error)
      {
         if (!error) upstream_socket_.async_read_some( boost::asio::buffer(upstream_data_,max_data_length), boost::bind(&bridge::handle_upstream_read, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
         else        close();
      }
      // *** End Of Section A ***



      //   Section B: Client --> Proxy --> Remove Server   (    Process data recieved from client then write to remove server. )
      // Read from client complete, now send data to remote server
      void handle_downstream_read(const boost::system::error_code& error,  const size_t& bytes_transferred)
      {
         if (!error)
         {
            async_write(upstream_socket_, boost::asio::buffer(downstream_data_,bytes_transferred), boost::bind(&bridge::handle_upstream_write, shared_from_this(), boost::asio::placeholders::error));
         }
         else
            close();
      }

      // Write to remote server complete, Async read from client
      void handle_upstream_write(const boost::system::error_code& error)
      {
         if (!error)
         {
            downstream_socket_.async_read_some(  boost::asio::buffer(downstream_data_,max_data_length),  boost::bind(&bridge::handle_downstream_read, shared_from_this(), boost::asio::placeholders::error, boost::asio::placeholders::bytes_transferred));
         }
         else
            close();
      }
      // *** End Of Section B ***

      void close()
      {
         boost::mutex::scoped_lock lock(mutex_);

         if (downstream_socket_.is_open()) downstream_socket_.close();
         if (upstream_socket_.is_open()) upstream_socket_.close();
      }

      socket_type downstream_socket_;
      socket_type upstream_socket_;

      enum { max_data_length = 8192 }; //8KB
      unsigned char downstream_data_[max_data_length];
      unsigned char upstream_data_  [max_data_length];
      unsigned char downstream_data_copy_[max_data_length];
      unsigned char upstream_data_copy_  [max_data_length];
      boost::mutex mutex_;

   public:

      class acceptor
      {
      public:

         acceptor(boost::asio::io_service& io_service, const std::string& local_host, unsigned short local_port,  const std::string& upstream_host, unsigned short upstream_port)
         : io_service_(io_service),  localhost_address(boost::asio::ip::address_v4::from_string(local_host)),  acceptor_(io_service_,ip::tcp::endpoint(localhost_address,local_port)),  upstream_port_(upstream_port), upstream_host_(upstream_host)
         {}

         bool accept_connections()
         {
            try
            {
               session_ = boost::shared_ptr<bridge>(new bridge(io_service_));
               acceptor_.async_accept(session_->downstream_socket(),  boost::bind(&acceptor::handle_accept, this,  boost::asio::placeholders::error));
            }
            catch(std::exception& e)
            {
               std::cerr << "acceptor exception: " << e.what() << std::endl;
               return false;
            }

            return true;
         }

      private:

         void handle_accept(const boost::system::error_code& error)
         {
            if (!error)
            {
               session_->start(upstream_host_,upstream_port_);

               if (!accept_connections())
               {
                  std::cerr << "Failure during call to accept." << std::endl;
               }
            }
            else
            {
               std::cerr << "Error: " << error.message() << std::endl;
            }
         }

         boost::asio::io_service& io_service_;
         ip::address_v4 localhost_address;
         ip::tcp::acceptor acceptor_;
         ptr_type session_;
         unsigned short upstream_port_;
         std::string upstream_host_;
      };

   };
}

int main(int argc, char* argv[])
{
   if (argc != 3)
      {
         std::cerr << "usage: program <forward host ip> <forward port>" << std::endl;
         return 1;
      }

   const unsigned short local_port   = 15019;
   const unsigned short forward_port = static_cast<unsigned short>(::atoi(argv[2]));
   const std::string local_host      = "0.0.0.0";
   const std::string forward_host    = argv[1];

   boost::asio::io_service ios;

   try
   {
      tcp_proxy::bridge::acceptor acceptor(ios, local_host, local_port, forward_host, forward_port);

      acceptor.accept_connections();

      ios.run();
   }
   catch(std::exception& e)
   {
      std::cerr << "Error: " << e.what() << std::endl;
      return 1;
   }

   return 0;
}



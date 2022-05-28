#pragma once

#include <boost/asio.hpp>
#include <boost/beast.hpp>
#include <boost/beast/ssl.hpp>
#include <nlohmann/json.hpp>
#include<string>
#include <iostream>
#include <string>
#include <chrono>
#include <exception>
#include "utils.hpp"

namespace beast = boost::beast;         
namespace http  = beast::http;          
namespace net   = boost::asio;          
namespace ssl   = boost::asio::ssl;     
using tcp       = boost::asio::ip::tcp; 

using json = nlohmann::json;
using executor = net::any_io_executor; 
using namespace utils;

void fail_http(beast::error_code ec, char const* what)
{
    std::cerr << what << ": " << ec.message() << "\n";
}

boost::url make_url(boost::url base_api, boost::url method) {
    assert(!method.is_path_absolute());
    assert(base_api.data()[base_api.size() - 1] == '/');

    boost::urls::error_code ec;
    boost::url url;
    resolve(base_api, method, url, ec);
    if (ec)
        throw boost::system::system_error(ec);
    return url;
}

namespace binance{


    class RESTClient : public std::enable_shared_from_this<RESTClient> 
    {

    private:    

        void configure(const std::string &api,const std::string &secret);
        boost::url base_api{"https://testnet.binance.vision/api/v3/"};
        tcp::resolver resolver_;
        beast::ssl_stream<beast::tcp_stream> stream_;
        beast::flat_buffer buffer_;
        http::request<http::string_body>  req_;
        http::response<http::string_body> res_;
        std::string api_key = "v6uhUtse5Ae1Gyz72eMSbUMGw7VUDdd5AnqobMOW1Llzi4snnfP4YCyY9V74PFJ4";
        std::string secret_key = "FW8j4YobD26PVP6QLu0sv4Dv7OzrtfgQKzn8FoIMwGzMW9Y0VmX1DatbLIfXoCHV";
        net::io_context& ioc;
        ssl::context& ctx;
        
    public:

        RESTClient(executor ex, ssl::context& ctx,net::io_context& ioc);

        http::response<http::string_body> http_call(boost::url url, http::verb action);

        std::string authenticate(const char* key, const char* data);

        json server_time();

        json latest_price(const std::string &symbol);

        json exchange_info(const std::string &symbol);

        json get_account_info();

        json check_order_status(const std::string &symbol,int orderid);

        json cancel_all_orders(const std::string &symbol);

        json cancel_order(const std::string &symbol,int orderid);

        json place_order(const std::string &symbol,int price,e_side side,order_type type,timeforce time,const std::string &quantity);

        json open_orders();

        json bidask(const std::string &symbol);

        json avg_price(const std::string &symbol);

        json klines(const std::string &symbol,const std::string &interval);

        json recent_trades(const std::string &symbol,const std::string &levels);

        json orderbook(const std::string &symbol,const std::string &levels);

        json ping_binance();

    };
}

namespace binance
{
    RESTClient::RESTClient(executor ex, ssl::context& ctxe, net::io_context &ioce)
        : resolver_(ex),stream_(ex, ctxe),ioc(ioce),ctx(ctxe){}


    http::response<http::string_body> RESTClient::http_call(boost::url url, http::verb action) 
    {

        std::string const host(url.host());
        std::string const service = url.has_port() 
            ? url.port()
            : (url.scheme_id() == boost::urls::scheme::https) 
                ? "https"
                : "http";
        url.remove_origin(); 

        if(! SSL_set_tlsext_host_name(stream_.native_handle(), host.c_str()))
        {
            beast::error_code ec{static_cast<int>(::ERR_get_error()), net::error::get_ssl_category()};
            std::cerr << ec.message() << "\n";
        }

        req_.method(action);
        req_.target(url.c_str());
        req_.set(http::field::host, host);
        req_.set(http::field::user_agent, BOOST_BEAST_VERSION_STRING);
        req_.set("X-MBX-APIKEY", api_key);

        req_.prepare_payload();

        auto const results = resolver_.resolve(host, service);
        beast::get_lowest_layer(stream_).connect(results);

        stream_.handshake(ssl::stream_base::client);

        http::write(stream_, req_);
        http::read(stream_, buffer_, res_);
        beast::error_code ec;
        stream_.shutdown(ec);

        return res_;
    }
    
    std::string RESTClient::authenticate(const char* key, const char* data) 
    {
        unsigned char *result;
        static char res_hexstring[64];
        int result_len = 32;
        std::string signature;

        result = HMAC(EVP_sha256(), key, strlen((char *)key), const_cast<unsigned char *>(reinterpret_cast<const unsigned char*>(data)), strlen((char *)data), NULL, NULL);
        for (int i = 0; i < result_len; i++) {
            sprintf(&(res_hexstring[i * 2]), "%02x", result[i]);
        }

        for (int i = 0; i < 64; i++) {
            signature += res_hexstring[i];
        }

        return signature;
    }

    void RESTClient::configure(const std::string &api,const std::string &secret)
    {
        this->api_key = api;
        this->secret_key = secret;
    }

    json RESTClient::server_time()
    {
        boost::url method{"time"};
        return json::parse(http_call(make_url(base_api,method),http::verb::get).body());
    }

    json RESTClient::latest_price(const std::string &symbol)
    {
        boost::url method{"ticker/price"};
        method.params().emplace_back("symbol",symbol);
        return json::parse(std::make_shared<RESTClient>(ioc.get_executor(),ctx,ioc)->http_call(make_url(base_api,method),http::verb::get).body());
    }


    json RESTClient::exchange_info(const std::string &symbol)
    {
        boost::url method{"exchangeInfo"};
        method.params().emplace_back("symbol",symbol);
        return json::parse(std::make_shared<RESTClient>(ioc.get_executor(),ctx,ioc)->http_call(make_url(base_api,method),http::verb::get).body());
    }

    json RESTClient::ping_binance( )
    {
        boost::url method{"ping"};
        return json::parse(std::make_shared<RESTClient>(ioc.get_executor(),ctx,ioc)->http_call(make_url(base_api,method),http::verb::get).body());

    }

    json RESTClient::orderbook(const std::string &symbol,const std::string &levels)
    {

        boost::url method{"depth"};
        method.params().emplace_back("symbol",symbol);
        method.params().emplace_back("limit",levels);
        return json::parse(std::make_shared<RESTClient>(ioc.get_executor(),ctx,ioc)->http_call(make_url(base_api,method),http::verb::get).body());

    }

    json RESTClient::recent_trades(const std::string &symbol,const std::string &levels)
    {
        boost::url method{"trades"};
        method.params().emplace_back("symbol",symbol);
        method.params().emplace_back("limit",levels);
        return json::parse(std::make_shared<RESTClient>(ioc.get_executor(),ctx,ioc)->http_call(make_url(base_api,method),http::verb::get).body());
    }

    json RESTClient::klines(const std::string &symbol,const std::string &interval)
    {

        boost::url method{"klines"};
        method.params().emplace_back("symbol",symbol);
        method.params().emplace_back("interval",interval);
        return json::parse(std::make_shared<RESTClient>(ioc.get_executor(),ctx,ioc)->http_call(make_url(base_api,method),http::verb::get).body());

    }

    json RESTClient::avg_price(const std::string &symbol )
    {
        // this->server_time( );
        auto t1 = high_resolution_clock::now();
        boost::url method{"avgPrice"};
        method.params().emplace_back("symbol",symbol);
        auto t2 = high_resolution_clock::now();
        auto ms_int = duration_cast<std::chrono::microseconds>(t2 -t1);
        std::cout << "it took avg price: " << ms_int.count() << "micros" <<std::endl;
        return json::parse(std::make_shared<RESTClient>(ioc.get_executor(),ctx,ioc)->http_call(make_url(base_api,method),http::verb::get).body());
    
    }

    json RESTClient::bidask(const std::string &symbol )
    {
        boost::url method{"ticker/bookTicker"};
        method.params().emplace_back("symbol",symbol);
        return json::parse(std::make_shared<RESTClient>(ioc.get_executor(),ctx,ioc)->http_call(make_url(base_api,method),http::verb::get).body());
    }

    json RESTClient::open_orders( )
    {
        boost::url method{"openOrders"};
        std::string server_timestamp = std::to_string(get_ms_timestamp(current_time()).count());
        std::string query_params = "timestamp=" +server_timestamp;
        method.params().emplace_back("signature",this->authenticate(this->secret_key.c_str(),query_params.c_str()));  // order matters
        method.params().emplace_back("timestamp",server_timestamp);
        return json::parse(std::make_shared<RESTClient>(ioc.get_executor(),ctx,ioc)->http_call(make_url(base_api,method),http::verb::get).body());
    }
    
    json RESTClient::place_order(const std::string &symbol,int price,e_side side,order_type type,timeforce time,const std::string &quantity )
    {
        boost::url method{"order"};
        std::string server_timestamp = std::to_string(get_ms_timestamp(current_time()).count());
        std::string query_params;
        
        if (type == order_type::market) query_params ="symbol="+symbol+"&side="+e_side_to_string(side) +"&type="+order_type_to_string(type)+ "&quantity="+quantity+"&recvWindow=60000"+"&timestamp=" + server_timestamp;
        else if(type == order_type::limit)  query_params ="symbol="+symbol+"&side="+e_side_to_string(side) +"&type="+order_type_to_string(type)+ "&timeInForce="+timeforce_to_string(time)+ "&quantity="+quantity+"&price="+std::to_string(price)+"&recvWindow=60000"+"&timestamp=" + server_timestamp;

        method.params().emplace_back("symbol",symbol);
        method.params().emplace_back("side",e_side_to_string(side));
        method.params().emplace_back("type",order_type_to_string(type));
        if (type == order_type::limit) method.params().emplace_back("timeInForce",timeforce_to_string(time));
        method.params().emplace_back("quantity",quantity); 
        if (type == order_type::limit) method.params().emplace_back("price",std::to_string(price));
        method.params().emplace_back("recvWindow", "60000");
        method.params().emplace_back("timestamp",server_timestamp);
        method.params().emplace_back("signature",this->authenticate(this->secret_key.c_str(),query_params.c_str())); 

        return json::parse(std::make_shared<RESTClient>(ioc.get_executor(),ctx,ioc)->http_call(make_url(base_api,method),http::verb::post).body());;

    }

    json RESTClient::cancel_order(const std::string &symbol,int orderid )
    {
        std::string server_timestamp = std::to_string(get_ms_timestamp(current_time()).count());
        std::string query_params = "symbol="+symbol+"&orderId="+std::to_string(orderid)+"&timestamp="+server_timestamp;
        boost::url method{"order"};
        method.params().emplace_back("symbol",symbol);
        method.params().emplace_back("orderId",std::to_string(orderid));
        method.params().emplace_back("signature",this->authenticate(this->secret_key.c_str(),query_params.c_str()));
        method.params().emplace_back("timestamp",server_timestamp);
        return json::parse(std::make_shared<RESTClient>(ioc.get_executor(),ctx,ioc)->http_call(make_url(base_api,method),http::verb::delete_).body());

    }

    json RESTClient::cancel_all_orders(const std::string &symbol)
    {
        std::string server_timestamp = std::to_string(get_ms_timestamp(current_time()).count());
        std::string query_params = "symbol="+symbol+"&timestamp="+server_timestamp;
        boost::url method{"openOrders"};
        method.params().emplace_back("symbol",symbol);
        method.params().emplace_back("signature",this->authenticate(this->secret_key.c_str(),query_params.c_str()));
        method.params().emplace_back("timestamp",server_timestamp);
        return json::parse(std::make_shared<RESTClient>(ioc.get_executor(),ctx,ioc)->http_call(make_url(base_api,method),http::verb::delete_).body());
    }
    
    json RESTClient::check_order_status(const std::string &symbol,int orderid )
    {
        std::string server_timestamp = std::to_string(get_ms_timestamp(current_time()).count());
        std::string query_params = "symbol="+symbol+"&orderId="+std::to_string(orderid)+"&timestamp="+server_timestamp;
        boost::url method{"order"};
        method.params().emplace_back("symbol",symbol);
        method.params().emplace_back("orderId",std::to_string(orderid));
        method.params().emplace_back("signature",this->authenticate(this->secret_key.c_str(),query_params.c_str()));
        method.params().emplace_back("timestamp",server_timestamp);
        return json::parse(std::make_shared<RESTClient>(ioc.get_executor(),ctx,ioc)->http_call(make_url(base_api,method),http::verb::get).body());

    }

    json RESTClient::get_account_info()
    {
        std::string server_timestamp = std::to_string(get_ms_timestamp(current_time()).count());
        std::string query_params = "timestamp="+server_timestamp;
        boost::url method{"account"};
        method.params().emplace_back("signature",this->authenticate(this->secret_key.c_str(),query_params.c_str()));
        method.params().emplace_back("timestamp",server_timestamp);
        return json::parse(std::make_shared<RESTClient>(ioc.get_executor(),ctx,ioc)->http_call(make_url(base_api,method),http::verb::get).body());

    }
}



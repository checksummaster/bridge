#define ASIO_STANDALONE
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/client.hpp>
#include "json11.hpp"

typedef websocketpp::client<websocketpp::config::asio> client;
client _client;


void on_message(websocketpp::connection_hdl hdl, client::message_ptr msg) {
	std::string err;
	json11::Json json = json11::Json::parse(msg->get_payload(), err);

	std::string type = "";
	if (json["type"].is_string()) {
		type = json["type"].string_value();
	}
	std::string message = "";
	if (json["message"].is_string()) {
		message = json["message"].string_value();
	}

	if ( type == "stdout") {
		printf("%s",message.c_str());
	}

	if ( type == "stderr") {
		printf("\x1B[31m%s\x1B[0m\n",message.c_str());
	}


}

void on_open1(websocketpp::connection_hdl hdl) {
	json11::Json json = json11::Json::object {
		{ "command",  "run" },
//		{ "args",  json11::Json::array {"/bin/ls","-l"} },
		{ "args",  json11::Json::array {"./test.sh","-l"} },
		{ "id", 1}
	};
	_client.send(hdl, json.dump() , websocketpp::frame::opcode::text);	


}


websocketpp::connection_hdl target;
unsigned char *img1, *img2;
long l1, l2;
int c = 1;
void on_timer(websocketpp::lib::error_code const & ec) {
	if (c++%2 == 0) {
    	_client.send(target, (void*)img1 , l1,websocketpp::frame::opcode::binary);
    }
    else {
    	_client.send(target, (void*)img2 , l2,websocketpp::frame::opcode::binary);  	
    }
  	_client.set_timer(10,on_timer);
}

void on_open2(websocketpp::connection_hdl hdl) {
	json11::Json json = json11::Json::object {
		{ "command", "transmitter" },
		{ "name", "test"}
	};
	_client.send(hdl, json.dump() , websocketpp::frame::opcode::text);	
	target = hdl;
	_client.set_timer(10,on_timer);
}




void on_close(websocketpp::connection_hdl hdl) {

}

unsigned char *readfile(const char *filePath, long *fileSize)
{

	FILE *file = fopen(filePath, "rb");

    fseek(file, 0, 2);
    *fileSize = ftell(file);
    fseek(file, 0, 0);

    unsigned char *fileBuf = new unsigned char[*fileSize];
    fread(fileBuf, *fileSize, 1, file);
    fclose(file);

    return fileBuf;

}


int main()
{
	std::string uri = "ws://localhost:9002";
	img1 = readfile("a.jpg",&l1);
	img2 = readfile("b.jpg",&l2);

	_client.set_access_channels(websocketpp::log::alevel::none);

	//_client.set_open_handler(&on_open1);
    _client.set_open_handler(&on_open2);
    _client.set_close_handler(&on_close);
	_client.set_message_handler(&on_message);


  	_client.init_asio();


    websocketpp::lib::error_code ec;
    client::connection_ptr con = _client.get_connection(uri, ec);

    if (ec) {
        printf("%s", ec.message().c_str());
        return 0;
    }

    _client.connect(con);
    _client.run();
    return 0;
}
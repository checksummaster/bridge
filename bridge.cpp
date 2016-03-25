#define ASIO_STANDALONE
#include <websocketpp/config/asio_no_tls.hpp>
#include <websocketpp/server.hpp>
#include "json11.hpp"

typedef websocketpp::server<websocketpp::config::asio> server;
server _server;

std::vector<std::string> logstable;

pid_t popen3(int *fdin, int *fdout, int *fderr,const char **const cmd) {
    int i, e;
    int p[3][2];
    pid_t pid;
    // set all the FDs to invalid
    for(i=0; i<3; i++)
            p[i][0] = p[i][1] = -1;

    // create the pipes
    for(int i=0; i<3; i++) 
            if(pipe(p[i]))
                    return -1;

    // and fork
    pid = fork();
    if(pid < 0 )
    	return pid;
    if(pid == 0) {
    	 // child
        dup2(p[STDIN_FILENO][0],STDIN_FILENO);
        close(p[STDIN_FILENO][1]);
        dup2(p[STDOUT_FILENO][1],STDOUT_FILENO);
        close(p[STDOUT_FILENO][0]);
        dup2(p[STDERR_FILENO][1],STDERR_FILENO);
        close(p[STDERR_FILENO][0]);
        execv(*cmd,const_cast<char*const*>(cmd));
        perror("execvp");
        exit(1); // Should never be executed .... 
        
    }

	// parent
    *fdin = p[STDIN_FILENO][1];
    close(p[STDIN_FILENO][0]);
    *fdout = p[STDOUT_FILENO][0];
    close(p[STDOUT_FILENO][1]);
    *fderr = p[STDERR_FILENO][0];
    close(p[STDERR_FILENO][1]);
    return pid;

}

void SendMessage(websocketpp::connection_hdl hdl,  std::string mes, int id)
{
	json11::Json json = json11::Json::object {
		{ "message",  mes },
		{ "id", id}
	};
	_server.send(hdl, json.dump() , websocketpp::frame::opcode::text);	
}

void SendMessageType(websocketpp::connection_hdl hdl,  std::string mes, int id, const char *type)
{
	json11::Json json = json11::Json::object {
		{ "message",  mes },
		{ "type",  type },
		{ "id", id}
	};
	_server.send(hdl, json.dump() , websocketpp::frame::opcode::text);	
}


void SendError(websocketpp::connection_hdl hdl,  std::string err, int id)
{
	json11::Json json = json11::Json::object {
		{ "error",  err },
		{ "id", id}
	};
	_server.send(hdl, json.dump() , websocketpp::frame::opcode::text);	
}

//////////////////////////////////////////////////
//////////////////////////////////////////////////
//
// RUN
//
//////////////////////////////////////////////////
//////////////////////////////////////////////////
struct _rundata {
	bool init;
	int fdin, fdout, fderr;
	websocketpp::connection_hdl hdl;
	fd_set read_fds,read_fds2;
	int id;	
	pid_t pid;
};

std::vector<struct _rundata *> rundata;

bool checkrun(struct _rundata *data)
{
	if (!data->init) {
		data->init = true;
		SendMessageType(data->hdl, std::string("runstart"), data->id, "event");

		fd_set read_fds; 
		FD_ZERO(&(data->read_fds));
		FD_ZERO(&(data->read_fds2));
	    FD_SET(data->fdout, &(data->read_fds));  
	    FD_SET(data->fderr, &(data->read_fds2));


	    fcntl(data->fdout, F_SETFL, fcntl(data->fdout, F_GETFL, 0) | O_NONBLOCK);
	    fcntl(data->fderr, F_SETFL, fcntl(data->fderr, F_GETFL, 0) | O_NONBLOCK);
	}
	bool ret = false;
	struct timeval tv;

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	int nfd2 = select(FD_SETSIZE, &(data->read_fds2), NULL, NULL, &tv);
	if (nfd2 > 0) {
		if (FD_ISSET(data->fderr, &(data->read_fds2))) {

			char buf[1024];
            ssize_t bytes = read(data->fderr, buf, sizeof(buf));
            if (bytes > 0) {
            	
            	buf[bytes] = '\0';
				SendMessageType(data->hdl,  buf, data->id, "stderr");
				
			} 
        } 
    }
    else {
    	FD_SET(data->fderr, &(data->read_fds2)); 
    }	

	tv.tv_sec = 0;
	tv.tv_usec = 0;
	int nfd = select(FD_SETSIZE, &(data->read_fds), NULL, NULL, &tv);
	if (nfd > 0) {
		if (FD_ISSET(data->fdout, &(data->read_fds))) {

			char buf[1024];
            ssize_t bytes = read(data->fdout, buf, sizeof(buf));
            if (bytes > 0) {
            	
            	buf[bytes] = '\0';
				SendMessageType(data->hdl,  buf, data->id, "stdout");
				
			} else {
                if (errno != EWOULDBLOCK) {
                	SendMessageType(data->hdl, std::string("runend"), data->id, "event");
                    close(data->fdin);
                    close(data->fdout);
                    close(data->fderr);
                    ret = true;
                } 
            } 
        } 

    }
    else {
    	FD_SET(data->fdout, &(data->read_fds)); 
    }	

    return ret;
}

void on_timer(websocketpp::lib::error_code const & ec) {
  
    for (std::vector<struct _rundata *>::iterator it = rundata.begin(); it !=  rundata.end(); ++it) {
    	if ( checkrun(*it) ) {
    		delete (*it);
    		rundata.erase(it);
    		break;
    	}
    }

  	_server.set_timer(10,on_timer);
}


void run(websocketpp::connection_hdl hdl,  json11::Json::array args , int id)
{

	struct _rundata *data = new _rundata;
	data->id = id;
	data->hdl = hdl; 
	data->init = false;

	const char **line = new const char* [args.size()+1];
	for (int i = 0; i < args.size(); i++) {
		line[i] = args[i].string_value().c_str();
	}
	line[args.size()] = NULL;
	pid_t pid = popen3(&(data->fdin), &(data->fdout), &(data->fderr),line);
	delete line;

	data->pid = pid;

	if (pid < 0) {
		SendError(hdl, "run error fork", id);
	} else {
#if true
		rundata.push_back(data);
#else
		while(1) {
			if (checkrun(data)) {
				break;
			}
	    }
#endif
	}
}

void cleanrun(websocketpp::connection_hdl hdl)
{
    for (std::vector<struct _rundata *>::iterator it = rundata.begin(); it !=  rundata.end(); ++it) {
    	if ( ((*it)->hdl).lock().get() == hdl.lock().get() ) {

    		close((*it)->fdin);
            close((*it)->fdout);
            close((*it)->fderr);
            kill( (*it)->pid , SIGKILL );

    		delete (*it);
    		rundata.erase(it);
    		break;
    	}
    }   
}

//////////////////////////////////////////////////
//////////////////////////////////////////////////
//
// Receiver - emit
//
//////////////////////////////////////////////////
//////////////////////////////////////////////////

struct _receiver {
	websocketpp::connection_hdl hdl;
	int id;	
};
std::map<std::string, struct _receiver *> receiver;


struct _transmitter {
	websocketpp::connection_hdl hdl;
	int id;
	struct _receiver *receiver;
};
std::vector<_transmitter*> transmitter;


void addreceiver(websocketpp::connection_hdl hdl,  std::string name , int id) {
	_receiver *data = new _receiver;
	data->hdl = hdl;
	data->id = id;
	receiver[name] = data;
}


void addtransmitter(websocketpp::connection_hdl hdl,  std::string name , int id) {
	std::map<std::string, struct _receiver *>::iterator it = receiver.find(name);
	if ( it != receiver.end()) {
		_transmitter *data = new _transmitter;
		data->receiver = it->second;
		data->hdl = hdl;
		data->id = id;
		transmitter.push_back(data);
	}
}

void cleanupreceiver(websocketpp::connection_hdl hdl)
{
	struct _receiver *toerase = NULL; 

	for (std::map<std::string, struct _receiver *>::iterator it = receiver.begin() ; it != receiver.end() ; it++ ) {
		if ( ((*it).second->hdl).lock().get() == hdl.lock().get() ) {
			toerase = it->second;
			receiver.erase(it);
			break;
		}
	}	



	for (std::vector<_transmitter*>::iterator it = transmitter.begin() ; it != transmitter.end() ; it++ ) {
		if ( ((*it)->hdl).lock().get() == hdl.lock().get() ) {
			transmitter.erase(it);
			break;
		}
		else if ((*it)->receiver == toerase ) {
			printf("close transmitter\n");
			_server.close((*it)->hdl, websocketpp::close::status::normal, "Success");
			transmitter.erase(it);
			break;
		}
	}
}

int c = 0;
void transmit(websocketpp::connection_hdl hdl,  server::message_ptr msg ) {
	for (std::vector<_transmitter*>::iterator it = transmitter.begin() ; it != transmitter.end() ; it++ ) {
		if ( ((*it)->hdl).lock().get() == hdl.lock().get() ) {
			_server.send((*it)->receiver->hdl, msg->get_payload(), msg->get_opcode());
		} 
	}
}


//////////////////////////////////////////////////
//////////////////////////////////////////////////
//
// Main
//
//////////////////////////////////////////////////
//////////////////////////////////////////////////
void on_message(websocketpp::connection_hdl hdl, server::message_ptr msg) {

	if ( msg->get_opcode() == websocketpp::frame::opcode::text) {

		std::string err;
		json11::Json json = json11::Json::parse(msg->get_payload(), err);

		if (!err.empty()) {
		  	SendError(hdl, err, -1);

		} else {
			if ( json.is_object()) {
				std::string command = "";
				std::string value = "";
				int id = -1;
				if (json["command"].is_string()) {
					command = json["command"].string_value();
				}
				if (json["value"].is_string()) {
					value = json["value"].string_value();
				}
				if (json["id"].is_number()) {
					id = json["id"].int_value();
				}

				if ( command == "log") {
					if (value != "") {
						logstable.push_back(value);
					} else {
						SendError(hdl,  std::string("log need value"), id);
					}

				} else if ( command == "run") {

					if (json["args"].is_array()) {
				
						run(hdl, json["args"].array_items() , id);
		
					} else {
						SendError(hdl,  std::string("run need args (array)"), id);
					}
				} else if ( command == "receiver") {
					if (json["name"].is_string()) {
						addreceiver(hdl, json["name"].string_value() , id);
					} else {
						SendError(hdl,  std::string("receiver need name"), id);
					}
				}else if ( command == "transmitter") {

					if (json["name"].is_string()) {
						addtransmitter(hdl, json["name"].string_value() , id);
					} else {
						SendError(hdl,  std::string("receiver need name"), id);
					}
				}else {
					SendError(hdl, std::string("command not found"), id);
				}
			}
		}  
	} 
	else {
		transmit(hdl, msg );	
	} 
}

void on_open(websocketpp::connection_hdl hdl) {
    
}

void on_close(websocketpp::connection_hdl hdl) {
	cleanrun(hdl);
	cleanupreceiver(hdl);

}

void on_http(websocketpp::connection_hdl hdl) {

    // Upgrade our connection handle to a full connection_ptr
    server::connection_ptr con = _server.get_con_from_hdl(hdl);
    std::string filename = con->get_resource();
    
	if ( filename == "/logs" ) {
		std::string ret = 
			"<!DOCTYPE html>\n"
			"<meta charset=\"utf8\">\n"
			"<title>websocketd console</title>\n"
			"<body>\n";
		for (std::vector<std::string>::iterator i = logstable.begin() ; i != logstable.end() ; i++) {
			ret += (*i) + "<br>";
		}
		ret += "</body>\n";
		logstable.clear(); 

		con->set_body(ret.c_str());

	}
	else {
		con->set_body( 
		"<!DOCTYPE html>\n"
		"<meta charset=\"utf8\">\n"
		"<title>websocketd console</title>\n"
		"\n"
		"<style>\n"
		"	.template {\n"
		"		display: none !important;\n"
		"	}\n"
		"	body, input {\n"
		"		font-family: dejavu sans mono, Menlo, Monaco, Consolas, Lucida Console, tahoma, arial;\n"
		"		font-size: 13px;\n"
		"	}\n"
		"	body {\n"
		"		margin: 0;\n"
		"	}\n"
		"	.header {\n"
		"		background-color: #efefef;\n"
		"		padding: 2px;\n"
		"		position: absolute;\n"
		"		top: 0;\n"
		"		left: 0;\n"
		"		right: 0;\n"
		"		height: 32px;\n"
		"	}\n"
		"	.header button {\n"
		"		font-size: 19px;\n"
		"		width: 30px;\n"
		"		margin: 2px 2px 0 2px;\n"
		"		padding: 0;\n"
		"		float: left;\n"
		"	}\n"
		"	.header .url-holder {\n"
		"		position: absolute;\n"
		"		left: 38px;\n"
		"		top: 4px;\n"
		"		right: 14px;\n"
		"		bottom: 9px;\n"
		"	}\n"
		"	.header .url {\n"
		"		border: 1px solid #999;\n"
		"		background-color: #fff;\n"
		"		width: 100%;\n"
		"		height: 100%;\n"
		"		border-radius: 2px;\n"
		"		padding-left: 4px;\n"
		"		padding-right: 4px;\n"
		"	}\n"
		"	.messages {\n"
		"		overflow-y: scroll;\n"
		"		position: absolute;\n"
		"		left: 0;\n"
		"		right: 0;\n"
		"		top: 36px;\n"
		"		bottom: 0;\n"
		"		border-top: 1px solid #ccc;\n"
		"	}\n"
		"	.message {\n"
		"		border-bottom: 1px solid #bbb;\n"
		"		padding: 2px;\n"
		"	}\n"
		"	.message-type {\n"
		"		font-weight: bold;\n"
		"		position: absolute;\n"
		"		width: 80px;\n"
		"		display: block;\n"
		"	}\n"
		"	.message-data {\n"
		"		margin-left: 90px;\n"
		"		display: block;\n"
		"		word-wrap: break-word;\n"
		"		white-space: pre;\n"
		"	}\n"
		"	.type-input,\n"
		"	.type-send {\n"
		"		background-color: #ffe;\n"
		"	}\n"
		"	.type-onmessage {\n"
		"		background-color: #eef;\n"
		"	}\n"
		"	.type-open,\n"
		"	.type-onopen {\n"
		"		background-color: #efe;\n"
		"	}\n"
		"	.type-close,\n"
		"	.type-onclose {\n"
		"		background-color: #fee;\n"
		"	}\n"
		"	.type-onerror,\n"
		"	.type-exception {\n"
		"		background-color: #333;\n"
		"		color: #f99;\n"
		"	}\n"
		"	.type-send .message-type,\n"
		"	.type-onmessage .message-type {\n"
		"		opacity: 0.2;\n"
		"	}\n"
		"	.type-input .message-type {\n"
		"		color: #090;\n"
		"	}\n"
		"	.send-input {\n"
		"		width: 100%;\n"
		"		border: 0;\n"
		"		padding: 0;\n"
		"		margin: -1px;\n"
		"		background-color: inherit;\n"
		"	}\n"
		"	.send-input:focus {\n"
		"		outline: none;\n"
		"	}\n"
		"</style>\n"
		"\n"
		"<header class=\"header\">\n"
		"	<button class=\"disconnect\" title=\"Disconnect\" style=\"display:none\">&times;</button>\n"
		"	<button class=\"connect\" title=\"Connect\" style=\"display:none\">&#x2714;</button>\n"
		"	<div class=\"url-holder\">\n"
		"		<input class=\"url\" type=\"text\" value=\"{{addr}}\" spellcheck=\"false\">\n"
		"	</div>\n"
		"</header>\n"
		"\n"
		"<section class=\"messages\">\n"
		"	<div class=\"message template\">\n"
		"		<span class=\"message-type\"></span>\n"
		"		<span class=\"message-data\"></span>\n"
		"	</div>\n"
		"	<div class=\"message type-input\">\n"
		"		<span class=\"message-type\">send &#xbb;</span>\n"
		"		<span class=\"message-data\"><input type=\"text\" class=\"send-input\" spellcheck=\"false\"></span>\n"
		"	</div>\n"
		"</section>\n"
		"\n"
		"<script>\n"
		"\n"
		"	var ws = null;\n"
		"\n"
		"	function ready() {\n"
		"		select('.connect').style.display = 'block';\n"
		"		select('.disconnect').style.display = 'none';\n"
		"\n"
		"		select('.connect').addEventListener('click', function() {\n"
		"			connect(select('.url').value);\n"
		"		});\n"
		"		select('.disconnect').addEventListener('click', function() {\n"
		"			disconnect();\n"
		"		});\n"
		"\n"
		"		select('.url').focus();\n"
		"		select('.url').addEventListener('keydown', function(ev) {\n"
		"			var code = ev.which || ev.keyCode;\n"
		"			// Enter key pressed\n"
		"			if (code  == 13) { 			\n"
		"				updatePageUrl();\n"
		"				connect(select('.url').value);\n"
		"			}\n"
		"		});\n"
		"		select('.url').addEventListener('change', updatePageUrl);\n"
		"\n"
		"		select('.send-input').addEventListener('keydown', function(ev) {\n"
		"			var code = ev.which || ev.keyCode;\n"
		"			// Enter key pressed\n"
		"			if (code == 13) { \n"
		"				var msg = select('.send-input').value;\n"
		"				select('.send-input').value = '';\n"
		"				send(msg);\n"
		"			}\n"
		"			// Up key pressed\n"
		"			if (code == 38) {\n"
		"				moveThroughSendHistory(1);\n"
		"			}\n"
		"			// Down key pressed\n"
		"			if (code == 40) {\n"
		"				moveThroughSendHistory(-1);\n"
		"			}\n"
		"		});\n"
		"		window.addEventListener('popstate', updateWebSocketUrl);\n"
		"		updateWebSocketUrl();\n"
		"	}\n"
		"\n"
		"	function updatePageUrl() {\n"
		"		var match = select('.url').value.match(new RegExp('^(ws)(s)?://([^/]*)(/.*)$'));\n"
		"		if (match) {\n"
		"			var pageUrlSuffix = match[4];\n"
		"			if (history.state != pageUrlSuffix) {\n"
		"				history.pushState(pageUrlSuffix, pageUrlSuffix, pageUrlSuffix);\n"
		"			}\n"
		"		}\n"
		"	}\n"
		"\n"
		"	function updateWebSocketUrl() {\n"
		"		var match = location.href.match(new RegExp('^(http)(s)?://([^/]*)(/.*)$'));\n"
		"		if (match) {\n"
		"			var wsUrl = 'ws' + (match[2] || '') + '://' + match[3] + match[4];\n"
		"			select('.url').value = wsUrl;\n"
		"		}\n"
		"	}\n"
		"\n"
		"	function appendMessage(type, data) {\n"
		"		var template = select('.message.template');\n"
		"		var el = template.parentElement.insertBefore(template.cloneNode(true), select('.message.type-input'));\n"
		"		el.classList.remove('template');\n"
		"		el.classList.add('type-' + type.toLowerCase());\n"
		"		el.querySelector('.message-type').textContent = type;\n"
		"		el.querySelector('.message-data').textContent = data || '';\n"
		"		el.querySelector('.message-data').innerHTML += '&nbsp;';\n"
		"		el.scrollIntoView(true);\n"
		"	}\n"
		"\n"
		"	function connect(url) {\n"
		"		function action() {\n"
		"			appendMessage('open', url);\n"
		"			try {\n"
		"				ws = new WebSocket(url);\n"
		"			} catch (ex) {\n"
		"				appendMessage('exception', 'Cannot connect: ' + ex);\n"
		"				return;\n"
		"			}\n"
		"\n"
		"			select('.connect').style.display = 'none';\n"
		"			select('.disconnect').style.display = 'block';\n"
		"\n"
		"			ws.addEventListener('open', function(ev) {\n"
		"				appendMessage('onopen');\n"
		"			});\n"
		"			ws.addEventListener('close', function(ev) {\n"
		"				select('.connect').style.display = 'block';\n"
		"				select('.disconnect').style.display = 'none';\n"
		"				appendMessage('onclose', '[Clean: ' + ev.wasClean + ', Code: ' + ev.code + ', Reason: ' + (ev.reason || 'none') + ']');\n"
		"				ws = null;\n"
		"				select('.url').focus();\n"
		"			});\n"
		"			ws.addEventListener('message', function(ev) {\n"
		"				if (typeof(ev.data) == \"object\") { \n"
		"					var rd = new FileReader();\n"
		"					rd.onload = function(ev){\n"
		"						appendMessage('onmessage', \"BLOB: \"+rd.result);\n"
		"					};\n"
		"					rd.readAsBinaryString(ev.data);\n"
		"				} else {\n"
		"					appendMessage('onmessage', ev.data);\n"
		"				}\n"
		"			});\n"
		"			ws.addEventListener('error', function(ev) {\n"
		"				appendMessage('onerror');\n"
		"			});\n"
		"\n"
		"			select('.send-input').focus();\n"
		"		}\n"
		"\n"
		"		if (ws) {\n"
		"			ws.addEventListener('close', function(ev) {\n"
		"				action();\n"
		"			});\n"
		"			disconnect();\n"
		"		} else {\n"
		"			action();\n"
		"		}\n"
		"	}\n"
		"\n"
		"	function disconnect() {\n"
		"		if (ws) {\n"
		"			appendMessage('close');\n"
		"			ws.close();\n"
		"		}\n"
		"	}\n"
		"\n"
		"	function send(msg) {\n"
		"		appendToSendHistory(msg);\n"
		"		appendMessage('send', msg);\n"
		"		if (ws) {\n"
		"			try {\n"
		"				ws.send(msg);\n"
		"			} catch (ex) {\n"
		"				appendMessage('exception', 'Cannot send: ' + ex);\n"
		"			}\n"
		"		} else {\n"
		"			appendMessage('exception', 'Cannot send: Not connected');\n"
		"		}\n"
		"	}\n"
		"\n"
		"	function select(selector) {\n"
		"		return document.querySelector(selector);\n"
		"	}\n"
		"\n"
		"	var maxSendHistorySize = 100;\n"
		"		currentSendHistoryPosition = -1,\n"
		"		sendHistoryRollback = '';\n"
		"\n"
		"	function appendToSendHistory(msg) {\n"
		"		currentSendHistoryPosition = -1;\n"
		"		sendHistoryRollback = '';\n"
		"		var sendHistory = JSON.parse(localStorage['websocketdconsole.sendhistory'] || '[]');\n"
		"		if (sendHistory[0] !== msg) {\n"
		"			sendHistory.unshift(msg);\n"
		"			while (sendHistory.length > maxSendHistorySize) {\n"
		"				sendHistory.pop();\n"
		"			}\n"
		"			localStorage['websocketdconsole.sendhistory'] = JSON.stringify(sendHistory);\n"
		"		}\n"
		"	}\n"
		"\n"
		"	function moveThroughSendHistory(offset) {\n"
		"		if (currentSendHistoryPosition == -1) {\n"
		"			sendHistoryRollback = select('.send-input').value;\n"
		"		}\n"
		"		var sendHistory = JSON.parse(localStorage['websocketdconsole.sendhistory'] || '[]');\n"
		"		currentSendHistoryPosition += offset;\n"
		"		currentSendHistoryPosition = Math.max(-1, Math.min(sendHistory.length - 1, currentSendHistoryPosition));\n"
		"\n"
		"		var el = select('.send-input');\n"
		"		el.value = currentSendHistoryPosition == -1\n"
		"			? sendHistoryRollback\n"
		"			: sendHistory[currentSendHistoryPosition];\n"
		"		setTimeout(function() {\n"
		"			el.setSelectionRange(el.value.length, el.value.length);\n"
		"		}, 0);\n"
		"	}\n"
		"\n"
		"	document.addEventListener(\"DOMContentLoaded\", ready, false);\n"
		"\n"
		"</script>\n"
		);
	}    
    
    con->set_status(websocketpp::http::status_code::ok);

}

int main() {

	_server.set_access_channels(websocketpp::log::alevel::none);

    _server.set_open_handler(&on_open);
    _server.set_close_handler(&on_close);
	_server.set_message_handler(&on_message);
	_server.set_http_handler(&on_http);

  	_server.init_asio();
  	_server.set_reuse_addr(true);
    _server.listen(9002);
    _server.start_accept();
    _server.set_timer(10,on_timer);
    _server.run();
  
}

//g++ -std=c++11 bridge.cpp json11.cpp -I . -I ./asio/include -pthread -o bridge




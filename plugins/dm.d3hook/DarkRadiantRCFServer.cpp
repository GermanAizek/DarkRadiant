#include "DarkRadiantRCFServer.h"
#include <RCF/TcpEndpoint.hpp>

#include "stream/stringstream.h"
#include "stream/textstream.h"

#include "iregistry.h"

DarkRadiantRCFServer::DarkRadiantRCFServer() :
	_server(RCF::TcpEndpoint(PORT_NUMBER)),
	_commandIsDone(false),
	_consoleEnabled(GlobalRegistry().get(RKEY_D3HOOK_CONSOLE_ENABLED) == "1")
{
	_server.bind<DarkRadiantRCFService>(*this);
	globalOutputStream() << "Starting DarkRadiant RCF Server.\n";
    _server.start(false);
}

DarkRadiantRCFServer::~DarkRadiantRCFServer()
{
	_server.stop();
	globalOutputStream() << "DarkRadiant RCF Server stopped.\n";
}

void DarkRadiantRCFServer::cycle() {
	_server.cycle();
}

void DarkRadiantRCFServer::writeToConsole(const std::string& text) {
	if (_consoleEnabled) {
		globalOutputStream() << text.c_str();
	}
}

bool DarkRadiantRCFServer::commandIsDone() const {
	return _commandIsDone;
}

void DarkRadiantRCFServer::signalCommandDone() {
	_commandIsDone = true;
	globalOutputStream() << "DarkMod reported: Command is done\n";
}

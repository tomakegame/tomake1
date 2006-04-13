#include <wz/socket.h>
#include <wz/exception.h>
#include "worker-thread.h"

using namespace Wz;

WorkerThread::WorkerThread(long pid, ServerSocket *server)
	: wxThread(wxTHREAD_JOINABLE),
	  analyzer(),
	  parser(&analyzer)
{
	this->pid = pid;
	this->server = server;
	server->ref();
	stopped = false;
	status = OK;
}

WorkerThread::~WorkerThread() {
	server->unref();
}

wxThread::ExitCode
WorkerThread::Entry() {
	Socket *socket;
	InputStream *input;

	try {
		socket = server->accept(3000);
		if (input == NULL) {
			status = ERROR;
			error = wxT("The internal disassembler program failed to start.");
			return 0;
		}
		input = socket->getInputStream();
	} catch (IOException &e) {
		status = ERROR;
		error = wxT("Socket error: ") + e.getMessage();
		return 0;
	}

	while (!input->eof() && analyzer.getState() != PacketLengthAnalyzer::DONE
	       && analyzer.getState() != PacketLengthAnalyzer::FAILED
	       && !stopped) {
		char buffer[1024 * 24];
		int size;

		size = input->read(buffer, sizeof(buffer));
		if (size > 0) {
			parser.addData(buffer, size);
		}
	}

	parser.setEOF();
	return 0;
}

PacketLengthAnalyzer &
WorkerThread::getAnalyzer() {
	return analyzer;
}

void
WorkerThread::stop() {
	stopped = true;
	status = STOPPED;
}

WorkerThread::Status
WorkerThread::getStatus() {
	return status;
}

wxString
WorkerThread::getError() {
	return error;
}

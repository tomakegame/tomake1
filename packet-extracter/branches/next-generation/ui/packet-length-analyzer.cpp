#include <stdio.h>
#include <stdlib.h>
#include "packet-length-analyzer.h"
#include "utils.h"

PacketLengthAnalyzer::PacketLengthAnalyzer()
	: packetLengthFunctionStart(wxS("push   ebp"), wxRE_NOSUB),
	  packetLengthFunctionEnd  (wxS("ret "), wxRE_NOSUB),
	  progressRegex(wxS("^Progress: (.*)")),
	  movDword(wxS("mov    DWORD PTR \\[(.*)\\],(.*)")),
	  movToEbx(wxS("mov    ebx,(.*)"))
{
	state = FINDING_PACKET_LENGTH_FUNCTION;
	ebx = 0;
	progress = 0;
}

PacketLengthAnalyzer::~PacketLengthAnalyzer() {
}

void
PacketLengthAnalyzer::processLine(const wxString &line) {
	static wxString firstPacketSwitch(wxT("mov    DWORD PTR [ebp-8],0x187"));

	switch (state) {
	case FINDING_PACKET_LENGTH_FUNCTION:
		addToBacklog(line);
		if (line.Contains(firstPacketSwitch)) {
			// Find the start of the packet length function in the backlog,
			// and analyze everything in the backlog starting from that point,
			// plus the current line.

			int start = findPacketLengthFunction();
			if (start == -1) {
				break;
			}

			int i = start;
			while (i < (int) backlog.GetCount() && state == ANALYZING_PACKET_LENGTHS) {
				analyzeLine(backlog[i]);
				i++;
			}
		}
		break;

	case ANALYZING_PACKET_LENGTHS:
		if (packetLengthFunctionEnd.Matches(line)) {
			state = DONE;
			progress = 100;
		} else {
			analyzeLine(line);
		}
		break;

	default:
		break;
	};
}

void
PacketLengthAnalyzer::processEOF() {
	switch (state) {
	case FINDING_PACKET_LENGTH_FUNCTION:
	case ANALYZING_PACKET_LENGTHS:
		setFailed(wxT("End of file reached unexpectedly."));
		break;
	default:
		break;
	};
}

PacketLengthAnalyzer::State
PacketLengthAnalyzer::getState() {
	return state;
}

wxString
PacketLengthAnalyzer::getError() {
	return error;
}

PacketLengthMap&
PacketLengthAnalyzer::getPacketLengths() {
	return lengths;
}

double
PacketLengthAnalyzer::getProgress() {
	return progress;
}

void
PacketLengthAnalyzer::addToBacklog(const wxString &line) {
	backlog.Add(line);
	if (backlog.GetCount() > MAX_BACKLOG_SIZE) {
		backlog.RemoveAt(0);
	}
}

int
PacketLengthAnalyzer::findPacketLengthFunction() {
	int result = -1;
	int i = backlog.GetCount() - 1;

	while (i >= 0 && result == -1) {
		if (packetLengthFunctionStart.Matches(backlog[i])) {
			result = i;
			state = ANALYZING_PACKET_LENGTHS;
		}
		i--;
	}

	if (result == -1) {
		setFailed(wxT("Cannot find packet length function."));
	}

	return result;
}

/**
 * Convert a hexadecimal number to an int.
 *
 * @param hex A hexadecimal number in the form of "0x123".
 * @return Whether the conversion was successful.
 */
static bool
hexToInt(const wxString &hex, unsigned int &result) {
	return sscanf(hex.ToAscii(), "%x", &result) == 1;
}

void
PacketLengthAnalyzer::analyzeLine(const wxString &line) {
	// This is a progress message:
	// Progress: (double number)
	if (progressRegex.Matches(line)) {
		wxString progressString = progressRegex.GetMatch(line, 1);
		progress = strtod(progressString.ToAscii(), NULL);

	// Looking for something like:
	// mov   DWORD PTR [ebp-1],0x123
	} else if (movDword.Matches(line)) {
		wxString to = movDword.GetMatch(line, 1);
		wxString from = movDword.GetMatch(line, 2);
		static wxString ebp(wxT("ebp"));
		static wxString hexPrefix(wxT("0x"));
		static wxString eax(wxT("eax"));

		if (to.Contains(ebp) && from.StartsWith(hexPrefix)) {
			// Packet switch
			unsigned int value;

			if (!hexToInt(from, value)) {
				setFailed(wxString::Format(
					wxT("Invalid hexadecimal number encountered at line:\n%s"),
					line.c_str()));
			} else {
				packetSwitch = wxString::Format(wxT("%04X"), value);
			}

		} else if (to.Contains(eax)) {
			// Packet length
			unsigned int len;

			if (from == wxT("ebx")) {
				len = ebx;
			} else if (from.StartsWith(hexPrefix)) {
				if (!hexToInt(from, len)) {
					setFailed(wxString::Format(
						wxT("Invalid hexadecimal number encountered "
						"at line:\n%s"),
						line.c_str()));
				}
			} else {
				len = 0;
			}

			if (packetSwitch.Len() == 0) {
				setFailed(wxS("Packet length instruction encountered "
					  "but no packet switch instruction encountered."));
			} else {
				lengths[packetSwitch] = len;
			}
		}

	// Looking for something like:
	// mov   ebx,0x123
	} else if (movToEbx.Matches(line)) {
		// Remember the value of ebx.
		wxString value;

		value = movToEbx.GetMatch(line, 1);
		if (!hexToInt(value, ebx)) {
			setFailed(wxString::Format(
				wxT("Invalid hexadecimal number encountered at line:\n%s"),
				line.c_str()));
		}
	}
}

void
PacketLengthAnalyzer::setFailed(const wxString &error) {
	this->error = error;
	state = FAILED;
}

void
PacketLengthAnalyzer::setFailed(const wxChar *error) {
	setFailed(wxString(error));
}


#include <Arduino.h>


//  Try to figure out what our networking classes are
#if defined ARDUINO_ARCH_ESP8266
#include <ESP8266WiFi.h>
#include <WiFiUdp.h>
namespace
{
	ESP8266WiFiClass &Networking = WiFi;
	typedef WiFiUDP SpaUdp;
}
#elif defined ARDUINO_ARCH_ESP32
#include <WiFi.h>
namespace
{
	WiFiClass &Networking = WiFi;
	typedef WiFiUDP SpaUdp;
}
#elif defined ARDUINO_ARCH_AVR
#include <Ethernet.h>
namespace
{
	EthernetClass &Networking = Ethernet;
	typedef EthernetUDP SpaUdp;


}
#endif

#include "crc.h"
#include "BalBoaSpa.h"
#include "BalBoaMessages.h"


BalBoa::BalBoaSpa::BalBoaSpa()
{
	F_CRC_InicializaTabla();
	ResetInfo();
}


bool
BalBoa::BalBoaSpa::begin(
	unsigned long pollingInterval,
	unsigned long connectionTimeout)
{
	SpaUdp Udp;

	ResetInfo();

	_pollingInterval = pollingInterval;

	if (!Udp.begin(0))
	{
		return false;
	}

	IPAddress broadcast = Networking.localIP();

	broadcast[3] = 255;

	if (!Udp.beginPacket(broadcast, _discoveryPort))
	{
		Udp.stop();
		return false;
	}

	//  Anything at all is acceptable, so long as it starts with 'D'.
	Udp.write('D');

	if (!Udp.endPacket())
	{
		Udp.stop();
		return false;
	}

	const unsigned long tStart = millis();

	while ((millis() - tStart) < connectionTimeout)
	{
		if (Udp.parsePacket())
		{
			constexpr size_t buffSize = 256;
			char buffer[buffSize + 1];

			size_t responseSize = Udp.available();

			responseSize = min(responseSize, buffSize);

			responseSize = Udp.read(buffer, responseSize);

			if (responseSize)
			{
				buffer[responseSize] = '\0';

				_ipHotTub = Udp.remoteIP();

				Udp.flush();
				Udp.stop();

				_client.setTimeout(connectionTimeout);

				Reconnect();
				SendConfigRequest();
				SendControlConfigRequest();

				//  Don't send the filter request yet, we want a status update to arrive
				//  first to properly set the time format.
				// SendFilterConfigRequest();
				return true;
			}
		}
	}

	Udp.flush();
	Udp.stop();

	return false;
}


bool
BalBoa::BalBoaSpa::spaLocated() const
{
	return _ipHotTub != INADDR_NONE;
}


BalBoa::BalBoaSpa::operator bool() const
{
	return spaLocated();
}


const IPAddress &
BalBoa::BalBoaSpa::GetSpaIP()
{
	return _ipHotTub;
}


unsigned long
BalBoa::BalBoaSpa::getPollingInterval()
{
	return _pollingInterval;
}

void
BalBoa::BalBoaSpa::setPollingInterval(
	unsigned long seconds)
{
	_pollingInterval = seconds;
}


void
BalBoa::BalBoaSpa::SendFilterConfigRequest()
{
	if (!(_waitingForMessages & wfmFilter))
	{
		FilterConfigRequest message;

		SendMessage(&message);

		_waitingForMessages |= wfmFilter;
	}
}

void
BalBoa::BalBoaSpa::SendConfigRequest()
{
	if (!(_waitingForMessages & wfmConfig))
	{
		ConfigRequest message;
		SendMessage(&message);

		_waitingForMessages |= wfmConfig;
	}
}

void
BalBoa::BalBoaSpa::SendControlConfigRequest()
{
	if (!(_waitingForMessages & wfmControlConfig))
	{
		ControlConfigRequest message(true);
		SendMessage(&message);

		_waitingForMessages |= wfmControlConfig;
	}
}


void
BalBoa::BalBoaSpa::SendMessage(
	BalBoa::MessageBase *pMessage)
{

	Reconnect();
	pMessage->SetCRC();


	_client.write((byte *)pMessage, pMessage->_length + 2);
}

unsigned int BalBoa::BalBoaSpa::GetChanges()
{

	// Process incoming messages
	if (_client.connected())
	{
		int available = _client.available();

		while ((_bufferUsed >= 7) || (available > 0))
		{
			auto amountToRead = min(available, _maxMessageLength);
			amountToRead = min(amountToRead, _maxMessageLength - _bufferUsed);

			_bufferUsed += _client.read(_messageBuffer + _bufferUsed, amountToRead);

			if (_bufferUsed >= 7)
			{
				MessageBase *pMessageBase = reinterpret_cast<MessageBase *>(_messageBuffer);

				if (pMessageBase->_prefix == '\x7e')
				{
					byte messageLength = pMessageBase->_length;
					byte fullLength = messageLength + 2;

					if (fullLength >= _maxMessageLength)
					{
						Serial.println(F("Length too long?"));
						pMessageBase->Dump(_bufferUsed);
					}

					if (!pMessageBase->CheckCRC())
					{
						Serial.println(F("CRC mismatch?"));
						Serial.println(pMessageBase->CalcCRC());

						pMessageBase->Dump();
					}


					if (fullLength <= _bufferUsed)
					{
						if (_messageBuffer[fullLength - 1] == '\x7e')
						{
							unsigned long messageType = pMessageBase->_messageType;

							_lastMessageTime = millis();

							switch (messageType)
							{
							case msStatus:
								CrackStatusMessage(_messageBuffer);
								if (_filters._filter1.stStart.hour == UNKNOWN_VAL)
								{
									//  We wait until a status message has arrived so we
									//  know the right time format
									SendFilterConfigRequest();
								}
								break;

							case msConfigResponse:
								CrackConfigMessage(_messageBuffer);
								break;

							case msFilterConfig:
								CrackFilterMessage(_messageBuffer);
								break;

							case msControlConfig:  //  It's really version info
								CrackVersionMessage(_messageBuffer);
								break;

							case msControlConfig2:
							case msSetTempRange:
								//  Do nothing
								break;

							default:
								Serial.println(F("Unknown message!"));
								pMessageBase->Dump();
								break;
							}

							if (_bufferUsed > fullLength)
							{
								memmove(_messageBuffer, _messageBuffer + fullLength, _bufferUsed - fullLength);
								_bufferUsed -= fullLength;
							}
							else
							{
								_bufferUsed = 0;
							}
						}
						else
						{
							Serial.println(F("Message missing terminators!"));
							_bufferUsed = 0;
						}
					}
					else
					{
						Serial.println(F("Await more data."));
						Serial.println(_bufferUsed);
						Serial.println(fullLength);
						for (auto i = 0; i < _bufferUsed; i++)
						{
							//Serial.printf("%02X ", _messageBuffer[i]);
						}
						Serial.println();

					}
				}
				else
				{
					Serial.println(F("No prefix!"));
					Serial.println(_bufferUsed);

					//for (auto i = 0; i < _bufferUsed; i++)
					//{
					//	Serial.printf("%02X ", _messageBuffer[i]);
					//}
					Serial.println();
					_bufferUsed = 0;
				}
			}
			available = _client.available();
		}

		//  If we've processed all our expected messages, and there is a polling interval,
		//  then shut down the connection.
		if ((_bufferUsed == 0) && (_pollingInterval > 0)
			&& (!_waitingForMessages))
		{
			_client.stop();
		}

			
		//  If  we are not receiving messages, then close the connection to reset it.
		if (_client.connected())
		{
			if ( ((millis() - _lastMessageTime) > 5000))
			{
				Serial.println(F("Message timeout!"));

				_client.stop();
				return _changes;
			}
		}
	}
	else
	{
		if (_pollingInterval > 0)
		{
			
			if ((millis() - _lastMessageTime) > _pollingInterval)
			{
				//	Oh hey, it's been a while, maybe see what the hot-tub is doing these
				//	days...
				Reconnect();
			}

			if ((millis() - _lastMessageTime) > _pollingInterval * 2)
			{
				//  WHY WON'T YOU ANSWER MY CALLS????
				//  Assume we've completely lost contact, need to reset.
				ResetInfo();
			}

		}
		else
		{
			Reconnect();
		}
	}

	if (_bufferUsed != 0)
	{
		Serial.println(F("More message needed!"));
	}

	return _changes;
}


const BalBoa::SpaTime &
BalBoa::BalBoaSpa::GetSpaTime() const
{
	_changes &= ~scTime;

	return _time;
}


const BalBoa::SpaTemp &
BalBoa::BalBoaSpa::GetSpaTemp() const
{
	_changes &= ~scTemp;

	return _currentTemp;
}


const BalBoa::SpaTemp &
BalBoa::BalBoaSpa::GetSetTemp() const
{
	_changes &= ~scSetPoint;

	return _setPoint;
}


BalBoa::PumpSpeed
BalBoa::BalBoaSpa::GetPump1Speed() const
{
	_changes &= ~scPump1;

	return _pump1Speed;
}


BalBoa::PumpSpeed
BalBoa::BalBoaSpa::GetPump2Speed() const
{
	_changes &= ~scPump2;

	return _pump2Speed;
}



const BalBoa::FilterInfo &
BalBoa::BalBoaSpa::GetFilterInfo() const
{
	_changes &= ~scFilterTimes;

	//  Make sure we have the latest time format.
	_filters._filter1.stStart.displayAs24Hr = _time.displayAs24Hr;
	_filters._filter2.stStart.displayAs24Hr = _time.displayAs24Hr;

	return _filters;
}

BalBoa::RunningFilter
BalBoa::BalBoaSpa::GetRunningFilter() const
{
	_changes &= ~scFilterRunning;

	BalBoa::RunningFilter rf = rfNone;

	if (_filter1Running == tsTrue)
	{
		rf = rf1;
	}
	else if (_filter2Running == tsTrue)
	{
		rf = rf2;
	}

	return rf;
}


BalBoa::TriState
BalBoa::BalBoaSpa::IsTimeUnset() const
{
	return _timeUnset;
}

BalBoa::TriState
BalBoa::BalBoaSpa::IsHeating() const
{
	_changes &= ~scHeating;

	return _heating;
}

BalBoa::TriState
BalBoa::BalBoaSpa::IsLightOn() const
{
	_changes &= ~scLights;

	return _lights;
}

BalBoa::TriState
BalBoa::BalBoaSpa::IsHighRange() const
{
	return _rangeHigh;
}


const BalBoa::VersionInfo &
BalBoa::BalBoaSpa::GetVersion() const
{
	_changes &= ~scVersion;

	return _version;
}


uint8_t
BalBoa::BalBoaSpa::GetPanelMessages() const
{
	_changes &= ~scPanelMessages;
	return _messages;
}


BalBoa::TriState
BalBoa::BalBoaSpa::IsPriming() const
{
	_changes &= ~scPriming;

	return _priming;
}


void
BalBoa::BalBoaSpa::SetTime(
	const BalBoa::SpaTime &time)
{
	SetSpaTime newTime(time);

	//  Mark current time as unknown to force update.
	_time.hour = UNKNOWN_VAL;
	_time.minute = UNKNOWN_VAL;

	SendMessage(&newTime);
}


void
BalBoa::BalBoaSpa::ToggleLights()
{
	ToggleItemMessage message(BalBoa::tiLights);

	SendMessage(&message);
}

void
BalBoa::BalBoaSpa::TogglePump1()
{
	ToggleItemMessage message(BalBoa::tiPump1);

	
	SendMessage(&message);
}


void
BalBoa::BalBoaSpa::TogglePump2()
{
	ToggleItemMessage message(BalBoa::tiPump2);

	SendMessage(&message);
}


void
BalBoa::BalBoaSpa::ToggleTempRange()
{
	ToggleItemMessage message(BalBoa::tiTempRange);

	SendMessage(&message);
}


void
BalBoa::BalBoaSpa::ToggleTempScale()
{
	if (_tempCelsius != tsUnknown)
	{
		SetSpaTempScaleMessage message(!(bool)_tempCelsius);

		SendMessage(&message);
	}
}

void
BalBoa::BalBoaSpa::SetTemp(
	const BalBoa::SpaTemp &temp)
{
	SetSpaTempMessage message(temp);

	SendMessage(&message);
}


byte previousStatusMessage[sizeof(BalBoa::StatusMessage)];


void
BalBoa::BalBoaSpa::CrackStatusMessage(const byte *_messageBuffer)
{
	const StatusMessage *pMessage = reinterpret_cast<const StatusMessage *>(_messageBuffer);
	
	
	// Only mark this off if something changed.
	// _waitingForMessages &= ~wfmStatus;

	unsigned int newChanges = 0;

	if (pMessage->_hour != _time.hour)
	{
		_time.hour = pMessage->_hour;
		newChanges |= scTime;
	}

	if (pMessage->_minute != _time.minute)
	{
		_time.minute = pMessage->_minute;
		newChanges |= scTime;
	}

	if (pMessage->_24hrTime != _time.displayAs24Hr)
	{
		_time.displayAs24Hr = pMessage->_24hrTime;
		newChanges |= scTime;
		newChanges |= scFilterTimes;  //  Because time format has changed.
	}

	if (pMessage->_timeUnset != _timeUnset)
	{
		_timeUnset = static_cast<TriState>(pMessage->_timeUnset);
		newChanges |= scTime;
	}

	if (pMessage->_currentTemp != _currentTemp.temp)
	{
		_currentTemp.temp = pMessage->_currentTemp;
		_currentTemp.isCelsiusX2 = pMessage->_tempScaleCelsius;

		newChanges |= scTemp;
	}


	if (pMessage->_setTemp != _setPoint.temp)
	{
		_setPoint.temp = pMessage->_setTemp;
		_setPoint.isCelsiusX2 = pMessage->_tempScaleCelsius;

		newChanges |= scSetPoint;
	}


	if (static_cast<TriState>(pMessage->_tempRange) != _rangeHigh)
	{
		_rangeHigh = static_cast<TriState>(pMessage->_tempRange);
		newChanges |= scSetPoint;
	}

	if (static_cast<TriState>(pMessage->_tempScaleCelsius) != _tempCelsius)
	{
		_tempCelsius = static_cast<TriState>(pMessage->_tempScaleCelsius);
		newChanges |= (scTemp | scSetPoint);
	}

	if (pMessage->_pump1 != _pump1Speed)
	{
		_pump1Speed = static_cast<BalBoa::PumpSpeed>(pMessage->_pump1);

		newChanges |= scPump1;
	}

	if (pMessage->_pump2 != _pump2Speed)
	{
		_pump2Speed = static_cast<BalBoa::PumpSpeed>(pMessage->_pump2);

		newChanges |= scPump2;
	}

	if (static_cast<TriState>(pMessage->_light != 0) != _lights)
	{
		_lights = static_cast<TriState>(pMessage->_light != 0);
		newChanges |= scLights;
	}

	if (static_cast<TriState>(pMessage->_heating != 0) != _heating)
	{
		_heating = static_cast<TriState>(pMessage->_heating != 0);
		newChanges |= scHeating;
	}

	if (static_cast<TriState>(pMessage->_filter1Running != 0) != _filter1Running)
	{
		_filter1Running = static_cast<TriState>(pMessage->_filter1Running != 0);
		newChanges |= scFilterRunning;
	}

	if (static_cast<TriState>(pMessage->_filter2Running != 0) != _filter2Running)
	{
		_filter2Running = static_cast<TriState>(pMessage->_filter2Running != 0);
		newChanges |= scFilterRunning;
	}

	if (pMessage->_panelMessage != _messages)
	{
		_messages = pMessage->_panelMessage;
		newChanges |= scPanelMessages;
	}

	if (static_cast<TriState>(pMessage->_priming != 0) != _priming)
	{
		_priming = static_cast<TriState>(pMessage->_priming != 0);
		newChanges |= scPriming;
	}

	if (newChanges)
	{
		_waitingForMessages &= ~wfmStatus;

		_changes |= newChanges;
	}

	//  Look to see how the 'unknown' areas change, maybe we can figure some more stuff
	//  out.  
	//
	StatusMessage *pMess = const_cast<StatusMessage *>(pMessage);

	//  Blank out the bits we know about.
	pMess->_priming          = 0;
	pMess->_currentTemp      = 0;
	pMess->_hour             = 0;
	pMess->_minute           = 0;
	pMess->_heatingMode      = 0;
	pMess->_tempScaleCelsius = 0;
	pMess->_panelMessage     &= 0b11110001;
	pMess->_24hrTime         = 0;
	pMess->_tempRange        = 0;
	pMess->_timeUnset        = 0;
	pMess->_heating          = 0;
	pMess->_pump1            = 0;
	pMess->_pump2            = 0;
	pMess->_circPump         = 0;
	pMess->_light            = 0;
	pMess->_setTemp          = 0;
	pMess->_suffix._check    = 0;

	//  Look for changes
	if (memcmp(pMess, previousStatusMessage, sizeof(StatusMessage)) != 0)
	{
		Serial.print(_time.hour), Serial.print(':'), Serial.println(_time.minute);

		StatusMessage M;
		memcpy(&M, previousStatusMessage, sizeof(StatusMessage));

		M.Dump();
		pMess->Dump();

		memcpy(previousStatusMessage, pMess, sizeof(StatusMessage));
	}
}


void
BalBoa::BalBoaSpa::CrackConfigMessage(const byte *_messageBuffer)
{
	_waitingForMessages &= ~wfmConfig;
}


void
BalBoa::BalBoaSpa::CrackFilterMessage(const byte *_messageBuffer)
{
	const FilterStatusMessage *pMessage = (const FilterStatusMessage *)_messageBuffer;

	_filters._filter1.stStart.hour             = pMessage->filter1StartHour;
	_filters._filter1.stStart.minute           = pMessage->filter1StartMinute;
	_filters._filter1.stStart.displayAs24Hr    = _time.displayAs24Hr;
	_filters._filter1.stDuration.hour          = pMessage->filter1DurationHours;
	_filters._filter1.stDuration.minute        = pMessage->filter1DurationMinutes;
	_filters._filter1.stDuration.displayAs24Hr = true;

	_filters._filter2Enabled                   = pMessage->filter2enabled;
	_filters._filter2.stStart.hour             = pMessage->filter2StartHour;
	_filters._filter2.stStart.minute           = pMessage->filter2StartMinute;
	_filters._filter2.stStart.displayAs24Hr    = _time.displayAs24Hr;
	_filters._filter2.stDuration.hour          = pMessage->filter2DurationHours;
	_filters._filter2.stDuration.minute        = pMessage->filter2DurationMinutes;
	_filters._filter2.stDuration.displayAs24Hr = true;

	_changes |= scFilterTimes;

	_waitingForMessages &= ~wfmFilter;
}


void
BalBoa::BalBoaSpa::CrackVersionMessage(const byte *_messageBuffer)
{
	const ControlConfigResponse *pMessage = (const ControlConfigResponse *)_messageBuffer;

	_version._currentSetup = pMessage->_currentSetup;

	for (auto i = 0; i < 3; i++)
	{
		_version._version[i] = pMessage->_version[i];
	}

	_version._signature = pMessage->_signature;

	memcpy(_version._name, pMessage->_name, sizeof(pMessage->_name));
	_version._name[8] = '\0';

	_changes |= scVersion;

	_waitingForMessages &= ~wfmControlConfig;
}


void
BalBoa::BalBoaSpa::Reconnect()
{
	if (!_client.connected() && spaLocated())
	{

		//Serial.println(F("Reconnecting to spa"));
		if (_client.connect(_ipHotTub, _comPort))
		{
			_lastMessageTime = millis();

			_bufferUsed = 0;

			//  Once we re-connect, see if there are messages we are expecting.  If so,
			//  resend the request.
			auto oldWaiting = _waitingForMessages;

			//  We *always* want a status message when we connect.
			_waitingForMessages = wfmStatus;

			if (oldWaiting & wfmConfig)
			{
				SendConfigRequest();
			}

			if (oldWaiting & wfmFilter)
			{
				SendFilterConfigRequest();
			}

			if (oldWaiting & wfmControlConfig)
			{
				SendControlConfigRequest();
			}
		}
	}
}

void
BalBoa::BalBoaSpa::ResetInfo()
{
	// Serial.println(F("Spa Data Reset!"));
	_client.stop();

	_lastMessageTime = millis();

	//  If we had valid data, mark everything as changed.
	if (_time.hour != UNKNOWN_VAL)
	{
		_changes = SpaChanges::scMASK;
	}

	_time           = {UNKNOWN_VAL, UNKNOWN_VAL, true};
	_currentTemp    = {UNKNOWN_VAL, false};
	_setPoint       = {UNKNOWN_VAL, false};
	_rangeHigh      = tsUnknown;
	_tempCelsius    = tsUnknown;
	_pump1Speed     = psUNKNOWN;
	_pump2Speed     = psUNKNOWN;
	_recirc         = psUNKNOWN;
	_timeUnset      = tsUnknown;
	_lights         = tsUnknown;
	_heating        = tsUnknown;
	_filter1Running = tsUnknown;
	_filter2Running = tsUnknown;

	_ipHotTub = INADDR_NONE;

	_filters = {{{UNKNOWN_VAL, UNKNOWN_VAL, true}, {UNKNOWN_VAL, UNKNOWN_VAL, true}},
	{{UNKNOWN_VAL, UNKNOWN_VAL, true}, {UNKNOWN_VAL, UNKNOWN_VAL, true}}, true};
		

	_version = {UNKNOWN_VAL, {UNKNOWN_VAL, UNKNOWN_VAL, UNKNOWN_VAL}, 0xFFFFFFFF, {'\0'}};

	_messages = pmNone;
}


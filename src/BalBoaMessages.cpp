// 
// 
// 
#include <Arduino.h>
#include "crc.h"
#include "BalBoaSpa.h"
#include "BalBoaMessages.h"




BalBoa::MessageBase::MessageBase(size_t size, unsigned long messageType)
{
	_prefix = '\x7e';
	_length = size - 2;
	_messageType = messageType & 0x00ffffff;
}


void BalBoa::MessageBase::Dump() const
{
	Dump(_length + 2);
}

void BalBoa::MessageBase::Dump(size_t size) const
{
	byte *pStart = (byte *)this;

	for (size_t i = 0; i < size; i++)
	{
		if (pStart[i] < 0x10)
		{
			Serial.print('0');
		}
		Serial.print(pStart[i], HEX), Serial.print(' ');
	}

	Serial.println();
}

void 
BalBoa::MessageBase::SetCRC()
{
	*(((byte *)this) + (this->_length)) = CalcCRC();
}

bool
BalBoa::MessageBase::CheckCRC() const
{
	return (*(((byte *)this) + (this->_length)) == CalcCRC());
}


byte
BalBoa::MessageBase::CalcCRC() const
{
	return F_CRC_CalculaCheckSum(&_length, _length - 1);
}



BalBoa::MessageSuffix::MessageSuffix()
{
	_check = 0xff;
	_suffix = '\x7e';
}


BalBoa::ConfigRequest::ConfigRequest()
	: MessageBaseOutgoing(sizeof(*this), msConfigRequest)
{}


BalBoa::FilterConfigRequest::FilterConfigRequest()
	: MessageBaseOutgoing(sizeof(*this), msFilterConfigRequest),
	_payload{0x01, 0x00, 0x00}
{
}

BalBoa::ControlConfigRequest::ControlConfigRequest(
	bool isType1)
	: MessageBaseOutgoing(sizeof(*this), msControlConfigRequest)
{
	if (isType1)
	{
		_payload[0] = 0x02;
		_payload[1] = 0x00;
		_payload[2] = 0x00;
	}
	else
	{
		_payload[0] = 0x00;
		_payload[1] = 0x00;
		_payload[2] = 0x01;
	}
}


BalBoa::SetSpaTime::SetSpaTime(
	const BalBoa::SpaTime &time)
	: MessageBaseOutgoing(sizeof(*this), msSetTimeRequest),
	_hour(time.hour), _displayAs24Hr(time.displayAs24Hr), _minute(time.minute)
{}

BalBoa::ToggleItemMessage::ToggleItemMessage(
	ToggleItem item)
	: MessageBaseOutgoing(sizeof(*this), msToggleItemRequest),
	_item(item), _r(0)
{}


BalBoa::SetSpaTempMessage::SetSpaTempMessage(
	const BalBoa::SpaTemp &temp)
	: MessageBaseOutgoing(sizeof(*this), msSetTempRequest),
	_temp(temp.temp)
{}


BalBoa::SetSpaTempScaleMessage::SetSpaTempScaleMessage(
	bool scaleCelsius)
	: MessageBaseOutgoing(sizeof(*this), msSetTempScaleRequest),
	_scale(scaleCelsius)
{}
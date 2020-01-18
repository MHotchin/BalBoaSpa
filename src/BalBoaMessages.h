//  Message definitions for talking to BalBoa spas over Wifi.  Most of this info was taken
//  from https://github.com/ccutrer/balboa_worldwide_app

#ifndef _BALBOAMESSAGES_h
#define _BALBOAMESSAGES_h

namespace BalBoa
{
//#define MESSAGE_ID(X,Y,Z) ((unsigned long)(X&0xff) | ((unsigned long)(Y&0xff)<<8) | ((unsigned long)(Z&0xff)<<16))

	constexpr uint32_t MESSAGE_ID(uint8_t b1, uint8_t b2, uint8_t b3)
	{
		return ((uint32_t)(b1) | ((uint32_t)(b2) << 8) | ((uint32_t)(b3) << 16));
	}

	//  Message IDs for responses from the hot-tub.
	enum SpaResponseMessageID : uint32_t
	{
		msStatus         = MESSAGE_ID(0xff, 0xaf, 0x13),
		msConfigResponse = MESSAGE_ID(0x0a, 0xbf, 0x94),
		msFilterConfig   = MESSAGE_ID(0x0a, 0xbf, 0x23),
		msControlConfig  = MESSAGE_ID(0x0a, 0xbf, 0x24),
		msControlConfig2 = MESSAGE_ID(0x0a, 0xbf, 0x2e),
		msSetTempRange   = MESSAGE_ID(0xff, 0xaf, 0x26)
	};


	//  Message Id's for commands send to the hot-tub
	enum SpaCommandMessageID : uint32_t
	{
		msConfigRequest          = MESSAGE_ID(0x0a, 0xbf, 0x04),
		msFilterConfigRequest    = MESSAGE_ID(0x0a, 0xbf, 0x22),
		msToggleItemRequest      = MESSAGE_ID(0x0a, 0xbf, 0x11),
		msSetTempRequest         = MESSAGE_ID(0x0a, 0xbf, 0x20),
		msSetTempScaleRequest    = MESSAGE_ID(0x0a, 0xbf, 0x27),
		msSetTimeRequest         = MESSAGE_ID(0x0a, 0xbf, 0x21),
		msControlConfigRequest   = MESSAGE_ID(0x0a, 0xbf, 0x22),
		msSetWiFiSettingsRequest = MESSAGE_ID(0x0a, 0xbf, 0x92), // Not implemented
		msSetFilterConfigRequest = MESSAGE_ID(0x0a, 0xbf, 0x23)  // Not implemented
	};

	//  Each message consists of a payload and 7 bytes overhead.  At the beginning of each
	//  message is a prefix (always 0x7e), then length of the message *without the prefix
	//  or suffix*, and a three byte message ID.
	struct MessageBase
	{
		byte _prefix;
		byte _length;
		uint32_t _messageType : 24;

		void Dump() const;
		void Dump(size_t) const;
		void SetCRC();
		bool CheckCRC() const;
		byte CalcCRC() const;

	protected:
		MessageBase(size_t, unsigned long);
		MessageBase() = default;
	} __attribute__((packed));

	//  The last two bytes of overhead are a check byte (8 bit CRC), and a suffix (also
	//  always 0x7e).
	struct MessageSuffix
	{
		byte _check;
		byte _suffix;

		MessageSuffix();
	} __attribute__((packed));


	//  Some type safety, make sure the right message ID's are applied to the right
	//  messages.
	struct MessageBaseIncoming : public MessageBase
	{
	};

	struct MessageBaseOutgoing : public MessageBase
	{
	protected:
		MessageBaseOutgoing(size_t size, SpaCommandMessageID id) 
			: MessageBase(size, id)
		{};
	};



	//  Following messages are only received.  They are never instantiated, hence no
	//  constructor.  
	//
	//  Using bitfields, we can read the message directly from memory without any
	//  bit-twiddling.
	//
	//  As needed, unknown areas are marked as '_r*'.  Note that some bitfields are
	//  skipped rather than marked.

	struct ConfigResponseMessage : public MessageBaseIncoming
	{
		byte _r[25];    //  Unknown structure
		MessageSuffix _suffix;
	} __attribute__((packed));

	struct StatusMessage : public MessageBaseIncoming
	{                               // byte #
		byte _r1;                   // 00
		byte _priming : 1;          // 01
		byte : 0;                   //
		byte _currentTemp;          // 02
		byte _hour;                 // 03
		byte _minute;               // 04
		byte _heatingMode : 2;      // 05
		byte : 0;                   // 
		byte _panelMessage : 4;     // 06
		byte : 0;                   //
		byte _r2;                   // 07, 
		byte _holdTime;             // 08  if _systemHold is true
		byte _tempScaleCelsius : 1; // 09
		byte _24hrTime : 1;         //
		byte _filter1Running : 1;   // 
		byte _filter2Running : 1;   //
		byte : 0;                   //
		byte _r3 : 2;               // 10
		byte _tempRange : 1;        //
		byte _r4 : 1;               //
		byte _heating : 2;          //
		byte : 0;                   //
		byte _pump1 : 2;            // 11
		byte _pump2 : 2;            //
		byte : 0;                   //
		byte _r5;                   // 12
		byte _r6 : 1;               // 13
		byte _circPump : 1;         //
		byte : 0;                   //
		byte _light : 2;            // 14
		byte : 0;                   //
		byte _r7[3];                // 15->17
		byte _r7a : 1;              // both 18 & 19, bit 2 seem related to time not
		byte _timeUnset : 1;        // yet set.
		byte : 0;
		byte _r7b;                  // 19
		byte _setTemp;              // 20
		byte _r8 : 2;               // 21
		byte _systemHold : 1;
		byte : 0;
		byte _r9[2];                // 22, 23

		MessageSuffix _suffix;      

	} __attribute__((packed));

	struct FilterStatusMessage :public MessageBaseIncoming
	{                                // byte
		byte filter1StartHour;       // 00
		byte filter1StartMinute;     // 01
		byte filter1DurationHours;   // 02
		byte filter1DurationMinutes; // 03
		byte filter2StartHour : 7;   // 04
		byte filter2enabled : 1;     //
		byte filter2StartMinute;     // 05
		byte filter2DurationHours;   // 06
		byte filter2DurationMinutes; // 07

		MessageSuffix _suffix;       
	} __attribute__((packed));


	struct ControlConfigResponse : public MessageBaseIncoming
	{                        // byte
		byte _version[3];    // 00, 01, 02
		byte _r1;            // 03
		byte _name[8];       // 04->11
		byte _currentSetup;  // 12
		uint32_t _signature; // 13->16
		byte _r2[4];         // 17->20

		MessageSuffix _sufffix;
	} __attribute__((packed));


	struct ControlConfig2Response : public MessageBaseIncoming
	{
		byte _r1[6];

		MessageSuffix _suffix;
	};

	//  Following messages are only sent.  Once contructed, they are ready to go.
	struct ConfigRequest : public BalBoa::MessageBaseOutgoing
	{
		ConfigRequest();
		//  No content
		MessageSuffix _suffix;
	} __attribute__((packed));

	struct FilterConfigRequest : public BalBoa::MessageBaseOutgoing
	{
		FilterConfigRequest();

		byte _payload[3];

		MessageSuffix _suffix;
	} __attribute__((packed));

	struct ControlConfigRequest : public MessageBaseOutgoing
	{
		ControlConfigRequest(bool type1);

		byte _payload[3];

		MessageSuffix _suffix;
	}  __attribute__((packed));

	struct SetSpaTime : public MessageBaseOutgoing
	{
		SetSpaTime(const BalBoa::SpaTime &);

		byte _hour : 7;
		byte _displayAs24Hr : 1;
		byte _minute;

		MessageSuffix _suffix;
	} __attribute__((packed));


	enum ToggleItem
	{
		tiLights = 0x11,
		tiPump1 = 0x04,
		tiPump2 = 0x05,
		tiTempRange = 0x50
	};

	struct ToggleItemMessage : public MessageBaseOutgoing
	{
		ToggleItemMessage(ToggleItem Item);

		byte _item;
		byte _r;

		MessageSuffix _suffix;
	} __attribute__((packed));

	struct SetSpaTempMessage : public MessageBaseOutgoing
	{
		SetSpaTempMessage(const BalBoa::SpaTemp &);

		byte _temp;

		MessageSuffix _suffix;
	} __attribute__((packed));

	struct SetSpaTempScaleMessage : public MessageBaseOutgoing
	{
		SetSpaTempScaleMessage(bool scaleCelsius);

		byte _r1 = 0x01;
		byte _scale;

		MessageSuffix _suffix;
	} __attribute__((packed));

}
#endif


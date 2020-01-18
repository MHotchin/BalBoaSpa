

//  Try to determine the connection type we have based on what headers have been included.
#if defined WiFi_h

typedef WiFiClient SpaClient;
#define ETHERNET_INCLUDED 1
#elif defined ethernet_h_
#define ETHERNET_INCLUDED 1

typedef EthernetClient SpaClient;
#endif

namespace BalBoa
{
	//  Until the spa comes online and the code recieves data updates, everything is set
	//  with 'unknown' values.  When real data arrives, that will trigger change
	//  notifications.

	constexpr byte UNKNOWN_VAL = 0xff;


	//  Different kinds of changes tracked.  Each type of change has one *or more* data
	//  points associated with it.
	enum SpaChanges : unsigned int
	{
		scNONE = 0,                //  There are no changes to report

		scTime = 1 << 0,           //  Spa time changed
		scTemp = 1 << 1,           //  Spa temp changed
		scSetPoint = 1 << 2,       //  The set point or range changed
		scPump1 = 1 << 3,          //  Pump 1 speed
		scPump2 = 1 << 4,          //  Pump 2 speed
		scRecirc = 1 << 5,         //  Recirc pump speed (currently not implemented)
		scHeating = 1 << 6,        //  Heater on / off
		scFilterTimes = 1 << 7,    //  Times for both filter cycles
		scLights = 1 << 8,         //  Lights on / off
		scVersion = 1 << 9,        //  Balboa software version
		scFilterRunning = 1 << 10, //  What filter cycle is currently running
		scPanelMessages = 1 << 11, //  Message displayed on panel
		scPriming = 1 << 12,       //  Hot-tub is priming (power-on start-up)
		scMASK = (1 << 13) - 1
	};



	enum TriState : byte
	{
		tsFalse = false,
		tsTrue = true,
		tsUnknown = UNKNOWN_VAL
	};


	struct SpaTime
	{
		byte hour;
		byte minute;
		bool displayAs24Hr;
	};

	struct SpaTemp
	{
		byte temp;
		bool isCelsiusX2;
	};

	enum PumpSpeed : byte
	{
		psOff,
		psLow,
		psHigh,
		psUNKNOWN = UNKNOWN_VAL
	};


	struct FilterTimes
	{
		SpaTime stStart;
		SpaTime stDuration;
	};

	struct FilterInfo
	{
		FilterTimes _filter1;
		FilterTimes _filter2;
		bool _filter2Enabled;
	};

	enum RunningFilter
	{
		rfNone,
		rf1,
		rf2
	};


	struct VersionInfo
	{
		byte _currentSetup;
		byte _version[3];
		uint32_t _signature;
		char _name[9];
	};

	enum PanelMessages  //  So far looks like only 1 is displayed at a time
	{
		pmNone = 0,
		pmUnknown = 0x01,
		pmCheckGFCI = 0x02,
		pmCleanFilter = 0x04,
		pmChangeWater = 0x08
	};


	struct MessageBase;

#if defined ETHERNET_INCLUDED
	class BalBoaSpa
	{
	public:
		BalBoaSpa();
		bool begin(unsigned long pollingInterval = 60000,   // Milli-seconds
				   unsigned long connectionTimeout = 5000); // Milli-seconds
		bool spaLocated() const;
		explicit operator bool() const;   //  Returns 'true' once spa has been contacted
		const IPAddress &GetSpaIP();

		unsigned long getPollingInterval();
		void setPollingInterval(unsigned long pollingInterval);  //  Milli-seconds

		//  Call in your 'loop' function to get updates from the Spa.  Code will only
		//  report changes that you haven't acknowledged.
		unsigned int GetChanges(void);

		//  Pretend everything has changed.  Would force your code to retrieve and repaint
		//  everything, e.g. you switched to a different display screen and have just
		//  switched back.
		void TriggerChanges(void)
		{
			_changes = scMASK;
		};

		//  Get and acknowledge changes.         // Change flag affected
		const SpaTime &GetSpaTime() const;       // scTime
		const SpaTemp &GetSpaTemp() const;       // scTemp
		const SpaTemp &GetSetTemp() const;       // scSetPoint
		PumpSpeed GetPump1Speed() const;         // scPump1
		PumpSpeed GetPump2Speed() const;         // scPump2
		const FilterInfo &GetFilterInfo() const; // scFilterTimes
		RunningFilter GetRunningFilter() const;  // scFilterRunning
		TriState IsTimeUnset() const;
		TriState IsHeating() const;              // scHeating
		TriState IsLightOn() const;              // scLights
		TriState IsHighRange() const;            // scSetPoint
		const VersionInfo &GetVersion() const;   // scVersion
		uint8_t GetPanelMessages() const;        // scPanelMessages
		TriState IsPriming() const;              // scPriming
		void SendFilterConfigRequest();

		//  Calling any of these will likely cause change notifications to come back.  So,
		//  no need to explicitly update things on the client side, the change
		//  notifications will do that naturally.
		void SetTime(const SpaTime &);
		void ToggleLights();
		void TogglePump1();
		void TogglePump2();
		void ToggleTempRange();
		void ToggleTempScale();

		void SetTemp(const SpaTemp &);

		//  As yet unimplemented things
		//  void SetFilterTimes(const FilterInfo &);
		//  void SetSpaWiFiSettings(...);


	private:
		void Reconnect();
		void ResetInfo();

		void SendConfigRequest();
		void SendControlConfigRequest();

		void SendMessage(MessageBase *);

		void CrackStatusMessage(const byte *);
		void CrackConfigMessage(const byte *);
		void CrackFilterMessage(const byte *);
		void CrackVersionMessage(const byte *);

		typedef uint16_t portNum;
		static constexpr portNum _discoveryPort = 30303;
		static constexpr portNum _comPort = 4257;

		//  When polling, messages to wait for before disconnecting
		enum waitForMessage : byte
		{
			wfmStatus = 0x01,
			wfmFilter = 0x02,
			wfmControlConfig = 0x04,
			wfmConfig = 0x08
		};

		IPAddress _ipHotTub;
		SpaClient _client;

		unsigned long _pollingInterval;
		unsigned long _lastMessageTime;
		byte _waitingForMessages;
		static constexpr int _maxMessageLength = 40;
		byte _bufferUsed = 0;
		byte _messageBuffer[_maxMessageLength];

		mutable unsigned int _changes;

		//  Current view of the Spa.  As new data comes in, it's compared to the current
		//  view, and if different updated and change notifications set.
		SpaTime            _time;
		SpaTemp            _currentTemp;
		SpaTemp            _setPoint;
		TriState           _rangeHigh;
		TriState           _tempCelsius;
		PumpSpeed          _pump1Speed;
		PumpSpeed          _pump2Speed;
		PumpSpeed          _recirc;
		TriState           _timeUnset;
		TriState           _lights;
		TriState           _heating;
		mutable FilterInfo _filters;
		TriState           _filter1Running;
		TriState           _filter2Running;
		VersionInfo        _version;
		uint8_t            _messages;
		TriState           _priming;
	};
#endif
}
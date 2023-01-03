#pragma once

#include <vector>
#include <cstdint>
#include <string>
#include <stdexcept>


#ifdef _MSC_VER
#ifdef DLLEXPORT
#define REDAPP_EXPORT __declspec(dllexport)
#else
#define REDAPP_EXPORT __declspec(dllimport)
#endif
#else
#define REDAPP_EXPORT 
#endif

#ifdef _MSC_VER
#define NOT_EXPORTED(var) \
	__pragma(warning(push)) \
	__pragma(warning(disable : 4251)) \
	var; \
	__pragma(warning(pop))
#else
#define NOT_EXPORTED(var) var;
#endif

#define STANDARD_STRING_GETTER_DEFINITION(var) const std::string var();
#define STANDARD_DOUBLE_GETTER_DEFINITION(var) double var();
#define STANDARD_STRING_FIELD_DEFINITION(var) const std::string var();
#define JAVA_INTEGER_GETTER_DEFINITION(var) int get ## var();
#define STANDARD_INTEGER_SETTER_DEFINITION(var) void set ## var(int val);


struct WeatherCollection {
	double hour;
	uint_fast64_t epoch;
	double temp;
	double rh;
	double wd;
	double ws;
	double wg;	// gust
	double precip;
	double ffmc;
	double DMC;
	double DC;
	double BUI;
	double ISI;
	double FWI;
	int options;

	WeatherCollection() {
		hour = 0;
		epoch = 0;
		temp = 0;
		rh = 0;
		wd = 0;
		ws = 0;
		wg = -1.0;
		precip = 0;
		ffmc = -1;
		DMC = -1;
		DC = -1;
		BUI = -1;
		ISI = -1;
		FWI = -1;
		options = 0;
	}
};

#ifdef _MSC_VER
#define _GCC_NOTHROW 
#else
#define _GCC_NOTHROW noexcept
#endif

class NotImplementedException : public std::logic_error {
public:
	NotImplementedException() : std::logic_error("Function not yet implemented.") { }

	virtual char const * what() const _GCC_NOTHROW { return "Function not yet implemented."; }
};


namespace REDapp {
struct REDAPP_EXPORT JavaClassDef {
	void* data;
	NOT_EXPORTED(std::string name)
};

class REDAPP_EXPORT JavaObject {
protected:
	void* _internal;
	JavaClassDef _type;
	bool m_isnew;

public:
	inline bool isValid() { return (_internal != nullptr) && (_type.data != nullptr); }
	inline void requiresDelete(bool del) { m_isnew = del; }

	void dispose();

public:
	JavaObject(void* internal, JavaClassDef type) { this->_internal = internal; this->_type = type; this->m_isnew = false; }
	JavaObject(const JavaObject& toCopy) { this->_internal = toCopy._internal; this->_type = toCopy._type; this->m_isnew = false; }
	JavaObject& operator=(const JavaObject& toCopy) { if (&toCopy != this) { this->_internal = toCopy._internal; this->_type = toCopy._type; this->m_isnew = false; } return *this; }
	~JavaObject() { dispose(); }
};

class REDAPP_EXPORT Interpolator : public JavaObject {
	friend class REDappWrapper;

public:
	Interpolator();
	virtual ~Interpolator() { }

	std::vector<std::pair<int, double>> SplineInterpolate(double* houroffsets, double* values, int size);
};

class REDAPP_EXPORT Cities : public JavaObject {
	friend class REDappWrapper;

private:
	NOT_EXPORTED(std::string m_name)

public:
	inline const char* name() const { return m_name.c_str(); }

	Cities(const std::string& name, void* internal) : JavaObject(internal, JavaClassDef()) { this->m_name = name; }
	Cities(const Cities& toCopy) : JavaObject(toCopy._internal, JavaClassDef()) { this->m_name = toCopy.m_name; }
	Cities& operator=(const Cities& toCopy) { JavaObject::operator=(toCopy); this->m_name = toCopy.m_name; return *this; }
};

/**
Stores location information for use in getting forecast weather from weatheroffice.gc.ca.
 */
class REDAPP_EXPORT LocationSmall : public JavaObject {
	friend class REDappWrapper;
	friend class ForecastCalculator;

public:
	LocationSmall(void* internal, JavaClassDef type) : JavaObject(internal, type) { }

public:
	STANDARD_STRING_FIELD_DEFINITION(locationName)
};

/**
Stores current weather information retreived form weatheroffice.gc.ca.
 */
class REDAPP_EXPORT GCWeather : public JavaObject {
	friend class REDappWrapper;

public:
	GCWeather(void* internal, JavaClassDef type) : JavaObject(internal, type) { }

public:
	/*
	Get the observed date/time.
	*/
	STANDARD_STRING_GETTER_DEFINITION(Observed)
	/*
	Get the current temperature.
	*/
	STANDARD_DOUBLE_GETTER_DEFINITION(Temperature)
	/*
	Get the current air pressure.
	*/
	STANDARD_DOUBLE_GETTER_DEFINITION(Pressure)
	/*
	Get the current visibility.
	*/
	STANDARD_DOUBLE_GETTER_DEFINITION(Visibility)
	/*
	Get the current humidity.
	*/
	STANDARD_DOUBLE_GETTER_DEFINITION(Humidity)
	/*
	Get the current windchill.
	*/
	STANDARD_DOUBLE_GETTER_DEFINITION(Windchill)
	/*
	Get the current dewpoint.
	*/
	STANDARD_DOUBLE_GETTER_DEFINITION(Dewpoint)
	/*
	Get the current cardinal or ordinal wind direction.
	*/
	STANDARD_STRING_GETTER_DEFINITION(WindDirection)
	/*
	Get the current wind speed.
	*/
	STANDARD_DOUBLE_GETTER_DEFINITION(WindSpeed)
};

class REDAPP_EXPORT LocationWeatherIterator : public JavaObject {
	friend class LocationWeather;

public:
	LocationWeatherIterator(void* internal, JavaClassDef type) : JavaObject(internal, type) { }
};

/**
A list of Canadian provinces.
 */
enum class REDAPP_EXPORT Province : short int {
	ALBERTA,
	BRITISH_COLUMBIA,
	MANITOBA,
	NEW_BRUNSWICK,
	NEWFOUNDLAND_AND_LABRADOR,
	NORTHWEST_TERRITORIES,
	NOVA_SCOTIA,
	NUNAVUT,
	ONTARIO,
	PRINCE_EDWARD_ISLAND,
	QUEBEC,
	SASKATCHEWAN,
	YUKON
};

enum class REDAPP_EXPORT Model : short int {
	GEM_DETER,
	NCEP,
	GEM,
	BOTH,
	CUSTOM
};

enum class REDAPP_EXPORT SpotWXModel : short int {
	HRDPS_CONTINENTAL,
	HRDPS_WEST,
	HRDPS_PRAIRIES,
	HRDPS_EAST,
	HRDPS_MARITIMES,
	HRDPS_ARCTIC,
	HRDPS_LANCASTER,
	RDPS,
	GDPS,
	NAM,
	GFS,
	HRRR,
	RAP,
	GEPS
};

enum class REDAPP_EXPORT Time : short int {
	MIDNIGHT,
	NOON
};

class REDAPP_EXPORT Calendar : public JavaObject {
	friend class ForecastCalculator;

public:
	Calendar();
	Calendar(void* internal, JavaClassDef type) : JavaObject(internal, type) { }

	void setYear(int year);
	void setMonth(int month);
	void setDay(int day);
	void setHour(int hour);
	void setMinute(int min);
	void setSeconds(int sec);

	int getYear();
	int getMonth();
	int getDay();
	int getHour();
	int getMinute();
	int getSeconds();

	std::string toString();
	void fromString(const std::string& val);
};

/**
For importing weather data.
 */
class REDAPP_EXPORT JavaWeatherStream : public JavaObject {
public:
	enum class InvalidHandler {
		FAILURE = 0,
		ALLOW = 1,
		FIX = 2
	};

public:
	JavaWeatherStream();
	JavaWeatherStream(void* internal, JavaClassDef type) : JavaObject(internal, type) { }

	void setLatitude(double latitude);
	void setLongitude(double longitude);
	void setTimezone(int64_t offset);
	void setDaylightSavings(int64_t amount);
	void setDaylightSavingsStart(int64_t offset);
	void setDaylightSavingsEnd(int64_t offset);
	inline void setAllowInvalid(InvalidHandler allow) { m_allowInvalid = allow; }

	/**
	Import hourly weather data. The returned list must be deleted by the caller if it is
	not nullptr.
	 */
	WeatherCollection* importHourly(std::string& filename, long* hr, size_t* length);

private:
	InvalidHandler m_allowInvalid{ InvalidHandler::FAILURE };
};

class REDAPP_EXPORT LocationWeather : public JavaObject {
public:
	LocationWeather(void* internal, JavaClassDef type) : JavaObject(internal, type) { }

	/**
	Get a number of hours of data. data must be large enough to hold at least size hours of weather data.
	@param size The number of hours of data to retrieve. If size is larger than the number of hours left to retrieve it will be updated to the number of hours actually retrieved.
	@param offset The number of hours to offset into the weather forecast.
	 */
	virtual void getWeather(IWXData* /*data*/, size_t* size, size_t /*offset = 0*/) { *size = 0; }

	/**
	Get the start date of the weather information in GMT.
	 */
	virtual Calendar startDate() { return Calendar(); }

	/**
	Gets the number of hours of data stored.
	 */
	virtual size_t size() { return 0; }
};

class REDAPP_EXPORT LocationWeatherGC : public LocationWeather {
	friend class ForecastCalculator;

public:
	LocationWeatherGC(void* internal, JavaClassDef type) : LocationWeather(internal, type) { }

	/**
	Get a number of hours of data. data must be large enough to hold at least size hours of weather data.
	@param size The number of hours of data to retrieve. If size is larger than the number of hours left to retrieve it will be updated to the number of hours actually retrieved.
	@param offset The number of hours to offset into the weather forecast.
	 */
	virtual void getWeather(IWXData* data, size_t* size, size_t offset = 0);

	/**
	Get the start date of the weather information in GMT.
	 */
	virtual Calendar startDate();

	/**
	Gets the number of hours of data stored.
	 */
	virtual size_t size();
};

class REDAPP_EXPORT ForecastCalculator : public JavaObject {
public:
	ForecastCalculator();
	explicit ForecastCalculator(const std::string& stream);
	ForecastCalculator(void* internal, JavaClassDef type) : JavaObject(internal, type), m_location(nullptr, JavaClassDef()) { m_model = Model::NCEP; m_timezone = 0; m_time = Time::NOON; m_hack50 = 50; }
	ForecastCalculator(const ForecastCalculator& toCopy) : JavaObject(toCopy._internal, toCopy._type), m_location(nullptr, JavaClassDef()) { this->_internal = toCopy._internal; this->_type = toCopy._type; m_model = toCopy.m_model; m_location = toCopy.m_location; m_date = toCopy.m_date; m_timezone = toCopy.m_timezone; m_members = toCopy.m_members; m_date = toCopy.m_date; m_hack50 = 50; }
	ForecastCalculator& operator=(const ForecastCalculator& toCopy) { if (&toCopy != this) { JavaObject::operator=(toCopy); m_model = toCopy.m_model; m_location = toCopy.m_location; m_date = toCopy.m_date; m_timezone = toCopy.m_timezone; m_members = toCopy.m_members; m_date = toCopy.m_date; } return *this; }

	inline void setLocation(const LocationSmall& loc) { m_location = loc; }
	inline void setModel(REDapp::Model mod) { m_model = mod; }
	inline void setTime(Time tim) { m_time = tim; }
	inline void setTimezone(int offset) { m_timezone = offset; }
	inline void setDate(const Calendar& date) { m_date = date; }
	inline void clearMembers() { m_members.clear(); }
	inline void addMember(int member) { m_members.push_back(member); }
	inline void setPercentile(int val) { m_hack50 = val; }

	std::string toStreamable();
	static void fromStreamable(std::string str);

	std::vector<LocationSmall> getForecastCities(Province prov);

	LocationWeatherGC getWeather(bool* success);

	static inline bool isStreamable(const std::string& stream) { return !stream.compare(0, 7, std::string("ACHERON")); }

private:
	REDapp::Model m_model;
	LocationSmall m_location;
	int m_timezone;
	Time m_time;
	Calendar m_date;
	int m_hack50;
	NOT_EXPORTED(std::vector<int> m_members)
};

/**
The wrapper class for the main Java calls to REDapp.
 */
class REDAPP_EXPORT REDappWrapper {
public:
	REDappWrapper();
	REDappWrapper(const REDappWrapper& toCopy);
	REDappWrapper& operator=(const REDappWrapper& toCopy);

	static bool CanLoadJava(bool reInitIfPossible = false);

	static unsigned long JavaLoadError();
	static std::string GetErrorDescription();
	static std::string GetDetailedError();
	static std::string GetJavaPath();
	static std::string GetJavaVersion();

	static bool InternetDetected();

	static void SetPathOverride(const std::string& path);

	/*
	  Fetch the cities in a given province that weatheroffice.gc.ca has current weather data for.
	 */
	std::vector<Cities> getCities(Province prov);
	/*
	Get the current weather for the given city from weatheroffice.gc.ca.
	 */
	GCWeather getGCWeather(Cities city);
	void getGCForecast(LocationSmall loc);

private:
	void Initialize();
};
}

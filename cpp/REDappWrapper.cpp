/**
 * WISE_REDapp_Lib_Wrapper: REDappWrapper.h
 * Copyright (C) 2023  WISE
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Affero General Public License as
 * published by the Free Software Foundation, either version 3 of the
 * License, or (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Affero General Public License for more details.
 * 
 * You should have received a copy of the GNU Affero General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "stdafx.h"

#include "types.h"

#include "ICWFGM_Weather.h"

#include "REDappWrapper.h"
#include "jvm_wrapper.h"
#include "java_types.h"

#include <map>
#include <sys/stat.h>
#include <stdlib.h>
#include <sstream>
#include <thread>
#include <mutex>
#include <future>
#include <list>
#include <wchar.h>
#include <jni.h>
#include <functional>

#include <boost/utility.hpp>
#define BOOST_SERIALIZATION_NO_LIB //I only want singleton, not all of the serialization library
#include <boost/serialization/singleton.hpp>


#define CACHE_FIELD_VALUES 0
#define CACHE_METHOD_VALUES 0
#define CACHE_CLASS_VALUES 0


template<typename store>
class REDappWrapperCache {
protected:
	struct cache_entry {
		cache_entry(size_t key, store value)
			: key(key),
			  value(value) {
		}

		size_t key;
		store value;
	};

	typedef std::map<size_t, cache_entry> Data;

	Data data;

public:
	REDappWrapperCache() {
		const wchar_t * c = L"", * d = L"";
		wmemcmp(c, d, 0);
	}

	void add(size_t key, store value) {
		typename Data::iterator find = data.find(key);
		if (find != data.end())
			find->second.value = value;
		else
			data.insert(std::make_pair(key, cache_entry(key, value)));
	}

	void clear() {
		data.clear();
	}
};


class REDappWrapperCache_class : public REDappWrapperCache<jclass> {
public:
	REDappWrapperCache_class()
		: REDappWrapperCache() {
	}

	jclass create(std::string name, NativeJVM* env) {
#if CACHE_CLASS_VALUES
		size_t key = std::hash<std::string>()(name);
		Data::iterator find = data.find(key);
		if (find != data.end())
			return find->second.value;
		else
		{
			jclass value = env->FindClass(name.c_str());
			data.insert(std::make_pair(key, cache_entry(key, value)));
			return value;
		}
#else
		return env->FindClass(name);
#endif
	}
};

class REDappWrapperCache_method : public REDappWrapperCache<jmethodID> {
public:
	REDappWrapperCache_method()
		: REDappWrapperCache() {
	}

	jmethodID create(jclass cls, std::string clsname, std::string name, std::string sig, NativeJVM* env) {
#if CACHE_METHOD_VALUES
		std::string keystring = clsname + name + sig;
		size_t key = std::hash<std::string>()(keystring);
		Data::iterator find = data.find(key);
		if (find != data.end())
			return find->second.value;
		else {
			jmethodID value = env->GetMethodID(cls, name.c_str(), sig.c_str());
			data.insert(std::make_pair(key, cache_entry(key, value)));
			return value;
		}
#else
		return env->GetMethodID(cls, name, sig);
#endif
	}

	jmethodID createStatic(jclass cls, std::string clsname, std::string name, std::string sig, NativeJVM* env) {
#if CACHE_METHOD_VALUES
		std::string keystring = clsname + name + sig;
		size_t key = std::hash<std::string>()(keystring);
		Data::iterator find = data.find(key);
		if (find != data.end())
			return find->second.value;
		else {
			jmethodID value = env->GetStaticMethodID(cls, name.c_str(), sig.c_str());
			data.insert(std::make_pair(key, cache_entry(key, value)));
			return value;
		}
#else
		return env->GetStaticMethodID(cls, name, sig);
#endif
	}
};

class REDappWrapperCache_field: public REDappWrapperCache<jfieldID> {
public:
	REDappWrapperCache_field()
		: REDappWrapperCache() {
	}

	jfieldID create(jclass cls, std::string clsname, std::string name, std::string sig, NativeJVM* env) {
#if CACHE_FIELD_VALUES
		std::string keystring = clsname + name + sig;
		size_t key = std::hash<std::string>()(keystring);
		Data::iterator find = data.find(key);
		if (find != data.end())
			return find->second.value;
		else {
			jfieldID value = env->GetFieldID(cls, name.c_str(), sig.c_str());
			data.insert(std::make_pair(key, cache_entry(key, value)));
			return value;
		}
#else
		return env->GetFieldID(cls, name, sig);
#endif
	}

	jfieldID createStatic(jclass cls, std::string clsname, std::string name, std::string sig, NativeJVM* env) {
#if CACHE_FIELD_VALUES
		std::string keystring = clsname + name + sig;
		size_t key = std::hash<std::string>()(keystring);
		Data::iterator find = data.find(key);
		if (find != data.end())
			return find->second.value;
		else {
			jfieldID value = env->GetStaticFieldID(cls, name.c_str(), sig.c_str());
			data.insert(std::make_pair(key, cache_entry(key, value)));
			return value;
		}
#else
		return env->GetStaticFieldID(cls, name, sig);
#endif
	}
};

class WorkerThread {
public:
	typedef std::function<void()> job_t;

	WorkerThread() {
		m_exit = false;
		m_hasjob = false;
		m_thread = std::unique_ptr<std::thread>(new std::thread(std::bind(&WorkerThread::Entry, this)));
	}

	~WorkerThread() {
		{
			std::lock_guard<std::mutex> lock(m_locker);
			m_exit = true;
			m_jobPending.notify_one();
		}
		m_thread->join();
	}

	void runJob(job_t job) {
		std::unique_lock<std::mutex> lock(m_locker);
		m_job = job;
		m_hasjob = true;
		m_jobPending.notify_one();

		m_jobComplete.wait(lock, [&]() { return !m_hasjob; });
	}

private:
	void Entry() {
		while (true) {
			std::unique_lock<std::mutex> tlock(m_locker);
			m_jobPending.wait(tlock, [&]() { return m_exit || m_hasjob; });

			if (m_exit)
				return;

			m_job();
			
			m_hasjob = false;
			m_jobComplete.notify_one();
		}
	}

public:
	std::unique_ptr<std::thread> m_thread;
	std::condition_variable m_jobPending;
	std::condition_variable m_jobComplete;
	job_t m_job;
	std::mutex m_locker;
	bool m_hasjob;
	bool m_exit;

	WorkerThread(const WorkerThread&);
	WorkerThread& operator = (const WorkerThread&);
};

class REDappWrapperPrivate : public boost::serialization::singleton<REDappWrapperPrivate> {
public:
	REDappWrapperPrivate() : m_thread(nullptr), m_jvm(nullptr) { }
	virtual ~REDappWrapperPrivate();

private:
	inline void init() { std::lock_guard<std::mutex> lock(m_initLock); if (m_jvm == nullptr || !m_jvm->IsValid()) _init(); }
	void _init();

public:
	int run(WorkerThread::job_t job);

	jclass GetClass(const std::string& name);
	jmethodID GetMethod(REDapp::JavaClassDef& cls, const std::string& name, const std::string& sig);
	jmethodID GetMethod(jclass cls, const std::string& clsname, const std::string& name, const std::string& sig);
	jmethodID GetStaticMethod(REDapp::JavaClassDef& cls, const std::string& name, const std::string& sig);
	jmethodID GetStaticMethod(jclass cls, const std::string& clsname, const std::string& name, const std::string& sig);
	jfieldID GetStaticField(REDapp::JavaClassDef& cls, const std::string& name, const std::string& sig);
	jfieldID GetStaticField(jclass cls, const std::string& clsname, const std::string& name, const std::string& sig);
	jfieldID GetField(REDapp::JavaClassDef& cls, const std::string& name, const std::string& sig);
	jfieldID GetField(jclass cls, const std::string& clsname, const std::string& name, const std::string& sig);
	jobject GetStaticObjectField(jclass cls, jfieldID fid);
	jobject CallStaticObjectMethod(jclass cls, jmethodID mid, jobject param);
	jobject CallStaticObjectMethod(jclass cls, jmethodID mid, jint param);
	jboolean CallStaticBooleanMethod(jclass cls, jmethodID mid, jobject param);
	jobject CallObjectMethodO(jobject obj, jmethodID mid, jobject o);
	jobject CallObjectMethodOO(jobject obj, jmethodID mid, jobject o1, jobject o2);
	jobject CallObjectMethodOOI(jobject obj, jmethodID mid, jobject o1, jobject o2, jint i1);
	jobject CallObjectMethod(jobject obj, jmethodID mid, jint ind);
	jdouble CallDoubleMethod(jobject obj, jmethodID mid, ...);
	jdouble CallObjectDoubleMethod(jobject obj, jmethodID mid, ...);
	jboolean CallBooleanMethod(jobject obj, jmethodID mid);
	jboolean CallBooleanMethod(jobject obj, jmethodID mid, int param);
	jint CallIntegerMethod(jobject obj, jmethodID mid);
	jint CallIntegerMethod(jobject obj, jmethodID mid, jint param);
	jlong CallLongMethod(jobject obj, jmethodID mid);
	void CallMethod(jobject obj, jmethodID mid, jobject param);
	void CallMethod(jobject obj, jmethodID mid, jint param);
	void CallMethod(jobject obj, jmethodID mid, jint param1, jint param2);
	void CallMethod(jobject obj, jmethodID mid, jdouble param);
	void CallMethod(jobject obj, jmethodID mid, jlong param);
	jobject NewObject(jclass cls, jmethodID constructor, jobject param);
	jobject NewObject(jclass cls, jmethodID constructor, jlong lval);

	void DeleteObject(jobject obj);

	jobject CallObjectField(jobject obj, jfieldID fid);
	jint CallIntField(jobject obj, jfieldID fid);
	jdouble CallDoubleField(jobject obj, jfieldID fid);
	jlong CallLongField(jobject obj, jfieldID fid);

	jintArray NewIntArray(int size);
	jdoubleArray NewDoubleArray(int size);
	jobjectArray NewObjectArray(int size, jclass cls);

	int SetIntField(jobject obj, jfieldID fld, jint val);
	int SetDoubleField(jobject obj, jfieldID fld, jdouble val);
	int SetLongField(jobject obj, jfieldID fld, jlong val);
	int SetObjectField(jobject obj, jfieldID fld, jobject val);

	jobject GetArrayElement(jobject obj, int* size, int index = -1);
	int SetObjectArrayElement(jobjectArray arr, int index, jobject val);

	std::string GetJStringContent(jstring str);
	jstring GetJString(std::string str);
	void FreeJString(jstring str);

	jboolean ExceptionCheck();

	inline bool Valid(bool reInitIfPossible) { if (!m_jvm || reInitIfPossible) init(); return m_jvm && m_jvm->IsValid(); }
	inline unsigned long LoadError() { if (!m_jvm) init(); if (m_jvm->GetError()) return m_jvm->GetError(); return m_jvm->GetLoadError(); }
	inline std::string ErrorDescription() { if (!m_jvm) init(); return m_jvm->GetErrorDescription(); }
	inline std::string DetailedError() { if (!m_jvm) init(); return m_jvm->GetDetailedError(); }
	inline std::string JavaPath() { init(); return m_jvm->GetJavaPath(); }
	inline std::string JavaVersion() { init(); return m_jvm->GetJavaVersion(); }

	void SetPathOverride(const std::string& path) { m_overridePath = path; }

public:
	jobject NativeProvinceToJava(REDapp::Province prov);
	jobject NativeModelToJava(REDapp::Model mod);
	jobject NativeTimeToJava(REDapp::Time tim);

private:
	std::string m_overridePath;
	std::unique_ptr<NativeJVM> m_jvm;
	REDappWrapperCache_class m_classCache;
	REDappWrapperCache_method m_methodCache;
	REDappWrapperCache_field m_fieldCache;
	WorkerThread *m_thread;
	std::mutex m_locker;
	std::mutex m_initLock;
};

int REDappWrapperPrivate::run(WorkerThread::job_t job) {
	std::lock_guard<std::mutex> lock(m_locker);
	m_thread->runJob(job);

	return 0;
}


#define STANDARD_STRING_GETTER(cls, var) \
	const std::string cls::var() \
	{ \
		REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance(); \
		jmethodID mid = priv.GetMethod(_type, "get"#var, "()Ljava/lang/String;"); \
		return priv.GetJStringContent((jstring)priv.CallObjectMethodO((jobject)_internal, mid, nullptr)); \
	}
#define STANDARD_DOUBLE_GETTER(cls, var) \
	double cls::var() \
	{ \
		REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance(); \
		jmethodID mid = priv.GetMethod(_type, "get"#var, "()Ljava/lang/Double;"); \
		return priv.CallObjectDoubleMethod((jobject)_internal, mid); \
	}
#define JAVA_INTEGER_GETTER(cls, var) \
	int cls::get ## var() \
	{ \
		REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance(); \
		jmethodID mid = priv.GetMethod(_type, "get"#var, "()I"); \
		return priv.CallIntegerMethod((jclass)_internal, mid); \
	}
#define STANDARD_STRING_FIELD(cls, var) \
	const std::string cls::var() \
	{ \
		REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance(); \
		jfieldID fid = priv.GetField(_type, std::string(#var), std::string("Ljava/lang/String;")); \
		return priv.GetJStringContent((jstring)priv.CallObjectField((jobject)_internal, fid)); \
	}
#define STANDARD_INTEGER_SETTER(cls, var) \
	void cls::set ## var(int val) \
	{ \
		REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance(); \
		jmethodID mid = priv.GetMethod(_type, std::string("set"#var), std::string("(I)V")); \
		priv.CallMethod((jobject)_internal, mid, val); \
	}


namespace REDapp {
REDappWrapper::REDappWrapper() {
	Initialize();
}

REDappWrapper::REDappWrapper(const REDappWrapper& toCopy) {
}

REDappWrapper& REDappWrapper::operator=(const REDappWrapper& toCopy) {
	return *this;
}

bool REDappWrapper::CanLoadJava(bool reInitIfPossible) {
	return REDappWrapperPrivate::get_mutable_instance().Valid(reInitIfPossible);
}

unsigned long REDappWrapper::JavaLoadError() {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	return priv.LoadError();
}

std::string REDappWrapper::GetErrorDescription() {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	return priv.ErrorDescription();
}

std::string REDappWrapper::GetDetailedError() {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	return priv.DetailedError();
}

std::string REDappWrapper::GetJavaPath() {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	return priv.JavaPath();
}

std::string REDappWrapper::GetJavaVersion() {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	return priv.JavaVersion();
}

bool REDappWrapper::InternetDetected() {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jclass WebDownloader = priv.GetClass("ca/hss/general/WebDownloader");
	jmethodID hasInternetConnection = priv.GetStaticMethod(WebDownloader, "ca/hss/general/WebDownloader", std::string("hasInternetConnection"), std::string("()Z"));
	return priv.CallStaticBooleanMethod(WebDownloader, hasInternetConnection, nullptr) ? true : false;
}

void REDappWrapper::SetPathOverride(const std::string& path) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	priv.SetPathOverride(path);
}

void REDappWrapper::Initialize() {
	if (InternetDetected()) {
		REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
		jclass Calculator = priv.GetClass("ca/weather/acheron/Calculator");
		jmethodID getLocations = priv.GetStaticMethod(Calculator, "ca/weather/acheron/Calculator", std::string("getLocations"), std::string("()Ljava/util/List;"));
		priv.CallStaticObjectMethod(Calculator, getLocations, nullptr);
	}
}

std::vector<Cities> REDappWrapper::getCities(Province prov) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jobject jprov = priv.NativeProvinceToJava(prov);
	jclass citiesHelper = priv.GetClass("ca/weather/current/Cities/CitiesHelper");
	jmethodID getCities = priv.GetStaticMethod(citiesHelper, "ca/weather/current/Cities/CitiesHelper", std::string("getCities"), std::string("(Lca/weather/forecast/Province;)[Lca/weather/current/Cities/Cities;"));
	jobjectArray citylist = (jobjectArray)priv.CallStaticObjectMethod(citiesHelper, getCities, jprov);
	jclass cities = priv.GetClass("ca/weather/current/Cities/Cities");
	jmethodID getName = priv.GetMethod(cities, "ca/weather/current/Cities/Cities", std::string("getName"), std::string("()Ljava/lang/String;"));
	int length;
	priv.GetArrayElement(citylist, &length);
	std::vector<Cities> list;
	for (int i = 0; i < length; i++) {
		jobject j = priv.GetArrayElement(citylist, NULL, i);
		jobject t = priv.CallObjectMethodO(j, getName, nullptr);
		list.push_back(Cities(priv.GetJStringContent((jstring)t), (void*)j));
	}
	return list;
}


Interpolator::Interpolator()
	: JavaObject(0, JavaClassDef()) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	JavaClassDef def = { priv.GetClass("ca/weather/acheron/Interpolator"), "ca/weather/acheron/Interpolator" };
	_type = def;
	jmethodID mid = priv.GetMethod(_type, std::string("<init>"), std::string("()V"));
	_internal = priv.NewObject((jclass)_type.data, mid, nullptr);
}

std::vector<std::pair<int, double>> Interpolator::SplineInterpolate(double* houroffsets, double* values, int size) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jintArray iarr = priv.NewIntArray(size);
	jclass hourvaluescls = priv.GetClass(std::string("ca/weather/acheron/Interpolator$HourValue"));
	jmethodID hourvaluesconst = priv.GetMethod(hourvaluescls, std::string("ca/weather/acheron/Interpolator$HourValue"), std::string("<init>"), std::string("()V"));
	jfieldID houroffsetfld = priv.GetField(hourvaluescls, std::string("ca/weather/acheron/Interpolator$HourValue"), std::string("houroffset"), std::string("D"));
	jfieldID valuefld = priv.GetField(hourvaluescls, std::string("ca/weather/acheron/Interpolator$HourValue"), std::string("value"), std::string("D"));
	jobjectArray oarr = priv.NewObjectArray(size, hourvaluescls);
	for (int i = 0; i < size; i++) {
		jobject obj = priv.NewObject(hourvaluescls, hourvaluesconst, nullptr);
		priv.SetDoubleField(obj, houroffsetfld, houroffsets[i]);
		priv.SetDoubleField(obj, valuefld, values[i]);
		priv.SetObjectArrayElement(oarr, i, obj);
		priv.DeleteObject(obj);
	}

	jmethodID splineint = priv.GetMethod(_type, std::string("splineInterpolate"), std::string("([Lca/weather/acheron/Interpolator$HourValue;)[Lca/weather/acheron/Interpolator$HourValue;"));
	jobjectArray retarr = (jobjectArray)priv.CallObjectMethodO((jobject)_internal, splineint, oarr);

	std::vector<std::pair<int, double>> ret;
	int len;
	priv.GetArrayElement(retarr, &len);
	for (int i = 0; i < len; i++) {
		jobject v = priv.GetArrayElement(retarr, nullptr, i);
		std::pair<int, double> val;
		val.first = priv.CallDoubleField(v, houroffsetfld);
		val.second = priv.CallDoubleField(v, valuefld);
		ret.push_back(val);
	}

	return ret;
}


std::vector<LocationSmall> ForecastCalculator::getForecastCities(Province prov) {
	if (REDappWrapper::InternetDetected()) {
		REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
		jclass Calculator = priv.GetClass("ca/weather/acheron/Calculator");
		jobject province = priv.NativeProvinceToJava(prov);
		jmethodID getLocations = priv.GetStaticMethod(Calculator, "ca/weather/acheron/Calculator", std::string("getLocations"), std::string("(Lca/weather/forecast/Province;)Ljava/util/List;"));
		jobject list = priv.CallStaticObjectMethod(Calculator, getLocations, province);
		jclass List = priv.GetClass("java/util/List");
		jmethodID size = priv.GetMethod(List, "java/util/List", std::string("size"), std::string("()I"));
		jmethodID get = priv.GetMethod(List, "java/util/List", std::string("get"), std::string("(I)Ljava/lang/Object;"));
		jclass LocationSmallClass = priv.GetClass("ca/weather/acheron/Calculator$LocationSmall");
		int s = priv.CallIntegerMethod(list, size);
		std::vector<LocationSmall> retval;
		JavaClassDef def = { LocationSmallClass, "ca/weather/acheron/Calculator$LocationSmall" };
		for (int i = 0; i < s; i++) {
			jobject ind = priv.CallObjectMethod(list, get, i);
			LocationSmall loc(ind, def);
			retval.push_back(loc);
		}
		return retval;
	}
	return std::vector<LocationSmall>();
}

GCWeather REDappWrapper::getGCWeather(Cities city) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jclass CurrentWeather = priv.GetClass("ca/weather/current/CurrentWeather");
	jmethodID CurrentWeatherInit = priv.GetMethod(CurrentWeather, "ca/weather/current/CurrentWeather", "<init>", "(Lca/weather/current/Cities/Cities;)V");
	jobject weather = priv.NewObject(CurrentWeather, CurrentWeatherInit, (jobject)city._internal);
	JavaClassDef def = { CurrentWeather, "ca/weather/current/CurrentWeather" };
	return GCWeather((void*)weather, def);
}

enum class CalendarType {
	ERA = 0,
	YEAR = 1,
	MONTH = 2,
	WEEK_OF_YEAR = 3,
	WEEK_OF_MONTH = 4,
	DATE = 5,
	DAY_OF_MONTH = 5,
	DAY_OF_YEAR = 6,
	DAY_OF_WEEK = 7,
	DAY_OF_WEEK_IN_MONTH = 8,
	AM_PM = 9,
	HOUR = 10,
	HOUR_OF_DAY = 11,
	MINUTE = 12,
	SECOND = 13,
	MILLISECOND = 14,
	ZONE_OFFSET = 15,
	DST_OFFSET = 16
};

Calendar::Calendar()
	: JavaObject(0, JavaClassDef()) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	JavaClassDef def = { priv.GetClass("java/util/Calendar"), "java/util/Calendar" };
	_type = def;
	jmethodID mid = priv.GetStaticMethod(_type, std::string("getInstance"), std::string("()Ljava/util/Calendar;"));
	_internal = priv.CallStaticObjectMethod((jclass)_type.data, mid, nullptr);
	mid = priv.GetMethod(_type, std::string("setTimeZone"), std::string("(Ljava/util/TimeZone;)V"));
	jclass TimeZone = priv.GetClass("java/util/TimeZone");
	jmethodID getTimeZone = priv.GetStaticMethod(TimeZone, "java/util/TimeZone", std::string("getTimeZone"), std::string("(Ljava/lang/String;)Ljava/util/TimeZone;"));
	jstring str = priv.GetJString("UTC");
	jobject timezone = priv.CallStaticObjectMethod(TimeZone, getTimeZone, str);
	jmethodID setTimeZone = priv.GetMethod(_type, std::string("setTimeZone"), std::string("(Ljava/util/TimeZone;)V"));
	priv.CallMethod((jobject)_internal, setTimeZone, timezone);
	priv.FreeJString(str);
}

void Calendar::setYear(int year) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jmethodID mid = priv.GetMethod(_type, std::string("set"), std::string("(II)V"));
	priv.CallMethod((jobject)_internal, mid, (jint)CalendarType::YEAR, year);
}

void Calendar::setMonth(int month) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jmethodID mid = priv.GetMethod(_type, std::string("set"), std::string("(II)V"));
	priv.CallMethod((jobject)_internal, mid, (jint)CalendarType::MONTH, month);
}

void Calendar::setDay(int day) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jmethodID mid = priv.GetMethod(_type, std::string("set"), std::string("(II)V"));
	priv.CallMethod((jobject)_internal, mid, (jint)CalendarType::DAY_OF_MONTH, day);
}

void Calendar::setHour(int hour) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jmethodID mid = priv.GetMethod(_type, std::string("set"), std::string("(II)V"));
	priv.CallMethod((jobject)_internal, mid, (jint)CalendarType::HOUR_OF_DAY, hour);
}

void Calendar::setMinute(int min) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jmethodID mid = priv.GetMethod(_type, std::string("set"), std::string("(II)V"));
	priv.CallMethod((jobject)_internal, mid, (jint)CalendarType::MINUTE, min);
}

void Calendar::setSeconds(int sec) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jmethodID mid = priv.GetMethod(_type, std::string("set"), std::string("(II)V"));
	priv.CallMethod((jobject)_internal, mid, (jint)CalendarType::SECOND, sec);
}

int Calendar::getYear() {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jmethodID mid = priv.GetMethod(_type, std::string("get"), std::string("(I)I"));
	return priv.CallIntegerMethod((jobject)_internal, mid, (jint)CalendarType::YEAR);
}

int Calendar::getMonth() {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jmethodID mid = priv.GetMethod(_type, std::string("get"), std::string("(I)I"));
	return priv.CallIntegerMethod((jobject)_internal, mid, (jint)CalendarType::MONTH);
}

int Calendar::getDay() {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jmethodID mid = priv.GetMethod(_type, std::string("get"), std::string("(I)I"));
	return priv.CallIntegerMethod((jobject)_internal, mid, (jint)CalendarType::DAY_OF_MONTH);
}

int Calendar::getHour() {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jmethodID mid = priv.GetMethod(_type, std::string("get"), std::string("(I)I"));
	return priv.CallIntegerMethod((jobject)_internal, mid, (jint)CalendarType::HOUR_OF_DAY);
}

int Calendar::getMinute() {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jmethodID mid = priv.GetMethod(_type, std::string("get"), std::string("(I)I"));
	return priv.CallIntegerMethod((jobject)_internal, mid, (jint)CalendarType::MINUTE);
}

int Calendar::getSeconds() {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jmethodID mid = priv.GetMethod(_type, std::string("get"), std::string("(I)I"));
	return priv.CallIntegerMethod((jobject)_internal, mid, (jint)CalendarType::SECOND);
}

std::string Calendar::toString() {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jstring format = priv.GetJString(std::string("yyyyMMddHHmmss z"));
	jmethodID getTimezone = priv.GetMethod(_type, std::string("getTimeZone"), std::string("()Ljava/util/TimeZone;"));
	jobject tz = priv.CallObjectMethodO((jobject)_internal, getTimezone, nullptr);
	jclass SimpleDateFormatterCls = priv.GetClass(std::string("java/text/SimpleDateFormat"));
	jmethodID constr = priv.GetMethod(SimpleDateFormatterCls, std::string("java/text/SimpleDateFormat"), std::string("<init>"), std::string("(Ljava/lang/String;)V"));
	jobject formatter = priv.NewObject(SimpleDateFormatterCls, constr, format);
	jmethodID setTimezone = priv.GetMethod(SimpleDateFormatterCls, std::string("java/text/SimpleDateFormat"), std::string("setTimeZone"), std::string("(Ljava/util/TimeZone;)V"));
	priv.CallMethod(formatter, setTimezone, tz);
	jmethodID gettime = priv.GetMethod(_type, std::string("getTime"), std::string("()Ljava/util/Date;"));
	jobject time = priv.CallObjectMethodO((jobject)_internal, gettime, nullptr);
	jmethodID formatid = priv.GetMethod(SimpleDateFormatterCls, std::string("java/text/SimpleDateFormat"), std::string("format"), std::string("(Ljava/util/Date;)Ljava/lang/String;"));
	jobject s = priv.CallObjectMethodO(formatter, formatid, time);
	jstring str = (jstring)s;
	priv.DeleteObject(formatter);
	std::string ret = priv.GetJStringContent(str);
	priv.FreeJString(str);
	return ret;
}

void Calendar::fromString(const std::string& val) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jstring format = priv.GetJString(std::string("yyyyMMddHHmmss z"));
	jstring text = priv.GetJString(val);
	jclass SimpleDateFormatterCls = priv.GetClass(std::string("java/text/SimpleDateFormat"));
	jmethodID constr = priv.GetMethod(SimpleDateFormatterCls, std::string("java/text/SimpleDateFormat"), std::string("<init>"), std::string("(Ljava/lang/String;)V"));
	jobject formatter = priv.NewObject(SimpleDateFormatterCls, constr, format);
	jmethodID settime = priv.GetMethod(_type, std::string("setTime"), std::string("(Ljava/util/Date;)V"));
	jmethodID parse = priv.GetMethod(SimpleDateFormatterCls, std::string("java/text/SimpleDateFormat"), std::string("parse"), std::string("(Ljava/lang/String;)Ljava/util/Date;"));
	jobject date = priv.CallObjectMethodO(formatter, parse, text);
	priv.CallMethod((jobject)_internal, settime, date);
	priv.DeleteObject(formatter);
}

JavaWeatherStream::JavaWeatherStream()
	 : JavaObject(0, JavaClassDef()) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	JavaClassDef def = { priv.GetClass("ca/cwfgm/weather/WeatherCondition"), "ca/cwfgm/weather/WeatherCondition" };
	_type = def;
	jmethodID mid = priv.GetMethod(_type, std::string("<init>"), std::string("()V"));
	_internal = priv.NewObject((jclass)_type.data, mid, nullptr);
}

void JavaObject::dispose() {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	if (m_isnew)
		priv.DeleteObject((jobject)_internal);
	_internal = nullptr;
}

void JavaWeatherStream::setLatitude(double latitude) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jmethodID mid = priv.GetMethod(_type, std::string("setLatitude"), std::string("(D)V"));
	priv.CallMethod((jobject)_internal, mid, latitude);
}

void JavaWeatherStream::setLongitude(double longitude) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jmethodID mid = priv.GetMethod(_type, std::string("setLongitude"), std::string("(D)V"));
	priv.CallMethod((jobject)_internal, mid, longitude);
}

void JavaWeatherStream::setTimezone(int64_t offset) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jmethodID mid = priv.GetMethod(_type, std::string("setTimezone"), std::string("(J)V"));
	priv.CallMethod((jobject)_internal, mid, offset);
}

void JavaWeatherStream::setDaylightSavings(int64_t amount) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jmethodID mid = priv.GetMethod(_type, std::string("setDaylightSavings"), std::string("(J)V"));
	priv.CallMethod((jobject)_internal, mid, amount);
}

void JavaWeatherStream::setDaylightSavingsStart(int64_t offset) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jmethodID mid = priv.GetMethod(_type, std::string("setDaylightSavingsStart"), std::string("(J)V"));
	priv.CallMethod((jobject)_internal, mid, offset);
}

void JavaWeatherStream::setDaylightSavingsEnd(int64_t offset) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jmethodID mid = priv.GetMethod(_type, std::string("setDaylightSavingsEnd"), std::string("(J)V"));
	priv.CallMethod((jobject)_internal, mid, offset);
}

WeatherCollection* JavaWeatherStream::importHourly(std::string& filename, long* hr, size_t* length) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	JavaClassDef outvardef = { priv.GetClass("ca/hss/general/OutVariable"), "ca/hss/general/OutVariable" };
	jmethodID outvarinit = priv.GetMethod(outvardef, std::string("<init>"), std::string("()V"));
	jobject outvar = priv.NewObject((jclass)outvardef.data, outvarinit, nullptr);
	jfieldID outvarvalue = priv.GetField(outvardef, std::string("value"), std::string("Ljava/lang/Object;"));
	jclass longCls = priv.GetClass(std::string("java/lang/Long"));
	jmethodID longInit = priv.GetMethod(longCls, std::string("java/lang/Long"), std::string("<init>"), std::string("(J)V"));
	jobject zerol = priv.NewObject(longCls, longInit, (jlong)0);
	priv.SetObjectField(outvar, outvarvalue, zerol);
	jstring f = priv.GetJString(filename);
	jobject retval;
	jmethodID ihMID = priv.GetMethod(_type, std::string("importHourly"),
		std::string(JMethodDefinition(JParameter(JTypeString) JObjectParameter(ca/hss/general/OutVariable) JTypeInt, JObjectParameter(java/util/List))));
	if (ihMID == nullptr) {
		ihMID = priv.GetMethod(_type, std::string("importHourly"),
			std::string(JMethodDefinition(JParameter(JTypeString) JObjectParameter(ca/hss/general/OutVariable), JObjectParameter(java/util/List))));
		retval = priv.CallObjectMethodOO((jobject)_internal, ihMID, f, outvar);
	}
	else
		retval = priv.CallObjectMethodOOI((jobject)_internal, ihMID, f, outvar, (jint)m_allowInvalid);
	jobject hrjava = priv.CallObjectField(outvar, outvarvalue);
	jmethodID longGetLong = priv.GetMethod(longCls, std::string("java/lang/Long"), std::string("longValue"), std::string("()J"));
	*hr = priv.CallLongMethod(hrjava, longGetLong);

	priv.FreeJString(f);
	priv.DeleteObject(outvar);
	priv.DeleteObject(zerol);

	if ((*hr == 0) || (*hr == 12803) || (*hr == 12805) || (*hr == (0x80000000 | 13))) {
		jclass listClass = priv.GetClass(std::string("java/util/List"));
		jmethodID listSize = priv.GetMethod(listClass, std::string("java/util/List"), std::string("size"), std::string("()I"));
		int size = priv.CallIntegerMethod(retval, listSize);
		if (size > 0) {
			WeatherCollection* wcollection = new WeatherCollection[size];
			jmethodID listGet = priv.GetMethod(listClass, std::string("java/util/List"), std::string("get"), std::string("(I)Ljava/lang/Object;"));
			JavaClassDef wcDef = { priv.GetClass("ca/cwfgm/weather/WeatherCondition$WeatherCollection"), "ca/cwfgm/weather/WeatherCondition$WeatherCollection" };
			jfieldID hourFld = priv.GetField(wcDef, std::string("hour"), std::string("D"));
			jfieldID epochFld = priv.GetField(wcDef, std::string("epoch"), std::string("J"));
			jfieldID tempFld = priv.GetField(wcDef, std::string("temp"), std::string("D"));
			jfieldID rhFld = priv.GetField(wcDef, std::string("rh"), std::string("D"));
			jfieldID wdFld = priv.GetField(wcDef, std::string("wd"), std::string("D"));
			jfieldID wsFld = priv.GetField(wcDef, std::string("ws"), std::string("D"));
			jfieldID precipFld = priv.GetField(wcDef, std::string("precip"), std::string("D"));
			jfieldID ffmcFld = priv.GetField(wcDef, std::string("ffmc"), std::string("D"));
			jfieldID DMCFld = priv.GetField(wcDef, std::string("DMC"), std::string("D"));
			jfieldID DCFld = priv.GetField(wcDef, std::string("DC"), std::string("D"));
			jfieldID BUIFld = priv.GetField(wcDef, std::string("BUI"), std::string("D"));
			jfieldID ISIFld = priv.GetField(wcDef, std::string("ISI"), std::string("D"));
			jfieldID FWIFld = priv.GetField(wcDef, std::string("FWI"), std::string("D"));
			jfieldID optionFld = priv.GetField(wcDef, std::string("options"), std::string("I"));

			for (int i = 0; i < size; i++) {
				jobject wc = priv.CallObjectMethod(retval, listGet, i);
				WeatherCollection coll;
				coll.hour = priv.CallDoubleField(wc, hourFld);
				coll.epoch = (uint_fast64_t)priv.CallLongField(wc, epochFld);
				coll.temp = priv.CallDoubleField(wc, tempFld);
				coll.rh = priv.CallDoubleField(wc, rhFld);
				coll.wd = priv.CallDoubleField(wc, wdFld);
				coll.ws = priv.CallDoubleField(wc, wsFld);
				coll.precip = priv.CallDoubleField(wc, precipFld);
				coll.ffmc = priv.CallDoubleField(wc, ffmcFld);
				coll.DMC = priv.CallDoubleField(wc, DMCFld);
				coll.DC = priv.CallDoubleField(wc, DCFld);
				coll.BUI = priv.CallDoubleField(wc, BUIFld);
				coll.ISI = priv.CallDoubleField(wc, ISIFld);
				coll.FWI = priv.CallDoubleField(wc, FWIFld);
				coll.options = priv.CallIntField(wc, optionFld);
				wcollection[i] = coll;
			}
			*length = size;
			return wcollection;
		}
		else {
			if (priv.ExceptionCheck()) {
				*length = 0;
				return nullptr;
			}
		}
	}
	else {
		if (priv.ExceptionCheck()) {
			*length = 0;
			return nullptr;
		}
	}

	*length = 0;
	return nullptr;
}

ForecastCalculator::ForecastCalculator()
	 : JavaObject(0, JavaClassDef()),
	   m_location(nullptr, JavaClassDef()) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	JavaClassDef def = { priv.GetClass("ca/weather/acheron/Calculator"), "ca/weather/acheron/Calculator" };
	_type = def;
	jmethodID mid = priv.GetMethod(_type, std::string("<init>"), std::string("()V"));
	_internal = priv.NewObject((jclass)_type.data, mid, nullptr);
	m_model = Model::GEM_DETER;
	m_timezone = 0;
	m_hack50 = -1;
}


ForecastCalculator::ForecastCalculator(const std::string& stream)
	: JavaObject(0, JavaClassDef()),
	m_location(nullptr, JavaClassDef()) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	JavaClassDef def = { priv.GetClass("ca/weather/acheron/Calculator"), "ca/weather/acheron/Calculator" };
	_type = def;
}


#if !defined(__INTEL_COMPILER) && !defined(__INTEL_LLVM_COMPILER)
static const int ForecastCalculatorVersion = 1;
#else
static constexpr int ForecastCalculatorVersion = 1;
#endif

std::string ForecastCalculator::toStreamable() {
	std::stringstream stream;
	stream << std::string("ACHERON;");
	stream << ForecastCalculatorVersion;
	stream << ";" << (short int)m_model << ";" << m_location.locationName() << ";" << m_timezone << ";" << m_date.toString() << ";" << (short int)m_time << ";";
	for (int i : m_members) {
		stream << i << ";";
	}
	return stream.str();
}

struct semicolon_is_space : std::ctype<char> {
	semicolon_is_space() : std::ctype<char>(get_table()) { }
	static mask const* get_table() {
		static mask rc[table_size];
		rc[';'] = std::ctype_base::space;
		return &rc[0];
	}
};

void ForecastCalculator::fromStreamable(std::string str) {
	throw NotImplementedException();
	std::stringstream stream(str);
	stream.imbue(std::locale(stream.getloc(), new semicolon_is_space));
}

LocationWeatherGC ForecastCalculator::getWeather(bool* success) {
	if (m_location.isValid()) {
		REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
			jfieldID fid = priv.GetField(m_location._type, "locationName", "Ljava/lang/String;");
		jmethodID setLocation = priv.GetMethod(_type, std::string("setLocation"), std::string("(Ljava/lang/String;)V"));
		jstring name = (jstring)priv.CallObjectField((jobject)m_location._internal, fid);
		priv.CallMethod((jobject)_internal, setLocation, name);
		priv.FreeJString(name);
		jmethodID setModel = priv.GetMethod(_type, std::string("setModel"), std::string("(Lca/weather/forecast/Model;)V"));
		jobject mod = priv.NativeModelToJava(m_model);
		priv.CallMethod((jobject)_internal, setModel, mod);
		priv.DeleteObject(mod);
		jmethodID setTime = priv.GetMethod(_type, std::string("setTime"), std::string("(Lca/weather/forecast/Time;)V"));
		jobject tm = priv.NativeTimeToJava(m_time);
		priv.CallMethod((jobject)_internal, setTime, tm);
		priv.DeleteObject(tm);
		if (m_model == REDapp::Model::CUSTOM) {
			jmethodID clearMembers = priv.GetMethod(_type, std::string("clearMembers"), std::string("()V"));
			priv.CallMethod((jobject)_internal, clearMembers, nullptr);
			jmethodID addMember = priv.GetMethod(_type, std::string("addMember"), std::string("(I)V"));
			for (std::vector<int>::iterator it = m_members.begin(); it != m_members.end(); ++it) {
				priv.CallMethod((jobject)_internal, addMember, (jint)*it);
			}
		}
		jclass WorldLocationClass = priv.GetClass("ca/hss/times/WorldLocation");
		jmethodID getTimeZoneFromOffset = priv.GetStaticMethod(WorldLocationClass, "ca/hss/times/WorldLocation", std::string("getTimeZoneFromOffset"), std::string("(I)Lca/hss/times/TimeZoneInfo;"));
		jint zero = 0;
		jobject timezone = priv.CallStaticObjectMethod(WorldLocationClass, getTimeZoneFromOffset, zero);
		jmethodID setTimezone = priv.GetMethod(_type, std::string("setTimezone"), std::string("(Lca/hss/times/TimeZoneInfo;)V"));
		priv.CallMethod((jobject)_internal, setTimezone, timezone);
		jmethodID setDate = priv.GetMethod(_type, std::string("setDate"), std::string("(Ljava/util/Calendar;)V"));
		priv.CallMethod((jobject)_internal, setDate, (jobject)m_date._internal);
		jboolean ret = 0;
		if (m_hack50 > 0 && m_hack50 < 100) {
			jmethodID setperc = priv.GetMethod(_type, std::string("setPercentile"), std::string("(I)V"));
			priv.CallMethod((jobject)_internal, setperc, (jint)m_hack50);
		}
		jmethodID calculate = priv.GetMethod(_type, std::string("calculate"), std::string("()Z"));
		ret = priv.CallBooleanMethod((jobject)_internal, calculate);
		if (ret) {
			*success = true;
			jclass LocationWeatherClass = priv.GetClass("ca/weather/acheron/LocationWeather");
			jint index = 0;
			jmethodID getLocationsWeatherData = priv.GetMethod(_type, std::string("getLocationsWeatherData"), std::string("(I)Lca/weather/acheron/LocationWeather;"));
			jobject weather = priv.CallObjectMethod((jobject)_internal, getLocationsWeatherData, index);
			JavaClassDef def = { LocationWeatherClass, "ca/weather/acheron/LocationWeather" };
			return LocationWeatherGC(weather, def);
		}
	}
	*success = false;
	return LocationWeatherGC(0, JavaClassDef());
}

void LocationWeatherGC::getWeather(IWXData* data, size_t* size, size_t offset) {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jclass Iterator = priv.GetClass("java/util/Iterator");
	jclass List = priv.GetClass("java/util/List");
	jmethodID getHourData = priv.GetMethod(_type, std::string("getHourData"), std::string("()Ljava/util/List;"));
	jobject list = priv.CallObjectMethodO((jobject)_internal, getHourData, nullptr);
	jmethodID getiterator = priv.GetMethod(List, "java/util/List", std::string("iterator"), std::string("()Ljava/util/Iterator;"));
	jobject iter = priv.CallObjectMethodO(list, getiterator, nullptr);
	jclass Hour = priv.GetClass("ca/weather/acheron/Hour");
	jmethodID iteratornext = priv.GetMethod(Iterator, "java/util/Iterator", std::string("next"), std::string("()Ljava/lang/Object;"));
	jobject hour = priv.CallObjectMethodO(iter, iteratornext, nullptr);
	jmethodID iteratorhasnext = priv.GetMethod(Iterator, "java/util/Iterator", std::string("hasNext"), std::string("()Z"));
	jmethodID getTemperature = priv.GetMethod(Hour, "ca/weather/acheron/Hour", std::string("getTemperature"), std::string("()D"));
	jmethodID getRelativeHumidity = priv.GetMethod(Hour, "ca/weather/acheron/Hour", std::string("getRelativeHumidity"), std::string("()D"));
	jmethodID getPrecipitation = priv.GetMethod(Hour, "ca/weather/acheron/Hour", std::string("getPrecipitation"), std::string("()D"));
	jmethodID getWindSpeed = priv.GetMethod(Hour, "ca/weather/acheron/Hour", std::string("getWindSpeed"), std::string("()D"));
	jmethodID getWindDirection = priv.GetMethod(Hour, "ca/weather/acheron/Hour", std::string("getWindDirection"), std::string("()D"));
	jmethodID getInterpolated = priv.GetMethod(Hour, "ca/weather/acheron/Hour", std::string("isInterpolated"), std::string("()Z"));
	int j = 0;
	while (true) {
		if (j >= offset || !priv.CallBooleanMethod(iter, iteratorhasnext))
			break;
		hour = priv.CallObjectMethodO(iter, iteratornext, nullptr);
		j++;
	}
	size_t count = 0;
	if (priv.CallBooleanMethod(iter, iteratorhasnext)) {
		for (int i = 0; i < *size; i++) {
			data[i].Temperature = priv.CallDoubleMethod(hour, getTemperature);
			data[i].RH = priv.CallDoubleMethod(hour, getRelativeHumidity) / 100.0;
			data[i].Precipitation = priv.CallDoubleMethod(hour, getPrecipitation);
			data[i].WindSpeed = priv.CallDoubleMethod(hour, getWindSpeed);
			data[i].WindDirection = priv.CallDoubleMethod(hour, getWindDirection);
			jboolean b = priv.CallBooleanMethod(hour, getInterpolated);
			if (b) {
				data[i].SpecifiedBits = 0x00000040;
			}
			else {
				data[i].SpecifiedBits = 0;
			}
			count++;
			if (!priv.CallBooleanMethod(iter, iteratorhasnext))
				break;
			hour = priv.CallObjectMethodO(iter, iteratornext, nullptr);
		}
	}
	if (count != *size)
		*size = count;
}

Calendar LocationWeatherGC::startDate() {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jclass CalendarCls = priv.GetClass("java/util/Calendar");
	jmethodID getHourData50 = priv.GetMethod(_type, std::string("getHourData"), std::string("()Ljava/util/List;"));
	jclass List = priv.GetClass("java/util/List");
	jmethodID ListGet = priv.GetMethod(List, "java/util/List", std::string("get"), std::string("(I)Ljava/lang/Object;"));
	jclass Hour = priv.GetClass("ca/weather/acheron/Hour");
	jmethodID HourGetCalendarDate = priv.GetMethod(Hour, "ca/weather/acheron/Hour", std::string("getCalendarDate"), std::string("()Ljava/util/Calendar;"));
	jobject lst = priv.CallObjectMethodO((jobject)_internal, getHourData50, nullptr);
	jobject hr = priv.CallObjectMethod(lst, ListGet, 0);
	jobject cl = priv.CallObjectMethodO(hr, HourGetCalendarDate, nullptr);
	JavaClassDef def = { CalendarCls, "java/util/Calendar" };
	return Calendar(cl, def);
}

size_t LocationWeatherGC::size() {
	REDappWrapperPrivate& priv = REDappWrapperPrivate::get_mutable_instance();
	jmethodID mid = priv.GetMethod(_type, std::string("getHourData"), std::string("()Ljava/util/List;"));
	jclass list = priv.GetClass(std::string("java/util/List"));
	jmethodID msize = priv.GetMethod(list, "java/util/List", std::string("size"), std::string("()I"));
	jobject data = priv.CallObjectMethodO((jobject)_internal, mid, nullptr);
	jint sz = priv.CallIntegerMethod(data, msize);
	return sz;
}

STANDARD_STRING_FIELD(LocationSmall, locationName)

STANDARD_STRING_GETTER(GCWeather, Observed)
STANDARD_DOUBLE_GETTER(GCWeather, Temperature)
STANDARD_DOUBLE_GETTER(GCWeather, Pressure)
STANDARD_DOUBLE_GETTER(GCWeather, Visibility)
STANDARD_DOUBLE_GETTER(GCWeather, Humidity)
STANDARD_DOUBLE_GETTER(GCWeather, Windchill)
STANDARD_DOUBLE_GETTER(GCWeather, Dewpoint)
STANDARD_STRING_GETTER(GCWeather, WindDirection)
STANDARD_DOUBLE_GETTER(GCWeather, WindSpeed)
}

/// Initialize Java.
/// Adds the Java bin path to the DLL search path then creates a new JVM instance. This triggers the lazy loading of jvm.dll.
void REDappWrapperPrivate::_init() {
	if (m_thread)
		delete m_thread;

	m_thread = new WorkerThread();
	m_classCache.clear();
	m_methodCache.clear();
	m_fieldCache.clear();

	if (!m_jvm)
		m_jvm = NativeJVM::construct();
	WorkerThread::job_t job = [this] {
		m_jvm->Initialize(m_overridePath);
	};
	run(job);
}

REDappWrapperPrivate::~REDappWrapperPrivate() {
	m_jvm = nullptr;
	if (m_thread)
	{
		delete m_thread;
		m_thread = nullptr;
	}
}

jobject REDappWrapperPrivate::NativeModelToJava(REDapp::Model mod) {
	init();
	if (m_jvm->IsValid())
	{
		jobject retval = nullptr;
		WorkerThread::job_t job = [&retval,mod,this]{
			jclass model = m_jvm->FindClass("ca/weather/forecast/Model");
			jfieldID fid;
			switch (mod)
			{
			case REDapp::Model::NCEP:
				fid = m_jvm->GetStaticFieldID(model, "NCEP", "Lca/weather/forecast/Model;");
				break;
			case REDapp::Model::GEM:
				fid = m_jvm->GetStaticFieldID(model, "GEM", "Lca/weather/forecast/Model;");
				break;
			case REDapp::Model::GEM_DETER:
				fid = m_jvm->GetStaticFieldID(model, "GEM_DETER", "Lca/weather/forecast/Model;");
				break;
			case REDapp::Model::BOTH:
				fid = m_jvm->GetStaticFieldID(model, "BOTH", "Lca/weather/forecast/Model;");
				break;
			default:
				fid = m_jvm->GetStaticFieldID(model, "CUSTOM", "Lca/weather/forecast/Model;");
				break;
			}
			retval = m_jvm->GetStaticObjectField(model, fid);
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jobject REDappWrapperPrivate::NativeTimeToJava(REDapp::Time tim) {
	init();
	if (m_jvm->IsValid())
	{
		jobject retval = nullptr;
		WorkerThread::job_t job = [&retval,tim,this]{
			jclass model = m_jvm->FindClass("ca/weather/forecast/Time");
			jfieldID fid;
			switch (tim)
			{
			case REDapp::Time::MIDNIGHT:
				fid = m_jvm->GetStaticFieldID(model, "MIDNIGHT", "Lca/weather/forecast/Time;");
				break;
			default:
				fid = m_jvm->GetStaticFieldID(model, "NOON", "Lca/weather/forecast/Time;");
				break;
			}
			retval = m_jvm->GetStaticObjectField(model, fid);
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jobject REDappWrapperPrivate::NativeProvinceToJava(REDapp::Province prov) {
	init();
	if (m_jvm->IsValid())
	{
		jobject retval = nullptr;
		WorkerThread::job_t job = [&retval,prov,this]{
			jclass province = m_jvm->FindClass("ca/weather/forecast/Province");
			jfieldID fid;
			switch (prov)
			{
			case REDapp::Province::ALBERTA:
				fid = m_jvm->GetStaticFieldID(province, "ALBERTA", "Lca/weather/forecast/Province;");
				break;
			case REDapp::Province::BRITISH_COLUMBIA:
				fid = m_jvm->GetStaticFieldID(province, "BRITISH_COLUMBIA", "Lca/weather/forecast/Province;");
				break;
			case REDapp::Province::NEW_BRUNSWICK:
				fid = m_jvm->GetStaticFieldID(province, "NEW_BRUNSWICK", "Lca/weather/forecast/Province;");
				break;
			case REDapp::Province::NEWFOUNDLAND_AND_LABRADOR:
				fid = m_jvm->GetStaticFieldID(province, "NEWFOUNDLAND_AND_LABRADOR", "Lca/weather/forecast/Province;");
				break;
			case REDapp::Province::NORTHWEST_TERRITORIES:
				fid = m_jvm->GetStaticFieldID(province, "NORTHWEST_TERRITORIES", "Lca/weather/forecast/Province;");
				break;
			case REDapp::Province::NOVA_SCOTIA:
				fid = m_jvm->GetStaticFieldID(province, "NOVA_SCOTIA", "Lca/weather/forecast/Province;");
				break;
			case REDapp::Province::NUNAVUT:
				fid = m_jvm->GetStaticFieldID(province, "NUNAVUT", "Lca/weather/forecast/Province;");
				break;
			case REDapp::Province::ONTARIO:
				fid = m_jvm->GetStaticFieldID(province, "ONTARIO", "Lca/weather/forecast/Province;");
				break;
			case REDapp::Province::PRINCE_EDWARD_ISLAND:
				fid = m_jvm->GetStaticFieldID(province, "PRINCE_EDWARD_ISLAND", "Lca/weather/forecast/Province;");
				break;
			case REDapp::Province::QUEBEC:
				fid = m_jvm->GetStaticFieldID(province, "QUEBEC", "Lca/weather/forecast/Province;");
				break;
			case REDapp::Province::SASKATCHEWAN:
				fid = m_jvm->GetStaticFieldID(province, "SASKATCHEWAN", "Lca/weather/forecast/Province;");
				break;
			case REDapp::Province::YUKON:
				fid = m_jvm->GetStaticFieldID(province, "YUKON", "Lca/weather/forecast/Province;");
				break;
			default:
				fid = m_jvm->GetStaticFieldID(province, "MANITOBA", "Lca/weather/forecast/Province;");
				break;
			}
			retval = m_jvm->GetStaticObjectField(province, fid);
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jclass REDappWrapperPrivate::GetClass(const std::string& name) {
	init();
	if (m_jvm->IsValid())
	{
		jclass retval = nullptr;
		WorkerThread::job_t job = [&retval,name,this]{
			retval = m_classCache.create(name, m_jvm.get());
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jmethodID REDappWrapperPrivate::GetStaticMethod(REDapp::JavaClassDef& cls, const std::string& name, const std::string& sig) {
	init();
	if (m_jvm->IsValid())
	{
		jmethodID retval = nullptr;
		WorkerThread::job_t job = [&retval,cls,name,sig,this]{
			retval = m_methodCache.createStatic((jclass)cls.data, cls.name, name, sig, m_jvm.get());
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jmethodID REDappWrapperPrivate::GetStaticMethod(jclass cls, const std::string& clsname, const std::string& name, const std::string& sig) {
	init();
	if (m_jvm->IsValid())
	{
		jmethodID retval = nullptr;
		WorkerThread::job_t job = [&retval,cls,clsname,name,sig,this]{
			retval = m_methodCache.createStatic(cls, clsname, name, sig, m_jvm.get());
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jobject REDappWrapperPrivate::CallStaticObjectMethod(jclass cls, jmethodID mid, jobject param) {
	init();
	if (m_jvm->IsValid())
	{
		jobject retval = nullptr;
		WorkerThread::job_t job = [&retval,cls,mid,param,this]{
			retval = m_jvm->CallStaticObjectMethod(cls, mid, param);
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jobject REDappWrapperPrivate::CallStaticObjectMethod(jclass cls, jmethodID mid, jint param) {
	init();
	if (m_jvm->IsValid())
	{
		jobject retval = nullptr;
		WorkerThread::job_t job = [&retval,cls,mid,param,this]{
			retval = m_jvm->CallStaticObjectMethod(cls, mid, param);
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jboolean REDappWrapperPrivate::CallStaticBooleanMethod(jclass cls, jmethodID mid, jobject param) {
	init();
	if (m_jvm->IsValid())
	{
		jboolean retval = false;
		WorkerThread::job_t job = [&retval,cls,mid,param,this]{
			retval = m_jvm->CallStaticBooleanMethod(cls, mid, param);
		};
		run(job);
		return retval;
	}
	return (unsigned char)-1;
}

jobject REDappWrapperPrivate::CallObjectMethodO(jobject obj, jmethodID mid, jobject o) {
	init();
	if (m_jvm->IsValid())
	{
		jobject retval = nullptr;
		WorkerThread::job_t job = [&retval,obj,mid,o,this]{
			retval = m_jvm->CallObjectMethodO(obj, mid, o);
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jobject REDappWrapperPrivate::CallObjectMethodOO(jobject obj, jmethodID mid, jobject o1, jobject o2) {
	init();
	if (m_jvm->IsValid())
	{
		jobject retval = nullptr;
		WorkerThread::job_t job = [&retval,obj,mid,o1,o2,this]{
			retval = m_jvm->CallObjectMethodOO(obj, mid, o1, o2);
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jobject REDappWrapperPrivate::CallObjectMethodOOI(jobject obj, jmethodID mid, jobject o1, jobject o2, jint i1) {
	init();
	if (m_jvm->IsValid())
	{
		jobject retval = nullptr;
		WorkerThread::job_t job = [&retval, obj, mid, o1, o2, i1, this] {
			retval = m_jvm->CallObjectMethodOOI(obj, mid, o1, o2, i1);
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jobject REDappWrapperPrivate::CallObjectMethod(jobject obj, jmethodID mid, jint ind) {
	init();
	if (m_jvm->IsValid())
	{
		jobject retval = nullptr;
		WorkerThread::job_t job = [&retval,obj,mid,ind,this]{
			retval = m_jvm->CallObjectMethod(obj, mid, ind);
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jdouble REDappWrapperPrivate::CallDoubleMethod(jobject obj, jmethodID mid, ...) {
	init();
	if (m_jvm->IsValid())
	{
		jdouble retval;
		va_list vl;
		va_start(vl, mid);
		WorkerThread::job_t job = [&retval,obj,mid,vl,this]{
			retval = m_jvm->CallDoubleMethod(obj, mid, vl);
		};
		run(job);
		va_end(vl);
		return retval;
	}
	return 0.0;
}

jdouble REDappWrapperPrivate::CallObjectDoubleMethod(jobject obj, jmethodID mid, ...) {
	init();
	if (m_jvm->IsValid())
	{
		jdouble dval;
		va_list vl;
		va_start(vl, mid);
		WorkerThread::job_t job = [&dval,obj,mid,vl,this]{
			jobject retval = m_jvm->CallObjectDoubleMethod(obj, mid, vl);
			if (retval == nullptr)
				dval = std::numeric_limits<double>::infinity();
			jclass cls = m_jvm->FindClass("java/lang/Double");
			jmethodID mid = m_jvm->GetMethodID(cls, "doubleValue", "()D");
			dval = m_jvm->CallDoubleMethod(retval, mid);
		};
		run(job);
		va_end(vl);
		return dval;
	}
	return 0.0;
}

jboolean REDappWrapperPrivate::CallBooleanMethod(jobject obj, jmethodID mid) {
	init();
	if (m_jvm->IsValid())
	{
		jboolean retval;
		WorkerThread::job_t job = [&retval,obj,mid,this]{
			retval = m_jvm->CallBooleanMethod(obj, mid);
		};
		run(job);
		return retval;
	}
	return false;
}

jboolean REDappWrapperPrivate::CallBooleanMethod(jobject obj, jmethodID mid, int param) {
	init();
	if (m_jvm->IsValid())
	{
		jboolean retval;
		WorkerThread::job_t job = [&retval,obj,mid,param,this]{
			retval = m_jvm->CallBooleanMethod(obj, mid, param);
		};
		run(job);
		return retval;
	}
	return false;
}

jint REDappWrapperPrivate::CallIntegerMethod(jobject obj, jmethodID mid) {
	init();
	if (m_jvm->IsValid())
	{
		jint retval;
		WorkerThread::job_t job = [&retval,obj,mid,this]{
			retval = m_jvm->CallIntMethod(obj, mid);
		};
		run(job);
		return retval;
	}
	return 0;
}

jint REDappWrapperPrivate::CallIntegerMethod(jobject obj, jmethodID mid, jint param) {
	init();
	if (m_jvm->IsValid())
	{
		jint retval;
		WorkerThread::job_t job = [&retval,obj,mid,param,this]{
			retval = m_jvm->CallIntMethod(obj, mid, param);
		};
		run(job);
		return retval;
	}
	return 0;
}

jlong REDappWrapperPrivate::CallLongMethod(jobject obj, jmethodID mid) {
	init();
	if (m_jvm->IsValid())
	{
		jlong retval;
		WorkerThread::job_t job = [&retval,obj,mid,this]{
			retval = m_jvm->CallLongMethod(obj, mid);
		};
		run(job);
		return retval;
	}
	return 0;
}

void REDappWrapperPrivate::CallMethod(jobject obj, jmethodID mid, jobject param) {
	init();
	if (m_jvm->IsValid())
	{
		WorkerThread::job_t job = [obj,mid,param,this]{
			m_jvm->CallMethod(obj, mid, param);
		};
		run(job);
	}
}

void REDappWrapperPrivate::CallMethod(jobject obj, jmethodID mid, jint param) {
	init();
	if (m_jvm->IsValid())
	{
		WorkerThread::job_t job = [obj,mid,param,this]{
			m_jvm->CallMethod(obj, mid, param);
		};
		run(job);
	}
}

void REDappWrapperPrivate::CallMethod(jobject obj, jmethodID mid, jint param1, jint param2) {
	init();
	if (m_jvm->IsValid())
	{
		WorkerThread::job_t job = [obj,mid,param1,param2,this]{
			m_jvm->CallMethod(obj, mid, param1, param2);
		};
		run(job);
	}
}

void REDappWrapperPrivate::CallMethod(jobject obj, jmethodID mid, jdouble param) {
	init();
	if (m_jvm->IsValid())
	{
		WorkerThread::job_t job = [obj,mid,param,this]{
			m_jvm->CallMethod(obj, mid, param);
		};
		run(job);
	}
}

void REDappWrapperPrivate::CallMethod(jobject obj, jmethodID mid, jlong param) {
	init();
	if (m_jvm->IsValid())
	{
		WorkerThread::job_t job = [obj,mid,param,this]{
			m_jvm->CallMethod(obj, mid, param);
		};
		run(job);
	}
}

jfieldID REDappWrapperPrivate::GetStaticField(REDapp::JavaClassDef& cls, const std::string& name, const std::string& sig) {
	init();
	if (m_jvm->IsValid())
	{
		jfieldID retval = nullptr;
		WorkerThread::job_t job = [&retval,cls,name,sig,this]{
			retval = m_fieldCache.createStatic((jclass)cls.data, cls.name, name, sig, m_jvm.get());
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jfieldID REDappWrapperPrivate::GetStaticField(jclass cls, const std::string& clsname, const std::string& name, const std::string& sig) {
	init();
	if (m_jvm->IsValid())
	{
		jfieldID retval = nullptr;
		WorkerThread::job_t job = [&retval,cls,clsname,name,sig,this]{
			retval = m_fieldCache.createStatic(cls, clsname, name, sig, m_jvm.get());
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jobject REDappWrapperPrivate::GetStaticObjectField(jclass cls, jfieldID fid) {
	init();
	if (m_jvm->IsValid())
	{
		jobject retval = nullptr;
		WorkerThread::job_t job = [&retval,cls,fid,this]{
			retval = m_jvm->GetStaticObjectField(cls, fid);
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jmethodID REDappWrapperPrivate::GetMethod(REDapp::JavaClassDef& cls, const std::string& name, const std::string& sig) {
	init();
	if (m_jvm->IsValid())
	{
		jmethodID retval = nullptr;
		WorkerThread::job_t job = [&retval,cls,name,sig,this]{
			retval = m_methodCache.create((jclass)cls.data, cls.name, name, sig, m_jvm.get());
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jmethodID REDappWrapperPrivate::GetMethod(jclass cls, const std::string& clsname, const std::string& name, const std::string& sig) {
	init();
	if (m_jvm->IsValid())
	{
		jmethodID retval = nullptr;
		WorkerThread::job_t job = [&retval,cls,clsname,name,sig,this]{
			retval = m_methodCache.create(cls, clsname, name, sig, m_jvm.get());
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jobject REDappWrapperPrivate::GetArrayElement(jobject obj, int* size, int index) {
	init();
	if (m_jvm->IsValid())
	{
		jobject retval = nullptr;
		WorkerThread::job_t job = [&retval,obj,size,index,this]{
			if (size != nullptr)
				*size = m_jvm->GetArrayLength((jarray)obj);
			if (index >= 0)
				retval = m_jvm->GetObjectArrayElement((jobjectArray)obj, index);
		};
		run(job);
		return retval;
	}
	return nullptr;
}

std::string REDappWrapperPrivate::GetJStringContent(jstring str) {
	init();
	if (m_jvm->IsValid())
	{
		std::string retval;
		WorkerThread::job_t job = [&retval,str,this]{
			if (!str)
				retval.clear();
			else
			{
				const char *s = m_jvm->GetStringUTFChars(str);
				retval = s;
				m_jvm->ReleaseStringUTFChars(str, s);
			}
		};
		run(job);
		return retval;
	}
	return std::string("");
}

jstring REDappWrapperPrivate::GetJString(std::string str) {
	jstring retval;
	WorkerThread::job_t job = [&retval,str,this]{
		retval = m_jvm->NewStringUTF(str.c_str());
	};
	run(job);
	return retval;
}

void REDappWrapperPrivate::FreeJString(jstring str) {
	WorkerThread::job_t job = [str,this]{
		m_jvm->DeleteLocalRef(str);
	};
	run(job);
}

jobject REDappWrapperPrivate::NewObject(jclass cls, jmethodID constructor, jobject param) {
	init();
	if (m_jvm->IsValid())
	{
		jobject retval = nullptr;
		WorkerThread::job_t job = [&retval,cls,constructor,param,this]{
			retval = m_jvm->NewObject(cls, constructor, param);
		};
		run(job);
		return retval;
	}
	return nullptr;
}

void REDappWrapperPrivate::DeleteObject(jobject obj) {
	init();
	if (m_jvm->IsValid())
	{
		WorkerThread::job_t job = [obj, this] {
			m_jvm->DeleteLocalRef(obj);
		};
		run(job);
	}
}

jobject REDappWrapperPrivate::NewObject(jclass cls, jmethodID constructor, jlong lval) {
	init();
	if (m_jvm->IsValid())
	{
		jobject retval = nullptr;
		WorkerThread::job_t job = [&retval,cls,constructor,lval,this]{
			retval = m_jvm->NewObject(cls, constructor, lval);
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jintArray REDappWrapperPrivate::NewIntArray(int size) {
	init();
	if (m_jvm->IsValid())
	{
		jintArray retval = nullptr;
		WorkerThread::job_t job = [&retval,size,this]{
			retval = m_jvm->NewIntArray(size);
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jdoubleArray REDappWrapperPrivate::NewDoubleArray(int size) {
	init();
	if (m_jvm->IsValid())
	{
		jdoubleArray retval = nullptr;
		WorkerThread::job_t job = [&retval,size,this]{
			retval = m_jvm->NewDoubleArray(size);
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jobjectArray REDappWrapperPrivate::NewObjectArray(int size, jclass cls) {
	init();
	if (m_jvm->IsValid())
	{
		jobjectArray retval = nullptr;
		WorkerThread::job_t job = [&retval,size,cls,this]{
			retval = m_jvm->NewObjectArray(size, cls);
		};
		run(job);
		return retval;
	}
	return nullptr;
}

int REDappWrapperPrivate::SetIntField(jobject obj, jfieldID fld, jint val) {
	init();
	if (m_jvm->IsValid())
	{
		WorkerThread::job_t job = [obj,fld,val,this]{
			m_jvm->SetIntField(obj, fld, val);
		};
		run(job);
		return JNI_OK;
	}
	return JNI_ERR;
}

int REDappWrapperPrivate::SetDoubleField(jobject obj, jfieldID fld, jdouble val) {
	init();
	if (m_jvm->IsValid())
	{
		WorkerThread::job_t job = [obj,fld,val,this]{
			m_jvm->SetDoubleField(obj, fld, val);
		};
		run(job);
		return JNI_OK;
	}
	return JNI_ERR;
}

int REDappWrapperPrivate::SetLongField(jobject obj, jfieldID fld, jlong val) {
	init();
	if (m_jvm->IsValid()) {
		WorkerThread::job_t job = [obj,fld,val,this]{
			m_jvm->SetLongField(obj, fld, val);
		};
		run(job);
		return JNI_OK;
	}
	return JNI_ERR;
}

int REDappWrapperPrivate::SetObjectField(jobject obj, jfieldID fld, jobject val) {
	init();
	if (m_jvm->IsValid()) {
		WorkerThread::job_t job = [obj,fld,val,this]{
			m_jvm->SetObjectField(obj, fld, val);
		};
		run(job);
		return JNI_OK;
	}
	return JNI_ERR;
}

int REDappWrapperPrivate::SetObjectArrayElement(jobjectArray arr, int index, jobject val) {
	init();
	if (m_jvm->IsValid()) {
		WorkerThread::job_t job = [arr,index,val,this]{
			m_jvm->SetObjectArrayElement(arr, index, val);
		};
		run(job);
		return JNI_OK;
	}
	return JNI_ERR;
}

jfieldID REDappWrapperPrivate::GetField(REDapp::JavaClassDef& cls, const std::string& name, const std::string& sig) {
	init();
	if (m_jvm->IsValid()) {
		jfieldID retval = nullptr;
		WorkerThread::job_t job = [&retval,cls,name,sig,this]{
			retval = m_fieldCache.create((jclass)cls.data, cls.name, name, sig, m_jvm.get());
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jfieldID REDappWrapperPrivate::GetField(jclass cls, const std::string& clsname, const std::string& name, const std::string& sig) {
	init();
	if (m_jvm->IsValid()) {
		jfieldID retval = nullptr;
		WorkerThread::job_t job = [&retval,cls,clsname,name,sig,this]{
			retval = m_fieldCache.create(cls, clsname, name, sig, m_jvm.get());
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jobject REDappWrapperPrivate::CallObjectField(jobject obj, jfieldID fid) {
	init();
	if (m_jvm->IsValid()) {
		jobject retval = nullptr;
		WorkerThread::job_t job = [&retval,obj,fid,this]{
			retval = m_jvm->GetObjectField(obj, fid);
		};
		run(job);
		return retval;
	}
	return nullptr;
}

jint REDappWrapperPrivate::CallIntField(jobject obj, jfieldID fid) {
	init();
	if (m_jvm->IsValid()) {
		jint retval = 0;
		WorkerThread::job_t job = [&retval,obj,fid,this]{
			retval = m_jvm->GetIntField(obj, fid);
		};
		run(job);
		return retval;
	}
	return JNI_ERR;
}

jdouble REDappWrapperPrivate::CallDoubleField(jobject obj, jfieldID fid) {
	init();
	if (m_jvm->IsValid()) {
		jdouble retval;
		WorkerThread::job_t job = [&retval,obj,fid,this]{
			retval = m_jvm->GetDoubleField(obj, fid);
		};
		run(job);
		return retval;
	}
	return JNI_ERR;
}

jlong REDappWrapperPrivate::CallLongField(jobject obj, jfieldID fid) {
	init();
	if (m_jvm->IsValid()) {
		jlong retval;
		WorkerThread::job_t job = [&retval,obj,fid,this]{
			retval = m_jvm->GetLongField(obj, fid);
		};
		run(job);
		return retval;
	}
	return JNI_ERR;
}

jboolean REDappWrapperPrivate::ExceptionCheck() {
	init();
	if (m_jvm->IsValid()) {
		jboolean exc;
		WorkerThread::job_t job = [&exc,this]{
			exc = m_jvm->ExceptionCheck();
		};
		run(job);
		return exc;
	}
	return false;
}

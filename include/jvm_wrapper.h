/**
 * WISE_REDapp_Lib_Wrapper: jvm_wrapper.h
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

#pragma once

#include <boost/utility.hpp>
#include <jni.h>
#include <string>
#include <memory>


class NativeJVM : public boost::noncopyable {
public:
	static constexpr int ERROR_OK = 0;
	static constexpr int ERROR_NO_JAVA = 1;
	static constexpr int ERROR_NO_JNI = 2;
	static constexpr int ERROR_JVM_ERROR = 3;
	static constexpr int ERROR_MISSING_JAR = 4;
	static constexpr int ERROR_ARCHITECTURE = 5;

protected:
	int internalError = ERROR_OK;
	int jvmError = ERROR_OK;
	std::string javaPath;
	std::string javaVersion;
	std::string detailedError;

	NativeJVM() { }

public:
	static std::unique_ptr<NativeJVM> construct();

	virtual ~NativeJVM() { }

	virtual bool Initialize(const std::string& overridePath) = 0;
	virtual bool IsInitialized() = 0;
	virtual bool IsValid() = 0;
	virtual unsigned long GetLoadError() = 0;
	inline int GetError() { return internalError; }
	std::string GetErrorDescription();
	inline std::string GetJavaPath() { return javaPath; }
	inline std::string GetJavaVersion() { return javaVersion; }
	inline std::string GetDetailedError() { return detailedError; }

	virtual jclass FindClass(const std::string& signature) = 0;
	virtual jfieldID GetStaticFieldID(jclass clz, const std::string& name, const std::string& signature) = 0;
	virtual jobject GetStaticObjectField(jclass clz, jfieldID fld) = 0;
	virtual jmethodID GetMethodID(jclass clz, const std::string& name, const std::string& signature) = 0;
	virtual jmethodID GetStaticMethodID(jclass clz, const std::string& name, const std::string& signature) = 0;
	virtual jfieldID GetFieldID(jclass clz, const std::string& name, const std::string& signature) = 0;

	virtual jobject CallStaticObjectMethod(jclass cls, jmethodID mid, jobject param) = 0;
	virtual jobject CallStaticObjectMethod(jclass cls, jmethodID mid, jint param) = 0;
	virtual jboolean CallStaticBooleanMethod(jclass cls, jmethodID mid, jobject param) = 0;
	virtual jobject CallObjectMethodO(jobject obj, jmethodID mid, jobject o) = 0;
	virtual jobject CallObjectMethodOO(jobject obj, jmethodID mid, jobject o1, jobject o2) = 0;
	virtual jobject CallObjectMethodOOI(jobject obj, jmethodID mid, jobject o1, jobject o2, jint i1) = 0;
	virtual jobject CallObjectMethod(jobject obj, jmethodID mid, jint ind) = 0;
	virtual jdouble CallDoubleMethod(jobject obj, jmethodID mid, ...) = 0;
	virtual jobject CallObjectDoubleMethod(jobject obj, jmethodID mid, ...) = 0;
	virtual jboolean CallBooleanMethod(jobject obj, jmethodID mid) = 0;
	virtual jboolean CallBooleanMethod(jobject obj, jmethodID mid, int param) = 0;
	virtual jint CallIntMethod(jobject obj, jmethodID mid) = 0;
	virtual jint CallIntMethod(jobject obj, jmethodID mid, jint param) = 0;
	virtual jlong CallLongMethod(jobject obj, jmethodID mid) = 0;
	virtual void CallMethod(jobject obj, jmethodID mid, jobject param) = 0;
	virtual void CallMethod(jobject obj, jmethodID mid, jint param) = 0;
	virtual void CallMethod(jobject obj, jmethodID mid, jint param1, jint param2) = 0;
	virtual void CallMethod(jobject obj, jmethodID mid, jdouble param) = 0;
	virtual void CallMethod(jobject obj, jmethodID mid, jlong param) = 0;
	virtual int GetArrayLength(jarray arr) = 0;
	virtual jobject GetObjectArrayElement(jobjectArray arr, int index) = 0;
	virtual const char* GetStringUTFChars(jstring str) = 0;
	virtual void ReleaseStringUTFChars(jstring str, const char* s) = 0;
	virtual jstring NewStringUTF(const char* str) = 0;
	virtual void DeleteLocalRef(jstring str) = 0;
	virtual void DeleteLocalRef(jobject str) = 0;
	virtual jobject NewObject(jclass cls, jmethodID constructor, jobject param) = 0;
	virtual jobject NewObject(jclass cls, jmethodID constructor, jlong param) = 0;
	virtual jintArray NewIntArray(int size) = 0;
	virtual jdoubleArray NewDoubleArray(int size) = 0;
	virtual jobjectArray NewObjectArray(int size, jclass cls) = 0;
	virtual void SetIntField(jobject obj, jfieldID fld, jint val) = 0;
	virtual void SetDoubleField(jobject obj, jfieldID fld, jdouble val) = 0;
	virtual void SetLongField(jobject obj, jfieldID fld, jlong val) = 0;
	virtual void SetObjectField(jobject obj, jfieldID fld, jobject val) = 0;
	virtual void SetObjectArrayElement(jobjectArray arr, int index, jobject val) = 0;
	virtual jobject GetObjectField(jobject obj, jfieldID fid) = 0;
	virtual jint GetIntField(jobject obj, jfieldID fid) = 0;
	virtual jdouble GetDoubleField(jobject obj, jfieldID fid) = 0;
	virtual jlong GetLongField(jobject obj, jfieldID fid) = 0;
	virtual jboolean ExceptionCheck() = 0;
};

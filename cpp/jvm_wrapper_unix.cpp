/**
 * WISE_REDapp_Lib_Wrapper: jvm_wrapper_unix.h
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

#include "types.h"
#include "jvm_wrapper.h"
#include "hss_inlines.h"

#include <unistd.h>
#include <string>
#include "filesystem.hpp"
#include <dlfcn.h>
#include <algorithm>
#include <iostream>
#include <array>
#include <stdio.h>
#include <cctype>

#define BOOST_FILESYSTEM_NO_DEPRECATED
#define BOOST_FILESYSTEM_NO_LIB


static std::vector<std::string> dependencies =
{ "checker-qual.jar", "commons-codec.jar",
  "commons-collections4.jar", "commons-compress.jar",
  "commons-io.jar", "commons-math3.jar",
  "curvesapi.jar", "error_prone_annotations.jar",
  "failureaccess.jar", "fuel.jar",
  "fwi.jar", "grid.jar", "gson.jar", "guava.jar",
  "hss-java.jar", "jakarta.activation.jar",
  "jakarta.xml.bind-api.jar", "javax.activation-api.jar",
  "jaxb-api.jar", "jaxb-core.jar", "jaxb-impl.jar",
  "jsr305.jar", "listenablefuture.jar",
  "math.jar", "poi.jar", "poi-ooxml.jar",
  "poi-ooxml-lite.jar", "protobuf-java.jar",
  "protobuf-java-util.jar", "REDapp_Lib.jar",
  "SparseBitSet.jar", "weather.jar", "wtime.jar",
  "xmlbeans.jar" };


class NativeJVM_Unix : public NativeJVM {
public:
    void* m_handle;
	JavaVM *m_jvm;
	JNIEnv* m_env;
	bool m_valid;
	bool m_init;
	unsigned long error_code;

	NativeJVM_Unix() : m_jvm(nullptr), m_env(nullptr), m_valid(false), m_init(false), m_handle(nullptr) { }
	virtual ~NativeJVM_Unix();

	virtual bool Initialize(const std::string& overridePath) override;
	virtual void InitializeVersion();
	virtual bool IsInitialized() override { return m_init; }
	virtual bool IsValid() override { return m_valid; }
	virtual unsigned long GetLoadError() override { return error_code; }

	jclass FindClass(const std::string& signature) override;
	jfieldID GetStaticFieldID(jclass clz, const std::string& name, const std::string& signature) override;
	jobject GetStaticObjectField(jclass clz, jfieldID fld) override;
	jmethodID GetMethodID(jclass clz, const std::string& name, const std::string& signature) override;
	jmethodID GetStaticMethodID(jclass clz, const std::string& name, const std::string& signature) override;
	jfieldID GetFieldID(jclass clz, const std::string& name, const std::string& signature) override;

	jobject CallStaticObjectMethod(jclass cls, jmethodID mid, jobject param) override;
	jobject CallStaticObjectMethod(jclass cls, jmethodID mid, jint param) override;
	jboolean CallStaticBooleanMethod(jclass cls, jmethodID mid, jobject param) override;
	jobject CallObjectMethodO(jobject obj, jmethodID mid, jobject o) override;
	jobject CallObjectMethodOO(jobject obj, jmethodID mid, jobject o1, jobject o2) override;
	jobject CallObjectMethodOOI(jobject obj, jmethodID mid, jobject o1, jobject o2, jint i1) override;
	jobject CallObjectMethod(jobject obj, jmethodID mid, jint ind) override;
	jdouble CallDoubleMethod(jobject obj, jmethodID mid, ...) override;
	jobject CallObjectDoubleMethod(jobject obj, jmethodID mid, ...) override;
	jboolean CallBooleanMethod(jobject obj, jmethodID mid) override;
	jboolean CallBooleanMethod(jobject obj, jmethodID mid, int param) override;
	jint CallIntMethod(jobject obj, jmethodID mid) override;
	jint CallIntMethod(jobject obj, jmethodID mid, jint param) override;
	jlong CallLongMethod(jobject obj, jmethodID mid) override;
	void CallMethod(jobject obj, jmethodID mid, jobject param) override;
	void CallMethod(jobject obj, jmethodID mid, jint param) override;
	void CallMethod(jobject obj, jmethodID mid, jint param1, jint param2) override;
	void CallMethod(jobject obj, jmethodID mid, jdouble param) override;
	void CallMethod(jobject obj, jmethodID mid, jlong param) override;
	int GetArrayLength(jarray arr) override;
	jobject GetObjectArrayElement(jobjectArray arr, int index) override;
	const char* GetStringUTFChars(jstring str) override;
	void ReleaseStringUTFChars(jstring str, const char* s) override;
	jstring NewStringUTF(const char* str) override;
	void DeleteLocalRef(jstring str) override;
	void DeleteLocalRef(jobject str) override;
	jobject NewObject(jclass cls, jmethodID constructor, jobject param) override;
	jobject NewObject(jclass cls, jmethodID constructor, jlong param) override;
	jintArray NewIntArray(int size) override;
	jdoubleArray NewDoubleArray(int size) override;
	jobjectArray NewObjectArray(int size, jclass cls) override;
	void SetIntField(jobject obj, jfieldID fld, jint val) override;
	void SetDoubleField(jobject obj, jfieldID fld, jdouble val) override;
	void SetLongField(jobject obj, jfieldID fld, jlong val) override;
	void SetObjectField(jobject obj, jfieldID fld, jobject val) override;
	void SetObjectArrayElement(jobjectArray arr, int index, jobject val) override;
	jobject GetObjectField(jobject obj, jfieldID fid) override;
	jint GetIntField(jobject obj, jfieldID fid) override;
	jdouble GetDoubleField(jobject obj, jfieldID fid) override;
	jlong GetLongField(jobject obj, jfieldID fid) override;
	jboolean ExceptionCheck() override;
};

std::unique_ptr<NativeJVM> NativeJVM::construct() {
	return std::unique_ptr<NativeJVM>{ new NativeJVM_Unix() };
}

/**
 * Run a command on the command line and return what was printed to cout.
 */
std::string exec(const char* cmd) {
    std::array<char, 128> buffer{};
    std::string result;
    std::shared_ptr<FILE> pipe(popen(cmd, "r"), pclose);
    if (!pipe)
        throw std::runtime_error("popen() failed!");
    while (!feof(pipe.get()))
    {
        if (fgets(buffer.data(), 128, pipe.get()) != nullptr)
            result += buffer.data();
    }
    return result;
}

std::optional<fs::path> searchPath(const fs::path& path) {
	const std::vector<std::string> toTry = { "lib/server/libjvm.so", "lib/amd64/server/libjvm.so" };
	for (auto& loc : toTry) {
		fs::path attempt = path / loc;
		if (fs::exists(attempt))
		{
			return std::make_optional(std::move(attempt));
		}
	}
	return std::nullopt;
}

/**
 * Try to find the path to libjvm.so. java must be in the path in order for this to work.
 * @param result If found, the path to libjvm.so will be stored here.
 * @returns 0 if successful, -1 if java cound't be found, -2 if libjvm.so couldn't be found within the java directory.
 */
std::optional<fs::path> findJavaInstall() {
	//allow JAVA_HOME to override all other values
    std::string javaHome = hss::getenv("JAVA_HOME");
    auto test = searchPath(javaHome);
	//this is a valid java install, use this
    if (test.has_value())
        return test;
    else {
        std::string which = exec("which java");
        std::array<char, 512> buffer{};
        which.erase(std::remove(which.begin(), which.end(), '\n'), which.end());
        which.erase(std::remove(which.begin(), which.end(), '\r'), which.end());
        ssize_t readSize = 0;
        int bufferHelper = 0;
        do
        {
            buffer.fill('\0');
            readSize = readlink(which.c_str(), buffer.data(), 512);
            if (readSize >= 0)
                which = buffer.data();
            bufferHelper++;
        } while (readSize >= 0 && bufferHelper < 10);

        fs::path jvm(which);
        if (fs::exists(jvm))
            return searchPath(jvm.parent_path().parent_path());
        return std::nullopt;
    }
}


std::string NativeJVM::GetErrorDescription() {
	int error = GetError();
	int loadError = GetLoadError();
	//no errors
	if (error == ERROR_OK && loadError == 0)
		return "No error";
	//the Java install folder could not be found
	else if (error == ERROR_NO_JAVA)
		return "No Java installation found.";
	//jvm.dll could not be found
	else if (error == ERROR_NO_JNI)
		return "The Java install doesn't contain a valid JNI library.";
	//a required jar file was missing from the install folder
	else if (error == ERROR_MISSING_JAR)
		return "Java installation issues.";
	else if (error == ERROR_ARCHITECTURE)
		return "Installed Java is not 64-bit.";
	//errors loading the JVM
	else {
		if (jvmError == JNI_EVERSION)
			return "Incorrect Java version.";
		else if (jvmError == JNI_ENOMEM)
			return "Not enough memory to load the JVM.";
		else if (jvmError == JNI_EINVAL || jvmError == JNI_EDETACHED)
			return "Internal JVM error.";
	}
	return "Unknown Java initialization issue";
}

bool NativeJVM_Unix::Initialize(const std::string& overridePath) {
    if (!m_init) {
        m_valid = false;
		internalError = ERROR_OK;
        auto jvmLocation = findJavaInstall();

        if (jvmLocation.has_value())
        {
            jint(*create_java_jvm)(JavaVM**, void**, void*);
            error_code = -2;
            try {
                m_handle = dlopen(jvmLocation.value().string().c_str(), RTLD_NOW | RTLD_GLOBAL);
                if (m_handle)
                {
                    create_java_jvm = (jint(*)(JavaVM**, void**, void*))dlsym(m_handle, "JNI_CreateJavaVM");
                    if (create_java_jvm)
                        error_code = 0;
                }
            }
            catch (std::exception e) {
                m_handle = nullptr;
            }

            if (error_code == 0) {
                JavaVMInitArgs vm_args;
                int argCount;
#ifdef _DEBUG
                argCount = 2;
#else
                argCount = 1;
#endif
                JavaVMOption* options = new JavaVMOption[argCount];
                std::string key = "-Djava.class.path=";
                std::array<char, 1024> buf{};
                //look for the current executable directory
                ssize_t size = readlink("/proc/self/exe", buf.data(), 1024);
                if (static_cast<std::vector<char>::size_type>(size) < 1024) {
                    std::string wdir(buf.data(), 1024);
                    fs::path p(wdir);
                    for (auto& dep : dependencies) {
                        p = p.replace_filename(dep);
                        if (!fs::exists(p))
                            internalError = ERROR_MISSING_JAR;
                        key += p.string();
                        key += ":";
                    }
                    options[0].optionString = const_cast<char*>(key.c_str());
#ifdef _DEBUG
                    options[1].optionString = const_cast<char*>("-verbose:jni");
#endif
                    vm_args.version = JNI_VERSION_1_8;
                    vm_args.nOptions = argCount;
                    vm_args.options = options;
                    vm_args.ignoreUnrecognized = false;
                    JNIEnv* env;
                    JavaVM* jvm;
                    try {
                        jvmError = (*create_java_jvm)(&jvm, (void**)&env, &vm_args);
                    }
                    catch (std::exception e) {
#ifdef _DEBUG
                        std::cerr << e.what() << std::endl;
#endif
                        jvmError = -1;
                    }
                    delete[] options;
                    if (jvmError == JNI_OK && jvm && env) {
                        m_env = env;
                        m_jvm = jvm;
                        m_valid = internalError != ERROR_MISSING_JAR;
                        javaPath = jvmLocation.value().parent_path().parent_path().parent_path().string();
		                InitializeVersion();
                    }
                    else if (jvmError != JNI_OK && jvmError != JNI_EEXIST) {
                        if (internalError == ERROR_OK) {
                            std::string call = "java -version 2>&1";
                            std::string output = exec(call.c_str());
                            if (output.size() > 0)
                            {
                                std::string search = "64-bit";
                                auto it = std::search(output.begin(), output.end(),
                                    search.begin(), search.end(),
                                    [](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); });
                                if (it == output.end())
                                    internalError = ERROR_ARCHITECTURE;
                            }
                            if (internalError = ERROR_OK)
                                internalError = ERROR_JVM_ERROR;
                        }
                    }
                }
                else {
                    internalError = ERROR_MISSING_JAR;
#ifdef _DEBUG
                    perror("readlink");
#endif
                }
            }
            else
                internalError = ERROR_NO_JNI;
        }
        else
            internalError = ERROR_NO_JAVA;

        m_init = true;
    }

    return m_valid;
}

void NativeJVM_Unix::InitializeVersion() {
	jint ver = m_env->GetVersion();
	jint major = (ver >> 16) & 0xFFFF;
	jint minor = ver & 0xFFFF;
	javaVersion = std::to_string(major) + "." + std::to_string(minor);
}

NativeJVM_Unix::~NativeJVM_Unix() {
	m_jvm = nullptr;
	m_env = nullptr;
    if (m_handle)
    {
        dlclose(m_handle);
        m_handle = nullptr;
    }
}

jclass NativeJVM_Unix::FindClass(const std::string& signature) {
	return m_env->FindClass(signature.c_str());
}

jfieldID NativeJVM_Unix::GetStaticFieldID(jclass clz, const std::string& name, const std::string& signature) {
	return m_env->GetStaticFieldID(clz, name.c_str(), signature.c_str());
}

jobject NativeJVM_Unix::GetStaticObjectField(jclass clz, jfieldID fld) {
	return m_env->GetStaticObjectField(clz, fld);
}

jmethodID NativeJVM_Unix::GetMethodID(jclass clz, const std::string& name, const std::string& signature) {
	return m_env->GetMethodID(clz, name.c_str(), signature.c_str());
}

jmethodID NativeJVM_Unix::GetStaticMethodID(jclass clz, const std::string& name, const std::string& signature) {
	return m_env->GetStaticMethodID(clz, name.c_str(), signature.c_str());
}

jfieldID NativeJVM_Unix::GetFieldID(jclass clz, const std::string& name, const std::string& signature) {
	return m_env->GetFieldID(clz, name.c_str(), signature.c_str());
}


jobject NativeJVM_Unix::CallStaticObjectMethod(jclass cls, jmethodID mid, jobject param) {
	return m_env->CallStaticObjectMethod(cls, mid, param);
}

jobject NativeJVM_Unix::CallStaticObjectMethod(jclass cls, jmethodID mid, jint param) {
	return m_env->CallStaticObjectMethod(cls, mid, param);
}

jboolean NativeJVM_Unix::CallStaticBooleanMethod(jclass cls, jmethodID mid, jobject param) {
	return m_env->CallStaticBooleanMethod(cls, mid, param);
}

jobject NativeJVM_Unix::CallObjectMethodO(jobject obj, jmethodID mid, jobject o) {
	return m_env->CallObjectMethod(obj, mid, o);
}

jobject NativeJVM_Unix::CallObjectMethodOO(jobject obj, jmethodID mid, jobject o1, jobject o2) {
	return m_env->CallObjectMethod(obj, mid, o1, o2);
}

jobject NativeJVM_Unix::CallObjectMethodOOI(jobject obj, jmethodID mid, jobject o1, jobject o2, jint i1) {
	return m_env->CallObjectMethod(obj, mid, o1, o2, i1);
}

jobject NativeJVM_Unix::CallObjectMethod(jobject obj, jmethodID mid, jint ind) {
	return m_env->CallObjectMethod(obj, mid, ind);
}

jdouble NativeJVM_Unix::CallDoubleMethod(jobject obj, jmethodID mid, ...) {
	va_list vl;
	va_start(vl, mid);
	jdouble jd = m_env->CallDoubleMethod(obj, mid, vl);
	va_end(vl);
	return jd;
}

jobject NativeJVM_Unix::CallObjectDoubleMethod(jobject obj, jmethodID mid, ...) {
	va_list vl;
	va_start(vl, mid);
	jobject jo = m_env->CallObjectMethod(obj, mid, vl);
	va_end(vl);
	return jo;
}

jboolean NativeJVM_Unix::CallBooleanMethod(jobject obj, jmethodID mid) {
	return m_env->CallBooleanMethod(obj, mid);
}

jboolean NativeJVM_Unix::CallBooleanMethod(jobject obj, jmethodID mid, int param) {
	return m_env->CallBooleanMethod(obj, mid, param);
}

jint NativeJVM_Unix::CallIntMethod(jobject obj, jmethodID mid) {
	return m_env->CallIntMethod(obj, mid);
}

jint NativeJVM_Unix::CallIntMethod(jobject obj, jmethodID mid, jint param) {
	return m_env->CallIntMethod(obj, mid, param);
}

jlong NativeJVM_Unix::CallLongMethod(jobject obj, jmethodID mid) {
	return m_env->CallLongMethod(obj, mid);
}

void NativeJVM_Unix::CallMethod(jobject obj, jmethodID mid, jobject param) {
	return m_env->CallVoidMethod(obj, mid, param);
}

void NativeJVM_Unix::CallMethod(jobject obj, jmethodID mid, jint param) {
	return m_env->CallVoidMethod(obj, mid, param);
}

void NativeJVM_Unix::CallMethod(jobject obj, jmethodID mid, jint param1, jint param2) {
	return m_env->CallVoidMethod(obj, mid, param1, param2);
}

void NativeJVM_Unix::CallMethod(jobject obj, jmethodID mid, jdouble param) {
	return m_env->CallVoidMethod(obj, mid, param);
}

void NativeJVM_Unix::CallMethod(jobject obj, jmethodID mid, jlong param) {
	return m_env->CallVoidMethod(obj, mid, param);
}

int NativeJVM_Unix::GetArrayLength(jarray arr) {
	return m_env->GetArrayLength(arr);
}

jobject NativeJVM_Unix::GetObjectArrayElement(jobjectArray arr, int index) {
	return m_env->GetObjectArrayElement(arr, index);
}

const char* NativeJVM_Unix::GetStringUTFChars(jstring str) {
	return m_env->GetStringUTFChars(str, nullptr);
}

void NativeJVM_Unix::ReleaseStringUTFChars(jstring str, const char* s) {
	return m_env->ReleaseStringUTFChars(str, s);
}

jstring NativeJVM_Unix::NewStringUTF(const char* str) {
	return m_env->NewStringUTF(str);
}

void NativeJVM_Unix::DeleteLocalRef(jstring str) {
	return m_env->DeleteLocalRef(str);
}

void NativeJVM_Unix::DeleteLocalRef(jobject str) {
	return m_env->DeleteLocalRef(str);
}

jobject NativeJVM_Unix::NewObject(jclass cls, jmethodID constructor, jobject param) {
    return m_env->NewObject(cls, constructor, param);
}

jobject NativeJVM_Unix::NewObject(jclass cls, jmethodID constructor, jlong param) {
	return m_env->NewObject(cls, constructor, param);
}

jintArray NativeJVM_Unix::NewIntArray(int size) {
	return m_env->NewIntArray(size);
}

jdoubleArray NativeJVM_Unix::NewDoubleArray(int size) {
	return m_env->NewDoubleArray(size);
}

jobjectArray NativeJVM_Unix::NewObjectArray(int size, jclass cls) {
	return m_env->NewObjectArray(size, cls, nullptr);
}

void NativeJVM_Unix::SetIntField(jobject obj, jfieldID fld, jint val) {
	m_env->SetIntField(obj, fld, val);
}

void NativeJVM_Unix::SetDoubleField(jobject obj, jfieldID fld, jdouble val) {
	m_env->SetDoubleField(obj, fld, val);
}

void NativeJVM_Unix::SetLongField(jobject obj, jfieldID fld, jlong val) {
	m_env->SetLongField(obj, fld, val);
}

void NativeJVM_Unix::SetObjectField(jobject obj, jfieldID fld, jobject val) {
	m_env->SetObjectField(obj, fld, val);
}

void NativeJVM_Unix::SetObjectArrayElement(jobjectArray arr, int index, jobject val) {
	m_env->SetObjectArrayElement(arr, index, val);
}

jobject NativeJVM_Unix::GetObjectField(jobject obj, jfieldID fid) {
	return m_env->GetObjectField(obj, fid);
}

jint NativeJVM_Unix::GetIntField(jobject obj, jfieldID fid) {
	return m_env->GetIntField(obj, fid);
}

jdouble NativeJVM_Unix::GetDoubleField(jobject obj, jfieldID fid) {
	return m_env->GetDoubleField(obj, fid);
}

jlong NativeJVM_Unix::GetLongField(jobject obj, jfieldID fid) {
	return m_env->GetLongField(obj, fid);
}

jboolean NativeJVM_Unix::ExceptionCheck() {
	return m_env->ExceptionCheck();
}

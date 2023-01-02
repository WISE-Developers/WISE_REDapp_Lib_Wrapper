/**
 * WISE_REDapp_Lib_Wrapper: jvm_wrapper_win.h
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

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "jvm_wrapper.h"

#include <string>
#include <array>
#include <stdio.h>
#include <algorithm>
#include <cctype>
#include <winioctl.h>
#include <optional>
#include <sstream>
#include "filesystem.hpp"
#include "hss_inlines.h"

#if __has_include(<ntifs.h>)
#include <ntifs.h>
#else

#define SYMLINK_FLAG_RELATIVE 1

typedef struct _REPARSE_DATA_BUFFER {
	ULONG  ReparseTag;
	USHORT  ReparseDataLength;
	USHORT  Reserved;
	union {
		struct {
			USHORT  SubstituteNameOffset;
			USHORT  SubstituteNameLength;
			USHORT  PrintNameOffset;
			USHORT  PrintNameLength;
			ULONG  Flags;
			WCHAR  PathBuffer[1];
			/*  Example of distinction between substitute and print names:
				  mklink /d ldrive c:\
				  SubstituteName: c:\\??\
				  PrintName: c:\
			*/
		} SymbolicLinkReparseBuffer;
		struct {
			USHORT  SubstituteNameOffset;
			USHORT  SubstituteNameLength;
			USHORT  PrintNameOffset;
			USHORT  PrintNameLength;
			WCHAR  PathBuffer[1];
		} MountPointReparseBuffer;
		struct {
			UCHAR  DataBuffer[1];
		} GenericReparseBuffer;
	};
} REPARSE_DATA_BUFFER, * PREPARSE_DATA_BUFFER;

#define REPARSE_DATA_BUFFER_HEADER_SIZE \
  FIELD_OFFSET(REPARSE_DATA_BUFFER, GenericReparseBuffer)
#endif


union info_t {
	char buf[REPARSE_DATA_BUFFER_HEADER_SIZE + MAXIMUM_REPARSE_DATA_BUFFER_SIZE];
	REPARSE_DATA_BUFFER rdb;
};


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
  "xml-apis.jar", "xmlbeans.jar" };


class NativeJVM_Win : public NativeJVM {
public:
	JavaVM *m_jvm;
	JNIEnv* m_env;
	bool m_valid;
	bool m_init;
	unsigned long error_code;

	NativeJVM_Win() : m_jvm(nullptr), m_env(nullptr), m_valid(false), m_init(false) { }
	virtual ~NativeJVM_Win();

	bool Initialize(const std::string& overridePath) override;
	virtual void InitializeVersion();
	bool IsInitialized() override { return m_init; }
	bool IsValid() override { return m_valid; }
	unsigned long GetLoadError() override { return error_code; }

	void _initjava(fs::path libraryPath);

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
	return std::unique_ptr<NativeJVM>{new NativeJVM_Win()};
}

NativeJVM_Win::~NativeJVM_Win() {
	m_jvm = nullptr;
	m_env = nullptr;
}

std::string exec(std::string_view cmd) {
	std::array<char, 128> buffer{};
	std::string result;
	std::shared_ptr<FILE> pipe(_popen(cmd.data(), "r"), _pclose);
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
#ifdef WIN32
	const std::array<std::string, 4> toTry = { "bin\\server", "bin\\client", "jre\\bin\\server", "jre\\bin\\client" };
#else
	const std::array<std::string, 4> toTry = { "bin/server", "bin/client", "jre/bin/server", "jre/bin/client" };
#endif

	for (auto& loc : toTry) {
		fs::path attempt = path / loc;
		if (fs::exists(attempt)) {
			return std::make_optional(std::move(attempt));
		}
	}
	return std::nullopt;
}

fs::path resolvePath(const fs::path& path) {
	//MSVC warns if this is allocated on the stack because of its size, so put it on the heap
	std::shared_ptr<info_t> info(new info_t());
	auto str = path.string();

	//if the path is a symlink resolve it to its actual location
	DWORD readSize = 0;
	int bufferHelper = 0;
	std::string temp;
	do {
		std::shared_ptr<void> hFile(CreateFile(str.c_str(),
				GENERIC_READ,
				0,
				nullptr,
				OPEN_EXISTING,
				FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OPEN_REPARSE_POINT,
				nullptr),
			CloseHandle);
		if (hFile != nullptr && hFile.get() != INVALID_HANDLE_VALUE) {
			if (::DeviceIoControl(hFile.get(), FSCTL_GET_REPARSE_POINT, 0, 0, info->buf, sizeof(info_t), &readSize, 0)) {
				temp.assign(static_cast<wchar_t*>(info->rdb.SymbolicLinkReparseBuffer.PathBuffer) +
					info->rdb.SymbolicLinkReparseBuffer.PrintNameOffset / sizeof(wchar_t),
					static_cast<wchar_t*>(info->rdb.SymbolicLinkReparseBuffer.PathBuffer) +
					info->rdb.SymbolicLinkReparseBuffer.PrintNameOffset / sizeof(wchar_t) +
					info->rdb.SymbolicLinkReparseBuffer.PrintNameLength / sizeof(wchar_t));
				if (temp == str)
					readSize = 0;
				else {
					readSize = temp.length();
					str = temp;
				}
			}
			else
				readSize = 0;
			bufferHelper++;
		}
		else
			readSize = 0;
	} while (readSize > 0 && bufferHelper < 10);
	str.erase(std::find(str.begin(), str.end(), '\0'), str.end());

	return str;
}



/// Find the location of the local Java install.
/// 
/// NOTE: Java install paths in the registry are not guaranteed to be correct. Java 8 began the move from using \bin\client
/// to using \bin\server for the location of jvm.dll but they updated the path in the registry before they updated the path
/// that jvm.dll was actually installed to. So the RuntimeLib registry entry that is supposed to point directly to the file
/// that we want to load is not guaranteed to point to the file. Have to use the JavaHome entry instead which I haven't yet
/// found a Java version that has this set incorrectly then test for the existance of \bin\client or \bin\server.
std::optional<fs::path> findJavaInstall(const std::string& overridePath, std::string* error) {
	auto overPath = overridePath;
	overPath.erase(std::remove(overPath.begin(), overPath.end(), '\n'), overPath.end());
	overPath.erase(std::remove(overPath.begin(), overPath.end(), '\r'), overPath.end());
	overPath.erase(std::remove(overPath.begin(), overPath.end(), '\0'), overPath.end());
	//if the user has specified a Java path to use
	if (overPath.size() > 0) {
		if (error) {
			*error += R"T(Java user path: ")T";
			*error += overPath;
			*error += R"T(")T";
		}
		auto test = searchPath(overPath);
		if (test.has_value())
			return test;
		else if (error)
			*error += " (invalid Java path)\r\n";
	}
	//if the user hasn't sepecified a Java path try to find one ourselves
	else {
		//allow JAVA_HOME to override all other values
		std::string javaHome = hss::getenv("JAVA_HOME");
		if (javaHome.size()) {
			if (error)
				*error += R"T(JAVA_HOME = ")T" + javaHome + R"T(")T";
			if (javaHome.find("Common Files") == std::string::npos) {
				auto test = searchPath(javaHome);
				//this is a valid java install, use this
				if (test.has_value())
					return test;
			}
			if (error)
				*error += " (invalid Java path)\r\n";
		}
		else if (error)
			*error += "JAVA_HOME is not set\r\n";
		//check the PATH for java
		std::string str = exec("where java");
		int pathsChecked = 0;
		//will start with INFO: if java is not in the path at all
		if (!str.starts_with("INFO:")) {
			std::stringstream ss(str);
			std::string to;
			while (std::getline(ss, to, '\n')) {
				//remove all invalid newline characters
				to.erase(std::remove(to.begin(), to.end(), '\n'), to.end());
				to.erase(std::remove(to.begin(), to.end(), '\r'), to.end());
				fs::path p(to);
				p = p.parent_path().parent_path();
				if (error) {
					if (pathsChecked == 0)
						*error += "Testing paths: \r\n";
					*error += p.string();
				}
				//ignore the common files version
				if (to.find("Common Files") == std::string::npos) {
					auto test = searchPath(p);
					//this is a valid java install, use this
					if (test.has_value())
						return test;
				}
				if (error)
					*error += " (invalid Java path)\r\n";
				pathsChecked++;
			}
		}
		if (pathsChecked == 0 && error)
			*error += "Java is not in the path\r\n";
		//fallback to trying the registry
		bool foundRegistry = false;
		HKEY hk;
		std::string key = TEXT("SOFTWARE\\JavaSoft\\JRE");
		LSTATUS n = RegOpenKeyEx(HKEY_LOCAL_MACHINE, key.c_str(), NULL, KEY_READ, &hk);
		str.clear();
		//the key was successfully opened
		if (n == 0) {
			DWORD dwSize = 0;
			//read SOFTWARE\JavaSoft\JRE\CurrentVersion for the most recently installed Java version
			n = RegGetValue(hk, nullptr, TEXT("CurrentVersion"), RRF_RT_REG_SZ, nullptr, nullptr, &dwSize);
			str.resize(dwSize);
			n = RegGetValue(hk, nullptr, TEXT("CurrentVersion"), RRF_RT_REG_SZ, nullptr, &str[0], &dwSize);
			RegCloseKey(hk);
			key += "\\";
			key += str;
			//open the registry key for the most recent version of Java
			n = RegOpenKeyEx(HKEY_LOCAL_MACHINE, key.c_str(), NULL, KEY_READ, &hk);
			//the key was successfully opened
			if (n == 0) {
				dwSize = 0;
				//read SOFTWARE\JavaSoft\JRE\CurrentVersion\<version>\JavaHome for the location of the Java install
				//NOTE: using RuntimeLib instead of JavaHome may look like a good idea, but it's not. Read the method description.
				n = RegGetValue(hk, nullptr, TEXT("JavaHome"), RRF_RT_REG_SZ, nullptr, nullptr, &dwSize);
				str.resize(dwSize);
				n = RegGetValue(hk, nullptr, TEXT("JavaHome"), RRF_RT_REG_SZ, nullptr, &str[0], &dwSize);
				str.erase(std::remove(str.begin(), str.end(), '\0'), str.end());
				RegCloseKey(hk);
				if (error) {
					foundRegistry = true;
					*error += "Checking registry:\r\n";
					*error += str;
				}
				auto test = searchPath(str);
				//this is a valid java install, use this
				if (test.has_value())
					return test;
				else if (error)
					*error += " (invalid Java path)\r\n";
			}
		}
		//try a second registry location
		key = TEXT("SOFTWARE\\JavaSoft\\Java Runtime Environment");
		n = RegOpenKeyEx(HKEY_LOCAL_MACHINE, key.c_str(), NULL, KEY_READ, &hk);
		//the key was successfully opened
		if (n == 0) {
			DWORD dwSize = 0;
			//read SOFTWARE\JavaSoft\Java Runtime Environment\CurrentVersion for the most recently installed Java version
			n = RegGetValue(hk, nullptr, TEXT("CurrentVersion"), RRF_RT_REG_SZ, nullptr, nullptr, &dwSize);
			str.resize(dwSize);
			n = RegGetValue(hk, nullptr, TEXT("CurrentVersion"), RRF_RT_REG_SZ, nullptr, &str[0], &dwSize);
			RegCloseKey(hk);
			key += "\\";
			key += str;
			n = RegOpenKeyEx(HKEY_LOCAL_MACHINE, key.c_str(), NULL, KEY_READ, &hk);
			//the key was successfully opened
			if (n == 0) {
				dwSize = 0;
				//read SOFTWARE\JavaSoft\Java Runtime Environment\CurrentVersion\<version>\JavaHome for the location of the Java install
				//NOTE: using RuntimeLib instead of JavaHome may look like a good idea, but it's not. Read the method description.
				n = RegGetValue(hk, nullptr, TEXT("JavaHome"), RRF_RT_REG_SZ, nullptr, nullptr, &dwSize);
				str.resize(dwSize);
				n = RegGetValue(hk, nullptr, TEXT("JavaHome"), RRF_RT_REG_SZ, nullptr, &str[0], &dwSize);
				str.erase(std::remove(str.begin(), str.end(), '\0'), str.end());

				RegCloseKey(hk);
				if (error) {
					if (!foundRegistry)
						*error += "Checking registry:\r\n";
					foundRegistry = true;
					*error += str;
				}
				auto test = searchPath(str);
				//this is a valid java install, use this
				if (test.has_value())
					return test;
				else if (error)
					*error += " (invalid Java path)\r\n";
			}
		}

		if (!foundRegistry && error)
			*error += "Java is not in the registry";
	}

	return std::nullopt;
}

bool NativeJVM_Win::Initialize(const std::string& overridePath) {
	if (!m_init) {
		detailedError.clear();
		auto path = findJavaInstall(overridePath, &detailedError);

		m_valid = false;

		if (path.has_value()) {
			//this should be in the bin folder and we want the root folder
			auto p = path.value();

			if (fs::exists(p)) {
				try {
					char old[_MAX_PATH];
					DWORD oldExists = GetDllDirectory(_MAX_PATH, old);
					SetDllDirectory(p.string().c_str());
					_initjava(p);
					if (oldExists > 0)
						SetDllDirectory(old);
					else
						SetDllDirectory(nullptr);
				}
				catch (...) { }
			}
			else
				internalError = ERROR_NO_JAVA;

			m_init = true;
		}
		else {
			internalError = ERROR_NO_JNI;
			m_valid = false;
		}
	}
	return m_valid;
}

void NativeJVM_Win::InitializeVersion() {
	jint ver = m_env->GetVersion();
	jint major = (ver >> 16) & 0xFFFF;
	jint minor = ver & 0xFFFF;
	javaVersion = std::to_string(major) + "." + std::to_string(minor);
}

static int port = 8989;
EXTERN_C IMAGE_DOS_HEADER __ImageBase;

jint CreateJavaVM(JavaVM **pvm, void **penv, void *args, unsigned long* e_code) {
	jint ret = JNI_ERR;
	__try {
		ret = JNI_CreateJavaVM(pvm, penv, args);
		*e_code = 0;
	}
	__except (EXCEPTION_EXECUTE_HANDLER) {
		*e_code = GetExceptionCode();
		printf("ERROR LOADING JAVA LIBRARY: %lX\n", *e_code);
		fflush(stdout);
	}
	return ret;
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
		else if (loadError != 0) {
			LPVOID lpMsgBuf;
			if (FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_IGNORE_INSERTS,
				nullptr, loadError, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPTSTR)&lpMsgBuf, 0, nullptr) > 0) {
				std::string retval((TCHAR*)lpMsgBuf);
				LocalFree(lpMsgBuf);
				return retval;
			}
		}
	}
	return "Unknown Java initialization issue";
}

/**
 * Run a command on the command line and return what was printed to cout.
 */
std::string exec(const char* cmd) {
	std::array<char, 128> buffer{};
	std::string result;
	std::shared_ptr<FILE> pipe(_popen(cmd, "rt"), _pclose);
	if (!pipe)
		throw std::runtime_error("popen() failed!");
	while (fgets(buffer.data(), 128, pipe.get()) != nullptr)
		result += buffer.data();
	return result;
}

void NativeJVM_Win::_initjava(fs::path libraryPath) {
	JavaVMInitArgs vm_args;
	int argCount;
#ifdef _DEBUG
	argCount = 2;
#else
	argCount = 1;
#endif
	JavaVMOption* options = new JavaVMOption[argCount];
	std::string key = "-Djava.class.path=";
	char wdir[_MAX_PATH];
	GetModuleFileName((HINSTANCE)&__ImageBase, wdir, _MAX_PATH);
	fs::path p(wdir);
	for (auto& dep : dependencies) {
		p = p.replace_filename(dep);
		if (!fs::exists(p))
			internalError = ERROR_MISSING_JAR;
		key += p.string();
		key += ";";
	}
	std::replace(key.begin(), key.end(), '\\', '/');
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
	jvmError = CreateJavaVM(&jvm, (void**)&env, &vm_args, &error_code);
	delete[] options;
	if (jvmError == JNI_OK && jvm && env) {
		m_env = env;
		m_jvm = jvm;
		m_valid = internalError != ERROR_MISSING_JAR;
		javaPath = libraryPath.parent_path().parent_path().string();
		InitializeVersion();
	}
	else if (jvmError != JNI_OK && jvmError != JNI_EEXIST) {
		if (internalError == ERROR_OK) {
			fs::path java = libraryPath.parent_path() / "java.exe";
			std::string call = "\"" + java.string() + "\" -version 2>&1";
			std::string output = exec(call.c_str());
			if (output.size() > 0) {
				std::string search = "64-bit";
				auto it = std::search(output.begin(), output.end(),
					search.begin(), search.end(),
					[](char ch1, char ch2) { return std::tolower(ch1) == std::tolower(ch2); });
				if (it == output.end())
					internalError = ERROR_ARCHITECTURE;
			}
			if (internalError == ERROR_OK)
				internalError = ERROR_JVM_ERROR;
		}
		m_valid = false;
	}
}

jclass NativeJVM_Win::FindClass(const std::string& signature) {
	return m_env->FindClass(signature.c_str());
}

jfieldID NativeJVM_Win::GetStaticFieldID(jclass clz, const std::string& name, const std::string& signature) {
	return m_env->GetStaticFieldID(clz, name.c_str(), signature.c_str());
}

jobject NativeJVM_Win::GetStaticObjectField(jclass clz, jfieldID fld) {
	return m_env->GetStaticObjectField(clz, fld);
}

jmethodID NativeJVM_Win::GetMethodID(jclass clz, const std::string& name, const std::string& signature) {
	return m_env->GetMethodID(clz, name.c_str(), signature.c_str());
}

jmethodID NativeJVM_Win::GetStaticMethodID(jclass clz, const std::string& name, const std::string& signature) {
	return m_env->GetStaticMethodID(clz, name.c_str(), signature.c_str());
}

jfieldID NativeJVM_Win::GetFieldID(jclass clz, const std::string& name, const std::string& signature) {
	return m_env->GetFieldID(clz, name.c_str(), signature.c_str());
}

jobject NativeJVM_Win::CallStaticObjectMethod(jclass cls, jmethodID mid, jobject param) {
	return m_env->CallStaticObjectMethod(cls, mid, param);
}

jobject NativeJVM_Win::CallStaticObjectMethod(jclass cls, jmethodID mid, jint param) {
	return m_env->CallStaticObjectMethod(cls, mid, param);
}

jboolean NativeJVM_Win::CallStaticBooleanMethod(jclass cls, jmethodID mid, jobject param) {
	return m_env->CallStaticBooleanMethod(cls, mid, param);
}

jobject NativeJVM_Win::CallObjectMethodO(jobject obj, jmethodID mid, jobject o) {
	return m_env->CallObjectMethod(obj, mid, o);
}

jobject NativeJVM_Win::CallObjectMethodOO(jobject obj, jmethodID mid, jobject o1, jobject o2) {
	return m_env->CallObjectMethod(obj, mid, o1, o2);
}

jobject NativeJVM_Win::CallObjectMethodOOI(jobject obj, jmethodID mid, jobject o1, jobject o2, jint i1) {
	return m_env->CallObjectMethod(obj, mid, o1, o2, i1);
}

jobject NativeJVM_Win::CallObjectMethod(jobject obj, jmethodID mid, jint ind) {
	return m_env->CallObjectMethod(obj, mid, ind);
}

jdouble NativeJVM_Win::CallDoubleMethod(jobject obj, jmethodID mid, ...) {
	va_list vl;
	va_start(vl, mid);
	jdouble jd = m_env->CallDoubleMethod(obj, mid, vl);
	va_end(vl);
	return jd;
}

jobject NativeJVM_Win::CallObjectDoubleMethod(jobject obj, jmethodID mid, ...) {
	va_list vl;
	va_start(vl, mid);
	jobject jo = m_env->CallObjectMethod(obj, mid, vl);
	va_end(vl);
	return jo;
}

jboolean NativeJVM_Win::CallBooleanMethod(jobject obj, jmethodID mid) {
	return m_env->CallBooleanMethod(obj, mid);
}

jboolean NativeJVM_Win::CallBooleanMethod(jobject obj, jmethodID mid, int param) {
	return m_env->CallBooleanMethod(obj, mid, param);
}

jint NativeJVM_Win::CallIntMethod(jobject obj, jmethodID mid) {
	return m_env->CallIntMethod(obj, mid);
}

jint NativeJVM_Win::CallIntMethod(jobject obj, jmethodID mid, jint param) {
	return m_env->CallIntMethod(obj, mid, param);
}

jlong NativeJVM_Win::CallLongMethod(jobject obj, jmethodID mid) {
	return m_env->CallLongMethod(obj, mid);
}

void NativeJVM_Win::CallMethod(jobject obj, jmethodID mid, jobject param) {
	return m_env->CallVoidMethod(obj, mid, param);
}

void NativeJVM_Win::CallMethod(jobject obj, jmethodID mid, jint param) {
	return m_env->CallVoidMethod(obj, mid, param);
}

void NativeJVM_Win::CallMethod(jobject obj, jmethodID mid, jint param1, jint param2) {
	return m_env->CallVoidMethod(obj, mid, param1, param2);
}

void NativeJVM_Win::CallMethod(jobject obj, jmethodID mid, jdouble param) {
	return m_env->CallVoidMethod(obj, mid, param);
}

void NativeJVM_Win::CallMethod(jobject obj, jmethodID mid, jlong param) {
	return m_env->CallVoidMethod(obj, mid, param);
}

int NativeJVM_Win::GetArrayLength(jarray arr) {
	return m_env->GetArrayLength(arr);
}

jobject NativeJVM_Win::GetObjectArrayElement(jobjectArray arr, int index) {
	return m_env->GetObjectArrayElement(arr, index);
}

const char* NativeJVM_Win::GetStringUTFChars(jstring str) {
	return m_env->GetStringUTFChars(str, nullptr);
}

void NativeJVM_Win::ReleaseStringUTFChars(jstring str, const char* s) {
	return m_env->ReleaseStringUTFChars(str, s);
}

jstring NativeJVM_Win::NewStringUTF(const char* str) {
	return m_env->NewStringUTF(str);
}

void NativeJVM_Win::DeleteLocalRef(jstring str) {
	return m_env->DeleteLocalRef(str);
}

void NativeJVM_Win::DeleteLocalRef(jobject str) {
	return m_env->DeleteLocalRef(str);
}

jobject NativeJVM_Win::NewObject(jclass cls, jmethodID constructor, jobject param) {
	return m_env->NewObject(cls, constructor, param);
}

jobject NativeJVM_Win::NewObject(jclass cls, jmethodID constructor, jlong param) {
	return m_env->NewObject(cls, constructor, param);
}

jintArray NativeJVM_Win::NewIntArray(int size) {
	return m_env->NewIntArray(size);
}

jdoubleArray NativeJVM_Win::NewDoubleArray(int size) {
	return m_env->NewDoubleArray(size);
}

jobjectArray NativeJVM_Win::NewObjectArray(int size, jclass cls) {
	return m_env->NewObjectArray(size, cls, nullptr);
}

void NativeJVM_Win::SetIntField(jobject obj, jfieldID fld, jint val) {
	m_env->SetIntField(obj, fld, val);
}

void NativeJVM_Win::SetDoubleField(jobject obj, jfieldID fld, jdouble val) {
	m_env->SetDoubleField(obj, fld, val);
}

void NativeJVM_Win::SetLongField(jobject obj, jfieldID fld, jlong val) {
	m_env->SetLongField(obj, fld, val);
}

void NativeJVM_Win::SetObjectField(jobject obj, jfieldID fld, jobject val) {
	m_env->SetObjectField(obj, fld, val);
}

void NativeJVM_Win::SetObjectArrayElement(jobjectArray arr, int index, jobject val) {
	m_env->SetObjectArrayElement(arr, index, val);
}

jobject NativeJVM_Win::GetObjectField(jobject obj, jfieldID fid) {
	return m_env->GetObjectField(obj, fid);
}

jint NativeJVM_Win::GetIntField(jobject obj, jfieldID fid) {
	return m_env->GetIntField(obj, fid);
}

jdouble NativeJVM_Win::GetDoubleField(jobject obj, jfieldID fid) {
	return m_env->GetDoubleField(obj, fid);
}

jlong NativeJVM_Win::GetLongField(jobject obj, jfieldID fid) {
	return m_env->GetLongField(obj, fid);
}

jboolean NativeJVM_Win::ExceptionCheck() {
	return m_env->ExceptionCheck();
}

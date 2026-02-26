#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <map>
#include <string>
#include <vector>
#include <sstream>
#include <algorithm>

#include "zygisk.hpp"
#include "dobby.h"

#define LOGD(...) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__)

#define DEX_FILE_PATH "/data/adb/modules/playintegrityfix/classes.dex"
#define CUSTOM_PROP_FILE_PATH "/data/adb/modules/playintegrityfix/custom.pif.prop"
#define VENDING_PACKAGE "com.android.vending"
#define DROIDGUARD_PACKAGE "com.google.android.gms.unstable"

static int verboseLogs = 0;
static int spoofBuild = 1;
static int spoofProps = 1;
static int spoofProvider = 1;
static int spoofSignature = 0;
static int spoofVendingFinger = 0;
static int spoofVendingSdk = 0;
static int spoofPixel = 0;

static std::string pixelManufacturer;
static std::string pixelModel;
static std::string pixelDevice;
static std::string pixelBrand;
static std::string vendingFingerprintValue;

static std::map<std::string, std::string> propMap;
static std::map<std::string, std::string> jsonProps;

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);

static std::map<void *, T_Callback> callbacks;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {
    if (cookie == nullptr || name == nullptr || value == nullptr || !callbacks.contains(cookie)) return;
    const char *oldValue = value;
    std::string prop(name);
    if (jsonProps.count(prop)) {
        value = jsonProps[prop].c_str();
    } else {
        for (const auto &p: jsonProps) {
            if (p.first.starts_with("*") && prop.ends_with(p.first.substr(1))) {
                value = p.second.c_str();
                break;
            }
        }
    }
    if (oldValue != value) {
        LOGD("[%s]: %s -> %s", name, oldValue, value);
    } else if (verboseLogs > 99) {
        LOGD("[%s]: %s (unchanged)", name, oldValue);
    }
    return callbacks[cookie](cookie, name, value, serial);
}

static void (*o_system_property_read_callback)(const prop_info *, T_Callback, void *);

static void my_system_property_read_callback(const prop_info *pi, T_Callback callback, void *cookie) {
    if (pi == nullptr || callback == nullptr || cookie == nullptr) {
        return o_system_property_read_callback(pi, callback, cookie);
    }
    callbacks[cookie] = callback;
    return o_system_property_read_callback(pi, modify_callback, cookie);
}

static void doHook() {
    void *handle = DobbySymbolResolver(nullptr, "__system_property_read_callback");
    if (handle == nullptr) {
        LOGD("Couldn't find '__system_property_read_callback' handle");
        return;
    }
    LOGD("Found '__system_property_read_callback' handle at %p", handle);
    DobbyHook(handle, reinterpret_cast<dobby_dummy_func_t>(my_system_property_read_callback),
        reinterpret_cast<dobby_dummy_func_t *>(&o_system_property_read_callback));
}

class PlayIntegrityFix : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        bool isGms = false, isDroidGuardOrVending = false;
        auto rawProcess = env->GetStringUTFChars(args->nice_name, nullptr);
        auto rawDir = env->GetStringUTFChars(args->app_data_dir, nullptr);
        if (rawDir == nullptr) {
            env->ReleaseStringUTFChars(args->nice_name, rawProcess);
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        pkgName = rawProcess;
        std::string_view dir(rawDir);
        isGms = dir.ends_with("/com.google.android.gms") || dir.ends_with("/com.android.vending");
        isDroidGuardOrVending = pkgName == DROIDGUARD_PACKAGE || pkgName == VENDING_PACKAGE;
        env->ReleaseStringUTFChars(args->nice_name, rawProcess);
        env->ReleaseStringUTFChars(args->app_data_dir, rawDir);
        if (!isGms) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);
        if (!isDroidGuardOrVending) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        std::vector<char> configVector;
        long dexSize = 0, configSize = 0;
        int fd = api->connectCompanion();
        read(fd, &dexSize, sizeof(long));
        read(fd, &configSize, sizeof(long));
        if (dexSize < 1) {
            close(fd);
            LOGD("Couldn't read dex file");
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        if (configSize < 1) {
            close(fd);
            LOGD("Couldn't read config file");
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        LOGD("Read from file descriptor for 'dex' -> %ld bytes", dexSize);
        LOGD("Read from file descriptor for 'config' -> %ld bytes", configSize);
        dexVector.resize(dexSize);
        read(fd, dexVector.data(), dexSize);
        configVector.resize(configSize);
        read(fd, configVector.data(), configSize);
        close(fd);
        
        // Parse PROP file directly into propMap
        parsePropFile(configVector);
        configVector.clear();
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (dexVector.empty() || propMap.empty()) return;
        readConfig();
        if (pkgName == VENDING_PACKAGE) spoofBuild = spoofProps = spoofProvider = spoofSignature = 0;
        else spoofVendingFinger = spoofVendingSdk = spoofPixel = 0;
        if (spoofProps > 0) doHook();
        if (spoofBuild + spoofProvider + spoofSignature + spoofVendingFinger + spoofVendingSdk + spoofPixel > 0) inject();
        dexVector.clear();
        propMap.clear();
        jsonProps.clear();
    }

    void preServerSpecialize(zygisk::ServerSpecializeArgs *args) override {
        api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
    }

private:
    zygisk::Api *api = nullptr;
    JNIEnv *env = nullptr;
    std::vector<char> dexVector;
    std::string pkgName;

    void parsePropFile(const std::vector<char>& configVector) {
        std::string configString(configVector.begin(), configVector.end());
        
        // Remove carriage returns
        configString.erase(std::remove(configString.begin(), configString.end(), '\r'), configString.end());
        
        char propDelimiter = '=';
        char commentDelimiter = '#';
        size_t beginPos = 0, endPos = 0;
        
        while ((endPos = configString.find('\n', beginPos)) != std::string::npos) {
            std::string line = configString.substr(beginPos, endPos - beginPos);
            beginPos = endPos + 1;
            
            if (line.empty() || line[0] == '#') continue;
            
            std::string name, value;
            size_t propDelimiterPos = line.find(propDelimiter);
            
            if (propDelimiterPos != std::string::npos) {
                name = line.substr(0, propDelimiterPos);
                value = line.substr(propDelimiterPos + 1);
            } else {
                continue;
            }
            
            // Remove inline comments and trailing spaces
            size_t commentDelimiterPos = value.find(commentDelimiter);
            if (commentDelimiterPos != std::string::npos) {
                value = value.substr(0, commentDelimiterPos);
                size_t lastPos = value.find_last_not_of(" ");
                if (lastPos != std::string::npos) value.resize(lastPos + 1);
            }
            
            // Trim leading/trailing whitespace from name
            size_t nameStart = name.find_first_not_of(" ");
            size_t nameEnd = name.find_last_not_of(" ");
            if (nameStart != std::string::npos && nameEnd != std::string::npos) {
                name = name.substr(nameStart, nameEnd - nameStart + 1);
            }
            
            // Trim leading/trailing whitespace from value
            size_t valueStart = value.find_first_not_of(" ");
            size_t valueEnd = value.find_last_not_of(" ");
            if (valueStart != std::string::npos && valueEnd != std::string::npos) {
                value = value.substr(valueStart, valueEnd - valueStart + 1);
            }
            
            if (!name.empty()) {
                propMap[name] = value;
            }
        }
        
        LOGD("Parsed %d properties from config file", static_cast<int>(propMap.size()));
    }

    std::string getPropValue(const std::string& key, const std::string& defaultValue = "") {
        auto it = propMap.find(key);
        if (it != propMap.end() && !it->second.empty()) {
            return it->second;
        }
        return defaultValue;
    }

    int getPropInt(const std::string& key, int defaultValue = 0) {
        std::string value = getPropValue(key);
        if (!value.empty()) {
            try {
                return std::stoi(value);
            } catch (...) {
                return defaultValue;
            }
        }
        return defaultValue;
    }

    void readConfig() {
        LOGD("Config contains %d keys!", static_cast<int>(propMap.size()));
        
        verboseLogs = getPropInt("verboseLogs");
        spoofVendingSdk = getPropInt("spoofVendingSdk");
        
        std::string spoofVendingFingerValue = getPropValue("spoofVendingFinger");
        if (!spoofVendingFingerValue.empty()) {
            if (spoofVendingFingerValue.find_first_not_of("01") != std::string::npos) {
                spoofVendingFinger = 1;
                vendingFingerprintValue = spoofVendingFingerValue;
            } else {
                spoofVendingFinger = std::stoi(spoofVendingFingerValue);
                if (spoofVendingFinger > 0) {
                    vendingFingerprintValue = getPropValue("FINGERPRINT");
                }
            }
        }
        
        spoofPixel = getPropInt("spoofPixel");
        if (spoofPixel > 0) {
            pixelManufacturer = getPropValue("MANUFACTURER");
            pixelModel = getPropValue("MODEL");
            pixelDevice = getPropValue("DEVICE");
            pixelBrand = getPropValue("BRAND");
        }
        
        if (pkgName == VENDING_PACKAGE) {
            propMap.clear();
            return;
        }
        
        spoofBuild = getPropInt("spoofBuild", 1);
        spoofProps = getPropInt("spoofProps", 1);
        spoofProvider = getPropInt("spoofProvider", 1);
        spoofSignature = getPropInt("spoofSignature");
        
        // Process wildcard properties for prop hooking
        for (const auto& pair : propMap) {
            if (pair.first.find_first_of("*.") != std::string::npos) {
                if (!pair.second.empty()) {
                    jsonProps[pair.first] = pair.second;
                }
            }
        }
    }

    std::string buildJsonString() {
        // Build JSON string manually for Java side
        std::string jsonStr = "{";
        bool first = true;
        
        for (const auto& pair : propMap) {
            if (pair.first.find_first_of("*.") == std::string::npos) {
                if (!first) jsonStr += ",";
                first = false;
                
                // Simple JSON escaping for values
                std::string escapedValue = pair.second;
                size_t pos = 0;
                while ((pos = escapedValue.find('\\', pos)) != std::string::npos) {
                    escapedValue.insert(pos, "\\");
                    pos += 2;
                }
                pos = 0;
                while ((pos = escapedValue.find('"', pos)) != std::string::npos) {
                    escapedValue.insert(pos, "\\");
                    pos += 2;
                }
                
                jsonStr += "\"" + pair.first + "\":\"" + escapedValue + "\"";
            }
        }
        
        jsonStr += "}";
        return jsonStr;
    }

    void inject() {
        const char* niceName = pkgName == VENDING_PACKAGE ? "PS" : "DG";
        LOGD("JNI %s: Getting system classloader", niceName);
        auto clClass = env->FindClass("java/lang/ClassLoader");
        auto getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
        auto systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);
        LOGD("JNI %s: Creating module classloader", niceName);
        auto dexClClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
        auto dexClInit = env->GetMethodID(dexClClass, "<init>", "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
        auto buffer = env->NewDirectByteBuffer(dexVector.data(), static_cast<jlong>(dexVector.size()));
        auto dexCl = env->NewObject(dexClClass, dexClInit, buffer, systemClassLoader);
        LOGD("JNI %s: Loading module class", niceName);
        auto loadClass = env->GetMethodID(clClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        const char* className = pkgName == VENDING_PACKAGE ? "es.chiteroman.playintegrityfix.EntryPointVending" : "es.chiteroman.playintegrityfix.EntryPoint";
        auto entryClassName = env->NewStringUTF(className);
        auto entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);
        auto entryClass = (jclass) entryClassObj;
        if (pkgName == VENDING_PACKAGE) {
            LOGD("JNI %s: Calling EntryPointVending.init", niceName);
            auto entryInit = env->GetStaticMethodID(entryClass, "init", "(IIILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
            auto javaFinger = env->NewStringUTF(vendingFingerprintValue.c_str());
            auto javaManuf = env->NewStringUTF(pixelManufacturer.c_str());
            auto javaModel = env->NewStringUTF(pixelModel.c_str());
            auto javaDevice = env->NewStringUTF(pixelDevice.c_str());
            auto javaBrand = env->NewStringUTF(pixelBrand.c_str());
            env->CallStaticVoidMethod(entryClass, entryInit, verboseLogs, spoofVendingFinger, spoofVendingSdk, javaFinger, javaManuf, javaModel, javaDevice, javaBrand);
            env->DeleteLocalRef(javaFinger);
            env->DeleteLocalRef(javaManuf);
            env->DeleteLocalRef(javaModel);
            env->DeleteLocalRef(javaDevice);
            env->DeleteLocalRef(javaBrand);
        } else {
            LOGD("JNI %s: Sending JSON", niceName);
            auto receiveJson = env->GetStaticMethodID(entryClass, "receiveJson", "(Ljava/lang/String;)V");
            std::string jsonStr = buildJsonString();
            auto javaStr = env->NewStringUTF(jsonStr.c_str());
            env->CallStaticVoidMethod(entryClass, receiveJson, javaStr);
            LOGD("JNI %s: Calling EntryPoint.init", niceName);
            auto entryInit = env->GetStaticMethodID(entryClass, "init", "(IIII)V");
            env->CallStaticVoidMethod(entryClass, entryInit, verboseLogs, spoofBuild, spoofProvider, spoofSignature);
            env->DeleteLocalRef(javaStr);
        }
        env->DeleteLocalRef(clClass);
        env->DeleteLocalRef(systemClassLoader);
        env->DeleteLocalRef(dexClClass);
        env->DeleteLocalRef(buffer);
        env->DeleteLocalRef(dexCl);
        env->DeleteLocalRef(entryClassName);
        env->DeleteLocalRef(entryClassObj);
    }
};

static void companion(int fd) {
    long dexSize = 0, configSize = 0;
    std::vector<char> dexVector, configVector;
    FILE *dex = fopen(DEX_FILE_PATH, "rb");
    if (dex) {
        fseek(dex, 0, SEEK_END);
        dexSize = ftell(dex);
        fseek(dex, 0, SEEK_SET);
        dexVector.resize(dexSize);
        fread(dexVector.data(), 1, dexSize, dex);
        fclose(dex);
    }
    FILE *config = fopen(CUSTOM_PROP_FILE_PATH, "r");
    if (config) {
        fseek(config, 0, SEEK_END);
        configSize = ftell(config);
        fseek(config, 0, SEEK_SET);
        configVector.resize(configSize);
        fread(configVector.data(), 1, configSize, config);
        fclose(config);
    }
    write(fd, &dexSize, sizeof(long));
    write(fd, &configSize, sizeof(long));
    write(fd, dexVector.data(), dexSize);
    write(fd, configVector.data(), configSize);
    dexVector.clear();
    configVector.clear();
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)

REGISTER_ZYGISK_COMPANION(companion)
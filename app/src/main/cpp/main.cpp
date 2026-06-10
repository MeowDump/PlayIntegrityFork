#include <android/log.h>
#include <sys/system_properties.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <algorithm>
#include "zygisk.hpp"
#include "dobby.h"

#define DEX_FILE_PATH "/data/adb/modules/playintegrityfix/classes.dex"
#define CUSTOM_PROP_FILE_PATH "/data/adb/modules/playintegrityfix/pixel.txt"
#define APPS_LIST_FILE_PATH "/data/adb/modules/playintegrityfix/apps.txt"
#define VENDING_PACKAGE "com.android.vending"
#define DROIDGUARD_PACKAGE "com.google.android.gms.unstable"

static int g_verboseLogs = 0;

#define LOGD(...) do { if (g_verboseLogs > 0) __android_log_print(ANDROID_LOG_DEBUG, "PIF/Native", __VA_ARGS__); } while(0)
#define LOGI(...) do { if (g_verboseLogs > 0) __android_log_print(ANDROID_LOG_INFO, "PIF/Native", __VA_ARGS__); } while(0)
#define LOGE(...) do { if (g_verboseLogs > 0) __android_log_print(ANDROID_LOG_ERROR, "PIF/Native", __VA_ARGS__); } while(0)

static int spoofBuild = 1;
static int spoofProps = 1;
static int spoofProvider = 1;
static int spoofSignature = 0;
static int spoofVendingFinger = 1;
static int spoofVendingSdk = 0;
static int spoofPixel = 1;
static int spoofApps = 0;

static std::string pixelManufacturer;
static std::string pixelModel;
static std::string pixelDevice;
static std::string pixelBrand;
static std::string vendingFingerprintValue;

static std::unordered_map<std::string, std::string> propMap;
static std::unordered_map<std::string, std::string> jsonProps;
static std::unordered_set<std::string> spoofAppPackages;

typedef void (*T_Callback)(void *, const char *, const char *, uint32_t);
static std::unordered_map<void *, T_Callback> callbacks;

static void modify_callback(void *cookie, const char *name, const char *value, uint32_t serial) {
    if (!cookie || !name || !value) return;
    auto it = callbacks.find(cookie);
    if (it == callbacks.end()) return;
    const char *oldValue = value;
    std::string_view prop(name);
    bool modified = false;
    auto jt = jsonProps.find(std::string(prop));
    if (jt != jsonProps.end()) {
        value = jt->second.c_str();
        modified = true;
    } else {
        for (const auto &p : jsonProps) {
            if (!p.first.empty() && p.first[0] == '*') {
                std::string_view suffix(p.first.c_str() + 1);
                if (prop.length() >= suffix.length() && prop.compare(prop.length() - suffix.length(), suffix.length(), suffix) == 0) {
                    value = p.second.c_str();
                    modified = true;
                    break;
                }
            }
        }
    }
    if (modified) {
        LOGD("prop [%s]: %s -> %s", name, oldValue, value);
    } else if (g_verboseLogs > 2) {
        LOGD("prop [%s]: %s (unchanged)", name, oldValue);
    }
    it->second(cookie, name, value, serial);
}

static void (*o_system_property_read_callback)(const prop_info *, T_Callback, void *);

static void my_system_property_read_callback(const prop_info *pi, T_Callback callback, void *cookie) {
    if (!pi || !callback || !cookie) {
        if (o_system_property_read_callback) o_system_property_read_callback(pi, callback, cookie);
        return;
    }
    callbacks[cookie] = callback;
    o_system_property_read_callback(pi, modify_callback, cookie);
}

static void doHook() {
    void *handle = DobbySymbolResolver(nullptr, "__system_property_read_callback");
    if (!handle) {
        LOGE("hook failed: __system_property_read_callback not found");
        return;
    }
    LOGI("hooked __system_property_read_callback at %p", handle);
    DobbyHook(handle, reinterpret_cast<dobby_dummy_func_t>(my_system_property_read_callback),
              reinterpret_cast<dobby_dummy_func_t *>(&o_system_property_read_callback));
}

static inline std::string trim(const std::string &s) {
    size_t start = s.find_first_not_of(" \t\r\n");
    if (start == std::string::npos) return "";
    size_t end = s.find_last_not_of(" \t\r\n");
    return s.substr(start, end - start + 1);
}

static bool readFileToVector(const char *path, std::vector<char> &out) {
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return false;
    struct stat st;
    if (fstat(fd, &st) < 0) {
        close(fd);
        return false;
    }
    if (st.st_size <= 0) {
        close(fd);
        return false;
    }
    out.resize(st.st_size);
    ssize_t total = 0;
    while (total < st.st_size) {
        ssize_t r = read(fd, out.data() + total, st.st_size - total);
        if (r <= 0) break;
        total += r;
    }
    close(fd);
    if (total != st.st_size) {
        out.clear();
        return false;
    }
    return true;
}

static void parseProps(const std::vector<char> &data, std::unordered_map<std::string, std::string> &props) {
    if (data.empty()) return;
    std::string_view content(data.data(), data.size());
    size_t pos = 0;
    while (pos < content.size()) {
        size_t end = content.find('\n', pos);
        if (end == std::string::npos) end = content.size();
        std::string_view line = content.substr(pos, end - pos);
        pos = end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.empty() || line[0] == '#') continue;
        size_t eq = line.find('=');
        if (eq == std::string::npos || eq == 0) continue;
        std::string name = trim(std::string(line.substr(0, eq)));
        std::string value = trim(std::string(line.substr(eq + 1)));
        size_t hash = value.find('#');
        if (hash != std::string::npos) value = trim(value.substr(0, hash));
        if (!name.empty()) props[name] = value;
    }
}

static void parseAppsList(const std::vector<char> &data, std::unordered_set<std::string> &apps) {
    if (data.empty()) return;
    std::string_view content(data.data(), data.size());
    size_t pos = 0;
    while (pos < content.size()) {
        size_t end = content.find('\n', pos);
        if (end == std::string::npos) end = content.size();
        std::string_view line = content.substr(pos, end - pos);
        pos = end + 1;
        if (!line.empty() && line.back() == '\r') line.remove_suffix(1);
        if (line.empty() || line[0] == '#') continue;
        std::string pkg = trim(std::string(line));
        if (!pkg.empty()) {
            apps.insert(pkg);
            LOGI("app list: %s", pkg.c_str());
        }
    }
}

static int safeParseInt(const std::string& value, int defaultValue) {
    if (value.empty()) return defaultValue;
    size_t pos = 0;
    if (pos < value.length() && (value[pos] == '-' || value[pos] == '+')) pos++;
    if (pos >= value.length()) return defaultValue;
    for (; pos < value.length(); pos++) {
        if (value[pos] < '0' || value[pos] > '9') return defaultValue;
    }
    long result = 0;
    bool negative = false;
    pos = 0;
    if (value[pos] == '-') { negative = true; pos++; }
    else if (value[pos] == '+') pos++;
    for (; pos < value.length(); pos++) {
        result = result * 10 + (value[pos] - '0');
        if (result > INT_MAX) return negative ? INT_MIN : INT_MAX;
    }
    return negative ? -static_cast<int>(result) : static_cast<int>(result);
}

class PlayIntegrityFix : public zygisk::ModuleBase {
public:
    void onLoad(zygisk::Api *api, JNIEnv *env) override {
        this->api = api;
        this->env = env;
    }

    void preAppSpecialize(zygisk::AppSpecializeArgs *args) override {
        if (!args || !args->nice_name || !args->app_data_dir) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        const char *rawProcess = env->GetStringUTFChars(args->nice_name, nullptr);
        const char *rawDir = env->GetStringUTFChars(args->app_data_dir, nullptr);
        if (!rawProcess || !rawDir) {
            if (rawProcess) env->ReleaseStringUTFChars(args->nice_name, rawProcess);
            if (rawDir) env->ReleaseStringUTFChars(args->app_data_dir, rawDir);
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        pkgName = rawProcess;
        std::string_view dir(rawDir);
        bool isGms = dir.ends_with("/com.google.android.gms") || dir.ends_with("/com.android.vending");
        bool isDroidGuardOrVending = (pkgName == DROIDGUARD_PACKAGE) || (pkgName == VENDING_PACKAGE);
        env->ReleaseStringUTFChars(args->nice_name, rawProcess);
        env->ReleaseStringUTFChars(args->app_data_dir, rawDir);

        bool isSpoofApp = false;
        if (!appsListLoaded) {
            std::vector<char> configData;
            if (readFileToVector(CUSTOM_PROP_FILE_PATH, configData)) {
                std::unordered_map<std::string, std::string> earlyProps;
                parseProps(configData, earlyProps);
                auto vit = earlyProps.find("verboseLogs");
                if (vit != earlyProps.end() && !vit->second.empty()) {
                    g_verboseLogs = safeParseInt(vit->second, 0);
                }
                auto it = earlyProps.find("spoofApps");
                if (it != earlyProps.end() && !it->second.empty()) {
                    int appsFlag = safeParseInt(it->second, 0);
                    if (appsFlag > 0) {
                        std::vector<char> appsData;
                        if (readFileToVector(APPS_LIST_FILE_PATH, appsData)) {
                            parseAppsList(appsData, spoofAppPackages);
                        } else {
                            LOGE("apps.txt not found");
                        }
                    }
                }
                earlyProps.clear();
            }
            appsListLoaded = true;
        }
        isSpoofApp = spoofAppPackages.count(pkgName) > 0;

        LOGI("preAppSpecialize pkg=%s gms=%d dgv=%d spoofApp=%d apps=%zu",
             pkgName.c_str(), isGms, isDroidGuardOrVending, isSpoofApp, spoofAppPackages.size());

        if (!isGms && !isSpoofApp) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        api->setOption(zygisk::FORCE_DENYLIST_UNMOUNT);
        if (isGms && !isDroidGuardOrVending && !isSpoofApp) {
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }

        long dexSize = 0, configSize = 0;
        int fd = api->connectCompanion();
        if (fd < 0) {
            LOGE("companion connect failed");
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        if (read(fd, &dexSize, sizeof(long)) != sizeof(long) ||
            read(fd, &configSize, sizeof(long)) != sizeof(long)) {
            LOGE("companion read sizes failed");
            close(fd);
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        if (dexSize < 1) {
            close(fd);
            LOGE("invalid dex size");
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        LOGI("companion dex=%ld config=%ld", dexSize, configSize);
        dexVector.resize(dexSize);
        if (read(fd, dexVector.data(), dexSize) != dexSize) {
            close(fd);
            LOGE("companion read dex failed");
            api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
            return;
        }
        if (configSize > 0) {
            std::vector<char> configVector;
            configVector.resize(configSize);
            if (read(fd, configVector.data(), configSize) != configSize) {
                close(fd);
                LOGE("companion read config failed");
                api->setOption(zygisk::DLCLOSE_MODULE_LIBRARY);
                return;
            }
            parseProps(configVector, propMap);
        }
        close(fd);
        isTargetApp = isDroidGuardOrVending || isSpoofApp;
    }

    void postAppSpecialize(const zygisk::AppSpecializeArgs *args) override {
        if (dexVector.empty()) return;
        readConfig();
        bool isVending = (pkgName == VENDING_PACKAGE);
        bool isDroidGuard = (pkgName == DROIDGUARD_PACKAGE);
        bool isPerApp = spoofAppPackages.count(pkgName) > 0;
        LOGI("postAppSpecialize pkg=%s vending=%d droidguard=%d perapp=%d",
             pkgName.c_str(), isVending, isDroidGuard, isPerApp);
        if (isVending) {
            spoofBuild = spoofProps = spoofProvider = spoofSignature = 0;
        } else if (isDroidGuard) {
            spoofVendingFinger = spoofVendingSdk = spoofPixel = 0;
        } else if (isPerApp) {
            spoofProvider = spoofSignature = spoofVendingFinger = spoofVendingSdk = 0;
        }
        if (spoofProps > 0 && (isDroidGuard || isPerApp)) {
            doHook();
        }
        int totalSpoofs = spoofBuild + spoofProvider + spoofSignature + spoofVendingFinger + spoofVendingSdk + spoofPixel;
        LOGI("spoof flags: build=%d props=%d provider=%d signature=%d vfinger=%d vsdk=%d pixel=%d total=%d",
             spoofBuild, spoofProps, spoofProvider, spoofSignature, spoofVendingFinger, spoofVendingSdk, spoofPixel, totalSpoofs);
        if (totalSpoofs > 0 || isPerApp) {
            inject();
        }
        dexVector.clear();
        dexVector.shrink_to_fit();
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
    bool isTargetApp = false;
    static bool appsListLoaded;

    std::string getPropValue(const std::string& key, const std::string& defaultValue = "") {
        auto it = propMap.find(key);
        return (it != propMap.end() && !it->second.empty()) ? it->second : defaultValue;
    }

    int getPropInt(const std::string& key, int defaultValue = 0) {
        return safeParseInt(getPropValue(key), defaultValue);
    }

    void readConfig() {
        LOGI("config has %zu keys", propMap.size());
        g_verboseLogs = getPropInt("verboseLogs");
        spoofVendingSdk = getPropInt("spoofVendingSdk");
        spoofApps = getPropInt("spoofApps");
        std::string spoofVendingFingerValue = getPropValue("spoofVendingFinger");
        if (!spoofVendingFingerValue.empty()) {
            if (spoofVendingFingerValue.find_first_not_of("01") != std::string::npos) {
                spoofVendingFinger = 1;
                vendingFingerprintValue = spoofVendingFingerValue;
            } else {
                spoofVendingFinger = safeParseInt(spoofVendingFingerValue, 0);
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
        for (const auto& pair : propMap) {
            if (pair.first.find_first_of("*.") != std::string::npos) {
                if (!pair.second.empty()) jsonProps[pair.first] = pair.second;
            }
        }
    }

    std::string buildJsonString() {
        std::string jsonStr = "{";
        bool first = true;
        for (const auto& pair : propMap) {
            if (pair.first.find_first_of("*.") != std::string::npos) continue;
            if (!first) jsonStr += ",";
            first = false;
            std::string escaped;
            escaped.reserve(pair.second.length() * 2);
            for (char c : pair.second) {
                if (c == '\\' || c == '"') escaped += '\\';
                escaped += c;
            }
            jsonStr += "\"" + pair.first + "\":\"" + escaped + "\"";
        }
        jsonStr += "}";
        return jsonStr;
    }

    void inject() {
        bool isVending = (pkgName == VENDING_PACKAGE);
        const char* niceName = isVending ? "PS" : (pkgName == DROIDGUARD_PACKAGE ? "DG" : "APP");
        LOGI("jni %s: get system classloader", niceName);
        jclass clClass = env->FindClass("java/lang/ClassLoader");
        if (!clClass) { LOGE("ClassLoader not found"); return; }
        jmethodID getSystemClassLoader = env->GetStaticMethodID(clClass, "getSystemClassLoader", "()Ljava/lang/ClassLoader;");
        jobject systemClassLoader = env->CallStaticObjectMethod(clClass, getSystemClassLoader);
        LOGI("jni %s: create dex classloader", niceName);
        jclass dexClClass = env->FindClass("dalvik/system/InMemoryDexClassLoader");
        if (!dexClClass) {
            LOGE("InMemoryDexClassLoader not found");
            env->DeleteLocalRef(clClass);
            if (systemClassLoader) env->DeleteLocalRef(systemClassLoader);
            return;
        }
        jmethodID dexClInit = env->GetMethodID(dexClClass, "<init>", "(Ljava/nio/ByteBuffer;Ljava/lang/ClassLoader;)V");
        jobject buffer = env->NewDirectByteBuffer(dexVector.data(), static_cast<jlong>(dexVector.size()));
        jobject dexCl = env->NewObject(dexClClass, dexClInit, buffer, systemClassLoader);
        LOGI("jni %s: load entry class", niceName);
        jmethodID loadClass = env->GetMethodID(clClass, "loadClass", "(Ljava/lang/String;)Ljava/lang/Class;");
        const char* className = isVending ? "es.chiteroman.playintegrityfix.EntryPointVending" : "es.chiteroman.playintegrityfix.EntryPoint";
        jstring entryClassName = env->NewStringUTF(className);
        jobject entryClassObj = env->CallObjectMethod(dexCl, loadClass, entryClassName);
        jclass entryClass = static_cast<jclass>(entryClassObj);
        if (!entryClass) {
            LOGE("entry class not found");
            cleanupRefs(clClass, systemClassLoader, dexClClass, buffer, dexCl, entryClassName, entryClassObj);
            return;
        }
        if (isVending) {
            LOGI("jni %s: call EntryPointVending.init", niceName);
            jmethodID entryInit = env->GetStaticMethodID(entryClass, "init", "(IIILjava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;Ljava/lang/String;)V");
            jstring javaFinger = env->NewStringUTF(vendingFingerprintValue.c_str());
            jstring javaManuf = env->NewStringUTF(pixelManufacturer.c_str());
            jstring javaModel = env->NewStringUTF(pixelModel.c_str());
            jstring javaDevice = env->NewStringUTF(pixelDevice.c_str());
            jstring javaBrand = env->NewStringUTF(pixelBrand.c_str());
            env->CallStaticVoidMethod(entryClass, entryInit, g_verboseLogs, spoofVendingFinger, spoofVendingSdk, javaFinger, javaManuf, javaModel, javaDevice, javaBrand);
            env->DeleteLocalRef(javaFinger);
            env->DeleteLocalRef(javaManuf);
            env->DeleteLocalRef(javaModel);
            env->DeleteLocalRef(javaDevice);
            env->DeleteLocalRef(javaBrand);
        } else {
            LOGI("jni %s: send json", niceName);
            jmethodID receiveJson = env->GetStaticMethodID(entryClass, "receiveJson", "(Ljava/lang/String;)V");
            std::string jsonStr = buildJsonString();
            jstring javaStr = env->NewStringUTF(jsonStr.c_str());
            env->CallStaticVoidMethod(entryClass, receiveJson, javaStr);
            env->DeleteLocalRef(javaStr);
            LOGI("jni %s: call EntryPoint.init", niceName);
            jmethodID entryInit = env->GetStaticMethodID(entryClass, "init", "(IIII)V");
            env->CallStaticVoidMethod(entryClass, entryInit, g_verboseLogs, spoofBuild, spoofProvider, spoofSignature);
        }
        cleanupRefs(clClass, systemClassLoader, dexClClass, buffer, dexCl, entryClassName, entryClassObj);
    }

    void cleanupRefs(jobject clClass, jobject systemClassLoader, jobject dexClClass, jobject buffer, jobject dexCl, jobject entryClassName, jobject entryClassObj) {
        if (clClass) env->DeleteLocalRef(clClass);
        if (systemClassLoader) env->DeleteLocalRef(systemClassLoader);
        if (dexClClass) env->DeleteLocalRef(dexClClass);
        if (buffer) env->DeleteLocalRef(buffer);
        if (dexCl) env->DeleteLocalRef(dexCl);
        if (entryClassName) env->DeleteLocalRef(entryClassName);
        if (entryClassObj) env->DeleteLocalRef(entryClassObj);
    }
};

bool PlayIntegrityFix::appsListLoaded = false;

static void companion(int fd) {
    long dexSize = 0, configSize = 0;
    std::vector<char> dexVector, configVector;
    int dexFd = open(DEX_FILE_PATH, O_RDONLY | O_CLOEXEC);
    if (dexFd >= 0) {
        struct stat st;
        if (fstat(dexFd, &st) == 0 && st.st_size > 0) {
            dexSize = st.st_size;
            dexVector.resize(dexSize);
            ssize_t total = 0;
            while (total < dexSize) {
                ssize_t r = read(dexFd, dexVector.data() + total, dexSize - total);
                if (r <= 0) break;
                total += r;
            }
            if (total != dexSize) dexSize = 0;
        }
        close(dexFd);
    }
    int configFd = open(CUSTOM_PROP_FILE_PATH, O_RDONLY | O_CLOEXEC);
    if (configFd >= 0) {
        struct stat st;
        if (fstat(configFd, &st) == 0 && st.st_size > 0) {
            configSize = st.st_size;
            configVector.resize(configSize);
            ssize_t total = 0;
            while (total < configSize) {
                ssize_t r = read(configFd, configVector.data() + total, configSize - total);
                if (r <= 0) break;
                total += r;
            }
            if (total != configSize) configSize = 0;
        }
        close(configFd);
    }
    write(fd, &dexSize, sizeof(long));
    write(fd, &configSize, sizeof(long));
    if (dexSize > 0) write(fd, dexVector.data(), dexSize);
    if (configSize > 0) write(fd, configVector.data(), configSize);
}

REGISTER_ZYGISK_MODULE(PlayIntegrityFix)
REGISTER_ZYGISK_COMPANION(companion)

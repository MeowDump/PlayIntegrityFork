package es.chiteroman.playintegrityfix;

import android.content.pm.PackageInfo;
import android.content.pm.PackageManager;
import android.content.pm.Signature;
import android.os.Build;
import android.os.Parcel;
import android.os.Parcelable;
import android.util.Base64;
import android.util.JsonReader;
import android.util.Log;
import org.lsposed.hiddenapibypass.HiddenApiBypass;
import java.io.IOException;
import java.io.StringReader;
import java.lang.reflect.Field;
import java.lang.reflect.Method;
import java.security.KeyStore;
import java.security.KeyStoreSpi;
import java.security.Provider;
import java.security.Security;
import java.util.HashMap;
import java.util.Map;

public final class EntryPoint {
    private static int verboseLogs = 0;
    private static int spoofBuildEnabled = 1;
    private static final String TAG = "PIF/Java:DG";

    private static final String signatureData = "MIIFyTCCA7GgAwIBAgIVALyxxl+zDS9SL68SzOr48309eAZyMA0GCSqGSIb3DQEBCwUAMHQxCzAJ\n" +
            "BgNVBAYTAlVTMRMwEQYDVQQIEwpDYWxpZm9ybmlhMRYwFAYDVQQHEw1Nb3VudGFpbiBWaWV3MRQw\n" +
            "EgYDVQQKEwtHb29nbGUgSW5jLjEQMA4GA1UECxMHQW5kcm9pZDEQMA4GA1UEAxMHQW5kcm9pZDAg\n" +
            "Fw0yMjExMDExODExMzVaGA8yMDUyMTEwMTE4MTEzNVowdDELMAkGA1UEBhMCVVMxEzARBgNVBAgT\n" +
            "CkNhbGlmb3JuaWExFjAUBgNVBAcTDU1vdW50YWluIFZpZXcxFDASBgNVBAoTC0dvb2dsZSBJbmMu\n" +
            "MRAwDgYDVQQLEwdBbmRyb2lkMRAwDgYDVQQDEwdBbmRyb2lkMIICIjANBgkqhkiG9w0BAQEFAAOC\n" +
            "Ag8AMIICCgKCAgEAsqtalIy/nctKlrhd1UVoDffFGnDf9GLi0QQhsVoJkfF16vDDydZJOycG7/kQ\n" +
            "ziRZhFdcoMrIYZzzw0ppBjsSe1AiWMuKXwTBaEtxN99S1xsJiW4/QMI6N6kMunydWRMsbJ6aAxi1\n" +
            "lVq0bxSwr8Sg/8u9HGVivfdG8OpUM+qjuV5gey5xttNLK3BZDrAlco8RkJZryAD40flmJZrWXJmc\n" +
            "r2HhJJUnqG4Z3MSziEgW1u1JnnY3f/BFdgYsA54SgdUGdQP3aqzSjIpGK01/vjrXvifHazSANjvl\n" +
            "0AUE5i6AarMw2biEKB2ySUDp8idC5w12GpqDrhZ/QkW8yBSa87KbkMYXuRA2Gq1fYbQx3YJraw0U\n" +
            "gZ4M3fFKpt6raxxM5j0sWHlULD7dAZMERvNESVrKG3tQ7B39WAD8QLGYc45DFEGOhKv5Fv8510h5\n" +
            "sXK502IvGpI4FDwz2rbtAgJ0j+16db5wCSW5ThvNPhCheyciajc8dU1B5tJzZN/ksBpzne4Xf9gO\n" +
            "LZ9ZU0+3Z5gHVvTS/YpxBFwiFpmL7dvGxew0cXGSsG5UTBlgr7i0SX0WhY4Djjo8IfPwrvvA0QaC\n" +
            "FamdYXKqBsSHgEyXS9zgGIFPt2jWdhaS+sAa//5SXcWro0OdiKPuwEzLgj759ke1sHRnvO735dYn\n" +
            "5whVbzlGyLBh3L0CAwEAAaNQME4wDAYDVR0TBAUwAwEB/zAdBgNVHQ4EFgQUU1eXQ7NoYKjvOQlh\n" +
            "5V8jHQMoxA8wHwYDVR0jBBgwFoAUU1eXQ7NoYKjvOQlh5V8jHQMoxA8wDQYJKoZIhvcNAQELBQAD\n" +
            "ggIBAHFIazRLs3itnZKllPnboSd6sHbzeJURKehx8GJPvIC+xWlwWyFO5+GHmgc3yh/SVd3Xja/k\n" +
            "8Ud59WEYTjyJJWTw0Jygx37rHW7VGn2HDuy/x0D+els+S8HeLD1toPFMepjIXJn7nHLhtmzTPlDW\n" +
            "DrhiaYsls/k5Izf89xYnI4euuOY2+1gsweJqFGfbznqyqy8xLyzoZ6bvBJtgeY+G3i/9Be14HseS\n" +
            "Na4FvI1Oze/l2gUu1IXzN6DGWR/lxEyt+TncJfBGKbjafYrfSh3zsE4N3TU7BeOL5INirOMjre/j\n" +
            "VgB1YQG5qLVaPoz6mdn75AbBBm5a5ahApLiKqzy/hP+1rWgw8Ikb7vbUqov/bnY3IlIU6XcPJTCD\n" +
            "b9aRZQkStvYpQd82XTyxD/T0GgRLnUj5Uv6iZlikFx1KNj0YNS2T3gyvL++J9B0Y6gAkiG0EtNpl\n" +
            "z7Pomsv5pVdmHVdKMjqWw5/6zYzVmu5cXFtR384Ti1qwML1xkD6TC3VIv88rKIEjrkY2c+v1frh9\n" +
            "fRJ2OmzXmML9NgHTjEiJR2Ib2iNrMKxkuTIs9oxKZgrJtJKvdU9qJJKM5PnZuNuHhGs6A/9gt9Oc\n" +
            "cetYeQvVSqeEmQluWfcunQn9C9Vwi2BJIiVJh4IdWZf5/e2PlSSQ9CJjz2bKI17pzdxOmjQfE0JS\n" +
            "F7Xt\n";

    private static final Map<String, String> map = new HashMap<>();

    public static int getVerboseLogs() { return verboseLogs; }
    public static int getSpoofBuildEnabled() { return spoofBuildEnabled; }

    public static void init(int logLevel, int spoofBuildVal, int spoofProviderVal, int spoofSignatureVal) {
        verboseLogs = logLevel;
        spoofBuildEnabled = spoofBuildVal;
        LOGI("init verbose=" + logLevel + " build=" + spoofBuildVal + " provider=" + spoofProviderVal + " signature=" + spoofSignatureVal);
        if (verboseLogs > 99) logFields();
        if (spoofProviderVal > 0) spoofProvider();
        if (spoofBuildVal > 0) spoofDevice();
        if (spoofSignatureVal > 0) spoofPackageManager();
    }

    public static void receiveJson(String data) {
        LOGI("receiveJson len=" + (data != null ? data.length() : 0));
        if (data == null || data.isEmpty()) {
            LOGE("json empty");
            return;
        }
        map.clear();
        try (JsonReader reader = new JsonReader(new StringReader(data))) {
            reader.beginObject();
            while (reader.hasNext()) {
                String name = reader.nextName();
                String value = reader.nextString();
                map.put(name, value);
                if (verboseLogs > 2) LOG("json " + name + "=" + value);
            }
            reader.endObject();
            LOGI("parsed " + map.size() + " fields");
        } catch (Exception e) {
            LOGE("json parse failed: " + e.getMessage());
            map.clear();
        }
    }

    private static void spoofProvider() {
        LOGI("spoof provider");
        try {
            Provider provider = Security.getProvider("AndroidKeyStore");
            if (provider == null) { LOGE("provider not found"); return; }
            KeyStore keyStore = KeyStore.getInstance("AndroidKeyStore");
            Field f = keyStore.getClass().getDeclaredField("keyStoreSpi");
            f.setAccessible(true);
            CustomKeyStoreSpi.keyStoreSpi = (KeyStoreSpi) f.get(keyStore);
            f.setAccessible(false);
            CustomProvider customProvider = new CustomProvider(provider);
            Security.removeProvider("AndroidKeyStore");
            Security.insertProviderAt(customProvider, 1);
            LOGI("provider spoofed");
        } catch (Exception e) {
            LOGE("provider failed: " + e.getMessage());
        }
    }

    static void spoofDevice() {
        LOGI("spoof " + map.size() + " fields");
        for (Map.Entry<String, String> entry : map.entrySet()) {
            setField(entry.getKey(), entry.getValue());
        }
    }

    private static void spoofPackageManager() {
        LOGI("spoof signatures");
        Signature spoofedSignature = new Signature(Base64.decode(signatureData, Base64.DEFAULT));
        Parcelable.Creator<PackageInfo> customCreator = new CustomPackageInfoCreator(PackageInfo.CREATOR, spoofedSignature);
        try {
            Field creatorField = findField(PackageInfo.class, "CREATOR");
            creatorField.setAccessible(true);
            creatorField.set(null, customCreator);
            LOGI("creator replaced");
        } catch (Exception e) {
            LOGE("creator failed: " + e.getMessage());
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            try {
                HiddenApiBypass.addHiddenApiExemptions("Landroid/os/Parcel;", "Landroid/content/pm", "Landroid/app");
                LOGI("hidden api ok");
            } catch (Exception e) {
                LOGE("hidden api failed: " + e.getMessage());
            }
        }
        clearCaches();
    }

    private static void clearCaches() {
        try {
            Field cacheField = findField(PackageManager.class, "sPackageInfoCache");
            cacheField.setAccessible(true);
            Object cache = cacheField.get(null);
            if (cache != null) {
                cache.getClass().getMethod("clear").invoke(cache);
                LOGI("cache cleared");
            }
        } catch (Exception e) {
            if (verboseLogs > 1) LOG("cache clear: " + e.getMessage());
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.Q) {
            clearParcelField("mCreators");
            clearParcelField("sPairedCreators");
        }
    }

    private static void clearParcelField(String fieldName) {
        try {
            Field creatorsField = findField(Parcel.class, fieldName);
            creatorsField.setAccessible(true);
            Map<?, ?> creators = (Map<?, ?>) creatorsField.get(null);
            if (creators != null) {
                creators.clear();
                LOGI("parcel " + fieldName + " cleared");
            }
        } catch (Exception e) {
            if (verboseLogs > 1) LOG("parcel " + fieldName + ": " + e.getMessage());
        }
    }

    private static Field findField(Class<?> currentClass, String fieldName) throws NoSuchFieldException {
        while (currentClass != null && !currentClass.equals(Object.class)) {
            try { return currentClass.getDeclaredField(fieldName); }
            catch (NoSuchFieldException e) { currentClass = currentClass.getSuperclass(); }
        }
        throw new NoSuchFieldException("Field '" + fieldName + "' not found");
    }

    private static boolean classContainsField(Class<?> className, String fieldName) {
        for (Field field : className.getDeclaredFields()) {
            if (field.getName().equals(fieldName)) return true;
        }
        return false;
    }

    private static void setField(String name, String value) {
        if (value == null || value.isEmpty()) {
            if (verboseLogs > 1) LOG(name + " empty skip");
            return;
        }
        Field field = null;
        try {
            if (classContainsField(Build.class, name)) {
                field = Build.class.getDeclaredField(name);
            } else if (classContainsField(Build.VERSION.class, name)) {
                field = Build.VERSION.class.getDeclaredField(name);
            } else {
                if (verboseLogs > 1) LOG(name + " class unknown");
                return;
            }
        } catch (NoSuchFieldException e) {
            LOGE(name + " not found: " + e.getMessage());
            return;
        }
        field.setAccessible(true);
        String oldValue;
        try { oldValue = String.valueOf(field.get(null)); }
        catch (IllegalAccessException e) {
            LOGE(name + " access: " + e.getMessage());
            return;
        }
        if (value.equals(oldValue)) {
            if (verboseLogs > 2) LOG(name + " " + value + " unchanged");
            return;
        }
        Class<?> fieldType = field.getType();
        Object newValue;
        try {
            if (fieldType == String.class) newValue = value;
            else if (fieldType == int.class || fieldType == Integer.class) newValue = Integer.parseInt(value);
            else if (fieldType == long.class || fieldType == Long.class) newValue = Long.parseLong(value);
            else if (fieldType == boolean.class || fieldType == Boolean.class) newValue = Boolean.parseBoolean(value);
            else {
                LOGE(name + " type " + fieldType.getName() + " unsupported");
                return;
            }
            field.set(null, newValue);
            LOG(name + " " + oldValue + " -> " + value);
        } catch (NumberFormatException e) {
            LOGE(name + " parse " + value + ": " + e.getMessage());
        } catch (IllegalAccessException e) {
            LOGE(name + " set: " + e.getMessage());
        }
    }

    private static String logParseField(Field field) {
        Object value = null;
        String type = field.getType().getName();
        String name = field.getName();
        try { value = field.get(null); }
        catch (Exception e) { return name + " access: " + e.getMessage(); }
        return type + " " + name + ": " + String.valueOf(value);
    }

    private static void logFields() {
        LOGI("=== Build ===");
        for (Field field : Build.class.getDeclaredFields()) {
            field.setAccessible(true);
            LOG("Build " + logParseField(field));
            field.setAccessible(false);
        }
        LOGI("=== Build.VERSION ===");
        for (Field field : Build.VERSION.class.getDeclaredFields()) {
            field.setAccessible(true);
            LOG("Build.VERSION " + logParseField(field));
            field.setAccessible(false);
        }
    }

    static void LOG(String msg) { if (verboseLogs > 0) Log.d(TAG, msg); }
    static void LOGI(String msg) { if (verboseLogs > 0) Log.i(TAG, msg); }
    static void LOGE(String msg) { if (verboseLogs > 0) Log.e(TAG, msg); }
}

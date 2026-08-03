package es.chiteroman.playintegrityfix;

import android.os.Build;
import android.util.JsonReader;
import android.util.Log;

import org.lsposed.hiddenapibypass.HiddenApiBypass;

import java.io.IOException;
import java.io.StringReader;
import java.lang.reflect.Field;
import java.util.HashMap;
import java.util.Map;

public final class EntryPointApps {
    private static Integer verboseLogs = 0;
    private static final Map<String, String> map = new HashMap<>();

    public static void init(int logLevel, String data) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            HiddenApiBypass.addHiddenApiExemptions("");
        }
        verboseLogs = logLevel;
        receiveJson(data);
        if (verboseLogs > 99) logFields();
        spoofDevice();
    }

    public static void receiveJson(String data) {
        try (JsonReader reader = new JsonReader(new StringReader(data))) {
            reader.beginObject();
            while (reader.hasNext()) {
                String key = reader.nextName();
                if (key.equals("verboseLogs") || key.equals("spoofBuild") || key.equals("spoofProps")
                        || key.equals("spoofProvider") || key.equals("spoofSignature")
                        || key.equals("spoofVendingFinger") || key.equals("spoofVendingSdk")
                        || key.equals("spoofPixel") || key.equals("spoofApps")) {
                    reader.skipValue();
                    continue;
                }
                map.put(key, reader.nextString());
            }
            reader.endObject();
        } catch (IOException | IllegalStateException e) {
            LOG("Couldn't read JSON: " + e);
            map.clear();
        }
    }

    static void spoofDevice() {
        for (String key : map.keySet()) {
            setField(key, map.get(key));
        }
    }

    private static void setField(String name, String value) {
        if (value == null || value.isEmpty()) {
            if (verboseLogs > 1) LOG(name + " is empty, skipping");
            return;
        }

        Field field = null;
        String oldValue = null;
        Object newValue = null;

        try {
            if (classContainsField(Build.class, name)) {
                field = Build.class.getDeclaredField(name);
            } else if (classContainsField(Build.VERSION.class, name)) {
                field = Build.VERSION.class.getDeclaredField(name);
            } else {
                if (verboseLogs > 1) LOG("Couldn't determine '" + name + "' class name");
                return;
            }
        } catch (NoSuchFieldException e) {
            LOG("Couldn't find '" + name + "' field: " + e);
            return;
        }

        field.setAccessible(true);
        try {
            Field accessFlagsField = Field.class.getDeclaredField("accessFlags");
            accessFlagsField.setAccessible(true);
            accessFlagsField.setInt(field, field.getModifiers() & ~java.lang.reflect.Modifier.FINAL);
        } catch (NoSuchFieldException | IllegalAccessException e) {
            LOG("Couldn't modify accessFlags for '" + name + "': " + e);
        }

        try {
            oldValue = String.valueOf(field.get(null));
        } catch (IllegalAccessException e) {
            LOG("Couldn't access '" + name + "' value: " + e);
            return;
        }

        if (value.equals(oldValue)) {
            if (verboseLogs > 2) LOG("[" + name + "]: " + value + " (unchanged)");
            return;
        }

        Class<?> fieldType = field.getType();
        if (fieldType == String.class) {
            newValue = value;
        } else if (fieldType == int.class) {
            newValue = Integer.parseInt(value);
        } else if (fieldType == long.class) {
            newValue = Long.parseLong(value);
        } else if (fieldType == boolean.class) {
            newValue = Boolean.parseBoolean(value);
        } else {
            LOG("Couldn't convert '" + value + "' to '" + fieldType.getName() + "'");
            return;
        }

        try {
            setFieldNative(field.getDeclaringClass(), field, fieldType.getName(), newValue);
        } catch (Exception e) {
            LOG("Native setField failed for '" + name + "': " + e);
        }

        LOG("[" + name + "]: " + oldValue + " -> " + value);
    }

    private static boolean classContainsField(Class<?> className, String fieldName) {
        for (Field field : className.getDeclaredFields()) {
            if (field.getName().equals(fieldName)) return true;
        }
        return false;
    }

    private static native void setFieldNative(Class<?> targetClass, Field field, String type, Object value);

    private static String logParseField(Field field) {
        Object value = null;
        String type = field.getType().getName();
        String name = field.getName();
        try {
            value = field.get(null);
        } catch (IllegalAccessException | NullPointerException e) {
            return "Couldn't access '" + name + "' value: " + e;
        }
        return "<" + type + "> " + name + ": " + String.valueOf(value);
    }

    private static void logFields() {
        for (Field field : Build.class.getDeclaredFields()) {
            LOG("Build " + logParseField(field));
        }
        for (Field field : Build.VERSION.class.getDeclaredFields()) {
            LOG("Build.VERSION " + logParseField(field));
        }
    }

    static void LOG(String msg) {
        Log.d("PIF/Java:APPS", msg);
    }
}

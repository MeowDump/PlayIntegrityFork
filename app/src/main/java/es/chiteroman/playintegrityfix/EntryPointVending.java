package es.chiteroman.playintegrityfix;

import android.annotation.SuppressLint;
import android.os.Build;
import java.lang.reflect.Field;
import android.util.Log;
import org.lsposed.hiddenapibypass.HiddenApiBypass;

public final class EntryPointVending {

    private static void LOG(String msg) {
        Log.d("PIF/Java:PS", msg);
    }

    @SuppressLint("DefaultLocale")
    public static void init(int verboseLogs, int spoofVendingFinger, int spoofVendingSdk, String vendingFingerprintValue, int spoofPixel, String brandValue, String deviceValue, String modelValue) {
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P) {
            HiddenApiBypass.addHiddenApiExemptions("");
        }
        
        // Spoof Pixel fields (BRAND, DEVICE, MODEL) if enabled
        if (spoofPixel > 0) {
            if (brandValue != null && !brandValue.isEmpty()) spoofField("BRAND", brandValue, verboseLogs);
            if (deviceValue != null && !deviceValue.isEmpty()) spoofField("DEVICE", deviceValue, verboseLogs);
            if (modelValue != null && !modelValue.isEmpty()) spoofField("MODEL", modelValue, verboseLogs);
        }
        
        // Only spoof FINGERPRINT to Play Store if not forcing Android <13 Play Integrity verdict
        if (spoofVendingSdk < 1) {
            if (spoofVendingFinger < 1) return;
            spoofField("FINGERPRINT", vendingFingerprintValue, verboseLogs);
        } else {
            int requestSdk = spoofVendingSdk == 1 ? 32 : spoofVendingSdk;
            int targetSdk = Math.min(Build.VERSION.SDK_INT, requestSdk);
            Field field = null;
            try {
                field = Build.VERSION.class.getDeclaredField("SDK_INT");
                field.setAccessible(true);
                try {
                    Field accessFlagsField = Field.class.getDeclaredField("accessFlags");
                    accessFlagsField.setAccessible(true);
                    accessFlagsField.setInt(field, field.getModifiers() & ~java.lang.reflect.Modifier.FINAL);
                } catch (NoSuchFieldException | IllegalAccessException e) {
                    LOG("Couldn't modify accessFlags for SDK_INT: " + e);
                }
                int oldValue = field.getInt(null);
                if (oldValue == targetSdk) {
                    if (verboseLogs > 2) LOG(String.format("[SDK_INT]: %d (unchanged)", oldValue));
                    field.setAccessible(false);
                    return;
                }
                try {
                    setFieldNative(field.getDeclaringClass(), field, field.getType().getName(), targetSdk);
                } catch (Exception e) {
                    LOG("Native setField failed for SDK_INT: " + e);
                }
                field.setAccessible(false);
                LOG(String.format("[SDK_INT]: %d -> %d", oldValue, targetSdk));
            } catch (NoSuchFieldException e) {
                LOG("SDK_INT field not found: " + e);
            } catch (SecurityException | IllegalAccessException | IllegalArgumentException |
                     NullPointerException | ExceptionInInitializerError e) {
                LOG("SDK_INT field not accessible: " + e);
            }
        }
    }

    private static void spoofField(String fieldName, String value, int verboseLogs) {
        if (value == null || value.isEmpty()) return;
        Field field = null;
        try {
            field = Build.class.getDeclaredField(fieldName);
            field.setAccessible(true);
            try {
                Field accessFlagsField = Field.class.getDeclaredField("accessFlags");
                accessFlagsField.setAccessible(true);
                accessFlagsField.setInt(field, field.getModifiers() & ~java.lang.reflect.Modifier.FINAL);
            } catch (NoSuchFieldException | IllegalAccessException e) {
                LOG("Couldn't modify accessFlags for " + fieldName + ": " + e);
            }
            String oldValue = String.valueOf(field.get(null));
            if (oldValue.equals(value)) {
                if (verboseLogs > 2) LOG(String.format("[%s]: %s (unchanged)", fieldName, oldValue));
                field.setAccessible(false);
                return;
            }
            try {
                setFieldNative(field.getDeclaringClass(), field, field.getType().getName(), value);
            } catch (Exception e) {
                LOG("Native setField failed for " + fieldName + ": " + e);
            }
            field.setAccessible(false);
            LOG(String.format("[%s]: %s -> %s", fieldName, oldValue, value));
        } catch (NoSuchFieldException e) {
            LOG(fieldName + " field not found: " + e);
        } catch (SecurityException | IllegalAccessException | IllegalArgumentException |
                 NullPointerException | ExceptionInInitializerError e) {
            LOG(fieldName + " field not accessible: " + e);
        }
    }

    private static native void setFieldNative(Class<?> targetClass, Field field, String type, Object value);
}

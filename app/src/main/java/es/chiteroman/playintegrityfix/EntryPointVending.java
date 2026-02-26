package es.chiteroman.playintegrityfix;

import android.annotation.SuppressLint;
import android.os.Build;
import java.lang.reflect.Field;
import android.util.Log;

public final class EntryPointVending {

    private static void LOG(String msg) {
        Log.d("PIF/Java:PS", msg);
    }

    @SuppressLint("DefaultLocale")
    public static void init(int verboseLogs, int spoofVendingFinger, int spoofVendingSdk, String vendingFingerprintValue, String pixelManufacturer, String pixelModel, String pixelDevice, String pixelBrand) {
        if (spoofVendingSdk > 0) {
            int targetSdk = 29;
            int oldValue;
            try {
                Field field = Build.VERSION.class.getDeclaredField("SDK_INT");
                field.setAccessible(true);
                oldValue = field.getInt(null);
                if (oldValue == targetSdk) {
                    if (verboseLogs > 2) LOG(String.format("[SDK_INT]: %d (unchanged)", oldValue));
                    field.setAccessible(false);
                    return;
                }
                field.set(null, targetSdk);
                field.setAccessible(false);
                LOG(String.format("[SDK_INT]: %d -> %d", oldValue, targetSdk));
            } catch (NoSuchFieldException e) {
                LOG("SDK_INT field not found: " + e);
            } catch (SecurityException | IllegalAccessException | IllegalArgumentException | NullPointerException | ExceptionInInitializerError e) {
                LOG("SDK_INT field not accessible: " + e);
            }
            if (spoofVendingFinger < 1 && (pixelManufacturer.isEmpty() || pixelModel.isEmpty() || pixelDevice.isEmpty())) {
                return;
            }
        }
        if (spoofVendingFinger > 0 && !vendingFingerprintValue.isEmpty()) {
            String oldValue;
            try {
                Field field = Build.class.getDeclaredField("FINGERPRINT");
                field.setAccessible(true);
                oldValue = String.valueOf(field.get(null));
                if (oldValue.equals(vendingFingerprintValue)) {
                    if (verboseLogs > 2) LOG(String.format("[FINGERPRINT]: %s (unchanged)", oldValue));
                    field.setAccessible(false);
                } else {
                    field.set(null, vendingFingerprintValue);
                    field.setAccessible(false);
                    LOG(String.format("[FINGERPRINT]: %s -> %s", oldValue, vendingFingerprintValue));
                }
            } catch (NoSuchFieldException e) {
                LOG("FINGERPRINT field not found: " + e);
            } catch (SecurityException | IllegalAccessException | IllegalArgumentException | NullPointerException | ExceptionInInitializerError e) {
                LOG("FINGERPRINT field not accessible: " + e);
            }
        }
        if (!pixelBrand.isEmpty()) {
            String oldValue;
            try {
                Field field = Build.class.getDeclaredField("BRAND");
                field.setAccessible(true);
                oldValue = String.valueOf(field.get(null));
                if (oldValue.equals(pixelBrand)) {
                    if (verboseLogs > 2) LOG(String.format("[BRAND]: %s (unchanged)", oldValue));
                    field.setAccessible(false);
                } else {
                    field.set(null, pixelBrand);
                    field.setAccessible(false);
                    LOG(String.format("[BRAND]: %s -> %s", oldValue, pixelBrand));
                }
            } catch (NoSuchFieldException e) {
                LOG("BRAND field not found: " + e);
            } catch (SecurityException | IllegalAccessException | IllegalArgumentException | NullPointerException | ExceptionInInitializerError e) {
                LOG("BRAND field not accessible: " + e);
            }
        }
        if (!pixelManufacturer.isEmpty()) {
            String oldValue;
            try {
                Field field = Build.class.getDeclaredField("MANUFACTURER");
                field.setAccessible(true);
                oldValue = String.valueOf(field.get(null));
                if (oldValue.equals(pixelManufacturer)) {
                    if (verboseLogs > 2) LOG(String.format("[MANUFACTURER]: %s (unchanged)", oldValue));
                    field.setAccessible(false);
                } else {
                    field.set(null, pixelManufacturer);
                    field.setAccessible(false);
                    LOG(String.format("[MANUFACTURER]: %s -> %s", oldValue, pixelManufacturer));
                }
            } catch (NoSuchFieldException e) {
                LOG("MANUFACTURER field not found: " + e);
            } catch (SecurityException | IllegalAccessException | IllegalArgumentException | NullPointerException | ExceptionInInitializerError e) {
                LOG("MANUFACTURER field not accessible: " + e);
            }
        }
        if (!pixelModel.isEmpty()) {
            String oldValue;
            try {
                Field field = Build.class.getDeclaredField("MODEL");
                field.setAccessible(true);
                oldValue = String.valueOf(field.get(null));
                if (oldValue.equals(pixelModel)) {
                    if (verboseLogs > 2) LOG(String.format("[MODEL]: %s (unchanged)", oldValue));
                    field.setAccessible(false);
                } else {
                    field.set(null, pixelModel);
                    field.setAccessible(false);
                    LOG(String.format("[MODEL]: %s -> %s", oldValue, pixelModel));
                }
            } catch (NoSuchFieldException e) {
                LOG("MODEL field not found: " + e);
            } catch (SecurityException | IllegalAccessException | IllegalArgumentException | NullPointerException | ExceptionInInitializerError e) {
                LOG("MODEL field not accessible: " + e);
            }
        }
        if (!pixelDevice.isEmpty()) {
            String oldValue;
            try {
                Field field = Build.class.getDeclaredField("DEVICE");
                field.setAccessible(true);
                oldValue = String.valueOf(field.get(null));
                if (oldValue.equals(pixelDevice)) {
                    if (verboseLogs > 2) LOG(String.format("[DEVICE]: %s (unchanged)", oldValue));
                    field.setAccessible(false);
                } else {
                    field.set(null, pixelDevice);
                    field.setAccessible(false);
                    LOG(String.format("[DEVICE]: %s -> %s", oldValue, pixelDevice));
                }
            } catch (NoSuchFieldException e) {
                LOG("DEVICE field not found: " + e);
            } catch (SecurityException | IllegalAccessException | IllegalArgumentException | NullPointerException | ExceptionInInitializerError e) {
                LOG("DEVICE field not accessible: " + e);
            }
        }
    }
}
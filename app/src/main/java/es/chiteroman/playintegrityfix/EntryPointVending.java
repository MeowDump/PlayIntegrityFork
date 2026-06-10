package es.chiteroman.playintegrityfix;

import android.annotation.SuppressLint;
import android.os.Build;
import java.lang.reflect.Field;
import android.util.Log;

public final class EntryPointVending {
    private static int verboseLogs = 0;
    private static final String TAG = "PIF/Java:PS";

    private static void LOG(String msg) {
        if (verboseLogs > 0) Log.d(TAG, msg);
    }

    private static void LOGI(String msg) {
        if (verboseLogs > 0) Log.i(TAG, msg);
    }

    private static void LOGE(String msg) {
        if (verboseLogs > 0) Log.e(TAG, msg);
    }

    @SuppressLint("DefaultLocale")
    public static void init(int verbose, int spoofVendingFinger, int spoofVendingSdk,
                           String vendingFingerprintValue, String pixelManufacturer,
                           String pixelModel, String pixelDevice, String pixelBrand) {
        verboseLogs = verbose;
        LOGI("init verbose=" + verbose + " finger=" + spoofVendingFinger + " sdk=" + spoofVendingSdk);
        if (spoofVendingSdk > 0) {
            spoofSdkInt(29);
            if (spoofVendingFinger < 1 && (isEmpty(pixelManufacturer) || isEmpty(pixelModel) || isEmpty(pixelDevice))) {
                LOG("missing pixel fields");
                return;
            }
        }
        if (spoofVendingFinger > 0 && !isEmpty(vendingFingerprintValue)) {
            setBuildField("FINGERPRINT", vendingFingerprintValue);
        }
        setBuildField("BRAND", pixelBrand);
        setBuildField("MANUFACTURER", pixelManufacturer);
        setBuildField("MODEL", pixelModel);
        setBuildField("DEVICE", pixelDevice);
    }

    private static void spoofSdkInt(int targetSdk) {
        try {
            Field field = Build.VERSION.class.getDeclaredField("SDK_INT");
            field.setAccessible(true);
            int oldValue = field.getInt(null);
            if (oldValue == targetSdk) {
                LOG(String.format("SDK_INT %d unchanged", oldValue));
                field.setAccessible(false);
                return;
            }
            field.set(null, targetSdk);
            field.setAccessible(false);
            LOG(String.format("SDK_INT %d -> %d", oldValue, targetSdk));
        } catch (Exception e) {
            LOGE("SDK_INT failed: " + e.getMessage());
        }
    }

    private static void setBuildField(String fieldName, String newValue) {
        if (isEmpty(newValue)) {
            LOG(fieldName + " empty skip");
            return;
        }
        try {
            Field field = Build.class.getDeclaredField(fieldName);
            field.setAccessible(true);
            String oldValue = String.valueOf(field.get(null));
            if (oldValue.equals(newValue)) {
                if (verboseLogs > 2) LOG(String.format("%s %s unchanged", fieldName, oldValue));
                field.setAccessible(false);
                return;
            }
            field.set(null, newValue);
            field.setAccessible(false);
            LOG(String.format("%s %s -> %s", fieldName, oldValue, newValue));
        } catch (Exception e) {
            LOGE(fieldName + " failed: " + e.getMessage());
        }
    }

    private static boolean isEmpty(String str) {
        return str == null || str.isEmpty();
    }
}

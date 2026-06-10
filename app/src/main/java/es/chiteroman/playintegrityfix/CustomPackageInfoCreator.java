package es.chiteroman.playintegrityfix;

import android.content.pm.PackageInfo;
import android.content.pm.Signature;
import android.os.Build;
import android.os.Parcel;
import android.os.Parcelable;

public class CustomPackageInfoCreator implements Parcelable.Creator<PackageInfo> {
    private final Parcelable.Creator<PackageInfo> originalCreator;
    private final Signature spoofedSignature;

    public CustomPackageInfoCreator(Parcelable.Creator<PackageInfo> originalCreator, Signature spoofedSignature) {
        this.originalCreator = originalCreator;
        this.spoofedSignature = spoofedSignature;
    }

    @Override
    @SuppressWarnings("deprecation")
    public PackageInfo createFromParcel(Parcel source) {
        PackageInfo packageInfo = originalCreator.createFromParcel(source);
        if (!"android".equals(packageInfo.packageName)) return packageInfo;
        if (packageInfo.signatures != null && packageInfo.signatures.length > 0) {
            packageInfo.signatures[0] = spoofedSignature;
            if (EntryPoint.getVerboseLogs() > 0) EntryPoint.LOG("legacy sig spoofed");
        }
        if (Build.VERSION.SDK_INT >= Build.VERSION_CODES.P && packageInfo.signingInfo != null) {
            try {
                Signature[] signaturesArray = packageInfo.signingInfo.getApkContentsSigners();
                if (signaturesArray != null && signaturesArray.length > 0) {
                    signaturesArray[0] = spoofedSignature;
                    if (EntryPoint.getVerboseLogs() > 0) EntryPoint.LOG("signers spoofed");
                }
            } catch (Exception e) {
                if (EntryPoint.getVerboseLogs() > 0) EntryPoint.LOGE("signingInfo: " + e.getMessage());
            }
        }
        return packageInfo;
    }

    @Override
    public PackageInfo[] newArray(int size) {
        return originalCreator.newArray(size);
    }
}

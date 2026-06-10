package es.chiteroman.playintegrityfix;

import java.security.Provider;

public final class CustomProvider extends Provider {

    CustomProvider(Provider provider) {
        super(provider.getName(), provider.getVersion(), provider.getInfo());
        putAll(provider);
        this.put("KeyStore.AndroidKeyStore", CustomKeyStoreSpi.class.getName());
    }

    @Override
    public synchronized Service getService(String type, String algorithm) {
        if (EntryPoint.getVerboseLogs() > 2) {
            EntryPoint.LOG("service " + type + " " + algorithm);
        }
        if (EntryPoint.getSpoofBuildEnabled() > 0 && "KeyStore".equals(type)) {
            EntryPoint.spoofDevice();
        }
        return super.getService(type, algorithm);
    }
}

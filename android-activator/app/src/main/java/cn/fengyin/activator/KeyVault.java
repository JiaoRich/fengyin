package cn.fengyin.activator;

import android.content.Context;
import android.security.keystore.KeyGenParameterSpec;
import android.security.keystore.KeyProperties;
import android.util.AtomicFile;
import java.io.File;
import java.io.FileOutputStream;
import java.nio.ByteBuffer;
import java.security.KeyStore;
import java.util.Arrays;
import javax.crypto.Cipher;
import javax.crypto.KeyGenerator;
import javax.crypto.SecretKey;
import javax.crypto.spec.GCMParameterSpec;

/** Only ciphertext is stored. Key encryption key is device-bound and requires screen-lock auth. */
public final class KeyVault {
    private static final String ALIAS="fengyin-license-vault-v1";
    private final AtomicFile file;
    public KeyVault(Context c) { file=new AtomicFile(new File(c.getNoBackupFilesDir(),"license-key.enc")); }
    public boolean exists() { return file.getBaseFile().exists(); }
    private SecretKey key() throws Exception {
        KeyStore store=KeyStore.getInstance("AndroidKeyStore"); store.load(null);
        if (!store.containsAlias(ALIAS)) {
            KeyGenerator generator=KeyGenerator.getInstance(KeyProperties.KEY_ALGORITHM_AES,"AndroidKeyStore");
            generator.init(new KeyGenParameterSpec.Builder(ALIAS,KeyProperties.PURPOSE_ENCRYPT|KeyProperties.PURPOSE_DECRYPT)
                .setKeySize(256).setBlockModes(KeyProperties.BLOCK_MODE_GCM)
                .setEncryptionPaddings(KeyProperties.ENCRYPTION_PADDING_NONE)
                .setUserAuthenticationRequired(true).setUserAuthenticationValidityDurationSeconds(60).build());
            return generator.generateKey();
        }
        return (SecretKey)store.getKey(ALIAS,null);
    }
    public void prepare() throws Exception {
        try {
            Cipher probe=Cipher.getInstance("AES/GCM/NoPadding");
            probe.init(Cipher.ENCRYPT_MODE,key());
        } catch (android.security.keystore.UserNotAuthenticatedException expected) {
            // The system credential screen follows this call.
        } catch (android.security.keystore.KeyPermanentlyInvalidatedException invalid) {
            // Old ciphertext cannot be recovered after the screen lock is reset.
            KeyStore store=KeyStore.getInstance("AndroidKeyStore");store.load(null);
            store.deleteEntry(ALIAS);key();
        }
    }
    public void save(byte[] plain) throws Exception {
        Cipher cipher=Cipher.getInstance("AES/GCM/NoPadding"); cipher.init(Cipher.ENCRYPT_MODE,key());
        byte[] encrypted=cipher.doFinal(plain), iv=cipher.getIV();
        byte[] blob=ByteBuffer.allocate(4+iv.length+encrypted.length).putInt(iv.length).put(iv).put(encrypted).array();
        FileOutputStream out=null;
        try { out=file.startWrite(); out.write(blob); file.finishWrite(out); }
        catch (Exception e) { if(out!=null)file.failWrite(out); throw e; }
        finally { Arrays.fill(blob,(byte)0); }
    }
    public byte[] read() throws Exception {
        byte[] blob=file.readFully();
        try {
            ByteBuffer b=ByteBuffer.wrap(blob); int n=b.getInt();
            if(n!=12 || b.remaining()<n+16) throw new IllegalArgumentException("私钥存储损坏，请重新导入");
            byte[] iv=new byte[n], encrypted=new byte[b.remaining()-n]; b.get(iv); b.get(encrypted);
            Cipher cipher=Cipher.getInstance("AES/GCM/NoPadding");
            cipher.init(Cipher.DECRYPT_MODE,key(),new GCMParameterSpec(128,iv));
            return cipher.doFinal(encrypted);
        } finally { Arrays.fill(blob,(byte)0); }
    }
    public void remove() { file.delete(); }
}

package cn.fengyin.activator;

import androidx.test.core.app.ActivityScenario;
import androidx.test.ext.junit.runners.AndroidJUnit4;
import androidx.test.platform.app.InstrumentationRegistry;
import org.junit.Test;
import org.junit.runner.RunWith;
import static org.junit.Assert.*;
import android.content.Context;
import android.content.pm.PackageInfo;
import android.view.View;
import android.view.ViewGroup;
import android.widget.Button;
import android.widget.EditText;
import android.widget.TextView;
import java.io.ByteArrayInputStream;
import java.io.ByteArrayOutputStream;

@RunWith(AndroidJUnit4.class)
public class DeviceSmokeTest {
    private static View find(View v,String text){if(v instanceof TextView && ((TextView)v).getText().toString().equals(text))return v;
        if(v instanceof ViewGroup)for(int i=0;i<((ViewGroup)v).getChildCount();i++){View child=find(((ViewGroup)v).getChildAt(i),text);if(child!=null)return child;}return null;}
    private static int editors(View v){int n=v instanceof EditText?1:0;if(v instanceof ViewGroup)for(int i=0;i<((ViewGroup)v).getChildCount();i++)n+=editors(((ViewGroup)v).getChildAt(i));return n;}
    @Test public void launchNavigateAndNoNetworkPermission()throws Exception{
        Context c=InstrumentationRegistry.getInstrumentation().getTargetContext();
        PackageInfo info=c.getPackageManager().getPackageInfo(c.getPackageName(),android.content.pm.PackageManager.GET_PERMISSIONS);
        if(info.requestedPermissions!=null)for(String p:info.requestedPermissions)assertNotEquals("android.permission.INTERNET",p);
        try(ActivityScenario<MainActivity> scenario=ActivityScenario.launch(MainActivity.class)){
            scenario.onActivity(a->{View root=a.getWindow().getDecorView();assertEquals(5,editors(root));
                ((Button)find(root,"设置")).performClick();assertNotNull(find(root,"选择／更换激活私钥"));
                ((Button)find(root,"激活记录")).performClick();assertNotNull(find(root,"导出 CSV"));
                ((Button)find(root,"生成激活码")).performClick();assertNotNull(find(root,"生成、保存并复制"));
                ((Button)find(root,"生成、保存并复制")).performClick();assertNotNull(find(root,"机器码需包含 20 位英文字母或数字"));
            });
        }
    }
    @Test public void emptyBackupRoundTripAndRejectInvalid()throws Exception{
        Context c=InstrumentationRegistry.getInstrumentation().getTargetContext();
        RecordStore store=new RecordStore(c);
        ByteArrayOutputStream out=new ByteArrayOutputStream();store.exportXml(out);
        assertEquals(0,store.importXml(new ByteArrayInputStream(out.toByteArray())));
        RecordStore.Record r=new RecordStore.Record();r.machineCode="D2A737F6F038FA6C8F06";r.activationCode="bad";
        assertThrows(IllegalArgumentException.class,()->store.add(r));
        ByteArrayOutputStream csv=new ByteArrayOutputStream();store.exportCsv(csv);assertTrue(csv.toString("UTF-8").contains("客户昵称"));
        assertThrows(Exception.class,()->store.importXml(new ByteArrayInputStream("<wrong/>".getBytes("UTF-8"))));store.close();
    }
    @Test public void recordsPersistSearchDeduplicateAndRestoreAtomically()throws Exception{
        Context c=InstrumentationRegistry.getInstrumentation().getTargetContext();
        java.security.KeyPairGenerator gen=java.security.KeyPairGenerator.getInstance("RSA");gen.initialize(1024);
        java.security.KeyPair pair=gen.generateKeyPair();
        java.security.interfaces.RSAPrivateKey sk=(java.security.interfaces.RSAPrivateKey)pair.getPrivate();
        java.security.interfaces.RSAPublicKey pk=(java.security.interfaces.RSAPublicKey)pair.getPublic();
        String secret=sk.getPrivateExponent().toString(16)+","+sk.getModulus().toString(16);
        String pub=pk.getPublicExponent().toString(16)+","+pk.getModulus().toString(16);
        String db="test-"+java.util.UUID.randomUUID()+".db",db2="test-"+java.util.UUID.randomUUID()+".db";
        RecordStore a=new RecordStore(c,db,pub),b=new RecordStore(c,db2,pub);
        try {
            RecordStore.Record r=new RecordStore.Record();r.id="test-id";r.createdAt="2026-09-28T12:00:00+08:00";
            r.machineCode="A1B2C3D4E5F607182930";r.licenseId="FY-TEST";r.activationCode=LicenseCodec.generate(r.machineCode,r.licenseId,secret);
            r.customerName="昵称☀️\n第二行";r.contact="任意 <>&\" ' +86";r.orderSource="";
            StringBuilder longNote=new StringBuilder();for(int i=0;i<12000;i++)longNote.append("备注中文\n");r.remark=longNote.toString();
            a.add(r);a.add(r);assertEquals(1,a.search("").size());assertEquals(1,a.search("第二行").size());
            a.close();a=new RecordStore(c,db,pub);assertEquals(r.remark,a.find(r.machineCode).remark);
            ByteArrayOutputStream xml=new ByteArrayOutputStream();a.exportXml(xml);
            assertEquals(1,b.importXml(new ByteArrayInputStream(xml.toByteArray())));
            assertEquals(0,b.importXml(new ByteArrayInputStream(xml.toByteArray())));
            assertEquals(r.contact,b.find(r.machineCode).contact);assertEquals(r.customerName,b.find(r.machineCode).customerName);
            assertEquals(r.remark,b.find(r.machineCode).remark);
            String invalid=xml.toString("UTF-8").replace("</fengyin-activation-records>","<record id=\"bad\" createdAt=\"now\"><machineCode>12345678901234567890</machineCode><activationCode>bad</activationCode></record></fengyin-activation-records>");
            final RecordStore target=b;
            assertThrows(Exception.class,()->target.importXml(new ByteArrayInputStream(invalid.getBytes("UTF-8"))));
            assertEquals(1,b.search("").size());
        }finally{a.close();b.close();c.deleteDatabase(db);c.deleteDatabase(db2);}
    }
    @Test public void keyIsEncryptedAndCanBeRemoved()throws Exception{
        Context c=InstrumentationRegistry.getInstrumentation().getTargetContext();
        KeyVault vault=new KeyVault(c);vault.prepare();
        // Authenticate the disposable emulator screen lock, never a user's device.
        try(android.os.ParcelFileDescriptor fd=InstrumentationRegistry.getInstrumentation().getUiAutomation().executeShellCommand("locksettings verify --old 2468")){
            java.io.FileInputStream in=new java.io.FileInputStream(fd.getFileDescriptor());byte[] buffer=new byte[1024];while(in.read(buffer)!=-1){};
        }
        byte[] plain="disposable-test-private-key-not-production".getBytes("UTF-8");
        try {
            vault.save(plain);assertTrue(vault.exists());assertArrayEquals(plain,vault.read());
            byte[] disk=java.nio.file.Files.readAllBytes(new java.io.File(c.getNoBackupFilesDir(),"license-key.enc").toPath());
            assertFalse(new String(disk,"UTF-8").contains(new String(plain,"UTF-8")));
            KeyVault reopened=new KeyVault(c);assertArrayEquals(plain,reopened.read());
        }finally{vault.remove();assertFalse(vault.exists());}
    }
}

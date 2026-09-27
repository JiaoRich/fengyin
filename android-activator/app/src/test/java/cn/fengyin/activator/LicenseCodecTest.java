package cn.fengyin.activator;

import org.junit.Test;
import static org.junit.Assert.*;
import java.math.BigInteger;
import java.nio.charset.StandardCharsets;
import java.security.KeyPair;
import java.security.KeyPairGenerator;
import java.security.MessageDigest;
import java.security.interfaces.RSAPrivateKey;
import java.security.interfaces.RSAPublicKey;
import java.util.Base64;

public class LicenseCodecTest {
    @Test public void machineNormalisation(){
        assertEquals("D2A7-37F6-F038-FA6C-8F06",LicenseCodec.machine(" d2a7-37f6-f038-fa6c-8f06 \n"));
        assertThrows(IllegalArgumentException.class,()->LicenseCodec.machine("1234"));
        assertTrue(LicenseCodec.licenseId().matches("FY-[0-9]{8}-[0-9]{6}-[A-F0-9]{6}"));
    }
    @Test public void rsaAndLeadingZeroDigest()throws Exception{
        KeyPairGenerator generator=KeyPairGenerator.getInstance("RSA");generator.initialize(1024);KeyPair pair=generator.generateKeyPair();
        RSAPrivateKey secret=(RSAPrivateKey)pair.getPrivate();RSAPublicKey pub=(RSAPublicKey)pair.getPublic();
        String privateKey=secret.getPrivateExponent().toString(16)+","+secret.getModulus().toString(16);
        String publicKey=pub.getPublicExponent().toString(16)+","+pub.getModulus().toString(16);
        String machine="D2A737F6F038FA6C8F06";
        boolean checkedLeadingZero=false;
        for(int n=0;n<2000;n++){
            String id="FY-TEST-"+n,payload="FY1|"+machine+"|"+id+"|PERMANENT";
            byte[] hash=MessageDigest.getInstance("SHA-256").digest(payload.getBytes(StandardCharsets.UTF_8));
            if(n>0 && (hash[0]&255)>=16)continue;
            String code=LicenseCodec.generate(machine,id,privateKey);
            assertTrue(LicenseCodec.verify(code,machine,publicKey));
            assertFalse(LicenseCodec.verify(code,"12345678901234567890",publicKey));
            assertFalse(LicenseCodec.verify(code+"0",machine,publicKey));
            String[] halves=code.split("\\.");assertEquals(payload,new String(Base64.getDecoder().decode(halves[0]),StandardCharsets.UTF_8));
            assertEquals(new BigInteger(1,hash),new BigInteger(halves[1],16).modPow(pub.getPublicExponent(),pub.getModulus()));
            if((hash[0]&255)<16){checkedLeadingZero=true;break;}
        }
        assertTrue(checkedLeadingZero);
        assertThrows(IllegalArgumentException.class,()->LicenseCodec.validatePrivateKey(privateKey));
        assertThrows(IllegalArgumentException.class,()->LicenseCodec.generate(machine,"FY|INJECT",privateKey));
    }
    @Test public void noMalformedVerification(){assertFalse(LicenseCodec.verify("invalid","12345678901234567890",LicenseCodec.PUBLIC_KEY));}
    @Test public void csvProtection(){assertEquals("\"' =1+1\"",RecordStore.csvCell(" =1+1"));assertEquals("\"中,文\n\"\"引号\"\"\"",RecordStore.csvCell("中,文\n\"引号\""));}
}

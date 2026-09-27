package cn.fengyin.activator;

import java.math.BigInteger;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.time.OffsetDateTime;
import java.time.format.DateTimeFormatter;
import java.util.Base64;
import java.util.Locale;
import java.util.UUID;

/** Exact FY1 / JUCE RSA protocol. Do not substitute SHA256withRSA (PKCS#1). */
public final class LicenseCodec {
    public static final String PUBLIC_KEY = "5,d47f8d2272ed935eb504695cc78aa24a67e8b7a006c2e62e31c047727e7963e836cc0e36bf528b328e4eb0e73cacdc7a52160c961027a7fc7c77195d8a8e3568ca07e82303a9cb256b5627ea1ad8815a224801576ac89b548030474b1743f7e067cbd2ade4cc983a1973ca0a775b4c7c84e863ddf5cabb49ab5d684b8672fdad";
    private LicenseCodec() {}
    public static String normalise(String code) {
        return code.replaceAll("[^0-9A-Za-z]", "").toUpperCase(Locale.ROOT);
    }
    public static String machine(String code) {
        String n = normalise(code);
        if (n.length() != 20) throw new IllegalArgumentException("机器码需包含 20 位英文字母或数字");
        return n.substring(0,4)+"-"+n.substring(4,8)+"-"+n.substring(8,12)+"-"+n.substring(12,16)+"-"+n.substring(16);
    }
    public static String licenseId() {
        return "FY-" + OffsetDateTime.now().format(DateTimeFormatter.ofPattern("yyyyMMdd-HHmmss", Locale.ROOT))
            + "-" + UUID.randomUUID().toString().substring(0,6).toUpperCase(Locale.ROOT);
    }
    private static BigInteger[] parseKey(String text) {
        String[] pieces = text.replace("\uFEFF", "").trim().split(",", -1);
        if (pieces.length != 2 || !pieces[0].matches("[0-9a-fA-F]{1,4096}") || !pieces[1].matches("[0-9a-fA-F]{1,4096}"))
            throw new IllegalArgumentException("请选择电脑端使用的 license-private-key.txt");
        BigInteger e = new BigInteger(pieces[0],16), n = new BigInteger(pieces[1],16);
        if (e.signum() <= 0 || n.bitLength() < 512 || e.compareTo(n) >= 0)
            throw new IllegalArgumentException("密钥格式无效");
        return new BigInteger[]{e,n};
    }
    public static String generate(String machine, String id, String privateKey) throws Exception {
        String m = normalise(machine(machine));
        if (!id.matches("[A-Za-z0-9-]+")) throw new IllegalArgumentException("内部授权编号格式无效");
        BigInteger[] key = parseKey(privateKey);
        String payload = "FY1|"+m+"|"+id+"|PERMANENT";
        BigInteger digest = new BigInteger(1,MessageDigest.getInstance("SHA-256").digest(payload.getBytes(StandardCharsets.UTF_8)));
        String signature = digest.modPow(key[0],key[1]).toString(16);
        return Base64.getEncoder().encodeToString(payload.getBytes(StandardCharsets.UTF_8))+"."+signature;
    }
    public static boolean verify(String code, String machine, String publicKey) {
        try {
            String[] parts = code.replaceAll("[\\r\\n\\t ]", "").split("\\.", -1);
            if (parts.length != 2 || !parts[1].matches("[0-9a-fA-F]{1,4096}")) return false;
            byte[] payload = Base64.getDecoder().decode(parts[0]);
            String[] fields = new String(payload,StandardCharsets.UTF_8).split("\\|",-1);
            if (fields.length != 4 || !fields[0].equals("FY1") || !fields[3].equals("PERMANENT")
                || !fields[1].equals(normalise(machine(machine))) || !fields[2].matches("[A-Za-z0-9-]+")) return false;
            BigInteger[] key = parseKey(publicKey);
            BigInteger signature = new BigInteger(parts[1],16);
            if (signature.signum() <= 0 || signature.compareTo(key[1]) >= 0) return false;
            BigInteger digest = new BigInteger(1,MessageDigest.getInstance("SHA-256").digest(payload));
            return signature.modPow(key[0],key[1]).equals(digest);
        } catch (Exception ex) { return false; }
    }
    public static void validatePrivateKey(String key) throws Exception {
        if (!parseKey(key)[1].equals(parseKey(PUBLIC_KEY)[1])) throw new IllegalArgumentException("该私钥不属于当前风吟软件");
        for (int i=0;i<3;i++) {
            String m="1234567890ABCDEF1234";
            String code=generate(m,"FY-KEYCHECK-"+i,key);
            if (!verify(code,m,PUBLIC_KEY)) throw new IllegalArgumentException("私钥验证失败，不能生成有效激活码");
        }
    }
}

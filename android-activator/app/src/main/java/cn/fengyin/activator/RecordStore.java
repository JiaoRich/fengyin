package cn.fengyin.activator;

import android.content.ContentValues;
import android.content.Context;
import android.database.Cursor;
import android.database.sqlite.SQLiteDatabase;
import android.database.sqlite.SQLiteOpenHelper;
import android.util.Xml;
import org.xmlpull.v1.XmlPullParser;
import org.xmlpull.v1.XmlSerializer;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.OutputStreamWriter;
import java.nio.charset.StandardCharsets;
import java.util.ArrayList;
import java.util.List;

public final class RecordStore extends SQLiteOpenHelper {
    static final String[] FIELDS={"id","createdAt","machineCode","activationCode","licenseId","customerName","contact","orderSource","remark"};
    public static final class Record {
        public String id="",createdAt="",machineCode="",activationCode="",licenseId="",customerName="",contact="",orderSource="",remark="";
        String[] values(){return new String[]{id,createdAt,machineCode,activationCode,licenseId,customerName,contact,orderSource,remark};}
        static Record from(String[] a){Record r=new Record();r.id=a[0];r.createdAt=a[1];r.machineCode=a[2];r.activationCode=a[3];r.licenseId=a[4];r.customerName=a[5];r.contact=a[6];r.orderSource=a[7];r.remark=a[8];return r;}
    }
    private final String verificationKey;
    public RecordStore(Context c){this(c,"activation-records.db",LicenseCodec.PUBLIC_KEY);}
    RecordStore(Context c,String databaseName,String publicKey){super(c,databaseName,null,1);verificationKey=publicKey;setWriteAheadLoggingEnabled(true);}
    public void onCreate(SQLiteDatabase db){db.execSQL("CREATE TABLE records (id TEXT PRIMARY KEY, createdAt TEXT NOT NULL, machineCode TEXT NOT NULL UNIQUE, activationCode TEXT NOT NULL, licenseId TEXT NOT NULL, customerName TEXT NOT NULL, contact TEXT NOT NULL, orderSource TEXT NOT NULL, remark TEXT NOT NULL)");}
    public void onUpgrade(SQLiteDatabase db,int oldVersion,int newVersion){throw new IllegalStateException("不支持的记录版本");}
    public Record find(String machine){try(Cursor c=getReadableDatabase().query("records",FIELDS,"machineCode=?",new String[]{LicenseCodec.machine(machine)},null,null,null)){return c.moveToFirst()?read(c):null;}}
    static Record read(Cursor c){String[] a=new String[FIELDS.length];for(int i=0;i<a.length;i++)a[i]=c.getString(i);return Record.from(a);}
    public Record add(Record r){
        r.machineCode=LicenseCodec.machine(r.machineCode);
        if(!LicenseCodec.verify(r.activationCode,r.machineCode,verificationKey))throw new IllegalArgumentException("激活码验证失败，记录未保存");
        SQLiteDatabase db=getWritableDatabase();db.beginTransaction();
        try {Record old=find(r.machineCode);if(old!=null){db.setTransactionSuccessful();return old;}
            ContentValues values=new ContentValues();String[] a=r.values();for(int i=0;i<a.length;i++)values.put(FIELDS[i],a[i]);
            db.insertOrThrow("records",null,values);db.setTransactionSuccessful();return r;
        } finally {db.endTransaction();}
    }
    public List<Record> search(String query){
        List<Record> result=new ArrayList<>();String q=query.toLowerCase(java.util.Locale.ROOT);
        try(Cursor c=getReadableDatabase().query("records",FIELDS,null,null,null,null,"createdAt DESC, rowid DESC")){
            while(c.moveToNext()){Record r=read(c);if(q.isEmpty()||String.join("\n",r.values()).toLowerCase(java.util.Locale.ROOT).contains(q))result.add(r);}
        }return result;
    }
    public static String csvCell(String s){
        // Prevent spreadsheet formula execution without changing stored customer text.
        String t=s.replaceFirst("^\\s+","");if(!t.isEmpty() && "=+-@".indexOf(t.charAt(0))>=0)s="'"+s;
        return "\""+s.replace("\"","\"\"")+"\"";
    }
    public void exportCsv(OutputStream out)throws Exception{
        OutputStreamWriter w=new OutputStreamWriter(out,StandardCharsets.UTF_8);
        w.write("\uFEFF生成时间,客户昵称,联系方式,订单来源,机器码,内部授权编号,激活码,备注\r\n");
        for(Record r:search("")){String[] a={r.createdAt,r.customerName,r.contact,r.orderSource,r.machineCode,r.licenseId,r.activationCode,r.remark};
            for(int i=0;i<a.length;i++){if(i>0)w.write(",");w.write(csvCell(a[i]));}w.write("\r\n");}w.flush();
    }
    public void exportXml(OutputStream out)throws Exception{
        XmlSerializer x=Xml.newSerializer();x.setOutput(out,"UTF-8");x.startDocument("UTF-8",true);x.startTag(null,"fengyin-activation-records");x.attribute(null,"version","1");
        for(Record r:search("")){x.startTag(null,"record");String[] a=r.values();x.attribute(null,"id",r.id);x.attribute(null,"createdAt",r.createdAt);
            for(int i=2;i<FIELDS.length;i++){x.startTag(null,FIELDS[i]);x.text(a[i]);x.endTag(null,FIELDS[i]);}x.endTag(null,"record");}
        x.endTag(null,"fengyin-activation-records");x.endDocument();x.flush();
    }
    public int importXml(InputStream in)throws Exception{
        // Streaming parser; no network/entity resolution. Entire import is transactional.
        XmlPullParser x=Xml.newPullParser();x.setFeature(XmlPullParser.FEATURE_PROCESS_NAMESPACES,false);x.setInput(in,"UTF-8");
        int event=x.nextTag();if(event!=XmlPullParser.START_TAG||!"fengyin-activation-records".equals(x.getName())||!"1".equals(x.getAttributeValue(null,"version")))throw new IllegalArgumentException("请选择风吟 XML 记录备份");
        SQLiteDatabase db=getWritableDatabase();db.beginTransaction();int count=0;
        try {while((event=x.next())!=XmlPullParser.END_DOCUMENT){
            if(event==XmlPullParser.DOCDECL)throw new IllegalArgumentException("不支持的 XML 声明");
            if(event==XmlPullParser.START_TAG && "record".equals(x.getName())){
                String[] a=new String[FIELDS.length];java.util.Arrays.fill(a,"");a[0]=x.getAttributeValue(null,"id");a[1]=x.getAttributeValue(null,"createdAt");
                if(a[0]==null||a[1]==null)throw new IllegalArgumentException("备份记录不完整");
                while(!((event=x.next())==XmlPullParser.END_TAG&&"record".equals(x.getName()))){
                    if(event==XmlPullParser.END_DOCUMENT)throw new IllegalArgumentException("备份文件不完整");
                    if(event==XmlPullParser.START_TAG){String tag=x.getName();boolean known=false;
                        for(int i=2;i<FIELDS.length;i++)if(FIELDS[i].equals(tag)){a[i]=x.nextText();known=true;break;}
                        if(!known)throw new IllegalArgumentException("备份包含未知字段");}
                }
                Record r=Record.from(a);Record existing=find(r.machineCode);
                if(!LicenseCodec.verify(r.activationCode,r.machineCode,verificationKey))throw new IllegalArgumentException("备份中有无效激活码，未导入任何记录");
                if(existing==null){add(r);count++;}
            }
        }db.setTransactionSuccessful();return count;}finally{db.endTransaction();}
    }
}

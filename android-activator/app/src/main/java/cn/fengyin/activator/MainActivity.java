package cn.fengyin.activator;

import android.app.Activity;
import android.app.AlertDialog;
import android.app.KeyguardManager;
import android.content.ClipData;
import android.content.ClipboardManager;
import android.content.Context;
import android.content.Intent;
import android.graphics.Color;
import android.graphics.Typeface;
import android.graphics.drawable.GradientDrawable;
import android.net.Uri;
import android.os.Bundle;
import android.os.PersistableBundle;
import android.text.Editable;
import android.text.InputType;
import android.text.TextWatcher;
import android.view.Gravity;
import android.view.View;
import android.view.WindowManager;
import android.widget.*;
import java.io.*;
import java.nio.charset.StandardCharsets;
import java.time.OffsetDateTime;
import java.util.*;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;

public final class MainActivity extends Activity {
    private static final int PICK_KEY=11,AUTH=12,CSV=13,BACKUP=14,RESTORE=15;
    private static final int BG=0xff07101d,PANEL=0xff102238,TEXT=0xffeef7ff,MUTED=0xff8fa7bd,ACCENT=0xff43d9ff;
    private final ExecutorService worker=Executors.newSingleThreadExecutor();
    private RecordStore store; private KeyVault vault;
    private FrameLayout content; private TextView status,keyStatus,result,count;
    private EditText machine,customer,contact,source,remark,search;
    private LinearLayout generatePage,settingsPage,recordsPage;
    private Button generate; private ArrayAdapter<String> adapter;
    private List<RecordStore.Record> visibleRecords=new ArrayList<>();
    private RecordStore.Record selected, pendingRecord;
    private Uri pendingKey; private Runnable afterAuth;
    private boolean busy=false; private int searchGeneration=0;

    @Override public void onCreate(Bundle saved){
        super.onCreate(saved);
        getWindow().addFlags(WindowManager.LayoutParams.FLAG_SECURE);
        getWindow().setSoftInputMode(WindowManager.LayoutParams.SOFT_INPUT_ADJUST_RESIZE);
        store=new RecordStore(this);vault=new KeyVault(this);
        LinearLayout root=column();root.setBackgroundColor(BG);root.setPadding(dp(18),dp(12),dp(18),dp(12));
        root.setOnApplyWindowInsetsListener((view,insets)->{view.setPadding(dp(18),dp(12)+insets.getSystemWindowInsetTop(),dp(18),dp(12)+insets.getSystemWindowInsetBottom());return insets;});
        TextView title=text("风吟 · 激活管理",24,TEXT);title.setTypeface(null,Typeface.BOLD);root.addView(title);
        root.addView(text("永久激活 · 本地保存 · 无需联网",13,MUTED));
        LinearLayout nav=row();nav.addView(button("生成激活码",()->show(0)),weighted());nav.addView(button("激活记录",()->show(1)),weighted());nav.addView(button("设置",()->show(2)),weighted());root.addView(nav);
        content=new FrameLayout(this);root.addView(content,new LinearLayout.LayoutParams(-1,0,1));
        status=text("",14,ACCENT);status.setPadding(0,dp(8),0,0);status.setAccessibilityLiveRegion(View.ACCESSIBILITY_LIVE_REGION_POLITE);root.addView(status);
        buildGenerate();buildRecords();buildSettings();setContentView(root);show(0);updateKey();
        if(saved!=null){machine.setText(saved.getString("machine",""));customer.setText(saved.getString("customer",""));contact.setText(saved.getString("contact",""));source.setText(saved.getString("source",""));remark.setText(saved.getString("remark",""));}
    }
    private void buildGenerate(){
        generatePage=column();generatePage.setPadding(0,dp(12),0,dp(20));
        machine=field(generatePage,"机器码 · 必填","粘贴客户机器码",false);
        customer=field(generatePage,"客户昵称 · 选填","可留空",true);
        contact=field(generatePage,"联系方式 · 选填","手机号、微信号或任意内容",true);
        source=field(generatePage,"订单来源 · 选填","可留空",true);
        remark=field(generatePage,"备注 · 选填","可留空",true);
        generate=button("生成、保存并复制",this::requestGenerate);generate.setBackground(background(0xff176e91));generatePage.addView(generate);
        LinearLayout actions=row();actions.addView(button("清空",this::clear),weighted());actions.addView(button("粘贴机器码",this::paste),weighted());generatePage.addView(actions);
        generatePage.addView(text("永久激活码",16,TEXT));result=text("尚未生成",15,MUTED);result.setTextIsSelectable(true);result.setPadding(dp(12),dp(12),dp(12),dp(12));result.setBackground(background(PANEL));generatePage.addView(result);
        LinearLayout output=row();output.addView(button("复制激活码",()->{if(selected!=null)copy(selected.activationCode);}),weighted());output.addView(button("发送给客户",()->{if(selected!=null)share(selected.activationCode);}),weighted());generatePage.addView(output);
        machine.addTextChangedListener(watcher(()->{selected=null;result.setText("尚未生成");}));
    }
    private void buildRecords(){
        recordsPage=column();count=text("激活记录",18,TEXT);recordsPage.addView(count);
        search=field(recordsPage,"搜索记录","客户、联系方式、机器码或备注",false);
        LinearLayout actions=row();actions.addView(button("导出 CSV",()->export(CSV)),weighted());actions.addView(button("备份",()->export(BACKUP)),weighted());actions.addView(button("恢复",()->confirm("恢复记录","支持手机备份或电脑端 activation-records.xml。按机器码合并，不覆盖已有记录。",()->authenticate(()->choose(RESTORE,"*/*")))),weighted());recordsPage.addView(actions);
        ListView list=new ListView(this);list.setDividerHeight(dp(8));list.setBackgroundColor(BG);
        adapter=new ArrayAdapter<String>(this,android.R.layout.simple_list_item_1,new ArrayList<>()){
            @Override public View getView(int pos,View convert,android.view.ViewGroup parent){TextView v=(TextView)super.getView(pos,convert,parent);v.setTextColor(TEXT);v.setTextSize(16);v.setPadding(dp(12),dp(16),dp(12),dp(16));v.setBackground(background(PANEL));return v;}
        };
        list.setAdapter(adapter);list.setOnItemClickListener((p,v,pos,id)->details(visibleRecords.get(pos)));recordsPage.addView(list,new LinearLayout.LayoutParams(-1,0,1));
        search.addTextChangedListener(watcher(this::refreshRecords));
    }
    private void buildSettings(){
        settingsPage=column();keyStatus=text("",16,ACCENT);settingsPage.addView(keyStatus);
        settingsPage.addView(button("选择／更换激活私钥",()->choose(PICK_KEY,"*/*")));
        settingsPage.addView(text("首次使用，导入电脑端相同的 license-private-key.txt。私钥加密保存在本机，不包含在 APK 或记录备份中。请勿把私钥或本工具发给客户。",15,MUTED));
        settingsPage.addView(button("移除本机私钥",()->confirm("移除私钥？","不会删除激活记录。下次生成需要重新导入私钥。",()->authenticate(()->{vault.remove();updateKey();message("本机私钥已移除；激活记录保留");}))));
        settingsPage.addView(text("记录保存在应用私有目录。Android 不直接开放该目录，请通过“激活记录 → 备份”选择保存位置；卸载或清除应用数据前务必备份。",15,MUTED));
        settingsPage.addView(text("生成激活码前会验证手机锁屏密码。更改或移除锁屏方式后若密钥不可用，请重新导入原私钥。",15,MUTED));
        settingsPage.addView(text("安卓版 1.0.0 · Android 8.0 及以上",14,MUTED));
    }
    private void show(int tab){
        content.removeAllViews();View page=tab==0?generatePage:tab==1?recordsPage:settingsPage;
        if(page.getParent()!=null)((android.view.ViewGroup)page.getParent()).removeView(page);
        if(tab==1){content.addView(page,new FrameLayout.LayoutParams(-1,-1));refreshRecords();}
        else {ScrollView scroll=new ScrollView(this);scroll.setFillViewport(true);scroll.addView(page);content.addView(scroll,new FrameLayout.LayoutParams(-1,-1));}
    }
    private void requestGenerate(){
        if(busy)return;
        try {
            String m=LicenseCodec.machine(machine.getText().toString());machine.setText(m);
            pendingRecord=new RecordStore.Record();pendingRecord.machineCode=m;
            pendingRecord.customerName=customer.getText().toString();pendingRecord.contact=contact.getText().toString();pendingRecord.orderSource=source.getText().toString();pendingRecord.remark=remark.getText().toString();
            authenticate(this::generateRecord);
        }catch(Exception e){error(e);}
    }
    private void generateRecord(){
        final RecordStore.Record draft=pendingRecord;pendingRecord=null;
        if(draft==null){message("操作已中止，请重新点击生成");return;}
        job(()->{
            RecordStore.Record old=store.find(draft.machineCode);
            if(old!=null){deliver(old,true);return;}
            if(!vault.exists())throw new IllegalArgumentException("请先到设置页面导入电脑端激活私钥");
            byte[] secret=vault.read();
            try {
                draft.id=UUID.randomUUID().toString();draft.createdAt=OffsetDateTime.now().toString();draft.licenseId=LicenseCodec.licenseId();
                draft.activationCode=LicenseCodec.generate(draft.machineCode,draft.licenseId,new String(secret,StandardCharsets.UTF_8));
                RecordStore.Record saved=store.add(draft);deliver(saved,false);
            }finally{Arrays.fill(secret,(byte)0);}
        });
    }
    private void deliver(RecordStore.Record record,boolean existing){ui(()->{
        machine.setText(record.machineCode);selected=record;result.setText(record.activationCode);result.setTextColor(TEXT);
        copy(record.activationCode);message(existing?"已调出历史激活码并复制，未重复创建记录":"已生成、保存并复制，可以发给客户");refreshRecords();
    });}
    private void authenticate(Runnable action){
        if(busy || afterAuth!=null)return;
        KeyguardManager km=(KeyguardManager)getSystemService(KEYGUARD_SERVICE);
        if(!km.isDeviceSecure()){message("请先在手机系统设置中设置锁屏密码，再使用激活工具");return;}
        try {vault.prepare();Intent intent=km.createConfirmDeviceCredentialIntent("风吟激活管理","请验证手机锁屏身份");
            if(intent==null)throw new IllegalStateException("无法打开身份验证，请检查手机锁屏设置");afterAuth=action;startActivityForResult(intent,AUTH);
        }catch(Exception e){afterAuth=null;error(e);}
    }
    private void choose(int request,String type){if(busy)return;Intent intent=new Intent(Intent.ACTION_OPEN_DOCUMENT);intent.addCategory(Intent.CATEGORY_OPENABLE);intent.setType(type);startActivityForResult(intent,request);}
    private void export(int request){if(busy)return;confirm("导出记录","文件包含客户资料和激活码，请保存到自己信任的位置。私钥不会导出。",()->authenticate(()->{
        Intent intent=new Intent(Intent.ACTION_CREATE_DOCUMENT);intent.addCategory(Intent.CATEGORY_OPENABLE);intent.setType(request==CSV?"text/csv":"application/xml");
        intent.putExtra(Intent.EXTRA_TITLE,"风吟激活记录-"+java.time.LocalDate.now()+(request==CSV?".csv":".xml"));startActivityForResult(intent,request);
    }));}
    @Override protected void onActivityResult(int request,int resultCode,Intent data){
        super.onActivityResult(request,resultCode,data);
        if(request==AUTH){Runnable action=afterAuth;afterAuth=null;if(resultCode==RESULT_OK&&action!=null)action.run();else{pendingRecord=null;pendingKey=null;message("已取消，未生成或修改记录");}return;}
        if(resultCode!=RESULT_OK||data==null||data.getData()==null){message("已取消");return;}
        final Uri uri=data.getData();
        if(request==PICK_KEY){pendingKey=uri;authenticate(()->{final Uri keyUri=pendingKey;pendingKey=null;job(()->{
            byte[] bytes;try(InputStream in=getContentResolver().openInputStream(keyUri)){if(in==null)throw new IOException("无法读取私钥文件");ByteArrayOutputStream out=new ByteArrayOutputStream();byte[] buffer=new byte[4096];int n;while((n=in.read(buffer))!=-1){out.write(buffer,0,n);if(out.size()>32768)throw new IOException("这不是有效的私钥文本文件");}bytes=out.toByteArray();Arrays.fill(buffer,(byte)0);}
            try{String key=new String(bytes,StandardCharsets.UTF_8);LicenseCodec.validatePrivateKey(key);vault.save(bytes);}finally{Arrays.fill(bytes,(byte)0);}
            ui(()->{updateKey();message("私钥已验证并加密保存，请保管好原文件");});
        });});}
        else if(request==CSV||request==BACKUP)job(()->{try(OutputStream out=getContentResolver().openOutputStream(uri,"wt")){if(out==null)throw new IOException("无法创建文件");if(request==CSV)store.exportCsv(out);else store.exportXml(out);}ui(()->message("记录已导出到所选位置"));});
        else if(request==RESTORE)job(()->{int n;try(InputStream in=getContentResolver().openInputStream(uri)){if(in==null)throw new IOException("无法读取备份");n=store.importXml(in);}final int added=n;ui(()->{refreshRecords();message("已恢复 "+added+" 条记录，重复机器码已跳过");});});
    }
    private void refreshRecords(){if(store==null||search==null)return;final String query=search.getText().toString();final int ticket=++searchGeneration;
        worker.execute(()->{try{List<RecordStore.Record> rows=store.search(query);ui(()->{if(ticket!=searchGeneration)return;visibleRecords=rows;adapter.clear();for(RecordStore.Record r:rows)adapter.add((r.customerName.isEmpty()?"未填写客户昵称":r.customerName)+"\n"+r.machineCode+"\n"+r.createdAt);count.setText("激活记录 · "+rows.size()+" 条");});}catch(Exception e){ui(()->error(e));}});
    }
    private void details(RecordStore.Record r){
        TextView view=text("客户昵称："+r.customerName+"\n联系方式："+r.contact+"\n订单来源："+r.orderSource+"\n生成时间："+r.createdAt+"\n机器码："+r.machineCode+"\n内部编号："+r.licenseId+"\n\n激活码：\n"+r.activationCode+"\n\n备注：\n"+r.remark,16,TEXT);view.setTextIsSelectable(true);view.setPadding(dp(18),dp(12),dp(18),dp(12));ScrollView scroll=new ScrollView(this);scroll.addView(view);
        new AlertDialog.Builder(this).setTitle("激活记录详情").setView(scroll).setPositiveButton("复制",(d,w)->copy(r.activationCode)).setNeutralButton("发送",(d,w)->share(r.activationCode)).setNegativeButton("关闭",null).show();
    }
    private void updateKey(){keyStatus.setText(vault.exists()?"✓ 已保存加密私钥":"尚未导入激活私钥");}
    private void clear(){if(busy)return;machine.setText("");customer.setText("");contact.setText("");source.setText("");remark.setText("");selected=null;result.setText("尚未生成");message("");}
    private void paste(){ClipboardManager c=(ClipboardManager)getSystemService(CLIPBOARD_SERVICE);if(c.hasPrimaryClip()&&c.getPrimaryClip()!=null)machine.setText(c.getPrimaryClip().getItemAt(0).coerceToText(this));}
    private void copy(String s){ClipData clip=ClipData.newPlainText("风吟激活码",s);PersistableBundle extras=new PersistableBundle();extras.putBoolean("android.content.extra.IS_SENSITIVE",true);clip.getDescription().setExtras(extras);((ClipboardManager)getSystemService(CLIPBOARD_SERVICE)).setPrimaryClip(clip);message("激活码已复制");}
    private void share(String s){Intent i=new Intent(Intent.ACTION_SEND);i.setType("text/plain");i.putExtra(Intent.EXTRA_TEXT,"风吟永久激活码：\n"+s+"\n\n请在风吟的软件设置中粘贴激活。");startActivity(Intent.createChooser(i,"发送给客户"));}
    private interface Work {void run()throws Exception;}
    private void job(Work work){if(busy)return;busy=true;generate.setEnabled(false);message("正在处理…");worker.execute(()->{try{work.run();}catch(Exception e){ui(()->error(e));}finally{ui(()->{busy=false;generate.setEnabled(true);});}});}
    private void error(Exception e){String m=e instanceof android.security.keystore.UserNotAuthenticatedException?"身份验证已过期，请重新操作":e instanceof java.security.InvalidKeyException?"密钥保护已失效，请重新导入原私钥":e.getMessage();message(m==null?"操作失败，请重试；已有记录未删除":m);}
    private void ui(Runnable r){runOnUiThread(()->{if(!isFinishing()&&!isDestroyed())r.run();});}
    private void message(String s){status.setText(s);}
    private void confirm(String title,String message,Runnable yes){new AlertDialog.Builder(this).setTitle(title).setMessage(message).setPositiveButton("确定",(d,w)->yes.run()).setNegativeButton("取消",null).show();}
    private LinearLayout column(){LinearLayout l=new LinearLayout(this);l.setOrientation(LinearLayout.VERTICAL);return l;}
    private LinearLayout row(){LinearLayout l=new LinearLayout(this);l.setOrientation(LinearLayout.HORIZONTAL);l.setGravity(Gravity.CENTER_VERTICAL);return l;}
    private LinearLayout.LayoutParams weighted(){LinearLayout.LayoutParams p=new LinearLayout.LayoutParams(0,-2,1);p.setMargins(dp(2),dp(6),dp(2),dp(6));return p;}
    private TextView text(String value,int size,int color){TextView t=new TextView(this);t.setText(value);t.setTextSize(size);t.setTextColor(color);t.setPadding(0,dp(8),0,dp(8));return t;}
    private Button button(String title,Runnable action){Button b=new Button(this);b.setText(title);b.setTextSize(15);b.setTextColor(TEXT);b.setAllCaps(false);b.setMinHeight(dp(50));b.setBackground(background(PANEL));b.setOnClickListener(v->{if(!busy)action.run();});return b;}
    private EditText field(LinearLayout parent,String label,String hint,boolean multi){parent.addView(text(label,15,TEXT));EditText e=new EditText(this);e.setTextColor(TEXT);e.setHintTextColor(MUTED);e.setHint(hint);e.setTextSize(16);e.setMinHeight(dp(50));e.setPadding(dp(12),dp(10),dp(12),dp(10));e.setBackground(background(PANEL));e.setInputType(InputType.TYPE_CLASS_TEXT|InputType.TYPE_TEXT_FLAG_NO_SUGGESTIONS|(multi?InputType.TYPE_TEXT_FLAG_MULTI_LINE:0));e.setSingleLine(!multi);e.setSaveEnabled(false);e.setImportantForAutofill(View.IMPORTANT_FOR_AUTOFILL_NO);parent.addView(e,new LinearLayout.LayoutParams(-1,-2));return e;}
    private GradientDrawable background(int color){GradientDrawable g=new GradientDrawable();g.setColor(color);g.setCornerRadius(dp(12));g.setStroke(dp(1),0xff284359);return g;}
    private int dp(int n){return Math.round(n*getResources().getDisplayMetrics().density);}
    private TextWatcher watcher(Runnable r){return new TextWatcher(){public void beforeTextChanged(CharSequence s,int a,int c,int f){}public void onTextChanged(CharSequence s,int start,int before,int count){r.run();}public void afterTextChanged(Editable e){}};}
    @Override protected void onSaveInstanceState(Bundle out){super.onSaveInstanceState(out);out.putString("machine",machine.getText().toString());out.putString("customer",customer.getText().toString());out.putString("contact",contact.getText().toString());out.putString("source",source.getText().toString());out.putString("remark",remark.getText().toString());}
    @Override protected void onDestroy(){afterAuth=null;pendingRecord=null;pendingKey=null;worker.execute(store::close);worker.shutdown();super.onDestroy();}
}

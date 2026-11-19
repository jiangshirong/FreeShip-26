// SPDX-License-Identifier: GPL-2.0-or-later
// The MSI engine remains responsible for file/component ownership. This outer
// transaction keeps a verified previous package and repairs failed per-user
// MSI rollback without editing MSI-owned registry keys or changing permissions.
using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Security.Cryptography;
using System.Text;
using System.Threading;
using System.Web.Script.Serialization;
using System.Windows.Forms;
using Microsoft.Win32;

public sealed class Manifest {
    public string product_code, upgrade_code, version, name, folder, registry, test_id, msi_sha256, maintenance_sha256;
    public Dictionary<string,string> payload_sha256;
}
public sealed class Installed {
    public string product, directory, installed_utc, previous;
}
public sealed class Transaction {
    public Installed before;
    public string candidate, directory, operation;
}
static class Setup {
    static readonly JavaScriptSerializer Json=new JavaScriptSerializer { MaxJsonLength=8*1024*1024 };
    static readonly Dictionary<string,object> Report=new Dictionary<string,object>();
    static readonly List<object> Operations=new List<object>();
    static readonly string Self=Assembly.GetExecutingAssembly().Location;
    static string Cache, ReportPath, Target, Stage;
    static bool Quiet, Uninstall, RecoverOnly, Inspect, Test;
    static Action<string> Status=delegate(string text) {};
    static string T(string text) { return InstallerLanguage.Text(text); }
    static Manifest Bundle;
    const string ArpPrefix=@"Software\Microsoft\Windows\CurrentVersion\Uninstall\FreeShip26Qt-";
    [DllImport("msi.dll",CharSet=CharSet.Unicode)] static extern uint MsiInstallProductW(string package,string properties);
    [DllImport("msi.dll",CharSet=CharSet.Unicode)] static extern uint MsiConfigureProductExW(string code,int level,int state,string properties);
    [DllImport("msi.dll",CharSet=CharSet.Unicode)] static extern int MsiQueryProductStateW(string code);
    [DllImport("msi.dll",CharSet=CharSet.Unicode)] static extern uint MsiEnumRelatedProductsW(string code,uint reserved,uint index,StringBuilder product);
    [DllImport("msi.dll")] static extern uint MsiSetInternalUI(uint level,IntPtr owner);
    [DllImport("msi.dll",CharSet=CharSet.Unicode)] static extern uint MsiEnableLogW(uint mode,string path,uint attributes);
    [DllImport("msi.dll",CharSet=CharSet.Unicode)] static extern uint MsiOpenDatabaseW(string path,IntPtr persist,out uint db);
    [DllImport("msi.dll",CharSet=CharSet.Unicode)] static extern uint MsiDatabaseOpenViewW(uint db,string sql,out uint view);
    [DllImport("msi.dll")] static extern uint MsiViewExecute(uint view,uint record);
    [DllImport("msi.dll")] static extern uint MsiViewFetch(uint view,out uint record);
    [DllImport("msi.dll",CharSet=CharSet.Unicode)] static extern uint MsiRecordGetStringW(uint record,uint field,StringBuilder value,ref uint size);
    [DllImport("msi.dll")] static extern uint MsiCloseHandle(uint handle);
    static string Braces(string code) { return "{"+new Guid(code).ToString().ToUpperInvariant()+"}"; }
    static string FileName(string product,string suffix) { return Path.Combine(Cache,new Guid(product).ToString().ToUpperInvariant()+suffix); }
    static string StatePath { get { return Path.Combine(Cache,"installed.json"); } }
    static string JournalPath { get { return Path.Combine(Cache,"transaction.json"); } }
    static string Arp { get { return ArpPrefix+Identity.Family; } }
    static string Hash(string path) { using(var f=File.OpenRead(path)) using(var sha=SHA256.Create()) return BitConverter.ToString(sha.ComputeHash(f)).Replace("-","").ToLowerInvariant(); }
    static T Read<T>(string path) { return Json.Deserialize<T>(File.ReadAllText(path,Encoding.UTF8)); }
    static void NoLinks(string path) {
        for(var p=Path.GetFullPath(path);!String.IsNullOrEmpty(p);p=Path.GetDirectoryName(p))
            if((File.Exists(p)||Directory.Exists(p))&&(File.GetAttributes(p)&FileAttributes.ReparsePoint)!=0) throw new IOException(T("安装目录和缓存不能经过链接：")+p);
    }
    static void Atomic(string path,byte[] data) {
        NoLinks(path);Directory.CreateDirectory(Path.GetDirectoryName(path));
        string temp=path+"."+Guid.NewGuid().ToString("N")+".tmp";
        using(var f=new FileStream(temp,FileMode.CreateNew,FileAccess.Write,FileShare.None,65536,FileOptions.WriteThrough)) { f.Write(data,0,data.Length);f.Flush(true); }
        if(File.Exists(path)) File.Replace(temp,path,null);else File.Move(temp,path);
    }
    static void Save(string path,object value) { Atomic(path,Encoding.UTF8.GetBytes(Json.Serialize(value))); }
    static byte[] Resource(string name) { using(var s=Assembly.GetExecutingAssembly().GetManifestResourceStream(name)) { if(s==null)return null;using(var m=new MemoryStream()) {s.CopyTo(m);return m.ToArray();} } }
    static void Check(uint code) { if(code!=0)throw new IOException(T("Windows Installer API 错误：")+code); }
    static string Property(uint db,string name) {
        uint view=0,record=0;
        try {Check(MsiDatabaseOpenViewW(db,"SELECT `Value` FROM `Property` WHERE `Property`='"+name+"'",out view));Check(MsiViewExecute(view,0));Check(MsiViewFetch(view,out record));uint n=4096;var text=new StringBuilder((int)n);Check(MsiRecordGetStringW(record,1,text,ref n));return text.ToString();}
        finally {if(record!=0)MsiCloseHandle(record);if(view!=0)MsiCloseHandle(view);}
    }
    static string PayloadPath(string directory,string name) {
        if(String.IsNullOrEmpty(name)||name.Contains('\\')||name.Contains(':')||name.Split('/').Any(p=>p==""||p=="."||p==".."))throw new IOException(T("无效的安装文件名"));
        var path=Path.GetFullPath(Path.Combine(directory,name.Replace('/',Path.DirectorySeparatorChar)));
        if(!path.StartsWith(directory.TrimEnd('\\')+"\\",StringComparison.OrdinalIgnoreCase))throw new IOException(T("安装文件超出目标目录"));
        NoLinks(path);return path;
    }
    static Manifest Validate(string product) {
        var meta=FileName(product,".json");NoLinks(meta);var m=Read<Manifest>(meta);
        if(new Guid(m.product_code)!=new Guid(product)||new Guid(m.upgrade_code)!=new Guid(Identity.Family))throw new IOException(T("安装包身份不一致"));
        var package=FileName(product,".msi");NoLinks(package);
        if(Hash(package)!=m.msi_sha256)throw new IOException(T("恢复安装包的校验失败；现有程序未改动"));
        var engine=FileName(product,".exe");NoLinks(engine);
        if(Hash(engine)!=m.maintenance_sha256)throw new IOException(T("恢复维护程序的校验失败；现有程序未改动"));
        uint db=0;Check(MsiOpenDatabaseW(package,IntPtr.Zero,out db));
        try {if(Property(db,"ProductCode")!=Braces(product)||Property(db,"UpgradeCode")!=Braces(Identity.Family)||Property(db,"ProductVersion")!=m.version)throw new IOException(T("安装包内部身份不一致"));}
        finally {MsiCloseHandle(db);}
        if(m.payload_sha256==null||m.payload_sha256.Count==0)throw new IOException(T("缺少安装文件清单"));
        foreach(var name in m.payload_sha256.Keys)PayloadPath(Cache,name);
        return m;
    }
    static List<string> Related() {
        var result=new List<string>();for(uint i=0;;i++) {var s=new StringBuilder(39);var code=MsiEnumRelatedProductsW(Braces(Identity.Family),0,i,s);if(code==259)break;Check(code);result.Add(new Guid(s.ToString()).ToString().ToUpperInvariant());}return result;
    }
    static int State(Manifest m) {return MsiQueryProductStateW(Braces(m.product_code));}
    static void Verify(Manifest m,string directory) {
        if(State(m)!=5)throw new IOException(T("安装登记未恢复到已安装状态"));
        foreach(var pair in m.payload_sha256) {var path=PayloadPath(directory,pair.Key);if(!File.Exists(path)||Hash(path)!=pair.Value)throw new IOException(T("安装文件校验失败：")+pair.Key);}
        using(var key=Registry.CurrentUser.OpenSubKey(m.registry))
            if(key==null||!String.Equals(Path.GetFullPath((string)key.GetValue("InstallDir","")).TrimEnd('\\'),directory.TrimEnd('\\'),StringComparison.OrdinalIgnoreCase))throw new IOException(T("安装位置登记不一致"));
        var shortcut=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs),m.name,m.name+".lnk");
        if(!File.Exists(shortcut))throw new IOException(T("开始菜单入口未恢复"));
    }
    static uint Msi(string phase,Manifest m,string directory,bool remove,bool repair) {
        Stage=phase;Status(phase);string log=Path.Combine(Cache,"last-msi.log");
        Check(MsiEnableLogW(Test?0x7ffu:7u,log,0));var watch=Stopwatch.StartNew();uint code;
        try {code=remove?MsiConfigureProductExW(Braces(m.product_code),0,2,"REBOOT=ReallySuppress FREESHIP_SETUP=1"):
            MsiInstallProductW(FileName(m.product_code,".msi"),"INSTALLDIR=\""+directory.TrimEnd('\\')+"\" REBOOT=ReallySuppress FREESHIP_SETUP=1"+(repair?" REINSTALL=ALL REINSTALLMODE=amusv":""));}
        finally {MsiEnableLogW(0,null,0);}
        Operations.Add(new {phase=phase,product=m.product_code,code=code,seconds=watch.Elapsed.TotalSeconds});
        if(Test&&ReportPath!=null&&File.Exists(log))File.Copy(log,Path.Combine(Path.GetDirectoryName(ReportPath),Path.GetFileNameWithoutExtension(ReportPath)+"-"+Operations.Count+".msi.log"),true);
        if(code==3010)Report["reboot_required"]=true;
        return code;
    }
    static void Successful(uint code) {if(code!=0&&code!=3010)throw new IOException(T("Windows Installer 返回 ")+code);}
    static void Register(Installed installed,Manifest m) {
        using(var key=Registry.CurrentUser.CreateSubKey(Arp)) {
            key.SetValue("DisplayName",m.name);key.SetValue("DisplayVersion",m.version);key.SetValue("Publisher","FreeShip 26");
            key.SetValue("InstallLocation",installed.directory);key.SetValue("DisplayIcon",Path.Combine(installed.directory,"FreeShipQt.exe"));
            key.SetValue("UninstallString","\""+FileName(installed.product,".exe")+"\" --uninstall");
            key.SetValue("QuietUninstallString","\""+FileName(installed.product,".exe")+"\" --uninstall --quiet");
            key.SetValue("ModifyPath","\""+FileName(installed.product,".exe")+"\"");key.SetValue("NoModify",1,RegistryValueKind.DWord);
            key.SetValue("InstallDate",installed.installed_utc);key.SetValue("Comments",T("升级失败时自动恢复此前安装版本。用户模型和偏好在卸载后保留。"));
        }
    }
    static void Recover(Transaction tx) {
        Report["recovery_attempted"]=true;Status(T("正在恢复此前安装状态，请稍候……"));
        var next=Validate(tx.candidate);var old=tx.before==null?null:Validate(tx.before.product);
        // Both packages are checked before removing any product. State 1 is an
        // advertised remnant, not an absent product; it must also be removed.
        if(State(next)!=-1)Successful(Msi(T("恢复：移除失败版本"),next,tx.directory,true,false));
        if(old!=null) {
            if(State(old)!=-1)Successful(Msi(T("恢复：修复旧登记"),old,tx.directory,true,false));
            Successful(Msi(T("恢复：还原旧版本"),old,tx.before.directory,false,false));Verify(old,tx.before.directory);
            foreach(var name in next.payload_sha256.Keys.Except(old.payload_sha256.Keys))if(File.Exists(PayloadPath(tx.directory,name)))throw new IOException(T("失败版本仍留有安装文件：")+name);
            Register(tx.before,old);Save(StatePath,tx.before);
            if(!Related().SequenceEqual(new[]{old.product_code}))throw new IOException(T("恢复后仍有多余产品登记"));
        } else {
            if(Related().Count!=0||next.payload_sha256.Keys.Any(name=>File.Exists(PayloadPath(tx.directory,name))))throw new IOException(T("首次安装失败后仍有残留"));
            Registry.CurrentUser.DeleteSubKeyTree(Arp,false);if(File.Exists(StatePath))File.Delete(StatePath);
        }
        File.Delete(JournalPath);Report["recovered"]=true;
    }
    static void Trim(Installed installed) {
        // Only known cached package files are eligible. No recursive removal of
        // application directories, unknown files or user data.
        var keep=installed==null?new string[0]:new[]{installed.product,installed.previous};
        foreach(var path in Directory.GetFiles(Cache,"*.json")) {
            Guid id;if(!Guid.TryParse(Path.GetFileNameWithoutExtension(path),out id)||keep.Any(k=>String.Equals(k,id.ToString(),StringComparison.OrdinalIgnoreCase)))continue;
            try {
                NoLinks(path);var m=Read<Manifest>(path);
                if(new Guid(m.product_code)!=id||new Guid(m.upgrade_code)!=new Guid(Identity.Family))continue;
                var package=FileName(m.product_code,".msi");var engine=FileName(m.product_code,".exe");NoLinks(engine);NoLinks(package);
                if(File.Exists(package)) {Validate(m.product_code);File.Delete(package);}
                // The running uninstaller cannot delete itself. Keep its tiny,
                // hash-verified record so a later setup can finish this cleanup.
                if(String.Equals(engine,Self,StringComparison.OrdinalIgnoreCase))continue;
                if(File.Exists(engine)) {if(Hash(engine)!=m.maintenance_sha256)continue;File.Delete(engine);}
                File.Delete(path);
            } catch(Exception ex) {Report["cache_cleanup_note"]=ex.Message;}
        }
        // The tiny running maintenance executable can remain after uninstall;
        // the large MSI payloads cannot. One latest MSI log is bounded as well.
        try {var log=Path.Combine(Cache,"last-msi.log");if(File.Exists(log)&&new FileInfo(log).Length>1024*1024)File.Delete(log);}catch(IOException) {}
    }
    static void Preflight(Installed installed,Manifest next,string directory) {
        NoLinks(directory);if(directory.Contains('"')||directory.Contains('\n')||directory.TrimEnd('\\')==Path.GetPathRoot(directory).TrimEnd('\\'))throw new IOException(T("请选择有效的程序目录"));
        var old=installed==null?null:Validate(installed.product);
        var related=Related();if(related.Any(p=>installed==null||p!=installed.product))throw new IOException(T("发现本产品的其他安装登记，请先恢复原安装记录"));
        if(old!=null&&State(old)!=5)throw new IOException(T("当前安装登记不完整，无法开始新升级"));
        foreach(var name in next.payload_sha256.Keys) {
            var path=PayloadPath(directory,name);
            if(File.Exists(path)&&(old==null||!old.payload_sha256.ContainsKey(name)))throw new IOException(T("目标目录已有未由本安装器管理的同名文件：")+name);
        }
        foreach(var p in Process.GetProcesses())using(p) {try {if(p.Id!=Process.GetCurrentProcess().Id&&p.MainModule.FileName.StartsWith(directory.TrimEnd('\\')+"\\",StringComparison.OrdinalIgnoreCase))throw new IOException(T("请先关闭安装目录中正在运行的程序"));}catch(Win32Exception) {}catch(InvalidOperationException) {}}
        if(old!=null)foreach(var name in old.payload_sha256.Keys) {var path=PayloadPath(directory,name);if(File.Exists(path))using(var f=new FileStream(path,FileMode.Open,FileAccess.Read,FileShare.None)) {}}
        var drive=new DriveInfo(Path.GetPathRoot(directory));if(drive.AvailableFreeSpace<300*1024*1024)throw new IOException(T("安装磁盘至少需要 300 MiB 可用空间"));
    }
    static int Execute() {
        bool acquired=false;using(var gate=new Mutex(false,"Local\\FreeShip26Setup-"+Identity.Family)) {
            try {
                try {acquired=gate.WaitOne(0);}catch(AbandonedMutexException) {acquired=true;}
                if(!acquired)throw new IOException(T("另一个安装或恢复任务正在运行"));
                NoLinks(Cache);Directory.CreateDirectory(Cache);MsiSetInternalUI(2,IntPtr.Zero);
                if(File.Exists(JournalPath)) {Recover(Read<Transaction>(JournalPath));Report["status"]="recovered_interrupted";Trim(File.Exists(StatePath)?Read<Installed>(StatePath):null);return 0;}
                if(RecoverOnly) {Report["status"]="no_pending_transaction";return 0;}
                var installed=File.Exists(StatePath)?Read<Installed>(StatePath):null;
                var next=Bundle;
                if(next==null) {if(installed==null)throw new IOException(T("没有可维护的安装"));next=Validate(installed.product);}
                Target=installed==null?(Target??Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Programs",next.folder)):installed.directory;
                Target=Path.GetFullPath(Target).TrimEnd('\\');
                if(installed!=null&&!Uninstall&&new Version(next.version)<new Version(Validate(installed.product).version)) {Report["status"]="downgrade_rejected";return 1638;}
                Preflight(installed,next,Target);
                bool migrateProfile=false;
                if(!Uninstall&&installed!=null&&Bundle!=null){var oldMarker=Path.Combine(Target,"build-manifest.json");
                    if(File.Exists(oldMarker)){var oldInfo=Read<Dictionary<string,object>>(oldMarker);migrateProfile=!oldInfo.ContainsKey("settings_profile");}}

                if(Bundle!=null&&!Uninstall) {
                    string file=FileName(next.product_code,".msi");var bytes=Resource("payload.msi");
                    if(File.Exists(file)&&Hash(file)!=next.msi_sha256)throw new IOException(T("同版本安装包已改变，不能覆盖已有恢复包"));
                    var engine=FileName(next.product_code,".exe");
                    if(File.Exists(engine)&&Hash(engine)!=next.maintenance_sha256)throw new IOException(T("同版本维护程序已改变，不能覆盖已有恢复程序"));
                    if(!File.Exists(file))Atomic(file,bytes);
                    if(!File.Exists(engine))Atomic(engine,Resource("maintenance.exe"));
                    Save(FileName(next.product_code,".json"),next);Validate(next.product_code);
                }
                if(Uninstall&&installed==null)throw new IOException(T("没有可卸载的安装"));
                if(Uninstall)next=Validate(installed.product);
                var tx=new Transaction {before=installed,candidate=next.product_code,directory=Target,operation=Uninstall?"uninstall":"install"};
                Save(JournalPath,tx);
                try {
                    Successful(Msi(Uninstall?T("正在卸载程序……"):T("正在安装程序……"),next,Target,Uninstall,!Uninstall&&installed!=null&&installed.product==next.product_code));
                    if(Uninstall) {
                        if(State(next)!=-1||next.payload_sha256.Keys.Any(n=>File.Exists(PayloadPath(Target,n))))throw new IOException(T("卸载后仍有程序文件或安装登记"));
                        Registry.CurrentUser.DeleteSubKeyTree(Arp,false);File.Delete(StatePath);installed=null;
                    } else {
                        Verify(next,Target);
                        installed=new Installed {product=next.product_code,directory=Target,installed_utc=DateTime.UtcNow.ToString("yyyyMMdd"),previous=installed==null?null:(installed.product==next.product_code?installed.previous:installed.product)};
                        Register(installed,next);Save(StatePath,installed);
                        if(Test&&Identity.Fault=="after-msi")throw new IOException("Regression failure after MSI and outer registration commit");
                    }
                    File.Delete(JournalPath);Report["status"]=Uninstall?"uninstalled":"installed";Trim(installed);
                    // Optional user actions run after commit; their failure must never roll back a verified MSI.
                    try {
                        if(Uninstall){DesktopLink(false);if(ResetRequested)ResetPersonalFiles();}
                        else {
                            if(migrateProfile)ImportLegacyPreferences();
                            if(tx.before==null) {
                                try { SaveInitialLanguage(); }
                                catch(Exception ex) { Report["installer_language_error"]=ex.Message; }
                            }
                            if(CreateShortcut)DesktopLink(true);
                        }
                    } catch(Exception ex){Report["post_install_error"]=ex.Message;return 1;}
                    return 0;
                } catch(Exception ex) {
                    Report["installation_error"]=ex.Message;
                    Report["states_before_recovery"]=new {candidate=State(next),previous=tx.before==null?-1:State(Validate(tx.before.product))};
                    Report["outer_state_before_recovery"]=File.Exists(StatePath)?Read<Installed>(StatePath):null;
                    try {Recover(tx);Report["status"]="failed_restored";Trim(tx.before);}
                    catch(Exception recovery) {Report["status"]="recovery_required";Report["recovery_error"]=recovery.Message;}
                    return 1603;
                }
            } catch(Exception ex) {Report["status"]="rejected";Report["error"]=ex.Message;return 1603;}
            finally {if(acquired)gate.ReleaseMutex();}
        }
    }
    static string DataRoot(bool legacy) {
        return Test?Path.Combine(Cache,legacy?"legacy-test-profile":"installed-test-profile"):
            Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),legacy?"FreeShip26":"FreeShip26Installed","FreeShip_26 Qt");
    }
    const string InstallerLanguageFile="installer-language.json";
    static readonly string[] PersonalFiles={"preferences.json","recent-files.json"};
    static void SaveInitialLanguage() {
        var profile=DataRoot(false);
        var preferences=Path.Combine(profile,"preferences.json");
        NoLinks(preferences);
        // Seed the chooser, without accepting it on behalf of the user.
        if(!File.Exists(preferences))Save(Path.Combine(profile,InstallerLanguageFile),new {language=InstallerLanguage.Id});
    }
    static void ImportLegacyPreferences() {
        // Only on upgrade from a registered legacy build. Never move or erase the development profile.
        foreach(var n in PersonalFiles) {
            var src=Path.Combine(DataRoot(true),n);var dst=Path.Combine(DataRoot(false),n);
            NoLinks(src);NoLinks(dst);if(File.Exists(src)&&!File.Exists(dst)){Directory.CreateDirectory(DataRoot(false));File.Copy(src,dst,false);}
        }
    }
    static void ResetPersonalFiles() {
        foreach(var n in PersonalFiles.Concat(new[]{InstallerLanguageFile})){var f=Path.Combine(DataRoot(false),n);NoLinks(f);if(File.Exists(f))File.Delete(f);}
    }
    static void DesktopLink(bool create) {
        var directory=Test?Path.Combine(Cache,"test-desktop"):Environment.GetFolderPath(Environment.SpecialFolder.DesktopDirectory);
        if(String.IsNullOrEmpty(directory))throw new DirectoryNotFoundException();
        var dest=Path.Combine(directory,Identity.Name+".lnk");var receipt=Path.Combine(Cache,"desktop-link.sha256");
        NoLinks(dest);NoLinks(receipt);
        bool owned=File.Exists(dest)&&File.Exists(receipt)&&Hash(dest)==File.ReadAllText(receipt);
        if(!create){if(owned)File.Delete(dest);File.Delete(receipt);return;}
        var src=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Programs),Identity.Name,Identity.Name+".lnk");
        if(File.Exists(dest)&&!owned){if(Hash(dest)==Hash(src))return;throw new IOException(T("桌面已有其他同名快捷方式。"));}
        Directory.CreateDirectory(directory);File.Copy(src,dest,owned);File.WriteAllText(receipt,Hash(dest));
    }
    static bool ResetRequested, CreateShortcut;
    static int Run() {
        var code=Execute();Report["exit_code"]=code;Report["operations"]=Operations;Report["cache"]=Cache;Report["directory"]=Target;
        Report["stage"]=Stage;Report["utc"]=DateTime.UtcNow.ToString("o");
        if(ReportPath!=null)Save(ReportPath,Report);
        try {Save(Path.Combine(Cache,"last-result.json"),Report);}catch(IOException) {}
        return code;
    }
    static void CaptureReview(Form form,string filename) {
        using(var bitmap=new System.Drawing.Bitmap(form.Width,form.Height)) {
            form.DrawToBitmap(bitmap,new System.Drawing.Rectangle(0,0,form.Width,form.Height));
            bitmap.Save(Path.Combine(Path.GetDirectoryName(ReportPath),filename),System.Drawing.Imaging.ImageFormat.Png);
        }
    }
    [STAThread] static int Main(string[] args) {
        try {
            Quiet=Array.IndexOf(args,"--quiet")>=0;
            // Resolve the requested language before parsing any other argument
            // so argument and recovery errors use the same selected language.
            for(int i=0;i+1<args.Length;i++)if(args[i]=="--language")InstallerLanguage.Select(args[i+1]);
            bool observeLanguageUi=false;
            var raw=Resource("manifest.json");Bundle=raw==null?null:Json.Deserialize<Manifest>(Encoding.UTF8.GetString(raw));
            Test=Identity.TestId!="";Cache=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"FreeShip26","InstallerCache",Identity.Family);
            for(int i=0;i<args.Length;i++)switch(args[i]) {
                case "--quiet":Quiet=true;break;case "--uninstall":Uninstall=true;break;case "--recover":RecoverOnly=true;break;case "--inspect":Inspect=true;break;
                case "--language":InstallerLanguage.Select(args[++i]);break;
                case "--observe-language-ui":if(!Test)throw new IOException("Only private test builds may observe installer controls");observeLanguageUi=true;break;
                case "--install-dir":Target=Path.GetFullPath(args[++i]);break;
                case "--report":ReportPath=Path.GetFullPath(args[++i]);break;
                case "--cache-dir":if(!Test)throw new IOException(T("仅测试身份允许指定缓存目录"));Cache=Path.GetFullPath(args[++i]);break;
                default:throw new IOException(T("未知安装参数：")+args[i]);
            }
            if(Inspect) {
                if(Bundle==null||ReportPath==null)throw new IOException(T("检查安装包需要完整安装器及报告路径"));
                using(var sha=SHA256.Create())Save(ReportPath,new {manifest=Bundle,embedded_msi_sha256=BitConverter.ToString(sha.ComputeHash(Resource("payload.msi"))).Replace("-","").ToLowerInvariant(),test_identity=Test,fault=Identity.Fault});
                return 0;
            }
            if(raw==null&&Test)Cache=Path.GetDirectoryName(Self);
            if(Quiet)return Run();
            Application.EnableVisualStyles();Application.SetCompatibleTextRenderingDefault(false);
            int result=1602;bool running=false,finished=false;
            bool locked=Uninstall||File.Exists(StatePath);
            string destination=Target??(File.Exists(StatePath)?Read<Installed>(StatePath).directory:Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"Programs",Bundle==null?"FreeShip26Qt":Bundle.folder));
            using(var form=new InstallerView(Uninstall,locked,destination)) {
                var path=form.Destination;var browse=form.Browse;var language=form.Language;
                var shortcut=form.Shortcut;var launch=form.Launch;var reset=form.Reset;var start=form.ActionButton;
                browse.Click+=delegate {
                    using(var picker=new FolderBrowserDialog {Description=T("选择安装文件夹"),ShowNewFolderButton=true}) {
                        // An initial destination need not exist yet. Open at its nearest existing parent.
                        try {
                            var dir=new DirectoryInfo(path.Text);
                            while(dir!=null&&!dir.Exists)dir=dir.Parent;
                            if(dir!=null)picker.SelectedPath=dir.FullName;
                        } catch(ArgumentException) {} catch(IOException) {} catch(NotSupportedException) {}
                        if(picker.ShowDialog(form)==DialogResult.OK)path.Text=picker.SelectedPath;
                    }
                };
                foreach(var option in InstallerLanguage.Options) {language.Items.Add(option);if(option.Id==InstallerLanguage.Id)language.SelectedItem=option;}
                Action applyLanguage=delegate {
                    form.Relabel(Identity.Name+(Uninstall?T(" 卸载"):T(" 安装")),
                        Uninstall?T("卸载"):T("安装设置"),
                        T("安装语言"),
                        T("安装位置"), T("浏览…"),
                        T("创建桌面快捷方式"),T("安装完成后打开 FreeShip 26"),T("删除个人设置和最近文件记录（保留船模）"),
                        Uninstall?T("卸载"):T("安装"));
                    if(finished)form.ShowFeedback(result==0?(Report.ContainsKey("status")&&(string)Report["status"]=="recovered_interrupted"?T("上次安装被中断，已恢复此前版本。"):Uninstall?T("卸载完成，用户文件和设置已保留。"):T("操作完成。")):
                        Report.ContainsKey("recovered")?T("本次安装未完成，已恢复此前安装状态。"):T("操作未完成，请查看安装日志：")+Path.Combine(Cache,"last-result.json"),result!=0);
                    else form.ShowFeedback(Uninstall?T("卸载程序并保留船模和偏好设置。"):"",false);
                };
                language.SelectedIndexChanged+=delegate {if(language.SelectedItem!=null){InstallerLanguage.Select(((InstallerLanguage.Option)language.SelectedItem).Id);applyLanguage();}};
                applyLanguage();
                form.FormClosing+=delegate(object sender,FormClosingEventArgs e) {if(running)e.Cancel=true;};
                Status=delegate(string text) {form.BeginInvoke((Action)delegate {if(running)form.ShowFeedback(text,false);});};
                start.Click+=delegate {
                    try {Target=Path.GetFullPath(path.Text);}catch(Exception ex) {form.ShowFeedback(T("目录无效：")+ex.Message,true);return;}
                    ResetRequested=Uninstall&&reset.Checked;CreateShortcut=!Uninstall&&shortcut.Checked;
                    running=true;form.SetRunning(true);form.ShowFeedback("",false);
                    var worker=new BackgroundWorker();worker.DoWork+=delegate {result=Run();};worker.RunWorkerCompleted+=delegate(object sender,RunWorkerCompletedEventArgs e) {
                        running=false;finished=true;form.SetRunning(false);
                        start.Enabled=false;
                        if(e.Error!=null) {result=1603;form.ShowFeedback(e.Error.GetBaseException().Message,true);return;}
                        applyLanguage();
                        if(result==0){
                            if(!Uninstall&&launch.Checked&&Convert.ToString(Report["status"])=="installed"){
                                try {Process.Start(new ProcessStartInfo(Path.Combine(Target,"FreeShipQt.exe")){WorkingDirectory=Target,UseShellExecute=false});}
                                catch(Exception ex){MessageBox.Show(form,ex.Message,Identity.Name,MessageBoxButtons.OK,MessageBoxIcon.Warning);}
                            }
                            form.Close();return;
                        }
                        if(Report.ContainsKey("post_install_error"))form.ShowFeedback(Convert.ToString(Report["post_install_error"]),true);
                    };worker.RunWorkerAsync();
                };
                if(observeLanguageUi) {
                    if(ReportPath==null)throw new IOException("Private UI observation requires a report path");
                    var states=new List<object>();form.Show();Application.DoEvents();
                    foreach(var option in InstallerLanguage.Options) {
                        language.SelectedItem=option;Application.DoEvents();
                        var intro=form.FeedbackText;finished=true;result=0;applyLanguage();var completed=form.FeedbackText;finished=false;applyLanguage();
                        form.SetRunning(true);form.ShowFeedback(T("正在安装程序……"),false);Application.DoEvents();
                        bool progressState=form.ShowsProgress&&!start.Visible&&!language.Enabled&&!path.Enabled;
                        if(option.Id=="zh")CaptureReview(form,"installer-zh-progress.png");
                        form.SetRunning(false);applyLanguage();Application.DoEvents();
                        states.Add(new {id=InstallerLanguage.Id,title=form.Text,heading=form.SectionCaption,intro=intro,completed=completed,start=start.Text,
                            language=language.AccessibleName,location=path.AccessibleName,target=path.Text,visible=form.Visible,progress_state=progressState,ready_state=!form.ShowsProgress&&start.Visible&&language.Enabled,
                            status_install=T("正在安装程序……"),status_recover=T("恢复：还原旧版本"),failure=T("操作未完成，请查看安装日志：")+"C:\\测试模型\\日志.json"});
                        CaptureReview(form,"installer-"+option.Id+".png");
                    }
                    Save(ReportPath,new {ui_states=states,operations=Operations,test_identity=Test});form.Close();return 0;
                }
                Application.Run(form);
            }
            return result;
        } catch(Exception ex) {if(!Quiet)MessageBox.Show(ex.Message,T("FreeShip 26 安装"),MessageBoxButtons.OK,MessageBoxIcon.Error);return 1603;}
    }
}

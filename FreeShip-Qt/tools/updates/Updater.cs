// SPDX-License-Identifier: GPL-2.0-or-later
// Network delta reconstruction only. MSI ownership/rollback stays with Setup.cs.
using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Globalization;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Reflection;
using System.Security.Cryptography;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading;
using System.Threading.Tasks;
using System.Web.Script.Serialization;

public sealed class Envelope { public string payload,signature; }
public sealed class Chunk { public string hash,packed_hash; public int size,packed_size; }
public sealed class Release { public int schema; public string family,version,expires,sha256,next_endpoint; public long size; public Chunk[] chunks; public object release_notes; }
public sealed class Source { public string path; public long offset; public int size; }
static class Updater {
    static readonly JavaScriptSerializer Json=new JavaScriptSerializer {MaxJsonLength=16*1024*1024};
    static readonly Dictionary<string,object> Report=new Dictionary<string,object>();
    static readonly uint[] Gear=MakeGear();
    static string Root,ReportPath,Base,Current="0.0.0",Endpoint,InstallDirectory,MainHash;
    static long Downloaded;
    static readonly Regex HashPattern=new Regex("^[a-f0-9]{64}$");
    static uint[] MakeGear() {var a=new uint[256];uint v=0x46533236;for(int i=0;i<a.Length;i++){v=unchecked(v*1664525+1013904223);a[i]=v;}return a;}
    static byte[] Bytes(string s) {return Encoding.UTF8.GetBytes(s);}
    static string Hash(byte[] data) {using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(data)).Replace("-","").ToLowerInvariant();}
    static string HashFile(string path) {using(var f=File.OpenRead(path))using(var sha=SHA256.Create())return BitConverter.ToString(sha.ComputeHash(f)).Replace("-","").ToLowerInvariant();}
    static void Plain(string path) {for(var p=Path.GetFullPath(path);p!=null;p=Path.GetDirectoryName(p))if((Directory.Exists(p)||File.Exists(p))&&(File.GetAttributes(p)&FileAttributes.ReparsePoint)!=0)throw new IOException("更新路径包含链接");}
    static void Atomic(string path,byte[] bytes) {
        Plain(path);Directory.CreateDirectory(Path.GetDirectoryName(path));string temp=path+".part-"+Guid.NewGuid().ToString("N");
        try {using(var f=new FileStream(temp,FileMode.CreateNew,FileAccess.Write,FileShare.None)){f.Write(bytes,0,bytes.Length);f.Flush(true);}
            if(File.Exists(path))File.Replace(temp,path,null);else File.Move(temp,path);
        } finally {if(File.Exists(temp))File.Delete(temp);}
    }
    static void SaveIfChanged(string path,byte[] bytes) {
        Plain(path);
        if(!File.Exists(path)||new FileInfo(path).Length!=bytes.Length||!File.ReadAllBytes(path).SequenceEqual(bytes))Atomic(path,bytes);
    }
    static byte[] Limited(Stream stream,long limit,Action<int> read=null) {
        using(var output=new MemoryStream()) {var buffer=new byte[65536];int n;
            while((n=stream.Read(buffer,0,buffer.Length))>0){if(output.Length+n>limit)throw new IOException("更新响应超过允许大小");output.Write(buffer,0,n);if(read!=null)read(n);}return output.ToArray();}
    }
    static Uri Address(string text) {
        Uri uri;if(!Uri.TryCreate(text,UriKind.Absolute,out uri)||uri.UserInfo!=""||uri.Fragment!=""||
            !(uri.Scheme=="https"||(Channel.Test&&uri.Scheme=="http"&&uri.IsLoopback)))throw new IOException("更新地址必须使用 HTTPS");return uri;
    }
    static byte[] Fetch(string url,int limit,Action<int> read=null,CancellationToken cancel=default(CancellationToken)) {
        var request=(HttpWebRequest)WebRequest.Create(Address(url));request.AllowAutoRedirect=false;
        request.Timeout=20000;request.ReadWriteTimeout=20000;request.UserAgent="FreeShip26-Updater/1";
        request.AutomaticDecompression=DecompressionMethods.None;
        using(cancel.Register(request.Abort))
        using(var response=(HttpWebResponse)request.GetResponse()) {
            if(response.StatusCode!=HttpStatusCode.OK||response.ContentLength>limit)throw new IOException("更新服务器返回无效响应");
            using(var stream=response.GetResponseStream())return Limited(stream,limit,n=>{
                Interlocked.Add(ref Downloaded,n);if(read!=null)read(n);
            });
        }
    }
    sealed class DownloadProgress : IDisposable {
        readonly object gate=new object();
        readonly Stopwatch clock=Stopwatch.StartNew();
        readonly Timer timer;
        readonly long total;
        long downloaded,network,lastNetwork,lastTick;
        double speed;
        bool closed;
        internal DownloadProgress(long totalBytes,long retainedBytes) {
            total=totalBytes;
            downloaded=retainedBytes;
            Emit("downloading");
            timer=new Timer(_=>Tick(),null,500,500);
        }
        internal void Read(int bytesRead) {
            lock(gate)network+=bytesRead;
        }
        internal void Commit(Chunk chunk) { lock(gate)downloaded+=chunk.packed_size; }
        internal long Retained { get { lock(gate)return downloaded; } }
        void Emit(string stage) {
            // The report has its own serializer. Worker threads never access it.
            var line=String.Format(CultureInfo.InvariantCulture,
                "{{\"stage\":\"{0}\",\"downloaded_bytes\":{1},\"total_bytes\":{2},\"bytes_per_second\":{3:0}}}",
                stage,downloaded,total,speed);
            try {Console.Out.WriteLine(line);Console.Out.Flush();}catch(IOException){}
        }
        void Tick() {
            lock(gate) {
                if(closed)return;
                long now=clock.ElapsedMilliseconds,elapsed=now-lastTick;
                if(elapsed<=0)return;
                double current=(network-lastNetwork)*1000.0/elapsed;
                speed=current==0?0:speed==0?current:.35*current+.65*speed;
                lastNetwork=network;lastTick=now;
                Emit("downloading");
            }
        }
        internal void Verifying() {
            lock(gate){closed=true;speed=0;Emit("verifying");}
        }
        public void Dispose() {
            lock(gate)closed=true;
            timer.Dispose();
        }
    }
    static Release Verify(byte[] bytes) {
        var envelope=Json.Deserialize<Envelope>(Encoding.UTF8.GetString(bytes));
        var data=Convert.FromBase64String(envelope.payload);var signature=Convert.FromBase64String(envelope.signature);
        using(var rsa=new RSACryptoServiceProvider()){rsa.PersistKeyInCsp=false;rsa.FromXmlString(Channel.PublicKey);
            if(rsa.KeySize<3072||!rsa.VerifyData(data,CryptoConfig.MapNameToOID("SHA256"),signature))throw new IOException("更新签名无效");}
        var release=Json.Deserialize<Release>(Encoding.UTF8.GetString(data));
        if(release.schema!=1||release.family!=Channel.Family.ToLowerInvariant()||!Regex.IsMatch(release.version??"","^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$")||
            release.size<2||release.size>1024L*1024*1024||!HashPattern.IsMatch(release.sha256??"")||release.chunks==null||release.chunks.Length==0||release.chunks.Length>65536)
            throw new IOException("更新清单无效");
        DateTimeOffset expires;if(!DateTimeOffset.TryParse(release.expires,CultureInfo.InvariantCulture,DateTimeStyles.None,out expires)||expires<=DateTimeOffset.UtcNow)throw new IOException("更新清单已过期");
        long total=0;var unique=new Dictionary<string,Chunk>();
        foreach(var c in release.chunks) {
            if(c.size<=0||c.size>262144||c.packed_size<=0||c.packed_size>263000||!HashPattern.IsMatch(c.hash??"")||!HashPattern.IsMatch(c.packed_hash??""))throw new IOException("更新分块无效");
            Chunk prior;if(unique.TryGetValue(c.hash,out prior)&&(prior.size!=c.size||prior.packed_hash!=c.packed_hash||prior.packed_size!=c.packed_size))throw new IOException("更新分块冲突");
            unique[c.hash]=c;total+=c.size;
        }
        if(total!=release.size)throw new IOException("更新文件长度不符");
        if(release.next_endpoint!=null)Address(release.next_endpoint);
        return release;
    }
    static IEnumerable<KeyValuePair<string,Source>> Index(string path) {
        if(path==null||!File.Exists(path))yield break;
        Plain(path);if(new FileInfo(path).Length>1024L*1024*1024)throw new IOException("本地更新基准过大");
        using(var f=File.OpenRead(path)) {
            byte[] buffer=new byte[65536],block=new byte[262144];int size=0,n;uint value=0;long offset=0;
            while((n=f.Read(buffer,0,buffer.Length))>0)for(int i=0;i<n;i++) {
                byte b=buffer[i];block[size++]=b;value=unchecked((value<<1)+Gear[b]);
                if(size>=16384&&((value&65535)==0||size==block.Length)) {
                    var part=new byte[size];Buffer.BlockCopy(block,0,part,0,size);
                    yield return new KeyValuePair<string,Source>(Hash(part),new Source{path=path,offset=offset,size=size});offset+=size;size=0;value=0;
                }
            }
            if(size>0){var part=new byte[size];Buffer.BlockCopy(block,0,part,0,size);yield return new KeyValuePair<string,Source>(Hash(part),new Source{path=path,offset=offset,size=size});}
        }
    }
    static byte[] Unpack(Chunk c,byte[] packed) {
        if(packed.Length!=c.packed_size||Hash(packed)!=c.packed_hash)throw new IOException("下载分块校验失败");
        using(var input=new MemoryStream(packed))using(var gzip=new GZipStream(input,CompressionMode.Decompress)) {
            var raw=Limited(gzip,c.size);if(raw.Length!=c.size||Hash(raw)!=c.hash)throw new IOException("解压分块校验失败");return raw;
        }
    }
    static string ChunkPath(Chunk c) {return Path.Combine(Root,"chunks",c.hash+".gz");}
    static bool Cached(Chunk c) {
        var path=ChunkPath(c);Plain(path);if(!File.Exists(path))return false;
        try {if(new FileInfo(path).Length!=c.packed_size)return false;Unpack(c,File.ReadAllBytes(path));return true;}
        catch(InvalidDataException){return false;}catch(IOException){return false;}
    }
    static void Installed() {
        var cache=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"FreeShip26","InstallerCache",Channel.Family);
        var state=Path.Combine(cache,"installed.json");if(!File.Exists(state))throw new IOException("请先使用 FreeShip 26 安装包安装，再使用自动更新");
        Plain(state);var s=Json.Deserialize<Dictionary<string,object>>(File.ReadAllText(state));
        string product=new Guid((string)s["product"]).ToString().ToUpperInvariant();
        var manifest=Path.Combine(cache,product+".json");Plain(manifest);
        var m=Json.Deserialize<Dictionary<string,object>>(File.ReadAllText(manifest));Current=(string)m["version"];
        if(!String.Equals((string)m["upgrade_code"],Channel.Family,StringComparison.OrdinalIgnoreCase))throw new IOException("安装产品标识不符");
        Base=Path.Combine(cache,product+".msi");
        InstallDirectory=Path.GetFullPath((string)s["directory"]);Plain(InstallDirectory);
        MainHash=(string)((Dictionary<string,object>)m["payload_sha256"])["FreeShipQt.exe"];
    }
    static Release Latest(CancellationToken cancel) {
        string remembered=Path.Combine(Root,"endpoint.txt");
        if(File.Exists(remembered)){Plain(remembered);Endpoint=Encoding.UTF8.GetString(File.ReadAllBytes(remembered));}
        byte[] bytes;
        using(var deadline=CancellationTokenSource.CreateLinkedTokenSource(cancel)){
            deadline.CancelAfter(8000);
            try { bytes=Fetch(Endpoint,16*1024*1024,null,deadline.Token); }
            catch(Exception) {
                if(!deadline.IsCancellationRequested||cancel.IsCancellationRequested)throw;
                Report["error_code"]="check_timeout";
                throw new IOException("检查更新超时，请重试。");
            }
        }
        var release=Verify(bytes);
        string seen=Path.Combine(Root,"highest.txt");Plain(seen);
        if(File.Exists(seen)&&new Version(release.version)<new Version(File.ReadAllText(seen)))throw new IOException("服务器返回了较旧的更新清单");
        SaveIfChanged(seen,Bytes(release.version));
        SaveIfChanged(Path.Combine(Root,"latest.json"),bytes);SaveIfChanged(Path.Combine(Root,"source.txt"),Bytes(Endpoint));
        // Only a signature from the embedded publisher key may move the endpoint.
        if(release.next_endpoint!=null)SaveIfChanged(remembered,Bytes(release.next_endpoint));
        return release;
    }
    static bool Ready(Release release) {
        var record=Path.Combine(Root,"ready.json");var setup=Path.Combine(Root,"setup.exe");Plain(record);Plain(setup);
        if(!File.Exists(record)||!File.Exists(setup)||new FileInfo(setup).Length!=release.size)return false;
        try {
            var prepared=Verify(File.ReadAllBytes(record));
            return prepared.version==release.version&&prepared.sha256==release.sha256&&HashFile(setup)==release.sha256;
        } catch(IOException) {return false;} catch(ArgumentException) {return false;} catch(InvalidOperationException) {return false;}
    }
    static void Prepare(Release release,bool download,CancellationToken cancel) {
        var available=new Dictionary<string,Source>();foreach(var item in Index(Base)){
            cancel.ThrowIfCancellationRequested();available[item.Key]=item.Value;
        }
        var unique=release.chunks.GroupBy(c=>c.hash).Select(g=>g.First()).ToArray();
        var requiredChunks=unique.Where(c=>!available.ContainsKey(c.hash)).ToArray();
        var missing=requiredChunks.Where(c=>{cancel.ThrowIfCancellationRequested();return !Cached(c);}).ToArray();
        long bytesToDownload=missing.Sum(c=>(long)c.packed_size);
        long total=requiredChunks.Sum(c=>(long)c.packed_size), retained=total-bytesToDownload;
        Report["download_bytes"]=bytesToDownload;
        Report["transfer_total_bytes"]=total;
        Report["downloaded_bytes"]=retained;
        Report["reused_bytes"]=release.chunks.Where(c=>available.ContainsKey(c.hash)).Sum(c=>(long)c.size);
        Report["full_download_bytes"]=unique.Sum(c=>(long)c.packed_size);
        if(!download&&Ready(release)){Report["status"]="ready";Report["downloaded_bytes"]=total;return;}
        var request=Path.Combine(Root,"download-request.txt");Plain(request);
        if(!download){
            // Shared chunks may have come from an older update. Keep them for
            // reconstruction, but only restore progress for this signed package.
            if(retained>0&&File.Exists(request)&&File.ReadAllText(request)==release.sha256)
                Report["status"]="paused";
            else Report["downloaded_bytes"]=0;
            return;
        }
        cancel.ThrowIfCancellationRequested();
        long required=release.size+bytesToDownload+512L*1024*1024;
        if(new DriveInfo(Path.GetPathRoot(Root)).AvailableFreeSpace<required)throw new IOException("更新磁盘空间不足");
        SaveIfChanged(request,Bytes(release.sha256));
        Directory.CreateDirectory(Path.Combine(Root,"chunks"));Plain(Path.Combine(Root,"chunks"));
        var keep=new HashSet<string>(unique.Select(c=>c.hash+".gz"));
        foreach(var p in Directory.GetFiles(Path.Combine(Root,"chunks"),"*.gz"))if(Regex.IsMatch(Path.GetFileName(p),"^[a-f0-9]{64}\\.gz$")&&!keep.Contains(Path.GetFileName(p))){Plain(p);File.Delete(p);}
        using(var progress=new DownloadProgress(total,retained)) {
            try {
                Parallel.ForEach(missing,new ParallelOptions {MaxDegreeOfParallelism=4,CancellationToken=cancel},c=> {
                    Exception error=null;
                    for(int attempt=0;attempt<3;attempt++)try {
                        cancel.ThrowIfCancellationRequested();
                        var packed=Fetch(new Uri(Address(Endpoint),"chunks/"+c.hash+".gz").AbsoluteUri,c.packed_size,progress.Read,cancel);
                        Unpack(c,packed);Atomic(ChunkPath(c),packed);progress.Commit(c);return;
                    } catch(WebException e){error=e;}catch(IOException e){error=e;}
                    cancel.ThrowIfCancellationRequested();
                    throw new IOException("下载失败，可重试继续下载",error);
                });
            } finally {Report["downloaded_bytes"]=progress.Retained;}
            cancel.ThrowIfCancellationRequested();
            progress.Verifying();
        }
        var target=Path.Combine(Root,"setup.exe");var temp=target+".building";Plain(temp);
        try {
            using(var output=new FileStream(temp,FileMode.Create,FileAccess.Write,FileShare.None))foreach(var c in release.chunks) {
                cancel.ThrowIfCancellationRequested();
                byte[] raw;Source source;
                if(available.TryGetValue(c.hash,out source)) {
                    using(var f=File.OpenRead(source.path)){f.Position=source.offset;raw=new byte[source.size];int at=0,n;while(at<raw.Length&&(n=f.Read(raw,at,raw.Length-at))>0)at+=n;
                        if(at!=raw.Length||raw.Length!=c.size||Hash(raw)!=c.hash)throw new IOException("本地更新基准在准备过程中发生变化");}
                } else raw=Unpack(c,File.ReadAllBytes(ChunkPath(c)));
                output.Write(raw,0,raw.Length);
            }
            if(new FileInfo(temp).Length!=release.size||HashFile(temp)!=release.sha256)throw new IOException("更新安装包完整校验失败");
            Plain(target);if(File.Exists(target))File.Replace(temp,target,null);else File.Move(temp,target);
            Atomic(Path.Combine(Root,"ready.json"),File.ReadAllBytes(Path.Combine(Root,"latest.json")));
            Report["status"]="ready";
        } finally {if(File.Exists(temp))File.Delete(temp);}
    }
    static bool WorkCopy;
    static void LocalIdentity(string mode) {
        string path=Path.Combine(AppDomain.CurrentDomain.BaseDirectory,"build-manifest.json");
        if(!File.Exists(path))return;
        var m=Json.Deserialize<Dictionary<string,object>>(File.ReadAllText(path));
        object role;
        if(!m.TryGetValue("local_installation_role",out role)||!Convert.ToString(role).StartsWith("development;"))return;
        WorkCopy=true;
        if(mode!="--check")throw new IOException("本机工作版仅检查更新，不执行用户版安装");
        Current=new Version((string)m["release_version"]).ToString();
        Root=Path.Combine(Root,"work-check");
    }
    static int Run(string mode,int parent,CancellationToken cancel) {
        if(String.IsNullOrEmpty(Channel.Endpoint)||String.IsNullOrEmpty(Channel.PublicKey)){Report["status"]="not_configured";return 0;}
        Plain(Root);Directory.CreateDirectory(Root);
        using(var gate=new FileStream(Path.Combine(Root,"lock"),FileMode.OpenOrCreate,FileAccess.ReadWrite,FileShare.None)) {
            if(mode=="--install") {
                if(parent<=0)throw new IOException("缺少等待退出的程序标识");
                try {using(var process=Process.GetProcessById(parent)){if(!process.WaitForExit(30*60*1000))throw new IOException("程序尚未退出，更新未安装");}}
                catch(ArgumentException){} // The calling app already exited.
                if(!Channel.Test)Installed();
                var ready=Path.Combine(Root,"ready.json");Plain(ready);var release=Verify(File.ReadAllBytes(ready));
                var exe=Path.Combine(Root,"setup.exe");Plain(exe);
                if(new Version(release.version)<=new Version(Current)||new FileInfo(exe).Length!=release.size||HashFile(exe)!=release.sha256)throw new IOException("待安装更新无效或已安装");
                if(Channel.Test)throw new IOException("测试更新程序不允许执行安装包");
                // The signed setup retains existing MSI transactions and rollback.
                using(var process=Process.Start(new ProcessStartInfo(exe,"--quiet"){UseShellExecute=false,CreateNoWindow=true})) {
                    process.WaitForExit();Report["installer_exit_code"]=process.ExitCode;
                    Report["status"]=process.ExitCode==0?"installed":"install_failed";
                    if(process.ExitCode!=0)Report["error"]="更新安装失败，请查看安装记录";
                    if(ReportPath!=null)Atomic(ReportPath,Bytes(Json.Serialize(Report)));
                    // Restart only the registered and hash-verified application,
                    // including a previous version successfully restored by MSI.
                    Installed();var app=Path.Combine(InstallDirectory,"FreeShipQt.exe");Plain(app);
                    if(HashFile(app)==MainHash)Process.Start(new ProcessStartInfo(app){UseShellExecute=false,WorkingDirectory=InstallDirectory});
                    return process.ExitCode;
                }
            }
            var latest=Latest(cancel);Report["version"]=latest.version;Report["current_version"]=Current;
            int comparison=new Version(latest.version).CompareTo(new Version(Current));
            if(comparison>=0&&latest.release_notes!=null)Report["release_notes"]=latest.release_notes;
            if(comparison<=0){Report["status"]="current";return 0;}
            Report["status"]="available";if(!WorkCopy)Prepare(latest,mode=="--download",cancel);return 0;
        }
    }
    [STAThread]static int Main(string[] args) {
        int code=1,parent=0;string pauseName=null,mode="--check";Root=Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.LocalApplicationData),"FreeShip26","Updates",Channel.Family);Endpoint=Channel.Endpoint;
        var cancel=new CancellationTokenSource();
        EventWaitHandle pauseEvent=null;
        RegisteredWaitHandle pauseWait=null;
        try {
            ServicePointManager.SecurityProtocol=SecurityProtocolType.Tls12;ServicePointManager.DefaultConnectionLimit=4;
            for(int i=0;i<args.Length;i++)switch(args[i]) {
                case "--check":case "--download":case "--install":mode=args[i];break;
                case "--pause-event":pauseName=args[++i];break;
                case "--report":ReportPath=Path.GetFullPath(args[++i]);break;
                case "--parent":parent=int.Parse(args[++i]);break;
                case "--root":if(!Channel.Test)throw new IOException("不支持此参数");Root=Path.GetFullPath(args[++i]);break;
                case "--base":if(!Channel.Test)throw new IOException("不支持此参数");Base=Path.GetFullPath(args[++i]);break;
                case "--current":if(!Channel.Test)throw new IOException("不支持此参数");Current=args[++i];break;
                default:throw new IOException("未知更新参数");
            }
            if(!Channel.Test)LocalIdentity(mode);
            if(!Channel.Test&&!WorkCopy&&!String.IsNullOrEmpty(Channel.Endpoint))Installed();
            if(pauseName!=null){
                if(mode!="--download"||!Regex.IsMatch(pauseName,@"\ALocal\\FreeShip26-Pause-[a-f0-9-]{36}\z"))
                    throw new IOException("无效的下载暂停信号");
                pauseEvent=EventWaitHandle.OpenExisting(pauseName);
                pauseWait=ThreadPool.RegisterWaitForSingleObject(pauseEvent,(_,timedOut)=>cancel.Cancel(),null,Timeout.Infinite,true);
            }
            code=Run(mode,parent,cancel.Token);
        } catch(Exception e){
            if(cancel.IsCancellationRequested){Report["status"]="paused";code=0;}
            else {Report["status"]="error";Report["error"]=e.GetBaseException().Message;}
        }
        finally {
            if(pauseWait!=null)pauseWait.Unregister(null);
            if(pauseEvent!=null)pauseEvent.Dispose();
            Report["network_bytes"]=Downloaded;
            if(ReportPath!=null)try{Atomic(ReportPath,Bytes(Json.Serialize(Report)));}catch{code=1;}
        }
        return code;
    }
}

// SPDX-License-Identifier: GPL-2.0-or-later
using System;
using System.Collections.Generic;
using System.Globalization;
using System.IO;
using System.Reflection;
using System.Web.Script.Serialization;

// Embedded in both the outer setup and cached maintenance executable. No
// installed application, network request or pre-existing locale file is needed.
static class InstallerLanguage {
    internal sealed class Option {
        internal string Id, Name;
        public override string ToString() { return Name; }
    }
    static readonly Dictionary<string,Dictionary<string,object>> Catalogs=new Dictionary<string,Dictionary<string,object>>();
    static readonly List<Option> Choices=new List<Option>();
    static string current="en";
    static InstallerLanguage() {
        using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("installer-languages.json")) {
            if(stream==null || stream.Length>4*1024*1024)throw new InvalidDataException("Missing or invalid embedded installer languages");
            using(var reader=new StreamReader(stream)) {
                var root=(Dictionary<string,object>)new JavaScriptSerializer().DeserializeObject(reader.ReadToEnd());
                foreach(var item in (object[])root["languages"]) {
                    var entry=(Dictionary<string,object>)item;
                    var id=(string)entry["id"];
                    Catalogs.Add(id,(Dictionary<string,object>)entry["messages"]);
                    Choices.Add(new Option {Id=id,Name=(string)entry["name"]});
                }
            }
        }
        if(!Catalogs.ContainsKey("zh") || !Catalogs.ContainsKey("en"))throw new InvalidDataException("Missing required installer language");
        var preferred=CultureInfo.CurrentUICulture.TwoLetterISOLanguageName;
        current=Catalogs.ContainsKey(preferred)?preferred:"en";
    }
    internal static string Id { get { return current; } }
    internal static Option[] Options { get { return Choices.ToArray(); } }
    internal static void Select(string id) {
        if(!Catalogs.ContainsKey(id))throw new IOException(Text("不支持的安装语言：")+id);
        current=id;
    }
    internal static string Text(string source) {
        if(current=="zh")return source;
        object value;
        if(Catalogs[current].TryGetValue(source,out value) && value is string && (string)value!="")return (string)value;
        if(Catalogs["en"].TryGetValue(source,out value) && value is string && (string)value!="")return (string)value;
        return source;
    }
}

// SPDX-License-Identifier: GPL-2.0-or-later
using System;
using System.IO;
using System.Runtime.InteropServices;
using System.Windows.Forms;

// Native Windows common-item dialog in folder mode. The address bar, navigation,
// new-folder command and shell shortcuts behave like Explorer.
static class InstallerFolderPicker {
    const uint PickFolders=0x20, ForceFileSystem=0x40, PathMustExist=0x800, DontAddToRecent=0x02000000;
    const int Cancelled=unchecked((int)0x800704C7);
    const uint FileSystemPath=0x80058000;

    internal static string Browse(IWin32Window owner,string caption,string currentPath) {
        IFileDialog dialog=(IFileDialog)new FileOpenDialog();
        IShellItem initial=null,selected=null;
        try {
            uint options;
            dialog.GetOptions(out options);
            dialog.SetOptions(options|PickFolders|ForceFileSystem|PathMustExist|DontAddToRecent);
            dialog.SetTitle(caption);
            string parent=NearestFolder(currentPath);
            if(parent!=null && SHCreateItemFromParsingName(parent,IntPtr.Zero,typeof(IShellItem).GUID,out initial)>=0)
                dialog.SetFolder(initial);
            int status=dialog.Show(owner.Handle);
            if(status==Cancelled)return null;
            Marshal.ThrowExceptionForHR(status);
            dialog.GetResult(out selected);
            IntPtr path;
            selected.GetDisplayName(FileSystemPath,out path);
            try {return Marshal.PtrToStringUni(path);}
            finally {Marshal.FreeCoTaskMem(path);}
        } finally {
            if(selected!=null)Marshal.ReleaseComObject(selected);
            if(initial!=null)Marshal.ReleaseComObject(initial);
            Marshal.ReleaseComObject(dialog);
        }
    }

    static string NearestFolder(string path) {
        // The user may have already typed a folder that does not exist yet.
        try {
            var dir=new DirectoryInfo(path);
            while(dir!=null&&!dir.Exists)dir=dir.Parent;
            return dir==null?null:dir.FullName;
        } catch(ArgumentException) {return null;}
          catch(IOException) {return null;}
          catch(NotSupportedException) {return null;}
    }

    [DllImport("shell32.dll",CharSet=CharSet.Unicode,ExactSpelling=true)]
    static extern int SHCreateItemFromParsingName(string path,IntPtr binding,[MarshalAs(UnmanagedType.LPStruct)] Guid iid,
        [MarshalAs(UnmanagedType.Interface)] out IShellItem item);

    [ComImport,Guid("DC1C5A9C-E88A-4DDE-A5A1-60F82A20AEF7")]
    class FileOpenDialog {}

    // Declare all preceding COM slots, including those this picker doesn't use.
    [ComImport,Guid("42F85136-DB7E-439C-85F1-E4075D135FC8"),InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IFileDialog {
        [PreserveSig] int Show(IntPtr owner);
        void SetFileTypes(uint count,IntPtr filters);
        void SetFileTypeIndex(uint index);
        void GetFileTypeIndex(out uint index);
        void Advise(IntPtr events,out uint cookie);
        void Unadvise(uint cookie);
        void SetOptions(uint options);
        void GetOptions(out uint options);
        void SetDefaultFolder(IShellItem item);
        void SetFolder(IShellItem item);
        void GetFolder(out IShellItem item);
        void GetCurrentSelection(out IShellItem item);
        void SetFileName([MarshalAs(UnmanagedType.LPWStr)] string name);
        void GetFileName(out IntPtr name);
        void SetTitle([MarshalAs(UnmanagedType.LPWStr)] string title);
        void SetOkButtonLabel([MarshalAs(UnmanagedType.LPWStr)] string title);
        void SetFileNameLabel([MarshalAs(UnmanagedType.LPWStr)] string title);
        void GetResult(out IShellItem item);
    }

    [ComImport,Guid("43826D1E-E718-42EE-BC55-A1E261C37BFE"),InterfaceType(ComInterfaceType.InterfaceIsIUnknown)]
    interface IShellItem {
        void BindToHandler(IntPtr context,ref Guid handler,ref Guid iid,out IntPtr result);
        void GetParent(out IShellItem item);
        void GetDisplayName(uint kind,out IntPtr name);
    }
}

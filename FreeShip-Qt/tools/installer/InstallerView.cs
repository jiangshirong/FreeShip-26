// SPDX-License-Identifier: GPL-2.0-or-later
using System;
using System.Drawing;
using System.Drawing.Drawing2D;
using System.Reflection;
using System.Windows.Forms;

// The view only presents state. Setup owns the installer transaction and all
// decisions about the destination, options, and when the window may close.
sealed class InstallerView : Form {
    static readonly Color Surface=ColorTranslator.FromHtml("#F3F7F4"), Paper=ColorTranslator.FromHtml("#FCFEFC"),
        Ink=ColorTranslator.FromHtml("#193A35"), Muted=ColorTranslator.FromHtml("#466761"),
        Teal=ColorTranslator.FromHtml("#1D5E53"), Line=ColorTranslator.FromHtml("#CBDDD3");
    internal readonly TextBox Destination;
    internal readonly Button Browse, ActionButton;
    internal readonly ComboBox Language;
    internal readonly CheckBox Shortcut, Launch, Reset;
    readonly Panel pathFrame;
    readonly Label heading, languageLabel, pathLabel;
    readonly TextBox feedback;
    readonly ActivityStrip progress;
    readonly Font bodyFont, headingFont, labelFont;
    readonly Icon brandIcon;
    readonly bool destinationLocked;
    const int Right=299, WidthRight=458;

    internal InstallerView(bool uninstall, bool locked, string target) {
        destinationLocked=locked;
        SuspendLayout();
        AutoScaleDimensions=new SizeF(96,96);AutoScaleMode=AutoScaleMode.Dpi;
        bodyFont=new Font("Segoe UI",10f);headingFont=new Font("Segoe UI",19f,FontStyle.Bold);
        labelFont=new Font("Segoe UI",10.5f,FontStyle.Bold);
        Font=bodyFont;ForeColor=Ink;BackColor=Surface;ClientSize=new Size(785,448);
        StartPosition=FormStartPosition.CenterScreen;FormBorderStyle=FormBorderStyle.FixedDialog;
        MaximizeBox=false;MinimizeBox=false;
        using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("installer-brand.ico")) {
            using(var original=new Icon(stream))brandIcon=(Icon)original.Clone();
            Icon=brandIcon;
        }
        Controls.Add(new BrandPanel {Bounds=new Rectangle(0,0,273,448),Dock=DockStyle.Left,TabStop=false});
        heading=Caption("InstallerHeading",Right,25,WidthRight,41,headingFont,0);
        languageLabel=Caption("LanguageCaption",Right,85,WidthRight,24,labelFont,1);
        Language=new ComboBox {Name="InstallerLanguage",Bounds=new Rectangle(Right,114,246,36),
            BackColor=Paper,ForeColor=Ink,FlatStyle=FlatStyle.Flat,DrawMode=DrawMode.OwnerDrawFixed,ItemHeight=30,
            DropDownStyle=ComboBoxStyle.DropDownList,TabIndex=2};
        Language.DrawItem+=delegate(object sender,DrawItemEventArgs e) {
            if(e.Index<0)return;
            bool field=(e.State&DrawItemState.ComboBoxEdit)!=0;
            bool selected=!field&&(e.State&DrawItemState.Selected)!=0;
            using(var fill=new SolidBrush(!Language.Enabled?Surface:selected?ColorTranslator.FromHtml("#DFEBE4"):Paper))
                e.Graphics.FillRectangle(fill,e.Bounds);
            var rect=Rectangle.Inflate(e.Bounds,-7,0);
            TextRenderer.DrawText(e.Graphics,Language.GetItemText(Language.Items[e.Index]),Language.Font,rect,
                Language.Enabled?Ink:Muted,TextFormatFlags.Left|TextFormatFlags.VerticalCenter|TextFormatFlags.NoPrefix|TextFormatFlags.SingleLine);
            if(!field)e.DrawFocusRectangle();
        };
        pathLabel=Caption("DestinationCaption",Right,169,WidthRight,24,labelFont,3);
        pathFrame=new Panel {Bounds=new Rectangle(Right,198,locked?WidthRight:349,37),BackColor=Paper,TabIndex=4};
        pathFrame.Paint+=delegate(object sender,PaintEventArgs e) {
            using(var stroke=new Pen(pathFrame.ContainsFocus?Teal:Line))
                e.Graphics.DrawRectangle(stroke,0,0,pathFrame.Width-1,pathFrame.Height-1);
        };
        Destination=new TextBox {Name="InstallerDestination",Bounds=new Rectangle(9,8,pathFrame.Width-18,23),
            Anchor=AnchorStyles.Top|AnchorStyles.Left|AnchorStyles.Right,BorderStyle=BorderStyle.None,
            BackColor=Paper,ForeColor=Ink,Text=target,ReadOnly=locked,TabIndex=0};
        Destination.Enter+=delegate {pathFrame.Invalidate();};Destination.Leave+=delegate {pathFrame.Invalidate();};
        pathFrame.Click+=delegate {Destination.Focus();};pathFrame.Controls.Add(Destination);
        Browse=ButtonAt("InstallerBrowse",Right+359,198,99,37,false,5);
        Browse.Visible=!locked;
        Shortcut=Option("InstallerShortcut",266,true,!uninstall,6);
        Launch=Option("InstallerLaunch",302,true,!uninstall,7);
        Reset=Option("InstallerReset",266,false,uninstall,6);
        progress=new ActivityStrip {Name="InstallerProgress",Bounds=new Rectangle(Right,355,WidthRight,7),
            Visible=false,TabStop=false,AccessibleRole=AccessibleRole.ProgressBar};
        feedback=new TextBox {Name="InstallerFeedback",Bounds=new Rectangle(Right,377,320,54),
            BackColor=Surface,ForeColor=Muted,BorderStyle=BorderStyle.None,Multiline=true,
            ReadOnly=true,ScrollBars=ScrollBars.None,Visible=false,TabIndex=8};
        ActionButton=ButtonAt("InstallerAction",Right+338,380,120,40,true,9);
        Controls.AddRange(new Control[]{heading,languageLabel,Language,pathLabel,pathFrame,Browse,
            Rule(250),Shortcut,Launch,Reset,Rule(354),progress,feedback,ActionButton});
        AcceptButton=ActionButton;
        Shown+=delegate {LayoutBrowse();Language.ItemHeight=Math.Max(30,Language.Font.Height*2-2);Destination.Select(0,0);Destination.ScrollToCaret();};
        ResumeLayout(false);
    }
    static Label Caption(string name,int x,int y,int w,int h,Font font,int tab) {
        return new Label {Name=name,Bounds=new Rectangle(x,y,w,h),Font=font,
            TextAlign=ContentAlignment.MiddleLeft,Padding=new Padding(0),TabIndex=tab};
    }
    static Panel Rule(int y) {
        return new Panel {Bounds=new Rectangle(Right,y,WidthRight,1),BackColor=Line,TabStop=false};
    }
    static Button ButtonAt(string name,int x,int y,int w,int h,bool primary,int tab) {
        return new StudioButton(primary) {Name=name,Bounds=new Rectangle(x,y,w,h),
            TabIndex=tab,TextAlign=ContentAlignment.MiddleCenter};
    }
    static CheckBox Option(string name,int y,bool selected,bool show,int tab) {
        return new StudioCheck {Name=name,Bounds=new Rectangle(Right,y,WidthRight,34),
            Checked=selected,Visible=show,TabIndex=tab};
    }
    static GraphicsPath Rounded(RectangleF rect,float radius) {
        var path=new GraphicsPath();float d=radius*2;
        path.AddArc(rect.Left,rect.Top,d,d,180,90);path.AddArc(rect.Right-d,rect.Top,d,d,270,90);
        path.AddArc(rect.Right-d,rect.Bottom-d,d,d,0,90);path.AddArc(rect.Left,rect.Bottom-d,d,d,90,90);
        path.CloseFigure();return path;
    }
    // ButtonBase's buffered paint does not always clear the whole surface on
    // the live Windows desktop. Paint every pixel, including rounded corners.
    static void PaintBackground(PaintEventArgs e,Control control) {
        using(var brush=new SolidBrush(control.Parent==null?Surface:control.Parent.BackColor))
            e.Graphics.FillRectangle(brush,e.ClipRectangle);
    }
    sealed class StudioButton : Button {
        readonly bool primary;
        bool hover, down;
        internal StudioButton(bool primary) {this.primary=primary;DoubleBuffered=true;}
        protected override void OnMouseEnter(EventArgs e) {base.OnMouseEnter(e);hover=true;Invalidate();}
        protected override void OnMouseLeave(EventArgs e) {base.OnMouseLeave(e);hover=false;down=false;Invalidate();}
        protected override void OnMouseDown(MouseEventArgs e) {base.OnMouseDown(e);down=e.Button==MouseButtons.Left;Invalidate();}
        protected override void OnMouseUp(MouseEventArgs e) {base.OnMouseUp(e);down=false;Invalidate();}
        protected override void OnPaint(PaintEventArgs e) {
            PaintBackground(e,this);
            var g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;
            Color fill=primary?(down?ColorTranslator.FromHtml("#123E37"):hover?ColorTranslator.FromHtml("#164C43"):Teal)
                :(down?ColorTranslator.FromHtml("#DFEBE4"):hover?ColorTranslator.FromHtml("#EAF2ED"):Paper);
            using(var shape=Rounded(new RectangleF(.5f,.5f,Width-1,Height-1),Height*.145f))
            using(var brush=new SolidBrush(Enabled?fill:Surface))
            using(var stroke=new Pen(primary&&Enabled?fill:Line)) {g.FillPath(brush,shape);g.DrawPath(stroke,shape);}
            using(var layout=new StringFormat {Alignment=StringAlignment.Center,LineAlignment=StringAlignment.Center})
            using(var ink=new SolidBrush(!Enabled?Muted:primary?Color.White:Ink)) {
                var text=new RectangleF(2,0,Width-4,Height);
                g.DrawString(Text,Font,ink,text,layout);
            }
            if(Focused&&ShowFocusCues)ControlPaint.DrawFocusRectangle(g,Rectangle.Inflate(ClientRectangle,-5,-5),primary?Color.White:Ink,fill);
        }
    }
    sealed class StudioCheck : CheckBox {
        internal StudioCheck() {DoubleBuffered=true;}
        protected override void OnPaint(PaintEventArgs e) {
            PaintBackground(e,this);
            var g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;
            float size=Height*.49f, top=(Height-size)/2f;
            Color fill=Checked?(Enabled?Teal:ColorTranslator.FromHtml("#A4C2B5")):Paper;
            using(var border=new Pen(Checked?fill:ColorTranslator.FromHtml("#75988B")))
            using(var brush=new SolidBrush(fill))
            using(var shape=Rounded(new RectangleF(.5f,top,size,size),size*.21f)) {g.FillPath(brush,shape);g.DrawPath(border,shape);}
            if(Checked)using(var mark=new Pen(Color.White,Math.Max(1.3f,size*.11f))) {
                mark.StartCap=mark.EndCap=LineCap.Round;mark.LineJoin=LineJoin.Round;
                g.DrawLines(mark,new[]{new PointF(size*.19f,top+size*.52f),new PointF(size*.41f,top+size*.73f),new PointF(size*.80f,top+size*.30f)});
            }
            var text=Rectangle.FromLTRB((int)(size*1.62f),0,Width-1,Height);
            using(var layout=new StringFormat {Alignment=StringAlignment.Near,LineAlignment=StringAlignment.Center})
            using(var ink=new SolidBrush(Enabled?Ink:Muted))g.DrawString(Text,Font,ink,text,layout);
            if(Focused&&ShowFocusCues)ControlPaint.DrawFocusRectangle(g,Rectangle.Inflate(text,-1,-4),Ink,Surface);
        }
    }
    sealed class ActivityStrip : Control {
        readonly Timer timer=new Timer {Interval=30};
        int step=30;
        internal ActivityStrip() {DoubleBuffered=true;timer.Tick+=delegate {++step;Invalidate();};}
        protected override void OnVisibleChanged(EventArgs e) {base.OnVisibleChanged(e);if(timer!=null)timer.Enabled=Visible;}
        protected override void OnPaint(PaintEventArgs e) {
            var g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;
            float band=Width*.23f, r=Height*.45f;int phase=step%100;
            using(var track=Rounded(new RectangleF(0,0,Width,Height),r))
            using(var background=new SolidBrush(Line))using(var moving=new SolidBrush(Teal)) {
                g.FillPath(background,track);g.SetClip(track);
                g.FillRectangle(moving,(Width+band)*phase/100f-band,0,band,Height);
            }
        }
        protected override void Dispose(bool disposing) {base.Dispose(disposing);if(disposing)timer.Dispose();}
    }
    internal void Relabel(string title,string section,string lang,string path,string browse,
                           string shortcut,string launch,string reset,string action) {
        Text=title;heading.Text=section;languageLabel.Text=lang;Language.AccessibleName=lang;
        pathLabel.Text=path;Destination.AccessibleName=path;Browse.Text=browse;LayoutBrowse();
        Shortcut.Text=shortcut;Launch.Text=launch;Reset.Text=reset;ActionButton.Text=action;
        // Fit long localized action captions without reducing the font.
        int minimum=Math.Max(ActionButton.Height*3,TextRenderer.MeasureText(action,ActionButton.Font).Width+ActionButton.Height*3/5);
        int edge=Browse.Right;
        ActionButton.Width=Math.Min(edge-feedback.Left,minimum);ActionButton.Left=edge-ActionButton.Width;
        feedback.Width=Math.Max(1,ActionButton.Left-feedback.Left-Math.Max(10,ActionButton.Height/3));
    }
    void LayoutBrowse() {
        if(destinationLocked)return;
        int edge=Browse.Right;
        int gap=Math.Max(8,Browse.Height/4);
        int width=TextRenderer.MeasureText(Browse.Text,Browse.Font).Width+2*gap;
        Browse.Width=Math.Min((edge-pathFrame.Left)/2,Math.Max(Browse.Height*99/37,width));
        Browse.Left=edge-Browse.Width;
        pathFrame.Width=Browse.Left-pathFrame.Left-gap;
    }
    internal string SectionCaption { get { return heading.Text; } }
    internal string FeedbackText { get { return feedback.Text; } }
    internal bool ShowsProgress { get { return progress.Visible; } }
    internal void ShowFeedback(string text,bool error) {
        feedback.ScrollBars=error?ScrollBars.Vertical:ScrollBars.None;
        feedback.Text=text??"";feedback.ForeColor=error?ColorTranslator.FromHtml("#9B4139"):Muted;
        feedback.Visible=feedback.TextLength>0;
    }
    internal void SetRunning(bool value) {
        bool enabled=!value;
        Destination.Enabled=Browse.Enabled=Language.Enabled=Shortcut.Enabled=Launch.Enabled=Reset.Enabled=enabled;
        ActionButton.Enabled=enabled;ActionButton.Visible=enabled;progress.Visible=value;
        feedback.Width=value?Browse.Right-feedback.Left:ActionButton.Left-feedback.Left-Math.Max(10,ActionButton.Height/3);
    }
    protected override bool ProcessDialogKey(Keys keyData) {
        if(keyData==Keys.Escape) {Close();return true;}
        return base.ProcessDialogKey(keyData);
    }
    protected override void Dispose(bool disposing) {
        base.Dispose(disposing);
        if(disposing) {if(brandIcon!=null)brandIcon.Dispose();if(bodyFont!=null)bodyFont.Dispose();
            if(headingFont!=null)headingFont.Dispose();if(labelFont!=null)labelFont.Dispose();}
    }

    sealed class BrandPanel : Panel {
        readonly Bitmap logo;
        readonly Font wordmark=new Font("Segoe UI",33,FontStyle.Bold,GraphicsUnit.Pixel);
        static readonly Color Fill=ColorTranslator.FromHtml("#DFEBE4");
        internal BrandPanel() {
            DoubleBuffered=true;BackColor=Fill;AccessibleName="FreeShip 26";
            using(var stream=Assembly.GetExecutingAssembly().GetManifestResourceStream("installer-brand.png"))
            using(var image=Image.FromStream(stream))logo=new Bitmap(image);
        }
        protected override void OnPaint(PaintEventArgs e) {
            base.OnPaint(e);
            var g=e.Graphics;g.SmoothingMode=SmoothingMode.AntiAlias;
            g.InterpolationMode=InterpolationMode.HighQualityBicubic;g.ScaleTransform(Width/273f,Height/448f);
            using(var grid=new Pen(ColorTranslator.FromHtml("#CBDDD4"))) {
                for(int y=147;y<391;y+=23)g.DrawLine(grid,25,y,248,y);
                for(int x=29;x<252;x+=18)g.DrawLine(grid,x,148,x,386);
            }
            using(var center=new Pen(ColorTranslator.FromHtml("#A4C2B5")))g.DrawLine(center,136,136,136,396);
            for(int i=6;i>=0;--i) {
                float d=i*13f, x=48+i*2.9f, y=309+d*.43f;
                using(var pen=new Pen(ColorTranslator.FromHtml(i==0?"#466F63":"#75988B"),i==0?1.5f:1f))
                using(var hull=new GraphicsPath()) {
                    hull.AddBezier(136,166+d*.65f,94,183+d*.54f,62,232+d*.59f,x,y);
                    hull.AddBezier(x,y,73,330+d*.48f,114,336+d*.42f,136,336+d*.42f);
                    g.DrawPath(pen,hull);
                    var state=g.Save();g.TranslateTransform(272,0);g.ScaleTransform(-1,1);g.DrawPath(pen,hull);g.Restore(state);
                }
            }
            using(var edge=new Pen(ColorTranslator.FromHtml("#C7DAD0")))g.DrawLine(edge,272,0,272,448);
            g.DrawImage(logo,new Rectangle(25,31,43,43));
            using(var text=new SolidBrush(Ink))g.DrawString("FreeShip 26",wordmark,text,22,80);
        }
        protected override void Dispose(bool disposing) {
            base.Dispose(disposing);if(disposing) {logo.Dispose();wordmark.Dispose();}
        }
    }
}

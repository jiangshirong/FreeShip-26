// Native title-bar probe for an explicitly owned, private test process only.
// No global mouse/keyboard input, foreground-window lookup or user-window access.
#pragma once
#include <QApplication>
#include <QDir>
#include <QEventLoop>
#include <QImage>
#include <QFileInfo>
#include <QJsonObject>
#include <QWidget>
#include <QToolButton>
#include <cstring>
#ifdef Q_OS_WIN
#include <QtCore/qt_windows.h>
#endif

inline QJsonObject windowChromeProbe(QWidget* window,const QString& prefix){
    QJsonObject evidence{{"available",false}};
#ifdef Q_OS_WIN
    if(!window->testAttribute(Qt::WA_DontShowOnScreen)&&!window->property("studio_private_window_probe").toBool()){
        evidence["reason"]="not_a_private_probe_window";return evidence;
    }
    const auto hwnd=reinterpret_cast<HWND>(window->winId());DWORD process=0;
    GetWindowThreadProcessId(hwnd,&process);if(process!=GetCurrentProcessId()){evidence["reason"]="hwnd_not_owned";evidence["hwnd_valid"]=bool(IsWindow(hwnd));return evidence;}
    const auto style=GetWindowLongPtrW(hwnd,GWL_STYLE);
    evidence={{"available",true},{"owned_process",true},{"maximize_box",bool(style&WS_MAXIMIZEBOX)},
              {"minimize_box",bool(style&WS_MINIMIZEBOX)},
              {"resizable_frame",bool(style&WS_THICKFRAME)},{"system_menu",bool(style&WS_SYSMENU)}};
    const auto icon=reinterpret_cast<HICON>(SendMessageW(hwnd,WM_GETICON,ICON_SMALL,0));
    evidence["native_icon"]=bool(icon);
    if(icon){
        ICONINFO info{};
        if(GetIconInfo(icon,&info)){
            BITMAP bitmap{};GetObjectW(info.hbmColor?info.hbmColor:info.hbmMask,sizeof(bitmap),&bitmap);
            const int width=bitmap.bmWidth,height=info.hbmColor?bitmap.bmHeight:bitmap.bmHeight/2;
            if(width>0&&height>0&&width<=256&&height<=256){
                BITMAPINFO description{};description.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);
                description.bmiHeader.biWidth=width;description.bmiHeader.biHeight=-height;
                description.bmiHeader.biPlanes=1;description.bmiHeader.biBitCount=32;description.bmiHeader.biCompression=BI_RGB;
                void* pixels=nullptr;const auto dc=CreateCompatibleDC(nullptr);
                const auto dib=CreateDIBSection(dc,&description,DIB_RGB_COLORS,&pixels,nullptr,0);
                if(dib&&pixels){
                    auto old=SelectObject(dc,dib);std::memset(pixels,0,size_t(width)*height*4);
                    DrawIconEx(dc,0,0,icon,width,height,0,nullptr,DI_NORMAL);
                    const auto image=QImage(static_cast<uchar*>(pixels),width,height,QImage::Format_ARGB32_Premultiplied).copy();
                    int ink=0;for(int y=0;y<height;++y)for(int x=0;x<width;++x){const auto p=image.pixel(x,y);if(qAlpha(p)||qRed(p)||qGreen(p)||qBlue(p))++ink;}
                    QDir().mkpath(QFileInfo(prefix).absolutePath());image.save(prefix+"-native-icon.png");
                    // Width/height are already physical HWND pixels, not DIP.
                    window->windowIcon().pixmap(QSize(width,height),1.0).save(prefix+"-requested-icon.png");
                    evidence["icon_pixels"]=ink;evidence["icon_width"]=width;evidence["icon_height"]=height;
                    evidence["native_icon_image"]=prefix+"-native-icon.png";evidence["requested_icon_image"]=prefix+"-requested-icon.png";
                    SelectObject(dc,old);DeleteObject(dib);
                }
                DeleteDC(dc);
            }
            if(info.hbmColor)DeleteObject(info.hbmColor);if(info.hbmMask)DeleteObject(info.hbmMask);
        }
    }
    // Keep this private HWND transparent while exercising DefWindowProc's real
    // non-client double click. This is an input/state probe, not a motion capture.
    const auto oldExtended=GetWindowLongPtrW(hwnd,GWL_EXSTYLE);
    SetWindowLongPtrW(hwnd,GWL_EXSTYLE,oldExtended|WS_EX_LAYERED);
    SetLayeredWindowAttributes(hwnd,0,0,LWA_ALPHA);
    const bool wasHidden=window->testAttribute(Qt::WA_DontShowOnScreen);
    const bool requestedMaximized=window->isMaximized();
    // Re-enter the real Qt show path while transparent. Merely clearing the
    // attribute on an already logically visible widget leaves its native normal
    // placement stale and is not how a user opens a window.
    window->hide();
    window->setAttribute(Qt::WA_DontShowOnScreen,false);
    window->setAttribute(Qt::WA_ShowWithoutActivating);
    if(requestedMaximized)window->showMaximized();else window->showNormal();
    ShowWindow(hwnd,requestedMaximized?SW_SHOWMAXIMIZED:SW_SHOWNOACTIVATE);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    const bool custom=window->property("studio_caption_minimize").toBool();evidence["custom_caption"]=custom;
    QWidget* ownCaption=nullptr;
    for(auto* caption:window->findChildren<QWidget*>("StudioWindowCaption"))if(caption->window()==window){ownCaption=caption;break;}
    if(custom){
        auto* caption=ownCaption;
        int minimize=0,maximize=0,close=0;
        if(caption){
            for(auto* button:caption->findChildren<QToolButton*>())if(button->isVisibleTo(caption)){
                const auto control=button->property("studio_caption_control").toString();
                minimize+=control=="minimize";maximize+=control=="maximize";close+=control=="close";
            }
            caption->grab().save(prefix+"-caption.png");evidence["caption_image"]=prefix+"-caption.png";
            // QWidget global coordinates are DIP; Win32 hit tests need physical
            // screen pixels, including the native client origin on this monitor.
            const auto local=caption->mapTo(window,QPoint(caption->width()/2,caption->height()/2));
            POINT origin{};ClientToScreen(hwnd,&origin);const auto ratio=window->devicePixelRatioF();
            const POINT physical{origin.x+qRound(local.x()*ratio),origin.y+qRound(local.y()*ratio)};
            evidence["caption_hit_is_native_drag"]=SendMessageW(hwnd,WM_NCHITTEST,0,MAKELPARAM(short(physical.x),short(physical.y)))==HTCAPTION;
        }
        evidence["minimize_controls"]=minimize;evidence["maximize_controls"]=maximize;evidence["close_controls"]=close;
    }else{
        TITLEBARINFOEX title{};title.cbSize=sizeof(title);SendMessageW(hwnd,WM_GETTITLEBARINFOEX,0,reinterpret_cast<LPARAM>(&title));
        evidence["minimize_visible"]=!(title.rgstate[2]&(STATE_SYSTEM_INVISIBLE|STATE_SYSTEM_OFFSCREEN));
        evidence["maximize_visible"]=!(title.rgstate[3]&(STATE_SYSTEM_INVISIBLE|STATE_SYSTEM_OFFSCREEN));
        evidence["close_visible"]=!(title.rgstate[5]&(STATE_SYSTEM_INVISIBLE|STATE_SYSTEM_OFFSCREEN));
    }
    const bool initiallyMaximized=window->isMaximized();const auto before=window->size();
    evidence["initial_width"]=before.width();evidence["initial_height"]=before.height();
    auto doubleClick=[&]{
        RECT rect{};GetWindowRect(hwnd,&rect);
        const auto position=MAKELPARAM(short(rect.left+(rect.right-rect.left)/2),short(rect.top+10));
        SendMessageW(hwnd,WM_NCLBUTTONDBLCLK,HTCAPTION,position);
        QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
    };
    doubleClick();evidence["first_double_click_toggled"]=window->isMaximized()!=initiallyMaximized;
    evidence["native_zoom_matches_qt"]=bool(IsZoomed(hwnd))==window->isMaximized();
    doubleClick();evidence["second_double_click_restored"]=window->isMaximized()==initiallyMaximized;
    evidence["restored_size_matches"]=initiallyMaximized||window->size()==before;
    evidence["restored_width"]=window->width();evidence["restored_height"]=window->height();
    if(custom){
        auto* caption=ownCaption;
        if(caption)for(auto* button:caption->findChildren<QToolButton*>())if(button->property("studio_caption_control")=="minimize"){
            button->click();QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
            evidence["minimize_button_works"]=window->isMinimized()&&bool(IsIconic(hwnd));
            if(initiallyMaximized)window->showMaximized();else window->showNormal();
            QApplication::processEvents(QEventLoop::ExcludeUserInputEvents);
        }
    }
    window->hide();window->setAttribute(Qt::WA_DontShowOnScreen,wasHidden);SetWindowLongPtrW(hwnd,GWL_EXSTYLE,oldExtended);
#else
    Q_UNUSED(window);Q_UNUSED(prefix);
#endif
    return evidence;
}

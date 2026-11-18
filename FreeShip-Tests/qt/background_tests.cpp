// Independent geometry expectations and real HullView events. No desktop input.
#include "render/hull_view.h"
#include "render/background_geometry.h"
#include "render/hull_gpu.h"
#include <QApplication>
#include <QAction>
#include <QBuffer>
#include <QContextMenuEvent>
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QKeyEvent>
#include <QMenu>
#include <QLineF>
#include <QMouseEvent>
#include <QElapsedTimer>
#include <cmath>
#include <stdexcept>
namespace {
void mouse(HullView& view,QEvent::Type type,QPointF p,Qt::MouseButton button,Qt::MouseButtons held){QMouseEvent e(type,p,p,button,held,Qt::NoModifier);QApplication::sendEvent(&view,&e);}
}
int main(int argc,char** argv){
    QApplication app(argc,argv);const auto args=app.arguments();if(args.size()!=3)return 3;
    const auto out=args[2];QDir().mkpath(out);QJsonObject report;QJsonArray cases;bool failed=false;
    auto check=[&](QString id,bool pass){cases.append(QJsonObject{{"id",id},{"status",pass?"pass":"fail"}});failed|=!pass;};
    try{
        if(args[1]=="--gpu-only"){
            HullRenderMesh mesh;mesh.triangles.append(HullRenderTriangle{{fs::Vec3{-.5,-.5,0},fs::Vec3{.5,-.5,0},fs::Vec3{0,.5,0}},fs::Vec3{0,0,1},QColor(0,128,0)});
            HullGpu gpu(mesh);
            if(gpu.ready()){
                const auto fill=gpu.render({64,64},1,QMatrix4x4{},HullLighting{});check("gpu-framebuffer-rendered",!fill.isNull());
                if(!fill.isNull()){
                    check("gpu-outside-hull-remains-transparent",qAlpha(fill.pixel(0,0))==0);
                    check("gpu-hull-remains-opaque-original-color",fill.pixelColor(32,32)==QColor(0,128,0,255));
                    fill.save(out+"/gpu-fill.png");
                }
            }else cases.append(QJsonObject{{"id","gpu-context"},{"status","not_run"},{"message","No OpenGL context; no software substitute counted as GPU coverage."}});
            report["status"]=failed?"fail":gpu.ready()?"pass":"partial";report["cases"]=cases;
            report["scope"]="Real offscreen OpenGL surface on native platform; no widget display or desktop input.";
            QFile result(out+"/report.json");if(result.open(QIODevice::WriteOnly))result.write(QJsonDocument(report).toJson());
            return failed?1:gpu.ready()?0:2;
        }
        QImage pattern(128,96,QImage::Format_RGB32);pattern.fill(Qt::white);
        for(int y=0;y<48;++y)for(int x=0;x<64;++x)pattern.setPixelColor(x,y,Qt::red);
        QByteArray jpeg;QBuffer encoded(&jpeg);encoded.open(QIODevice::WriteOnly);if(!pattern.save(&encoded,"JPEG",100))throw std::runtime_error("JPEG writer unavailable");
        QJsonObject source{{"view",1},{"origin",QJsonArray{10,20}},{"scale",.5},{"visible",true},{"alpha",255},{"transparent",false},{"transparent_color",0xffffff},{"tolerance",3},{"quality",100},{"jpeg",QString::fromLatin1(jpeg.toBase64())}};
        auto b=fs::BackgroundImage::fromJson(source);
        check("exact-jpeg-is-retained",b.jpeg==jpeg&&b.pixels.size()==QSize(128,96));
        const auto bmpPath=out+QString::fromUtf8("/背景样本.bmp");
        if(!pattern.save(bmpPath,"BMP"))throw std::runtime_error("BMP writer unavailable");
        QFile bmp(bmpPath);bmp.open(QIODevice::ReadOnly);const auto originalBmp=bmp.readAll();bmp.close();
        const auto converted=fs::BackgroundImage::importFile(bmpPath);
        check("unicode-bmp-import-preserves-dimensions",fs::BackgroundImage::decode(converted).size()==pattern.size());
        bmp.open(QIODevice::ReadOnly);check("bmp-import-leaves-source-unchanged",bmp.readAll()==originalBmp);bmp.close();
        const auto jpegPath=out+"/roundtrip.jpg";QFile originalJpeg(jpegPath);originalJpeg.open(QIODevice::WriteOnly);originalJpeg.write(jpeg);originalJpeg.close();
        check("jpeg-file-import-is-lossless",fs::BackgroundImage::importFile(jpegPath)==jpeg);
        check("original-pixel-origin-and-scale",b.targetRect({100,200},4)==QRectF(80,160,256,192));
        check("screen-image-coordinate-roundtrip",b.imagePoint({112,184},{100,200},4)==QPointF(16,12));
        fs::BackgroundCalibration c;
        check("origin-drag-independent-equation",fs::moveBackground(b,{8,-12},4,c)&&c.origin==QPoint(6,26)&&c.scale==.5);
        check("east-edge-resize",fs::resizeBackground(b,{100,200},4,2,{256,0},c)&&c.origin==QPoint(10,20)&&c.scale==1);
        check("resize-does-not-invert-image",!fs::resizeBackground(b,{100,200},4,2,{-300,0},c));
        double scale=0;check("dominant-coordinate-scale",fs::calibrateBackground(b,{4,-3},{8,6},scale)&&scale==2);
        check("scale-origin-rejected",!fs::calibrateBackground(b,{0,0},{8,6},scale));
        check("negative-scale-rejected",!fs::calibrateBackground(b,{4,-3},{-8,6},scale));
        check("frame-hit-interior-and-edge",fs::backgroundSide({10,10,100,80},{60,50})==0&&fs::backgroundSide({10,10,100,80},{111,50})==2&&fs::backgroundSide({10,10,100,80},{150,50})==-1);
        b.transparent=true;auto masked=b.maskedPixels();check("transparent-color-mask",qAlpha(masked.pixel(110,80))==0&&qAlpha(masked.pixel(20,20))==255);
        check("mask-cache-reused",masked.cacheKey()==b.maskedPixels().cacheKey());
        check("decoded-jpeg-cache-reused",b.pixels.cacheKey()==fs::BackgroundImage::decode(jpeg).cacheKey());
        for(const auto& fault:QStringList{"scale","origin","view","alpha","tolerance","color","jpeg","base64"}){
            auto o=source;if(fault=="scale")o["scale"]=0;else if(fault=="origin")o["origin"]=QJsonArray{1.25,20};else if(fault=="view")o["view"]=3;
            else if(fault=="alpha")o["alpha"]=256;else if(fault=="tolerance")o["tolerance"]=-1;else if(fault=="color")o["transparent_color"]=0x1000000;
            else o["jpeg"]=fault=="base64"?"%%%":QString::fromLatin1(QByteArray("bad image").toBase64());
            bool rejected=false;try{fs::BackgroundImage::fromJson(o);}catch(const std::exception&){rejected=true;}check("invalid-background-rejected-"+fault,rejected);
        }
        auto actual=std::make_shared<fs::ModelSnapshot>(fs::ModelSnapshot::load(args[1]));
        auto m=std::make_shared<fs::ModelSnapshot>(*actual);m->backgrounds.clear();
        b=fs::BackgroundImage::fromJson(source);b.origin={0,96};b.scale=double(float(std::max(1.,m->maximum.x-m->minimum.x)/128));m->backgrounds.append(b);
        HullView view;view.setAttribute(Qt::WA_DontShowOnScreen);view.resize(800,520);view.setStudioAppearance(true);view.setAnimationsEnabled(false);
        view.setSnapshot(m);view.setFillVisible(false);view.setControlNetVisible(false);view.setGridVisible(false);view.setHydroVisible(false);view.fit();
        const auto image=view.image();image.save(out+"/background-profile.png");check("profile-maps-original-view-one",view.background()==&m->backgrounds[0]);
        view.setProjection(HullProjection::Body);check("background-does-not-leak-to-other-view",view.background()==nullptr);view.setProjection(HullProjection::Profile);
        check("frame-mode-enters-with-visible-image",view.setBackgroundMode(BackgroundMode::Frame));check("frame-is-not-exported",view.image()==image);
        int commits=0;QJsonObject changes;view.onBackgroundChanged=[&](QJsonObject fields){++commits;changes=fields;};
        view.setBackgroundMode(BackgroundMode::Origin);auto start=view.backgroundRect().center();
        mouse(view,QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);mouse(view,QEvent::MouseMove,start+QPointF(24,-16),Qt::NoButton,Qt::LeftButton);
        check("drag-preview-is-local",commits==0&&m->backgrounds[0].origin==QPoint(0,96));
        check("image-export-ignores-drag-preview",view.image()==image);
        mouse(view,QEvent::MouseButtonRelease,start+QPointF(24,-16),Qt::LeftButton,Qt::NoButton);
        check("one-commit-on-release",commits==1&&changes.contains("origin"));
        view.setBackgroundMode(BackgroundMode::Origin);mouse(view,QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);mouse(view,QEvent::MouseMove,start+QPointF(50,0),Qt::NoButton,Qt::LeftButton);
        QKeyEvent escape(QEvent::KeyPress,Qt::Key_Escape,Qt::NoModifier);QApplication::sendEvent(&view,&escape);mouse(view,QEvent::MouseButtonRelease,start+QPointF(50,0),Qt::LeftButton,Qt::NoButton);
        check("escape-cancels-without-commit",commits==1&&view.backgroundMode()==BackgroundMode::None);
        view.setBackgroundMode(BackgroundMode::Frame);mouse(view,QEvent::MouseButtonPress,start,Qt::RightButton,Qt::RightButton);mouse(view,QEvent::MouseButtonRelease,start,Qt::RightButton,Qt::NoButton);
        QContextMenuEvent right(QContextMenuEvent::Mouse,start.toPoint(),start.toPoint());QApplication::sendEvent(&view,&right);
        check("right-click-exits-frame-without-menu",view.backgroundMode()==BackgroundMode::None&&view.findChildren<QMenu*>().isEmpty());
        view.setBackgroundMode(BackgroundMode::Origin);mouse(view,QEvent::MouseButtonPress,start,Qt::LeftButton,Qt::LeftButton);
        auto empty=std::make_shared<fs::ModelSnapshot>(*m);empty->backgrounds.clear();view.setSnapshot(empty);mouse(view,QEvent::MouseMove,start+QPointF(80,0),Qt::NoButton,Qt::LeftButton);mouse(view,QEvent::MouseButtonRelease,start+QPointF(80,0),Qt::LeftButton,Qt::NoButton);
        check("removed-image-cancels-active-gesture",commits==1&&view.backgroundMode()==BackgroundMode::None&&view.backgroundRect().isEmpty());
        check("no-image-rejects-frame",!view.setBackgroundMode(BackgroundMode::Frame));
        QContextMenuEvent context(QContextMenuEvent::Keyboard,{20,40},{20,40});QApplication::sendEvent(&view,&context);QList<QAction*> found;
        for(auto* a:view.findChildren<QAction*>())if(a->property("background_operation")=="frame")found.append(a);
        if(found.size()==1)check("empty-view-frame-action-disabled",!found[0]->isEnabled());else cases.append(QJsonObject{{"id","frame-action-discovery"},{"status","not_run"}});
        for(auto* menu:view.findChildren<QMenu*>())menu->hide();
        auto only=std::make_shared<fs::ModelSnapshot>(*m);only->points.clear();only->edges.clear();only->faces.clear();only->triangles.clear();only->sections.clear();view.setSnapshot(only);view.setFillVisible(false);auto before=view.image();view.setFillVisible(true);
        check("empty-fill-buffer-keeps-background-visible",view.image()==before);
        view.setSnapshot(m);view.setFillVisible(false);view.fit();auto rect=view.backgroundRect();view.panBy({17,9});check("background-follows-pan",view.backgroundRect()==rect.translated(17,9));
        const auto anchor=QPointF(310,220);rect=view.backgroundRect();view.zoomAt(anchor,2);auto zoomed=view.backgroundRect();
        check("background-follows-anchor-zoom",QLineF(zoomed.topLeft(),anchor+2*(rect.topLeft()-anchor)).length()<1e-6&&std::abs(zoomed.width()-2*rect.width())<1e-6);
        check("hidpi-image-complete",view.image(2).size()==QSize(1600,1040));
        QElapsedTimer timer;timer.start();for(int i=0;i<20;++i){view.panBy({i%2?1.:-1.,0});view.image();}report["pan_mean_ms"]=timer.nsecsElapsed()/1e6/20;
        cases.append(QJsonObject{{"id","desktop-and-raster-parity"},{"status","review"},{"message","Offscreen image and event evidence only; Qt interpolation/blending is not pixel-identical to legacy GDI."}});
        report["status"]=failed?"fail":"partial";
    }catch(const std::exception& e){report["status"]="error";report["error"]=e.what();}
    report["cases"]=cases;QFile result(out+"/report.json");if(result.open(QIODevice::WriteOnly))result.write(QJsonDocument(report).toJson());
    return report["status"]=="partial"?2:report["status"]=="fail"?1:3;
}

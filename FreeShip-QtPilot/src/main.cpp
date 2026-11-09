#include "view.h"
#include <QApplication>
#include <QMainWindow>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QProcess>
#include <QTemporaryDir>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QSaveFile>
#include <QTimer>
#include <QElapsedTimer>
#include <QJsonDocument>
#include <QJsonObject>
#include <memory>

static void saveDxf(const QString&source,const QString&target){
    QFile src(source);QSaveFile dst(target);
    if(!src.open(QIODevice::ReadOnly)||!dst.open(QIODevice::WriteOnly))throw std::runtime_error("Cannot open DXF export");
    const auto bytes=src.readAll();if(src.error()!=QFile::NoError||dst.write(bytes)!=bytes.size()||!dst.commit())throw std::runtime_error("Cannot write DXF export");
}

int main(int argc,char**argv){
    QApplication app(argc,argv);QCoreApplication::setApplicationName("FreeShip_26 Qt 型线图试点");
    const bool smoke=app.arguments().size()==4&&app.arguments()[1]=="--verify-model";
    const QString smokeOut=smoke?app.arguments()[3]:QString();
    QMainWindow window;window.setWindowTitle("FreeShip_26 · Qt 型线图试点");window.resize(1280,850);
    auto*view=new LinesView;window.setCentralWidget(view);auto*bar=window.addToolBar("型线图");bar->setMovable(false);
    auto*open=bar->addAction("打开模型");auto*fit=bar->addAction("全图");auto*fill=bar->addAction("填色");fill->setCheckable(true);fill->setChecked(true);
    bar->addSeparator();auto*png=bar->addAction("保存图片");auto*dxf=bar->addAction("导出 DXF");
    window.statusBar()->showMessage("打开 FBM 模型；右键拖曳平移，滚轮缩放。");
    QProcess bridge;std::unique_ptr<QTemporaryDir> temp;QString referenceDxf;QElapsedTimer opening,bridgeTimer;
    QJsonObject timings;
    view->onFirstFrame=[&]{if(opening.isValid()&&!smoke)window.statusBar()->showMessage(
        QString("型线图已就绪 · %1 ms · 右键平移，滚轮缩放").arg(opening.elapsed()));};
    auto smokeFinish=[&](const QString&error){
        QDir().mkpath(smokeOut);QJsonObject report{{"status",error.isEmpty()?"pass":"error"},{"error",error},
            {"scope","Hidden production loader, first offscreen frame and export path; no desktop presentation measurement"},
            {"timings",timings},{"verification_total_ms",opening.isValid()?opening.elapsed():0}};
        QFile f(smokeOut+"/report.json");if(f.open(QIODevice::WriteOnly))f.write(QJsonDocument(report).toJson());app.exit(error.isEmpty()?0:1);
    };
    auto display=[&](const QString&path){
        auto scene=std::make_shared<Scene>(Scene::load(path));
        timings.insert("scene_read_ms",scene->loadMs);timings.insert("scene_compile_ms",scene->compileMs);
        view->setScene(scene);timings.insert("gpu_initialize_ms",view->gpuInitializationMs());
        timings.insert("model_to_scene_ready_ms",opening.nsecsElapsed()/1e6);
        referenceDxf=QFileInfo(path).dir().filePath("reference.dxf");
        window.statusBar()->showMessage(QString("%1 个填色三角形 · %2 个线顶点 · 读取 %3 ms · 整理 %4 ms · 右键平移，滚轮缩放")
          .arg(scene->triangles).arg(scene->lineVertices).arg(scene->loadMs,0,'f',1).arg(scene->compileMs,0,'f',1));
        if(smoke){
            QDir().mkpath(smokeOut);view->resize(1280,800);view->fit();
            QElapsedTimer render;render.start();auto pixels=view->image();
            timings.insert("first_offscreen_frame_ms",render.nsecsElapsed()/1e6);
            timings.insert("model_to_first_offscreen_frame_ms",opening.nsecsElapsed()/1e6);
            QElapsedTimer exports;exports.start();
            if(!pixels.save(smokeOut+"/production.png"))throw std::runtime_error("PNG export failed");
            saveDxf(referenceDxf,smokeOut+"/production.dxf");scene->save(smokeOut+"/production.fslp");
            if(!QFile::copy(referenceDxf,smokeOut+"/bridge-reference.dxf"))throw std::runtime_error("Cannot preserve bridge export evidence");
            timings.insert("verification_exports_ms",exports.nsecsElapsed()/1e6);smokeFinish({});
        }
    };
    auto load=[&](const QString&path){
        if(path.isEmpty()||bridge.state()!=QProcess::NotRunning)return;
        opening.start();timings={};
        try{
            if(path.endsWith(".fslp",Qt::CaseInsensitive)){display(path);return;}
            temp=std::make_unique<QTemporaryDir>(QDir::tempPath()+"/freeship-qt-XXXXXX");
            if(!temp->isValid())throw std::runtime_error("Cannot create private model workspace");
            const auto folder=temp->path();
            if(!QFile::copy(path,folder+"/input.fbm"))throw std::runtime_error("Cannot copy model for read-only opening");
            QProcessEnvironment env=QProcessEnvironment::systemEnvironment();
            for(const QString&key:{"APPDATA","LOCALAPPDATA","TEMP","TMP"}){QDir().mkpath(folder+"/"+key);env.insert(key,folder+"/"+key);}
            bridge.setProcessEnvironment(env);bridge.setWorkingDirectory(folder);
            bridge.setProgram(QCoreApplication::applicationDirPath()+"/bridge/FreeShipGeometryBridge.exe");
            bridge.setArguments({folder+"/input.fbm",folder});open->setEnabled(false);
            timings.insert("prepare_private_input_ms",opening.nsecsElapsed()/1e6);
            window.statusBar()->showMessage("正在读取模型并生成型线图…");bridgeTimer.start();bridge.start();
        }catch(const std::exception&e){if(smoke)smokeFinish(QString::fromUtf8(e.what()));else QMessageBox::warning(&window,"打开失败",QString::fromUtf8(e.what()));}
    };
    QObject::connect(&bridge,&QProcess::finished,&window,[&](int code,QProcess::ExitStatus status){
        open->setEnabled(true);
        timings.insert("bridge_process_ms",bridgeTimer.nsecsElapsed()/1e6);
        try{if(code!=0||status!=QProcess::NormalExit)throw std::runtime_error((bridge.readAllStandardOutput()+bridge.readAllStandardError()).toStdString());
            display(temp->path()+"/scene.fslp");
        }catch(const std::exception&e){if(smoke)smokeFinish(QString::fromUtf8(e.what()));else QMessageBox::warning(&window,"生成失败",QString::fromUtf8(e.what()));}
    });
    QObject::connect(&bridge,&QProcess::errorOccurred,&window,[&](QProcess::ProcessError error){
        open->setEnabled(true);if(error==QProcess::FailedToStart){if(smoke)smokeFinish(bridge.errorString());else QMessageBox::warning(&window,"无法启动计算组件",bridge.errorString());}
    });
    QObject::connect(open,&QAction::triggered,&window,[&]{load(QFileDialog::getOpenFileName(&window,"打开船模或矢量场景",{},"FreeShip (*.fbm *.fslp)"));});
    QObject::connect(fit,&QAction::triggered,view,&LinesView::fit);QObject::connect(fill,&QAction::toggled,view,&LinesView::setFill);
    QObject::connect(png,&QAction::triggered,&window,[&]{auto path=QFileDialog::getSaveFileName(&window,"保存图片",{},"PNG (*.png)");if(!path.isEmpty()&&!view->image().save(path,"PNG"))QMessageBox::warning(&window,"保存失败","无法写入图片。");});
    QObject::connect(dxf,&QAction::triggered,&window,[&]{
        if(!QFileInfo::exists(referenceDxf)){QMessageBox::information(&window,"导出 DXF","当前场景没有对应的原计算核心 DXF 结果。");return;}
        auto path=QFileDialog::getSaveFileName(&window,"导出 DXF",{},"DXF (*.dxf)");if(path.isEmpty())return;
        try{saveDxf(referenceDxf,path);}catch(const std::exception&e){QMessageBox::warning(&window,"保存失败",QString::fromUtf8(e.what()));}
    });
    QObject::connect(&app,&QApplication::aboutToQuit,[&]{if(bridge.state()!=QProcess::NotRunning){bridge.kill();bridge.waitForFinished(3000);}});
    if(smoke){QTimer::singleShot(0,&window,[&]{load(app.arguments()[2]);});QTimer::singleShot(60000,&window,[&]{smokeFinish("Timed out");});}
    else{window.show();if(app.arguments().size()>1){auto path=app.arguments()[1];QTimer::singleShot(0,&window,[&,path]{load(path);});}}
    return app.exec();
}

// SPDX-License-Identifier: GPL-2.0-or-later
// Existing view-local Background image operations, backed by one document.
#include "main_window.h"
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include "preferences_store.h"
#include <QFileInfo>
#include <QFormLayout>
#include <QImageReader>
#include <QMessageBox>
#include <QPushButton>
#include <QSaveFile>
#include <QSpinBox>
#include <QStatusBar>
#include <QVBoxLayout>
namespace {
void buttons(QDialog& dialog,QVBoxLayout& layout){
    auto* b=new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel);
    b->button(QDialogButtonBox::Ok)->setText("确定");b->button(QDialogButtonBox::Cancel)->setText("取消");
    QObject::connect(b,&QDialogButtonBox::accepted,&dialog,&QDialog::accept);QObject::connect(b,&QDialogButtonBox::rejected,&dialog,&QDialog::reject);layout.addWidget(b);
}
}
void MainWindow::backgroundAction(HullView* view,const QString& operation,QPointF position){
    if(!session_.ready()||session_.busy()||view->snapshot()!=session_.model()||view->backgroundView()>2)return;
    const auto original=session_.model();const auto* image=view->background();
    auto current=[&]{return session_.model()==original&&!session_.busy();};
    auto confirm=[&](const QString& message){
        QMessageBox dialog(QMessageBox::Question,"背景影像",message,QMessageBox::Yes|QMessageBox::No,this);
        dialog.setProperty("background_operation",operation);dialog.setDefaultButton(QMessageBox::No);
        dialog.button(QMessageBox::Yes)->setText("确定");dialog.button(QMessageBox::No)->setText("取消");
        return dialog.exec()==QMessageBox::Yes&&current();
    };
    if(operation=="import"){
        if(image&&!confirm("当前视图已有背景图片，是否替换？"))return;
        QFileDialog dialog(this,"导入背景图片",preferenceFileLocation("import",QFileInfo(session_.sourcePath()).path()));
        dialog.setProperty("background_operation","import_file");dialog.setFileMode(QFileDialog::ExistingFile);
        dialog.setLabelText(QFileDialog::Accept,"打开");dialog.setLabelText(QFileDialog::Reject,"取消");dialog.setLabelText(QFileDialog::FileName,"文件名");dialog.setLabelText(QFileDialog::FileType,"文件类型");
        dialog.setNameFilter("图片 (*.jpg *.jpeg *.bmp)");if(workflow_)dialog.setOption(QFileDialog::DontUseNativeDialog);
        if(dialog.exec()!=QDialog::Accepted||dialog.selectedFiles().size()!=1||!current())return;
        const auto path=dialog.selectedFiles().first();QImageReader reader(path);
        session_.importBackground(view->backgroundView(),path,view->backgroundImportScale(reader.size().width()));return;
    }
    if(!image)return;
    if(operation=="remove"){
        if(confirm("确定清除当前视图的背景图片？"))session_.editBackground(view->backgroundView(),"remove");return;
    }
    if(operation=="visible"||operation=="transparent"){
        session_.editBackground(view->backgroundView(),"update",{{"changes",QJsonObject{{operation,operation=="visible"?!image->visible:!image->transparent}}}});return;
    }
    if(operation=="scale_at"){
        const auto reference=view->backgroundReference(position);
        if(reference.isNull()){statusBar()->showMessage("请在离原点较远的已知位置设置比例。");return;}
        QDialog dialog(this);dialog.setWindowTitle("设置比例");dialog.setProperty("background_operation",operation);
        QVBoxLayout layout(&dialog);auto* form=new QFormLayout;layout.addLayout(form);
        const QString units=original->units=="metric"?"m":"ft";
        const QString x=view->backgroundView()==0?"Y":"X",y=view->backgroundView()==2?"Y":"Z";
        QDoubleSpinBox values[2];for(int i=0;i<2;++i){values[i].setRange(-1e9,1e9);values[i].setDecimals(8);values[i].setKeyboardTracking(false);
            values[i].setProperty("background_field",i?"coordinate_y":"coordinate_x");values[i].setValue(i?reference.y():reference.x());form->addRow((i?y:x)+"（"+units+"）",&values[i]);}
        buttons(dialog,layout);
        if(dialog.exec()==QDialog::Accepted&&current()&&!view->calibrateBackgroundAt(position,{values[0].value(),values[1].value()}))
            statusBar()->showMessage("比例无效，请选取非零参考位置并输入同方向的坐标。");
        return;
    }
    if(operation=="alpha"||operation=="tolerance"){
        QDialog dialog(this);dialog.setWindowTitle(operation=="alpha"?"背景图片混合":"透明色容差");dialog.setProperty("background_operation",operation);
        QVBoxLayout layout(&dialog);auto* form=new QFormLayout;layout.addLayout(form);QSpinBox value;
        value.setRange(0,255);value.setValue(operation=="alpha"?image->alpha:image->tolerance);value.setProperty("background_field",operation);
        form->addRow(operation=="alpha"?"不透明度（0–255）":"颜色容差（0–255）",&value);buttons(dialog,layout);
        if(dialog.exec()==QDialog::Accepted&&current())session_.editBackground(view->backgroundView(),"update",{{"changes",QJsonObject{{operation,value.value()}}}});return;
    }
    if(operation=="export"){
        QFileDialog dialog(this,"保存背景图片",preferenceFileLocation("export",QFileInfo(session_.sourcePath()).path()));dialog.setProperty("background_operation","export_file");
        dialog.setAcceptMode(QFileDialog::AcceptSave);dialog.setNameFilters({"JPEG 图片 (*.jpg)","位图 (*.bmp)"});dialog.setDefaultSuffix("jpg");
        dialog.setLabelText(QFileDialog::Accept,"保存");dialog.setLabelText(QFileDialog::Reject,"取消");dialog.setLabelText(QFileDialog::FileName,"文件名");dialog.setLabelText(QFileDialog::FileType,"文件类型");
        connect(&dialog,&QFileDialog::filterSelected,&dialog,[&](const QString& filter){dialog.setDefaultSuffix(filter.contains("*.bmp")?"bmp":"jpg");});
        if(workflow_)dialog.setOption(QFileDialog::DontUseNativeDialog);
        if(dialog.exec()!=QDialog::Accepted||dialog.selectedFiles().size()!=1||!current())return;
        const auto path=dialog.selectedFiles().first();const bool bitmap=QFileInfo(path).suffix().compare("bmp",Qt::CaseInsensitive)==0;
        QSaveFile file(path);bool ok=file.open(QIODevice::WriteOnly);
        if(ok)ok=bitmap?image->pixels.save(&file,"BMP"):file.write(image->jpeg)==image->jpeg.size();
        if(!ok||!file.commit())showError("背景图片保存失败，原文件未被替换。");else statusBar()->showMessage("背景图片已保存："+path);
    }
}

#include "feedback_text.h"
#include "core/platform_process.h"
#include "app/ui_language.h"
#include "preferences_store.h"
// SPDX-License-Identifier: GPL-2.0-or-later
#include "performance_dialog.h"
#include "core/development_calculation_samples.h"
#include "core/principal_dimensions.h"
#include "calculation_input_status.h"
#include "performance_input_rules.h"
#include "document/calculation_tools.h"
#include "studio_ui.h"
#include "data_motion.h"
#include "studio_table.h"
#include "studio_numeric.h"
#include "calculation_pages.h"
#include "calculation_plot.h"
#include "maneuver_report.h"
#include "core/water_viscosity.h"
#include <QInputDialog>
#include "grid_preview_panel.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFile>
#include <QFormLayout>
#include <QGridLayout>
#include <QScrollBar>
#include <QResizeEvent>
#include <QSignalBlocker>
#include <QGroupBox>
#include <QHeaderView>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QMap>
#include <QPainter>
#include <QPainterPath>
#include <QPdfWriter>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QSaveFile>
#include <QScrollArea>
#include <QSettings>
#include <QSplitter>
#include <QTabWidget>
#include <QTableView>
#include <QAbstractTableModel>
#include <QTextDocument>
#include <QRegularExpression>
#include "report_browser.h"
#include "calculation_method_help.h"
#include <QTimer>
#include <QProcess>
#include <QTemporaryDir>
#include <QDir>
#include <QFileInfo>
#include <algorithm>
#include <cmath>
#include <tuple>
#ifdef Q_OS_WIN
#include <windows.h>
#endif
namespace {
QJsonObject parameterPages(const QString& method){
    static const auto pages=QJsonDocument::fromJson(CalculationPages).object();return pages[method].toObject();
}
QString full(double n){return QString::number(n,'g',17);}
QString quote(QString s){return '"'+s.replace('"',"\"\"")+'"';}
QString reportValue(const QJsonValue& value){
    if(value.isDouble())return QString::number(value.toDouble(),'g',10);
    if(value.isNull()||value.isUndefined())return QStringLiteral("—");
    return value.toVariant().toString();
}
QStringList pipeCells(const QString& line){
    if(!line.contains('|'))return {};
    auto cells=line.split('|');
    if(cells.first().trimmed().isEmpty())cells.removeFirst();
    if(!cells.isEmpty()&&cells.last().trimmed().isEmpty())cells.removeLast();
    for(auto& cell:cells)cell=cell.trimmed();
    return cells;
}
QStringList numericCells(const QString& line){
    static const QRegularExpression separator("\\s+");
    static const QRegularExpression number(R"(^[+-]?(?:\d+(?:[.,]\d*)?|[.,]\d+)(?:[eEdD][+-]?\d+)?$)");
    auto cells=line.trimmed().split(separator);
    if(cells.size()<2)return {};
    for(const auto& cell:cells)if(cell!="—"&&!number.match(cell).hasMatch())return {};
    return cells;
}
QString tableCurveHeading(const QJsonObject& curve){
    const auto name=uiMetadataText(curve,"name").trimmed();
    const auto axis=uiMetadataText(curve,"y_title").trimmed();
    if(curve["y_title"]=="10 × 转矩系数 KQ")return QStringLiteral("10 × ")+name;
    if(axis=="0.1 × r/min")return name+" (r/min)";
    const auto measured=notation::splitLabelUnit(axis);
    if(!measured.second.isEmpty())return (name.isEmpty()?measured.first:name)+" ("+measured.second+")";
    if(!notation::unitLatex(axis).isEmpty())return name.isEmpty()?axis:name+" ("+notation::unit(axis)+")";
    const auto comma=axis.lastIndexOf(QRegularExpression("[,，]"));
    if(comma>=0){
        const auto unit=axis.mid(comma+1).trimmed();
        if(!notation::unitLatex(unit).isEmpty())return name+" ("+notation::unit(unit)+")";
    }
    return name.isEmpty()?axis:name;
}
QList<QJsonArray> curveTables(const QJsonArray& series,const QString& method){
    QList<QJsonArray> groups;
    QHash<QString,qsizetype> byAxis;
    for(const auto& entry:series){
        // The working-point line uses artificial y coordinates to mark a single J.
        if((method=="propeller4"||method=="propeller5")&&entry.toObject()["source"]=="Series4")continue;
        // Use the solver's axis identity, before translation. The units are
        // part of that identity. Empty axis labels deliberately stand alone.
        const auto axis=entry.toObject()["x_title"].toString().simplified();
        if(!axis.isEmpty()&&byAxis.contains(axis))groups[byAxis.value(axis)].append(entry);
        else{
            if(!axis.isEmpty())byAxis.insert(axis,groups.size());
            groups.append(QJsonArray{entry});
        }
    }
    return groups;
}
QPair<QString,QString> legacyMeasurement(const QString& text){
    static const QRegularExpression valueUnit(
        QStringLiteral(R"(^([+−-]?(?:\d+(?:[.,]\d*)?|[.,]\d+)(?:[eEdD][+-]?\d+|\s*[*×]\s*10\^(?:\([+-]?\d+\)|\{[+-]?\d+\}|[+-]?\d+))?)(?:\s+(.{1,30})|([%°]))$)"));
    const auto match=valueUnit.match(text.trimmed());
    auto unit=match.captured(2).isEmpty()?match.captured(3):match.captured(2);
    if(unit.startsWith('[')&&unit.endsWith(']'))unit=unit.mid(1,unit.size()-2).trimmed();
    if(!match.hasMatch()||notation::unitLatex(unit).isEmpty())return {};
    return {match.captured(1),notation::unit(unit)};
}
void separatePipeRowUnits(report::Table& table){
    if(table.headers.size()<3)return;
    static const QRegularExpression number(R"(^[+−-]?\d)");
    if(std::any_of(table.rows.begin(),table.rows.end(),[](const auto& row){return number.match(row.first()).hasMatch();}))return;
    bool hasUnits=false;
    for(auto& row:table.rows){
        const auto first=legacyMeasurement(row.value(1));
        if(first.second.isEmpty())continue;
        QStringList values;
        for(qsizetype column=1;column<row.size();++column){
            const auto item=legacyMeasurement(row[column]);
            if(item.second!=first.second)break;
            values.append(item.first);
        }
        if(values.size()!=row.size()-1)continue;
        row={row.first()};
        row.append(values);
        row.append(first.second);
        hasUnits=true;
    }
    if(hasUnits){
        table.leftColumns={0,int(table.headers.size())};
        table.headers.append(uiText("单位"));
    }
}
QString legacyResult(const QString& source,bool html,const QJsonArray& warnings){
    const QStringList labels{uiText("结果量"),uiText("数值"),uiText("单位")};
    report::Table fields{{},labels,{}, {0,2}};
    QString text;QStringList notes,rangeNotices;
    const auto flush=[&]{
        text+=report::render(fields,html);fields.rows.clear();
        text+=report::metadata(notes.join('\n'),html);notes.clear();
        text+=report::message(rangeNotices.join('\n'),html,StudioMessageRole::Warning);rangeNotices.clear();
    };
    const auto lines=source.split('\n');
    static const QRegularExpression field(R"(^([^=：:。；;]{1,120})\s*[=：:]\s*(\S.*)$)");
    static const QRegularExpression spaceField(R"(^([^0-9=：:。；;]{1,120}?)\s+([+-]?\d[^。；;]*)$)");
    static const QRegularExpression divider(R"(^[-=+|*\s]+$)");
    static const QRegularExpression metadata(R"(^(?:PROGRAM|DATE|TIME|RUN IDENTIFICATION)\s*[:=])",QRegularExpression::CaseInsensitiveOption);
    static const QRegularExpression wideSpace("\\s{2,}");
    static const QRegularExpression bracketedUnit(R"(^\[\s*([^\[\]]*?)\s*\]$)");
    static const QRegularExpression rangeNotice(R"(超出有效范围|outside (?:the )?valid domain|^(?:注意|ATTENTION)\s*[:：])",QRegularExpression::CaseInsensitiveOption);
    for(int i=0;i<lines.size();++i){
        auto line=lines[i].trimmed();
        line.replace("&percent;","%");
        if(line.isEmpty()||divider.match(line).hasMatch()){flush();continue;}
        if(metadata.match(line).hasMatch())continue;
        // Warnings are supplied by the calculation core, not inferred from punctuation.
        bool isWarning=false;for(const auto& warning:warnings){const auto value=warning.toString().trimmed();if(!value.isEmpty()&&(line==value||line==QStringLiteral("提示：")+value)){isWarning=true;break;}}
        if(isWarning){flush();continue;} // The explicit warnings block below preserves each warning once.
        if(rangeNotice.match(line).hasMatch()){
            if(!fields.rows.isEmpty()||!notes.isEmpty())flush();
            rangeNotices.append(uiText(line));continue;
        }
        if(!rangeNotices.isEmpty())flush();
        const auto headers=pipeCells(line);
        if(headers.size()>1&&i+1<lines.size()){
            int next=i+1;
            if(divider.match(lines[next]).hasMatch())++next;
            auto translatedHeaders=headers;for(auto& title:translatedHeaders)title=uiText(title);
            if(next<lines.size()){
                const auto units=pipeCells(lines[next]);
                if(units.size()==headers.size()&&
                   std::any_of(units.begin(),units.end(),[](const QString& unit){return bracketedUnit.match(unit).hasMatch();})&&
                   std::all_of(units.begin(),units.end(),[](const QString& unit){
                       return bracketedUnit.match(unit).hasMatch()||unit.isEmpty()||unit=="-"||unit=="—"||
                           unit=="比值"||unit.compare("ratio",Qt::CaseInsensitive)==0;
                   })){
                    for(qsizetype column=0;column<units.size();++column){
                        const auto unit=bracketedUnit.match(units[column]).captured(1);
                        if(!unit.isEmpty()&&unit!="-"&&unit!="—"&&notation::splitLabelUnit(translatedHeaders[column]).second.isEmpty())
                            translatedHeaders[column]+=" ("+notation::unit(unit)+")";
                    }
                    ++next;
                    while(next<lines.size()&&divider.match(lines[next]).hasMatch())++next;
                }
            }
            report::Table table{{},translatedHeaders,{}, {}};
            for(;next<lines.size();++next){
                const auto cells=pipeCells(lines[next]);if(cells.size()!=headers.size()||divider.match(lines[next]).hasMatch())break;
                QStringList row;
                for(const auto& cell:cells)row.append(formatNumericRanges(cell));
                table.rows.append(row);
            }
            if(!table.rows.isEmpty()){
                separatePipeRowUnits(table);
                flush();text+=report::render(table,html);i=next-1;continue;
            }
        }
        const auto numbers=numericCells(line);
        if(!numbers.isEmpty()&&i+1<lines.size()&&numericCells(lines[i+1]).size()==numbers.size()){
            report::Table table{{},{},{numbers},{}};
            if(!notes.isEmpty()){
                auto columns=notes.last().split(wideSpace);
                if(columns.size()!=numbers.size())columns=notes.last().split(" / ");
                if(columns.size()==numbers.size()){for(auto& title:columns)title=uiText(title);table.headers=columns;notes.removeLast();}
            }
            if(table.headers.isEmpty())for(int column=0;column<numbers.size();++column)
                table.headers.append(QString::number(column+1));
            int next=i+1;
            for(;next<lines.size();++next){
                const auto cells=numericCells(lines[next]);if(cells.size()!=numbers.size())break;
                table.rows.append(cells);
            }
            flush();text+=report::render(table,html);i=next-1;continue;
        }
        static const QString holtropNote="注：系数 Wt、t 和 EtaR 按 Holtrop 方法的公式计算 -";
        if(line.startsWith(holtropNote)){
            if(!fields.rows.isEmpty())flush();
            notes.append(uiText(holtropNote)+line.mid(holtropNote.size()));continue;
        }
        QList<QStringList> parsed;
        static const QRegularExpression entrySeparator("[；;]");
        const auto parts=line.split(entrySeparator);
        for(const auto& part:parts){
            auto match=field.match(part.trimmed());
            const bool explicitAssignment=match.hasMatch();
            if(parts.size()>1&&!explicitAssignment)match=spaceField.match(part.trimmed());
            if(!match.hasMatch()||match.captured(2).startsWith("//")||
               (parts.size()>1&&((explicitAssignment&&!part.contains('='))||match.captured(2).endsWith(QChar(0x3002))))){parsed.clear();break;}
            // The label is program report metadata. The value may be a model
            // name/path and must remain byte-for-byte as supplied.
            auto labelSource=match.captured(1).trimmed();
            auto caption=uiText(labelSource);
            // The legacy dictionaries include the padded assignment up to
            // '='. Reuse that key before formatting the label and value into
            // separate cells. Leave the value itself outside translation.
            const auto assignment=part.trimmed().left(match.capturedStart(2)).trimmed();
            if(caption==labelSource&&explicitAssignment&&assignment.endsWith('=')){
                auto translated=uiText(assignment);
                if(translated!=assignment&&translated.endsWith('='))caption=translated.chopped(1).simplified();
            }
            auto label=notation::splitLabelUnit(caption);
            auto value=formatNumericRanges(match.captured(2).trimmed());
            // Only the known numerical annotations are product captions.
            // A project name, designer or arbitrary solver payload remains intact.
            static const QRegularExpression annotated(QStringLiteral("^([+-]?[0-9.]+) (- (?:计算|用于选择螺旋桨图谱|按螺旋桨最佳频率曲线))$"));
            if(const auto note=annotated.match(value);note.hasMatch())
                value=note.captured(1)+" "+uiText(note.captured(2));
            const auto alternatives=value.split('=');
            if(alternatives.size()>1){
                QList<QStringList> conversions;
                for(const auto& alternative:alternatives){
                    const auto [number,unit]=legacyMeasurement(alternative);
                    if(unit.isEmpty()){conversions.clear();break;}
                    conversions.append({label.first,number,unit});
                }
                if(!conversions.isEmpty()){parsed.append(conversions);continue;}
            }
            const auto [number,unit]=legacyMeasurement(value);
            if(!unit.isEmpty()){
                label.second=unit;
                value=number;
            }
            parsed.append({label.first,value,label.second});
        }
        if(!parsed.isEmpty()){
            if(!notes.isEmpty())flush();
            fields.rows.append(parsed);
        }else{
            if(!fields.rows.isEmpty())flush();
            const auto series=QRegularExpression(QStringLiteral("^螺旋桨 ([A-Z]+) (I\\.Titov 系列|V\\.Mishkevich 系列|M\\.Zavadovsky 系列|V\\.Turbal 系列|KSRI-BIHS 系列)$")).match(line);
            if(series.hasMatch())notes.append(uiText(QStringLiteral("螺旋桨"))+" "+series.captured(1)+" · "+uiText(series.captured(2)));
            else if(line.endsWith(':')&&(line.chopped(1).trimmed()=="工程"||line.chopped(1).trimmed()=="设计者"))
                notes.append(uiText(line.chopped(1).trimmed())+" :");
            else if(line=="根据副教授 Timoshenko 方法，估计海上船舶阻力.")
                notes.append(QString(uiText("根据副教授 Timoshenko 方法，估计海上船舶阻力"))+".");
            else notes.append(uiText(line));
        }
    }
    flush();return text;
}
bool sharedAxis(const QJsonArray& series){
    QVector<QJsonArray> shared;bool aligned=!series.isEmpty();
        for(const auto& value:series){
            const auto samples=value.toObject()["points"].toArray();
            if(!shared.isEmpty()&&samples.size()!=shared.first().size()){aligned=false;break;}
            double previous=-INFINITY;qsizetype index=0;
            for(const auto& value:samples){const auto point=value.toArray();
                if(point.size()<2||!point[0].isDouble()||!std::isfinite(point[0].toDouble())||point[0].toDouble()<previous||
                   (!shared.isEmpty()&&point[0]!=shared.first()[index].toArray()[0])){aligned=false;break;}
                previous=point[0].toDouble();++index;
            }
            if(!aligned)break;shared.append(samples);
        }
    return aligned;
}
class PerformanceTable final:public QAbstractTableModel {
    QJsonArray points_,rowHeaders_;QStringList headers_;
    QVector<QJsonArray> sharedCurves_;
public:
    explicit PerformanceTable(QObject* parent):QAbstractTableModel(parent){}
    static QStringList curveHeaders(const QJsonArray& series){
        if(series.isEmpty())return {};
        const auto axis=notation::splitLabelUnit(uiMetadataText(series.first().toObject(),"x_title").trimmed());
        QStringList headers{axis.second.isEmpty()?axis.first:axis.first+" ("+axis.second+")"};
        for(const auto& value:series)headers.append(tableCurveHeading(value.toObject()));
        return headers;
    }
    void refreshHeaders(const QVariant& dataset,bool table){
        if(table){
            headers_.clear();for(auto value:dataset.toJsonObject()["columns"].toArray())headers_<<uiText(value.toVariant().toString());
        }else headers_=curveHeaders(dataset.toJsonArray());
        if(!headers_.isEmpty())emit headerDataChanged(Qt::Horizontal,0,headers_.size()-1);
    }
    void setCurves(const QJsonArray& series,bool axisVerified=false){
        auto headers=curveHeaders(series);
        QMap<QPair<double,int>,QJsonArray> rows;
        // Time histories normally share an ordered time axis. Keep their
        // immutable arrays instead of allocating and sorting every table cell.
        QVector<QJsonArray> shared;const bool aligned=axisVerified||sharedAxis(series);
        if(aligned)for(const auto& value:series)shared.append(value.toObject()["points"].toArray());
        if(aligned){
            beginResetModel();headers_=headers;points_={};rowHeaders_={};sharedCurves_=std::move(shared);endResetModel();return;
        }
        sharedCurves_.clear();
        for(qsizetype column=0;column<series.size();++column){
            const auto curve=series[column].toObject();
            QHash<double,int> occurrences;
            for(const auto& sample:curve["points"].toArray()){
                const auto point=sample.toArray();
                if(point.size()<2||!point[0].isDouble())continue;
                const double x=point[0].toDouble();
                // A trajectory can contain two states at the same instant.
                // Keep both samples instead of overwriting the first one.
                auto& row=rows[{x,occurrences[x]++}];
                if(row.isEmpty()){
                    row.append(point[0]);
                    for(qsizetype index=0;index<series.size();++index)row.append(QJsonValue{});
                }
                row[column+1]=point[1];
            }
        }
        beginResetModel();
        headers_=std::move(headers);
        rowHeaders_={};
        points_={};
        for(const auto& row:rows)points_.append(row);
        endResetModel();
    }
    void setTable(const QJsonObject& t){beginResetModel();sharedCurves_.clear();points_=t["values"].toArray();rowHeaders_=t["rows"].toArray();headers_.clear();for(auto v:t["columns"].toArray())headers_<<uiText(v.toVariant().toString());endResetModel();}
    int rowCount(const QModelIndex& parent={})const override{return parent.isValid()?0:int(sharedCurves_.isEmpty()?points_.size():sharedCurves_.first().size());}
    int columnCount(const QModelIndex& parent={})const override{return parent.isValid()?0:headers_.size();}
    QVariant data(const QModelIndex& i,int role)const override{
        if(!i.isValid()||i.row()>=rowCount()||i.column()>=headers_.size())return {};
        if(role==Qt::TextAlignmentRole)return int(Qt::AlignRight|Qt::AlignVCenter);
        if(role!=Qt::DisplayRole&&role!=Qt::ToolTipRole)return {};
        if(!sharedCurves_.isEmpty()){
            const auto point=sharedCurves_[i.column()==0?0:i.column()-1][i.row()].toArray();
            const auto value=point[i.column()==0?0:1];
            if(value.isNull())return QStringLiteral("—");
            return QString::number(value.toDouble(),'g',role==Qt::ToolTipRole?17:10);
        }
        const auto row=points_[i.row()].toArray();
        if(i.column()>=row.size())return {};
        const auto value=row[i.column()];
        if(value.isNull())return QStringLiteral("—");
        return QString::number(value.toDouble(),'g',role==Qt::ToolTipRole?17:10);
    }
    QVariant headerData(int section,Qt::Orientation orientation,int role)const override{if(role!=Qt::DisplayRole)return {};if(orientation==Qt::Vertical)return section<rowHeaders_.size()?rowHeaders_[section].toVariant():QVariant(section+1);return section<headers_.size()?QVariant(headers_[section]):QVariant{};}
};
}
QString PerformanceDialog::defaultSolverDirectory(){
    return fs::document::calculationToolDirectory(QSettings().value("calculation/solver-directory").toString());
}
PerformanceDialog::PerformanceDialog(ModelSession& session,const QJsonObject& schema,bool hidden,QWidget* parent)
    :QDialog(parent),session_(session),method_(schema["calculator"].toString()),hidden_(hidden){
    if(session_.model())principalSource_=session_.model()->principalDimensions;
    setStudioWindowIcon(this,"Calculate");
    if(method_=="acceleration")CalculationPlot::prepareRenderer();
    setAttribute(Qt::WA_DontShowOnScreen,hidden);setProperty("calculation_title_source",schema["title"].toString());setWindowTitle(uiText(schema["title"].toString()));resize(1280,820);setMinimumSize(740,480);
    auto* outer=new QVBoxLayout(this);auto* toolbar=new QHBoxLayout;outer->addLayout(toolbar);toolbar->addStretch();
    pages_=new QTabWidget;pages_->setObjectName("PerformancePages");pages_->setProperty("calculation_pages",true);outer->addWidget(pages_,1);
    auto* split=new QSplitter;split->setProperty("calculation_page","general");pages_->addTab(split,uiText("常规"));
    additional_=new QScrollArea;additional_->setWidgetResizable(true);additional_->setFrameShape(QFrame::NoFrame);
    additional_->setProperty("calculation_parameter_section","additional");
    additional_->viewport()->installEventFilter(this);
    auto* left=new QWidget;auto* form=new QVBoxLayout(left);form->setContentsMargins(0,0,5,0);
    parameters_=new QScrollArea;parameters_->setWidgetResizable(true);parameters_->setFrameShape(QFrame::NoFrame);form->addWidget(parameters_,1);
    const auto solverPreference=preferenceDirectory("solver");
    const auto configured=hidden?QString{}:solverPreference.isEmpty()?defaultSolverDirectory():fs::document::calculationToolDirectory(solverPreference);
    solverDirectory_=configured.isEmpty()?schema["solver_directory"].toString():configured;
    auto button=[&](const QString& title,const QString& operation){auto* b=new QPushButton(title);b->setProperty("performance_action",operation);return b;};
    run_=button(uiText("计算"),"calculate");run_->setIcon(studioIcon("Calculate"));run_->setDefault(true);
    if(qApp->property("freeship_development").toBool()&&!fs::developmentCalculation::sample(method_).isEmpty()){
        sampleButton_=button(uiText("载入测试参数"),"load_development_sample");
        toolbar->addWidget(sampleButton_);
        connect(sampleButton_,&QPushButton::clicked,this,&PerformanceDialog::loadDevelopmentSample);
    }
    cancel_=button(uiText("取消计算"),"cancel");cancel_->setIcon(studioIcon("Cancel"));toolbar->addWidget(run_);toolbar->addWidget(cancel_);
    left->setMinimumWidth(300);split->addWidget(left);
    split->addWidget(additional_);split->setChildrenCollapsible(false);
    split->setSizes({560,640});
    if(method_=="propeller4"){
        databaseButton_=button(uiText("数据库查看器…"),"database_viewer");form->addWidget(databaseButton_);
        connect(databaseButton_,&QPushButton::clicked,this,&PerformanceDialog::openDatabaseViewer);
    }
    auto* presetRow=new QHBoxLayout;auto* presetSave=button(uiText("保存参数…"),"save_parameters");auto* presetLoad=button(uiText("载入参数…"),"load_parameters");
    presetSave->setIcon(studioIcon("SaveAs"));presetLoad->setIcon(studioIcon("OpenFile"));
    presetRow->addWidget(presetSave);presetRow->addWidget(presetLoad);presetRow->addStretch();outer->addLayout(presetRow);
    connect(presetSave,&QPushButton::clicked,this,[this]{const auto path=QFileDialog::getSaveFileName(this,uiText("保存计算参数"),preferenceFileLocation("save"),uiText("计算参数 (*.json)"));if(!path.isEmpty()){auto err=saveParameters(path);status_->setMessage(err.isEmpty()?uiText("参数已保存。"):err,err.isEmpty()?StudioMessageRole::Feedback:StudioMessageRole::Warning);}});
    connect(presetLoad,&QPushButton::clicked,this,[this]{const auto path=QFileDialog::getOpenFileName(this,uiText("载入计算参数"),preferenceFileLocation("open"),uiText("计算参数 (*.json)"));if(!path.isEmpty()){auto err=loadParameters(path);if(!err.isEmpty())showFailure(err);}});
    auto* results=new QWidget;results->setProperty("calculation_page","results");auto* resultsLayout=new QVBoxLayout(results);resultsLayout->setContentsMargins(0,0,0,0);
    tabs_=new QTabWidget;resultsLayout->addWidget(tabs_,1);pages_->addTab(results,uiText("结果"));
    auto* tablePage=new QWidget;auto* tableLayout=new QVBoxLayout(tablePage);tableLayout->setContentsMargins(0,0,0,0);
    auto* chooser=new QWidget(tablePage);
    auto* chooserLayout=new QHBoxLayout(chooser);chooserLayout->setContentsMargins(0,0,0,0);
    auto* tableLabel=new QLabel(uiText("查看表格"),chooser);
    series_=new QComboBox(chooser);series_->setProperty("performance_series",true);
    series_->setSizeAdjustPolicy(QComboBox::AdjustToContents);
    tableLabel->setBuddy(series_);chooserLayout->addWidget(tableLabel);chooserLayout->addWidget(series_);
    chooserLayout->addStretch();tableLayout->addWidget(chooser);chooser->hide();
    connect(series_,qOverload<int>(&QComboBox::currentIndexChanged),this,[this](int){renderSeries();});
    table_=new QTableView;table_->setProperty("performance_table",true);table_->setModel(new PerformanceTable(table_));table_->setEditTriggers(QAbstractItemView::NoEditTriggers);table_->setAlternatingRowColors(false);tableLayout->addWidget(table_,1);tabs_->addTab(tablePage,uiText("曲线数据"));
    applyStudioScrollbars(table_);
    const bool addedMassTable=method_=="addmass";
    auto font=table_->font();
    font.setPointSizeF(std::max(13.0,font.pointSizeF()));
    table_->setFont(font);
    configureStudioDataTable(table_,true,-1,addedMassTable);
    plot_=new CalculationPlot;tabs_->addTab(plot_,uiText("曲线"));text_=new report::Browser;text_->setObjectName("PerformanceReport");tabs_->addTab(text_,uiText("报告"));
    help_=new report::Browser;help_->setObjectName("PerformanceHelp");help_->setProperty("calculation_page","help");pages_->addTab(help_,uiText("方法说明"));
    status_=new StudioMessage(StudioMessageRole::Feedback);status_->enableMathNotation();status_->setObjectName("PerformanceStatus");outer->addWidget(status_);
    int exportPosition=0;toolbar->setSpacing(8);
    for(const auto& entry:QList<QPair<QString,QString>>{{"csv","表格数据"},{"txt","纯文本报告"},{"json","结构化数据"},{"pdf","打印版报告"}}){
        const auto& format=entry.first;
        auto* control=new QPushButton(this);control->setProperty("performance_export",format);
        const auto refreshFormat=[control,entry]{setStudioFormatButton(control,entry.first,QString(uiText("导出 "))+entry.first.toUpper()+" · "+uiText(entry.second));};
        refreshFormat();bindUiRefresh(control,refreshFormat);
        exports_.append(control);toolbar->insertWidget(exportPosition++,control);
        connect(control,&QPushButton::clicked,this,[this,format]{const auto path=QFileDialog::getSaveFileName(this,uiText("保存计算报告"),preferenceFileLocation("export"),format.toUpper()+" (*."+format+")");if(path.isEmpty())return;const auto err=exportReport(path,format);status_->setMessage(err.isEmpty()?uiText("报告已保存：")+path:err,err.isEmpty()?StudioMessageRole::Feedback:StudioMessageRole::Warning);});
    }
    connect(run_,&QPushButton::clicked,this,&PerformanceDialog::calculate);
    connect(cancel_,&QPushButton::clicked,this,[this]{if(preparingResult_){++resultGeneration_;preparingResult_=false;showFailure(uiText("计算已取消。"));return;}session_.cancelPerformance();if(session_.busy())status_->setMessage(uiText("正在取消计算…"),StudioMessageRole::Feedback,true);});
    setParameters(schema);
    if(method_=="maneuver"){tabs_->setTabVisible(0,false);tabs_->setTabVisible(1,false);tabs_->setCurrentWidget(text_);}
    if(method_=="addmass"){tabs_->setTabText(0,uiText("计算数据"));tabs_->setTabVisible(1,false);}
    if(method_=="addmass"){preview_=new GridPreviewPanel(session_,"addmass",[this]{auto values=inputs()["values"].toObject();values["object_type"]=values["object_type"].toInt()+1;values["panel_type"]=values["panel_type"].toInt()+1;return values;});tabs_->addTab(preview_,uiText("型值预览"));
        for(auto* field:fields_){if(auto* n=qobject_cast<QDoubleSpinBox*>(field))connect(n,qOverload<double>(&QDoubleSpinBox::valueChanged),preview_,[this]{preview_->invalidate();});else if(auto* c=qobject_cast<QComboBox*>(field))connect(c,qOverload<int>(&QComboBox::currentIndexChanged),preview_,[this]{preview_->invalidate();});}}
    refreshState();
}
void PerformanceDialog::convertViscosityFromTemperature(){
    if(session_.busy())return;
    QInputDialog dialog(this);dialog.setAttribute(Qt::WA_DontShowOnScreen,hidden_);
    dialog.setWindowTitle(uiText("水温换算"));dialog.setLabelText(uiText("水温（°C，0–30）"));
    dialog.setInputMode(QInputDialog::DoubleInput);dialog.setDoubleRange(0,30);dialog.setDoubleDecimals(2);dialog.setDoubleValue(15);
    if(dialog.exec()!=QDialog::Accepted)return;
    const auto key=method_=="maneuver"?"Edit11":"WaterViscosity";
    if(auto* field=qobject_cast<QDoubleSpinBox*>(fields_.value(key))){
        const double value=fs::waterViscosity(float(dialog.doubleValue()));
        field->setValue(value);field->setProperty("performance_exact_value",value);
    }
}
void PerformanceDialog::loadDevelopmentSample(){
    if(session_.busy()||preparingResult_||choicesPending_)return;
    const auto model=session_.model();
    auto values=fs::developmentCalculation::sampleForUnits(method_,model&&model->units=="imperial");
    for(auto it=values.begin();it!=values.end();){
        if(!fields_.contains(it.key()))it=values.erase(it);
        else ++it;
    }
    if(!session_.performanceDefaults({{"calculator",method_},{"values",values},
        {"solver_directory",solverDirectory_.trimmed()},{"report_language",uiLanguage()},{"development_sample",true}}))
        showFailure(uiText("无法载入计算参数。"));
}
namespace {
QString parameterCaption(const QString& method,const QJsonObject& field,const QJsonObject& values){
    const auto title=methodHelp::parameterLabel(method,field,values);
    const auto unit=notation::unit(field["unit"].toString());
    if(!unit.isEmpty())return title+" ("+unit+")";
    const auto [label,legacyUnit]=notation::splitLabelUnit(title);
    return legacyUnit.isEmpty()?title:label+" ("+legacyUnit+")";
}
}
void PerformanceDialog::setParameters(const QJsonObject& schema){
    additionalLayout_=nullptr;
    const int generalScroll=parameters_->verticalScrollBar()->value(),extraScroll=additional_->verticalScrollBar()->value();
    const quint64 generation=++parameterGeneration_;
    schema_=schema;fields_.clear();auto* body=new QWidget;auto* layout=new QVBoxLayout(body);QMap<QString,QGridLayout*> groups;
    auto* extraBody=new QWidget;auto* extraLayout=new QGridLayout(extraBody);additionalGroups_.clear();
    for(int i=0;i<2;++i){auto* column=new QWidget(extraBody);additionalColumnLayouts_[i]=new QVBoxLayout(column);additionalColumnLayouts_[i]->setContentsMargins(0,0,0,0);additionalColumnLayouts_[i]->addStretch();extraLayout->addWidget(column,0,i);}
    const auto pageMap=parameterPages(method_);bool hasAdditional=false;
    QJsonObject values;for(const auto& item:schema["fields"].toArray()){const auto field=item.toObject();values[field["key"].toString()]=field["value"];}
    for(auto v:schema["fields"].toArray()){
        const auto f=v.toObject();const auto key=f["key"].toString(),group=f["group"].toString();
        const bool extra=pageMap[key]=="additional";const auto groupKey=(extra?"additional/":"general/")+group;
        if(!groups.contains(groupKey)){auto* box=new QGroupBox(uiText(group));auto* rows=new QGridLayout(box);rows->setVerticalSpacing(12);rows->setHorizontalSpacing(16);rows->setColumnStretch(0,1);groups[groupKey]=rows;
            if(extra){additionalColumnLayouts_[0]->insertWidget(additionalColumnLayouts_[0]->count()-1,box);additionalGroups_.append(box);hasAdditional=true;}else layout->addWidget(box);}
        QWidget* widget=nullptr;const auto kind=f["kind"].toString();
        if(kind=="number"){
            auto* n=new QDoubleSpinBox;n->setDecimals(std::max(0,std::min(12,f["decimals"].toInt())));
            n->setRange(f["minimum"].toDouble(),f["maximum"].toDouble());n->setValue(f["value"].toDouble());
            n->setProperty("performance_exact_value",f["value"].toDouble());n->setKeyboardTracking(false);widget=n;
            connect(n,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this,n,key,precise=f["double_precision"].toBool()](double value){
                const double previous=n->property("performance_exact_value").toDouble();
                n->setProperty("performance_exact_value",precise?value:double(float(value)));
                if(!result_.isEmpty())status_->setMessage(uiText("参数已更改，结果待更新。"),StudioMessageRole::Warning);
                if(method_=="acceleration"&&key=="Edit18")refreshAccelerationStopControls();
                if((method_=="propeller1"&&key=="Edit14")||(method_=="propeller2"&&key=="Edit12")||
                    (method_=="propeller3"&&key=="Edit7")||(key=="KeBox"&&fields_.contains("A11Box")&&((previous==0)!=(value==0))))
                    refreshChoices();
            });
        }else if(kind=="boolean"){
            auto* n=new QCheckBox;n->setChecked(f["value"].toBool());widget=n;
            connect(n,&QCheckBox::toggled,this,[this,key]{
                if(method_=="acceleration"&&key=="StopAtSpeed")refreshAccelerationStopControls();
                if(!result_.isEmpty())status_->setMessage(uiText("参数已更改，结果待更新。"),StudioMessageRole::Warning);
                if(key=="Estimate2Box"&&fields_.contains("A11Box"))refreshChoices();
            });
        }
        else {auto* n=new QComboBox;n->setSizeAdjustPolicy(QComboBox::AdjustToContents);for(auto c:f["choices"].toArray())n->addItem(uiText(c.toString()));n->setCurrentIndex(f["value"].toInt());widget=n;connect(n,qOverload<int>(&QComboBox::currentIndexChanged),this,[this,key]{if(!result_.isEmpty())status_->setMessage(uiText("参数已更改，结果待更新。"),StudioMessageRole::Warning);if((method_=="propeller5"&&(key=="ComboBox"||key=="BSeriesReynoldsMode"))||key=="DesignSolver"||(method_=="oortmerssen"&&(key=="ComboBox"||key=="OortSolver"))||(method_=="hollenbach"&&(key=="HolSolver"||key=="HolCondition"))||(method_=="fung"&&key=="FungSolver"))refreshChoices();});}
        fields_[key]=widget;widget->setProperty("performance_field",key);
        widget->setProperty("calculation_parameter_section",extra?"additional":"general");
        widget->setProperty("calculation_parameter_page","general");widget->setProperty("calculation_declared_visible",f["visible"].toBool(true));
        if(preview_){if(auto* n=qobject_cast<QDoubleSpinBox*>(widget))connect(n,qOverload<double>(&QDoubleSpinBox::valueChanged),preview_,[this]{preview_->invalidate();});else if(auto* c=qobject_cast<QComboBox*>(widget))connect(c,qOverload<int>(&QComboBox::currentIndexChanged),preview_,[this]{preview_->invalidate();});}
        widget->setEnabled(f["enabled"].toBool(true));
        const auto title=parameterCaption(method_,f,values);
        auto* label=new QLabel(notation::html(title));label->setTextFormat(Qt::RichText);label->setProperty("performance_label",key);label->setWordWrap(true);label->setMinimumWidth(100);label->setBuddy(widget);auto* rows=groups[groupKey];const int row=rows->rowCount();
        if(method_=="wind")if(auto* number=qobject_cast<QDoubleSpinBox*>(widget))connect(number,qOverload<double>(&QDoubleSpinBox::valueChanged),this,[this]{refreshWindLabels();});
        auto* caption=calculationInput::caption(label,widget,key,true);
        widget->setProperty("calculation_declared_enabled",f["enabled"].toBool(true));
        calculationInput::watch(widget,this,[this]{run_->setEnabled(!session_.busy()&&refreshInputValidity());});
        rows->addWidget(caption,row,0,Qt::AlignVCenter);
        rows->addWidget(widget,row,2,Qt::AlignLeft|Qt::AlignVCenter);
        if((method_=="maneuver"&&key=="Edit11")||(method_=="propeller4"&&key=="WaterViscosity")){
            auto* temperature=new QPushButton(uiText("按水温换算黏度…"));
            temperature->setProperty("performance_action","water_temperature");
            connect(temperature,&QPushButton::clicked,this,&PerformanceDialog::convertViscosityFromTemperature);
            rows->addWidget(temperature,row,1,Qt::AlignRight|Qt::AlignVCenter);
            temperature->setVisible(f["visible"].toBool(true));
        }
        if(!f["visible"].toBool(true)){caption->hide();widget->hide();}
    }
    refreshAccelerationStopControls();
    configureStudioNumericForms(body);configureStudioNumericForms(extraBody);
    layout->addStretch();parameters_->setWidget(body);
    additional_->setWidget(extraBody);additionalLayout_=extraLayout;additionalColumns_=0;
    additional_->setVisible(hasAdditional);arrangeAdditional();
    const auto helpText=methodHelp::document(method_,result_);
    help_->setReport(helpText);
    help_->moveCursor(QTextCursor::Start);help_->verticalScrollBar()->setValue(0);
    pages_->setTabVisible(pages_->indexOf(help_),!helpText.trimmed().isEmpty());refreshWindLabels();
    QTimer::singleShot(0,this,[this,generation,generalScroll,extraScroll]{
        if(generation!=parameterGeneration_)return;
        parameters_->verticalScrollBar()->setValue(generalScroll);additional_->verticalScrollBar()->setValue(extraScroll);
    });
    if(preview_)preview_->invalidate();
}
void PerformanceDialog::refreshWindLabels(){
    if(method_!="wind")return;
    const auto values=inputs()["values"].toObject();
    for(auto* label:findChildren<QLabel*>()){
        const auto key=label->property("performance_label").toString();if(key!="Edit9")continue;
        const auto title=parameterCaption(method_,QJsonObject{{"key",key}},values);
        label->setText(notation::html(title));
    }
}
void PerformanceDialog::arrangeAdditional(){
    if(!additionalLayout_)return;
    int columnWidth=340;
    for(auto* group:additionalGroups_)columnWidth=std::max(columnWidth,group->minimumSizeHint().width());
    columnWidth+=additionalLayout_->horizontalSpacing()+additionalLayout_->contentsMargins().left();
    const int columns=additional_->viewport()->width()>=2*columnWidth?2:1;
    if(columns==additionalColumns_)return;
    additionalColumns_=columns;
    int weights[2]{};
    for(auto* group:additionalGroups_){
        for(auto* column:additionalColumnLayouts_)column->removeWidget(group);
        const int column=columns==1||weights[0]<=weights[1]?0:1;auto* layout=additionalColumnLayouts_[column];
        layout->insertWidget(layout->count()-1,group);weights[column]+=group->layout()->count();group->show();
    }
    additionalColumnLayouts_[1]->parentWidget()->setVisible(columns==2);
    additionalLayout_->setColumnStretch(0,1);additionalLayout_->setColumnStretch(1,columns==2?1:0);
}
void PerformanceDialog::resizeEvent(QResizeEvent* event){QDialog::resizeEvent(event);arrangeAdditional();}
void PerformanceDialog::refreshParameterLanguage(){
    if(schema_.isEmpty())return;
    const auto values=inputs()["values"].toObject();
    QHash<QString,QLabel*> labels;
    for(auto* label:findChildren<QLabel*>()){
        const auto key=label->property("performance_label").toString();
        if(!key.isEmpty())labels.insert(key,label);
    }
    for(const auto& entry:schema_["fields"].toArray()){
        const auto field=entry.toObject();const auto key=field["key"].toString();
        if(auto* label=labels.value(key))label->setText(notation::html(parameterCaption(method_,field,values)));
        if(auto* combo=qobject_cast<QComboBox*>(fields_.value(key))){
            const QSignalBlocker blocked(combo);const auto choices=field["choices"].toArray();
            for(int i=0;i<combo->count()&&i<choices.size();++i)combo->setItemText(i,uiText(choices.at(i).toString()));
        }
    }
    const int scroll=help_->verticalScrollBar()->value();
    help_->setReport(methodHelp::document(method_,result_));help_->verticalScrollBar()->setValue(scroll);
    refreshInputValidity();arrangeAdditional();
}
void PerformanceDialog::changeEvent(QEvent* event){
    QDialog::changeEvent(event);
    if(event->type()==QEvent::LanguageChange){refreshParameterLanguage();refreshResultLanguage();}
    if(event->type()==QEvent::FontChange||event->type()==QEvent::StyleChange)arrangeAdditional();
}
bool PerformanceDialog::eventFilter(QObject* object,QEvent* event){
    if(additional_&&object==additional_->viewport()&&event->type()==QEvent::Resize)arrangeAdditional();
    return QDialog::eventFilter(object,event);
}
void PerformanceDialog::refreshAccelerationStopControls(){
    if(method_!="acceleration")return;
    auto* task=qobject_cast<QDoubleSpinBox*>(fields_.value("Edit18"));
    auto* stop=qobject_cast<QCheckBox*>(fields_.value("StopAtSpeed"));
    auto* target=fields_.value("TargetSpeed");
    if(!task||!stop||!target)return;
    const bool accelerating=task->value()==1;
    stop->setEnabled(accelerating);stop->setProperty("calculation_declared_enabled",accelerating);
    target->setEnabled(accelerating&&stop->isChecked());
    target->setProperty("calculation_declared_enabled",accelerating&&stop->isChecked());
}
void PerformanceDialog::refreshChoices(){
    if(choicesPending_)return;
    choicesPending_=true;
    run_->setEnabled(false);
    QTimer::singleShot(0,this,[this]{
        choicesPending_=false;
        if(session_.busy()||!session_.performanceDefaults(inputs()))refreshState();
    });
}
QJsonObject PerformanceDialog::inputs()const{
    QJsonObject values;for(auto it=fields_.begin();it!=fields_.end();++it){auto* w=it.value();if(auto* n=qobject_cast<QDoubleSpinBox*>(w))values[it.key()]=n->property("performance_exact_value").toDouble();else if(auto* n=qobject_cast<QCheckBox*>(w))values[it.key()]=n->isChecked();else if(auto* n=qobject_cast<QComboBox*>(w))values[it.key()]=n->currentIndex();}
    QJsonObject request{{"calculator",method_},{"values",values},{"solver_directory",solverDirectory_.trimmed()},{"report_language",uiLanguage()}};
    if(developmentSample_)request["development_sample"]=true;
    return request;
}
void PerformanceDialog::calculate(){
    if(!refreshInputValidity())return;
    if(session_.calculatePerformance(inputs())){setProperty("performance_finished",false);setProperty("performance_error",QString{});status_->setMessage(uiText("正在计算…"),StudioMessageRole::Feedback,true);refreshState();}
}
bool PerformanceDialog::refreshInputValidity(){
    QMap<QString,double> numbers;
    QMap<QString,QString> errors;
    QMap<QString,QString> hints;
    QSet<QString> pending;
    QMap<QString,hollenbachFeedback::EditorRange> editors;
    for(auto it=fields_.cbegin();it!=fields_.cend();++it){
        auto* widget=it.value();
        const bool visible=widget->property("calculation_declared_visible").toBool();
        const bool editable=widget->property("calculation_declared_enabled").toBool();
        if(auto* field=qobject_cast<QDoubleSpinBox*>(widget)){
            bool parsed=false;
            auto value=calculationInput::number(*field,parsed);
            const double exact=field->property("performance_exact_value").toDouble();
            if(value==field->value()&&std::abs(value-exact)<=.5*std::pow(10.,-field->decimals())+1e-10)value=exact;
            numbers[it.key()]=value;
            editors[it.key()]={field->minimum(),field->maximum(),field->decimals(),parsed};
            if(visible&&editable&&!parsed)errors[it.key()]=uiText("请输入范围内的数值。");
        }else if(auto* choice=qobject_cast<QComboBox*>(widget)){
            numbers[it.key()]=choice->currentIndex();
            if(visible&&editable&&choice->currentIndex()<0)errors[it.key()]=uiText("请选择有效选项。");
        }else if(auto* option=qobject_cast<QCheckBox*>(widget))numbers[it.key()]=option->isChecked();
    }
    const auto model=session_.model();
    checkPerformanceInputs(method_,numbers,editors,errors,hints,pending,model&&model->units=="imperial");
    if(method_=="acceleration"&&numbers.value("Edit18")==1&&numbers.value("StopAtSpeed")!=0){
        const double target=numbers.value("TargetSpeed");
        const auto maximum=developmentSample_?std::optional<double>{fs::developmentCalculation::accelerationMaximumKn}:
            session_.accelerationResistanceMaximum();
        if(!maximum){
            pending.insert("TargetSpeed");
            hints["TargetSpeed"]=uiText("待校验：请先生成可用于加速计算的阻力曲线。");
        }else if(target>*maximum+1e-6&&!errors.contains("TargetSpeed"))
            errors["TargetSpeed"]=QString(uiText("目标航速为 %1 kn。当前阻力结果允许大于 0、且不超过 %2 kn。"))
                .arg(target,0,'g',8).arg(*maximum,0,'g',8);
    }
    for(auto it=errors.begin();it!=errors.end();){
        const auto* field=fields_.value(it.key());
        if(!field||!field->property("calculation_declared_visible").toBool())it=errors.erase(it);
        else ++it;
    }
    for(auto it=pending.begin();it!=pending.end();){
        const auto* field=fields_.value(*it);
        if(!field||!field->property("calculation_declared_visible").toBool()){
            hints.remove(*it);
            it=pending.erase(it);
        }else ++it;
    }
    const bool valid=calculationInput::update(fields_,errors,hints,pending);
    return valid&&!choicesPending_;
}
void PerformanceDialog::refreshState(){
    const bool busy=session_.busy()||preparingResult_;parameters_->setEnabled(!busy);additional_->setEnabled(!busy);run_->setEnabled(!busy&&refreshInputValidity());cancel_->setEnabled(busy&&!session_.recordingPerformanceParameters());
    if(databaseButton_)databaseButton_->setEnabled(!busy&&!databaseViewer_);
    if(sampleButton_)sampleButton_->setEnabled(!busy&&!choicesPending_);
    for(auto* b:exports_)b->setEnabled(!busy&&!result_.isEmpty());
    for(auto* b:findChildren<QPushButton*>())if(b->property("performance_action")=="save_parameters"||b->property("performance_action")=="load_parameters"||b->property("performance_action")=="water_temperature"||b->property("performance_action")=="check_tools")b->setEnabled(!busy);
    if(session_.calculationQueued())status_->setMessage(uiText("排队中")+QString(" · %1").arg(session_.calculationQueuePosition()),StudioMessageRole::Feedback);
    else if(busy&&property("calculation_was_queued").toBool())status_->setMessage(uiText("正在计算…"),StudioMessageRole::Feedback,true);
    setProperty("calculation_was_queued",session_.calculationQueued());
    if(closing_&&!busy)QTimer::singleShot(0,this,[this]{QDialog::reject();});
}
void PerformanceDialog::showFailure(const QString& error){status_->setMessage(error+(result_.isEmpty()?QString{}:uiText("\n保留上一次完整结果。")),StudioMessageRole::Warning);setProperty("performance_error",error);setProperty("performance_finished",true);refreshState();}
PerformanceDialog::~PerformanceDialog(){++resultGeneration_;resultPool_.waitForDone();}
void PerformanceDialog::syncPrincipalDimensions(const QJsonObject& principal){
    if(session_.busy()||preparingResult_||developmentSample_||principal==principalSource_)return;
    principalSource_=principal;
    const auto key=fs::principalLengthInput(method_);
    if(!key.isEmpty()&&principal.contains("length_between_perpendiculars_m"))
        if(auto* field=qobject_cast<QDoubleSpinBox*>(fields_.value(key)))field->setValue(principal["length_between_perpendiculars_m"].toDouble());
    refreshState();
}
void PerformanceDialog::receive(const QJsonObject& result,bool defaults){
    const auto generation=++resultGeneration_;sharedCurveAxes_.clear();
    if(defaults){applyResult(result,true,{});return;}
    auto state=std::make_shared<CalculationChartState>();state->data=CalculationChartData::read(method_,result,false);state->visible.fill(true,state->data->curves.size());
    auto data=std::const_pointer_cast<CalculationChartData>(state->data);
    qsizetype count=0;for(const auto& value:result["series"].toArray())count+=value.toObject()["points"].toArray().size();
    if(count<20000){data->decodeNumeric(result);applyResult(result,false,state);return;}
    preparingResult_=true;setProperty("performance_finished",false);refreshState();
    const auto groups=curveTables(result["series"].toArray(),method_);
    resultPool_.start([this,generation,result,state,data,groups]{
        QVector<bool> axes;QString issue;QElapsedTimer timer;timer.start();
        try{data->decodeNumeric(result);for(const auto& group:groups)axes.append(sharedAxis(group));}
        catch(const std::exception& ex){issue=QString::fromUtf8(ex.what());}
        const double elapsed=timer.nsecsElapsed()/1e6;
        QMetaObject::invokeMethod(this,[this,generation,result,state,axes,issue,elapsed]{
            if(generation!=resultGeneration_)return;
            preparingResult_=false;if(!issue.isEmpty()){showFailure(issue);return;}
            sharedCurveAxes_=axes;setProperty("performance_prepare_ms",elapsed);applyResult(result,false,state);
        },Qt::QueuedConnection);
    });
}
namespace {
QString datasetCaption(const QVariant& data,bool table){
    if(table)return uiMetadataText(data.toJsonObject(),"name");
    const auto group=data.toJsonArray();if(group.isEmpty())return {};
    const auto first=group.first().toObject();const auto heading=uiMetadataText(first,"x_title").trimmed();
    return heading.isEmpty()?uiMetadataText(first,"name").trimmed():heading;
}
}
void PerformanceDialog::refreshResultLanguage(){
    if(result_.isEmpty())return;
    const bool tables=result_.contains("tables");
    const QSignalBlocker blocked(series_);
    for(int i=0;i<series_->count();++i)series_->setItemText(i,datasetCaption(series_->itemData(i),tables));
    static_cast<PerformanceTable*>(table_->model())->refreshHeaders(series_->currentData(),tables);
    refreshStudioDataTable(table_);
    static_cast<CalculationPlot*>(plot_)->refreshLanguage(result_["series"].toArray());
    const auto scroll=text_->verticalScrollBar()->value();
    text_->setReport(reportText(true));text_->verticalScrollBar()->setValue(scroll);
    if(!completionMessage_.isEmpty()&&status_->text()==completionMessage_){
        completionMessage_=performanceFeedbackText(result_[tables?"tables":"series"].toArray().size(),method_=="addmass",result_["elapsed_ms"].toDouble());
        status_->setMessage(completionMessage_,StudioMessageRole::Feedback);
    }
}
void PerformanceDialog::applyResult(const QJsonObject& result,bool defaults,std::shared_ptr<CalculationChartState> state){
    QElapsedTimer timing;timing.start();QJsonObject timings;
    if(!defaults){
        auto* plot=static_cast<CalculationPlot*>(plot_);plot->setState(state);
        // A background decoder may finish after a language switch.
        if(state->data->captionLanguage!=uiLanguage())plot->refreshLanguage(result["series"].toArray());
    }
    timings["chart_ms"]=timing.nsecsElapsed()/1e6;
    if(defaults){
        developmentSample_=result["development_sample"].toBool();
        setParameters(result);
        if(developmentSample_)status_->setMessage(uiText("已载入测试参数。"),StudioMessageRole::Feedback);
    }
    else{
        result_=result;setParameters(result["parameters"].toObject());setProperty("performance_finished",true);
        const auto datasets=result[result.contains("tables")?"tables":"series"].toArray();
        {
            const QSignalBlocker blocked(series_);
            series_->clear();
            if(result.contains("tables")){
                for(const auto& dataset:datasets)series_->addItem(uiMetadataText(dataset.toObject(),"name"),dataset.toObject());
            }else{
                for(const auto& group:curveTables(datasets,method_))series_->addItem(datasetCaption(group,false),group);
            }
            series_->parentWidget()->setVisible(series_->count()>1);
        }
        timings["before_table_ms"]=timing.nsecsElapsed()/1e6;renderSeries();timings["after_table_ms"]=timing.nsecsElapsed()/1e6;
        const auto html=reportText(true);text_->setReport(html);
        completionMessage_=performanceFeedbackText(datasets.size(),method_=="addmass",result["elapsed_ms"].toDouble());
        status_->setMessage(completionMessage_,StudioMessageRole::Feedback);
    }
    if(!result["parameter_record_error"].toString().isEmpty())status_->setMessage(result["parameter_record_error"].toString(),StudioMessageRole::Warning);
    refreshState();timings["total_ms"]=timing.nsecsElapsed()/1e6;setProperty("performance_receive_timings",timings);
}
void PerformanceDialog::reject(){if(databaseViewer_){status_->setMessage(uiText("请先关闭外部数据库查看器，再关闭此窗口。"),StudioMessageRole::Warning);return;}if(session_.isCalculationSession()){closing_=false;if(onBackground)onBackground();QDialog::reject();return;}if(session_.busy()){closing_=true;session_.cancelPerformance();status_->setMessage(uiText("取消计算后关闭…"),StudioMessageRole::Feedback,true);return;}QDialog::reject();}
void PerformanceDialog::openDatabaseViewer(){
    if(databaseViewer_||session_.busy())return;
    const auto directory=fs::document::calculationToolDirectory(solverDirectory_);
    const auto exe=QDir(directory).absoluteFilePath("DBFview.EXE");
    if(directory.isEmpty()||!QFileInfo(exe).isFile()){showFailure(uiText("找不到 DBFview.EXE，请在首选项的目录设置中指定外部求解器目录。"));return;}
    auto work=std::make_shared<QTemporaryDir>(QDir::tempPath()+"/fs-dbfview-XXXXXX");
    if(!work->isValid()){showFailure(uiText("无法创建数据库查看器工作目录。"));return;}
    auto* process=new QProcess(this);databaseViewer_=process;process->setProgram(exe);process->setWorkingDirectory(work->path());
#ifdef Q_OS_WIN
    process->setCreateProcessArgumentsModifier([](QProcess::CreateProcessArguments *a){a->flags|=CREATE_NO_WINDOW;});
#endif
    // This is an explicitly requested interactive viewer, not a background solver.
    process->setStandardOutputFile(QProcess::nullDevice());process->setStandardErrorFile(QProcess::nullDevice());
    connect(process,&QProcess::errorOccurred,this,[this,process,work](QProcess::ProcessError error){if(error!=QProcess::FailedToStart)return;databaseViewer_=nullptr;process->deleteLater();showFailure(uiText("数据库查看器无法启动：")+process->errorString());});
    connect(process,qOverload<int,QProcess::ExitStatus>(&QProcess::finished),this,[this,process,work](int code,QProcess::ExitStatus state){databaseViewer_=nullptr;process->deleteLater();setProperty("database_viewer_exit",code);refreshState();if(state!=QProcess::NormalExit||code!=0)showFailure(uiText("数据库查看器异常退出，原计算结果已保留。"));else{status_->setMessage(uiText("数据库查看器已关闭。"),StudioMessageRole::Feedback);calculate();}});
    try{fs::platform::prepareLegacySolver(*process,uiText(fs::platform::legacySolverWineMissing));}catch(const std::exception& e){databaseViewer_=nullptr;process->deleteLater();showFailure(QString::fromUtf8(e.what()));return;}
    setProperty("database_viewer_exit",QVariant{});process->start();refreshState();
}
void PerformanceDialog::renderSeries(){
    data_motion::beginTableUpdate(table_);
    auto* model=static_cast<PerformanceTable*>(table_->model());
    if(result_.contains("tables"))model->setTable(series_->currentData().toJsonObject());
    else model->setCurves(series_->currentData().toJsonArray(),sharedCurveAxes_.value(series_->currentIndex()));
    refreshStudioDataTable(table_);
    data_motion::finishTableUpdate(table_,"performance:"+method_+":"+QString::number(series_->currentIndex()));
}
QByteArray PerformanceDialog::csv()const{
    if(result_.contains("tables")){QStringList lines{uiText("数据组,行,列,值")};for(auto value:result_["tables"].toArray()){auto t=value.toObject();auto rows=t["rows"].toArray(),cols=t["columns"].toArray(),values=t["values"].toArray();for(qsizetype i=0;i<values.size();++i){auto row=values[i].toArray();for(qsizetype j=0;j<row.size();++j)lines<<QStringList{quote(uiMetadataText(t,"name")),quote(rows[i].toVariant().toString()),quote(cols[j].isString()?uiText(cols[j].toString()):cols[j].toVariant().toString()),row[j].isNull()?QString{}:full(row[j].toDouble())}.join(',');}}return QByteArray("\xEF\xBB\xBF")+(lines.join("\r\n")+"\r\n").toUtf8();}
    QStringList lines;lines<<uiText("曲线,X 量与单位,Y 量与单位,X,Y");
    for(auto v:result_["series"].toArray()){auto s=v.toObject();for(auto p:s["points"].toArray()){auto xy=p.toArray();lines<<QStringList{quote(uiMetadataText(s,"name")),quote(uiMetadataText(s,"x_title")),quote(uiMetadataText(s,"y_title")),full(xy[0].toDouble()),full(xy[1].toDouble())}.join(',');}}
    return QByteArray("\xEF\xBB\xBF")+(lines.join("\r\n")+"\r\n").toUtf8();
}
QString PerformanceDialog::reportText(bool html)const{
    QString title="FreeShip 26 — "+windowTitle();
    if(result_["development_sample"]==true)title+=" · "+QString(uiText("测试计算"));
    QString text=report::heading(title,html,true);
    text+=report::metadata(QString(uiText("模型版本："))+QString::number(result_["revision"].toDouble(),'f',0),html);
    report::Table parameters{uiText("本次计算参数"),{uiText("参数"),uiText("数值"),uiText("单位")},{},{0,2},{55,28,17},780};
    QJsonObject parameterValues;for(const auto& item:result_["parameters"].toObject()["fields"].toArray()){const auto f=item.toObject();parameterValues[f["key"].toString()]=f["value"];}
    for(auto v:result_["parameters"].toObject()["fields"].toArray()){
        const auto f=v.toObject();QString value;
        if(f.contains("used_in_calculation")&&!f["used_in_calculation"].toBool())continue;
        if(f["kind"]=="boolean")value=f["value"].toBool()?uiText("是"):uiText("否");
        else if(f["kind"]=="choice"){const auto index=f["value"].toInt();const auto choices=f["choices"].toArray();value=index>=0&&index<choices.size()?uiText(choices[index].toString()):uiText("未选择");}
        else value=reportValue(f["value"]);
        const auto name=notation::splitLabelUnit(methodHelp::parameterLabel(method_,f,parameterValues));
        parameters.rows.append({name.first,value,notation::unit(f["unit"].toString().isEmpty()?name.second:f["unit"].toString())});
    }
    text+=report::render(parameters,html);
    text+=report::heading(uiText("结果"),html);
    const auto datasets=result_["tables"].toArray();
    if(method_=="wind"){
        report::Table results{{},{uiText("计算结果"),uiText("数值"),uiText("单位")},{},{0,2},{55,28,17},780};
        const auto scalars=result_["scalars"].toObject();
        for(const auto& [key,label,unit]:{
                std::tuple{"relative_wind_m_s","相对风速","m/s"},
                {"relative_wind_angle_deg","相对风角","°"},
                {"longitudinal_force_kN","纵向力","kN"},
                {"lateral_force_kN","横向力","kN"},
                {"yaw_moment_kNm","艏摇力矩","kN·m"}}){
            const auto value=scalars.value(key);
            if(value.isDouble())results.rows.append({uiText(label),reportValue(value),notation::unit(unit)});
        }
        text+=report::render(results,html);
    }
    else if(method_=="acceleration"){
        report::Table results{uiText("加速与制动结果"),{uiText("计算结果"),uiText("数值"),uiText("单位")},{},{0,2},{55,28,17},780};
        const auto scalars=result_["scalars"].toObject();
        for(const auto& [key,label,unit]:{
                std::tuple{"time_s","时间","s"},
                {"distance_m","距离","m"},
                {"speed_m_s","最终航速","m/s"},
                {"shaft_rpm","最终桨轴转速","r/min"},
                {"engine_inertia_kg_m2","主机等效惯性矩","kg·m²"}}){
            const auto value=scalars.value(key);
            if(value.isDouble())results.rows.append({uiText(label),reportValue(value),notation::unit(unit)});
        }
        text+=report::render(results,html);
        if(scalars["target_reached"].toBool()&&scalars["target_speed_kn"].isDouble())
            text+=report::metadata(QString(uiText("已达到目标航速：%1 kn")).arg(scalars["target_speed_kn"].toDouble(),0,'f',3),html);
    }
    else if(method_=="maneuver")text+=report::maneuverResult(result_["text"].toString(),html);
    else if(datasets.isEmpty())text+=legacyResult(result_["text"].toString(),html,result_["warnings"].toArray());
    else{
        for(const auto& entry:datasets){
            const auto dataset=entry.toObject();report::Table table{uiMetadataText(dataset,"name"),{uiText("分量")},{},{0}};
            for(const auto& col:dataset["columns"].toArray())table.headers.append(col.isString()?uiText(col.toString()):reportValue(col));
            const auto values=dataset["values"].toArray(),labels=dataset["rows"].toArray();
            bool hasUnits=false;
            for(int i=0;i<values.size();++i){
                const auto [label,unit]=notation::splitLabelUnit(labels.at(i).isString()?uiText(labels.at(i).toString()):reportValue(labels.at(i)));
                QStringList row{label};
                for(const auto& cell:values[i].toArray())row.append(reportValue(cell));
                if(!unit.isEmpty()){
                    while(row.size()<table.headers.size())row.append(QString());
                    row.append(unit);
                    hasUnits=true;
                }
                table.rows.append(row);
            }
            if(hasUnits){
                table.leftColumns.append(int(table.headers.size()));
                table.headers.append(uiText("单位"));
            }
            text+=report::render(table,html);
        }
        if(method_=="addmass"){
            report::Table details{{},{uiText("结果量"),uiText("数值")},
                {{uiText("实际面元数量"),reportValue(result_["panel_count"])},{uiText("缩放除数"),reportValue(result_["scale_divisor"])}},{0}};
            text+=report::render(details,html);
        }
    }
    const auto warnings=result_["warnings"].toArray();
    if(!warnings.isEmpty()){
        text+=report::heading(uiText("提示","warnings"),html);
        QStringList entries;for(const auto& warning:warnings)entries.append(uiText(warning.toString()));
        text+=report::message(entries.join("\n"),html,StudioMessageRole::Warning);
    }
    text+=methodHelp::limits(method_,result_,html);
    return text;
}
QString PerformanceDialog::exportReport(const QString& path,const QString& format)const{
    if(result_.isEmpty())return uiText("尚无完整计算结果。");
    QSaveFile file(path);file.setDirectWriteFallback(false);if(!file.open(QIODevice::WriteOnly))return uiText("无法写入报告：")+file.errorString();
    if(format=="pdf"){QPdfWriter writer(&file);writer.setPageSize(QPageSize(QPageSize::A4));writer.setPageOrientation(QPageLayout::Landscape);writer.setResolution(120);QTextDocument doc;QFont font("Microsoft YaHei UI",9);doc.setDefaultFont(font);report::setHtml(&doc,reportText(true));doc.print(&writer);}
    else{QByteArray bytes;if(format=="csv")bytes=csv();else if(format=="json")bytes=QJsonDocument(result_).toJson();else if(format=="txt")bytes=reportText(false).toUtf8();else return uiText("未知报告格式。");if(file.write(bytes)!=bytes.size())return uiText("报告写入未完成。");}
    return file.commit()?QString{}:uiText("无法完成报告保存：")+file.errorString();
}
QString PerformanceDialog::saveParameters(const QString& path)const{
    if(session_.busy())return uiText("请等待当前操作结束。");
    auto data=inputs();data.remove("solver_directory");data.remove("report_language");data.remove("development_sample");data["format"]="FreeShipPerformanceParameters";data["version"]=1;data["units"]=session_.model()->units;
    if(method_=="acceleration")data["engine_inertia_convention"]=QJsonObject{{"unit","kg*m^2"},{"basis","per_propeller_shaft"}};
    QSaveFile f(path);const auto bytes=QJsonDocument(data).toJson();if(!f.open(QIODevice::WriteOnly)||f.write(bytes)!=bytes.size()||!f.commit())return uiText("无法保存参数文件。");return {};
}
QString PerformanceDialog::loadParameters(const QString& path){
    if(session_.busy())return uiText("请等待当前操作结束。");
    QFile f(path);if(!f.open(QIODevice::ReadOnly)||f.size()>1024*1024)return uiText("无法读取参数文件。");
    QJsonParseError parse;auto data=QJsonDocument::fromJson(f.readAll(),&parse).object();
    if(parse.error!=QJsonParseError::NoError||data["format"]!="FreeShipPerformanceParameters"||data["version"]!=1||data["calculator"]!=method_||data["units"]!=session_.model()->units||!data["values"].isObject())return uiText("参数文件的方法、版本或单位与当前计算不一致。");
    if(method_=="acceleration"&&data.contains("engine_inertia_convention")){
        const auto convention=data["engine_inertia_convention"].toObject();
        if(convention["unit"]!="kg*m^2"||convention["basis"]!="per_propeller_shaft")return uiText("惯性矩单位或轴系基准不一致，请先核对设备资料并换算。");
    }
    data.remove("development_sample");data["solver_directory"]=solverDirectory_.trimmed();return session_.performanceDefaults(data)?QString{}:uiText("无法载入计算参数。");
}

// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once
#include <QFileDevice>
#include <QList>
#include <atomic>
#include <stdexcept>
#include <utility>

namespace fs::frames {
// Qt rounds page sizes to whole points. Set an exact A4 box for butt joints,
// or attach a UserUnit for oversized plots, using an incremental PDF update.
// Read only our own Qt output, via its classic xref. Leave fonts and streams intact.
inline void finishStationPdfPages(QFileDevice& file,int unit,bool edgeToEdge,qsizetype pages,const std::atomic_bool& cancel) {
    auto require=[](bool valid) {
        if(!valid) throw std::runtime_error("无法写入 PDF 的实际纸张尺寸。");
    };
    const auto end=file.size();
    require(file.seek(std::max<qint64>(0,end-128)));
    const auto tail=file.readAll();
    const auto marker=tail.lastIndexOf("startxref\n");
    require(marker>=0);
    bool ok=false;
    auto previous=tail.sliced(marker+10).split('\n').first().trimmed().toLongLong(&ok);
    require(ok && previous>0 && previous<end && file.seek(previous));
    require(file.readLine().trimmed()=="xref");
    const auto range=file.readLine().simplified().split(' ');
    require(range.size()==2 && range[0]=="0");
    const auto count=range[1].toInt(&ok);
    require(ok && count>pages && count<(end-previous)/16);
    QList<qint64> offsets;
    for(int i=0;i<count;++i) {
        auto row=file.readLine(32).simplified().split(' ');
        require(row.size()==3);
        auto offset=row[0].toLongLong(&ok);
        require(ok && offset>=0 && offset<previous);
        offsets.append(row[2]=="n"?offset:0);
    }
    auto trailer=file.readAll();
    require(trailer.startsWith("trailer\n<<") && trailer.contains("/Root "));
    const auto close=trailer.lastIndexOf(">>");
    require(close>0);
    trailer.truncate(close);
    QList<std::pair<int,qint64>> updated;
    auto put=[&](const QByteArray& bytes){require(file.write(bytes)==bytes.size());};
    qint64 append=end;
    for(int id=1;id<count;++id) {
        if(cancel.load()) throw std::runtime_error("已取消导出。");
        if(!offsets[id]) continue;
        require(file.seek(offsets[id]));
        const auto prefix=QByteArray::number(id)+" 0 obj\n<<\n/Type /Page\n";
        if(!file.peek(prefix.size()).startsWith(prefix)) continue;
        const auto dict=file.read(4096);
        const auto stop=dict.indexOf(">>\nendobj");
        require(stop>=prefix.size() && file.seek(append));
        updated.append({id,append});
        auto page=dict.first(stop);
        if(edgeToEdge){
            const auto box=page.indexOf("/MediaBox [");
            const auto last=page.indexOf(']',box);
            require(box>=0 && last>box);
            const auto values=page.sliced(box+11,last-box-11).simplified().split(' ');
            require(values.size()==4);
            const double top=values[3].toDouble(&ok);
            require(ok && top>594 && top<596);
            // Qt's content transform uses its original top-left origin.
            const auto precise=QByteArray("/MediaBox [0 ")+QByteArray::number(top-210.*72/25.4,'f',8)
                +" "+QByteArray::number(297.*72/25.4,'f',8)+" "+values[3]+"]";
            page.replace(box,last-box+1,precise);
        }else{
            page+="/UserUnit "+QByteArray::number(unit)+"\n";
        }
        put(page+">>\nendobj\n");
        append=file.pos();
    }
    require(updated.size()==pages && file.seek(append));
    put("xref\n");
    for(auto [id,offset]:updated) {
        require(offset<10000000000LL);
        put(QByteArray::number(id)+" 1\n"+QByteArray::number(offset).rightJustified(10,'0')+" 00000 n \n");
    }
    put(trailer+"/Prev "+QByteArray::number(previous)+"\n>>\nstartxref\n"+QByteArray::number(append)+"\n%%EOF\n");
}
}

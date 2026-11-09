#include "model_checks.h"
#include <QHash>
#include <QRegularExpression>
#include <QSet>
#include <cmath>
#include <stdexcept>
namespace fs {
QVector<ModelIssue> inspectModel(const ModelSnapshot& m){
    QVector<ModelIssue> result;QHash<QString,QVector<std::pair<EntityId,int>>> incident;QSet<EntityId> owned;
    QHash<EntityId,Vec3> points;for(const auto& p:m.points)points[p.id]=p.position;
    auto key=[](EntityId a,EntityId b){return QString::number(std::min(a,b))+":"+QString::number(std::max(a,b));};
    QVector<EntityId> leaks,unused,nonmanifold,winding,degenerate,broken;
    for(const auto& f:m.faces){Vec3 n{};
        for(int i=0;i<f.points.size();++i){auto a=f.points[i],b=f.points[(i+1)%f.points.size()];owned.insert(a);incident[key(a,b)].append({f.id,a<b?1:-1});
            const auto p=points.value(a),q=points.value(b);n.x+=(p.y-q.y)*(p.z+q.z);n.y+=(p.z-q.z)*(p.x+q.x);n.z+=(p.x-q.x)*(p.y+q.y);}
        if(n.x==0&&n.y==0&&n.z==0)degenerate.append(f.id);
    }
    QHash<EntityId,int> pending;QHash<EntityId,QVector<EntityId>> followers;QSet<EntityId> invalid;
    for(const auto& p:m.points){if(p.leak)leaks.append(p.id);if(!owned.contains(p.id))unused.append(p.id);
        if((p.lineA==0)!=(p.lineB==0)||(p.lineA&&p.lineA==p.lineB))invalid.insert(p.id);
        QSet<EntityId> refs;for(auto id:{p.anchor,p.lineA,p.lineB})if(id)refs.insert(id);
        pending[p.id]=refs.size();
        for(auto id:refs){if(!points.contains(id))invalid.insert(p.id);else followers[id].append(p.id);}
    }
    // Peel acyclic dependencies in linear time. The remaining points contain a
    // cycle, or depend on one (or on a missing reference); none can update safely.
    QVector<EntityId> queue;for(auto i=pending.cbegin();i!=pending.cend();++i)if(!i.value())queue.append(i.key());
    for(int i=0;i<queue.size();++i)for(auto id:followers.value(queue[i]))if(--pending[id]==0)queue.append(id);
    for(const auto& p:m.points)if(pending[p.id]>0||invalid.contains(p.id))broken.append(p.id);
    for(const auto& e:m.edges){const auto faces=incident.value(key(e.a,e.b));if(faces.size()>2)nonmanifold.append(e.id);
        else if(faces.size()==2&&faces[0].second==faces[1].second)winding.append(e.id);}
    auto add=[&](QString code,QString kind,QString text,QVector<EntityId> ids){if(!ids.isEmpty())result.append({code,kind,text.arg(ids.size()),std::move(ids)});};
    add("leaks","point","%1 个可能的漏点（开口船体也可能存在）",leaks);
    add("unused","point","%1 个不属于任何面的控制点",unused);
    add("nonmanifold","edge","%1 条边连接了超过两个面",nonmanifold);
    add("winding","edge","%1 条共享边两侧的面朝向不一致",winding);
    add("degenerate","face","%1 个控制面的有向面积为零",degenerate);
    add("constraint","point","%1 个点有无效约束或依赖循环",broken);
    return result;
}
QJsonArray parseMarkerText(const QByteArray& bytes,const QString& units){
    Q_UNUSED(units);
    if(bytes.size()>8*1024*1024||bytes.contains('\0'))throw std::runtime_error("标记文件过大或包含无效字符。");
    auto text=QString::fromUtf8(bytes);if(text.startsWith(QChar(0xfeff)))text.remove(0,1);
    const auto lines=text.split('\n');QJsonArray paths,path;int total=0;bool ended=false;
    auto finish=[&]{if(!path.isEmpty()){if(path.size()<2)throw std::runtime_error("每条标记至少需要两个点。");paths.append(path);path={};}};
    for(int i=0;i<lines.size();++i){const auto line=lines[i].trimmed();if(ended){if(!line.isEmpty())throw std::runtime_error("EOF 后还有标记内容。");continue;}
        if(line.isEmpty()){finish();continue;}if(line.compare("EOF",Qt::CaseInsensitive)==0){finish();ended=true;continue;}
        const auto parts=line.split(QRegularExpression("\\s+"),Qt::SkipEmptyParts);QJsonArray point;bool valid=parts.size()==3;
        for(const auto& token:parts){bool ok=false;const double value=token.toDouble(&ok);valid=valid&&ok&&std::isfinite(value)&&std::abs(value)<=1e9;point.append(value);}
        if(!valid){if(i==0)continue;throw std::runtime_error(QString("标记文件第 %1 行需要三个有效坐标。").arg(i+1).toStdString());}
        if(++total>100000)throw std::runtime_error("标记文件的点数超过限制。");path.append(point);
    }
    finish();if(paths.isEmpty()||paths.size()>4096)throw std::runtime_error("文件中没有有效标记，或标记数量超过限制。");return paths;
}
}

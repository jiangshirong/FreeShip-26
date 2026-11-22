// SPDX-License-Identifier: GPL-2.0-or-later
#include "plate_assembly.h"
#include "plate_depth_buffer.h"
#include "core/developed_plates.h"
#include "ui_language.h"
#include "studio_ui.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QElapsedTimer>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPushButton>
#include <QQuaternion>
#include <QSlider>
#include <QSignalBlocker>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <optional>
#include <tuple>

using EdgeRef = std::pair<int,int>; // plate and local edge
QVector<PlateSeam> collectPlateSeams(const fs::DevelopedPlates& data) {
    std::map<qint64,QVector<EdgeRef>> byIdentity;
    for(int plate=0;plate<data.plates.size();++plate) {
        const auto& edges=data.plates[plate].edges;
        for(int i=0;i<edges.size();++i)
            if(edges[i].meshId)byIdentity[edges[i].meshId].append({plate,i});
    }
    std::map<std::pair<int,int>,QVector<std::pair<int,int>>> pairs;
    for(const auto& entry:byIdentity) {
        const auto& refs=entry.second;
        if(refs.size()==2 && refs[0].first!=refs[1].first)
            pairs[{refs[0].first,refs[1].first}].append({refs[0].second,refs[1].second});
    }
    QVector<PlateSeam> seams;
    for(const auto& [panels,segments]:pairs) {
        const auto& a=data.plates[panels.first];
        const auto& b=data.plates[panels.second];
        std::map<int,QVector<int>> neighbors;
        for(int i=0;i<segments.size();++i) {
            const auto& edge=a.edges[segments[i].first];
            neighbors[edge.a].append(i);neighbors[edge.b].append(i);
        }
        QVector<bool> visited(segments.size(),false);
        auto walk=[&](int first,int vertex) {
            PlateSeam seam{panels.first,panels.second,{}, {}, {}};
            int i=first;
            const int start=vertex;
            while(!visited[i]) {
                visited[i]=true;
                const auto [ea,eb]=segments[i];
                const auto& x=a.edges[ea];const auto& y=b.edges[eb];
                seam.segments.append(segments[i]);
                seam.length[0]+=QLineF(a.points[x.a],a.points[x.b]).length();
                seam.length[1]+=QLineF(b.points[y.a],b.points[y.b]).length();
                vertex=vertex==x.a?x.b:x.a;
                const auto& next=neighbors.at(vertex);
                if(next.size()!=2)break;
                i=next[0]==i?next[1]:next[0];
            }
            if(vertex!=start){
                // Both copies keep the endpoint order of the very same mesh edge.
                auto endpoints=[&](int leftVertex,std::pair<int,int> edges){
                    const auto& left=a.edges[edges.first];
                    const auto& right=b.edges[edges.second];
                    return std::make_pair(leftVertex,leftVertex==left.a?right.a:right.b);
                };
                seam.ends={endpoints(start,seam.segments.front()),endpoints(vertex,seam.segments.back())};
                const auto dx=a.original[vertex].x()-a.original[start].x();
                seam.longitudinal=std::abs(dx)>1e-5;
                if(dx<0)std::swap(seam.ends[0],seam.ends[1]);
            }
            seams.append(std::move(seam));
        };
        for(const auto& [vertex,incident]:neighbors)if(incident.size()!=2)
            for(int i:incident)if(!visited[i])walk(i,vertex);
        for(int i=0;i<segments.size();++i)if(!visited[i])walk(i,a.edges[segments[i].first].a);
    }
    return seams;
}

QString PlateSeam::endLabel(int end)const{
    if(!longitudinal)return end==0?QStringLiteral("A"):QStringLiteral("B");
    return end==0?uiText("艉端"):uiText("艏端");
}

QString plateSeamMeasurement(const fs::DevelopedPlates& data,const PlateSeam& s) {
    const double unit=data.units?304.8:1000.;
    return QString(uiText("%1：%2 mm　%3：%4 mm　长度差 %5 mm"))
        .arg(data.plates[s.left].name,QString::number(s.length[0]*unit,'f',2),
             data.plates[s.right].name,QString::number(s.length[1]*unit,'f',2),
             QString::number(std::abs(s.length[0]-s.length[1])*unit,'f',2));
}
namespace {
struct Pose {
    QVector<QVector3D> flat, hull;
    QVector3D to;
    explicit Pose(const fs::DevelopedPlate& plate) {
        QVector3D from;
        hull=plate.original;
        for(int i=0;i<hull.size();++i) {
            if(plate.sideSuffix==" (P)")hull[i].setY(-hull[i].y());
            const auto p=plate.points[i];
            flat.append(QVector3D(p.x(),p.y(),0));
            from+=flat.back();to+=hull[i];
        }
        from/=flat.size();to/=flat.size();
        // Lay each blank in its assembly orientation before it enters the scene.
        int far=0,wide=0;
        for(int i=1;i<flat.size();++i)
            if((flat[i]-from).lengthSquared()>(flat[far]-from).lengthSquared())far=i;
        const auto axis=(flat[far]-from).normalized();
        float width=-1;
        for(int i=0;i<flat.size();++i) {
            const float area=QVector3D::crossProduct(axis,flat[i]-from).lengthSquared();
            if(area>width){width=area;wide=i;}
        }
        auto frame=[&](const QVector<QVector3D>& points,QVector3D center) {
            const auto x=(points[far]-center).normalized();
            const auto z=QVector3D::crossProduct(x,points[wide]-center).normalized();
            if(z.isNull())return QQuaternion();
            return QQuaternion::fromAxes(x,QVector3D::crossProduct(z,x),z);
        };
        const auto alignment=(frame(hull,to)*frame(flat,from).conjugated()).normalized();
        for(auto& p:flat)p=to+alignment.rotatedVector(p-from);
    }
};

class AssemblyCanvas final:public QWidget {
public:
    const fs::DevelopedPlates& data;
    const QVector<PlateSeam> seams;
    std::function<void(int,bool)> picked;
    int selected=-1;
    bool whole=true;
    float progress=0;
    AssemblyCanvas(const fs::DevelopedPlates& model,const QVector<PlateSeam>& pairs,QWidget* parent)
        :QWidget(parent),data(model),seams(pairs) {
        setMinimumSize(440,260);
        setMouseTracking(true);
        setObjectName("PlateAssemblyCanvas");
        setToolTip(uiText("单击接缝，拖动旋转，滚轮缩放"));
        for(const auto& p:data.plates){
            poses.emplaceBack(p);
            QVector<int> indices;
            for(int i=0;i<p.edges.size();++i)if(p.edges[i].boundary)indices.append(i);
            boundaries.append(indices);
        }
        QVector3D low=poses.front().hull.front(),high=low;
        for(const auto& pose:poses)for(const auto& v:pose.hull)
            for(int axis=0;axis<3;++axis){low[axis]=std::min(low[axis],v[axis]);high[axis]=std::max(high[axis],v[axis]);}
        const auto center=(low+high)*.5f;
        ends[0]=ends[1]=center;
        for(int side=0;side<2;++side){
            QVector3D sum;int n=0;const float x=side?high.x():low.x();
            for(const auto& pose:poses)for(auto v:pose.hull)if(std::abs(v.x()-x)<=1e-5f*(high.x()-low.x())){sum+=v;++n;}
            if(n)ends[side]=sum/n;
        }
        for(auto& pose:poses) {
            QVector3D outward(0,pose.to.y()-center.y(),pose.to.z()-center.z());
            if(outward.lengthSquared()<1e-10f)outward={0,0,1};
            const float clearance=.65f*std::max({high.y()-low.y(),high.z()-low.z(),.06f*(high.x()-low.x())});
            const auto offset=outward.normalized()*clearance;
            for(auto& p:pose.flat) {
                p+=offset;
                for(int axis=0;axis<3;++axis){boundsLow[axis]=std::min(boundsLow[axis],p[axis]);boundsHigh[axis]=std::max(boundsHigh[axis],p[axis]);}
            }
        }
        for(int axis=0;axis<3;++axis){boundsLow[axis]=std::min(boundsLow[axis],low[axis]);boundsHigh[axis]=std::max(boundsHigh[axis],high[axis]);}
        // Grow each connected part from its lowest panel. Prefer lower neighbors
        // so that hull sides are in place before the deck on a typical kayak.
        QVector<QVector<int>> neighbors(poses.size());
        for(const auto& seam:seams){neighbors[seam.left].append(seam.right);neighbors[seam.right].append(seam.left);}
        using Priority=std::pair<float,int>;
        std::set<Priority> remaining,frontier;
        for(int i=0;i<poses.size();++i)remaining.emplace(poses[i].to.z(),i);
        ranks.resize(poses.size());
        while(!remaining.empty()) {
            const auto next=frontier.empty()?*remaining.begin():*frontier.begin();
            frontier.erase(next);remaining.erase(next);
            ranks[next.second]=order.size();order.append(next.second);
            for(int n:neighbors[next.second]){
                Priority candidate{poses[n].to.z(),n};
                if(remaining.count(candidate))frontier.insert(candidate);
            }
        }
        for(int i=0;i<seams.size();++i)for(auto [a,b]:seams[i].segments) {
            seamAt[{seams[i].left,a}]=i;seamAt[{seams[i].right,b}]=i;
        }
    }
    int count()const{return whole||selected<0?int(order.size()):2;}
    int stepCount()const{return std::max(1,count()-1);}
    int activeRank()const{return std::min(count()-1,int(progress*stepCount())+(count()>1));}
    int panelAt(int rank)const{return whole||selected<0?order[rank]:(rank==0?seams[selected].left:seams[selected].right);}
    int displayedRank()const{return whole&&progress==0?0:activeRank();}
    QString currentPanel()const{return data.plates[panelAt(displayedRank())].name;}
    QVector<QPolygonF> screenGeometry(){
        project();
        QVector<QPolygonF> result(poses.size());
        for(int p=0;p<poses.size();++p)if(shown(p))
            for(const auto& v:vertices[p])result[p].append(v.screen);
        return result;
    }
private:
    QVector<Pose> poses;
    QVector<QVector<int>> boundaries;
    PlateEdgeHit hovered;
    std::optional<std::tuple<QSize,QRgb,float,float,double,float,int,bool>> frame;
    void setHover(PlateEdgeHit hit){
        if(!hovered.sameStroke(hit)){hovered=hit;update();}
    }
    QVector<int> order,ranks;
    QVector3D boundsLow{1e30f,1e30f,1e30f},boundsHigh{-1e30f,-1e30f,-1e30f};
    QVector<QVector<ProjectedVertex>> vertices;
    PlateDepthBuffer buffer;
    QVector3D ends[2];
    QPointF endScreen[2];
    QQuaternion fittedCamera;
    QRectF fittedExtent;
    std::map<EdgeRef,int> seamAt;
    QPoint press,last;
    bool dragging=false;
    float yaw=0,pitch=0;
    double zoom=1;
    bool shown(int p)const {
        if(whole||selected<0)return ranks[p]<=activeRank();
        return seams[selected].left==p||seams[selected].right==p;
    }
    QPointF screen(int p,int v)const{return vertices[p][v].screen;}
    float stepPhase()const{return progress*stepCount()-std::max(0,activeRank()-1);}
    void project() {
        const int active=panelAt(activeRank());
        // A brief settled interval makes each joint legible before the next arrival.
        const float t=std::clamp(stepPhase()/.86f,0.f,1.f);
        const float blend=t*t*(3-2*t);
        const auto camera=QQuaternion::fromAxisAndAngle(0,0,1,-19)
                        *QQuaternion::fromEulerAngles(-58+pitch,0,0)
                        *QQuaternion::fromEulerAngles(0,0,yaw-127);
        const auto pivot=(boundsHigh+boundsLow)*.5f;
        const float distance=std::max(1e-6f,(boundsHigh-boundsLow).length()*1.35f);
        auto projectPoint=[&](QVector3D world){
            const auto v=camera.rotatedVector(world-pivot);
            const float w=distance/(distance-v.z());
            return ProjectedVertex{QPointF(v.x()*w,-v.y()*w),w,v};
        };
        vertices.resize(poses.size());
        if(camera!=fittedCamera||fittedExtent.isNull()){
            double xmin=1e100,xmax=-1e100,ymin=1e100,ymax=-1e100;
            for(const auto& pose:poses)for(const auto* positions:{&pose.flat,&pose.hull})for(auto v:*positions){
                const auto pt=projectPoint(v).screen;
                xmin=std::min(xmin,pt.x());xmax=std::max(xmax,pt.x());
                ymin=std::min(ymin,pt.y());ymax=std::max(ymax,pt.y());
            }
            fittedCamera=camera;fittedExtent=QRectF(QPointF(xmin,ymin),QPointF(xmax,ymax));
        }
        for(int p=0;p<poses.size();++p)if(shown(p)) {
            const auto& pose=poses[p];
            auto& out=vertices[p];out.resize(pose.flat.size());
            for(int i=0;i<out.size();++i) {
                const auto world=p==active && blend<1 ? (1-blend)*pose.flat[i]+blend*pose.hull[i] : pose.hull[i];
                out[i]=projectPoint(world);
            }
        }
        const double scale=zoom*std::min((width()-94)/std::max(1e-8,fittedExtent.width()),(height()-72)/std::max(1e-8,fittedExtent.height()));
        const QPointF center=fittedExtent.center();
        for(int i=0;i<2;++i)endScreen[i]=(projectPoint(ends[i]).screen-center)*scale+QPointF(width()/2.,height()/2.);
        for(int p=0;p<poses.size();++p)if(shown(p))
            for(auto& v:vertices[p])v.screen=(v.screen-center)*scale+QPointF(width()/2.,height()/2.);
    }
    void renderSurface() {
        const auto key=std::make_tuple(size(),palette().base().color().rgba(),yaw,pitch,zoom,progress,selected,whole);
        if(frame&&*frame==key)return;
        frame=key;hovered={};
        project();
        buffer.clear(size(),palette().base().color());
        const int active=panelAt(activeRank());
        const int coverage=whole&&count()>1?qRound(16*std::clamp(stepPhase()/.12f,0.f,1.f)):16;
        const QVector3D light=QVector3D(-.38f,.77f,.51f).normalized();
        for(int p=0;p<poses.size();++p)if(shown(p)) {
            QColor base=p==active&&stepPhase()<.86f?QColor(83,171,157):QColor(172,195,205);
            if(selected>=0&&!whole)base=p==seams[selected].left?QColor(119,184,166):QColor(124,166,207);
            const auto& projected=vertices[p];
            for(const auto& face:data.plates[p].faces) {
                const auto& a=projected[face.front()];
                for(int j=2;j<face.size();++j){
                    const auto& b=projected[face[j-1]];const auto& c=projected[face[j]];
                    auto n=QVector3D::normal(b.view-a.view,c.view-a.view);
                    // Reflection reverses winding on some plates. Orient the lit side
                    // toward the camera consistently on both halves of an open shell.
                    const double winding=(b.screen.x()-a.screen.x())*(c.screen.y()-a.screen.y())-
                                         (b.screen.y()-a.screen.y())*(c.screen.x()-a.screen.x());
                    if(winding>0)n=-n;
                    const float shade=.54f+.55f*std::max(0.f,QVector3D::dotProduct(n,light));
                    auto channel=[&](int component){return std::min(255,qRound(component*shade+10));};
                    buffer.triangle(a,b,c,qRgb(channel(base.red()),channel(base.green()),channel(base.blue())),
                                    p==active?coverage:16);
                }
            }
        }
    }
    void stroke(QPainter& painter,int plate,int ia,int ib)const {
        const auto& a=vertices[plate][ia];const auto& b=vertices[plate][ib];
        const QPointF d=b.screen-a.screen;
        const int steps=std::max(1,int(std::ceil(std::max(std::abs(d.x()),std::abs(d.y())))));
        int first=-1;
        for(int i=0;i<=steps;++i){
            const float t=float(i)/steps;
            const bool visible=buffer.visible(a.screen+d*t,a.depth+(b.depth-a.depth)*t);
            if(visible&&first<0)first=i;
            if(first>=0&&(!visible||i==steps)){
                painter.drawLine(a.screen+d*(double(first)/steps),a.screen+d*(double(visible?i:i-1)/steps));
                first=-1;
            }
        }
    }
    void drawOrientation(QPainter& painter)const {
        const auto axis=QLineF(endScreen[0],endScreen[1]);
        if(axis.length()<1)return;
        const auto direction=(axis.p2()-axis.p1())/axis.length();
        for(int i=0;i<2;++i){
            const QString text=i?uiText("艏端"):uiText("艉端");
            const QSizeF extent=painter.fontMetrics().size(Qt::TextSingleLine,text)+QSize(12,6);
            const auto anchor=endScreen[i];const auto outward=(i?1:-1)*direction;
            auto center=anchor+outward*(16+extent.width()*.5);
            center.setX(std::clamp(center.x(),extent.width()/2+3,width()-extent.width()/2-3));
            center.setY(std::clamp(center.y(),extent.height()/2+3,height()-extent.height()/2-3));
            const QRectF box(center-QPointF(extent.width()/2,extent.height()/2),extent);
            painter.setPen(QPen(QColor("#677c85"),1));painter.drawLine(anchor,center);
            painter.setBrush(palette().base());painter.drawRoundedRect(box,4,4);
            painter.setPen(palette().text().color());painter.drawText(box,Qt::AlignCenter,text);
        }
    }
    void paintEvent(QPaintEvent*)override {
        renderSurface();
        QPainter painter(this);painter.drawImage(0,0,buffer.image);
        painter.setRenderHint(QPainter::Antialiasing);
        const int active=panelAt(activeRank());
        const auto arrival=whole&&count()>1?std::clamp(stepPhase()/.12f,0.f,1.f):1.f;
        for(int p=0;p<poses.size();++p)if(shown(p)){
            painter.setPen(QPen(QColor(49,68,82,p==active?qRound(165*arrival):165),1));
            for(const auto& loop:data.plates[p].loops)for(int j=0;j<loop.size();++j)
                stroke(painter,p,loop[j],loop[(j+1)%loop.size()]);
        }
        if(whole)drawOrientation(painter);
        if(hovered.valid()&&(selected<0||hovered.seam!=selected))drawHover(painter);
        if(selected<0)return;
        const auto& seam=seams[selected];
        for(int side=0;side<2;++side) {
            int p=side?seam.right:seam.left;
            if(!shown(p))continue;
            painter.setOpacity(p==active?arrival:1.f);
            painter.setPen(QPen(plateSeamColor,hovered.plate==p&&hovered.seam==selected?4:3,Qt::SolidLine,Qt::RoundCap));
            for(auto [a,b]:seam.segments) {
                const auto& e=data.plates[p].edges[side?b:a];
                stroke(painter,p,e.a,e.b);
            }
            if(whole||(side&&progress>.97))continue;
            for(int end=0;end<seam.ends.size();++end) {
                const auto [left,right]=seam.ends[end];
                const int v=side?right:left;
                const auto pt=screen(p,v);
                if(!buffer.visible(pt,vertices[p][v].depth))continue;
                painter.drawText(pt+QPointF(3,side?16:-5),seam.endLabel(end));
            }
        }
    }
    void drawHover(QPainter& painter)const {
        painter.setPen(QPen(plateHoverColor,2.5,Qt::SolidLine,Qt::RoundCap));
        const int p=hovered.plate;
        auto draw=[&](int i){const auto& edge=data.plates[p].edges[i];stroke(painter,p,edge.a,edge.b);};
        if(hovered.seam<0)draw(hovered.edge);
        else {
            const auto& seam=seams[hovered.seam];
            for(auto [a,b]:seam.segments)draw(p==seam.left?a:b);
        }
    }
    void leaveEvent(QEvent* e)override{setHover({});QWidget::leaveEvent(e);}
    void mousePressEvent(QMouseEvent* e)override {press=last=e->pos();dragging=false;}
    void mouseMoveEvent(QMouseEvent* e)override {
        if(!e->buttons()){setHover(edgeAt(e->position()));return;}
        if(!dragging && (e->pos()-press).manhattanLength()<4)return;
        dragging=true;
        const auto delta=e->pos()-last;last=e->pos();
        yaw+=delta.x()*.45f;pitch+=delta.y()*.45f;update();
    }
    void mouseReleaseEvent(QMouseEvent* e)override {
        if(dragging||e->button()!=Qt::LeftButton)return;
        const auto hit=edgeAt(e->position());
        if(hit.valid()&&picked)picked(hit.seam,true);
    }
    PlateEdgeHit edgeAt(QPointF pos) {
        renderSurface();
        double nearest=36;float bestDepth=-1e30;PlateEdgeHit hit;
        for(int p=0;p<poses.size();++p)if(shown(p)&&!(whole&&p==panelAt(activeRank())&&stepPhase()<.015f)) {
            const auto& edges=data.plates[p].edges;
            for(int i:boundaries[p]) {
                const auto& edge=edges[i];const auto a=screen(p,edge.a),v=screen(p,edge.b)-a;
                const double f=std::clamp(QPointF::dotProduct(pos-a,v)/std::max(1e-12,QPointF::dotProduct(v,v)),0.,1.);
                const auto delta=pos-(a+f*v);
                const double distance=QPointF::dotProduct(delta,delta);
                const float depth=vertices[p][edge.a].depth*(1-f)+vertices[p][edge.b].depth*f;
                if(!buffer.visible(a+f*v,depth))continue;
                if(distance<nearest-1e-6||(std::abs(distance-nearest)<1e-6 && depth>bestDepth)) {
                    nearest=distance;bestDepth=depth;const auto it=seamAt.find({p,i});hit={p,i,it==seamAt.end()?-1:it->second};
                }
            }
        }
        return hit;
    }
    void wheelEvent(QWheelEvent* e)override {
        zoom=std::clamp(zoom*std::pow(1.0015,e->angleDelta().y()),.3,6.);update();e->accept();
    }
};
}

QVector<QPolygonF> plateAssemblyScreenGeometry(QWidget* widget) {
    if(auto* canvas=dynamic_cast<AssemblyCanvas*>(widget))return canvas->screenGeometry();
    return {};
}

int showPlateAssembly(const fs::DevelopedPlates& source,const QVector<PlateSeam>& seams,int initial,QWidget* parent) {
    // A calculation completion can replace the parent dialog's snapshot during exec().
    const fs::DevelopedPlates snapshot=source;
    QDialog dialog(parent);dialog.setWindowTitle(uiText("拼合预览"));dialog.resize(980,590);
    dialog.setAttribute(Qt::WA_DontShowOnScreen,parent->testAttribute(Qt::WA_DontShowOnScreen));
    setStudioWindowIcon(&dialog,"Face");
    auto* layout=new QVBoxLayout(&dialog);
    auto* canvas=new AssemblyCanvas(snapshot,seams,&dialog);
    auto* row=new QHBoxLayout;layout->addLayout(row);
    auto* choices=new QComboBox;choices->setObjectName("PlateAssemblySeams");row->addWidget(choices,1);
    choices->addItem(uiText("选择接缝"));
    for(const auto& s:canvas->seams)
        choices->addItem(snapshot.plates[s.left].name+" ↔ "+snapshot.plates[s.right].name);
    auto* all=new QCheckBox(uiText("整船"));all->setChecked(true);row->addWidget(all);
    layout->addWidget(canvas,1);
    auto* info=new QLabel(uiText("点击板片边缘查看配对接缝"));info->setObjectName("PlateAssemblyMeasurement");layout->addWidget(info);
    auto* controls=new QHBoxLayout;layout->addLayout(controls);
    auto* play=new QPushButton(uiText("播放"));controls->addWidget(play);
    auto* slider=new QSlider(Qt::Horizontal);slider->setRange(0,1000);controls->addWidget(slider,1);
    auto* step=new QLabel;row->addWidget(step);
    auto* timer=new QTimer(&dialog);timer->setInterval(16);
    QElapsedTimer elapsed;int start=0;
    auto pause=[&]{timer->stop();play->setText(uiText("播放"));};
    auto refreshStep=[&]{
        step->setText(QString("%1 / %2 · %3").arg(canvas->displayedRank()+1).arg(canvas->count()).arg(canvas->currentPanel()));
        canvas->update();
    };
    auto configureSteps=[&]{
        const auto progress=canvas->progress;
        QSignalBlocker block(slider);
        slider->setMaximum(std::max(1000,canvas->stepCount()*100));
        slider->setValue(qRound(progress*slider->maximum()));refreshStep();
    };
    QObject::connect(slider,&QSlider::valueChanged,&dialog,[&](int n){canvas->progress=float(n)/slider->maximum();refreshStep();});
    QObject::connect(slider,&QSlider::sliderPressed,&dialog,pause);
    QObject::connect(timer,&QTimer::timeout,&dialog,[&]{
        const qint64 duration=1500LL*canvas->stepCount();
        slider->setValue(std::min<qint64>(slider->maximum(),start+elapsed.elapsed()*slider->maximum()/duration));
        if(slider->value()==slider->maximum())pause();
    });
    QObject::connect(play,&QPushButton::clicked,&dialog,[&]{
        if(timer->isActive()){pause();return;}
        if(slider->value()==slider->maximum())slider->setValue(0);
        start=slider->value();elapsed.start();play->setText(uiText("暂停"));timer->start();
    });
    QObject::connect(all,&QCheckBox::toggled,&dialog,[&](bool checked){pause();canvas->whole=checked;configureSteps();});
    auto select=[&](int index,bool clicked){
        pause();
        canvas->selected=index;
        all->setChecked(index<0);
        all->setEnabled(index>=0);
        if(index>=0)slider->setValue(0);
        configureSteps();
        if(index>=0) {
            info->setText(plateSeamMeasurement(snapshot,canvas->seams[index]));
        }else info->setText(clicked?uiText("这条边没有唯一的配对板片"):uiText("点击板片边缘查看配对接缝"));
        canvas->update();
    };
    QObject::connect(choices,&QComboBox::currentIndexChanged,&dialog,[&](int i){select(i-1,false);});
    canvas->picked=[&](int i,bool clicked){
        const QSignalBlocker block(choices);choices->setCurrentIndex(i+1);select(i,clicked);
    };
    all->setEnabled(false);
    configureSteps();
    choices->setCurrentIndex(initial+1);
    runStudioDialog(dialog);
    return canvas->selected;
}

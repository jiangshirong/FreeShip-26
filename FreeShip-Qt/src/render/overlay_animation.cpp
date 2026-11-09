#include "overlay_animation.h"
#include <QSet>
#include <QRectF>
#include <algorithm>
#include <cmath>
#include <limits>
#include <queue>
namespace fs::presentation {
namespace {double length(QPointF p){return std::hypot(p.x(),p.y());}double cross(QPointF a,QPointF b){return a.x()*b.y()-a.y()*b.x();}double unit(double p){return std::clamp(p,0.,1.);}}
void RevealGraph::prepare(const QVector<Stroke>& lines,QPointF center){
    bool same=center_==center&&source_.size()==lines.size();
    for(int i=0;same&&i<lines.size();++i)same=source_[i].line==lines[i].line&&source_[i].color==lines[i].color&&source_[i].width==lines[i].width;
    if(same)return;
    ++builds_;source_=lines;center_=center;nodes_.clear();edges_.clear();maximum_=0;
    QVector<QVector<double>> cuts(lines.size());
    for(int i=0;i<lines.size();++i){auto n=std::max(1,int(std::ceil(lines[i].line.length()/32)));for(int j=0;j<=n;++j)cuts[i].append(double(j)/n);}
    for(int i=0;i<lines.size();++i)for(int j=i+1;j<lines.size();++j){
        const auto& a=lines[i].line;const auto& b=lines[j].line;
        if(!QRectF(a.p1(),a.p2()).normalized().adjusted(-.01,-.01,.01,.01).intersects(QRectF(b.p1(),b.p2()).normalized().adjusted(-.01,-.01,.01,.01)))continue;
        auto d=a.p2()-a.p1(),e=b.p2()-b.p1(),q=b.p1()-a.p1();double det=cross(d,e);if(std::abs(det)<1e-8)continue;
        auto t=cross(q,e)/det,u=cross(q,d)/det;if(t>=0&&t<=1&&u>=0&&u<=1){cuts[i].append(t);cuts[j].append(u);}
    }
    QHash<QPair<int,int>,QVector<int>> cells;
    auto vertex=[&](QPointF p){
        int x=int(std::floor(p.x()*2)),y=int(std::floor(p.y()*2));
        for(int dx=-1;dx<=1;++dx)for(int dy=-1;dy<=1;++dy)for(auto id:cells.value({x+dx,y+dy}))if(length(nodes_[id].point-p)<.5)return id;
        int id=int(nodes_.size());nodes_.append({p,std::numeric_limits<double>::infinity()});cells[{x,y}].append(id);return id;
    };
    for(int i=0;i<lines.size();++i){auto& c=cuts[i];std::sort(c.begin(),c.end());for(int j=1;j<c.size();++j)if(c[j]-c[j-1]>=1e-7){
        int a=vertex(lines[i].line.pointAt(c[j-1])),b=vertex(lines[i].line.pointAt(c[j]));if(a!=b)edges_.append({a,b,lines[i],length(nodes_[a].point-nodes_[b].point)});
    }}
    if(nodes_.isEmpty())return;
    QVector<QVector<QPair<int,double>>> adjacent(nodes_.size());
    for(auto e:edges_){adjacent[e.a].append({e.b,e.length});adjacent[e.b].append({e.a,e.length});}
    int root=0;for(int i=1;i<nodes_.size();++i)if(length(nodes_[i].point-center)<length(nodes_[root].point-center))root=i;
    using Visit=std::pair<double,int>;std::priority_queue<Visit,std::vector<Visit>,std::greater<Visit>> queue;
    QVector<bool> visited(nodes_.size(),false);nodes_[root].distance=0;queue.push({0,root});int count=0;
    while(count<nodes_.size()){
        if(queue.empty()){
            double gap=std::numeric_limits<double>::infinity();int next=-1,from=-1;
            for(int i=0;i<nodes_.size();++i)if(!visited[i])for(int j=0;j<nodes_.size();++j)if(visited[j]){
                auto d=length(nodes_[i].point-nodes_[j].point);if(d<gap){gap=d;next=i;from=j;}
            }
            if(next<0)break;
            nodes_[next].distance=nodes_[from].distance+gap;queue.push({nodes_[next].distance,next});
        }
        auto [distance,id]=queue.top();queue.pop();if(visited[id])continue;visited[id]=true;++count;maximum_=std::max(maximum_,distance);
        for(auto [other,span]:adjacent[id])if(!visited[other]&&distance+span<nodes_[other].distance){nodes_[other].distance=distance+span;queue.push({distance+span,other});}
    }
    for(auto& e:edges_){
        if(nodes_[e.a].distance>nodes_[e.b].distance)std::swap(e.a,e.b);
        maximum_=std::max(maximum_,nodes_[e.a].distance+e.length);
    }
}
QVector<Stroke> RevealGraph::visible(double fraction)const{
    if(fraction>=1)return source_;
    QVector<Stroke> result;for(const auto& e:edges_){double t=unit((fraction*maximum_-nodes_[e.a].distance)/std::max(.001,e.length));if(t<=0)continue;
        result.append({{nodes_[e.a].point,nodes_[e.a].point+t*(nodes_[e.b].point-nodes_[e.a].point)},e.stroke.color,e.stroke.width});}
    return result;
}
void Entrance::restart(){phase_=1;start_=-1;paused_=-1;frame_=overlayStart_=lastPulse_=0;pulses_.clear();}
void Entrance::finish(){phase_=0;paused_=-1;pulses_.clear();}
void Entrance::pause(qint64 now){if(paused_<0)paused_=now;}
void Entrance::resume(qint64 now){if(paused_<0)return;auto delta=now-paused_;if(start_>=0)start_+=delta;paused_=-1;}
void Entrance::beginFrame(qint64 now,double width){if(start_<0)start_=now;frame_=std::max<qint64>(0,(paused_>=0?paused_:now)-start_);width_=std::max(1.,width);}
double Entrance::previewFraction()const{return phase_==1?unit(frame_/550.):1;}
double Entrance::lineFraction()const{return phase_==1?0:phase_==2?unit((frame_-overlayStart_)/1300.):1;}
bool Entrance::pointVisible(double x)const{return phase_==0||(phase_==2&&x>=width_*(1-unit((frame_-overlayStart_)/1000.)));}
QColor Entrance::pointColor(quint64 id,QColor base){
    if(phase_!=2)return base;
    if(!pulses_.contains(id)){pulses_.insert(id,frame_);lastPulse_=std::max(lastPulse_,frame_+650);}
    double p=unit((frame_-pulses_[id]-100)/550.),a=1-p*p*(3-2*p);QColor highlight(255,155,20);
    return QColor(qRound(base.red()*(1-a)+highlight.red()*a),qRound(base.green()*(1-a)+highlight.green()*a),qRound(base.blue()*(1-a)+highlight.blue()*a));
}
double Entrance::textOpacity(bool numeric)const{if(phase_==0)return 1;if(phase_==1)return 0;double p=unit((frame_-overlayStart_-1000-(numeric?140:0))/260.);return 1-(1-p)*(1-p);}
double Entrance::textFraction(bool numeric)const{if(numeric||phase_==0)return 1;if(phase_==1)return 0;double p=unit((frame_-overlayStart_-1000)/260.);return 1-std::pow(1-p,3);}
void Entrance::endFrame(){if(phase_==1&&frame_>=550){phase_=2;overlayStart_=frame_;}else if(phase_==2&&frame_>=overlayStart_+1420&&frame_>=lastPulse_)finish();}
}

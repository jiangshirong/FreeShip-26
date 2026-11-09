#include "label_layout.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <QRegularExpression>
namespace fs::presentation {
namespace {double distance(QPointF p){return std::hypot(p.x(),p.y());}}
void LabelLayout::clear(){history_.clear();claims_.clear();last_=-1;}
void LabelLayout::translate(QPointF delta){for(auto& e:history_){e.position+=delta;e.rect.translate(delta);}}
void LabelLayout::begin(QRectF bounds,qint64 time,bool animate){
    if(history_.size()>20000)clear();
    bounds_=bounds;dt_=last_<0?.016:std::clamp((time-last_)/1000.,.001,.05);last_=time;++frame_;
    animate_=animate;moving_=false;occupied_.clear();lines_.clear();lineCells_.clear();lineSeen_.clear();query_=0;claims_.clear();namedValues_.clear();
    for(auto it=history_.begin();it!=history_.end();)if(it->seen+120<frame_)it=history_.erase(it);else ++it;
}
void LabelLayout::reserve(QRectF rect){occupied_.append(rect);}
void LabelLayout::segment(QLineF line){
    // Broad-phase only: every candidate still uses the exact segment/rectangle
    // test below. Long offscreen coordinates never become bucket coordinates.
    const auto box=QRectF(line.p1(),line.p2()).normalized().adjusted(-3,-3,3,3).intersected(bounds_.adjusted(-3,-3,3,3));
    if(box.isEmpty())return;
    const int id=int(lines_.size());lines_.append(line);lineSeen_.append(0);
    for(int x=int(std::floor(box.left()/64));x<=int(std::floor(box.right()/64));++x)
        for(int y=int(std::floor(box.top()/64));y<=int(std::floor(box.bottom()/64));++y)lineCells_[{x,y}].append(id);
}
bool LabelLayout::claim(QPointF anchor,const QString& value){auto key=QString::number(qRound(anchor.x()))+':'+QString::number(qRound(anchor.y()))+':'+value;if(claims_.contains(key))return false;claims_[key]=true;return true;}
bool LabelLayout::segmentHits(QRectF rect,QLineF line){
    double lo=0,hi=1;auto clip=[&](double start,double delta,double low,double high){
        if(std::abs(delta)<1e-6)return start>=low&&start<=high;
        auto a=(low-start)/delta,b=(high-start)/delta;lo=std::max(lo,std::min(a,b));hi=std::min(hi,std::max(a,b));return lo<=hi;
    };return clip(line.x1(),line.dx(),rect.left(),rect.right())&&clip(line.y1(),line.dy(),rect.top(),rect.bottom());
}
int LabelLayout::segmentCrossings(QRectF box){
    int hits=0;
    if(++query_==0){lineSeen_.fill(0);query_=1;}
    for(int x=int(std::floor(box.left()/64));x<=int(std::floor(box.right()/64));++x)
        for(int y=int(std::floor(box.top()/64));y<=int(std::floor(box.bottom()/64));++y){
            auto found=lineCells_.constFind({x,y});if(found==lineCells_.cend())continue;
            for(auto id:*found)if(lineSeen_[id]!=query_){lineSeen_[id]=query_;if(segmentHits(box,lines_[id]))++hits;}
        }
    return hits;
}
LabelPlacement LabelLayout::place(const QString& key,const QString& text,QPointF anchor,QPointF preferred,QSizeF size,LabelKind kind,int outward){
    if(size.isEmpty())return {};
    const bool named=kind==LabelKind::Named;
    if(named){
        static const QRegularExpression suffix("([+-]?[0-9]+(?:[.,][0-9]+)?)\\s*$");const auto match=suffix.match(text);
        if(match.hasMatch())namedValues_.append({match.captured(1),anchor,size});
    }else if(kind==LabelKind::VerticalTick||kind==LabelKind::HorizontalTick){
        for(const auto& reference:namedValues_)if(reference.value==text.trimmed()&&std::abs(reference.anchor.x()-anchor.x())<=std::max(reference.size.width(),size.width())+24&&std::abs(reference.anchor.y()-anchor.y())<=std::max(reference.size.height(),size.height())+12)return {};
    }
    auto old=history_.constFind(key);bool hasOld=old!=history_.cend()&&old->text==text&&old->size==size&&old->kind==kind&&old->outward==outward;Entry entry=hasOld?old.value():Entry{};
    auto available=[&](QRectF full){
        QRectF rect=full.intersected(bounds_);
        auto avoid=[&](QRectF obstacle){
            obstacle.adjust(-2,-2,2,2);if(!rect.intersects(obstacle))return;
            if(!named){rect={};return;}
            double left=std::max(0.,obstacle.left()-rect.left()),right=std::max(0.,rect.right()-obstacle.right());
            if(left>=right)rect.setRight(std::max(rect.left(),obstacle.left()));else rect.setLeft(std::min(rect.right(),obstacle.right()));
        };
        for(auto r:occupied_)avoid(r);
        for(auto it=history_.cbegin();it!=history_.cend();++it)if(it.key()!=key&&it->seen+1==frame_&&(!named||it->kind==LabelKind::Named))avoid(it->rect);
        return rect;
    };
    auto edge=[&](QRectF rect){
        double radius=std::max(24.,size.height()*2),strength=std::max(8.,size.height()*.75);
        auto pull=[&](double d){return strength*std::pow(std::clamp(1-d/radius,0.,1.6),2);};
        return QPointF(rect.left()-bounds_.left()<bounds_.right()-rect.right()?-pull(rect.left()-bounds_.left()):pull(bounds_.right()-rect.right()),
                       rect.top()-bounds_.top()<bounds_.bottom()-rect.bottom()?-pull(rect.top()-bounds_.top()):pull(bounds_.bottom()-rect.bottom()));
    };
    QPointF best,offset=preferred-anchor;double bestScore=std::numeric_limits<double>::infinity();
    auto evaluate=[&](QPointF candidate,bool prior=false){
        QRectF full(anchor+candidate,size);
        if(kind==LabelKind::Point&&bounds_.contains(anchor))full.translate(edge(full));
        auto rect=available(full);if(rect.isEmpty())return;
        if(prior&&rect.size()==size&&entry.preferred==offset){bestScore=-1;best=candidate;return;}
        const int hits=named?0:segmentCrossings(rect.adjusted(-3,-3,3,3));
        double score=hits*100000.+distance(candidate-offset)+(hasOld?.5*distance(candidate-entry.target):0)+(size.width()-rect.width())*.01;
        if(kind==LabelKind::Point&&bounds_.contains(anchor))score+=1e6*((size.width()-rect.width())+(size.height()-rect.height()));
        if(score<bestScore){bestScore=score;best=candidate;}
    };
    if(hasOld)evaluate(entry.target,true);
    if(bestScore>0)evaluate(offset);
    if(!std::isfinite(bestScore)&&!bounds_.contains(anchor)){if(hasOld){best=entry.target;bestScore=0;}else return {};}
    constexpr int radii[]{0,6,12,18,24,36,48,64,80,96,128,160};
    if(bestScore>0){
        for(auto r:radii){
            if(kind==LabelKind::Point)for(auto p:{QPointF(6+r,-size.height()-6-r),QPointF(6+r,6+r),QPointF(-size.width()-6-r,-size.height()-6-r),QPointF(-size.width()-6-r,6+r),QPointF(-size.width()/2,-size.height()-8-r),QPointF(-size.width()/2,8+r)})evaluate(p);
            if((kind==LabelKind::VerticalTick||kind==LabelKind::HorizontalTick)&&r>24)continue;
            for(auto delta:{QPointF(0,0),QPointF(r,0),QPointF(-r,0),QPointF(0,r),QPointF(0,-r),QPointF(r,r),QPointF(-r,r),QPointF(r,-r)}){
                if(kind==LabelKind::VerticalTick&&(delta.x()!=0||delta.y()*outward<0))continue;
                if(kind==LabelKind::HorizontalTick&&(delta.y()!=0||delta.x()*outward<0))continue;
                evaluate(offset+delta);
            }
        }
    }
    if(!std::isfinite(bestScore)&&kind==LabelKind::Point)for(int r=192;r<=std::max(bounds_.width(),bounds_.height())+std::max(size.width(),size.height())&&!std::isfinite(bestScore);r+=32)
        for(int d=-r;d<=r;d+=32)for(auto delta:{QPointF(d,-r),QPointF(d,r),QPointF(-r,d),QPointF(r,d)})evaluate(offset+delta);
    // Keep a named label's simulation alive while temporarily covered. It can
    // recover at its former anchor when the obstacle leaves the frame.
    if(!std::isfinite(bestScore)&&named){best=hasOld?entry.target:offset;bestScore=0;}
    if(!std::isfinite(bestScore))return {};
    QPointF current=best;
    if(hasOld&&animate_){
        auto decay=std::exp(-12*dt_);auto delta=entry.offset-best;auto temp=(entry.velocity+12*delta)*dt_;
        current=best+(delta+temp)*decay;entry.velocity=(entry.velocity-12*temp)*decay;
        auto movement=current-entry.offset;auto maximum=std::min(8.,180*dt_);if(distance(movement)>maximum)current=entry.offset+movement*(maximum/distance(movement));
        moving_|=distance(current-best)>.25||distance(entry.velocity)>.5;
    }
    auto shift=edge(QRectF(anchor+current,size));
    // Boundary forces belong to the viewport, not the orientation of a grid line.
    if(hasOld&&animate_){shift=entry.edge+(shift-entry.edge)*(1-std::exp(-dt_/.08));moving_|=distance(shift-entry.edge)>.25;}
    QPointF position=anchor+current+shift;auto clip=available(QRectF(position,size));
    // A label pulled completely through the viewport edge is still a live
    // simulation entry. Do not snap it back to the unshifted candidate: short
    // numeric labels cross top/bottom edges sooner than wide labels cross sides.
    if(clip.isEmpty()&&!named&&bounds_.intersects(QRectF(position,size))){
        current=best;position=anchor+best+shift;
        clip=available(QRectF(position,size));entry.velocity={};
    }
    if(!clip.isEmpty())reserve(clip);entry.offset=current;entry.target=best;entry.edge=shift;entry.position=position;entry.rect=clip;entry.text=text;entry.size=size;entry.kind=kind;entry.outward=outward;entry.preferred=offset;entry.seen=frame_;history_[key]=entry;
    return {position,clip,!clip.isEmpty()};
}
}

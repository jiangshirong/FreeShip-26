// Interactive transforms share the existing, atomic point-move submission path.
bool HullView::showsTransformHandles() const {
    const auto &s=*state_;
    return s.snapshot && s.display && (s.controlNetVisible || s.inspection!=State::Inspection::Off) &&
        !s.transformPoints().isEmpty() &&
        !s.flowPlacement && s.backgroundMode==BackgroundMode::None;
}
bool HullView::canTransform() const {
    return showsTransformHandles()&&state_->editingEnabled&&state_->pendingPointPositions.isEmpty();
}

bool HullView::beginTransform(int axis,QPointF origin,bool handle) {
    if(!canTransform())return false;
    cancelGesture();auto &s=*state_;
    auto ids=s.transformPoints().values();std::sort(ids.begin(),ids.end());
    for(auto id:ids){
        auto i=s.display->pointIndices.constFind(id);
        if(i==s.display->pointIndices.cend() || s.display->points[*i].locked || !s.display->points[*i].batchMovable){
            if(onEditRejected)onEditRejected(uiText("此移动只适用于可自由移动的控制点。"));return false;
        }
    }
    s.pivot={};for(auto id:ids){const auto p=s.display->points[s.display->pointIndices[id]].position;
        s.pivot.x+=p.x/ids.size();s.pivot.y+=p.y/ids.size();s.pivot.z+=p.z/ids.size();}
    s.precise=true;s.axis=axis;s.fromHandle=handle;s.press=s.last=origin;s.radius=std::max(s.display->mesh.extent*.15,1e-6);
    s.gesture=State::Gesture::Move;s.dragBodySign=s.splitBody()&&s.pivot.x<=fs::presentation::splitLocation(*s.snapshot)?-1:1;
    finishPresentation();rebuildInfluence();setCursor(Qt::CrossCursor);update();return true;
}

void HullView::rebuildInfluence(){
    auto &s=*state_;s.weights.clear();s.dragStarts.clear();
    auto ids=s.transformPoints().values();std::sort(ids.begin(),ids.end());
    for(auto id:ids){s.weights[id]=1;s.dragStarts.append({id,s.display->points[s.display->pointIndices[id]].position});}
    if(!s.soft)return;
    // Graph distance on visible control edges, not straight-line distance through the hull.
    using Id=fs::EntityId;using Entry=std::pair<double,Id>;
    struct Link{double length;Id other;bool crease;};
    QHash<Id,QVector<Link>> links;QSet<Id> chines;
    for(const auto &e:s.display->edges){auto d=subtract(s.display->points[e.ia].position,s.display->points[e.ib].position);
        double length=std::hypot(d.x,d.y,d.z);links[e.a].append({length,e.b,e.crease});links[e.b].append({length,e.a,e.crease});
        if(e.crease){chines.insert(e.a);chines.insert(e.b);}}
    std::priority_queue<Entry,std::vector<Entry>,std::greater<Entry>> queue;QHash<Id,double> distance;
    for(auto id:ids){distance[id]=0;queue.emplace(0,id);}
    while(!queue.empty()){auto [d,id]=queue.top();queue.pop();if(d!=distance[id])continue;
        for(const auto &[length,other,crease]:std::as_const(links[id])){double next=d+length;
            if(next>=s.radius || next>=distance.value(other,std::numeric_limits<double>::infinity()))continue;
            const auto &p=s.display->points[s.display->pointIndices[other]];
            if(p.locked||!p.batchMovable||p.vertexType==3||(chines.contains(other)&&!crease)||std::abs(p.position.y)<1e-8)continue;
            distance[other]=next;queue.emplace(next,other);
        }
    }
    auto affected=distance.keys();std::sort(affected.begin(),affected.end());
    for(auto id:affected)if(!s.transformPoints().contains(id)){double t=1-distance[id]/s.radius;
        s.weights[id]=t*t*(3-2*t);s.dragStarts.append({id,s.display->points[s.display->pointIndices[id]].position});}
}

namespace {
// Unsuffixed metric input is in mm, unsuffixed imperial input is in feet.
bool transformDistance(QString text,bool imperial,double &amount){
    double metres=imperial?.3048:.001;
    for(const auto &[suffix,factor]:std::initializer_list<std::pair<QString,double>>{{"mm",.001},{"cm",.01},{"ft",.3048},{"in",.0254},{"m",1.}}){
        if(text.endsWith(suffix)){text.chop(suffix.size());metres=factor;break;}}
    bool ok=false;amount=text.toDouble(&ok)*metres/(imperial?.3048:1.);
    return ok&&std::isfinite(amount);
}
}
void HullView::previewTransform(QPointF cursor){
    auto &s=*state_;s.last=cursor;const auto total=cursor-s.press;
    s.dragDelta={};auto &v=s.dragDelta;const auto pivot=s.controlPosition(s.pivot);
    const double pixelScale=s.scale*s.projectPoint(pivot).reciprocalW;
    if(s.axis){
        double amount=0;
        if(!s.digits.isEmpty()){if(!transformDistance(s.digits,s.snapshot->units=="imperial",amount))amount=0;}
        else {auto dir=s.axisVector(s.axis,s.pivot);const auto n=QPointF::dotProduct(dir,dir);
            if(n>pixelScale*pixelScale*.0001){amount=QPointF::dotProduct(total,dir)/n;
                // Undo perspective foreshortening at the target depth.
                if(s.projection==HullProjection::Perspective){fs::Vec3 r,u,depth;s.basis(r,u,depth);
                    const double dz=s.axis==1?depth.x:s.axis==2?depth.y:depth.z;
                    const double denom=1+amount*dz*s.projectPoint(pivot).reciprocalW/(s.cameraExtent*s.cameraDistanceFactor());
                    amount=denom>.001?amount/denom:0;}}}
        if(s.axis==1)v.x=amount;else if(s.axis==2)v.y=amount;else v.z=amount;
    }
    s.moved=!samePosition(v,{});if(onTransformPreview)onTransformPreview(transformMoves());
    if(onPointMovePreviewChanged)onPointMovePreviewChanged();
    update();
}
QVector<fs::PointMove> HullView::transformMoves()const{
    QVector<fs::PointMove> out;if(!state_->moved)return out;out.reserve(state_->dragStarts.size());
    for(const auto &p:state_->dragStarts){auto next=state_->movedPosition(p.position,p.id);if(!samePosition(next,p.position))out.append({p.id,next});}return out;
}
void HullView::finishTransform(){
    if(!state_->digits.isEmpty()){double amount;if(!transformDistance(state_->digits,state_->snapshot->units=="imperial",amount))return;}
    auto moves=transformMoves();
    // A confirmed preview stays on screen while the normal revision is rebuilt.
    if(!moves.isEmpty()&&onPointsMoved){state_->precise=false;cancelGesture(true);onPointsMoved(std::move(moves));}
    else cancelGesture();
}
void HullView::drawTransform(QPainter &p)const{
    auto &s=*state_;if(!s.precise && (!showsTransformHandles() ||
        (s.gesture!=State::Gesture::None&&s.gesture!=State::Gesture::Move)))return;
    fs::Vec3 base;if(s.precise)base=s.movedPosition(s.pivot);else if(!s.transformCenter(base))return;
    p.save();p.setRenderHint(QPainter::Antialiasing);
    for(const auto &pt:s.dragStarts)if(!s.transformPoints().contains(pt.id)){
        auto at=project(s.controlPosition(s.movedPosition(pt.position,pt.id)));p.setBrush(Qt::NoBrush);p.setPen(QPen(QColor(23,141,149),1.5));p.drawEllipse(at,4.,4.);}
    const QColor colors[]={QColor("#ac3244"),QColor("#16734b"),QColor("#285fa9")};
    const QColor highlights[]={QColor("#ed384e"),QColor("#12a75d"),QColor("#297df5")};
    const int active=s.axis?s.axis:s.hoverAxis;
    p.save();auto font=p.font();font.setBold(true);font.setPixelSize(12);p.setFont(font);
    auto axisPaint=[&](int a){State::Handle h;if(!s.axisHandle(a,base,h))return;
        const bool hot=a==active;const QColor ink=hot?highlights[a-1]:colors[a-1];
        // A light keyline keeps the control separate from station lines on either side.
        const double shaft=hot?5.:3.,halfHead=hot?7.:5.;
        p.setPen(QPen(Qt::white,shaft+2.8,Qt::SolidLine,Qt::RoundCap));p.drawLine(h.start,h.neck);
        p.setPen(QPen(ink,shaft,Qt::SolidLine,Qt::RoundCap));p.drawLine(h.start,h.neck);
        p.setBrush(ink);p.setPen(QPen(Qt::white,1.4,Qt::SolidLine,Qt::RoundCap,Qt::RoundJoin));
        p.drawPolygon(QPolygonF{h.tip,h.neck+h.side*halfHead,h.neck-h.side*halfHead});
        const double r=hot?10.:8.5;QRectF badge(h.label-QPointF(r,r),QSizeF(2*r,2*r));
        p.setBrush(hot?Qt::white:ink);p.setPen(QPen(hot?ink:QColor(Qt::white),hot?2.5:1.4));p.drawEllipse(badge);
        p.setPen(hot?colors[a-1]:QColor(Qt::white));p.drawText(badge,Qt::AlignCenter,QString(QChar('X'+a-1)));};
    for(int a=1;a<=3;++a)if(a!=active)axisPaint(a);
    if(active)axisPaint(active);p.restore();
    if(!s.precise){p.restore();return;}
    const bool imperial=s.snapshot->units=="imperial";const QString unit=imperial?"ft":"mm";
    const double component=s.axis==1?s.dragDelta.x:s.axis==2?s.dragDelta.y:s.dragDelta.z;
    QString hint=s.axis?QString("%1: %2 %3").arg(QChar('X'+s.axis-1)).arg(s.digits.isEmpty()?QString::number(component*(imperial?1:1000),'g',6):s.digits,(!s.digits.isEmpty()&&s.digits.back().isLetter())?QString{}:unit):QString(uiText("移动：选择 X / Y / Z 轴"));
    hint+="   ·   "+QString(uiText("O 邻点随动"));if(s.soft)hint+=QString("  %1 %2  ↕").arg(s.radius*(imperial?1:1000),0,'g',4).arg(unit);
    hint+="   ·   ↵ / Esc";auto area=rect().adjusted(6,6,-6,-6);
    auto box=p.fontMetrics().boundingRect(area,Qt::AlignLeft|Qt::TextWordWrap,hint).adjusted(-3,-2,3,2);box.moveBottomLeft(area.bottomLeft());
    p.fillRect(box,QColor(250,252,252,235));p.setPen(QColor(32,50,60));p.drawText(box.adjusted(3,2,-3,-2),Qt::AlignLeft|Qt::TextWordWrap,hint);p.restore();
}

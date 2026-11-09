// GPL-2.0-or-later. Part layout and wave coordinates adapted from current Plus
// FreeShip.SavePart / FreeEdit.File_ImportPart / File_Import_MichletWaves.
// Included inside model_exchange.cpp's private namespace to share validation.
struct PartReader {
    const QByteArray& data;qsizetype at=0;
    QByteArray take(int n){if(n<0||n>data.size()-at)bad("部件文件已截断。");auto b=data.mid(at,n);at+=n;return b;}
    int i(){auto b=take(4);return qFromLittleEndian<qint32>(b.constData());}
    int count(int max){int n=i();if(n<0||n>max)bad("部件数量或索引超限。");return n;}
    bool flag(){auto b=take(1);if(b[0]!=0&&b[0]!=1)bad("部件布尔字段无效。");return b[0]!=0;}
    double f(){quint32 bits=quint32(i());float v;std::memcpy(&v,&bits,4);if(!std::isfinite(v)||std::abs(v)>1e9)bad("部件数值无效。");return v;}
    QString string(const QString& encoding){auto b=take(count(1048576));return fs::document::decodeFbmText(b,encoding);}
};
Document part(const QByteArray& bytes,const Options& o){
    PartReader r{bytes};if(r.string("cp1252")!="FREE!ship partfile")bad("不是 FreeShip 部件文件。");
    int version=r.count(43),units=r.count(1);Document d;auto& m=d.model;m.units=units?"imperial":"metric";m.precision=0;
    if(units)m.waterDensity=coreFloat(1.025*1000*.3048*.3048*.3048/.45359237);
    int n=r.count(1000);for(int i=0;i<n;++i){auto name=r.string(o.textEncoding);r.i();auto l=layer(name,i+1);l.color=quint32(r.i());if(l.color>0xffffff)bad("部件包含不能独立解析的系统颜色。");
        l.visible=r.flag();l.symmetric=r.flag();l.developable=r.flag();
        if(version>=9){l.useForIntersections=r.flag();l.useInHydrostatics=r.flag();if(version>=11){l.materialDensity=r.f();l.thickness=r.f();if(version>=15){l.showInLinesplan=r.flag();if(version>=21)l.alphaBlend=r.count(255);}}}
        m.layers.append(l);
    }
    n=r.count(maxPoints);for(int i=0;i<n;++i){Vec3 p{r.f(),r.f(),r.f()};int type=r.count(3);r.flag();addPoint(d,p,type);if(version>=13)m.points.back().locked=r.flag();}
    n=r.count(300000);for(int i=0;i<n;++i){auto a=r.count(m.points.size()-1),b=r.count(m.points.size()-1);m.edges.append({EntityId(i+1),EntityId(a+1),EntityId(b+1),r.flag()});}
    n=r.count(maxFaces);for(int i=0;i<n;++i){int count=r.count(maxCorners);QVector<EntityId> ids;for(int j=0;j<count;++j)ids.append(r.count(m.points.size()-1)+1);addFace(d,ids,r.count(m.layers.size()-1)+1);}
    n=r.count(4096);for(int i=0;i<n;++i){QVector<EntityId> ids;int count=r.count(maxPoints);for(int j=0;j<count;++j)ids.append(r.count(m.points.size()-1)+1);d.controlCurves.append(ids);}
    if(r.at!=bytes.size())bad("部件文件含未识别的尾部数据。");d.validate();return d;
}
void writePart(QIODevice& file,const ModelSnapshot& m,const Options& o){
    QSet<EntityId> faces(o.faceIds.begin(),o.faceIds.end()),points,layers;QSet<quint64> edges;
    for(const auto& f:m.faces)if(faces.contains(f.id)|| (faces.isEmpty()&&(o.includeHidden||m.layer(f.layer)->visible))){layers.insert(f.layer);for(int i=0;i<f.points.size();++i){points.insert(f.points[i]);edges.insert(edgeKey(f.points[i],f.points[(i+1)%f.points.size()]));}}
    if(points.isEmpty())bad("请先选择面，或保留至少一个可见图层。");
    if(!faces.isEmpty())for(auto id:faces)if(std::none_of(m.faces.begin(),m.faces.end(),[&](const auto& f){return f.id==id;}))bad("所选面已失效。");
    QByteArray data;auto i=[&](int v){char b[4];qToLittleEndian<qint32>(v,b);data.append(b,4);};auto f=[&](double v){float n=float(v);if(!std::isfinite(n))bad("部件数值超限。");quint32 bits;std::memcpy(&bits,&n,4);i(qint32(bits));};auto flag=[&](bool v){data.append(char(v));};
    auto string=[&](const QString& s){auto b=fs::document::encodeFbmText(s,o.textEncoding);i(b.size());data.append(b);};
    string("FREE!ship partfile");i(21);i(m.units=="imperial"?1:0);i(layers.size());QHash<EntityId,int> lm,pm;
    for(const auto& l:m.layers)if(layers.contains(l.id)){lm[l.id]=lm.size();string(l.name);i(lm[l.id]+1);i(l.color);flag(l.visible);flag(l.symmetric);flag(l.developable);flag(l.useForIntersections);flag(l.useInHydrostatics);f(l.materialDensity);f(l.thickness);flag(l.showInLinesplan);i(l.alphaBlend);}
    i(points.size());for(const auto& p:m.points)if(points.contains(p.id)){pm[p.id]=pm.size();f(p.position.x);f(p.position.y);f(p.position.z);i(p.vertexType);flag(false);flag(p.locked);}
    QVector<const ControlEdge*> es;for(const auto& e:m.edges)if(edges.contains(edgeKey(e.a,e.b)))es.append(&e);
    if(es.size()!=edges.size())bad("部件面引用了缺失的边。");i(es.size());for(auto e:es){i(pm[e->a]);i(pm[e->b]);flag(e->crease);}
    QVector<const ControlFace*> fs;for(const auto& face:m.faces)if(faces.contains(face.id)||(faces.isEmpty()&&(o.includeHidden||m.layer(face.layer)->visible)))fs.append(&face);
    i(fs.size());for(auto face:fs){i(face->points.size());for(auto p:face->points)i(pm[p]);i(lm[face->layer]);}
    QVector<const AuxiliaryCurve*> curves;for(const auto& c:m.controlCurves){bool all=c.controls.size()>1;for(int j=1;j<c.controls.size();++j)all&=edges.contains(edgeKey(c.controls[j-1],c.controls[j]));if(all)curves.append(&c);}
    i(curves.size());for(auto c:curves){i(c->controls.size());for(auto p:c->controls)i(pm[p]);}
    if(file.write(data)!=data.size())bad("部件文件写入失败。");
}
Document waves(const QByteArray& bytes){
    auto lines=text(bytes).split('\n');for(auto& s:lines)s=s.trimmed();while(!lines.isEmpty()&&lines.back().isEmpty())lines.removeLast();
    if(lines.size()<15)bad("波形文件不完整。");bool sector=lines[0]=="SECTORIAL PATCH";if(!sector&&lines[0]!="RECTANGULAR PATCH")bad("仅支持 Michlet 矩形或扇形波高文件。");
    auto csv=[&](int row){if(row>=lines.size())bad("波形文件已截断。");auto cells=lines[row].split(',');if(cells.back().isEmpty())cells.removeLast();for(auto& c:cells)c=c.trimmed();return cells;};
    auto a=csv(4),b=csv(6);sizeIs(a,3);sizeIs(b,3);number(a[0]);number(a[1]);number(b[0]);number(b[1]);int nx=integer(a[2],2,10000),ny=integer(b[2],2,10000);
    if(qint64(nx)*ny>maxPoints)bad("波形网格超过 100000 个点。");int header=sector?20:12;auto xs=csv(header);sizeIs(xs,nx+1);
    if(lines.size()!=header+1+ny)bad("波形行数与头部声明不一致。");Document d;auto l=layer("Michlet waves, speed="+lines[2]+" m/s",1,false);l.color=0xffff00;l.useInHydrostatics=l.useForIntersections=l.showInLinesplan=false;d.model.layers.append(l);
    QVector<double> xx;for(int i=1;i<xs.size();++i)xx.append(number(xs[i]));
    auto monotonic=[](const QVector<double>& values){bool positive=values[1]>values[0];for(int i=1;i<values.size();++i)if(values[i]==values[i-1]||(values[i]>values[i-1])!=positive)bad("波形坐标必须严格单调。");};monotonic(xx);QVector<double> yy;
    for(int j=0;j<ny;++j){auto cells=csv(header+1+j);sizeIs(cells,nx+1);double y=number(cells[0]);yy.append(y);double theta=-y*std::acos(-1.)/180.;
        for(int i=0;i<nx;++i){if(sector&&xx[i]<=0)bad("扇形半径必须为正。");Vec3 p{sector?xx[i]*std::cos(theta):xx[i],sector?xx[i]*std::sin(theta):y,number(cells[i+1])};addPoint(d,{coreFloat(p.x),coreFloat(p.y),coreFloat(p.z)},3);}}
    monotonic(yy);for(int j=1;j<ny;++j)for(int i=1;i<nx;++i){QVector<EntityId> ids{EntityId((j-1)*nx+i),EntityId((j-1)*nx+i+1),EntityId(j*nx+i+1),EntityId(j*nx+i)};addFace(d,ids,1);}
    Options o;o.symmetric=false;finishMesh(d,o);d.notes={"完整读取波高网格，作为非静水力图层追加；不丢弃零波高行，不强制中心线，不做细分平滑。"};return d;
}

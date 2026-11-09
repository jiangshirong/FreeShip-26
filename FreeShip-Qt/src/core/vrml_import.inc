// GPL-2.0-or-later. Bounded static VRML 1/2 polygon importer.
// VRML97 transform order: T C R SR S inverse(SR) inverse(C).
// https://www.web3d.org/documents/specifications/14772/V2.0/part1/nodesRef.html
struct VrmlReader {
    QStringList tokens;int at=0;QHash<QString,QJsonObject> definitions;
    explicit VrmlReader(QString text){
        for(qsizetype i=0;i<text.size();){auto c=text[i];if(c.isSpace()||c==','){++i;continue;}
            if(c=='#'){while(i<text.size()&&text[i]!='\n')++i;continue;}
            if(QString("{}[]").contains(c)){tokens.append(c);++i;continue;}
            qsizetype start=i;
            if(c=='"'){++i;bool closed=false;while(i<text.size()){if(text[i]=='\\'){i+=2;continue;}if(text[i++]=='"'){closed=true;break;}}if(!closed)bad("VRML 字符串未闭合。");}
            else while(i<text.size()&&!text[i].isSpace()&&!QString(",{}[]#").contains(text[i]))++i;
            if(i-start>16384)bad("VRML 字段过长。");tokens.append(text.mid(start,i-start));if(tokens.size()>2000000)bad("VRML 数据量超限。");
        }
    }
    QString next(){if(at>=tokens.size())bad("VRML 文件已截断。");return tokens[at++];}
    bool eat(const QString& t){if(at<tokens.size()&&tokens[at]==t){++at;return true;}return false;}
    void expect(const QString& t){if(!eat(t))bad("VRML 缺少 "+t);}
    QJsonObject node(int depth=0){
        if(depth>64)bad("VRML 嵌套过深。");QString type=next(),name;
        if(type=="USE"){name=next();if(!definitions.contains(name))bad("VRML USE 未定义或形成循环。");return definitions[name];}
        if(type=="DEF"){name=next();type=next();if(definitions.contains(name))bad("VRML DEF 名称重复。");}
        if(type=="NULL")return {};
        const QSet<QString> allowed{"Separator","Group","Transform","Translation","Rotation","Scale","Coordinate3","Coordinate","IndexedFaceSet","Shape","Appearance","Material","Normal","TextureCoordinate","TextureCoordinate2","Texture2","ImageTexture","WorldInfo","ShapeHints","Info","NormalBinding","MaterialBinding","TextureCoordinateBinding"};
        if(!allowed.contains(type))bad("VRML 节点暂不支持，未导入部分模型："+type);
        expect("{");QJsonObject n{{"type",type},{"name",name}};QJsonArray children;
        while(!eat("}")){
            if(at>=tokens.size())bad("VRML 节点未闭合。");
            if(type=="Separator"||(type=="Group"&&tokens[at]!="children"&&tokens[at]!="bboxCenter"&&tokens[at]!="bboxSize")){children.append(node(depth+1));continue;}
            auto key=next();if(n.contains(key))bad("VRML 字段重复："+key);
            if(key=="children"){QJsonArray nodes;if(eat("[")){while(!eat("]"))nodes.append(node(depth+1));}else nodes.append(node(depth+1));n[key]=nodes;continue;}
            if(key=="coord"||key=="geometry"||key=="appearance"||key=="material"||key=="normal"||key=="texCoord"||key=="texture"){n[key]=node(depth+1);continue;}
            const QSet<QString> vec3{"translation","center","scale","scaleFactor","ambientColor","diffuseColor","specularColor","emissiveColor","bboxCenter","bboxSize"};
            const QSet<QString> vec4{"rotation","scaleOrientation"};
            const QSet<QString> scalar{"value","ccw","solid","convex","creaseAngle","normalPerVertex","colorPerVertex","shininess","transparency","ambientIntensity","vertexOrdering","shapeType","faceType","string","title","name","repeatS","repeatT","wrapS","wrapT","model"};
            const QSet<QString> arrays{"point","coordIndex","normalIndex","textureCoordIndex","texCoordIndex","colorIndex","materialIndex","vector","info","url","filename"};
            if(!vec3.contains(key)&&!vec4.contains(key)&&!scalar.contains(key)&&!arrays.contains(key))bad("VRML 字段暂不支持："+key);
            QJsonArray values;if(eat("[")){while(!eat("]")){auto v=next();if(v=="{"||v=="}"||v=="[")bad("VRML 数组无效。");values.append(v);}}
            else{int count=vec3.contains(key)?3:vec4.contains(key)?4:1;for(int j=0;j<count;++j)values.append(next());}
            n[key]=values;
        }
        if(!children.isEmpty())n["children"]=children;if(!name.isEmpty())definitions[name]=n;return n;
    }
};
using Matrix=std::array<double,16>;
Matrix identity(){Matrix a{};for(int i=0;i<4;++i)a[i*4+i]=1;return a;}
Matrix multiply(const Matrix&a,const Matrix&b){Matrix r{};for(int i=0;i<4;++i)for(int j=0;j<4;++j)for(int k=0;k<4;++k)r[i*4+j]+=a[i*4+k]*b[k*4+j];return r;}
Matrix translation(double x,double y,double z){auto a=identity();a[3]=x;a[7]=y;a[11]=z;return a;}
QVector<double> vrValues(const QJsonObject& n,const QString& key,QVector<double> fallback){if(!n.contains(key))return fallback;QVector<double> r;for(auto v:n[key].toArray())r.append(number(v.toString()));if(r.size()!=fallback.size())bad("VRML 向量长度错误。");return r;}
Matrix rotation(const QVector<double>& v,double sign=1){auto a=identity();double len=std::hypot(v[0],v[1],v[2]);if(len==0){if(v[3]!=0)bad("VRML 旋转轴为零。");return a;}double x=v[0]/len,y=v[1]/len,z=v[2]/len,c=std::cos(sign*v[3]),s=std::sin(sign*v[3]),t=1-c;
    a[0]=t*x*x+c;a[1]=t*x*y-s*z;a[2]=t*x*z+s*y;a[4]=t*x*y+s*z;a[5]=t*y*y+c;a[6]=t*y*z-s*x;a[8]=t*x*z-s*y;a[9]=t*y*z+s*x;a[10]=t*z*z+c;return a;}
Matrix vrTransform(const QJsonObject& n){auto t=vrValues(n,"translation",{0,0,0}),c=vrValues(n,"center",{0,0,0}),s=vrValues(n,n.contains("scaleFactor")?"scaleFactor":"scale",{1,1,1});for(auto v:s)if(v==0)bad("VRML 缩放不能为零。");auto r=rotation(vrValues(n,"rotation",{0,0,1,0}));auto sr=vrValues(n,"scaleOrientation",{0,0,1,0});auto sc=identity();sc[0]=s[0];sc[5]=s[1];sc[10]=s[2];
    auto m=multiply(translation(t[0],t[1],t[2]),translation(c[0],c[1],c[2]));for(auto b:{r,rotation(sr),sc,rotation(sr,-1),translation(-c[0],-c[1],-c[2])})m=multiply(m,b);return m;}
Document vrml(const QByteArray& bytes,const Options& options){
    auto source=text(bytes);bool v1=source.startsWith("#VRML V1.0 ascii");if(!v1&&!source.startsWith("#VRML V2.0 utf8"))bad("需要 VRML 1.0 ASCII 或 2.0 UTF-8 文件头。");VrmlReader parser(source);QJsonArray roots;while(parser.at<parser.tokens.size())roots.append(parser.node());
    QByteArray mesh;QTextStream out(&mesh,QIODevice::WriteOnly);int total=0,shapes=0,visits=0;bool appearance=false;
    struct State{Matrix matrix=identity();QJsonObject coordinates;bool ccw=true;};
    std::function<void(const QJsonObject&,State&,int)> visit=[&](const QJsonObject& n,State& state,int depth){
        if(++visits>100000||depth>64)bad("VRML 实例数量或深度超限。");auto type=n["type"].toString();if(type.isEmpty())return;
        if(type=="Coordinate"||type=="Coordinate3"){state.coordinates=n;return;}
        if(type=="ShapeHints"){auto order=n["vertexOrdering"].toArray();if(!order.isEmpty())state.ccw=order[0]!="CLOCKWISE";return;}
        if(type=="Material"||type=="Appearance"||type=="Texture2"||type=="ImageTexture"){appearance=true;return;}
        if(type=="Shape"){State copy=state;if(n.contains("appearance"))appearance=true;visit(n["geometry"].toObject(),copy,depth+1);return;}
        if(type=="IndexedFaceSet"){
            auto coords=n.contains("coord")?n["coord"].toObject():state.coordinates;auto values=coords["point"].toArray();if(values.size()<9||values.size()%3)bad("VRML 坐标数组不完整。");if(++shapes>1000||total+values.size()/3>maxPoints)bad("VRML 几何数量超限。");
            out<<"g VRML_"<<shapes<<'\n';int base=total;
            for(int i=0;i<values.size();i+=3){double p[3]={number(values[i].toString()),number(values[i+1].toString()),number(values[i+2].toString())};out<<"v";for(int row=0;row<3;++row){double v=state.matrix[row*4+3];for(int k=0;k<3;++k)v+=state.matrix[row*4+k]*p[k];out<<' '<<QString::number(v,'g',17);}out<<'\n';++total;}
            bool ccw=state.ccw;if(n.contains("ccw")){auto v=n["ccw"].toArray();if(v.size()!=1||(v[0]!="TRUE"&&v[0]!="FALSE"))bad("VRML ccw 无效。");ccw=v[0]=="TRUE";}
            const auto& m=state.matrix;double determinant=m[0]*(m[5]*m[10]-m[6]*m[9])-m[1]*(m[4]*m[10]-m[6]*m[8])+m[2]*(m[4]*m[9]-m[5]*m[8]);if(determinant<0)ccw=!ccw;
            QVector<int> face;auto flush=[&]{if(face.size()<3)bad("VRML 面少于三个点。");if(!ccw)std::reverse(face.begin(),face.end());out<<"f";for(int p:face)out<<' '<<base+p+1;out<<'\n';face.clear();};
            for(auto v:n["coordIndex"].toArray()){int p=integer(v.toString(),-1,values.size()/3-1);if(p<0)flush();else{face.append(p);if(face.size()>maxCorners)bad("VRML 面顶点过多。");}}if(!face.isEmpty())flush();return;
        }
        if(type=="Separator"||type=="Group"||type=="Transform"){
            State copy=state;if(type=="Transform")copy.matrix=multiply(copy.matrix,vrTransform(n));
            if(v1&&type=="Transform"){state=copy;return;}
            for(auto child:n["children"].toArray())visit(child.toObject(),copy,depth+1);return;
        }
        if(type=="Translation"||type=="Rotation"||type=="Scale"){if(!v1)bad("VRML 2 不支持此旧版变换节点。");state.matrix=multiply(state.matrix,vrTransform(n));return;}
        if(type!="NormalBinding"&&type!="MaterialBinding"&&type!="TextureCoordinateBinding"&&type!="WorldInfo"&&type!="Info"&&type!="Normal"&&type!="TextureCoordinate"&&type!="TextureCoordinate2")bad("VRML 未处理节点。");
    };
    State state;for(auto node:roots)visit(node.toObject(),state,0);out.flush();auto o=options;o.legacyAxes=false;auto d=obj(mesh,o);d.notes={"读取静态 IndexedFaceSet，应用坐标变换及面朝向；保留文件 XYZ 轴，不自动猜测船艏方向。"};if(appearance)d.notes<<"材质、纹理及渲染属性不转换为船体属性。";return d;
}

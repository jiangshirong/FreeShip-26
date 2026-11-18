// SPDX-License-Identifier: GPL-2.0-or-later
// Text serialization follows TFreeTextBuffer and the version gates in
// FreeShip-Plus-Lazarus. Included inside fbm_codec.cpp's anonymous namespace.
class FtmCodec {
    bool decoding_;
    QByteArray input_, binary_, text_;
    qsizetype pos_=0;
    int version_=0;
    BlockMap blocks_;
    QString encoding_;
    QByteArray fromText(const QByteArray& s){return encodeFbmText(decodeFbmText(s,"utf8"),encoding_);}
    QByteArray toText(const QByteArray& s){return decodeFbmText(s,encoding_).toUtf8();}
    [[noreturn]] void fail(const char* why) const {
        throw std::invalid_argument(QString("Invalid FTM at offset %1: %2").arg(pos_).arg(why).toStdString());
    }
    void limit()const{if(binary_.size()>Fbm::MaximumBytes||text_.size()>Fbm::MaximumBytes)fail("file too large");}
    QByteArray line(){
        if(pos_>=input_.size())fail("missing record");
        auto end=input_.indexOf('\n',pos_);if(end<0)end=input_.size();
        auto s=input_.mid(pos_,end-pos_);pos_=end<input_.size()?end+1:end;
        if(s.endsWith('\r'))s.chop(1);return s;
    }
    void out(const QByteArray& s){text_+=s;text_+="\r\n";limit();}
    QByteArray take(int n){if(n<0||n>input_.size()-pos_)fail("truncated binary record");auto b=input_.mid(pos_,n);pos_+=n;return b;}
    void put32(quint32 x){char b[4];qToLittleEndian(x,b);binary_.append(b,4);limit();}
    int number(const QByteArray& s){bool ok=false;auto n=s.trimmed().toLongLong(&ok);if(!ok||n<INT_MIN||n>INT_MAX)fail("invalid integer");return int(n);}
    int integer(){if(decoding_){int n=number(line());put32(quint32(n));return n;}int n=qint32(qFromLittleEndian<quint32>(take(4).constData()));out(QByteArray::number(n));return n;}
    int count(){int n=integer();if(n<0||n>Fbm::MaximumCollectionItems||n>input_.size()-pos_)fail("invalid collection count");return n;}
    void flag(){if(decoding_){auto s=line().trimmed().toLower();if(s!="0"&&s!="1"&&s!="true"&&s!="false")fail("invalid boolean");binary_+=char(s=="1"||s=="true");}else{auto n=uchar(take(1)[0]);if(n>1)fail("invalid boolean");out(QByteArray::number(n));}}
    bool boolean(){if(decoding_){flag();return binary_.back()!=0;}if(pos_>=input_.size())fail("missing boolean");bool b=input_.at(pos_)!=0;flag();return b;}
    void flags(int n){while(n--)flag();}
    QByteArray realToken(const QByteArray& s={}){
        if(decoding_){auto token=s.trimmed();token.replace(',','.');bool ok=false;float f=token.toFloat(&ok);if(!ok||!std::isfinite(f))fail("invalid float");quint32 bits;std::memcpy(&bits,&f,4);put32(bits);return {};}
        auto b=take(4);float f;std::memcpy(&f,b.constData(),4);if(!std::isfinite(f))fail("non-finite calculation value cannot be written as portable text");return QByteArray::number(double(f),'g',std::numeric_limits<float>::max_digits10);
    }
    void reals(int n){while(n--){if(decoding_)realToken(line());else out(realToken());}}
    QList<QByteArray> words(const QByteArray& s){return s.simplified().split(' ');}
    void vector(int n){if(decoding_){auto w=words(line());if(w.size()!=n)fail("invalid coordinate/plane");for(auto& s:w)realToken(s);}else{QByteArray s;for(int i=0;i<n;++i){if(i)s+=' ';s+=realToken();}out(s);}}
    QByteArray string(bool escaped=true,bool transcode=true){
        if(decoding_){auto s=line();if(escaped){s.replace("\\n","\r\n");s.replace("\\\\","\\");}if(transcode)s=fromText(s);put32(s.size());binary_+=s;limit();return s;}
        int n=qint32(qFromLittleEndian<quint32>(take(4).constData()));auto s=take(n),encoded=s;
        if(transcode)s=toText(s);encoded=s;
        if(escaped){encoded.replace("\\","\\\\");encoded.replace("\r\n","\\n");}
        if(encoded.contains('\n')||encoded.contains('\r'))fail("unrepresentable newline in text field");
        // The original reader unescapes in two passes. Some literal backslash-n
        // strings cannot survive that reader, so reject instead of corrupting.
        if(escaped){auto check=encoded;check.replace("\\n","\r\n");check.replace("\\\\","\\");if(check!=s)fail("text cannot round-trip through legacy escaping");}
        out(encoded);return s;
    }
    void array(){int n=count();reals(n);}
    void jpeg(){integer();integer();int n=integer();if(n<0||n>Fbm::MaximumBytes)fail("invalid JPEG length");
        if(decoding_){auto h=line();if(h.size()!=qint64(n)*2)fail("invalid JPEG hex length");for(char c:h)if(!((c>='0'&&c<='9')||(c>='a'&&c<='f')||(c>='A'&&c<='F')))fail("invalid JPEG hex");binary_+=QByteArray::fromHex(h);limit();}
        else out(take(n).toHex().toUpper());}
    void record(const char* name,int floats,int flagsCount){
        if(!decoding_){auto it=blocks_.find(QString::fromLatin1(name));if(it==blocks_.end())fail("missing calculation block");if(it->second.size!=floats*4+4)fail("historical short calculator layout requires FBM version upgrade before text conversion");}
        reals(floats);flags(flagsCount);
        if(decoding_)binary_.append(4-flagsCount,char(0xff));else take(4-flagsCount);
    }
    void sections(){int n=count();while(n--){int kind=integer();if(version_>=11)flag();vector(4);flag();int s=count();while(s--){int p=count();while(p--){if(version_>=6&&kind>=1&&kind<=3)reals(2);else vector(3);flag();}}}}
    void spline(){int n=count();while(n--){vector(3);flag();}}
    void names(){int n=count();while(n--){if(decoding_){auto s=line();auto i=s.indexOf(' ');if(i<1)fail("invalid named entity");put32(quint32(number(s.left(i))));auto name=fromText(s.mid(i+1));put32(name.size());binary_+=name;}
        else{auto id=qint32(qFromLittleEndian<quint32>(take(4).constData()));auto size=qint32(qFromLittleEndian<quint32>(take(4).constData()));auto s=toText(take(size));if(s.contains('\n')||s.contains('\r'))fail("newline in entity name");out(QByteArray::number(id)+' '+s);}}}
public:
    FtmCodec(const QByteArray& input,bool decoding,const QJsonObject& inspection={},const QString& encoding="utf8"):decoding_(decoding),input_(input),blocks_(blockMap(inspection)),encoding_(encoding){}
    QByteArray convert(){
        if(decoding_&&input_.startsWith("\xef\xbb\xbf"))pos_=3;
        if(string(false,false)!=QByteArray("FREE!ship"))fail("invalid signature");
        if(decoding_){auto v=line();version_=-1;for(int i=0;i<45;++i)if(v==VersionNames[i])version_=i;if(version_<0)fail("unsupported version");binary_+=char(version_);}
        else{version_=uchar(take(1)[0]);if(version_>44)fail("unsupported version");out(VersionNames[version_]);}
        integer();integer();flags(10);reals(1);if(version_>=12)flag();if(version_>=16)reals(1);if(version_>=17)flags(6);if(version_>=20)flag();
        if(version_>=2){string();string();reals(3);flag();reals(2);flag();integer();integer();if(version_>=6){flag();reals(1);}if(version_>=7)flag();string();string();}
        if(version_>=16){integer();if(boolean())jpeg();}
        if(version_>=18)flag();
        if(version_>=20){reals(4);array();reals(3);flag();array();array();flag();reals(1);}
        if(version_>=37)flag();if(version_>=39)flag();if(version_>=44)integer();
        int layers=count();for(int i=0;i<layers;++i){string();integer();integer();flags(3);if(version_>=9)flags(2);if(version_>=11)reals(2);if(version_>=15)flag();if(version_>=21)integer();}integer();
        int points=count();for(int i=0;i<points;++i){vector(3);integer();flag();if(version_>=13)flag();}
        int edges=count();while(edges--){integer();integer();flags(2);}
        if(version_>=12){int n=count();while(n--){int k=count();while(k--)integer();flag();}}
        int faces=count();while(faces--){int n=count();while(n--)integer();integer();flag();}
        sections();sections();sections();if(version_>=9)sections();
        if(version_>=11){int n=count();while(n--){flag();if(version_>=21)flag();flag();reals(1);spline();}}
        if(version_>=16){record("delft",17,2);record("kaper",9,1);}
        if(version_>=20){int n=count();while(n--){integer();flag();integer();integer();integer();reals(1);integer();flag();integer();integer();jpeg();}
            n=count();while(n--){reals(2);integer();flags(2);spline();}}
        if(version_>=22){record("holtr",43,2);record("ost",52,2);record("propeller_task1",15,1);record("propeller_task2",25,1);record("propeller_task3",16,1);}
        if(version_>=23)record("planing",13,1);if(version_>=24)record("reverse_propeller",16,1);if(version_>=25)record("hollenbach",43,2);
        if(version_>=26)record("propeller_task4",15,1);if(version_>=30)record("propeller_task5",5,1);if(version_>=33)record("oortmerssen",43,2);if(version_>=35)record("fung",43,2);
        if(version_>=38){record("maneuvering",19,1);record("hydrodynamic_task1",11,1);}if(version_>=40){record("rbhs",52,2);record("mh",52,2);}
        if(version_>=41){names();names();names();names();int n=count();while(n--){if(decoding_){auto w=words(line());if(w.size()!=3)fail("invalid constraint");for(auto s:w)put32(quint32(number(s)));}else{auto a=qint32(qFromLittleEndian<quint32>(take(4).constData())),b=qint32(qFromLittleEndian<quint32>(take(4).constData())),c=qint32(qFromLittleEndian<quint32>(take(4).constData()));out(QByteArray::number(a)+' '+QByteArray::number(b)+' '+QByteArray::number(c));}}}
        if(version_>=42){int n=count();while(n--){string();if(decoding_){auto s=line().simplified();auto w=s.isEmpty()?QList<QByteArray>{}:s.split(' ');put32(w.size());for(auto id:w){number(id);put32(id.size());binary_+=id;}}
            else{int k=qint32(qFromLittleEndian<quint32>(take(4).constData()));if(k<0||k>Fbm::MaximumCollectionItems)fail("invalid group size");QByteArray s;while(k--){auto n=qint32(qFromLittleEndian<quint32>(take(4).constData()));auto id=take(n);number(id);if(!s.isEmpty())s+=' ';s+=id;}out(s);}}}
        if(version_>=43){int n=count();while(n--){integer();integer();flag();}}
        if(pos_!=input_.size())fail("unknown trailing data cannot be converted");limit();return decoding_?binary_:text_;
    }
};

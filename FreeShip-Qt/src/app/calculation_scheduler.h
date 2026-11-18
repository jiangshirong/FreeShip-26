#pragma once
#include <QObject>
#include <QPointer>
#include <QTimer>
#include <QList>
#include <functional>

// GUI-thread scheduler; a slot belongs to one private calculation process.
// Initializing a snapshot and reading editable defaults are not solver jobs.
class CalculationScheduler final {
    struct Entry {QPointer<QObject> owner;std::function<void()> start,changed;};
    QList<Entry> waiting_,running_;
    bool pumping_=false,paused_=false;
    void notify(){const auto entries=waiting_;for(auto e:entries)if(e.owner&&e.changed)e.changed();}
    void pump(){
        if(pumping_||paused_)return;pumping_=true;
        while(running_.size()<2&&!waiting_.isEmpty()){
            auto e=waiting_.takeFirst();if(!e.owner)continue;
            running_.append(e);if(e.changed)e.changed();if(e.owner)e.start();
        }
        pumping_=false;notify();
    }
public:
    void setPaused(bool paused){paused_=paused;if(!paused_)pump();}
    int position(const QObject* owner)const{for(int i=0;i<waiting_.size();++i)if(waiting_[i].owner==owner)return i+1;return 0;}
    bool active(const QObject* owner)const{for(auto e:running_)if(e.owner==owner)return true;return false;}
    int running()const{return running_.size();}
    void submit(QObject* owner,std::function<void()> start,std::function<void()> changed){
        waiting_.append({owner,std::move(start),std::move(changed)});pump();
    }
    void release(QObject* owner){
        for(auto* list:{&waiting_,&running_})for(int i=list->size()-1;i>=0;--i)if(!(*list)[i].owner||(*list)[i].owner==owner)list->removeAt(i);
        pump();
    }
};

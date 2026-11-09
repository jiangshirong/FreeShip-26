"""Extract evidence from a supplied recording; never capture or operate the desktop."""
from pathlib import Path
import argparse, hashlib, json
import av
from PIL import Image, ImageDraw

def digest(path):
 h=hashlib.sha256()
 with path.open('rb') as f:
  for chunk in iter(lambda:f.read(1024*1024),b''):h.update(chunk)
 return h.hexdigest()

def extract(video,output,start=0,end=None,step=.1,max_frames=120):
 video=Path(video).resolve();output=Path(output).resolve()
 if output.exists():raise ValueError('Output directory must not exist')
 if step<=0 or max_frames<1 or max_frames>500:raise ValueError('Positive step and 1..500 frames required')
 original=digest(video);output.mkdir(parents=True)
 samples=[];thumbs=[];next_at=start
 with av.open(str(video)) as container:
  stream=container.streams.video[0]
  metadata={'width':stream.width,'height':stream.height,'fps':str(stream.average_rate),
            'duration_s':None if container.duration is None else container.duration/1000000}
  for index,frame in enumerate(container.decode(stream)):
   at=float(frame.time)
   if end is not None and at>end:break
   if at+1e-6<next_at:continue
   name=f'frame-{index:06d}.png';im=frame.to_image();im.save(output/name)
   samples.append({'index':index,'at_s':at,'image':name})
   im.thumbnail((480,300));thumbs.append(im)
   next_at=at+step
   if len(samples)>=max_frames:break
 sheet=Image.new('RGB',(480*3,330*max(1,(len(samples)+2)//3)),'white');draw=ImageDraw.Draw(sheet)
 for i,(sample,thumb) in enumerate(zip(samples,thumbs)):
  x=i%3*480;y=i//3*330;sheet.paste(thumb,(x,y+24));draw.text((x+5,y+3),f"{sample['index']}  {sample['at_s']:.3f}s",fill='black')
 sheet.save(output/'contact.jpg')
 report={'status':'review','source':str(video),'sha256':original,'source_unchanged':digest(video)==original,
         'metadata':metadata,'frames':samples,'contact':'contact.jpg',
         'scope':'Recorded frames only. No desktop operation, automatic flicker verdict, or native animation acceptance.'}
 (output/'report.json').write_text(json.dumps(report,ensure_ascii=False,indent=2),encoding='utf-8')
 return report

if __name__=='__main__':
 p=argparse.ArgumentParser(description=__doc__);p.add_argument('video',type=Path);p.add_argument('--output',type=Path,required=True)
 p.add_argument('--start',type=float,default=0);p.add_argument('--end',type=float);p.add_argument('--step',type=float,default=.1)
 p.add_argument('--max-frames',type=int,default=120);a=p.parse_args()
 r=extract(a.video,a.output,a.start,a.end,a.step,a.max_frames)
 print(json.dumps({'status':r['status'],'frames':len(r['frames']),'report':str(a.output/'report.json')},ensure_ascii=False))
